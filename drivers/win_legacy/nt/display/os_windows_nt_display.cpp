#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Legacy Windows includes.
#include "drivers/win_legacy/nt/os/os_windows_nt.h"

void OS_WindowsNT::set_clipboard(const String &p_text) {
	if (_display) {
		_display->clipboard_set(p_text);
	} else {
		OS::set_clipboard(p_text);
	}
}

String OS_WindowsNT::get_clipboard() const {
	return _display ? _display->clipboard_get() : OS::get_clipboard();
}

Error OS_WindowsNT::dialog_show(String p_title, String p_description, Vector<String> p_buttons, Object *p_object, String p_callback) {
	return _display ? _display->dialog_show(p_title, p_description, p_buttons, p_object, p_callback) : OS::dialog_show(p_title, p_description, p_buttons, p_object, p_callback);
}

Error OS_WindowsNT::dialog_input_text(String p_title, String p_description, String p_partial, Object *p_object, String p_callback) {
	return _display ? _display->dialog_input_text(p_title, p_description, p_partial, p_object, p_callback) : OS::dialog_input_text(p_title, p_description, p_partial, p_object, p_callback);
}

void OS_WindowsNT::set_ime_active(bool p_active) {
	if (_display) {
		_display->ime_set_active(p_active);
	}
}

void OS_WindowsNT::set_ime_position(const Point2 &p_position) {
	if (_display) {
		_display->ime_set_position(p_position);
	}
}

String OS_WindowsNT::get_ime_text() const {
	return _display ? _display->ime_get_text() : String();
}

Point2 OS_WindowsNT::get_ime_selection() const {
	return _display ? Point2(_display->ime_get_selection()) : Point2();
}

OS::LatinKeyboardVariant OS_WindowsNT::get_latin_keyboard_variant() const {
	return _display ? _display->get_latin_keyboard_variant() : OS::get_latin_keyboard_variant();
}

int OS_WindowsNT::keyboard_get_layout_count() const {
	return _display ? _display->keyboard_get_layout_count() : OS::keyboard_get_layout_count();
}

int OS_WindowsNT::keyboard_get_current_layout() const {
	return _display ? _display->keyboard_get_current_layout() : OS::keyboard_get_current_layout();
}

void OS_WindowsNT::keyboard_set_current_layout(int p_index) {
	if (_display) {
		_display->keyboard_set_current_layout(p_index);
	}
}

String OS_WindowsNT::keyboard_get_layout_language(int p_index) const {
	return _display ? _display->keyboard_get_layout_language(p_index) : OS::keyboard_get_layout_language(p_index);
}

String OS_WindowsNT::keyboard_get_layout_name(int p_index) const {
	return _display ? _display->keyboard_get_layout_name(p_index) : OS::keyboard_get_layout_name(p_index);
}

uint32_t OS_WindowsNT::keyboard_get_scancode_from_physical(uint32_t p_scancode) const {
	return _display ? _display->keyboard_get_scancode_from_physical(p_scancode) : p_scancode;
}

void OS_WindowsNT::set_icon(const Ref<Image> &p_icon) {
	if (_display) {
		_display->set_icon(p_icon);
	}
}

void OS_WindowsNT::set_native_icon(const String &p_filename) {
	if (_display) {
		_display->set_native_icon(p_filename);
	}
}

Size2 OS_WindowsNT::get_real_window_size() const {
	return _display ? Size2(_display->get_frame_size()) : get_window_size();
}

void OS_WindowsNT::set_window_size(const Size2 p_size) {
	if (_display) {
		_display->set_size(p_size);
	}
}

Size2 OS_WindowsNT::get_min_window_size() const {
	return _display ? Size2(_display->get_min_size()) : Size2();
}

void OS_WindowsNT::set_min_window_size(const Size2 p_size) {
	if (_display) {
		_display->set_min_size(p_size);
	}
}

Size2 OS_WindowsNT::get_max_window_size() const {
	return _display ? Size2(_display->get_max_size()) : Size2();
}

void OS_WindowsNT::set_max_window_size(const Size2 p_size) {
	if (_display) {
		_display->set_max_size(p_size);
	}
}

Point2 OS_WindowsNT::get_window_position() const {
	return _display ? _display->get_position() : Point2();
}

void OS_WindowsNT::set_window_position(const Point2 &p_position) {
	if (_display) {
		_display->set_position(p_position);
	}
}

void OS_WindowsNT::set_window_mouse_passthrough(const PoolVector2Array &p_region) {
	if (_display) {
		_display->set_mouse_passthrough(p_region);
	}
}

void OS_WindowsNT::set_window_minimized(bool p_enabled) {
	if (_display) {
		_display->set_minimized(p_enabled);
	}
}

bool OS_WindowsNT::is_window_minimized() const {
	return _display && _display->is_minimized();
}

void OS_WindowsNT::set_window_maximized(bool p_enabled) {
	if (_display) {
		_display->set_maximized(p_enabled);
	}
}

bool OS_WindowsNT::is_window_maximized() const {
	return _display && _display->is_maximized();
}

