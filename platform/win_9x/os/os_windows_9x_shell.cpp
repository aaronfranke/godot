#include "encoding_windows_9x.h"
#include "os_windows_9x.h"
#include "path_windows_9x.h"

#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>

String OS_Windows9x::_get_shell_folder_path(int p_folder) {
	String path;
	// Newer hosts can return Unicode paths without an ANSI best-fit substitution.
	// Neither SHGetFolderPathW nor the known-folder API is a static 9x dependency.
	typedef HRESULT(WINAPI * FolderPathFunction)(HWND, int, HANDLE, DWORD, LPWSTR);
	const auto folder_path = reinterpret_cast<FolderPathFunction>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleA("shell32.dll"), "SHGetFolderPathW")));
	if (folder_path) {
		// The Microsoft SDK hides SHGFP_TYPE_CURRENT at the Windows 9x header floor.
		enum { FOLDER_PATH_CURRENT = 0 };
		wchar_t native[MAX_PATH] = {};
		if (folder_path(nullptr, p_folder | CSIDL_FLAG_DONT_VERIFY, nullptr, FOLDER_PATH_CURRENT, native) == S_OK) {
			path = String(native);
		}
	}
	if (path.empty()) {
		// SHGetSpecialFolderLocation and the Shell allocator are available on Windows 95.
		IMalloc *allocator = nullptr;
		if (SHGetMalloc(&allocator) == S_OK && allocator) {
			LPITEMIDLIST item = nullptr;
			if (SHGetSpecialFolderLocation(nullptr, p_folder, &item) == S_OK && item) {
				char native[MAX_PATH] = {};
				if (SHGetPathFromIDListA(item, native)) {
					path = Windows9xEncoding::decode(native);
				}
			}
			if (item) {
				allocator->Free(item);
			}
			allocator->Release();
		}
	}
	return path.replace("\\", "/");
}

String OS_Windows9x::get_system_dir(SystemDir p_dir, bool) const {
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
			const GUID DOWNLOADS_FOLDER_ID = { 0x374de290, 0x123f, 0x4565, { 0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b } };
			enum { KNOWN_FOLDER_DONT_VERIFY = 0x4000 };
			typedef HRESULT(WINAPI * KnownFolderPathFunction)(const GUID &, DWORD, HANDLE, PWSTR *);
			const auto known_folder = reinterpret_cast<KnownFolderPathFunction>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleA("shell32.dll"), "SHGetKnownFolderPath")));
			if (known_folder) {
				PWSTR native = nullptr;
				String path;
				if (known_folder(DOWNLOADS_FOLDER_ID, KNOWN_FOLDER_DONT_VERIFY, nullptr, &native) == S_OK && native) {
					path = String(native);
				}
				CoTaskMemFree(native);
				if (!path.empty()) {
					return path.replace("\\", "/");
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
			return {};
	}
	String path = _get_shell_folder_path(folder);
	if (path.empty() && folder != CSIDL_PERSONAL && folder != CSIDL_DESKTOPDIRECTORY) {
		// Older Shell versions have no Downloads and may have no separate media folders.
		path = _get_shell_folder_path(CSIDL_PERSONAL);
	}
	return path;
}

String OS_Windows9x::expand_path(const String &p_path) const {
	if (_has_embedded_null(p_path)) {
		return p_path;
	}
	String path = p_path;
	if (path == "~" || path.begins_with("~/") || path.begins_with("~\\")) {
		String home = get_environment("USERPROFILE");
		if (home.empty()) {
			home = _get_shell_folder_path(CSIDL_PROFILE);
		}
		if (home.empty()) {
			// Default 9x profiles keep Application Data under the Windows directory.
			home = get_config_path().get_base_dir();
		}
		if (!home.empty()) {
			path = home + path.substr(1);
		}
	}
	// Expand environment references on the CPU so a Unicode suffix need not fit the ACP.
	// Like ExpandEnvironmentStrings, substitute once and retain undefined references.
	String expanded;
	int from = 0;
	while (from < path.length()) {
		const int start = path.find("%", from);
		const int end = start < 0 ? -1 : path.find("%", start + 1);
		if (end < 0) {
			expanded += path.substr(from);
			break;
		}
		expanded += path.substr(from, start - from);
		const String name = path.substr(start + 1, end - start - 1);
		expanded += has_environment(name) ? get_environment(name) : path.substr(start, end - start + 1);
		from = end + 1;
	}
	return expanded;
}

Error OS_Windows9x::_shell_error(DWORD p_error) {
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

Error OS_Windows9x::_shell_execute(const CharString &p_target, const CharString &p_arguments) {
	// CoInitialize provides the STA baseline without requiring DCOM's CoInitializeEx on 95.
	const HRESULT apartment = CoInitialize(nullptr);
	if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE) {
		return FAILED;
	}
	SHELLEXECUTEINFOA request = {};
	request.cbSize = sizeof(request);
	// Headless and worker-thread callers may not pump messages for a later DDE exchange.
	request.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
	request.lpFile = p_target.get_data();
	request.lpParameters = p_arguments.get_data();
	request.nShow = SW_SHOWNORMAL;
	const Error error = ShellExecuteExA(&request) ? OK : _shell_error(GetLastError());
	if (SUCCEEDED(apartment)) {
		CoUninitialize();
	}
	return error;
}

Error OS_Windows9x::shell_open(String p_uri) {
	if (p_uri.empty() || _has_embedded_null(p_uri)) {
		return ERR_INVALID_PARAMETER;
	}
	const int colon = p_uri.find(":");
	const String target = (colon < 0 || colon == 1) ? p_uri.replace("/", "\\") : p_uri;
	CharString native;
	if (Windows9xEncoding::encode(target, native) != OK) {
		return ERR_FILE_BAD_PATH;
	}
	return _shell_execute(native);
}
