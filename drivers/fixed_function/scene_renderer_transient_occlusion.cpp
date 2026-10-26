#include "drivers/fixed_function/scene_renderer.h"

bool FixedFunctionSceneRenderer::_prepare_transient_occlusion(const FFDrawPacket &p_packet, FixedFunctionGraphicsDevice::FFOcclusionMapParameters &r_occlusion) {
	FixedFunctionGraphicsDevice &device = _context.get_device();
	const FixedFunctionSpatialMaterial::FFOcclusionChannel &occlusion = p_packet.material.occlusion;
	if (!device.supports_transient_occlusion_mapping() || !_get_occlusion_texture(occlusion.texture, r_occlusion.texture)) {
		return false;
	}
	r_occlusion.weights = occlusion.weights;
	r_occlusion.light_affect = occlusion.light_affect;
	r_occlusion.sampler_flags = occlusion.sampler_flags;
	return true;
}

Error FixedFunctionSceneRenderer::_occlusion_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, FixedFunctionTransientOcclusionVertex *r_vertices) {
	FixedFunctionLightingVertex lighting[3];
	const Error error = _lighting_triangle(p_packet, p_data, p_indices, lighting);
	if (error != OK) {
		return error;
	}
	for (int i = 0; i < 3; i++) {
		const Error pack_error = _pack_occlusion_vertex(p_packet, p_indices[i], lighting[i], r_vertices[i]);
		if (pack_error != OK) {
			return pack_error;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_pack_occlusion_vertex(const FFDrawPacket &p_packet, uint32_t p_index, const FixedFunctionLightingVertex &p_lighting, FixedFunctionTransientOcclusionVertex &r_vertex) {
	const FixedFunctionSpatialMaterial::FFOcclusionChannel &occlusion = p_packet.material.occlusion;
	const Vector2 uv = FixedFunctionMeshStorage::surface_read_uv(p_packet.surface, p_index, occlusion.uv_set) * occlusion.uv_scale + occlusion.uv_offset;
	if (!(Math::abs(uv.x) <= real_t(1.0e10)) || !(Math::abs(uv.y) <= real_t(1.0e10))) {
		return ERR_UNAVAILABLE;
	}
	r_vertex.lighting = p_lighting;
	r_vertex.occlusion_u = float(uv.x);
	r_vertex.occlusion_v = float(uv.y);
	return OK;
}
