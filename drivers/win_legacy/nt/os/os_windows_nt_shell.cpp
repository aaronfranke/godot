#include "drivers/win_legacy/nt/os/os_windows_nt.h"
#include "drivers/win_legacy/nt/path_windows_nt.h"

#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>

String OS_WindowsNT::expand_path(const String &p_path) const {
	String path = p_path;
	if (path == "~" || path.begins_with("~/") || path.begins_with("~\\")) {
		const String home = get_environment("USERPROFILE");
		if (!home.is_empty()) {
			path = home + path.substr(1);
		}
	}

	if (path.find_char('%') < 0 || path.find_char('\0') >= 0) {
		return path;
	}
	const Char16String source = path.utf16();
	const LPCWSTR native = reinterpret_cast<LPCWSTR>(source.get_data());
	const DWORD capacity = ExpandEnvironmentStringsW(native, nullptr, 0);
	Char16String expanded;
	if (!capacity || expanded.resize_uninitialized(capacity) != OK) {
		return path;
	}
	const DWORD length = ExpandEnvironmentStringsW(native, reinterpret_cast<LPWSTR>(expanded.ptrw()), capacity);
	if (!length || length > capacity) {
		return path;
	}
	return String::utf16(expanded.ptr(), length - 1);
}

bool OS_WindowsNT::_get_shell_folder_path(int p_folder, wchar_t *r_path) {
#if _WIN32_WINNT < 0x0500
	using GetFolderPath = HRESULT(WINAPI *)(HWND, int, HANDLE, DWORD, LPWSTR);
	static const auto get_folder_path = reinterpret_cast<GetFolderPath>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"shell32.dll"), "SHGetFolderPathW")));
	if (!get_folder_path) {
		// The original NT4 Shell exposes special folders through item ID lists.
		IMalloc *allocator = nullptr;
		if (SHGetMalloc(&allocator) != S_OK) {
			return false;
		}
		LPITEMIDLIST item = nullptr;
		const HRESULT result = SHGetSpecialFolderLocation(nullptr, p_folder & ~CSIDL_FLAG_DONT_VERIFY, &item);
		const bool found = SUCCEEDED(result) && item && SHGetPathFromIDListW(item, r_path);
		if (item) {
			allocator->Free(item);
		}
		allocator->Release();
		return found;
	}
	return get_folder_path(nullptr, p_folder, nullptr, SHGFP_TYPE_CURRENT, r_path) == S_OK;
#else
	return SHGetFolderPathW(nullptr, p_folder, nullptr, SHGFP_TYPE_CURRENT, r_path) == S_OK;
#endif // _WIN32_WINNT < 0x0500
}

String OS_WindowsNT::get_system_dir(SystemDir p_dir, bool) const {
	int folder = CSIDL_PERSONAL;
	switch (p_dir) {
		case SYSTEM_DIR_DESKTOP:
			folder = CSIDL_DESKTOPDIRECTORY;
			break;
		case SYSTEM_DIR_DCIM:
		case SYSTEM_DIR_PICTURES:
			folder = CSIDL_MYPICTURES;
			break;
		case SYSTEM_DIR_DOCUMENTS:
			break;
		case SYSTEM_DIR_DOWNLOADS: {
			// Downloads has no CSIDL. Resolve the Vista API on systems that export it.
			constexpr GUID DOWNLOADS_FOLDER_ID = { 0x374de290, 0x123f, 0x4565, { 0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b } };
			constexpr DWORD KNOWN_FOLDER_DONT_VERIFY = 0x4000;
			using KnownFolderPath = HRESULT(WINAPI *)(const GUID &, DWORD, HANDLE, PWSTR *);
			const auto known_folder = reinterpret_cast<KnownFolderPath>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"shell32.dll"), "SHGetKnownFolderPath")));
			if (known_folder) {
				PWSTR path = nullptr;
				String downloads;
				if (known_folder(DOWNLOADS_FOLDER_ID, KNOWN_FOLDER_DONT_VERIFY, nullptr, &path) == S_OK && path) {
					downloads = String::utf16(reinterpret_cast<const char16_t *>(path)).replace_char('\\', '/');
				}
				CoTaskMemFree(path);
				if (!downloads.is_empty()) {
					return downloads;
				}
			}
		} break;
		case SYSTEM_DIR_MOVIES:
			folder = CSIDL_MYVIDEO;
			break;
		case SYSTEM_DIR_MUSIC:
		case SYSTEM_DIR_RINGTONES:
			folder = CSIDL_MYMUSIC;
			break;
		default:
			return String();
	}

	wchar_t path[MAX_PATH];
	bool found = _get_shell_folder_path(folder | CSIDL_FLAG_DONT_VERIFY, path);
	if (!found && folder != CSIDL_PERSONAL && folder != CSIDL_DESKTOPDIRECTORY) {
		// Older Shell versions do not have every media folder.
		found = _get_shell_folder_path(CSIDL_PERSONAL | CSIDL_FLAG_DONT_VERIFY, path);
	}
	return found ? String::utf16(reinterpret_cast<const char16_t *>(path)).replace_char('\\', '/') : String();
}

