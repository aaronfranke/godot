#include "file_access_windows_nt.h"

#include "drivers/win_legacy/nt/path_windows_nt.h"

uint64_t FileAccessWindowsNT::_filetime_to_unix(const FILETIME &p_time) {
	// The number of 100-nanosecond intervals between Windows epoch (1601) to Unix epoch (1970).
	const uint64_t epoch = 116444736000000000ULL;
	const uint64_t ticks = (uint64_t(p_time.dwHighDateTime) << 32) | p_time.dwLowDateTime;
	// Divide by 10000000 to get seconds.
	return ticks >= epoch ? (ticks - epoch) / 10000000 : 0;
}

bool FileAccessWindowsNT::_query_file_metadata(const Char16String &p_path, WIN32_FILE_ATTRIBUTE_DATA &r_data) {
	return GetFileAttributesExW((LPCWSTR)p_path.get_data(), GetFileExInfoStandard, &r_data);
}

Char16String FileAccessWindowsNT::_get_native_path(const String &p_path) const {
	const String path = PathWindowsNT::get_absolute(fix_path(p_path));
	return PathWindowsNT::is_reserved_filename(path) ? Char16String() : PathWindowsNT::to_native(path);
}

Error FileAccessWindowsNT::open_internal(const String &p_path, int p_mode_flags) {
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
	if (path.is_empty() || PathWindowsNT::is_reserved_filename(path)) {
		return _error = ERR_FILE_BAD_PATH;
	}
	Char16String wide_path = PathWindowsNT::to_native(path);
	const bool safe_save = p_mode_flags == WRITE && is_backup_save_enabled();
	HANDLE file;
	if (safe_save) {
		const DWORD attributes = GetFileAttributesW((LPCWSTR)wide_path.get_data());
		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY))) {
			return _error = ERR_FILE_CANT_WRITE;
		}
		// Keep the temporary file on the same volume. CREATE_NEW lets
		// simultaneous saves choose different names without a race.
		const String parent = path.get_base_dir();
		for (uint32_t attempt = 0;; ++attempt) {
			const String name = vformat(".godot-save-%d-%d.tmp", GetCurrentProcessId(), attempt);
			wide_path = PathWindowsNT::to_native(parent.path_join(name));
			file = CreateFileW((LPCWSTR)wide_path.get_data(), access, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS) {
				break;
			}
		}
	} else {
		file = CreateFileW((LPCWSTR)wide_path.get_data(), access,
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
	big_endian = false;
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

uint64_t FileAccessWindowsNT::_get_access_time(const String &p_file) {
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!_query_file_metadata(_get_native_path(p_file), data)) {
		return 0;
	}
	const FILETIME &time = data.ftLastAccessTime;
	return _filetime_to_unix(time.dwLowDateTime || time.dwHighDateTime ? time : data.ftCreationTime);
}

int64_t FileAccessWindowsNT::_get_size(const String &p_file) {
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!_query_file_metadata(_get_native_path(p_file), data) || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
		return -1;
	}
	return (uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
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
		const bool safe_save = !_pending_save.is_empty();
		if (safe_save && !_write_failed && !FlushFileBuffers(_file)) {
			_write_failed = true;
		}
		CloseHandle(_file);
		_file = nullptr;
		if (safe_save) {
			const LPCWSTR temporary = (LPCWSTR)_pending_save.get_data();
			const Char16String destination = PathWindowsNT::to_native(_absolute_path);
			if (_write_failed || !MoveFileExW(temporary, (LPCWSTR)destination.get_data(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
				DeleteFileW(temporary);
				_error = ERR_FILE_CANT_WRITE;
				_pending_save = Char16String();
				_write_failed = false;
				if (close_fail_notify) {
					close_fail_notify(_absolute_path);
				}
				_eof = false;
				return;
			}
			_pending_save = Char16String();
		}
	}
	_write_failed = false;
	_eof = false;
	_error = ERR_UNCONFIGURED;
}

bool FileAccessWindowsNT::_set_file_pointer(HANDLE p_file, LARGE_INTEGER p_distance, LARGE_INTEGER *r_position, DWORD p_origin) {
	return SetFilePointerEx(p_file, p_distance, r_position, p_origin);
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

uint64_t FileAccessWindowsNT::get_length() const {
	LARGE_INTEGER length;
	if (!_file || !GetFileSizeEx(_file, &length)) {
		_error = _file ? ERR_FILE_CANT_READ : ERR_UNCONFIGURED;
		return 0;
	}
	return length.QuadPart;
}

bool FileAccessWindowsNT::eof_reached() const {
	return _eof;
}

Error FileAccessWindowsNT::get_error() const {
	return _error;
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

bool FileAccessWindowsNT::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND_V(!p_src && p_length != 0, false);
	if (!_file) {
		_error = ERR_UNCONFIGURED;
		return false;
	}
	uint64_t total = 0;
	while (total < p_length) {
		const DWORD chunk = MIN(p_length - total, uint64_t(INT32_MAX));
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

Error FileAccessWindowsNT::resize(int64_t p_length) {
	if (!_file) {
		return _error = ERR_UNCONFIGURED;
	}
	if (p_length < 0) {
		return _error = ERR_INVALID_PARAMETER;
	}

	LARGE_INTEGER position;
	if (!_set_file_pointer(_file, {}, &position, FILE_CURRENT) || !_seek(p_length, FILE_BEGIN)) {
		return _error = ERR_FILE_CANT_WRITE;
	}
	const bool resized = SetEndOfFile(_file);
	if (!_seek(position.QuadPart, FILE_BEGIN) || !resized) {
		_write_failed = true;
		return _error = ERR_FILE_CANT_WRITE;
	}
	return OK;
}

void FileAccessWindowsNT::flush() {
	if (!_file || !FlushFileBuffers(_file)) {
		_write_failed |= _file != nullptr;
		_error = _file ? ERR_FILE_CANT_WRITE : ERR_UNCONFIGURED;
	}
}

bool FileAccessWindowsNT::file_exists(const String &p_name) {
	const DWORD attributes = GetFileAttributesW((LPCWSTR)_get_native_path(p_name).get_data());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

BitField<FileAccess::UnixPermissionFlags> FileAccessWindowsNT::_get_unix_permissions(const String &) {
	// Win32 file attributes and NTFS ACLs do not represent POSIX mode bits.
	return 0;
}

Error FileAccessWindowsNT::_set_unix_permissions(const String &, BitField<UnixPermissionFlags>) {
	return ERR_UNAVAILABLE;
}

Error FileAccessWindowsNT::_set_file_attribute(const Char16String &p_path, DWORD p_attribute, bool p_enabled) {
	const LPCWSTR path = (LPCWSTR)p_path.get_data();
	const DWORD attributes = GetFileAttributesW(path);
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return GetLastError() == ERROR_ACCESS_DENIED ? ERR_UNAUTHORIZED : ERR_FILE_NOT_FOUND;
	}
	const DWORD requested = p_enabled ? attributes | p_attribute : attributes & ~p_attribute;
	if (requested == attributes) {
		return OK;
	}
	const DWORD updated = requested & ~FILE_ATTRIBUTE_NORMAL;
	return SetFileAttributesW(path, updated ? updated : FILE_ATTRIBUTE_NORMAL) ? OK : ERR_FILE_CANT_WRITE;
}

bool FileAccessWindowsNT::_get_hidden_attribute(const String &p_file) {
	const Char16String path = _get_native_path(p_file);
	const DWORD attributes = GetFileAttributesW((LPCWSTR)path.get_data());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN);
}

Error FileAccessWindowsNT::_set_hidden_attribute(const String &p_file, bool p_hidden) {
	return _set_file_attribute(_get_native_path(p_file), FILE_ATTRIBUTE_HIDDEN, p_hidden);
}

bool FileAccessWindowsNT::_get_read_only_attribute(const String &p_file) {
	const Char16String path = _get_native_path(p_file);
	const DWORD attributes = GetFileAttributesW((LPCWSTR)path.get_data());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY);
}

Error FileAccessWindowsNT::_set_read_only_attribute(const String &p_file, bool p_read_only) {
	return _set_file_attribute(_get_native_path(p_file), FILE_ATTRIBUTE_READONLY, p_read_only);
}

Char16String FileAccessWindowsNT::_get_stream_path(const String &p_file, const String &p_name) {
	if (p_name.is_empty() || p_name.find_char(':') >= 0 || p_name.find_char('/') >= 0 ||
			p_name.find_char('\\') >= 0 || p_name.find_char('\0') >= 0) {
		return {};
	}
	const String path = PathWindowsNT::get_absolute(fix_path(p_file));
	if (path.is_empty() || !file_exists(p_file)) {
		return {};
	}
	return PathWindowsNT::to_native(path + ":" + p_name + ":$DATA");
}

PackedByteArray FileAccessWindowsNT::_get_extended_attribute(const String &p_file, const String &p_attribute_name) {
	const Char16String path = _get_stream_path(p_file, p_attribute_name);
	if (path.is_empty()) {
		return {};
	}
	FileAccessWindowsNT stream;
	stream._file = CreateFileW((LPCWSTR)path.get_data(), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (stream._file == INVALID_HANDLE_VALUE) {
		stream._file = nullptr;
		return {};
	}
	const uint64_t size = stream.get_length();
	PackedByteArray data;
	if (size > INT32_MAX || data.resize(size) != OK || stream.get_buffer(data.ptrw(), size) != size) {
		return {};
	}
	return data;
}

Error FileAccessWindowsNT::_set_extended_attribute(const String &p_file, const String &p_attribute_name, const PackedByteArray &p_data) {
	const Char16String path = _get_stream_path(p_file, p_attribute_name);
	if (path.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	FileAccessWindowsNT stream;
	stream._file = CreateFileW((LPCWSTR)path.get_data(), GENERIC_WRITE, FILE_SHARE_READ,
			nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (stream._file == INVALID_HANDLE_VALUE) {
		stream._file = nullptr;
		return ERR_CANT_CREATE;
	}
	return stream.store_buffer(p_data.ptr(), p_data.size()) ? OK : ERR_FILE_CANT_WRITE;
}

Error FileAccessWindowsNT::_remove_extended_attribute(const String &p_file, const String &p_attribute_name) {
	const Char16String path = _get_stream_path(p_file, p_attribute_name);
	if (path.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	return DeleteFileW((LPCWSTR)path.get_data()) ? OK : ERR_FILE_CANT_WRITE;
}

PackedStringArray FileAccessWindowsNT::_get_extended_attributes_list(const String &p_file) {
	const Char16String path = _get_native_path(p_file);
	const HANDLE file = CreateFileW((LPCWSTR)path.get_data(), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		return {};
	}

	// BackupRead enumerates NTFS data streams; BackupSeek skips each stream's payload.
	void *context = nullptr;
	DWORD count;
	PackedStringArray names;
	WIN32_STREAM_ID stream;
	const DWORD header_size = offsetof(WIN32_STREAM_ID, cStreamName);
	while (BackupRead(file, reinterpret_cast<BYTE *>(&stream), header_size, &count, FALSE, FALSE, &context) && count == header_size) {
		if (stream.dwStreamNameSize) {
			if (stream.dwStreamNameSize % sizeof(wchar_t) != 0 || stream.dwStreamNameSize > 65536) {
				break;
			}
			Char16String name;
			name.resize_uninitialized(stream.dwStreamNameSize / sizeof(wchar_t) + 1);
			if (!BackupRead(file, reinterpret_cast<BYTE *>(name.ptrw()), stream.dwStreamNameSize, &count, FALSE, FALSE, &context) || count != stream.dwStreamNameSize) {
				break;
			}
			if (stream.dwStreamId == BACKUP_ALTERNATE_DATA) {
				const String full_name = String::utf16(name.get_data(), count / sizeof(wchar_t));
				if (full_name.begins_with(":") && full_name.ends_with(":$DATA") && full_name.length() > 7) {
					names.push_back(full_name.substr(1, full_name.length() - 7));
				}
			}
		}
		if (stream.Size.QuadPart > 0) {
			DWORD skipped_low;
			DWORD skipped_high;
			if (!BackupSeek(file, stream.Size.LowPart, stream.Size.HighPart, &skipped_low, &skipped_high, &context) ||
					skipped_low != stream.Size.LowPart || skipped_high != DWORD(stream.Size.HighPart)) {
				break;
			}
		}
	}
	BackupRead(file, nullptr, 0, &count, TRUE, FALSE, &context);
	CloseHandle(file);
	return names;
}

FileAccessWindowsNT::~FileAccessWindowsNT() {
	close();
}
