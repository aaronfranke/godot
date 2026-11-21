#pragma once

#include "drivers/fixed_function/scene_renderer.h"

#include <vector>

class TestFixedFunctionUnlitScene {
	struct FFInstance : FixedFunctionGeometryInstance {
		FFInstance(FixedFunctionRenderContext &p_context, RID p_mesh, RID p_material);
	};
	static RenderingServerTypes::RenderInfo _render_info;
	static Error _render(FixedFunctionSceneRenderer &p_renderer, const Projection &p_projection, RenderGeometryInstance **p_instances, int p_count);
	static bool _test_cutout_materials();
	static bool _test_fallbacks(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_cutouts(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_materials(FixedFunctionGraphicsDevice &p_device, void *p_window);
	static RID _mesh(FixedFunctionMeshStorage &p_meshes, int p_vertex_count = 3);
	static bool _draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, FFInstance &p_instance, const Projection &p_projection, std::vector<uint32_t> &r_pixels, const Transform3D &p_camera = Transform3D());
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
