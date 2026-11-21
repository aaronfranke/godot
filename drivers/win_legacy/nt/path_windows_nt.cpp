#include "path_windows_nt.h"

#include <windows.h>

String PathWindowsNT::get_absolute(const String &p_path) {
	if (p_path.is_empty() || p_path.find_char('\0') >= 0) {
		return {};
	}

	const String path = p_path.replace_char('/', '\\');
	if (path.begins_with(R"(\\?\)") || path.begins_with(R"(\\.\)")) {
		return {};
	}
	const Char16String native = path.utf16();
	wchar_t local[MAX_PATH];
	const DWORD length = GetFullPathNameW((LPCWSTR)native.get_data(), MAX_PATH, local, nullptr);
	if (length == 0) {
		return {};
	}
	if (length < MAX_PATH) {
		return String::utf16(reinterpret_cast<const char16_t *>(local), length).replace_char('\\', '/');
	}

	Char16String buffer;
	buffer.resize_uninitialized(length);
	const DWORD actual_length = GetFullPathNameW((LPCWSTR)native.get_data(), length, (LPWSTR)buffer.ptrw(), nullptr);
	if (actual_length == 0 || actual_length >= length) {
		return {};
	}
	return String::utf16(buffer.get_data(), actual_length).replace_char('\\', '/');
}

Char16String PathWindowsNT::to_native(const String &p_path, bool p_force_extended) {
	const String path = p_path.replace_char('/', '\\');
	const Char16String wide_path = path.utf16();
	if (wide_path.is_empty() || (!p_force_extended && wide_path.length() < MAX_PATH)) {
		return wide_path;
	}
	// Extended paths bypass Win32 normalization. Only add a prefix after
	// get_absolute() resolves separators and navigation components.
	if (path.begins_with(R"(\\)")) {
		return (String(R"(\\?\UNC\)") + path.substr(2)).utf16();
	}
	return (String(R"(\\?\)") + path).utf16();
}

Char16String PathWindowsNT::get_volume_root(const String &p_path) {
	const Char16String native = to_native(p_path);
	wchar_t root[MAX_PATH];
#if _WIN32_WINNT < 0x0500
	using GetVolumePath = BOOL(WINAPI *)(LPCWSTR, LPWSTR, DWORD);
	static const auto get_volume_path = reinterpret_cast<GetVolumePath>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetVolumePathNameW")));
	if (!get_volume_path) {
		// NT4 has drive and UNC-share roots, without NT5's mounted-folder volumes.
		// Use the native API when this NT4-targeted binary runs on a newer host.
		const String path = get_absolute(p_path);
		if (path.length() >= 3 && path[1] == ':' && path[2] == '/') {
			return path.left(3).replace_char('/', '\\').utf16();
		}
		if (path.begins_with("//")) {
			const int server_end = path.find_char('/', 2);
			if (server_end > 2 && server_end + 1 < path.length()) {
				const int share_end = path.find_char('/', server_end + 1);
				return ((share_end < 0 ? path : path.left(share_end)) + "/").replace_char('/', '\\').utf16();
			}
		}
		return {};
	}
	const bool found = get_volume_path(reinterpret_cast<LPCWSTR>(native.get_data()), root, MAX_PATH);
#else
	const bool found = GetVolumePathNameW(reinterpret_cast<LPCWSTR>(native.get_data()), root, MAX_PATH);
#endif // _WIN32_WINNT < 0x0500
	if (!found) {
		return {};
	}
	return String::utf16(reinterpret_cast<const char16_t *>(root)).utf16();
}

bool PathWindowsNT::is_reserved_filename(const String &p_path) {
	const String name = p_path.get_file().get_slicec('.', 0).strip_edges().to_upper();
	if (name == "CON" || name == "PRN" || name == "AUX" || name == "NUL") {
		return true;
	}
	if (name.length() != 4 || (!name.begins_with("COM") && !name.begins_with("LPT"))) {
		return false;
	}
	const char32_t number = name[3];
	return (number >= '1' && number <= '9') || number == 0x00B9 || number == 0x00B2 || number == 0x00B3;
}
