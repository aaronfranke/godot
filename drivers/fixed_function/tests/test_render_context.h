#ifndef TEST_FIXED_FUNCTION_RENDER_CONTEXT_H
#define TEST_FIXED_FUNCTION_RENDER_CONTEXT_H

#include "drivers/fixed_function/render_context.h"

#include <vector>

class TestFixedFunctionRenderContext {
	static Ref<Image> _make_image();
	static bool _read_pixels(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels);
	static bool _pixel_matches(const std::vector<uint32_t> &p_pixels, int p_width, int p_x, int p_y, uint32_t p_expected);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};

	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_RENDER_CONTEXT_H
