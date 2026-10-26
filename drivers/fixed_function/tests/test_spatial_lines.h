#ifndef TEST_FIXED_FUNCTION_SPATIAL_LINES_H
#define TEST_FIXED_FUNCTION_SPATIAL_LINES_H

#include "drivers/fixed_function/scene_renderer.h"

#include <vector>

class TestFixedFunctionSpatialLines {
	struct FFInstance : RasterizerScene::InstanceBase {
		void base_removed() override { base = RID(); }
		void base_changed(bool p_aabb, bool p_materials) override {}
		FFInstance(RID p_mesh, RID p_material);
	};
	static bool _math();
	static bool _aliased(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _quality(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _color(uint32_t p_pixel, uint32_t p_expected);
	static RID _mesh(FixedFunctionMeshStorage &p_meshes, VS::PrimitiveType p_primitive, const PoolVector<Vector3> &p_positions, const PoolVector<int> &p_indices = PoolVector<int>());
	static bool _draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RasterizerScene::InstanceBase **p_instances, int p_count, const CameraMatrix &p_projection, std::vector<uint32_t> &r_pixels);
	static bool _depth(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, const CameraMatrix &p_projection, std::vector<uint32_t> &r_pixels);
	static bool _attributes(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, const CameraMatrix &p_projection, std::vector<uint32_t> &r_pixels);

public:
	static bool test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
};

#endif // TEST_FIXED_FUNCTION_SPATIAL_LINES_H
