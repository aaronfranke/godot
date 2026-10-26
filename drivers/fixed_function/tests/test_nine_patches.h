#pragma once

#include "drivers/fixed_function/canvas_item_renderer.h"
#include "servers/rendering/renderer_canvas_cull.h"

#include <vector>

class TestFixedFunctionNinePatches {
	struct FFPatchFixture {
		RendererCanvasCull &server;
		RID item;
		FFPatchFixture();
		~FFPatchFixture();
	};
	struct FFPatchCase {
		Rect2 rect;
		Rect2 source;
		RSE::NinePatchAxisMode horizontal;
		RSE::NinePatchAxisMode vertical;
		bool center;
	};
	static uint32_t _texel(int p_x, int p_y);
	static Ref<Image> _make_image();
	static Color _sample(real_t p_u, real_t p_v, bool p_filtered);
	static real_t _reference_axis(real_t p_pixel, real_t p_destination, real_t p_source, real_t p_scale, RSE::NinePatchAxisMode p_mode);
	static bool _test_mapping(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer, RID p_texture, bool p_mirrored, bool p_filtered, std::vector<uint32_t> &r_pixels);
	static bool _test_limits(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer, RID p_texture, std::vector<uint32_t> &r_pixels);

public:
	static bool test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer, std::vector<uint32_t> &r_pixels);
};
