#ifndef TEST_FIXED_FUNCTION_UNLIT_SCENE_H
#define TEST_FIXED_FUNCTION_UNLIT_SCENE_H

#include "drivers/fixed_function/scene_renderer.h"

#include <vector>

class TestFixedFunctionUnlitScene {
	struct FFInstance : RasterizerScene::InstanceBase {
		void base_removed() override { base = RID(); }
		void base_changed(bool p_aabb, bool p_materials) override {}
		FFInstance(RID p_mesh, RID p_material);
	};
	static bool _test_materials();
	static bool _test_cutout_materials();
	static bool _test_fallbacks(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_cutouts(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static RID _mesh(FixedFunctionMeshStorage &p_meshes, int p_vertex_count = 3);
	static bool _draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, FFInstance &p_instance, const CameraMatrix &p_projection, std::vector<uint32_t> &r_pixels, const Transform &p_camera = Transform());
	static bool _color(uint32_t p_actual, uint32_t p_expected);
	static bool _test_textures(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, std::vector<uint32_t> &r_pixels);
	static bool _test_depth_batches(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, std::vector<uint32_t> &r_pixels);
	static bool _test_rejections(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_UNLIT_SCENE_H
