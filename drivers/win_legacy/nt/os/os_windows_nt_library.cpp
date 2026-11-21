#include "drivers/win_legacy/nt/os/os_windows_nt.h"

// Godot includes.
#include "core/io/file_access.h"
#include "drivers/win_legacy/nt/path_windows_nt.h"

Error OS_WindowsNT::open_dynamic_library(const String &p_path, void *&r_library_handle, GDExtensionData *p_data) {
	r_library_handle = nullptr;
	if (p_data && p_data->generate_temp_files) {
		// Temporary DLL copies for editor hot reload are not implemented.
		return ERR_UNAVAILABLE;
	}

	String path = PathWindowsNT::get_absolute(p_path);
	if (path.is_empty() || PathWindowsNT::is_reserved_filename(path)) {
		return ERR_FILE_BAD_PATH;
	}
	const Ref<FileAccess> filesystem = FileAccess::create(FileAccess::ACCESS_FILESYSTEM);
	if (!filesystem->file_exists(path)) {
		// PC exports place the library beside the executable, outside the project pack.
		const String executable = get_executable_path();
		if (executable.is_empty()) {
			return ERR_FILE_NOT_FOUND;
		}
		path = executable.get_base_dir().path_join(path.get_file());
		ERR_FAIL_COND_V_MSG(!filesystem->file_exists(path), ERR_FILE_NOT_FOUND, vformat("Dynamic library not found: %s.", p_path));
	}

	DWORD flags = 0;
	if (p_data && p_data->also_set_library_path) {
		const bool has_search_flags = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "AddDllDirectory") != nullptr;
		if (has_search_flags) {
			flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
		} else {
			// Find sibling dependencies on older hosts without changing the global DLL search path.
			flags = LOAD_WITH_ALTERED_SEARCH_PATH;
		}
	}
	const Char16String native_path = PathWindowsNT::to_native(path);
	r_library_handle = LoadLibraryExW(reinterpret_cast<const wchar_t *>(native_path.get_data()), nullptr, flags);
	if (!r_library_handle) {
		const DWORD error = GetLastError();
		ERR_FAIL_V_MSG(ERR_CANT_OPEN, vformat("Could not load dynamic library \"%s\" (Windows error %d).", path, error));
	}
	if (p_data && p_data->r_resolved_path) {
		*p_data->r_resolved_path = path;
	}
	return OK;
}

Error OS_WindowsNT::close_dynamic_library(void *p_library_handle) {
	return p_library_handle && FreeLibrary(static_cast<HMODULE>(p_library_handle)) ? OK : FAILED;
}

Error OS_WindowsNT::get_dynamic_library_symbol_handle(void *p_library_handle, const String &p_name, void *&r_symbol_handle, bool p_optional) {
	r_symbol_handle = nullptr;
	ERR_FAIL_NULL_V(p_library_handle, ERR_INVALID_PARAMETER);
	const CharString name = p_name.utf8();
	r_symbol_handle = reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(p_library_handle), name.get_data()));
	if (!r_symbol_handle) {
		if (!p_optional) {
			const DWORD error = GetLastError();
			ERR_FAIL_V_MSG(ERR_CANT_RESOLVE, vformat("Could not resolve dynamic library symbol \"%s\" (Windows error %d).", p_name, error));
		}
		return ERR_CANT_RESOLVE;
	}
	return OK;
}
