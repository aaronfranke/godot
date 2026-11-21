#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/input/input.h"
#include "core/os/main_loop.h"
#include "core/os/os.h"

#include "platform/windows/key_mapping_windows.h"
#include "platform/windows/native_menu_windows.h"

#include <limits.h>
#include <shellapi.h>
#include <windowsx.h>

// Vista and later can send this message even when the build targets Windows 2000.
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif // WM_MOUSEHWHEEL

using namespace DisplayServerEnums;

void DisplayServerWindowsNT::_dispatch_input(const Ref<InputEvent> &p_event) {
	auto *server = static_cast<DisplayServerWindowsNT *>(get_singleton());
	if (server && server->_input_event.is_valid()) {
		server->_input_event.call(p_event);
	}
}

void DisplayServerWindowsNT::_handle_file_drop(WPARAM p_drop) {
	const HDROP drop = reinterpret_cast<HDROP>(p_drop);
	if (!drop) {
		return;
	}
	const Callable callback = _drop_files_callback;
	Vector<String> paths;
	if (!_drop_input && callback.is_valid()) {
		const UINT count = DragQueryFileW(drop, 0xffffffff, nullptr, 0);
		Char16String path;
		for (UINT i = 0; i < count; ++i) {
			const UINT length = DragQueryFileW(drop, i, nullptr, 0);
			if (length == 0 || length >= INT_MAX || path.resize_uninitialized(int64_t(length) + 1) != OK) {
				continue;
			}
			if (DragQueryFileW(drop, i, reinterpret_cast<LPWSTR>(path.ptrw()), length + 1) == length) {
				paths.push_back(String::utf16(path.ptr(), int(length)));
			}
		}
	}
	DragFinish(drop);
	if (!paths.is_empty()) {
		callback.call(paths);
	}
}

void DisplayServerWindowsNT::_queue_key(UINT p_message, WPARAM p_key, LPARAM p_flags) {
	const bool extended = (p_flags & (1 << 24)) != 0;
	const unsigned int scan = (p_flags >> 16) & 0xff;
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
	event->set_shift_pressed(logical != Key::SHIFT && GetKeyState(VK_SHIFT) < 0);
	event->set_ctrl_pressed(logical != Key::CTRL && GetKeyState(VK_CONTROL) < 0);
	event->set_alt_pressed(logical != Key::ALT && GetKeyState(VK_MENU) < 0);
	event->set_meta_pressed(logical != Key::META && (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0));
	_keys.push_back({ event, uint16_t((p_flags >> 16) & 0x1ff) });
}

void DisplayServerWindowsNT::_queue_character(char16_t p_unit, LPARAM p_flags) {
	char32_t character = p_unit;
	if (p_unit >= 0xd800 && p_unit <= 0xdbff) {
		_high_surrogate = p_unit;
		return;
	}
	if (p_unit >= 0xdc00 && p_unit <= 0xdfff) {
		if (!_high_surrogate) {
			return;
		}
		character = 0x10000 + ((uint32_t(_high_surrogate) - 0xd800) << 10) + (p_unit - 0xdc00);
	}
	_high_surrogate = 0;
	character = fix_unicode(character);
	if (character < 32) {
		return;
	}

	// TranslateMessage appends WM_CHAR to the queue. Match it to the oldest
	// untranslated key press with the same scan code, even if key-up is queued.
	const uint16_t scan = (p_flags >> 16) & 0x1ff;
	if (scan != 0) {
		for (const KeyEvent &key : _keys) {
			if (key.scan == scan && key.event->is_pressed() && key.event->get_unicode() == 0) {
				key.event->set_unicode(character);
				key.event->set_key_label(fix_key_label(character, key.event->get_keycode()));
				return;
			}
		}
	}
	Ref<InputEventKey> event;
	event.instantiate();
	event->set_window_id(MAIN_WINDOW_ID);
	event->set_pressed(true);
	event->set_unicode(character);
	_keys.push_back({ event, 0 });
}

void DisplayServerWindowsNT::_flush_keys() {
	const Vector<KeyEvent> keys = std::move(_keys);
	for (const KeyEvent &key : keys) {
		Input::get_singleton()->parse_input_event(key.event);
	}
}

