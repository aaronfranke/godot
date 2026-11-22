#include "display_server_windows_9x.h"
#include "platform/win_9x/os/os_windows_9x.h"

void OS_Windows9x::set_clipboard(const String &p_text) {
	if (_display) {
		_display->clipboard_set(p_text);
	} else {
		OS::set_clipboard(p_text);
	}
}

String OS_Windows9x::get_clipboard() const {
	return _display ? _display->clipboard_get() : OS::get_clipboard();
}

Size2 OS_Windows9x::get_real_window_size() const {
	return _display ? Size2(_display->window_get_size_with_decorations(0)) : get_window_size();
}

void OS_Windows9x::set_window_size(const Size2 p_size) {
	if (_display) {
		_display->window_set_size(p_size, 0);
	}
}

Size2 OS_Windows9x::get_min_window_size() const {
	return _display ? Size2(_display->window_get_min_size(0)) : Size2();
}

void OS_Windows9x::set_min_window_size(const Size2 p_size) {
	if (_display) {
		_display->window_set_min_size(p_size, 0);
	}
}

Size2 OS_Windows9x::get_max_window_size() const {
	return _display ? Size2(_display->window_get_max_size(0)) : Size2();
}

void OS_Windows9x::set_max_window_size(const Size2 p_size) {
	if (_display) {
		_display->window_set_max_size(p_size, 0);
	}
}

Point2 OS_Windows9x::get_window_position() const {
	return _display ? Point2(_display->window_get_position(0)) : Point2();
}

void OS_Windows9x::set_window_position(const Point2 &p_position) {
	if (_display) {
		_display->window_set_position(p_position, 0);
	}
}

void OS_Windows9x::set_window_minimized(bool p_enabled) {
	if (_display) {
		_display->window_set_mode(p_enabled ? DisplayServerWindows9x::WINDOW_MODE_MINIMIZED : DisplayServerWindows9x::WINDOW_MODE_WINDOWED, 0);
	}
}

bool OS_Windows9x::is_window_minimized() const {
	return _display && _display->window_get_mode(0) == DisplayServerWindows9x::WINDOW_MODE_MINIMIZED;
}

void OS_Windows9x::set_window_maximized(bool p_enabled) {
	if (_display) {
		_display->window_set_mode(p_enabled ? DisplayServerWindows9x::WINDOW_MODE_MAXIMIZED : DisplayServerWindows9x::WINDOW_MODE_WINDOWED, 0);
	}
}

bool OS_Windows9x::is_window_maximized() const {
	return _display && _display->window_get_mode(0) == DisplayServerWindows9x::WINDOW_MODE_MAXIMIZED;
}

bool OS_Windows9x::is_window_focused() const {
	return _display && _display->window_is_focused(0);
}

void OS_Windows9x::request_attention() {
	if (_display) {
		_display->window_request_attention(0);
	}
}

void OS_Windows9x::move_window_to_foreground() {
	if (_display) {
		_display->window_move_to_foreground(0);
	}
}

void OS_Windows9x::set_borderless_window(bool p_enabled) {
	if (_display) {
		_display->window_set_flag(DisplayServerWindows9x::WINDOW_FLAG_BORDERLESS, p_enabled, 0);
	}
	_video_mode.borderless_window = p_enabled;
}

bool OS_Windows9x::get_borderless_window() {
	return _display && _display->window_get_flag(DisplayServerWindows9x::WINDOW_FLAG_BORDERLESS, 0);
}

void OS_Windows9x::set_window_resizable(bool p_enabled) {
	if (_display) {
		_display->window_set_flag(DisplayServerWindows9x::WINDOW_FLAG_RESIZE_DISABLED, !p_enabled, 0);
	}
	_video_mode.resizable = p_enabled;
}

bool OS_Windows9x::is_window_resizable() const {
	return _display && !_display->window_get_flag(DisplayServerWindows9x::WINDOW_FLAG_RESIZE_DISABLED, 0);
}

void OS_Windows9x::set_mouse_mode(MouseMode p_mode) {
	if (_display) {
		_display->mouse_set_mode(p_mode);
	}
}

OS::MouseMode OS_Windows9x::get_mouse_mode() const {
	return _display ? _display->mouse_get_mode() : OS::get_mouse_mode();
}

void OS_Windows9x::set_cursor_shape(CursorShape p_shape) {
	if (_display) {
		_display->cursor_set_shape(p_shape);
	}
}

OS::CursorShape OS_Windows9x::get_cursor_shape() const {
	return _display ? _display->cursor_get_shape() : OS::get_cursor_shape();
}

void OS_Windows9x::set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	if (_display) {
		_display->cursor_set_custom_image(p_cursor, p_shape, p_hotspot);
	}
}

void OS_Windows9x::warp_mouse_position(const Point2 &p_position) {
	if (_display) {
		_display->warp_mouse(p_position);
	}
}

void OS_Windows9x::set_window_fullscreen(bool p_enabled) {
	if (_display) {
		_display->window_set_mode(p_enabled ? DisplayServerWindows9x::WINDOW_MODE_FULLSCREEN : DisplayServerWindows9x::WINDOW_MODE_WINDOWED, 0);
	}
}

bool OS_Windows9x::is_window_fullscreen() const {
	return _display && _display->window_get_mode(0) == DisplayServerWindows9x::WINDOW_MODE_FULLSCREEN;
}

bool OS_Windows9x::is_window_use_nonexclusive_fullscreen() const {
	return _display != nullptr;
}

void OS_Windows9x::set_window_always_on_top(bool p_enabled) {
	if (_display) {
		_display->window_set_flag(DisplayServerWindows9x::WINDOW_FLAG_ALWAYS_ON_TOP, p_enabled, 0);
	}
}

bool OS_Windows9x::is_window_always_on_top() const {
	return _display && _display->window_get_flag(DisplayServerWindows9x::WINDOW_FLAG_ALWAYS_ON_TOP, 0);
}

int OS_Windows9x::get_screen_count() const { return _display ? _display->get_screen_count() : 1; }
int OS_Windows9x::get_current_screen() const { return 0; }
void OS_Windows9x::set_current_screen(int p_screen) { ERR_FAIL_COND(p_screen > 0); }
Point2 OS_Windows9x::get_screen_position(int) const { return Point2(); }
Size2 OS_Windows9x::get_screen_size(int p_screen) const { return _display ? Size2(_display->screen_get_size(p_screen)) : OS::get_screen_size(p_screen); }
int OS_Windows9x::get_screen_dpi(int p_screen) const { return _display ? _display->screen_get_dpi(p_screen) : OS::get_screen_dpi(p_screen); }
float OS_Windows9x::get_screen_refresh_rate(int p_screen) const { return _display ? _display->screen_get_refresh_rate(p_screen) : SCREEN_REFRESH_RATE_FALLBACK; }

void *OS_Windows9x::get_native_handle(int p_handle_type) {
	if (p_handle_type == WINDOW_HANDLE) {
		return _display ? _display->get_window() : nullptr;
	}
	return p_handle_type == APPLICATION_HANDLE ? GetModuleHandleA(nullptr) : nullptr;
}

void OS_Windows9x::force_process_input() {
	if (_display) {
		_display->process_events();
	}
}
void OS_Windows9x::process_and_drop_events() {
	if (_display) {
		_display->force_process_and_drop_events();
	}
}
void OS_Windows9x::_set_use_vsync(bool p_enable) {
	if (_display) {
		_display->window_set_vsync_mode(p_enable ? DisplayServerWindows9x::VSYNC_ENABLED : DisplayServerWindows9x::VSYNC_DISABLED, 0);
	}
}
