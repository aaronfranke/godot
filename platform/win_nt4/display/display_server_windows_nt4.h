#pragma once

#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

class DisplayServerWindowsNT4 : public DisplayServerWindowsNT {
	GDSOFTCLASS(DisplayServerWindowsNT4, DisplayServerWindowsNT);

	static DisplayServerWindowsNT *_create_instance();
	static DisplayServer *_create(const String &p_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_size, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error);

public:
	static void register_driver();
};
