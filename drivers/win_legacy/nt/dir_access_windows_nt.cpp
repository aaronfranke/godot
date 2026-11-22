#include "drivers/win_legacy/nt/dir_access_windows_nt.h"

#include "drivers/win_legacy/nt/path_windows_nt.h"

// Godot includes.
#include "core/io/marshalls.h"
#include "core/os/os.h"

#include <windows.h>

#include <winioctl.h>

bool DirAccessWindowsNT::_is_under_root(const String &p_path, const String &p_root) {
	return p_path.nocasecmp_to(p_root) == 0 ||
			(p_path.length() > p_root.length() && p_path[p_root.length()] == '/' &&
					p_path.left(p_root.length()).nocasecmp_to(p_root) == 0);
}

struct DirAccessWindowsNT::Search {
	WIN32_FIND_DATAW entry;
	HANDLE handle;
};

String DirAccessWindowsNT::_get_absolute_path(const String &p_path) const {
	String path = DirAccess::fix_path(p_path.replace("\\", "/"));
	if (path.empty()) {
		return _current_dir;
	}
	if (path.length() >= 2 && path[1] == ':') {
		// A bare drive name from get_drive() denotes that drive's root.
		if (path.length() == 2) {
			path += "/";
		}
	} else if (!path.is_abs_path()) {
		path = _current_dir.plus_file(path);
	} else if (path.begins_with("/") && !path.begins_with("//") &&
			_current_dir.length() >= 2 && _current_dir[1] == ':') {
		path = _current_dir.substr(0, 2) + path;
	}
	return PathWindowsNT::get_absolute(path);
}

Error DirAccessWindowsNT::list_dir_begin() {
	list_dir_end();
	const String wide_pattern = PathWindowsNT::to_native(_current_dir.plus_file("*"));
	WIN32_FIND_DATAW entry;
	HANDLE handle = FindFirstFileW(wide_pattern.c_str(), &entry);
	if (handle == INVALID_HANDLE_VALUE) {
		// FAT may yield no matching entries in an empty directory.
		return GetLastError() == ERROR_FILE_NOT_FOUND ? OK : ERR_CANT_OPEN;
	}
	_search = memnew(Search({ entry, handle }));
	return OK;
}

