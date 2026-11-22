#ifndef TEST_FIXED_FUNCTION_LAMBERT_H
#define TEST_FIXED_FUNCTION_LAMBERT_H

#include "drivers/fixed_function/scene_renderer.h"

#include <vector>

class TestFixedFunctionLambert {
	friend class TestFixedFunctionPixelLighting;
	friend class TestFixedFunctionRetainedLighting;
	struct FFInstance : RasterizerScene::InstanceBase {
		FFInstance(RID p_mesh, RID p_material);
		void base_removed() override { base = RID(); }
		void base_changed(bool p_aabb, bool p_materials) override {}
	};
	static RID _mesh(FixedFunctionMeshStorage &p_storage, uint32_t p_compression, bool p_normal, const Vector3 &p_direction = Vector3(0, 0, 1), bool p_tangents = false, bool p_lines = false, bool p_gradient = false);
	static bool _draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, FFInstance &p_instance, RID p_environment, const Vector<RID> &p_lights, std::vector<uint32_t> &r_pixels, const Transform &p_camera = Transform());
	static bool _color(uint32_t p_actual, uint32_t p_expected);
	static bool _math();
	static bool _normal_formats(FixedFunctionMeshStorage &p_storage);
	static bool _materials(FixedFunctionMaterialStorage &p_materials);
	static bool _test_vertex_path(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_sky_ambient(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_material_channels(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_emission_alpha(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_transient_normal_maps(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static RID _animated_normal_mesh(FixedFunctionMeshStorage &p_storage, uint32_t p_compression, bool p_relative, float p_sign, bool p_tangents = true);
	static bool _test_skinning(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_blend_shapes(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_multimesh(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);

public:
	static bool test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
};

#endif // TEST_FIXED_FUNCTION_LAMBERT_H
