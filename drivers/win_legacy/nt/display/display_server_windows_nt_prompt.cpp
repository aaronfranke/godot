#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/object.h"
#include "core/os/thread.h"

#include <limits.h>

class NativePromptWindowsNT {
	enum {
		TEXT_ID = 10,
		FIRST_BUTTON_ID = 100,
	};

	String _title;
	String _description;
	String _existing_text;
	String _text;
	Vector<String> _buttons;
	Rect2i _work_area;
	bool _text_input;

	static HWND _create_control(HWND p_dialog, LPCWSTR p_class, LPCWSTR p_text, DWORD p_style, const Rect2i &p_rect, int p_id, DWORD p_ex_style = 0) {
		const HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(p_dialog, GWLP_HINSTANCE));
		const HWND control = CreateWindowExW(p_ex_style, p_class, p_text, WS_CHILD | WS_VISIBLE | p_style,
				p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, p_dialog,
				reinterpret_cast<HMENU>(INT_PTR(p_id)), instance, nullptr);
		if (control) {
			SendMessageW(control, WM_SETFONT, SendMessageW(p_dialog, WM_GETFONT, 0, 0), FALSE);
		}
		return control;
	}

	bool _layout(HWND p_dialog) {
		SetWindowTextW(p_dialog, reinterpret_cast<LPCWSTR>(_title.c_str()));
		RECT units = { 0, 0, 240, 14 };
		MapDialogRect(p_dialog, &units);
		const int button_height = MAX(1, int(units.bottom));
		const int margin = MAX(8, button_height / 2);
		const int gap = MAX(4, button_height / 4);

		RECT border = {};
		AdjustWindowRectEx(&border, GetWindowLongW(p_dialog, GWL_STYLE), FALSE, GetWindowLongW(p_dialog, GWL_EXSTYLE));
		const Size2i frame(border.right - border.left, border.bottom - border.top);
		// Keep long messages scrollable instead of filling the screen, leaving room
		// for themed frames that extend beyond the legacy window metrics.
		const Size2i max_size(_work_area.size.x, _work_area.size.y * 3 / 4);
		const Size2i available = max_size - frame - Size2i(2 * margin, 2 * margin);
		if (available.x <= 0 || available.y <= 0) {
			return false;
		}

		const HDC dc = GetDC(p_dialog);
		if (!dc) {
			return false;
		}
		const HGDIOBJ previous_font = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(p_dialog, WM_GETFONT, 0, 0)));
		int button_width = 3 * button_height;
		for (int i = 0; i < _buttons.size(); i++) {
			const String &caption = _buttons[i];
			RECT bounds = {};
			DrawTextW(dc, reinterpret_cast<LPCWSTR>(caption.c_str()), caption.length(), &bounds, DT_SINGLELINE | DT_CALCRECT);
			button_width = MAX(button_width, int(bounds.right) + 2 * gap);
		}
		button_width = MIN(button_width, available.x);
		const int width = MIN(available.x, MAX(int(units.right), _buttons.size() * (button_width + gap) - gap));
		const int columns = (width + gap) / (button_width + gap);
		const int rows = (_buttons.size() + columns - 1) / columns;
		const int button_block = rows * (button_height + gap) - gap;
		const int input_block = _text_input ? button_height + gap : 0;
		RECT text_bounds = { 0, 0, width, 0 };
		const int text_height = DrawTextW(dc, reinterpret_cast<LPCWSTR>(_description.c_str()), _description.length(), &text_bounds, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
		SelectObject(dc, previous_font);
		ReleaseDC(p_dialog, dc);

		const int max_text_height = available.y - button_block - input_block - (text_height > 0 ? gap : 0);
		if (max_text_height < (text_height > 0 ? button_height : 0)) {
			return false;
		}
		const int visible_text_height = MIN(text_height, max_text_height);
		const int text_block = visible_text_height > 0 ? visible_text_height + gap : 0;
		const Size2i outer = Size2i(width + 2 * margin, button_block + input_block + text_block + 2 * margin) + frame;
		const Point2i position = _work_area.position + (_work_area.size - outer) / 2;
		if (!SetWindowPos(p_dialog, nullptr, position.x, position.y, outer.x, outer.y, SWP_NOZORDER | SWP_NOACTIVATE)) {
			return false;
		}

		int y = margin;
		if (visible_text_height > 0) {
			const Rect2i rect(margin, y, width, visible_text_height);
			HWND label;
			if (visible_text_height < text_height) {
				label = _create_control(p_dialog, L"Edit", reinterpret_cast<LPCWSTR>(_description.c_str()), WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_READONLY, rect, 0, WS_EX_CLIENTEDGE);
			} else {
				label = _create_control(p_dialog, L"Static", reinterpret_cast<LPCWSTR>(_description.c_str()), SS_LEFT | SS_NOPREFIX, rect, 0);
			}
			if (!label) {
				return false;
			}
			y += text_block;
		}
		HWND focus = nullptr;
		if (_text_input) {
			focus = _create_control(p_dialog, L"Edit", reinterpret_cast<LPCWSTR>(_existing_text.c_str()), WS_TABSTOP | ES_AUTOHSCROLL, Rect2i(margin, y, width, button_height), TEXT_ID, WS_EX_CLIENTEDGE);
			if (!focus) {
				return false;
			}
			y += input_block;
			SendMessageW(focus, EM_SETSEL, 0, -1);
		}
		for (int i = 0; i < _buttons.size(); i++) {
			const int column = i % columns;
			const int count = MIN(columns, _buttons.size() - (i - column));
			const int x = margin + width - (count - column) * (button_width + gap) + gap;
			int id = FIRST_BUTTON_ID + i;
			if (_text_input) {
				id = i == 0 ? IDOK : IDCANCEL;
			}
			const String &caption = _buttons[i];
			const Rect2i rect(x, y + (i / columns) * (button_height + gap), button_width, button_height);
			const HWND button = _create_control(p_dialog, L"Button", reinterpret_cast<LPCWSTR>(caption.c_str()), WS_TABSTOP | (i == 0 ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), rect, id);
			if (!button) {
				return false;
			}
			if (i == 0) {
				SendMessageW(p_dialog, DM_SETDEFID, id, 0);
				if (!focus) {
					focus = button;
				}
			}
		}
		SetFocus(focus);
		return true;
	}

	static INT_PTR CALLBACK _dialog_proc(HWND p_dialog, UINT p_message, WPARAM p_wparam, LPARAM p_data) {
		if (p_message == WM_INITDIALOG) {
			auto *prompt = reinterpret_cast<NativePromptWindowsNT *>(p_data);
			SetWindowLongPtrW(p_dialog, DWLP_USER, p_data);
			if (!prompt->_layout(p_dialog)) {
				EndDialog(p_dialog, -1);
			}
			return FALSE; // Focus was set explicitly by the layout.
		}
		if (p_message == WM_CLOSE || (p_message == WM_COMMAND && LOWORD(p_wparam) == IDCANCEL)) {
			EndDialog(p_dialog, IDCANCEL);
			return TRUE;
		}
		if (p_message != WM_COMMAND || HIWORD(p_wparam) != BN_CLICKED) {
			return FALSE;
		}
		auto *prompt = reinterpret_cast<NativePromptWindowsNT *>(GetWindowLongPtrW(p_dialog, DWLP_USER));
		const int id = LOWORD(p_wparam);
		if (prompt->_text_input && id == IDOK) {
			const HWND edit = GetDlgItem(p_dialog, TEXT_ID);
			const int length = GetWindowTextLengthW(edit);
			Vector<WCHAR> text;
			if (length == INT_MAX || text.resize(length + 1) != OK) {
				EndDialog(p_dialog, -1);
				return TRUE;
			}
			const int copied = GetWindowTextW(edit, reinterpret_cast<LPWSTR>(text.ptrw()), text.size());
			prompt->_text = String(text.ptr(), copied);
			EndDialog(p_dialog, IDOK);
			return TRUE;
		}
		if (!prompt->_text_input && id >= FIRST_BUTTON_ID && id - FIRST_BUTTON_ID < prompt->_buttons.size()) {
			EndDialog(p_dialog, id);
			return TRUE;
		}
		return FALSE;
	}

