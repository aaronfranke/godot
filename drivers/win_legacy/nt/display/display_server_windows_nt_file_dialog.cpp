#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/templates/local_vector.h"

#include <commdlg.h>
#include <shlobj.h>

using namespace DisplayServerEnums;

struct DisplayServerWindowsNT::FileDialogRequest {
	enum { FILE_BUFFER_SIZE = 32768 };

	Thread thread;
	Callable callback;
	Vector<String> filters;
	Vector<String> paths;
	Char16String title;
	Char16String directory;
	Char16String filename;
	Char16String default_extension;
	HWND owner = nullptr;
	SafeNumeric<uintptr_t> dialog{ 0 };
	SafeNumeric<DWORD> thread_id{ 0 };
	SafeFlag close_requested;
	SafeFlag finished;
	FileDialogMode mode = FILE_DIALOG_MODE_OPEN_FILE;
	int filter_index = 0;
	bool show_hidden = false;

	static void run(void *p_userdata);
	static UINT_PTR CALLBACK file_hook(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_data);
	static int CALLBACK browse_callback(HWND p_window, UINT p_message, LPARAM p_item, LPARAM p_userdata);
	void open_file_dialog();
	void open_browse_dialog();
};

UINT_PTR CALLBACK DisplayServerWindowsNT::FileDialogRequest::file_hook(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_data) {
	if (p_message == WM_INITDIALOG) {
		const auto *options = reinterpret_cast<const OPENFILENAMEW *>(p_data);
		auto *request = reinterpret_cast<FileDialogRequest *>(options->lCustData);
		// The Explorer hook belongs to a child of the actual modal dialog.
		request->dialog.set(reinterpret_cast<uintptr_t>(GetParent(p_window)));
	}
	return 0;
}

int CALLBACK DisplayServerWindowsNT::FileDialogRequest::browse_callback(HWND p_window, UINT p_message, LPARAM p_item, LPARAM p_userdata) {
	auto *request = reinterpret_cast<FileDialogRequest *>(p_userdata);
	if (p_message == BFFM_INITIALIZED) {
		request->dialog.set(reinterpret_cast<uintptr_t>(p_window));
		if (!request->title.is_empty()) {
			SetWindowTextW(p_window, reinterpret_cast<LPCWSTR>(request->title.get_data()));
		}
		if (!request->directory.is_empty()) {
			SendMessageW(p_window, BFFM_SETSELECTIONW, TRUE, reinterpret_cast<LPARAM>(request->directory.get_data()));
		}
	} else if (p_message == BFFM_SELCHANGED) {
		WCHAR path[MAX_PATH];
		const BOOL valid = p_item && SHGetPathFromIDListW(reinterpret_cast<LPCITEMIDLIST>(p_item), path);
		SendMessageW(p_window, BFFM_ENABLEOK, 0, valid);
	}
	return 0;
}

void DisplayServerWindowsNT::FileDialogRequest::open_browse_dialog() {
	WCHAR path[MAX_PATH];
	BROWSEINFOW options = {};
	options.hwndOwner = owner;
	options.pszDisplayName = path;
	options.ulFlags = BIF_USENEWUI | (mode == FILE_DIALOG_MODE_OPEN_DIR ? BIF_RETURNONLYFSDIRS : BIF_BROWSEINCLUDEFILES);
	options.lpfn = browse_callback;
	options.lParam = reinterpret_cast<LPARAM>(this);
	LPITEMIDLIST item = SHBrowseForFolderW(&options);
	if (item) {
		if (SHGetPathFromIDListW(item, path)) {
			paths.push_back(String::utf16(reinterpret_cast<const char16_t *>(path)).replace_char('\\', '/'));
		}
		CoTaskMemFree(item);
	}
}