Error OS_WindowsNT::_shell_error(DWORD p_error) {
	switch (p_error) {
		case ERROR_FILE_NOT_FOUND:
		case ERROR_MOD_NOT_FOUND:
			return ERR_FILE_NOT_FOUND;
		case ERROR_PATH_NOT_FOUND:
		case ERROR_INVALID_NAME:
			return ERR_FILE_BAD_PATH;
		case ERROR_BAD_FORMAT:
		case ERROR_BAD_EXE_FORMAT:
			return ERR_FILE_CORRUPT;
		case ERROR_ACCESS_DENIED:
			return ERR_UNAUTHORIZED;
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
			return ERR_OUT_OF_MEMORY;
		case ERROR_NO_ASSOCIATION:
			return ERR_UNAVAILABLE;
		default:
			return FAILED;
	}
}

Error OS_WindowsNT::_shell_execute(const Char16String &p_target, const Char16String &p_arguments) {
	const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE) {
		return FAILED;
	}

	SHELLEXECUTEINFOW request = {};
	request.cbSize = sizeof(request);
	// Headless and worker-thread callers may have no message loop for a later DDE exchange.
	request.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
	request.lpFile = reinterpret_cast<const wchar_t *>(p_target.get_data());
	request.lpParameters = reinterpret_cast<const wchar_t *>(p_arguments.ptr());
	request.nShow = SW_SHOWNORMAL;
	const Error error = ShellExecuteExW(&request) ? OK : _shell_error(GetLastError());

	if (SUCCEEDED(apartment)) {
		CoUninitialize();
	}
	return error;
}

Error OS_WindowsNT::shell_open(const String &p_uri) {
	if (p_uri.is_empty() || p_uri.find_char('\0') >= 0) {
		return ERR_INVALID_PARAMETER;
	}
	const int colon = p_uri.find_char(':');
	const String target = (colon < 0 || colon == 1) ? p_uri.replace_char('/', '\\') : p_uri;
	return _shell_execute(target.utf16());
}

Error OS_WindowsNT::shell_show_in_file_manager(String p_path, bool p_open_folder) {
	if (p_path.find_char('"') >= 0) {
		return ERR_INVALID_PARAMETER;
	}
	const String path = PathWindowsNT::get_absolute(p_path);
	const Char16String native_path = PathWindowsNT::to_native(path);
	if (path.is_empty() || native_path.length() >= MAX_PATH) {
		return ERR_FILE_BAD_PATH;
	}

	const DWORD attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(native_path.get_data()));
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return _shell_error(GetLastError());
	}

	wchar_t windows_dir[MAX_PATH];
#if _WIN32_WINNT < 0x0500
	using GetWindowsPath = UINT(WINAPI *)(LPWSTR, UINT);
	static const auto get_windows_path = reinterpret_cast<GetWindowsPath>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemWindowsDirectoryW")));
	const UINT length = get_windows_path ? get_windows_path(windows_dir, MAX_PATH) : GetWindowsDirectoryW(windows_dir, MAX_PATH);
#else
	const UINT length = GetSystemWindowsDirectoryW(windows_dir, MAX_PATH);
#endif // _WIN32_WINNT < 0x0500
	if (length == 0 || length >= MAX_PATH) {
		return ERR_CANT_OPEN;
	}
	const String explorer = String::utf16(reinterpret_cast<const char16_t *>(windows_dir), length).path_join("explorer.exe").replace_char('/', '\\');
	String arguments = path.replace_char('/', '\\').quote();
	if (!p_open_folder || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		arguments = "/select," + arguments;
	}
	return _shell_execute(explorer.utf16(), arguments.utf16());
}

Error OS_WindowsNT::move_to_trash(const String &p_path) {
	if (!p_path.is_absolute_path() || p_path.find_char('*') >= 0 || p_path.find_char('?') >= 0 || p_path.find_char(':', 2) >= 0) {
		return FAILED;
	}
	const String path = PathWindowsNT::get_absolute(p_path);
	Char16String source = PathWindowsNT::to_native(path);
	if (path.is_empty() || source.length() >= MAX_PATH || PathWindowsNT::is_reserved_filename(path)) {
		return FAILED;
	}
	const wchar_t *native_path = reinterpret_cast<const wchar_t *>(source.get_data());
	const DWORD attributes = GetFileAttributesW(native_path);
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return FAILED;
	}
	if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
		const Char16String volume_root = PathWindowsNT::get_volume_root(path);
		if (volume_root.is_empty()) {
			return FAILED;
		}
		const String root = String::utf16(volume_root.get_data()).replace_char('\\', '/').trim_suffix("/");
		if (path.trim_suffix("/").nocasecmp_to(root) == 0) {
			return FAILED;
		}
	}

	// SHFileOperation expects a double-null-terminated list, even for one path.
	const int length = source.length();
	if (source.resize_uninitialized(length + 2) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	char16_t *source_buffer = source.ptrw();
	source_buffer[length] = 0;
	source_buffer[length + 1] = 0;

	SHFILEOPSTRUCTW operation = {};
	operation.wFunc = FO_DELETE;
	operation.pFrom = reinterpret_cast<const wchar_t *>(source.get_data());
	operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_WANTNUKEWARNING | FOF_NO_CONNECTED_ELEMENTS | FOF_NOERRORUI | FOF_SILENT;
	const int result = SHFileOperationW(&operation);
	return result == 0 && !operation.fAnyOperationsAborted ? OK : FAILED;
}
