#include "file_access_windows_9x.h"

#include "path_windows_9x.h"

CharString FileAccessWindows9x::_get_native_path(const String &p_path) const {
	const String path = PathWindows9x::get_absolute(fix_path(p_path));
	return PathWindows9x::is_reserved_filename(path) ? CharString() : PathWindows9x::to_native(path);
}

Error FileAccessWindows9x::open_internal(const String &p_path, int p_mode_flags) {
	close();
	DWORD access;
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
	const String absolute_path = PathWindows9x::get_absolute(fix_path(p_path));
	if (absolute_path.is_empty() || PathWindows9x::is_reserved_filename(absolute_path)) {
		return _error = ERR_FILE_BAD_PATH;
	}
	const CharString native_path = PathWindows9x::to_native(absolute_path);
	const bool safe_save = p_mode_flags == WRITE && is_backup_save_enabled();
	HANDLE file = INVALID_HANDLE_VALUE;
	String temporary;
	if (safe_save) {
		const DWORD attributes = GetFileAttributesA(native_path.get_data());
		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY))) {
			return _error = ERR_FILE_CANT_WRITE;
		}
		// CREATE_NEW reserves a sibling without replacing another save's temporary file.
		for (uint32_t attempt = 0; attempt < PathWindows9x::TEMPORARY_ATTEMPTS; ++attempt) {
			temporary = PathWindows9x::get_temporary_path(absolute_path, "save", attempt);
			if (temporary.is_empty()) {
				return _error = ERR_FILE_BAD_PATH;
			}
			file = CreateFileA(PathWindows9x::to_native(temporary).get_data(), access, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			const DWORD native_error = GetLastError();
			if (file != INVALID_HANDLE_VALUE || (native_error != ERROR_FILE_EXISTS && native_error != ERROR_ALREADY_EXISTS)) {
				break;
			}
		}
	} else {
		// FILE_SHARE_DELETE is unsupported on 9x.
		file = CreateFileA(native_path.get_data(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
	}
	if (file == INVALID_HANDLE_VALUE) {
		const DWORD native_error = GetLastError();
		return _error = native_error == ERROR_FILE_NOT_FOUND || native_error == ERROR_PATH_NOT_FOUND ? ERR_FILE_NOT_FOUND : ERR_FILE_CANT_OPEN;
	}
	if ((GetFileType(file) & ~DWORD(FILE_TYPE_REMOTE)) != FILE_TYPE_DISK) {
		CloseHandle(file);
		return _error = ERR_FILE_CANT_OPEN;
	}
	_file = file;
	_path = p_path;
	_absolute_path = absolute_path;
	_pending_save = temporary;
	big_endian = false;
	return _error = OK;
}

bool FileAccessWindows9x::is_open() const {
	return _file != nullptr;
}

String FileAccessWindows9x::get_path() const {
	return _path;
}

String FileAccessWindows9x::get_path_absolute() const {
	return _absolute_path;
}

void FileAccessWindows9x::close() {
	if (_file) {
		const bool safe_save = !_pending_save.is_empty();
		if (safe_save && !_write_failed && !FlushFileBuffers(_file)) {
			_write_failed = true;
		}
		if (!CloseHandle(_file)) {
			_write_failed = true;
		}
		_file = nullptr;
		if (safe_save) {
			const bool installed = !_write_failed && PathWindows9x::replace_file(_pending_save, _absolute_path) == OK;
			if (!installed) {
				DeleteFileA(PathWindows9x::to_native(_pending_save).get_data());
			}
			_pending_save = String();
			_write_failed = false;
			_eof = false;
			if (!installed) {
				_error = ERR_FILE_CANT_WRITE;
				if (close_fail_notify) {
					close_fail_notify(_absolute_path);
				}
				return;
			}
		}
	}
	_write_failed = false;
	_eof = false;
	_error = ERR_UNCONFIGURED;
}

bool FileAccessWindows9x::_seek(int64_t p_offset, uint32_t p_origin, uint64_t *r_position) const {
	if (!_file) {
		_error = ERR_UNCONFIGURED;
		return false;
	}
	LONG high = LONG(uint64_t(p_offset) >> 32);
	SetLastError(NO_ERROR);
	const DWORD low = SetFilePointer(_file, LONG(p_offset), &high, p_origin);
	if (low == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
		_error = ERR_FILE_CANT_READ;
		return false;
	}
	if (r_position) {
		*r_position = (uint64_t(uint32_t(high)) << 32) | low;
	}
	_eof = false;
	_error = OK;
	return true;
}

void FileAccessWindows9x::seek(uint64_t p_position) {
	if (p_position > INT64_MAX) {
		_error = ERR_INVALID_PARAMETER;
		return;
	}
	_seek(p_position, FILE_BEGIN);
}

void FileAccessWindows9x::seek_end(int64_t p_position) {
	_seek(p_position, FILE_END);
}

uint64_t FileAccessWindows9x::get_position() const {
	uint64_t position = 0;
	// Querying the cursor must not clear a previous EOF or I/O error.
	const Error previous_error = _error;
	const bool previous_eof = _eof;
	if (_seek(0, FILE_CURRENT, &position)) {
		_error = previous_error;
		_eof = previous_eof;
	}
	return position;
}

uint64_t FileAccessWindows9x::get_length() const {
	if (!_file) {
		_error = ERR_UNCONFIGURED;
		return 0;
	}
	DWORD high = 0;
	SetLastError(NO_ERROR);
	const DWORD low = GetFileSize(_file, &high);
	if (low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
		_error = ERR_FILE_CANT_READ;
		return 0;
	}
	return (uint64_t(high) << 32) | low;
}

bool FileAccessWindows9x::eof_reached() const {
	return _eof;
}

Error FileAccessWindows9x::get_error() const {
	return _error;
}

uint64_t FileAccessWindows9x::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V(!p_dst && p_length, uint64_t(-1));
	if (!_file) {
		_error = ERR_UNCONFIGURED;
		return uint64_t(-1);
	}
	uint64_t total = 0;
	while (total < p_length) {
		const DWORD chunk = MIN(p_length - total, uint64_t(1 << 20));
		DWORD count = 0;
		if (!ReadFile(_file, p_dst + total, chunk, &count, nullptr)) {
			if (GetLastError() != ERROR_HANDLE_EOF) {
				_error = ERR_FILE_CANT_READ;
				return total;
			}
			count = 0;
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

bool FileAccessWindows9x::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND_V(!p_src && p_length, false);
	if (!_file) {
		_error = ERR_UNCONFIGURED;
		return false;
	}
	uint64_t total = 0;
	while (total < p_length) {
		const DWORD chunk = MIN(p_length - total, uint64_t(1 << 20));
		DWORD count = 0;
		if (!WriteFile(_file, p_src + total, chunk, &count, nullptr) || count == 0) {
			_write_failed = true;
			_error = ERR_FILE_CANT_WRITE;
			return false;
		}
		total += count;
	}
	_eof = false;
	_error = OK;
	return true;
}

Error FileAccessWindows9x::resize(int64_t p_length) {
	if (!_file) {
		return _error = ERR_UNCONFIGURED;
	}
	if (p_length < 0) {
		return _error = ERR_INVALID_PARAMETER;
	}
	uint64_t position = 0;
	if (!_seek(0, FILE_CURRENT, &position) || !_seek(p_length, FILE_BEGIN)) {
		_write_failed = true;
		return _error = ERR_FILE_CANT_WRITE;
	}
	const bool resized = SetEndOfFile(_file);
	if (!_seek(position, FILE_BEGIN) || !resized) {
		_write_failed = true;
		return _error = ERR_FILE_CANT_WRITE;
	}
	return _error = OK;
}

void FileAccessWindows9x::flush() {
	if (!_file || !FlushFileBuffers(_file)) {
		_write_failed |= _file != nullptr;
		_error = _file ? ERR_FILE_CANT_WRITE : ERR_UNCONFIGURED;
	}
}

bool FileAccessWindows9x::file_exists(const String &p_name) {
	const DWORD attributes = GetFileAttributesA(_get_native_path(p_name).get_data());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

uint64_t FileAccessWindows9x::_filetime_to_unix(const FILETIME &p_time) {
	const uint64_t ticks = (uint64_t(p_time.dwHighDateTime) << 32) | p_time.dwLowDateTime;
	const uint64_t epoch = 116444736000000000ULL;
	return ticks >= epoch ? (ticks - epoch) / 10000000 : 0;
}

bool FileAccessWindows9x::_query_file_metadata(const CharString &p_path, WIN32_FILE_ATTRIBUTE_DATA &r_data) {
	if (p_path.is_empty()) {
		return false;
	}
#if _WIN32_WINDOWS >= 0x0410
	return GetFileAttributesExA(p_path.get_data(), GetFileExInfoStandard, &r_data);
#else
	// Original Windows 95 lacks GetFileAttributesExA.
	WIN32_FIND_DATAA data;
	const HANDLE search = FindFirstFileA(p_path.get_data(), &data);
	if (search == INVALID_HANDLE_VALUE) {
		return false;
	}
	FindClose(search);
	r_data = { data.dwFileAttributes, data.ftCreationTime, data.ftLastAccessTime, data.ftLastWriteTime, data.nFileSizeHigh, data.nFileSizeLow };
	return true;
#endif // _WIN32_WINDOWS >= 0x0410
}

uint64_t FileAccessWindows9x::_get_modified_time(const String &p_file) {
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!_query_file_metadata(_get_native_path(p_file), data)) {
		return 0;
	}
	const FILETIME &time = data.ftLastWriteTime;
	return _filetime_to_unix(time.dwLowDateTime || time.dwHighDateTime ? time : data.ftCreationTime);
}

uint64_t FileAccessWindows9x::_get_access_time(const String &p_file) {
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!_query_file_metadata(_get_native_path(p_file), data)) {
		return 0;
	}
	const FILETIME &time = data.ftLastAccessTime;
	return _filetime_to_unix(time.dwLowDateTime || time.dwHighDateTime ? time : data.ftCreationTime);
}

int64_t FileAccessWindows9x::_get_size(const String &p_file) {
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!_query_file_metadata(_get_native_path(p_file), data) || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
		return -1;
	}
	return (uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

BitField<FileAccess::UnixPermissionFlags> FileAccessWindows9x::_get_unix_permissions(const String &) {
	return 0;
}

Error FileAccessWindows9x::_set_unix_permissions(const String &, BitField<UnixPermissionFlags>) {
	return ERR_UNAVAILABLE;
}

Error FileAccessWindows9x::_set_file_attribute(const CharString &p_path, DWORD p_attribute, bool p_enabled) {
	const DWORD attributes = GetFileAttributesA(p_path.get_data());
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return ERR_FILE_NOT_FOUND;
	}
	const DWORD requested = p_enabled ? attributes | p_attribute : attributes & ~p_attribute;
	if (requested == attributes) {
		return OK;
	}
	const DWORD updated = requested & ~FILE_ATTRIBUTE_NORMAL;
	return SetFileAttributesA(p_path.get_data(), updated ? updated : FILE_ATTRIBUTE_NORMAL) ? OK : ERR_FILE_CANT_WRITE;
}

bool FileAccessWindows9x::_get_hidden_attribute(const String &p_file) {
	const DWORD attributes = GetFileAttributesA(_get_native_path(p_file).get_data());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN);
}

Error FileAccessWindows9x::_set_hidden_attribute(const String &p_file, bool p_hidden) {
	return _set_file_attribute(_get_native_path(p_file), FILE_ATTRIBUTE_HIDDEN, p_hidden);
}

bool FileAccessWindows9x::_get_read_only_attribute(const String &p_file) {
	const DWORD attributes = GetFileAttributesA(_get_native_path(p_file).get_data());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY);
}

Error FileAccessWindows9x::_set_read_only_attribute(const String &p_file, bool p_read_only) {
	return _set_file_attribute(_get_native_path(p_file), FILE_ATTRIBUTE_READONLY, p_read_only);
}

FileAccessWindows9x::~FileAccessWindows9x() {
	close();
}