void DisplayServerWindowsNT::mouse_set_mode(MouseMode p_mode) {
	ERR_FAIL_INDEX(p_mode, MOUSE_MODE_MAX);
	_mouse_mode_base = p_mode;
	_update_effective_mouse_mode();
}

MouseMode DisplayServerWindowsNT::mouse_get_mode() const {
	return _mouse_mode;
}

void DisplayServerWindowsNT::mouse_set_mode_override(MouseMode p_mode) {
	ERR_FAIL_INDEX(p_mode, MOUSE_MODE_MAX);
	_mouse_mode_override = p_mode;
	_update_effective_mouse_mode();
}

MouseMode DisplayServerWindowsNT::mouse_get_mode_override() const {
	return _mouse_mode_override;
}

void DisplayServerWindowsNT::mouse_set_mode_override_enabled(bool p_enabled) {
	_mouse_mode_override_enabled = p_enabled;
	_update_effective_mouse_mode();
}

bool DisplayServerWindowsNT::mouse_is_mode_override_enabled() const {
	return _mouse_mode_override_enabled;
}

void DisplayServerWindowsNT::_update_effective_mouse_mode() {
	const MouseMode mode = _mouse_mode_override_enabled ? _mouse_mode_override : _mouse_mode_base;
	if (mode != _mouse_mode) {
		_mouse_mode = mode;
		_mouse_position_valid = false;
		_update_mouse_mode(true);
	}
}

void DisplayServerWindowsNT::warp_mouse(const Point2i &p_position) {
	if (!_mouse_focused || _mouse_mode == MOUSE_MODE_CAPTURED) {
		return;
	}
	POINT point = { p_position.x, p_position.y };
	if (ClientToScreen(_window, &point)) {
		SetCursorPos(point.x, point.y);
	}
}

bool DisplayServerWindowsNT::_mouse_hides_cursor() const {
	if (!_mouse_focused || _mouse_focus_pending || IsIconic(_window)) {
		return false;
	}
	return _mouse_mode == MOUSE_MODE_HIDDEN ||
			(_mouse_clip_rect.has_area() && (_mouse_mode == MOUSE_MODE_CAPTURED || _mouse_mode == MOUSE_MODE_CONFINED_HIDDEN));
}

void DisplayServerWindowsNT::_release_mouse_clip() {
	if (_mouse_clip_rect.has_area()) {
		ClipCursor(nullptr);
		_mouse_clip_rect = Rect2i();
	}
}

bool DisplayServerWindowsNT::_recenter_mouse() {
	const Point2i center = _mouse_clip_rect.get_center();
	return SetCursorPos(center.x, center.y);
}

void DisplayServerWindowsNT::_update_mouse_mode(bool p_recenter) {
	const bool confined = _mouse_mode == MOUSE_MODE_CAPTURED || _mouse_mode == MOUSE_MODE_CONFINED || _mouse_mode == MOUSE_MODE_CONFINED_HIDDEN;
	Rect2i clip;
	if (confined && _mouse_focused && !_mouse_focus_pending && !IsIconic(_window)) {
		clip = _get_client_rect();
	}
	const bool clip_changed = clip != _mouse_clip_rect;
	if (clip_changed && clip.has_area()) {
		const RECT native = { clip.position.x, clip.position.y, clip.get_end().x, clip.get_end().y };
		if (ClipCursor(&native)) {
			_mouse_clip_rect = clip;
		} else {
			_release_mouse_clip();
		}
	} else if (!clip.has_area()) {
		_release_mouse_clip();
	}

	if (_mouse_mode == MOUSE_MODE_CAPTURED && _mouse_clip_rect.has_area() && (p_recenter || clip_changed)) {
		if (_recenter_mouse()) {
			_last_mouse_position = _mouse_clip_rect.size / 2;
			Input::get_singleton()->set_mouse_position(_last_mouse_position);
		} else {
			_release_mouse_clip();
		}
	}
	_update_mouse_cursor();
}

