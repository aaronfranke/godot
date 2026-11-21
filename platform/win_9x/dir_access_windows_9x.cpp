#include "dir_access_windows_9x.h"

#include "encoding_windows_9x.h"
#include "path_windows_9x.h"

#include "core/os/os.h"

#include <windows.h>

struct DirAccessWindows9x::Search {
	WIN32_FIND_DATAA entry;
	HANDLE handle;
};

bool DirAccessWindows9x::_is_under_root(const String &p_path, const String &p_root) {
	return p_path.nocasecmp_to(p_root) == 0 ||
			(p_path.length() > p_root.length() && p_path[p_root.length()] == '/' && p_path.left(p_root.length()).nocasecmp_to(p_root) == 0);
}

String DirAccessWindows9x::fix_path(const String &p_path) const {
	String path = DirAccess::fix_path(p_path.replace_char('\\', '/'));
	if (path.is_empty()) {
		return _current_dir;
	}
	if (path.length() >= 2 && path[1] == ':') {
		if (path.length() == 2) {
			path += "/";
		}
	} else if (path.is_relative_path()) {
		path = _current_dir.path_join(path);
	} else if (path.begins_with("/") && !path.begins_with("//") && _current_dir.length() >= 2 && _current_dir[1] == ':') {
		path = _current_dir.left(2) + path;
	}
	return PathWindows9x::get_absolute(path);
}

Error DirAccessWindows9x::list_dir_begin() {
	list_dir_end();
	const CharString pattern = PathWindows9x::to_native(_current_dir.path_join("*"));
	if (pattern.is_empty()) {
		return ERR_FILE_BAD_PATH;
	}
	WIN32_FIND_DATAA entry;
	const HANDLE handle = FindFirstFileA(pattern.get_data(), &entry);
	if (handle == INVALID_HANDLE_VALUE) {
		return GetLastError() == ERROR_FILE_NOT_FOUND ? OK : ERR_CANT_OPEN;
	}
	_search = memnew(Search({ entry, handle }));
	return OK;
}

String DirAccessWindows9x::get_next() {
	if (!_search) {
		_current_is_dir = false;
		_current_is_hidden = false;
		return {};
	}
	const String name = Windows9xEncoding::decode(_search->entry.cFileName);
	const DWORD attributes = _search->entry.dwFileAttributes;
	if (!FindNextFileA(_search->handle, &_search->entry)) {
		list_dir_end();
	}
	_current_is_dir = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	_current_is_hidden = (attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
	return name;
}

bool DirAccessWindows9x::current_is_dir() const {
	return _current_is_dir;
}

bool DirAccessWindows9x::current_is_hidden() const {
	return _current_is_hidden;
}

void DirAccessWindows9x::list_dir_end() {
	if (_search) {
		FindClose(_search->handle);
		memdelete(_search);
		_search = nullptr;
	}
	_current_is_dir = false;
	_current_is_hidden = false;
}

int DirAccessWindows9x::get_drive_count() {
	int count = 0;
	for (DWORD drives = GetLogicalDrives(); drives; drives &= drives - 1) {
		++count;
	}
	return count;
}

String DirAccessWindows9x::get_drive(int p_drive) {
	if (p_drive < 0) {
		return {};
	}
	DWORD drives = GetLogicalDrives();
	for (char32_t letter = 'A'; drives && letter <= 'Z'; ++letter, drives >>= 1) {
		if ((drives & 1) && p_drive-- == 0) {
			return String::chr(letter) + ":";
		}
	}
	return {};
}

String DirAccessWindows9x::get_drive_label(int p_drive) {
	const String drive = get_drive(p_drive);
	if (drive.is_empty()) {
		return {};
	}
	const CharString root = PathWindows9x::to_native(drive + "/");
	const UINT type = GetDriveTypeA(root.get_data());
	if (type == DRIVE_REMOTE) {
		return "<network>";
	}
	// Avoid a system dialog for removable drives without media.
	if (type != DRIVE_FIXED && type != DRIVE_RAMDISK) {
		return {};
	}
	char label[MAX_PATH];
	return GetVolumeInformationA(root.get_data(), label, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0) ? Windows9xEncoding::decode(label) : String();
}

Error DirAccessWindows9x::change_dir(String p_dir) {
	const String path = fix_path(p_dir);
	const String root = _get_root_path().replace_char('\\', '/').simplify_path().trim_suffix("/");
	if (!root.is_empty() && !_is_under_root(path, root)) {
		return ERR_INVALID_PARAMETER;
	}
	const DWORD attributes = GetFileAttributesA(PathWindows9x::to_native(path).get_data());
	if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		return ERR_CANT_OPEN;
	}
	list_dir_end();
	_current_dir = path;
	return OK;
}