public:
	Error show(HWND p_owner, Object *p_object, const String &p_callback, bool &r_open) {
		ERR_FAIL_COND_V(_buttons.size() > 0x10000 - FIRST_BUTTON_ID, ERR_INVALID_PARAMETER);
		// The fixed header has no resource menu or controls. Layout uses the system dialog font.
		alignas(DWORD) const struct {
			DLGTEMPLATE header;
			WORD menu;
			WORD window_class;
			WCHAR title;
			WORD font_size;
			WCHAR font[13];
		} dialog = { { DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 0, 0, 240, 30 }, 0, 0, 0, 8, L"MS Shell Dlg" };

		// The modal message loop may destroy the callback object. Resolve its ID afterward.
		const ObjectID callback_id = p_object ? p_object->get_instance_id() : 0;
		r_open = true;
		const INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &dialog.header, p_owner, _dialog_proc, reinterpret_cast<LPARAM>(this));
		r_open = false;
		if (result <= 0) {
			return ERR_CANT_CREATE;
		}
		Object *callback = ObjectDB::get_instance(callback_id);
		if (callback && !p_callback.empty()) {
			// Godot 3.x uses deferred Object callbacks; text callbacks receive success and text.
			if (_text_input) {
				callback->call_deferred(p_callback, result == IDOK, _text);
			} else {
				callback->call_deferred(p_callback, result == IDCANCEL ? -1 : int(result) - FIRST_BUTTON_ID);
			}
		}
		return OK;
	}

	NativePromptWindowsNT(const String &p_title, const String &p_description, const Vector<String> &p_buttons, const String *p_existing_text, const Rect2i &p_work_area) :
			_title(p_title),
			_description(p_description.replace("\r\n", "\n").replace("\n", "\r\n")),
			_existing_text(p_existing_text ? *p_existing_text : String()),
			_buttons(p_buttons),
			_work_area(p_work_area),
			_text_input(p_existing_text != nullptr) {}
};

