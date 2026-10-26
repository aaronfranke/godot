#ifndef TEST_FIXED_FUNCTION_RETAINED_LIGHTING_H
#define TEST_FIXED_FUNCTION_RETAINED_LIGHTING_H

#include "test_lambert.h"
#include "test_retained_scene.h"

class TestFixedFunctionRetainedLighting {
	friend class TestFixedFunctionLambert;
	static bool _images(const std::vector<uint32_t> &p_left, const std::vector<uint32_t> &p_right);
	static bool _compare(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, TestFixedFunctionLambert::FFInstance &p_instance, RID p_environment, const Vector<RID> &p_lights, const Transform &p_camera = Transform(), std::vector<uint32_t> *r_pixels = nullptr);
	static bool _transient_occlusion(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static RID _occlusion_mesh(FixedFunctionMeshStorage &p_storage, int p_triangles = 1);
	static bool _normal_maps(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static RID _normal_mesh(FixedFunctionMeshStorage &p_storage, uint32_t p_compression, float p_sign, bool p_tangent = true);
	static RID _pack_normal_mesh(FixedFunctionMeshStorage &p_storage, const Array &p_arrays, uint32_t p_compression);
	static RID _tangent_source_mesh(FixedFunctionMeshStorage &p_storage, bool p_mirrored, bool p_indexed);
	static bool _normal_pixel(uint32_t p_actual, const Color &p_texel, float p_strength, float p_sign, const Basis &p_model = Basis(), const Color &p_albedo = Color(0.5, 0.5, 0.5), float p_occlusion = 1, float p_light_affect = 0);
	static bool _run(FixedFunctionGraphicsDevice &p_device, void *p_window);
	static bool _missing_normals(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_environment, const Vector<RID> &p_lights);
	static bool _multimesh_light_range(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material, RID p_environment);
	static bool _benchmark(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material, RID p_environment, const Vector<RID> &p_lights);

public:
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window);
};

#endif // TEST_FIXED_FUNCTION_RETAINED_LIGHTING_H
