#pragma once

#include "drivers/fixed_function/render_context.h"

#include <vector>

class TestFixedFunctionSampling {
	static bool _supported(const FixedFunctionGraphicsDevice::FFCapabilities &p_caps, uint8_t p_flags);
	static uint32_t _texel(int p_x, int p_y, uint8_t p_flags);
	static uint32_t _sample(real_t p_u, real_t p_v, uint8_t p_flags);
	static bool _color_matches(uint32_t p_actual, uint32_t p_expected);
	static Error _draw_quad(FixedFunctionGraphicsDevice &p_device, RID p_texture, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_minify = false);
	static bool _render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, RID p_texture, std::vector<uint32_t> &r_pixels, bool p_mirror = false);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};