Point2i DisplayServerWindowsNT::mouse_get_position() const {
	POINT point;
	return GetCursorPos(&point) ? Point2i(point.x, point.y) - _get_screens_origin() : Point2i();
}

BitField<MouseButtonMask> DisplayServerWindowsNT::mouse_get_button_state() const {
	return _buttons;
}

void DisplayServerWindowsNT::_send_mouse_motion(const Point2i &p_position) {
	Point2i position = p_position;
	Point2i relative;
	if (_mouse_mode == MOUSE_MODE_CAPTURED) {
		if (!_mouse_clip_rect.has_area()) {
			return;
		}
		// A queued WM_MOUSEMOVE can still describe the cursor before recentering,
		// especially when the window has just regained focus.
		POINT cursor;
		if (!GetCursorPos(&cursor)) {
			return;
		}
		position = _mouse_clip_rect.size / 2;
		relative = Point2i(cursor.x, cursor.y) - _mouse_clip_rect.get_center();
		if (relative == Point2i()) {
			return;
		}
		if (!_recenter_mouse()) {
			_release_mouse_clip();
			_update_mouse_cursor();
			return;
		}
	} else if (!_mouse_inside) {
		TRACKMOUSEEVENT tracking = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, _window, 0 };
		TrackMouseEvent(&tracking);
		_mouse_inside = true;
		_send_window_event(WINDOW_EVENT_MOUSE_ENTER);
	} else if (_mouse_position_valid) {
		relative = position - _last_mouse_position;
	}
	Ref<InputEventMouseMotion> event;
	event.instantiate();
	event->set_window_id(MAIN_WINDOW_ID);
	event->set_position(position);
	event->set_global_position(position);
	event->set_relative(relative);
	event->set_relative_screen_position(relative);
	event->set_button_mask(_buttons);
	event->set_shift_pressed(GetKeyState(VK_SHIFT) < 0);
	event->set_ctrl_pressed(GetKeyState(VK_CONTROL) < 0);
	event->set_alt_pressed(GetKeyState(VK_MENU) < 0);
	event->set_meta_pressed(GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0);
	_last_mouse_position = position;
	_mouse_position_valid = true;
	Input::get_singleton()->parse_input_event(event);
}

