#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/os/main_loop.h"
#include "main/input_default.h"
#include "platform/windows/key_mapping_windows.h"

#include <limits.h>
#include <shellapi.h>

void DisplayServerWindowsNT::_handle_file_drop(WPARAM p_drop) {
	const HDROP drop = reinterpret_cast<HDROP>(p_drop);
	if (!drop) {
		return;
	}

	Vector<String> paths;
	if (_main_loop) {
		const UINT count = DragQueryFileW(drop, 0xffffffff, nullptr, 0);
		Vector<WCHAR> path;
		for (UINT i = 0; i < count; ++i) {
			const UINT length = DragQueryFileW(drop, i, nullptr, 0);
			if (length == 0 || length >= INT_MAX || path.resize(int(length) + 1) != OK) {
				continue;
			}
			if (DragQueryFileW(drop, i, path.ptrw(), length + 1) == length) {
				paths.push_back(String(path.ptr(), int(length)));
			}
		}
	}
	DragFinish(drop);
	if (!paths.empty() && _main_loop) {
		_main_loop->drop_files(paths, 0);
	}
}

void DisplayServerWindowsNT::_queue_key(UINT p_message, WPARAM p_key, LPARAM p_flags) {
	const bool extended = (p_flags & (1 << 24)) != 0;
	const unsigned int scan = (p_flags >> 16) & 0xff;
	unsigned int logical = KeyMappingWindows::get_keysym(p_key);
	if (p_key == VK_RETURN && extended) {
		logical = KEY_KP_ENTER;
	}

	Ref<InputEventKey> event;
	event.instance();
	event->set_scancode(logical);
	event->set_physical_scancode(KeyMappingWindows::get_scansym(scan, extended));
	const bool pressed = p_message == WM_KEYDOWN || p_message == WM_SYSKEYDOWN;
	event->set_pressed(pressed);
	event->set_echo(pressed && (p_flags & (1 << 30)) != 0);
	event->set_shift(logical != KEY_SHIFT && GetKeyState(VK_SHIFT) < 0);
	event->set_control(logical != KEY_CONTROL && GetKeyState(VK_CONTROL) < 0);
	event->set_alt(logical != KEY_ALT && GetKeyState(VK_MENU) < 0);
	event->set_metakey(logical != KEY_META && (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0));
	const uint16_t scan_id = uint16_t((p_flags >> 16) & 0x1ff);
	const uint16_t key_id = scan_id ? scan_id : uint16_t(0x200 | (p_key & 0xff));
	_keys.push_back({ event, key_id });
}

void DisplayServerWindowsNT::_queue_character(uint16_t p_character, LPARAM p_flags) {
	uint32_t character = p_character;
	if (p_character >= 0xd800 && p_character <= 0xdbff) {
		_high_surrogate = p_character;
		return;
	}
	if (p_character >= 0xdc00 && p_character <= 0xdfff) {
		if (!_high_surrogate) {
			return;
		}
		character = 0x10000 + ((uint32_t(_high_surrogate) - 0xd800) << 10) + (p_character - 0xdc00);
	}
	_high_surrogate = 0;
	if (character < 32 || character == 127) {
		return;
	}

	// TranslateMessage queues WM_CHAR after pending key messages. Pair it with the oldest matching press.
	const uint16_t scan = (p_flags >> 16) & 0x1ff;
	if (scan) {
		for (int i = 0; i < _keys.size(); ++i) {
			KeyEvent &key = _keys.write[i];
			if (key.scan == scan && key.event->is_pressed() && !key.event->get_unicode()) {
				key.event->set_unicode(character);
				return;
			}
		}
	}
	Ref<InputEventKey> event;
	event.instance();
	event->set_pressed(true);
	event->set_unicode(character);
	_keys.push_back({ event, 0 });
}