void DisplayServerWindowsNT::FileDialogRequest::open_file_dialog() {
	LocalVector<WCHAR> filter_list;
	for (const String &filter : filters) {
		Vector<String> globs = filter.get_slicec(';', 0).split(",", false);
		for (int i = 0; i < globs.size(); i++) {
			globs.write[i] = globs[i].strip_edges();
		}
		String patterns = String(";").join(globs);
		if (patterns.is_empty()) {
			patterns = "*.*";
		}
		String description = filter.get_slicec(';', 1).strip_edges();
		if (description.is_empty()) {
			description = patterns;
		}
		if (filter_list.is_empty()) {
			const String first = patterns.get_slicec(';', 0).strip_edges();
			const String extension = first.trim_prefix("*.");
			if (extension != first && !extension.contains_char('*') && !extension.contains_char('?') && !extension.contains_char('.')) {
				default_extension = extension.utf16();
			}
		}
		const Char16String label = description.utf16();
		const Char16String pattern_list = patterns.utf16();
		for (int i = 0; i <= label.length(); i++) {
			filter_list.push_back(label[i]);
		}
		for (int i = 0; i <= pattern_list.length(); i++) {
			filter_list.push_back(pattern_list[i]);
		}
	}
	filter_list.push_back(0);

	WCHAR file[FILE_BUFFER_SIZE] = {};
	memcpy(file, filename.get_data(), filename.length() * sizeof(WCHAR));
	OPENFILENAMEW options = {};
	options.lStructSize = sizeof(options);
#if _WIN32_WINNT < 0x0500
	// MinGW headers can expose the larger layout even for an NT4 target.
	// NT4's COMDLG32 rejects it with CDERR_STRUCTSIZE; newer hosts keep the full layout.
	if (OS::get_singleton()->get_version().begins_with("4.")) {
		options.lStructSize = OPENFILENAME_SIZE_VERSION_400W;
	}
#endif // _WIN32_WINNT < 0x0500
	options.hwndOwner = owner;
	options.lpstrTitle = title.is_empty() ? nullptr : reinterpret_cast<LPCWSTR>(title.get_data());
	options.lpstrInitialDir = directory.is_empty() ? nullptr : reinterpret_cast<LPCWSTR>(directory.get_data());
	options.lpstrFilter = filters.is_empty() ? nullptr : filter_list.ptr();
	options.nFilterIndex = filters.is_empty() ? 0 : 1;
	options.lpstrFile = file;
	options.nMaxFile = FILE_BUFFER_SIZE;
	options.Flags = OFN_EXPLORER | OFN_ENABLEHOOK | OFN_ENABLESIZING | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	options.lpfnHook = file_hook;
	options.lCustData = reinterpret_cast<LPARAM>(this);
	if (show_hidden) {
		options.Flags |= OFN_FORCESHOWHIDDEN;
	}
	const bool save = mode == FILE_DIALOG_MODE_SAVE_FILE;
	if (save) {
		options.Flags |= OFN_OVERWRITEPROMPT;
		options.lpstrDefExt = reinterpret_cast<LPCWSTR>(default_extension.get_data());
	} else {
		options.Flags |= OFN_FILEMUSTEXIST;
		if (mode == FILE_DIALOG_MODE_OPEN_FILES) {
			options.Flags |= OFN_ALLOWMULTISELECT;
		}
	}
	const BOOL accepted = save ? GetSaveFileNameW(&options) : GetOpenFileNameW(&options);
	filter_index = options.nFilterIndex > 0 ? options.nFilterIndex - 1 : 0;
	if (!accepted) {
		const DWORD error = CommDlgExtendedError();
		if (error != 0) {
			ERR_PRINT(vformat("Windows NT file dialog failed (common dialog error %d).", error));
		}
		return;
	}

	const String first = String::utf16(reinterpret_cast<const char16_t *>(file)).replace_char('\\', '/');
	// Explorer returns one complete path, or a directory followed by a list of file names.
	if (mode == FILE_DIALOG_MODE_OPEN_FILES && options.nFileOffset > 0 && options.nFileOffset < FILE_BUFFER_SIZE && file[options.nFileOffset - 1] == 0) {
		const WCHAR *next = file + options.nFileOffset;
		while (*next) {
			paths.push_back(first.path_join(String::utf16(reinterpret_cast<const char16_t *>(next))));
			next += wcslen(next) + 1;
		}
	} else {
		paths.push_back(first);
	}
}

