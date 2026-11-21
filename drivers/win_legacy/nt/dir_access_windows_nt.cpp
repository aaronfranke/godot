#include "dir_access_windows_nt.h"

// Godot includes.
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "drivers/win_legacy/nt/path_windows_nt.h"

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

String DirAccessWindowsNT::fix_path(const String &p_path) const {
	String path = DirAccess::fix_path(p_path.replace_char('\\', '/'));
	if (path.is_empty()) {
		return _current_dir;
	}
	if (path.length() >= 2 && path[1] == ':') {
		// A bare drive name from get_drive() denotes that drive's root.
		if (path.length() == 2) {
			path += "/";
		}
	} else if (path.is_relative_path()) {
		path = _current_dir.path_join(path);
	} else if (path.begins_with("/") && !path.begins_with("//") &&
			_current_dir.length() >= 2 && _current_dir[1] == ':') {
		path = _current_dir.substr(0, 2) + path;
	}
	return PathWindowsNT::get_absolute(path);
}

Error DirAccessWindowsNT::list_dir_begin() {
	list_dir_end();
	const Char16String wide_pattern = PathWindowsNT::to_native(_current_dir.path_join("*"));
	WIN32_FIND_DATAW entry;
	HANDLE handle = FindFirstFileW((LPCWSTR)wide_pattern.get_data(), &entry);
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
	const String name = String::utf16(reinterpret_cast<const char16_t *>(_search->entry.cFileName));
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
	for (char32_t letter = 'A'; drives && letter <= 'Z'; ++letter, drives >>= 1) {
		if ((drives & 1) && p_drive-- == 0) {
			return String::chr(letter) + ":";
		}
	}
	return {};
}

String DirAccessWindowsNT::get_drive_label(int p_drive) {
	const String drive = get_drive(p_drive);
	if (drive.is_empty()) {
		return {};
	}
	const Char16String root = (drive + "\\").utf16();
	const LPCWSTR path = (LPCWSTR)root.get_data();
	const UINT type = GetDriveTypeW(path);
	if (type == DRIVE_REMOTE) {
		return "<network>";
	}
	// A volume query on removable media can raise a system dialog.
	if (type != DRIVE_FIXED && type != DRIVE_RAMDISK) {
		return {};
	}
	wchar_t label[MAX_PATH];
	if (GetVolumeInformationW(path, label, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0)) {
		return String::utf16(reinterpret_cast<const char16_t *>(label));
	}
	return {};
}

Error DirAccessWindowsNT::change_dir(String p_dir) {
	const String path = fix_path(p_dir);
	const String root = _get_root_path().replace_char('\\', '/').simplify_path().trim_suffix("/");
	if (!root.is_empty() && !_is_under_root(path, root)) {
		return ERR_INVALID_PARAMETER;
	}
	const DWORD attributes = GetFileAttributesW((LPCWSTR)PathWindowsNT::to_native(path).get_data());
	if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		return ERR_CANT_OPEN;
	}
	list_dir_end();
	_current_dir = path;
	return OK;
}

String DirAccessWindowsNT::get_current_dir(bool p_include_drive) const {
	const String root = _get_root_path().replace_char('\\', '/').simplify_path().trim_suffix("/");
	if (!root.is_empty() && _is_under_root(_current_dir, root)) {
		const String relative = _current_dir.substr(root.length()).trim_prefix("/");
		return _get_root_string() + relative;
	}
	if (!p_include_drive && _current_dir.length() >= 2 && _current_dir[1] == ':') {
		return _current_dir.substr(2);
	}
	return _current_dir;
}

Error DirAccessWindowsNT::make_dir(String p_dir) {
	const String absolute_path = fix_path(p_dir);
	if (absolute_path.is_empty() || PathWindowsNT::is_reserved_filename(absolute_path)) {
		return ERR_FILE_BAD_PATH;
	}
	// Legacy CreateDirectoryW reserves room for an 8.3 filename under MAX_PATH.
	const bool extended = absolute_path.utf16().length() >= MAX_PATH - 12;
	const Char16String path = PathWindowsNT::to_native(absolute_path, extended);
	if (CreateDirectoryW((LPCWSTR)path.get_data(), nullptr)) {
		return OK;
	}
	return GetLastError() == ERROR_ALREADY_EXISTS ? ERR_ALREADY_EXISTS : ERR_CANT_CREATE;
}

