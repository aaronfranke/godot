#pragma once

#include "drivers/fixed_function/canvas_item_renderer.h"
#include "servers/rendering/renderer_canvas_cull.h"

#include <vector>

class TestFixedFunctionCanvasItemRenderer {
	struct FFCanvasFixture {
		RendererCanvasCull &server;
		FixedFunctionCanvasItemRenderer &renderer;
		List<RID> items;

		FFCanvasFixture(FixedFunctionCanvasItemRenderer &p_renderer);
		bool add_polygon(RID p_item, const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, RSE::PrimitiveType p_primitive);
		RID create_item();
		~FFCanvasFixture();
	};
	static Ref<Image> _make_image();
	static Ref<Image> _make_glyph();
	static Ref<Image> _make_nine_patch();
	static bool _check_pixel(const std::vector<uint32_t> &p_pixels, int p_x, int p_y, uint32_t p_expected);
	static bool _render(FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_canvas, FixedFunctionGraphicsDevice &p_device, RendererCanvasRender::Item *p_items, std::vector<uint32_t> &r_pixels);

	static bool _test_lines_and_viewport_sampling(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionRenderContext &p_context, std::vector<uint32_t> &r_pixels);
	static bool _test_canvas_meshes(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionRenderContext &p_context);

	static bool _test_redraw_requests(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionTextureStorage &p_storage, RID p_texture);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};
