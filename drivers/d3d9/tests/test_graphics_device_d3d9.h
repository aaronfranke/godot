#ifndef TEST_FIXED_FUNCTION_GRAPHICS_DEVICE_D3D9_H
#define TEST_FIXED_FUNCTION_GRAPHICS_DEVICE_D3D9_H

#include "core/list.h"
#include "core/os/main_loop.h"
#include "drivers/d3d9/graphics_device_d3d9.h"

#include <vector>

class TestFixedFunctionGraphicsDeviceD3D9 {
	static const uint32_t CLEAR_COLOR = 0xff182430;
	static const FixedFunctionVertex TRIANGLE[3];

	static bool _render_and_read(FixedFunctionGraphicsDeviceD3D9 &p_device, const CameraMatrix &p_transform, std::vector<uint32_t> &r_pixels);
	static bool _check_pixels(const std::vector<uint32_t> &p_pixels, uint32_t p_width, uint32_t p_height);
	static bool _save_frame(const String &p_path, const std::vector<uint32_t> &p_pixels, uint32_t p_width, uint32_t p_height);
	static DWORD WINAPI _wrong_thread(void *p_device);
	static bool _test_depth16(HWND p_window);
	static bool _test_vertex_stream(HWND p_window);
	static bool _test_geometry(HWND p_window);
	static bool _test_lit_geometry(HWND p_window);
	static bool _test_normal_geometry(HWND p_window);
	static bool _test_directional_shadows(HWND p_window);
	static bool _run_probe(HWND p_window, const String &p_capture_path);
	static LRESULT CALLBACK _window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam);

public:
	static MainLoop *test(const List<String> &p_args);
};

#endif // TEST_FIXED_FUNCTION_GRAPHICS_DEVICE_D3D9_H
