#include "display_server_windows_9x.h"
#include "encoding_windows_9x.h"

#include "core/input/input.h"
#include "core/os/main_loop.h"
#include "core/os/os.h"

#include "platform/windows/key_mapping_windows.h"

#include <windowsx.h>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif // WM_DPICHANGED

using namespace DisplayServerEnums;

void DisplayServerWindows9x::_dispatch_input(const Ref<InputEvent> &p_event) {
	auto *server = static_cast<DisplayServerWindows9x *>(get_singleton());
	if (server && server->_input_event.is_valid()) {
		server->_input_event.call(p_event);
	}
}

void DisplayServerWindows9x::_set_modifiers(const Ref<InputEventWithModifiers> &p_event) {
	p_event->set_shift_pressed(GetKeyState(VK_SHIFT) < 0);
	p_event->set_ctrl_pressed(GetKeyState(VK_CONTROL) < 0);
	p_event->set_alt_pressed(GetKeyState(VK_MENU) < 0);
	p_event->set_meta_pressed(GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0);
}

void DisplayServerWindows9x::_queue_key(UINT p_message, WPARAM p_key, LPARAM p_flags) {
	const unsigned int scan = (p_flags >> 16) & 0xff;
	const bool extended = (p_flags & (1 << 24)) != 0;
	Key logical = KeyMappingWindows::get_keysym(p_key);
	if (p_key == VK_RETURN && extended) {
		logical = Key::KP_ENTER;
	}
	Ref<InputEventKey> event;
	event.instantiate();
	event->set_window_id(MAIN_WINDOW_ID);
	event->set_keycode(logical);
	event->set_key_label(logical);
	event->set_physical_keycode(KeyMappingWindows::get_scansym(scan, extended));
	event->set_location(KeyMappingWindows::get_location(scan, extended));
	const bool pressed = p_message == WM_KEYDOWN || p_message == WM_SYSKEYDOWN;
	event->set_pressed(pressed);
	event->set_echo(pressed && (p_flags & (1 << 30)) != 0);
	_set_modifiers(event);
	// A modifier key does not include itself in its modifier mask.
	if (logical == Key::SHIFT) {
		event->set_shift_pressed(false);
	} else if (logical == Key::CTRL) {
		event->set_ctrl_pressed(false);
	} else if (logical == Key::ALT) {
		event->set_alt_pressed(false);
	} else if (logical == Key::META) {
		event->set_meta_pressed(false);
	}
	_keys.push_back({ event, uint16_t((p_flags >> 16) & 0x1ff) });
}

void DisplayServerWindows9x::_queue_character(uint8_t p_byte, LPARAM p_flags) {
	if (_character_length == 0) {
		_character_expected = 1;
		if (GetACP() == CP_UTF8) {
			// Modern NT hosts can select UTF-8 as their ANSI code page.
			if (p_byte >= 0xc2 && p_byte <= 0xdf) {
				_character_expected = 2;
			} else if (p_byte >= 0xe0 && p_byte <= 0xef) {
				_character_expected = 3;
			} else if (p_byte >= 0xf0 && p_byte <= 0xf4) {
				_character_expected = 4;
			}
		} else if (IsDBCSLeadByteEx(CP_ACP, p_byte)) {
			// DefWindowProcA splits an IME's double-byte character into two WM_CHARs.
			_character_expected = 2;
		}
	}
	_character_bytes[_character_length++] = char(p_byte);
	if (_character_length < _character_expected) {
		return;
	}
	const String text = Windows9xEncoding::decode(_character_bytes, _character_length);
	_character_length = 0;
	for (int i = 0; i < text.length(); ++i) {
		const char32_t character = text[i];
		if (character < 32) {
			continue;
		}
		const uint16_t scan = (p_flags >> 16) & 0x1ff;
		bool matched = false;
		for (const KeyEvent &key : _keys) {
			if (scan && key.scan == scan && key.event->is_pressed() && key.event->get_unicode() == 0) {
				key.event->set_unicode(character);
				key.event->set_key_label(fix_key_label(character, key.event->get_keycode()));
				matched = true;
				break;
			}
		}
		if (!matched) {
			Ref<InputEventKey> event;
			event.instantiate();
			event->set_window_id(MAIN_WINDOW_ID);
			event->set_pressed(true);
			event->set_unicode(character);
			_set_modifiers(event);
			_keys.push_back({ event, 0 });
		}
	}
}