String DirAccessWindows9x::get_current_dir(bool p_include_drive) const {
	const String root = _get_root_path().replace_char('\\', '/').simplify_path().trim_suffix("/");
	if (!root.is_empty() && _is_under_root(_current_dir, root)) {
		return _get_root_string() + _current_dir.substr(root.length()).trim_prefix("/");
	}
	if (!p_include_drive && _current_dir.length() >= 2 && _current_dir[1] == ':') {
		return _current_dir.substr(2);
	}
	return _current_dir;
}

Error DirAccessWindows9x::make_dir(String p_dir) {
	const String path = fix_path(p_dir);
	const CharString native = PathWindows9x::to_native(path);
	if (native.is_empty() || PathWindows9x::is_reserved_filename(path)) {
		return ERR_FILE_BAD_PATH;
	}
	if (CreateDirectoryA(native.get_data(), nullptr)) {
		return OK;
	}
	return GetLastError() == ERROR_ALREADY_EXISTS ? ERR_ALREADY_EXISTS : ERR_CANT_CREATE;
}

bool DirAccessWindows9x::file_exists(String p_file) {
	const DWORD attributes = GetFileAttributesA(PathWindows9x::to_native(fix_path(p_file)).get_data());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirAccessWindows9x::dir_exists(String p_dir) {
	const DWORD attributes = GetFileAttributesA(PathWindows9x::to_native(fix_path(p_dir)).get_data());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

Error DirAccessWindows9x::rename(String p_from, String p_to) {
	const String from = fix_path(p_from);
	const String to = fix_path(p_to);
	const CharString native_from = PathWindows9x::to_native(from);
	const CharString native_to = PathWindows9x::to_native(to);
	if (native_from.is_empty() || native_to.is_empty() || PathWindows9x::is_reserved_filename(to)) {
		return ERR_FILE_BAD_PATH;
	}
	const DWORD attributes = GetFileAttributesA(native_from.get_data());
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return ERR_FILE_NOT_FOUND;
	}
	if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
		return MoveFileA(native_from.get_data(), native_to.get_data()) ? OK : FAILED;
	}
	return PathWindows9x::replace_file(from, to);
}

Error DirAccessWindows9x::remove(String p_name) {
	const CharString path = PathWindows9x::to_native(fix_path(p_name));
	const DWORD attributes = GetFileAttributesA(path.get_data());
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return ERR_FILE_NOT_FOUND;
	}
	const bool removed = (attributes & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryA(path.get_data()) : DeleteFileA(path.get_data());
	return removed ? OK : FAILED;
}

bool DirAccessWindows9x::is_link(String) {
	return false;
}

String DirAccessWindows9x::read_link(String) {
	return {};
}

Error DirAccessWindows9x::create_link(String, String) {
	// Native 9x filesystems do not implement NTFS reparse points or symbolic links.
	return ERR_UNAVAILABLE;
}

uint64_t DirAccessWindows9x::get_space_left() {
	ULARGE_INTEGER available;
	const CharString path = PathWindows9x::to_native(_current_dir.trim_suffix("/") + "/");
	// Present from Windows 95 OSR2 onward, including the Windows 98 SE baseline.
	return GetDiskFreeSpaceExA(path.get_data(), &available, nullptr, nullptr) ? available.QuadPart : 0;
}

String DirAccessWindows9x::_get_volume_root() const {
	if (_current_dir.length() >= 3 && _current_dir[1] == ':') {
		return _current_dir.left(3);
	}
	if (_current_dir.begins_with("//")) {
		const int server_end = _current_dir.find_char('/', 2);
		if (server_end >= 0) {
			const int share_end = _current_dir.find_char('/', server_end + 1);
			return share_end < 0 ? _current_dir + "/" : _current_dir.left(share_end + 1);
		}
	}
	return {};
}

String DirAccessWindows9x::get_filesystem_type() const {
	const CharString root = PathWindows9x::to_native(_get_volume_root());
	char filesystem[MAX_PATH];
	return GetVolumeInformationA(root.get_data(), nullptr, 0, nullptr, nullptr, nullptr, filesystem, MAX_PATH) ? Windows9xEncoding::decode(filesystem) : String();
}

bool DirAccessWindows9x::is_case_sensitive(const String &) const {
	return false;
}

bool DirAccessWindows9x::is_equivalent(const String &p_path_a, const String &p_path_b) const {
	return PathWindows9x::is_equivalent(fix_path(p_path_a), fix_path(p_path_b));
}

DirAccessWindows9x::DirAccessWindows9x() :
		_current_dir(OS::get_singleton()->get_cwd()) {
}

DirAccessWindows9x::~DirAccessWindows9x() {
	list_dir_end();
}
