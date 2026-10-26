#pragma once

#include "drivers/fixed_function/render_context.h"

#include <vector>

class TestFixedFunctionDepth {
	static Error _draw(FixedFunctionGraphicsDevice &p_device, uint32_t p_color, float p_z, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, RID p_texture = RID(), const Projection &p_projection = Projection());
	static bool _finish(FixedFunctionRenderContext &p_context, std::vector<uint32_t> &r_pixels, uint32_t p_left, uint32_t p_right);
	static bool _test_targets(FixedFunctionRenderContext &p_context, std::vector<uint32_t> &r_pixels, bool p_preserves_targets_on_resize);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels, bool p_preserves_targets_on_resize = false);
};