void DisplayServerWindows9x::_flush_keys() {
	const Vector<KeyEvent> keys = std::move(_keys);
	for (const KeyEvent &key : keys) {
		Input::get_singleton()->parse_input_event(key.event);
	}
}

void DisplayServerWindows9x::_send_window_event(WindowEvent p_event) {
	if (_window_event.is_valid()) {
		_window_event.call(p_event);
	}
}

Point2i DisplayServerWindows9x::mouse_get_position() const {
	POINT position;
	return GetCursorPos(&position) ? Point2i(position.x, position.y) : Point2i();
}

void DisplayServerWindows9x::mouse_set_mode(MouseMode p_mode) {
	ERR_FAIL_INDEX(p_mode, MOUSE_MODE_MAX);
	_mouse_mode_base = p_mode;
	_update_effective_mouse_mode();
}

MouseMode DisplayServerWindows9x::mouse_get_mode() const {
	return _mouse_mode;
}

void DisplayServerWindows9x::mouse_set_mode_override(MouseMode p_mode) {
	ERR_FAIL_INDEX(p_mode, MOUSE_MODE_MAX);
	_mouse_mode_override = p_mode;
	_update_effective_mouse_mode();
}

MouseMode DisplayServerWindows9x::mouse_get_mode_override() const {
	return _mouse_mode_override;
}

void DisplayServerWindows9x::mouse_set_mode_override_enabled(bool p_enabled) {
	_mouse_mode_override_enabled = p_enabled;
	_update_effective_mouse_mode();
}

bool DisplayServerWindows9x::mouse_is_mode_override_enabled() const {
	return _mouse_mode_override_enabled;
}

void DisplayServerWindows9x::_update_effective_mouse_mode() {
	const MouseMode mode = _mouse_mode_override_enabled ? _mouse_mode_override : _mouse_mode_base;
	if (_mouse_mode != mode) {
		_mouse_mode = mode;
		_mouse_position_valid = false;
		_update_mouse_mode(true);
	}
}

void DisplayServerWindows9x::_release_mouse_clip() {
	if (_mouse_clip_rect.has_area()) {
		ClipCursor(nullptr);
		_mouse_clip_rect = Rect2i();
	}
}

bool DisplayServerWindows9x::_recenter_mouse() {
	const Point2i center = _mouse_clip_rect.get_center();
	return SetCursorPos(center.x, center.y);
}

void DisplayServerWindows9x::_update_mouse_mode(bool p_recenter) {
	const bool focused = _shown && !_mouse_focus_pending && GetFocus() == _window && GetForegroundWindow() == _window && !IsIconic(_window);
	const bool confined = _mouse_mode == MOUSE_MODE_CAPTURED || _mouse_mode == MOUSE_MODE_CONFINED || _mouse_mode == MOUSE_MODE_CONFINED_HIDDEN;
	const Rect2i clip = focused && confined ? _get_client_rect() : Rect2i();
	const bool changed = clip != _mouse_clip_rect;
	if (changed && clip.has_area()) {
		const RECT bounds = { clip.position.x, clip.position.y, clip.get_end().x, clip.get_end().y };
		if (ClipCursor(&bounds)) {
			_mouse_clip_rect = clip;
		} else {
			_release_mouse_clip();
		}
	} else if (!clip.has_area()) {
		_release_mouse_clip();
	}
	if (_mouse_mode == MOUSE_MODE_CAPTURED && _mouse_clip_rect.has_area() && (p_recenter || changed)) {
		if (_recenter_mouse()) {
			_last_mouse_position = _mouse_clip_rect.size / 2;
			Input::get_singleton()->set_mouse_position(_last_mouse_position);
		} else {
			_release_mouse_clip();
		}
	}
	_apply_mouse_cursor();
}

