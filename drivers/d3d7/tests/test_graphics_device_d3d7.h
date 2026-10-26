#pragma once

#include "core/templates/list.h"
#include "drivers/fixed_function/graphics_device.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <vector>

class FixedFunctionGraphicsDeviceD3D7;

class TestFixedFunctionGraphicsDeviceD3D7 {
	static const uint32_t CLEAR_COLOR = 0xff182430;
	static const FixedFunctionVertex TRIANGLE[3];

	static bool _render_and_read(FixedFunctionGraphicsDeviceD3D7 &p_device, const Projection &p_transform, std::vector<uint32_t> &r_pixels);
	static bool _is_clear_color(uint32_t p_pixel);
	static bool _check_pixels(const std::vector<uint32_t> &p_pixels, uint32_t p_width, uint32_t p_height);
	static bool _save_frame(const String &p_path, const std::vector<uint32_t> &p_pixels, uint32_t p_width, uint32_t p_height);
	static DWORD WINAPI _wrong_thread(void *p_device);
	static bool _test_depth16(HWND p_window, bool p_force_rgb);
	static bool _test_vertex_stream(HWND p_window, bool p_force_rgb);
	static bool _test_streamed_lighting(HWND p_window, bool p_force_rgb);
	static bool _test_geometry(HWND p_window, bool p_force_rgb);
	static bool _run_probe(HWND p_window, const String &p_capture_path, bool p_force_rgb);
	static LRESULT CALLBACK _window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam);

public:
	static bool test(const List<String> &p_args);
};