void DisplayServerWindowsNT::_flush_keys() {
	const Vector<KeyEvent> keys = _keys;
	_keys.clear();
	for (int i = 0; i < keys.size(); ++i) {
		const KeyEvent &key = keys[i];
		const KeyScancodes scancodes = { key.event->get_scancode(), key.event->get_physical_scancode() };
		if (!key.event->is_pressed()) {
			_pressed_keys.erase(key.scan);
		} else if (!key.event->is_echo() && (scancodes.logical || scancodes.physical)) {
			_pressed_keys[key.scan] = scancodes;
		}
		_input->parse_input_event(key.event);
	}
}

void DisplayServerWindowsNT::_release_keys() {
	// Release only the Win32 window's held keys. Joypads can keep reporting in the background.
	_flush_keys();
	while (_pressed_keys.front()) {
		const Map<uint16_t, KeyScancodes>::Element *key = _pressed_keys.front();
		const KeyScancodes scancodes = key->get();
		_pressed_keys.erase(key->key());

		Ref<InputEventKey> event;
		event.instance();
		event->set_scancode(scancodes.logical);
		event->set_physical_scancode(scancodes.physical);
		event->set_pressed(false);
		_input->parse_input_event(event);
	}
}

void DisplayServerWindowsNT::set_mouse_mode(OS::MouseMode p_mode) {
	ERR_FAIL_INDEX(p_mode, OS::MOUSE_MODE_CONFINED_HIDDEN + 1);
	if (_mouse_mode != p_mode) {
		_mouse_mode = p_mode;
		_mouse_position_valid = false;
		_update_mouse_mode(true);
	}
}

OS::MouseMode DisplayServerWindowsNT::get_mouse_mode() const {
	return _mouse_mode;
}

void DisplayServerWindowsNT::warp_mouse_position(const Point2 &p_position) {
	if (!_mouse_focused || _mouse_mode == OS::MOUSE_MODE_CAPTURED) {
		return;
	}
	POINT position = { LONG(p_position.x), LONG(p_position.y) };
	if (ClientToScreen(_window, &position)) {
		SetCursorPos(position.x, position.y);
	}
}

bool DisplayServerWindowsNT::_mouse_hides_cursor() const {
	if (!_mouse_focused || _mouse_focus_pending || is_minimized()) {
		return false;
	}
	return _mouse_mode == OS::MOUSE_MODE_HIDDEN ||
			(!_mouse_clip_rect.has_no_area() && (_mouse_mode == OS::MOUSE_MODE_CAPTURED || _mouse_mode == OS::MOUSE_MODE_CONFINED_HIDDEN));
}

void DisplayServerWindowsNT::_release_mouse_clip() {
	if (!_mouse_clip_rect.has_no_area()) {
		ClipCursor(nullptr);
		_mouse_clip_rect = Rect2i();
	}
}

bool DisplayServerWindowsNT::_recenter_mouse() {
	const Point2i center = _mouse_clip_rect.get_center();
	return SetCursorPos(center.x, center.y);
}

void DisplayServerWindowsNT::_update_mouse_mode(bool p_recenter) {
	const bool confined = _mouse_mode == OS::MOUSE_MODE_CAPTURED || _mouse_mode == OS::MOUSE_MODE_CONFINED || _mouse_mode == OS::MOUSE_MODE_CONFINED_HIDDEN;
	Rect2i clip;
	if (confined && _mouse_focused && !_mouse_focus_pending && !is_minimized()) {
		clip = _get_client_rect();
	}
	const bool clip_changed = clip != _mouse_clip_rect;
	if (clip.has_no_area()) {
		_release_mouse_clip();
	} else if (clip_changed) {
		const Point2i end = clip.position + clip.size;
		const RECT native = { clip.position.x, clip.position.y, end.x, end.y };
		if (ClipCursor(&native)) {
			_mouse_clip_rect = clip;
		} else {
			_release_mouse_clip();
		}
	}

	if (_mouse_mode == OS::MOUSE_MODE_CAPTURED && !_mouse_clip_rect.has_no_area() && (p_recenter || clip_changed)) {
		if (_recenter_mouse()) {
			_mouse_position = Point2(_mouse_clip_rect.size / 2);
			_input->set_mouse_position(_mouse_position);
		} else {
			_release_mouse_clip();
		}
	}
	_update_mouse_cursor();
}

