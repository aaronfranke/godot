#pragma once

#include "drivers/fixed_function/tests/test_lambert.h"
#include "drivers/fixed_function/tests/test_retained_scene.h"

class TestFixedFunctionRetainedLighting {
	friend class TestFixedFunctionLambert;
	static RID _mesh_from_arrays(FixedFunctionMeshStorage &p_storage, const Array &p_arrays, uint64_t p_compression = 0);
	static bool _images(const std::vector<uint32_t> &p_left, const std::vector<uint32_t> &p_right);
	static bool _compare(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, TestFixedFunctionLambert::FFInstance &p_instance, const FixedFunctionSceneRenderer::FFLightingEnvironment &p_environment, const Vector<RID> &p_lights, const Transform3D &p_camera = Transform3D(), std::vector<uint32_t> *r_pixels = nullptr);
	static bool _transient_occlusion(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static RID _occlusion_mesh(FixedFunctionMeshStorage &p_storage, int p_triangles = 1);
	static bool _normal_maps(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static RID _normal_mesh(FixedFunctionMeshStorage &p_storage, uint64_t p_compression, float p_sign, bool p_tangent = true);
	static bool _normal_pixel(uint32_t p_actual, const Color &p_texel, float p_strength, float p_sign, const Basis &p_model = Basis(), const Color &p_albedo = Color(0.5, 0.5, 0.5), float p_occlusion = 1, float p_light_affect = 0);
	static bool _run(FixedFunctionGraphicsDevice &p_device, void *p_window);
	static bool _missing_normals(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, const FixedFunctionSceneRenderer::FFLightingEnvironment &p_environment, const Vector<RID> &p_lights);
	static bool _multimesh_light_range(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material, const FixedFunctionSceneRenderer::FFLightingEnvironment &p_environment);
	static bool _benchmark(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material, const FixedFunctionSceneRenderer::FFLightingEnvironment &p_environment, const Vector<RID> &p_lights);

public:
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window);
};
