#include "drivers/win_legacy/nt/file_access_windows_nt.h"

// Legacy Windows includes.
#include "drivers/win_legacy/nt/path_windows_nt.h"

uint64_t FileAccessWindowsNT::_filetime_to_unix(const FILETIME &p_time) {
	// The number of 100-nanosecond intervals between Windows epoch (1601) to Unix epoch (1970).
	const uint64_t epoch = 116444736000000000ULL;
	const uint64_t ticks = (uint64_t(p_time.dwHighDateTime) << 32) | p_time.dwLowDateTime;
	// Divide by 10000000 to get seconds.
	return ticks >= epoch ? (ticks - epoch) / 10000000 : 0;
}

bool FileAccessWindowsNT::_query_file_metadata(const String &p_path, WIN32_FILE_ATTRIBUTE_DATA &r_data) {
	// GetFileAttributesExW was added in NT 4 SP3.
	return GetFileAttributesExW(p_path.c_str(), GetFileExInfoStandard, &r_data);
}

String FileAccessWindowsNT::_get_native_path(const String &p_path) const {
	const String path = PathWindowsNT::get_absolute(fix_path(p_path));
	return PathWindowsNT::is_reserved_filename(path) ? String() : PathWindowsNT::to_native(path);
}

Error FileAccessWindowsNT::_open(const String &p_path, int p_mode_flags) {
	close();
	DWORD access = 0;
	DWORD disposition = OPEN_EXISTING;
	switch (p_mode_flags) {
		case READ:
			access = GENERIC_READ;
			break;
		case WRITE:
			access = GENERIC_WRITE;
			disposition = CREATE_ALWAYS;
			break;
		case READ_WRITE:
			access = GENERIC_READ | GENERIC_WRITE;
			break;
		case WRITE_READ:
			access = GENERIC_READ | GENERIC_WRITE;
			disposition = CREATE_ALWAYS;
			break;
		default:
			return _error = ERR_INVALID_PARAMETER;
	}

	const String path = PathWindowsNT::get_absolute(fix_path(p_path));
	if (path.empty() || PathWindowsNT::is_reserved_filename(path)) {
		return _error = ERR_FILE_BAD_PATH;
	}
	String wide_path = PathWindowsNT::to_native(path);
	const bool safe_save = p_mode_flags == WRITE && is_backup_save_enabled();
	HANDLE file;
	if (safe_save) {
		const DWORD attributes = GetFileAttributesW(wide_path.c_str());
		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY))) {
			return _error = ERR_FILE_CANT_WRITE;
		}
		// Keep the temporary file on the same volume. CREATE_NEW lets
		// simultaneous saves choose different names without a race.
		const String parent = path.get_base_dir();
		for (uint32_t attempt = 0;; ++attempt) {
			const String name = ".godot-save-" + itos(GetCurrentProcessId()) + "-" + itos(attempt) + ".tmp";
			wide_path = PathWindowsNT::to_native(parent.plus_file(name));
			file = CreateFileW(wide_path.c_str(), access, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS) {
				break;
			}
		}
	} else {
		file = CreateFileW(wide_path.c_str(), access,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
	}
	if (file == INVALID_HANDLE_VALUE) {
		const DWORD error = GetLastError();
		return _error = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? ERR_FILE_NOT_FOUND : ERR_FILE_CANT_OPEN;
	}
	if (GetFileType(file) != FILE_TYPE_DISK) {
		CloseHandle(file);
		return _error = ERR_FILE_CANT_OPEN;
	}

	_file = file;
	if (safe_save) {
		_pending_save = wide_path;
	}
	_path = p_path;
	_absolute_path = path;
	endian_swap = false;
	return _error = OK;
}

uint64_t FileAccessWindowsNT::_get_modified_time(const String &p_file) {
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!_query_file_metadata(_get_native_path(p_file), data)) {
		return 0;
	}
	const FILETIME &time = data.ftLastWriteTime;
	return _filetime_to_unix(time.dwLowDateTime || time.dwHighDateTime ? time : data.ftCreationTime);
}

bool FileAccessWindowsNT::is_open() const {
	return _file != nullptr;
}

String FileAccessWindowsNT::get_path() const {
	return _path;
}

String FileAccessWindowsNT::get_path_absolute() const {
	return _absolute_path;
}

