#pragma once

#include "drivers/fixed_function/render_context.h"

#include <vector>

class TestFixedFunctionRenderContext {
	static Ref<Image> _make_image();
	static bool _read_pixels(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels);
	static bool _pixel_matches(const std::vector<uint32_t> &p_pixels, int p_width, int p_x, int p_y, uint32_t p_expected);
	static bool _test_stretch_modes(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, const Ref<Image> &p_image, const Color &p_background, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};