bool DirAccessWindowsNT::file_exists(String p_file) {
	const DWORD attributes = GetFileAttributesW((LPCWSTR)PathWindowsNT::to_native(fix_path(p_file)).get_data());
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirAccessWindowsNT::dir_exists(String p_dir) {
	const DWORD attributes = GetFileAttributesW((LPCWSTR)PathWindowsNT::to_native(fix_path(p_dir)).get_data());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

Error DirAccessWindowsNT::rename(String p_from, String p_to) {
	const Char16String from = PathWindowsNT::to_native(fix_path(p_from));
	const Char16String to = PathWindowsNT::to_native(fix_path(p_to));
	// Keep the destination in place if the rename fails. Cross-volume
	// file moves are handled by the system when MOVEFILE_COPY_ALLOWED is set.
	return MoveFileExW((LPCWSTR)from.get_data(), (LPCWSTR)to.get_data(),
				   MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)
			? OK
			: FAILED;
}

Error DirAccessWindowsNT::remove(String p_name) {
	const Char16String path = PathWindowsNT::to_native(fix_path(p_name));
	const LPCWSTR native_path = (LPCWSTR)path.get_data();
	const DWORD attributes = GetFileAttributesW(native_path);
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return ERR_FILE_NOT_FOUND;
	}
	const bool removed = (attributes & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryW(native_path) : DeleteFileW(native_path);
	return removed ? OK : FAILED;
}

bool DirAccessWindowsNT::is_link(String p_file) {
	const String absolute_path = fix_path(p_file);
	if (absolute_path.find_char('*') >= 0 || absolute_path.find_char('?') >= 0) {
		return false;
	}
	const Char16String path = PathWindowsNT::to_native(absolute_path);
	WIN32_FIND_DATAW data;
	const HANDLE find = FindFirstFileW((LPCWSTR)path.get_data(), &data);
	if (find == INVALID_HANDLE_VALUE) {
		return false;
	}
	FindClose(find);
	return (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
			(data.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT || data.dwReserved0 == IO_REPARSE_TAG_SYMLINK);
}

String DirAccessWindowsNT::read_link(String p_file) {
	const Char16String path = PathWindowsNT::to_native(fix_path(p_file));
	const HANDLE file = CreateFileW((LPCWSTR)path.get_data(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		return {};
	}
	alignas(char16_t) uint8_t data[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
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

	String target = String::utf16(reinterpret_cast<const char16_t *>(data + header_size + offset), length / sizeof(char16_t));
	if (target.begins_with(R"(\??\Volume{)")) {
		return (String(R"(\\?\)") + target.substr(4)).replace_char('\\', '/');
	}
	if (target.begins_with(R"(\??\UNC\)")) {
		return "//" + target.substr(8).replace_char('\\', '/');
	}
	return target.trim_prefix(R"(\??\)").replace_char('\\', '/');
}

Error DirAccessWindowsNT::create_link(String, String) {
	// Vista added symbolic links. NTFS junctions cannot provide file or relative links.
	return ERR_UNAVAILABLE;
}

uint64_t DirAccessWindowsNT::get_space_left() {
	ULARGE_INTEGER available;
	ULARGE_INTEGER total;
	ULARGE_INTEGER free;
	const Char16String path = PathWindowsNT::to_native(_current_dir);
	// GetDiskFreeSpaceExW was added in NT 4 SP3.
	// NT4 dereferences the total/free outputs even when only available space is needed.
	const bool found = GetDiskFreeSpaceExW((LPCWSTR)path.get_data(), &available, &total, &free);
	return found ? available.QuadPart : 0;
}

String DirAccessWindowsNT::get_filesystem_type() const {
	const Char16String root = PathWindowsNT::get_volume_root(_current_dir);
	if (root.is_empty()) {
		return {};
	}
	wchar_t filesystem[MAX_PATH];
	if (!GetVolumeInformationW(reinterpret_cast<LPCWSTR>(root.get_data()), nullptr, 0, nullptr, nullptr, nullptr, filesystem, MAX_PATH)) {
		return {};
	}
	return String::utf16(reinterpret_cast<const char16_t *>(filesystem));
}

bool DirAccessWindowsNT::is_case_sensitive(const String &p_path) const {
	// Win32 paths are case-insensitive. Per-directory flags are a newer feature.
	return false;
}

bool DirAccessWindowsNT::is_equivalent(const String &p_path_a, const String &p_path_b) const {
	const auto open_for_identity = [](const Char16String &p_path) {
		return CreateFileW((LPCWSTR)p_path.get_data(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	};
	const HANDLE a = open_for_identity(PathWindowsNT::to_native(fix_path(p_path_a)));
	if (a == INVALID_HANDLE_VALUE) {
		return DirAccess::is_equivalent(p_path_a, p_path_b);
	}
	const HANDLE b = open_for_identity(PathWindowsNT::to_native(fix_path(p_path_b)));
	BY_HANDLE_FILE_INFORMATION first = {};
	BY_HANDLE_FILE_INFORMATION second = {};
	const bool have_identity = b != INVALID_HANDLE_VALUE && GetFileInformationByHandle(a, &first) && GetFileInformationByHandle(b, &second);
	if (b != INVALID_HANDLE_VALUE) {
		CloseHandle(b);
	}
	CloseHandle(a);
	if (!have_identity) {
		return DirAccess::is_equivalent(p_path_a, p_path_b);
	}
	return first.dwVolumeSerialNumber == second.dwVolumeSerialNumber &&
			first.nFileIndexHigh == second.nFileIndexHigh && first.nFileIndexLow == second.nFileIndexLow;
}

DirAccessWindowsNT::DirAccessWindowsNT() :
		_current_dir(OS::get_singleton()->get_cwd()) {
}

DirAccessWindowsNT::~DirAccessWindowsNT() {
	list_dir_end();
}