void FileAccessWindowsNT::close() {
	if (_file) {
		const bool safe_save = !_pending_save.empty();
		if (safe_save && !_write_failed && !FlushFileBuffers(_file)) {
			_write_failed = true;
		}
		CloseHandle(_file);
		_file = nullptr;
		if (safe_save) {
			const LPCWSTR temporary = _pending_save.c_str();
			const String destination = PathWindowsNT::to_native(_absolute_path);
			if (_write_failed || !MoveFileExW(temporary, destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
				DeleteFileW(temporary);
				_error = ERR_FILE_CANT_WRITE;
				_pending_save = String();
				_write_failed = false;
				if (close_fail_notify) {
					close_fail_notify(_absolute_path);
				}
				_eof = false;
				return;
			}
			_pending_save = String();
		}
	}
	_write_failed = false;
	_eof = false;
	_error = ERR_UNCONFIGURED;
}

bool FileAccessWindowsNT::_set_file_pointer(HANDLE p_file, LARGE_INTEGER p_distance, LARGE_INTEGER *r_position, DWORD p_origin) {
#if _WIN32_WINNT < 0x0500
	// NT4's older API supports 64-bit positions. An all-ones low word can be valid.
	LONG high = p_distance.HighPart;
	SetLastError(NO_ERROR);
	const DWORD low = SetFilePointer(p_file, p_distance.LowPart, &high, p_origin);
	if (low == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
		return false;
	}
	if (r_position) {
		r_position->LowPart = low;
		r_position->HighPart = high;
	}
	return true;
#else
	return SetFilePointerEx(p_file, p_distance, r_position, p_origin);
#endif // _WIN32_WINNT < 0x0500
}

bool FileAccessWindowsNT::_seek(int64_t p_offset, uint32_t p_origin) const {
	if (!_file) {
		_error = ERR_UNCONFIGURED;
		return false;
	}
	LARGE_INTEGER distance;
	distance.QuadPart = p_offset;
	if (!_set_file_pointer(_file, distance, nullptr, p_origin)) {
		_error = ERR_FILE_CANT_READ;
		return false;
	}
	_eof = false;
	_error = OK;
	return true;
}

void FileAccessWindowsNT::seek(uint64_t p_position) {
	if (p_position > INT64_MAX) {
		_error = ERR_INVALID_PARAMETER;
		return;
	}
	_seek(p_position, FILE_BEGIN);
}

void FileAccessWindowsNT::seek_end(int64_t p_position) {
	_seek(p_position, FILE_END);
}

uint64_t FileAccessWindowsNT::get_position() const {
	LARGE_INTEGER position;
	if (!_file || !_set_file_pointer(_file, {}, &position, FILE_CURRENT)) {
		_error = _file ? ERR_FILE_CANT_READ : ERR_UNCONFIGURED;
		return 0;
	}
	return position.QuadPart;
}

uint64_t FileAccessWindowsNT::get_len() const {
#if _WIN32_WINNT < 0x0500
	if (!_file) {
		_error = ERR_UNCONFIGURED;
		return 0;
	}
	ULARGE_INTEGER length;
	SetLastError(NO_ERROR);
	length.LowPart = GetFileSize(_file, &length.HighPart);
	if (length.LowPart == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
		_error = ERR_FILE_CANT_READ;
		return 0;
	}
#else
	LARGE_INTEGER length;
	if (!_file || !GetFileSizeEx(_file, &length)) {
		_error = _file ? ERR_FILE_CANT_READ : ERR_UNCONFIGURED;
		return 0;
	}
#endif // _WIN32_WINNT < 0x0500
	return length.QuadPart;
}

bool FileAccessWindowsNT::eof_reached() const {
	return _eof;
}

Error FileAccessWindowsNT::get_error() const {
	return _error;
}

uint8_t FileAccessWindowsNT::get_8() const {
	uint8_t value = 0;
	get_buffer(&value, 1);
	return value;
}

uint64_t FileAccessWindowsNT::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V(!p_dst && p_length != 0, uint64_t(-1));
	if (!_file) {
		_error = ERR_UNCONFIGURED;
		return uint64_t(-1);
	}
	uint64_t total = 0;
	while (total < p_length) {
		const DWORD chunk = MIN(p_length - total, uint64_t(INT32_MAX));
		DWORD count = 0;
		if (!ReadFile(_file, p_dst + total, chunk, &count, nullptr)) {
			if (GetLastError() != ERROR_HANDLE_EOF) {
				_error = ERR_FILE_CANT_READ;
				return total ? total : uint64_t(-1);
			}
		}
		total += count;
		if (count < chunk) {
			_eof = true;
			_error = ERR_FILE_EOF;
			return total;
		}
	}
	_error = _eof ? ERR_FILE_EOF : OK;
	return total;
}

void FileAccessWindowsNT::store_8(uint8_t p_value) {
	store_buffer(&p_value, 1);
}

void FileAccessWindowsNT::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND(!p_src && p_length != 0);
	if (!_file) {
		_error = ERR_UNCONFIGURED;
		return;
	}
	uint64_t total = 0;
	while (total < p_length) {
		const DWORD chunk = MIN(p_length - total, uint64_t(INT32_MAX));
		DWORD count = 0;
		if (!WriteFile(_file, p_src + total, chunk, &count, nullptr) || count == 0) {
			_write_failed = true;
			_error = ERR_FILE_CANT_WRITE;
			return;
		}
		total += count;
	}
	_eof = false;
	_error = OK;
}

void FileAccessWindowsNT::flush() {
	if (!_file || !FlushFileBuffers(_file)) {
		_write_failed |= _file != nullptr;
		_error = _file ? ERR_FILE_CANT_WRITE : ERR_UNCONFIGURED;
	}
}

bool FileAccessWindowsNT::file_exists(const String &p_name) {
	const DWORD attributes = GetFileAttributesW(_get_native_path(p_name).c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

uint32_t FileAccessWindowsNT::_get_unix_permissions(const String &) {
	// Win32 file attributes and NTFS ACLs do not represent POSIX mode bits.
	return 0;
}

Error FileAccessWindowsNT::_set_unix_permissions(const String &, uint32_t) {
	return ERR_UNAVAILABLE;
}

FileAccessWindowsNT::~FileAccessWindowsNT() {
	close();
}