void DisplayServerWindowsNT::FileDialogRequest::run(void *p_userdata) {
	auto *request = static_cast<FileDialogRequest *>(p_userdata);
	request->thread_id.set(GetCurrentThreadId());
	if (!request->close_requested.is_set()) {
		const HRESULT result = OleInitialize(nullptr);
		if (SUCCEEDED(result)) {
			if (request->mode == FILE_DIALOG_MODE_OPEN_DIR || request->mode == FILE_DIALOG_MODE_OPEN_ANY) {
				request->open_browse_dialog();
			} else {
				request->open_file_dialog();
			}
			request->dialog.set(0);
			OleUninitialize();
		} else {
			ERR_PRINT("Could not initialize OLE for the Windows NT file dialog.");
		}
	}
	request->finished.set();
}

Error DisplayServerWindowsNT::file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback, WindowID p_window_id) {
	ERR_FAIL_COND_V(!Thread::is_main_thread(), ERR_UNAVAILABLE);
	if (!has_feature(FEATURE_NATIVE_DIALOG_FILE)) {
		WARN_PRINT_ONCE("Native file dialogs require Shell 4.71 or newer. Use Godot's FileDialog instead.");
		return ERR_UNAVAILABLE;
	}
	ERR_FAIL_INDEX_V(int(p_mode), FILE_DIALOG_MODE_SAVE_MAX, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!p_callback.is_valid(), ERR_INVALID_PARAMETER);
	if (_file_dialog_request || _native_prompt_open) {
		return ERR_BUSY;
	}
	const Char16String filename = p_filename.replace_char('/', '\\').utf16();
	ERR_FAIL_COND_V(filename.length() >= FileDialogRequest::FILE_BUFFER_SIZE, ERR_INVALID_PARAMETER);
	String directory = ProjectSettings::get_singleton()->globalize_path(p_current_directory);
	if (directory == ".") {
		directory = OS::get_singleton()->get_executable_path().get_base_dir();
	}
	if (p_mode == FILE_DIALOG_MODE_OPEN_ANY && !p_filename.is_empty()) {
		directory = directory.path_join(p_filename);
	}
	FileDialogRequest *request = memnew(FileDialogRequest);
	request->owner = p_window_id == MAIN_WINDOW_ID ? _window : nullptr;
	request->callback = p_callback;
	request->mode = p_mode;
	request->title = p_title.utf16();
	request->directory = directory.replace_char('/', '\\').utf16();
	request->filename = filename;
	request->filters = p_filters;
	request->show_hidden = p_show_hidden;
	_file_dialog_request = request;
	if (request->thread.start(FileDialogRequest::run, request) == Thread::UNASSIGNED_ID) {
		_file_dialog_request = nullptr;
		memdelete(request);
		return ERR_CANT_CREATE;
	}
	return OK;
}

void DisplayServerWindowsNT::_process_file_dialog() {
	if (!_file_dialog_request || !_file_dialog_request->finished.is_set()) {
		return;
	}
	FileDialogRequest *request = _file_dialog_request;
	_file_dialog_request = nullptr;
	request->thread.wait_to_finish();
	if (request->callback.is_valid()) {
		request->callback.call(!request->paths.is_empty(), request->paths, request->filter_index);
	}
	memdelete(request);
}

void DisplayServerWindowsNT::_stop_file_dialog() {
	if (!_file_dialog_request) {
		return;
	}
	FileDialogRequest *request = _file_dialog_request;
	request->close_requested.set();
	while (!request->finished.is_set()) {
		const HWND dialog = reinterpret_cast<HWND>(request->dialog.get());
		const DWORD thread_id = request->thread_id.get();
		if (dialog && GetWindowThreadProcessId(dialog, nullptr) == thread_id) {
			const HWND popup = GetLastActivePopup(dialog);
			if (GetWindowThreadProcessId(popup, nullptr) == thread_id) {
				PostMessageW(popup, WM_CLOSE, 0, 0);
			}
		}
		// Modal dialog teardown may send synchronous window messages to its main-thread owner.
		MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_SENDMESSAGE);
		MSG message;
		PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
	}
	request->thread.wait_to_finish();
	_file_dialog_request = nullptr;
	memdelete(request);
}
