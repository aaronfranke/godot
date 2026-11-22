#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/os/main_loop.h"
#include "main/input_default.h"

Vector<WCHAR> DisplayServerWindowsNT::_read_ime_string(DWORD p_type) const {
	const LONG bytes = ImmGetCompositionStringW(_ime_context, p_type, nullptr, 0);
	Vector<WCHAR> text;
	if (bytes <= 0 || bytes % sizeof(WCHAR) != 0 || text.resize(bytes / sizeof(WCHAR)) != OK) {
		return text;
	}
	const LONG read = ImmGetCompositionStringW(_ime_context, p_type, text.ptrw(), bytes);
	if (read <= 0 || read > bytes || read % sizeof(WCHAR) != 0) {
		text.clear();
	} else {
		text.resize(read / sizeof(WCHAR));
	}
	return text;
}

void DisplayServerWindowsNT::_set_ime_preedit(const String &p_text, int p_cursor) {
	if (_ime_text == p_text && _ime_cursor == p_cursor) {
		return;
	}
	_ime_text = p_text;
	_ime_cursor = p_cursor;
	_notify(MainLoop::NOTIFICATION_OS_IME_UPDATE);
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
		const Vector<WCHAR> result = _read_ime_string(GCS_RESULTSTR);
		_set_ime_preedit(String(), 0);
		_high_surrogate = 0;
		for (int i = 0; i < result.size(); ++i) {
			_queue_character(result[i], 0);
		}
		_high_surrogate = 0;
		// Deliver the commit before applying preedit for the next segment.
		_flush_keys();
		_input->flush_buffered_events();
	}
	if (_ime_active && _mouse_focused && (p_flags & (GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS))) {
		const Vector<WCHAR> text = _read_ime_string(GCS_COMPSTR);
		// On Windows, 3.x text control offsets are already in UTF-16 code units.
		const int cursor = CLAMP(int(ImmGetCompositionStringW(_ime_context, GCS_CURSORPOS, nullptr, 0)), 0, text.size());
		_ime_composing = true;
		_set_ime_preedit(text.empty() ? String() : String(text.ptr(), text.size()), cursor);
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
	CANDIDATEFORM candidate = { 0, CFS_EXCLUDE, position, { position.x, position.y, position.x + 1, position.y + 1 } };
	ImmSetCandidateWindow(_ime_context, &candidate);
}

void DisplayServerWindowsNT::ime_set_active(bool p_active) {
	if (!_ime_context || _ime_active == p_active) {
		return;
	}
	_ime_active = p_active;
	if (p_active) {
		ImmAssociateContext(_window, _ime_context);
		_update_ime_position();
	} else {
		_cancel_ime();
		ImmAssociateContext(_window, nullptr);
	}
}

void DisplayServerWindowsNT::ime_set_position(const Point2i &p_position) {
	if (p_position != _ime_position) {
		_ime_position = p_position;
		_update_ime_position();
	}
}

String DisplayServerWindowsNT::ime_get_text() const {
	return _ime_text;
}

Point2i DisplayServerWindowsNT::ime_get_selection() const {
	return Point2i(_ime_cursor, 0);
}
