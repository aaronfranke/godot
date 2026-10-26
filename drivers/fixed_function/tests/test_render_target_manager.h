#ifndef TEST_FIXED_FUNCTION_RENDER_TARGET_MANAGER_H
#define TEST_FIXED_FUNCTION_RENDER_TARGET_MANAGER_H

#include "drivers/fixed_function/render_target_manager.h"

#include <vector>

class TestFixedFunctionRenderTargetManager {
	static Error _draw_color(FixedFunctionGraphicsDevice &p_device, uint32_t p_color, const Rect2i &p_clip);
	static bool _matches(const std::vector<uint32_t> &p_pixels, uint32_t p_expected, int p_x, int p_y);
	static bool _compose(FixedFunctionRenderTargetManager &p_manager, FixedFunctionGraphicsDevice &p_device, RID p_left, RID p_right, std::vector<uint32_t> &r_pixels);
	static bool _test_failed_viewports(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RID p_bad, RID p_good, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_RENDER_TARGET_MANAGER_H
