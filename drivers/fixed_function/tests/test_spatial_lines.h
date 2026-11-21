#pragma once

#include "drivers/fixed_function/scene_renderer.h"

#include <vector>

class TestFixedFunctionSpatialLines {
	struct FFInstance : FixedFunctionGeometryInstance {
		FFInstance(FixedFunctionRenderContext &p_context, RID p_mesh, RID p_material);
	};
	static RenderingServerTypes::RenderInfo _render_info;
	static bool _math();
	static bool _aliased(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _quality(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _color(uint32_t p_pixel, uint32_t p_expected);
	static RID _mesh(FixedFunctionMeshStorage &p_meshes, RSE::PrimitiveType p_primitive, const Vector<Vector3> &p_positions, const Vector<int> &p_indices = Vector<int>());
	static bool _draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RenderGeometryInstance **p_instances, int p_count, const Projection &p_projection, std::vector<uint32_t> &r_pixels);
	static bool _depth(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, const Projection &p_projection, std::vector<uint32_t> &r_pixels);
	static bool _attributes(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, const Projection &p_projection, std::vector<uint32_t> &r_pixels);

public:
	static bool test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
};
