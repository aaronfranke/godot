#ifndef TEST_FIXED_FUNCTION_RENDER_TARGETS_H
#define TEST_FIXED_FUNCTION_RENDER_TARGETS_H

#include "drivers/fixed_function/render_context.h"

#include <vector>

class TestFixedFunctionRenderTargets {
	static Error _draw_quad(FixedFunctionGraphicsDevice &p_device, uint32_t p_color, RID p_texture = RID(), const FixedFunctionGraphicsDevice::FFDrawParameters &p_state = FixedFunctionGraphicsDevice::FFDrawParameters());
	static bool _draw_pattern(FixedFunctionGraphicsDevice &p_device);
	static bool _test_readback(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, RID p_target);
	static bool _readback_pattern(Ref<Image> p_image, const Size2i &p_size, bool p_flipped);
	static bool _test_defaults(FixedFunctionTextureStorage &p_storage, RID p_target, bool p_transparent);
	static bool _test_transparency(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, bool p_preserves_targets_on_resize = false);
	static bool _render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, RID p_source, RID p_copy, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
	static bool test_transparency(FixedFunctionGraphicsDevice &p_device, void *p_window, bool p_preserves_targets_on_resize);
};

#endif // TEST_FIXED_FUNCTION_RENDER_TARGETS_H