BitField<MouseButtonMask> DisplayServerWindows9x::mouse_get_button_state() const {
	return _buttons;
}

void DisplayServerWindows9x::warp_mouse(const Point2i &p_position) {
	if (_mouse_mode == MOUSE_MODE_CAPTURED || GetFocus() != _window) {
		return;
	}
	POINT position = { p_position.x, p_position.y };
	if (ClientToScreen(_window, &position)) {
		SetCursorPos(position.x, position.y);
	}
}

void DisplayServerWindows9x::cursor_set_shape(CursorShape p_shape) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);
	_cursor_shape = p_shape;
	_apply_mouse_cursor();
}

void DisplayServerWindows9x::_apply_mouse_cursor() {
	// SetCursor affects the desktop cursor. Leave another application's cursor alone.
	POINT position;
	if (!GetCursorPos(&position) || WindowFromPoint(position) != _window) {
		return;
	}
	POINT client = position;
	RECT bounds;
	if (!ScreenToClient(_window, &client) || !GetClientRect(_window, &bounds) || !PtInRect(&bounds, client)) {
		return;
	}
	const bool focused = !_mouse_focus_pending && GetFocus() == _window && GetForegroundWindow() == _window && !IsIconic(_window);
	if (focused && (_mouse_mode == MOUSE_MODE_HIDDEN || (_mouse_clip_rect.has_area() && (_mouse_mode == MOUSE_MODE_CAPTURED || _mouse_mode == MOUSE_MODE_CONFINED_HIDDEN)))) {
		SetCursor(nullptr);
		return;
	}
	const HCURSOR custom = _custom_cursors[_cursor_shape];
	SetCursor(custom ? custom : _get_system_cursor(_cursor_shape));
}

CursorShape DisplayServerWindows9x::cursor_get_shape() const {
	return _cursor_shape;
}

void DisplayServerWindows9x::_send_mouse_motion(const Point2i &p_position) {
	Point2i position = p_position;
	Point2i relative = _mouse_position_valid ? position - _last_mouse_position : Point2i();
	if (_mouse_mode == MOUSE_MODE_CAPTURED) {
		if (!_mouse_clip_rect.has_area()) {
			return;
		}
		POINT cursor;
		if (!GetCursorPos(&cursor)) {
			return;
		}
		// Ignore queued coordinates from before focus recovery and recentering.
		relative = Point2i(cursor.x, cursor.y) - _mouse_clip_rect.get_center();
		position = _mouse_clip_rect.size / 2;
		if (relative == Point2i()) {
			return;
		}
		if (!_recenter_mouse()) {
			_release_mouse_clip();
			_apply_mouse_cursor();
			return;
		}
	}
	if (!_mouse_inside) {
		_mouse_inside = true;
		_send_window_event(WINDOW_EVENT_MOUSE_ENTER);
	}
	Ref<InputEventMouseMotion> event;
	event.instantiate();
	event->set_window_id(MAIN_WINDOW_ID);
	event->set_position(position);
	event->set_global_position(position);
	event->set_relative(relative);
	event->set_relative_screen_position(event->get_relative());
	event->set_button_mask(_buttons);
	_set_modifiers(event);
	_last_mouse_position = position;
	_mouse_position_valid = true;
	Input::get_singleton()->parse_input_event(event);
}

void DisplayServerWindows9x::_send_mouse_button(MouseButton p_button, const Point2i &p_position, bool p_pressed, bool p_double_click, float p_factor) {
	if (p_button < MouseButton::WHEEL_UP || p_button > MouseButton::WHEEL_RIGHT) {
		if (p_pressed) {
			_buttons.set_flag(mouse_button_to_mask(p_button));
			SetCapture(_window);
		} else {
			_buttons.clear_flag(mouse_button_to_mask(p_button));
			if (_buttons.is_empty() && GetCapture() == _window) {
				ReleaseCapture();
			}
		}
	}
	Ref<InputEventMouseButton> event;
	event.instantiate();
	event->set_window_id(MAIN_WINDOW_ID);
	event->set_button_index(p_button);
	event->set_button_mask(_buttons);
	const Point2i position = _mouse_mode == MOUSE_MODE_CAPTURED && _mouse_clip_rect.has_area() ? _mouse_clip_rect.size / 2 : p_position;
	event->set_position(position);
	event->set_global_position(position);
	event->set_pressed(p_pressed);
	event->set_double_click(p_double_click);
	event->set_factor(p_factor);
	_set_modifiers(event);
	Input::get_singleton()->parse_input_event(event);
}

