#include "drivers/win_legacy/nt/path_windows_nt.h"

#include <limits.h>
#include <windows.h>

bool PathWindowsNT::has_embedded_null(const String &p_path) {
	// In Godot 3, find_char() can also find the string's terminating null.
	const int index = p_path.find_char('\0');
	return index >= 0 && index < p_path.length();
}

String PathWindowsNT::get_absolute(const String &p_path) {
	if (p_path.empty() || has_embedded_null(p_path)) {
		return {};
	}

	const String path = p_path.replace("/", "\\");
	if (path.begins_with(R"(\\?\)") || path.begins_with(R"(\\.\)")) {
		return {};
	}
	wchar_t local[MAX_PATH];
	const DWORD length = GetFullPathNameW(path.c_str(), MAX_PATH, local, nullptr);
	if (length == 0) {
		return {};
	}
	if (length < MAX_PATH) {
		return String(local, length).replace("\\", "/");
	}

	String buffer;
	if (length > INT_MAX || buffer.resize(length) != OK) {
		return {};
	}
	const DWORD actual_length = GetFullPathNameW(path.c_str(), length, buffer.ptrw(), nullptr);
	if (actual_length == 0 || actual_length >= length) {
		return {};
	}
	return String(buffer.c_str(), actual_length).replace("\\", "/");
}

String PathWindowsNT::to_native(const String &p_path, bool p_force_extended) {
	const String path = p_path.replace("/", "\\");
	if (path.empty() || (!p_force_extended && path.length() < MAX_PATH)) {
		return path;
	}
	// Extended paths bypass Win32 normalization. Only add a prefix after
	// get_absolute() resolves separators and navigation components.
	if (path.begins_with(R"(\\)")) {
		return String(R"(\\?\UNC\)") + path.substr(2);
	}
	return String(R"(\\?\)") + path;
}

String PathWindowsNT::get_volume_root(const String &p_path) {
	const String native = to_native(p_path);
	wchar_t root[MAX_PATH];
#if _WIN32_WINNT < 0x0500
	using GetVolumePath = BOOL(WINAPI *)(LPCWSTR, LPWSTR, DWORD);
	static const auto get_volume_path = reinterpret_cast<GetVolumePath>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetVolumePathNameW")));
	if (!get_volume_path) {
		// NT4 has drive and UNC-share roots, without NT5's mounted-folder volumes.
		// Use the native API when this NT4-targeted binary runs on a newer host.
		const String path = get_absolute(p_path);
		if (path.length() >= 3 && path[1] == ':' && path[2] == '/') {
			return path.left(3).replace("/", "\\");
		}
		if (path.begins_with("//")) {
			const int server_end = path.find_char('/', 2);
			if (server_end > 2 && server_end + 1 < path.length()) {
				const int share_end = path.find_char('/', server_end + 1);
				return ((share_end < 0 ? path : path.left(share_end)) + "/").replace("/", "\\");
			}
		}
		return {};
	}
	const bool found = get_volume_path(native.c_str(), root, MAX_PATH);
#else
	const bool found = GetVolumePathNameW(native.c_str(), root, MAX_PATH);
#endif // _WIN32_WINNT < 0x0500
	return found ? String(root) : String();
}

bool PathWindowsNT::is_reserved_filename(const String &p_path) {
	const String name = p_path.get_file().get_slicec('.', 0).strip_edges().to_upper();
	if (name == "CON" || name == "PRN" || name == "AUX" || name == "NUL") {
		return true;
	}
	if (name.length() != 4 || (!name.begins_with("COM") && !name.begins_with("LPT"))) {
		return false;
	}
	const CharType number = name[3];
	return (number >= '1' && number <= '9') || number == 0x00B9 || number == 0x00B2 || number == 0x00B3;
}
