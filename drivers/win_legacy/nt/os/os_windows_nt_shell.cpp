#include "drivers/win_legacy/nt/os/os_windows_nt.h"

// Legacy Windows includes.
#include "drivers/win_legacy/nt/path_windows_nt.h"

#include <limits.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>

String OS_WindowsNT::expand_path(const String &p_path) const {
	String path = p_path;
	if (path == "~" || path.begins_with("~/") || path.begins_with("~\\")) {
		const String home = get_environment("USERPROFILE");
		if (!home.empty()) {
			path = home + path.substr(1);
		}
	}

	if (path.find_char('%') < 0 || PathWindowsNT::has_embedded_null(path)) {
		return path;
	}
	const DWORD capacity = ExpandEnvironmentStringsW(path.c_str(), nullptr, 0);
	String expanded;
	if (!capacity || capacity > INT_MAX || expanded.resize(capacity) != OK) {
		return path;
	}
	const DWORD length = ExpandEnvironmentStringsW(path.c_str(), expanded.ptrw(), capacity);
	if (!length || length > capacity) {
		return path;
	}
	return String(expanded.c_str(), length - 1);
}

bool OS_WindowsNT::_get_shell_folder_path(int p_folder, wchar_t *r_path) {
#if _WIN32_WINNT < 0x0500
	using GetFolderPath = HRESULT(WINAPI *)(HWND, int, HANDLE, DWORD, LPWSTR);
	static const auto get_folder_path = reinterpret_cast<GetFolderPath>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"shell32.dll"), "SHGetFolderPathW")));
	if (!get_folder_path) {
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
			static const auto known_folder = reinterpret_cast<KnownFolderPath>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"shell32.dll"), "SHGetKnownFolderPath")));
			if (known_folder) {
				PWSTR path = nullptr;
				String downloads;
				if (known_folder(DOWNLOADS_FOLDER_ID, KNOWN_FOLDER_DONT_VERIFY, nullptr, &path) == S_OK && path) {
					downloads = String(path).replace("\\", "/");
				}
				CoTaskMemFree(path);
				if (!downloads.empty()) {
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
	return found ? String(path).replace("\\", "/") : String();
}

Error OS_WindowsNT::shell_open(String p_uri) {
	if (p_uri.empty() || PathWindowsNT::has_embedded_null(p_uri)) {
		return ERR_INVALID_PARAMETER;
	}

	const int colon = p_uri.find_char(':');
	if (colon < 0 || colon == 1) {
		p_uri = p_uri.replace("/", "\\");
	}

	const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE) {
		return FAILED;
	}

	SHELLEXECUTEINFOW request = {};
	request.cbSize = sizeof(request);
	// A headless caller or worker may have no message loop for a later DDE exchange.
	request.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
	request.lpFile = p_uri.c_str();
	request.nShow = SW_SHOWNORMAL;
	const BOOL accepted = ShellExecuteExW(&request);
	const DWORD error = accepted ? ERROR_SUCCESS : GetLastError();
	if (SUCCEEDED(apartment)) {
		CoUninitialize();
	}
	if (accepted) {
		return OK;
	}

	switch (error) {
		case ERROR_FILE_NOT_FOUND:
		case ERROR_MOD_NOT_FOUND:
		case ERROR_DLL_NOT_FOUND:
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

Error OS_WindowsNT::move_to_trash(const String &p_path) {
	if (PathWindowsNT::has_embedded_null(p_path)) {
		return FAILED;
	}
	const String windows_path = p_path.replace("/", "\\");
	const CharType drive = windows_path.length() >= 3 ? windows_path[0] : 0;
	const bool drive_letter = (drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z');
	const bool drive_path = drive_letter && windows_path[1] == ':' && windows_path[2] == '\\';
	const bool unc_path = windows_path.begins_with(R"(\\)");
	if ((!drive_path && !unc_path) || windows_path.find_char('*') >= 0 || windows_path.find_char('?') >= 0 || windows_path.find_char(':', 2) >= 0) {
		return FAILED;
	}
	const String path = PathWindowsNT::get_absolute(windows_path);
	if (path.empty() || path.length() >= MAX_PATH || PathWindowsNT::is_reserved_filename(path)) {
		return FAILED;
	}

	String source = PathWindowsNT::to_native(path);
	const DWORD attributes = GetFileAttributesW(source.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return FAILED;
	}
	if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
		const String volume_root = PathWindowsNT::get_volume_root(path);
		if (volume_root.empty()) {
			return FAILED;
		}
		const String root = volume_root.replace("\\", "/").trim_suffix("/");
		if (path.trim_suffix("/").nocasecmp_to(root) == 0) {
			return FAILED;
		}
	}

	// The Shell takes a double-null-terminated list even for a single path.
	const int length = source.length();
	if (source.resize(length + 2) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	WCHAR *buffer = source.ptrw();
	buffer[length] = 0;
	buffer[length + 1] = 0;

	SHFILEOPSTRUCTW operation = {};
	operation.wFunc = FO_DELETE;
	operation.pFrom = buffer;
	operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_WANTNUKEWARNING | FOF_NO_CONNECTED_ELEMENTS | FOF_NOERRORUI | FOF_SILENT;
	const int result = SHFileOperationW(&operation);
	return result == 0 && !operation.fAnyOperationsAborted ? OK : FAILED;
}
