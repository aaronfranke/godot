#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/input/input.h"
#include "core/os/main_loop.h"
#include "core/os/os.h"

using namespace DisplayServerEnums;

Vector<char16_t> DisplayServerWindowsNT::_read_ime_string(DWORD p_type) const {
	const LONG bytes = ImmGetCompositionStringW(_ime_context, p_type, nullptr, 0);
	Vector<char16_t> text;
	if (bytes <= 0 || bytes % sizeof(char16_t) != 0 || text.resize(bytes / sizeof(char16_t)) != OK) {
		return text;
	}
	const LONG read = ImmGetCompositionStringW(_ime_context, p_type, text.ptrw(), bytes);
	if (read <= 0 || read > bytes || read % sizeof(char16_t) != 0) {
		text.clear();
	} else {
		text.resize(read / sizeof(char16_t));
	}
	return text;
}

void DisplayServerWindowsNT::_set_ime_preedit(const String &p_text, int p_cursor) {
	if (_ime_text == p_text && _ime_cursor == p_cursor) {
		return;
	}
	_ime_text = p_text;
	_ime_cursor = p_cursor;
	if (MainLoop *loop = OS::get_singleton()->get_main_loop()) {
		loop->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
	}
}

void DisplayServerWindowsNT::_cancel_ime() {
	if (_ime_composing) {
		_ime_composing = false;
		ImmNotifyIME(_ime_context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
	}
	_set_ime_preedit(String(), 0);
}

void DisplayServerWindowsNT::_handle_ime_composition(LPARAM p_flags) {
	if (p_flags & GCS_RESULTSTR) {
		const Vector<char16_t> result = _read_ime_string(GCS_RESULTSTR);
		_set_ime_preedit(String(), 0);
		_high_surrogate = 0;
		for (const char16_t unit : result) {
			_queue_character(unit, 0);
		}
		_high_surrogate = 0;
		// Deliver the commit before applying preedit for the next segment.
		_flush_keys();
		Input::get_singleton()->flush_buffered_events();
	}
	if (_ime_active && _mouse_focused && (p_flags & (GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS))) {
		const Vector<char16_t> text = _read_ime_string(GCS_COMPSTR);
		const int units = text.size();
		const int cursor_utf16 = CLAMP(int(ImmGetCompositionStringW(_ime_context, GCS_CURSORPOS, nullptr, 0)), 0, units);
		int cursor = 0;
		for (int i = 0; i < cursor_utf16; ++i) {
			if (text[i] >= 0xd800 && text[i] <= 0xdbff && i + 1 < units && text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff) {
				if (++i == cursor_utf16) {
					break;
				}
			}
			++cursor;
		}
		_ime_composing = true;
		_set_ime_preedit(String::utf16(text.ptr(), units), cursor);
	} else if (p_flags == 0) {
		_ime_composing = false;
		_set_ime_preedit(String(), 0);
	}
}

void DisplayServerWindowsNT::_update_ime_position() {
	if (!_ime_active) {
		return;
	}
	const POINT position = { _ime_position.x, _ime_position.y };
	COMPOSITIONFORM composition = { CFS_POINT, position, {} };
	ImmSetCompositionWindow(_ime_context, &composition);
	const Rect2i area = _ime_rect.has_area() ? _ime_rect : Rect2i(_ime_position, Size2i(1, 1));
	CANDIDATEFORM candidate = { 0, CFS_EXCLUDE, position, { area.position.x, area.position.y, area.get_end().x, area.get_end().y } };
	ImmSetCandidateWindow(_ime_context, &candidate);
}

String DisplayServerWindowsNT::ime_get_text() const {
	return _ime_text;
}

Point2i DisplayServerWindowsNT::ime_get_selection() const {
	return Point2i(_ime_cursor, 0);
}

void DisplayServerWindowsNT::window_set_ime_active(bool p_active, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID || !_ime_context || p_active == _ime_active) {
		return;
	}
	_ime_active = p_active;
	if (_ime_active) {
		ImmAssociateContext(_window, _ime_context);
		_update_ime_position();
	} else {
		_cancel_ime();
		ImmAssociateContext(_window, nullptr);
	}
}

void DisplayServerWindowsNT::window_set_ime_position(const Point2i &p_position, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID && p_position != _ime_position) {
		_ime_position = p_position;
		_update_ime_position();
	}
}

void DisplayServerWindowsNT::window_set_ime_rect(const Rect2i &p_rect, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID && p_rect != _ime_rect) {
		_ime_rect = p_rect;
		_update_ime_position();
	}
}