void DisplayServerWindows9x::_release_mouse_buttons() {
	const MouseButton buttons[] = { MouseButton::LEFT, MouseButton::RIGHT, MouseButton::MIDDLE };
	for (MouseButton button : buttons) {
		if (_buttons.has_flag(mouse_button_to_mask(button))) {
			_send_mouse_button(button, _last_mouse_position, false);
		}
	}
}

LRESULT DisplayServerWindows9x::_handle_message(UINT p_message, WPARAM p_wparam, LPARAM p_lparam) {
	switch (p_message) {
		case WM_CLOSE:
			_send_window_event(WINDOW_EVENT_CLOSE_REQUEST);
			return 0;
		case WM_DROPFILES:
			_handle_file_drop(p_wparam);
			return 0;
		case WM_GETMINMAXINFO: {
			if (_fullscreen) {
				break;
			}
			MINMAXINFO *info = reinterpret_cast<MINMAXINFO *>(p_lparam);
			DefWindowProcA(_window, p_message, p_wparam, p_lparam);
			const Size2i minimum = _get_frame_size(_min_size);
			// Godot supplies its own minimum, which can be smaller than the native
			// caption/button minimum on a modern high-DPI desktop.
			if (_min_size.x > 0) {
				info->ptMinTrackSize.x = minimum.x;
			}
			if (_min_size.y > 0) {
				info->ptMinTrackSize.y = minimum.y;
			}
			const Size2i limit = _get_size_limit();
			const Size2i maximum = _get_frame_size(limit);
			if (limit.x > 0) {
				info->ptMaxTrackSize.x = maximum.x;
				info->ptMaxSize.x = MIN(info->ptMaxSize.x, maximum.x);
				info->ptMinTrackSize.x = MIN(info->ptMinTrackSize.x, maximum.x);
			}
			if (limit.y > 0) {
				info->ptMaxTrackSize.y = maximum.y;
				info->ptMaxSize.y = MIN(info->ptMaxSize.y, maximum.y);
				info->ptMinTrackSize.y = MIN(info->ptMinTrackSize.y, maximum.y);
			}
			return 0;
		}
		case WM_ACTIVATEAPP: {
			MainLoop *loop = OS::get_singleton()->get_main_loop();
			if (loop) {
				_flush_keys();
				if (!p_wparam && Input::get_singleton()->is_ignoring_joypad_on_unfocused_application()) {
					_joypad.release_inputs();
				}
				loop->notification(p_wparam ? MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN : MainLoop::NOTIFICATION_APPLICATION_FOCUS_OUT);
			}
		} break;
		case WM_SETFOCUS:
			_mouse_position_valid = false;
			_mouse_focus_pending = true;
			_send_window_event(WINDOW_EVENT_FOCUS_IN);
			return 0;
		case WM_KILLFOCUS:
			_mouse_focus_pending = true;
			_release_mouse_clip();
			_flush_keys();
			_release_mouse_buttons();
			Input::get_singleton()->release_pressed_events();
			_character_length = 0;
			_mouse_position_valid = false;
			_send_window_event(WINDOW_EVENT_FOCUS_OUT);
			_apply_mouse_cursor();
			return 0;
		case WM_CANCELMODE:
		case WM_CAPTURECHANGED:
			_release_mouse_buttons();
			break;
		case WM_DISPLAYCHANGE:
			_display_changed = true;
			break;
		case WM_DPICHANGED:
			if (_fullscreen) {
				_display_changed = true;
			} else {
				const RECT *suggested = reinterpret_cast<const RECT *>(p_lparam);
				SetWindowPos(_window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOACTIVATE | SWP_NOZORDER);
			}
			return 0;
		case WM_SETCURSOR:
			if (LOWORD(p_lparam) == HTCLIENT) {
				_apply_mouse_cursor();
				return TRUE;
			}
			break;
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT: {
			PAINTSTRUCT paint;
			BeginPaint(_window, &paint);
			EndPaint(_window, &paint);
			return 0;
		}
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			if (!_drop_input) {
				_queue_key(p_message, p_wparam, p_lparam);
			}
			// Preserve system shortcuts such as Alt+F4 through DefWindowProcA.
			if (p_message == WM_KEYDOWN || p_message == WM_KEYUP) {
				return 0;
			}
			break;
		case WM_CHAR:
			if (!_drop_input) {
				_queue_character(uint8_t(p_wparam), p_lparam);
			}
			return 0;
		case WM_INPUTLANGCHANGE:
			_character_length = 0;
			break;
		case WM_MOUSEMOVE:
			if (!_drop_input) {
				_send_mouse_motion(Point2i(GET_X_LPARAM(p_lparam), GET_Y_LPARAM(p_lparam)));
			}
			return 0;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK: {
			if (_drop_input) {
				return 0;
			}
			MouseButton button = MouseButton::LEFT;
			if (p_message >= WM_MBUTTONDOWN) {
				button = MouseButton::MIDDLE;
			} else if (p_message >= WM_RBUTTONDOWN) {
				button = MouseButton::RIGHT;
			}
			const bool pressed = p_message != WM_LBUTTONUP && p_message != WM_RBUTTONUP && p_message != WM_MBUTTONUP;
			const bool double_click = p_message == WM_LBUTTONDBLCLK || p_message == WM_RBUTTONDBLCLK || p_message == WM_MBUTTONDBLCLK;
			_send_mouse_button(button, Point2i(GET_X_LPARAM(p_lparam), GET_Y_LPARAM(p_lparam)), pressed, double_click);
			return 0;
		}
		case WM_MOUSEWHEEL: {
			const int delta = GET_WHEEL_DELTA_WPARAM(p_wparam);
			if (!_drop_input && delta) {
				POINT point = { GET_X_LPARAM(p_lparam), GET_Y_LPARAM(p_lparam) };
				ScreenToClient(_window, &point);
				const MouseButton button = delta > 0 ? MouseButton::WHEEL_UP : MouseButton::WHEEL_DOWN;
				const float factor = Math::abs(delta) / float(WHEEL_DELTA);
				_send_mouse_button(button, Point2i(point.x, point.y), true, false, factor);
				_send_mouse_button(button, Point2i(point.x, point.y), false, false, factor);
			}
			return 0;
		}
	}
	return DefWindowProcA(_window, p_message, p_wparam, p_lparam);
}

void DisplayServerWindows9x::_pump_messages() {
	MSG message;
	while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
		if (message.message == WM_QUIT) {
			_send_window_event(WINDOW_EVENT_CLOSE_REQUEST);
			continue;
		}
		if (!_drop_input) {
			TranslateMessage(&message);
		}
		DispatchMessageA(&message);
	}
	if (_mouse_focus_pending && GetFocus() == _window && GetForegroundWindow() == _window) {
		_mouse_focus_pending = false;
		_update_mouse_mode(true);
	}
	POINT position;
	if (_mouse_inside && GetCursorPos(&position) && WindowFromPoint(position) != _window) {
		_mouse_inside = false;
		_mouse_position_valid = false;
		_send_window_event(WINDOW_EVENT_MOUSE_EXIT);
	}
}

void DisplayServerWindows9x::process_events() {
	_pump_messages();
	_flush_keys();
	_update_client_rect();
	_update_mouse_mode(false);
	_joypad.process();
	Input::get_singleton()->flush_buffered_events();
}

void DisplayServerWindows9x::force_process_and_drop_events() {
	_drop_input = true;
	_pump_messages();
	_drop_input = false;
	_update_client_rect();
}
