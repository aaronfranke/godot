#include "display_server_windows_nt6.h"

DisplayServerWindowsNT *DisplayServerWindowsNT6::_create_instance() {
	return memnew(DisplayServerWindowsNT6);
}

DisplayServer *DisplayServerWindowsNT6::_create(const String &p_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_size, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	return _create_display_server(p_driver, p_mode, p_vsync, p_flags, p_position, p_size, p_screen, p_context, p_parent_window, r_error, _create_instance);
}

void DisplayServerWindowsNT6::register_driver() {
	register_create_function("windows", _create, _get_rendering_drivers);
}
