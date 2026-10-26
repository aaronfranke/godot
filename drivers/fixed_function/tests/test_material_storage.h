#ifndef TEST_FIXED_FUNCTION_MATERIAL_STORAGE_H
#define TEST_FIXED_FUNCTION_MATERIAL_STORAGE_H

#include "drivers/fixed_function/canvas_item_renderer.h"

#include <vector>

class TestFixedFunctionMaterialStorage {
	static String _get_preset_code(int p_blend, int p_light, bool p_particles = false);
	static bool _test_ownership(FixedFunctionMaterialStorage &p_storage);
	static bool _test_shader_metadata(FixedFunctionMaterialStorage &p_storage, FixedFunctionTextureStorage &p_textures);
	static bool _test_properties(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer);
	static bool _test_rejections(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer);
	static bool _render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer, std::vector<uint32_t> &r_pixels);
	static bool _test_antialiased_lines(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 160,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_MATERIAL_STORAGE_H