Error DisplayServerWindowsNT::_show_native_prompt(const String &p_title, const String &p_description, const Vector<String> &p_buttons, const String *p_existing_text, Object *p_object, const String &p_callback) {
	ERR_FAIL_COND_V(Thread::get_caller_id() != Thread::get_main_id(), ERR_UNAVAILABLE);
	if (_native_prompt_open) {
		return ERR_BUSY;
	}
	Rect2i work_area = _get_monitor_rect(_get_window_monitor(_window, MONITOR_DEFAULTTONEAREST), true);
	if (work_area.has_no_area()) {
		work_area = Rect2i(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
	}
	NativePromptWindowsNT prompt(p_title, p_description, p_buttons, p_existing_text, work_area);
	return prompt.show(_window, p_object, p_callback, _native_prompt_open);
}

Error DisplayServerWindowsNT::dialog_show(String p_title, String p_description, Vector<String> p_buttons, Object *p_object, const String &p_callback) {
	if (p_buttons.empty()) {
		p_buttons.push_back(RTR("OK"));
	}
	return _show_native_prompt(p_title, p_description, p_buttons, nullptr, p_object, p_callback);
}

Error DisplayServerWindowsNT::dialog_input_text(String p_title, String p_description, String p_partial, Object *p_object, const String &p_callback) {
	ERR_FAIL_COND_V(!p_object || p_callback.empty(), FAILED);
	Vector<String> buttons;
	buttons.push_back(RTR("OK"));
	buttons.push_back(RTR("Cancel"));
	return _show_native_prompt(p_title, p_description, buttons, &p_partial, p_object, p_callback);
}
