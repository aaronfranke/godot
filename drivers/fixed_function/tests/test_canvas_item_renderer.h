#ifndef TEST_FIXED_FUNCTION_CANVAS_ITEM_RENDERER_H
#define TEST_FIXED_FUNCTION_CANVAS_ITEM_RENDERER_H

#include "drivers/fixed_function/rasterizer_canvas_fixed_function.h"

#include "servers/visual/visual_server_canvas.h"

#include <vector>

class TestFixedFunctionCanvasItemRenderer {
	struct FFCanvasFixture {
		VisualServerCanvas server;
		List<RID> items;

		RID create_item();
		~FFCanvasFixture();
	};
	static Ref<Image> _make_image();
	static Ref<Image> _make_glyph();
	static Ref<Image> _make_nine_patch();
	static bool _check_pixel(const std::vector<uint32_t> &p_pixels, int p_x, int p_y, uint32_t p_expected);
	static bool _render(FixedFunctionRenderTargetManager &p_manager, RasterizerCanvasFixedFunction &p_canvas, FixedFunctionGraphicsDevice &p_device, RasterizerCanvas::Item *p_items, std::vector<uint32_t> &r_pixels);
	static bool _test_redraw_requests(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionTextureStorage &p_storage, RID p_texture);
	static bool _test_rect_batches(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionRenderContext &p_context, RID p_glyph, std::vector<uint32_t> &r_pixels);
	static bool _test_thin_lines(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels);
	static bool _test_meshes(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionRenderContext &p_context);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_CANVAS_ITEM_RENDERER_H
