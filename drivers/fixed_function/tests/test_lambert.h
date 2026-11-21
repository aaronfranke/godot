#pragma once

#include "drivers/fixed_function/scene_renderer.h"

#include <vector>

class TestFixedFunctionLambert {
	friend class TestFixedFunctionSceneCPU;
	friend class TestFixedFunctionPixelLighting;
	friend class TestFixedFunctionRetainedLighting;
	struct FFInstance : FixedFunctionGeometryInstance {
		FFInstance(FixedFunctionRenderContext &p_context, RID p_mesh, RID p_material);
	};
	static RID _mesh(FixedFunctionMeshStorage &p_storage, uint64_t p_compression, bool p_normal, const Vector3 &p_direction = Vector3(0, 0, 1), bool p_tangents = false, bool p_lines = false, bool p_gradient = false);
	static bool _draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, FFInstance &p_instance, const FixedFunctionSceneRenderer::FFLightingEnvironment &p_environment, const Vector<RID> &p_lights, std::vector<uint32_t> &r_pixels, const Transform3D &p_camera = Transform3D());
	static bool _color(uint32_t p_actual, uint32_t p_expected);
	static bool _math();
	static bool _normal_formats(FixedFunctionMeshStorage &p_storage);
	static bool _materials(FixedFunctionRenderContext &p_context);
	static bool _test_material_channels(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_vertex_path(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static RID _animated_normal_mesh(FixedFunctionMeshStorage &p_storage, uint64_t p_flags, bool p_relative, float p_sign, bool p_tangents = true, bool p_shapes = true, bool p_indexed = false);
	static bool _test_transient_normal_maps(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_emission_alpha(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_sky_ambient(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);

public:
	static bool test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool test_streamed_vertex_lighting(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
};