void DisplayServerWindowsNT::_send_mouse_motion(const Point2 &p_position) {
	Point2 position = p_position;
	Point2 relative;
	if (_mouse_mode == OS::MOUSE_MODE_CAPTURED) {
		POINT cursor;
		if (_mouse_clip_rect.has_no_area() || !GetCursorPos(&cursor)) {
			return;
		}
		// Read the actual cursor so a queued WM_MOUSEMOVE from before recentering cannot add motion.
		position = Point2(_mouse_clip_rect.size / 2);
		relative = Point2(Point2i(cursor.x, cursor.y) - _mouse_clip_rect.get_center());
		if (relative == Point2()) {
			return;
		}
		if (!_recenter_mouse()) {
			_release_mouse_clip();
			_update_mouse_cursor();
			return;
		}
	} else if (!_mouse_inside) {
		TRACKMOUSEEVENT tracking = {};
		tracking.cbSize = sizeof(tracking);
		tracking.dwFlags = TME_LEAVE;
		tracking.hwndTrack = _window;
		TrackMouseEvent(&tracking);
		_mouse_inside = true;
		_notify(MainLoop::NOTIFICATION_WM_MOUSE_ENTER);
	} else if (_mouse_position_valid) {
		relative = position - _mouse_position;
	}
	Ref<InputEventMouseMotion> event;
	event.instance();
	event->set_relative(relative);
	event->set_speed(_input->get_last_mouse_speed());
	event->set_position(position);
	event->set_global_position(position);
	event->set_button_mask(_mouse_buttons);
	event->set_shift(GetKeyState(VK_SHIFT) < 0);
	event->set_control(GetKeyState(VK_CONTROL) < 0);
	event->set_alt(GetKeyState(VK_MENU) < 0);
	event->set_metakey(GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0);
	_mouse_position = position;
	_mouse_position_valid = true;
	_input->parse_input_event(event);
}

void DisplayServerWindowsNT::_release_mouse_buttons() {
	const int buttons[] = { BUTTON_LEFT, BUTTON_RIGHT, BUTTON_MIDDLE, BUTTON_XBUTTON1, BUTTON_XBUTTON2 };
	for (int button : buttons) {
		if (_mouse_buttons & (1 << (button - 1))) {
			_send_mouse_button(button, _mouse_position, false);
		}
	}
}

void DisplayServerWindowsNT::_send_mouse_button(int p_button, const Point2 &p_position, bool p_pressed, bool p_double_click, float p_factor) {
	if (p_button <= BUTTON_MIDDLE || p_button == BUTTON_XBUTTON1 || p_button == BUTTON_XBUTTON2) {
		const int mask = 1 << (p_button - 1);
		if (p_pressed) {
			_mouse_buttons |= mask;
		} else {
			_mouse_buttons &= ~mask;
		}
	}
	const Point2 position = _mouse_mode == OS::MOUSE_MODE_CAPTURED && !_mouse_clip_rect.has_no_area() ? Point2(_mouse_clip_rect.size / 2) : p_position;
	Ref<InputEventMouseButton> event;
	event.instance();
	event->set_button_index(p_button);
	event->set_button_mask(_mouse_buttons);
	event->set_position(position);
	event->set_global_position(position);
	event->set_pressed(p_pressed);
	event->set_doubleclick(p_double_click);
	event->set_factor(p_factor);
	event->set_shift(GetKeyState(VK_SHIFT) < 0);
	event->set_control(GetKeyState(VK_CONTROL) < 0);
	event->set_alt(GetKeyState(VK_MENU) < 0);
	event->set_metakey(GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0);
	_mouse_position = position;
	_input->parse_input_event(event);
}