void DisplayServerWindowsNT::_send_mouse_button(MouseButton p_button, const Point2i &p_position, bool p_pressed, bool p_double_click, float p_factor) {
	const bool wheel = p_button >= MouseButton::WHEEL_UP && p_button <= MouseButton::WHEEL_RIGHT;
	if (!wheel) {
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
	event->set_shift_pressed(GetKeyState(VK_SHIFT) < 0);
	event->set_ctrl_pressed(GetKeyState(VK_CONTROL) < 0);
	event->set_alt_pressed(GetKeyState(VK_MENU) < 0);
	event->set_meta_pressed(GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0);
	Input::get_singleton()->parse_input_event(event);
}

void DisplayServerWindowsNT::_release_mouse_buttons() {
	const MouseButton buttons[] = { MouseButton::LEFT, MouseButton::RIGHT, MouseButton::MIDDLE, MouseButton::MB_XBUTTON1, MouseButton::MB_XBUTTON2 };
	for (MouseButton button : buttons) {
		if (_buttons.has_flag(mouse_button_to_mask(button))) {
			_send_mouse_button(button, _last_mouse_position, false, false);
		}
	}
}

LRESULT DisplayServerWindowsNT::_handle_message(UINT p_message, WPARAM p_wparam, LPARAM p_lparam) {
	if (_taskbar_created_message && p_message == _taskbar_created_message) {
		_restore_status_indicators();
		return 0;
	}
	switch (p_message) {
		case WM_STATUS_INDICATOR:
			_dispatch_status_indicator(IndicatorID(p_wparam), p_lparam);
			return 0;
		case WM_MENUCOMMAND:
			if (_native_menu) {
				static_cast<NativeMenuWindows *>(_native_menu)->_menu_activate(reinterpret_cast<HMENU>(p_lparam), int(p_wparam));
			}
			return 0;
		case WM_GETMINMAXINFO: {
			if (_fullscreen) {
				break;
			}
			auto *info = reinterpret_cast<MINMAXINFO *>(p_lparam);
			const Size2i minimum = _get_frame_size(_min_size);
			const Size2i maximum = _get_frame_size(_get_size_limit());
			const Size2i limit = _get_size_limit();
			if (limit.x) {
				info->ptMaxTrackSize.x = MIN(info->ptMaxTrackSize.x, LONG(maximum.x));
				info->ptMaxSize.x = MIN(info->ptMaxSize.x, LONG(maximum.x));
			}
			if (limit.y) {
				info->ptMaxTrackSize.y = MIN(info->ptMaxTrackSize.y, LONG(maximum.y));
				info->ptMaxSize.y = MIN(info->ptMaxSize.y, LONG(maximum.y));
			}
			info->ptMinTrackSize.x = MIN(info->ptMaxTrackSize.x, MAX(info->ptMinTrackSize.x, LONG(minimum.x)));
			info->ptMinTrackSize.y = MIN(info->ptMaxTrackSize.y, MAX(info->ptMinTrackSize.y, LONG(minimum.y)));
			return 0;
		}
		case WM_CLOSE:
			_send_window_event(WINDOW_EVENT_CLOSE_REQUEST);
			return 0;
		case WM_DROPFILES:
			_handle_file_drop(p_wparam);
			return 0;
		case WM_ACTIVATEAPP: {
			MainLoop *loop = OS::get_singleton()->get_main_loop();
			if (loop) {
				_flush_keys();
				if (!p_wparam && Input::get_singleton()->is_ignoring_joypad_on_unfocused_application()) {
					_joypad.release_inputs();
				}
				loop->notification(p_wparam ? MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN : MainLoop::NOTIFICATION_APPLICATION_FOCUS_OUT);
			}
			break;
		}
		case WM_SETFOCUS:
			_mouse_focused = true;
			_mouse_focus_pending = true;
			_mouse_position_valid = false;
			_send_window_event(WINDOW_EVENT_FOCUS_IN);
			return 0;
		case WM_KILLFOCUS:
			_mouse_focused = false;
			_cancel_ime();
			_mouse_focus_pending = false;
			_mouse_position_valid = false;
			_update_mouse_mode();
			_high_surrogate = 0;
			_flush_keys();
			_release_mouse_buttons();
			_send_window_event(WINDOW_EVENT_FOCUS_OUT);
			return 0;
		case WM_DISPLAYCHANGE:
			_display_change_pending = true;
			_fullscreen_bounds_dirty = _fullscreen;
			break;
		case WM_SIZE:
			if (_fullscreen && p_wparam == SIZE_RESTORED) {
				_fullscreen_bounds_dirty = true;
			}
			[[fallthrough]];
		case WM_MOVE:
			_update_mouse_mode();
			break;
		case WM_SETCURSOR:
			if (LOWORD(p_lparam) == HTCLIENT) {
				_apply_mouse_cursor();
				return TRUE;
			}
			break;
		case WM_SYSCOMMAND:
			// Windows can still start a screen saver while display idle is inhibited.
			if (_keep_screen_on && (p_wparam & 0xFFF0) == SC_SCREENSAVE) {
				return 0;
			}
			break;
		case WM_ERASEBKGND:
			return 1; // Fixed Function paints the client area.
		case WM_PAINT: {
			PAINTSTRUCT paint;
			BeginPaint(_window, &paint);
			EndPaint(_window, &paint);
			return 0;
		}
		case WM_INPUTLANGCHANGE:
			_update_keyboard_labels();
			break;
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			if (!_drop_input && p_wparam != VK_PROCESSKEY &&
					(!_ime_composing || p_message == WM_KEYUP || p_message == WM_SYSKEYUP)) {
				_queue_key(p_message, p_wparam, p_lparam);
			}
			if (p_message == WM_KEYDOWN || p_message == WM_KEYUP) {
				return 0;
			}
			break; // Retain default system handling, including Alt+F4.
		case WM_CHAR:
			if (!_drop_input) {
				_queue_character(p_wparam, p_lparam);
			}
			return 0;
		case WM_IME_SETCONTEXT:
			// Godot draws preedit text, and Windows draws the native candidate list.
			return DefWindowProcW(_window, p_message, p_wparam, p_lparam & ~LPARAM(ISC_SHOWUICOMPOSITIONWINDOW));
		case WM_IME_STARTCOMPOSITION:
			if (_ime_active && _mouse_focused && !_drop_input) {
				_ime_composing = true;
				_update_ime_position();
			}
			return 0;
		case WM_IME_COMPOSITION:
			if (_ime_active && _mouse_focused && !_drop_input) {
				_handle_ime_composition(p_lparam);
			}
			return 0;
		case WM_IME_ENDCOMPOSITION:
			_ime_composing = false;
			_set_ime_preedit(String(), 0);
			return 0;
		case WM_IME_CHAR:
			if (_ime_active && _mouse_focused && !_drop_input) {
				_queue_character(p_wparam, 0);
			}
			return 0;
		case WM_MOUSEMOVE:
			if (!_drop_input) {
				_send_mouse_motion(Point2i(GET_X_LPARAM(p_lparam), GET_Y_LPARAM(p_lparam)));
			}
			return 0;
		case WM_MOUSELEAVE:
			_mouse_inside = false;
			_send_window_event(WINDOW_EVENT_MOUSE_EXIT);
			return 0;
		case WM_CANCELMODE:
		case WM_CAPTURECHANGED:
			_release_mouse_buttons();
			break;
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
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_XBUTTONDBLCLK: {
			const WORD native_button = GET_XBUTTON_WPARAM(p_wparam);
			if (!_drop_input && (native_button == XBUTTON1 || native_button == XBUTTON2)) {
				const MouseButton button = native_button == XBUTTON1 ? MouseButton::MB_XBUTTON1 : MouseButton::MB_XBUTTON2;
				const Point2i position(GET_X_LPARAM(p_lparam), GET_Y_LPARAM(p_lparam));
				_send_mouse_button(button, position, p_message != WM_XBUTTONUP, p_message == WM_XBUTTONDBLCLK);
			}
			return TRUE;
		}
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL: {
			const int delta = GET_WHEEL_DELTA_WPARAM(p_wparam);
			if (_drop_input || delta == 0) {
				return 0;
			}
			POINT point = { GET_X_LPARAM(p_lparam), GET_Y_LPARAM(p_lparam) };
			ScreenToClient(_window, &point);
			const Point2i position(point.x, point.y);
			MouseButton button;
			if (p_message == WM_MOUSEWHEEL) {
				button = delta > 0 ? MouseButton::WHEEL_UP : MouseButton::WHEEL_DOWN;
			} else {
				button = delta > 0 ? MouseButton::WHEEL_RIGHT : MouseButton::WHEEL_LEFT;
			}
			const float factor = Math::abs(delta) / float(WHEEL_DELTA);
			_send_mouse_button(button, position, true, false, factor);
			_send_mouse_button(button, position, false, false, factor);
			return 0;
		}
	}
	return DefWindowProcW(_window, p_message, p_wparam, p_lparam);
}

void DisplayServerWindowsNT::_pump_messages() {
	MSG message;
	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
		if (message.message == WM_QUIT) {
			_send_window_event(WINDOW_EVENT_CLOSE_REQUEST);
			continue;
		}
		if (!_drop_input) {
			TranslateMessage(&message);
		}
		DispatchMessageW(&message);
	}
	// Let Win32 finish activation and drain old mouse messages before recapturing.
	if (_mouse_focus_pending && GetForegroundWindow() == _window) {
		_mouse_focus_pending = false;
		_update_mouse_mode(true);
	}
}

void DisplayServerWindowsNT::process_events() {
	_pump_messages();
	_flush_keys();
	_update_client_rect();
	_joypad.process();
	Input::get_singleton()->flush_buffered_events();
	_process_file_dialog();
}

void DisplayServerWindowsNT::force_process_and_drop_events() {
	_drop_input = true;
	_pump_messages();
	_drop_input = false;
	_update_client_rect();
}