String DirAccessWindowsNT::get_next() {
	if (!_search) {
		_current_is_dir = false;
		_current_is_hidden = false;
		return {};
	}
	const String name = String(_search->entry.cFileName);
	const DWORD attributes = _search->entry.dwFileAttributes;
	if (!FindNextFileW(_search->handle, &_search->entry)) {
		list_dir_end();
	}
	_current_is_dir = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	_current_is_hidden = (attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
	return name;
}

bool DirAccessWindowsNT::current_is_dir() const {
	return _current_is_dir;
}

bool DirAccessWindowsNT::current_is_hidden() const {
	return _current_is_hidden;
}

void DirAccessWindowsNT::list_dir_end() {
	if (_search) {
		FindClose(_search->handle);
		memdelete(_search);
		_search = nullptr;
	}
	_current_is_dir = false;
	_current_is_hidden = false;
}

int DirAccessWindowsNT::get_drive_count() {
	int count = 0;
	for (DWORD drives = GetLogicalDrives(); drives; drives &= drives - 1) {
		++count;
	}
	return count;
}

String DirAccessWindowsNT::get_drive(int p_drive) {
	if (p_drive < 0) {
		return {};
	}
	DWORD drives = GetLogicalDrives();
	for (CharType letter = 'A'; drives && letter <= 'Z'; ++letter, drives >>= 1) {
		if ((drives & 1) && p_drive-- == 0) {
			return String::chr(letter) + ":";
		}
	}
	return {};
}

Error DirAccessWindowsNT::change_dir(String p_dir) {
	const String path = _get_absolute_path(p_dir);
	const String root = _get_root_path().replace("\\", "/").simplify_path().trim_suffix("/");
	if (!root.empty() && !_is_under_root(path, root)) {
		return ERR_INVALID_PARAMETER;
	}
	const DWORD attributes = GetFileAttributesW(PathWindowsNT::to_native(path).c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		return ERR_CANT_OPEN;
	}
	list_dir_end();
	_current_dir = path;
	return OK;
}

String DirAccessWindowsNT::get_current_dir() {
	const String root = _get_root_path().replace("\\", "/").simplify_path().trim_suffix("/");
	if (!root.empty() && _is_under_root(_current_dir, root)) {
		const String relative = _current_dir.substr(root.length()).trim_prefix("/");
		return _get_root_string() + relative;
	}
	return _current_dir;
}

Error DirAccessWindowsNT::make_dir(String p_dir) {
	const String absolute_path = _get_absolute_path(p_dir);
	if (absolute_path.empty() || PathWindowsNT::is_reserved_filename(absolute_path)) {
		return ERR_FILE_BAD_PATH;
	}
	// Legacy CreateDirectoryW reserves room for an 8.3 filename under MAX_PATH.
	const bool extended = absolute_path.length() >= MAX_PATH - 12;
	const String path = PathWindowsNT::to_native(absolute_path, extended);
	if (CreateDirectoryW(path.c_str(), nullptr)) {
		return OK;
	}
	return GetLastError() == ERROR_ALREADY_EXISTS ? ERR_ALREADY_EXISTS : ERR_CANT_CREATE;
}

bool DirAccessWindowsNT::file_exists(String p_file) {
	const DWORD attributes = GetFileAttributesW(PathWindowsNT::to_native(_get_absolute_path(p_file)).c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirAccessWindowsNT::dir_exists(String p_dir) {
	const DWORD attributes = GetFileAttributesW(PathWindowsNT::to_native(_get_absolute_path(p_dir)).c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

Error DirAccessWindowsNT::rename(String p_from, String p_to) {
	const String from = PathWindowsNT::to_native(_get_absolute_path(p_from));
	const String to = PathWindowsNT::to_native(_get_absolute_path(p_to));
	// Keep the destination in place if the rename fails. Cross-volume
	// file moves are handled by the system when MOVEFILE_COPY_ALLOWED is set.
	return MoveFileExW(from.c_str(), to.c_str(),
				   MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)
			? OK
			: FAILED;
}

Error DirAccessWindowsNT::remove(String p_name) {
	const String path = PathWindowsNT::to_native(_get_absolute_path(p_name));
	const LPCWSTR native_path = path.c_str();
	const DWORD attributes = GetFileAttributesW(native_path);
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return ERR_FILE_NOT_FOUND;
	}
	const bool removed = (attributes & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryW(native_path) : DeleteFileW(native_path);
	return removed ? OK : FAILED;
}

bool DirAccessWindowsNT::is_link(String p_file) {
	const String absolute_path = _get_absolute_path(p_file);
	if (absolute_path.find_char('*') >= 0 || absolute_path.find_char('?') >= 0) {
		return false;
	}
	const String path = PathWindowsNT::to_native(absolute_path);
	WIN32_FIND_DATAW data;
	const HANDLE find = FindFirstFileW(path.c_str(), &data);
	if (find == INVALID_HANDLE_VALUE) {
		return false;
	}
	FindClose(find);
	return (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
			(data.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT || data.dwReserved0 == IO_REPARSE_TAG_SYMLINK);
}

String DirAccessWindowsNT::read_link(String p_file) {
	const String path = PathWindowsNT::to_native(_get_absolute_path(p_file));
	// Windows 2000 uses NTFS junctions and mount points. The reparse point
	// flag and FSCTL_GET_REPARSE_POINT are unavailable on standard NT 4.
	const HANDLE file = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		return {};
	}
	alignas(wchar_t) uint8_t data[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
	DWORD received = 0;
	const bool read = DeviceIoControl(file, FSCTL_GET_REPARSE_POINT, nullptr, 0, data, sizeof(data), &received, nullptr);
	CloseHandle(file);
	if (!read || received < 16) {
		return {};
	}
	const uint32_t tag = decode_uint32(data);
	DWORD header_size;
	if (tag == DWORD(IO_REPARSE_TAG_MOUNT_POINT)) {
		header_size = 16;
	} else if (tag == DWORD(IO_REPARSE_TAG_SYMLINK)) {
		header_size = 20;
	} else {
		return {};
	}
	const uint32_t offset = decode_uint16(data + 8);
	const uint32_t length = decode_uint16(data + 10);
	if (received < header_size || length == 0 || ((offset | length) & 1) != 0 || offset + length > received - header_size) {
		return {};
	}

	String target = String(reinterpret_cast<const wchar_t *>(data + header_size + offset), length / sizeof(wchar_t));
	if (target.begins_with(R"(\??\Volume{)")) {
		return (String(R"(\\?\)") + target.substr(4)).replace("\\", "/");
	}
	if (target.begins_with(R"(\??\UNC\)")) {
		return "//" + target.substr(8).replace("\\", "/");
	}
	return target.trim_prefix(R"(\??\)").replace("\\", "/");
}

Error DirAccessWindowsNT::create_link(String, String) {
	// Vista added symbolic links. NTFS junctions cannot provide file or relative links.
	return ERR_UNAVAILABLE;
}

uint64_t DirAccessWindowsNT::get_space_left() {
	ULARGE_INTEGER available;
	ULARGE_INTEGER total;
	ULARGE_INTEGER free;
	const String path = PathWindowsNT::to_native(_current_dir);
	// GetDiskFreeSpaceExW was added in NT 4 SP3.
	// NT4 dereferences total/free even when only available space is requested.
	return GetDiskFreeSpaceExW(path.c_str(), &available, &total, &free) ? available.QuadPart : 0;
}

String DirAccessWindowsNT::get_filesystem_type() const {
	const String root = PathWindowsNT::get_volume_root(_current_dir);
	if (root.empty()) {
		return {};
	}
	wchar_t filesystem[MAX_PATH];
	if (!GetVolumeInformationW(root.c_str(), nullptr, 0, nullptr, nullptr, nullptr, filesystem, MAX_PATH)) {
		return {};
	}
	return String(filesystem);
}

DirAccessWindowsNT::DirAccessWindowsNT() :
		_current_dir(PathWindowsNT::get_absolute(".")) {
}

DirAccessWindowsNT::~DirAccessWindowsNT() {
	list_dir_end();
}