bool OS_WindowsNT::is_window_focused() const {
	return _display && _display->is_focused();
}

void OS_WindowsNT::request_attention() {
	if (_display) {
		_display->request_attention();
	}
}

void OS_WindowsNT::move_window_to_foreground() {
	if (_display) {
		_display->move_to_foreground();
	}
}

void OS_WindowsNT::set_borderless_window(bool p_enabled) {
	if (_display && _display->set_windowed_frame(p_enabled, _video_mode.resizable)) {
		_video_mode.borderless_window = p_enabled;
	}
}

bool OS_WindowsNT::get_borderless_window() {
	return _display && _video_mode.borderless_window;
}

void OS_WindowsNT::set_window_resizable(bool p_enabled) {
	if (_display && _display->set_windowed_frame(_video_mode.borderless_window, p_enabled)) {
		_video_mode.resizable = p_enabled;
	}
}

void OS_WindowsNT::set_window_fullscreen(bool p_enabled) {
	if (_display) {
		_display->set_fullscreen(p_enabled);
	}
}

bool OS_WindowsNT::is_window_fullscreen() const {
	return _display && _display->is_fullscreen();
}

bool OS_WindowsNT::is_window_use_nonexclusive_fullscreen() const {
	return _display != nullptr;
}

void OS_WindowsNT::set_window_always_on_top(bool p_enabled) {
	if (_display) {
		_display->set_always_on_top(p_enabled);
	}
}

bool OS_WindowsNT::is_window_always_on_top() const {
	return _display && _display->is_always_on_top();
}

int OS_WindowsNT::get_screen_count() const {
	return _display ? _display->get_screen_count() : OS::get_screen_count();
}

int OS_WindowsNT::get_current_screen() const {
	return _display ? _display->get_current_screen() : OS::get_current_screen();
}

void OS_WindowsNT::set_current_screen(int p_screen) {
	if (_display) {
		_display->set_current_screen(p_screen);
	}
}

Point2 OS_WindowsNT::get_screen_position(int p_screen) const {
	return _display ? Point2(_display->get_screen_position(p_screen)) : OS::get_screen_position(p_screen);
}

Size2 OS_WindowsNT::get_screen_size(int p_screen) const {
	return _display ? Size2(_display->get_screen_size(p_screen)) : OS::get_screen_size(p_screen);
}

int OS_WindowsNT::get_screen_dpi(int p_screen) const {
	return _display ? _display->get_screen_dpi() : OS::get_screen_dpi(p_screen);
}

float OS_WindowsNT::get_screen_refresh_rate(int p_screen) const {
	return _display ? _display->get_screen_refresh_rate(p_screen, SCREEN_REFRESH_RATE_FALLBACK) : SCREEN_REFRESH_RATE_FALLBACK;
}

void OS_WindowsNT::set_keep_screen_on(bool p_enabled) {
	if (!_display || _display->set_keep_screen_on(p_enabled)) {
		OS::set_keep_screen_on(p_enabled);
	}
}

void OS_WindowsNT::set_mouse_mode(MouseMode p_mode) {
	if (_display) {
		_display->set_mouse_mode(p_mode);
	}
}

OS::MouseMode OS_WindowsNT::get_mouse_mode() const {
	return _display ? _display->get_mouse_mode() : OS::get_mouse_mode();
}

void OS_WindowsNT::set_cursor_shape(CursorShape p_shape) {
	if (_display) {
		_display->cursor_set_shape(p_shape);
	}
}

OS::CursorShape OS_WindowsNT::get_cursor_shape() const {
	return _display ? _display->cursor_get_shape() : OS::get_cursor_shape();
}

void OS_WindowsNT::set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	if (_display) {
		_display->cursor_set_custom_image(p_cursor, p_shape, p_hotspot);
	}
}

void OS_WindowsNT::warp_mouse_position(const Point2 &p_position) {
	if (_display) {
		_display->warp_mouse_position(p_position);
	}
}

void *OS_WindowsNT::get_native_handle(int p_handle_type) {
	if (p_handle_type == WINDOW_HANDLE) {
		return _display ? _display->get_window() : nullptr;
	}
	return p_handle_type == APPLICATION_HANDLE ? GetModuleHandleW(nullptr) : nullptr;
}

void OS_WindowsNT::force_process_input() {
	if (_display) {
		_display->process_events();
	}
}

void OS_WindowsNT::_set_use_vsync(bool p_enable) {
	if (_display) {
		_display->set_use_vsync(p_enable);
	}
}

void OS_WindowsNT::process_and_drop_events() {
	if (_display) {
		_display->process_and_drop_events();
	}
}

void OS_WindowsNT::make_rendering_thread() {
	if (_display) {
		_display->make_rendering_thread();
	}
}

void OS_WindowsNT::release_rendering_thread() {
	if (_display) {
		_display->release_rendering_thread();
	}
}

void OS_WindowsNT::swap_buffers() {
	if (_display) {
		_display->swap_buffers();
	}
}
