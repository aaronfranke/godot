#include "os_windows_9x.h"
#include "path_windows_9x.h"

#include "core/os/file_access.h"

Error OS_Windows9x::open_dynamic_library(const String p_path, void *&r_library_handle, bool p_also_set_library_path) {
	r_library_handle = nullptr;

	String path = PathWindows9x::get_absolute(p_path);
	if (path.empty() || PathWindows9x::is_reserved_filename(path)) {
		return ERR_FILE_BAD_PATH;
	}
	FileAccessRef filesystem = FileAccess::create(FileAccess::ACCESS_FILESYSTEM);
	if (!filesystem->file_exists(path)) {
		// Exported libraries are native files beside the executable, outside the project pack.
		const String executable = get_executable_path();
		if (executable.empty()) {
			return ERR_FILE_NOT_FOUND;
		}
		path = executable.get_base_dir().plus_file(path.get_file());
		ERR_FAIL_COND_V_MSG(!filesystem->file_exists(path), ERR_FILE_NOT_FOUND, vformat("Dynamic library not found: %s.", p_path));
	}
	const CharString native = PathWindows9x::to_native(path);
	if (native.length() == 0) {
		return ERR_FILE_BAD_PATH;
	}
	DWORD flags = 0;
	if (p_also_set_library_path) {
		// 9x has no AddDllDirectory. Alter the search for this load without changing the process CWD.
		const bool has_search_flags = GetProcAddress(GetModuleHandleA("kernel32.dll"), "AddDllDirectory") != nullptr;
		flags = has_search_flags ? LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS : LOAD_WITH_ALTERED_SEARCH_PATH;
	}
	r_library_handle = LoadLibraryExA(native.get_data(), nullptr, flags);
	if (!r_library_handle) {
		const DWORD error = GetLastError();
		ERR_FAIL_V_MSG(ERR_CANT_OPEN, vformat("Could not load dynamic library \"%s\" (Windows error %d).", path, uint32_t(error)));
	}

	return OK;
}

Error OS_Windows9x::close_dynamic_library(void *p_library_handle) {
	return p_library_handle && FreeLibrary(static_cast<HMODULE>(p_library_handle)) ? OK : FAILED;
}

Error OS_Windows9x::get_dynamic_library_symbol_handle(void *p_library_handle, const String p_name, void *&r_symbol_handle, bool p_optional) {
	r_symbol_handle = nullptr;
	ERR_FAIL_NULL_V(p_library_handle, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_name.empty() || _has_embedded_null(p_name), ERR_INVALID_PARAMETER);
	const CharString name = p_name.utf8();
	r_symbol_handle = reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(p_library_handle), name.get_data()));
	if (!r_symbol_handle) {
		if (!p_optional) {
			const DWORD error = GetLastError();
			ERR_FAIL_V_MSG(ERR_CANT_RESOLVE, vformat("Could not resolve dynamic library symbol \"%s\" (Windows error %d).", p_name, uint32_t(error)));
		}
		return ERR_CANT_RESOLVE;
	}
	return OK;
}
