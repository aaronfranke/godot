#include "display_server_windows_nt4.h"

DisplayServerWindowsNT *DisplayServerWindowsNT4::_create_instance() {
	return memnew(DisplayServerWindowsNT4);
}

DisplayServer *DisplayServerWindowsNT4::_create(const String &p_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_size, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	DisplayServer *server = _create_display_server(p_driver, p_mode, p_vsync, p_flags, p_position, p_size, p_screen, p_context, p_parent_window, r_error, _create_instance);
	if (!server && r_error == ERR_UNAVAILABLE && p_driver == "opengl1") {
		// Failed GUI startup needs a visible explanation without a DisplayServer.
		MessageBoxW(nullptr, L"OpenGL1 could not initialize. A compatible OpenGL 1.1 window pixel format is required.\n\nTry D3D3 Software or another graphics driver. See the log for details.", L"OpenGL1 unavailable", MB_OK | MB_ICONEXCLAMATION);
	}
	return server;
}

void DisplayServerWindowsNT4::register_driver() {
	register_create_function("windows", _create, _get_rendering_drivers);
}
