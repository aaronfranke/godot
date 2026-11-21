#include "drivers/fixed_function/scene_renderer.h"

bool FixedFunctionSceneRenderer::_prepare_transient_normal_map(const FFDrawPacket &p_packet, FixedFunctionGraphicsDevice::FFNormalMapParameters &r_normal, FixedFunctionGraphicsDevice::FFOcclusionMapParameters &r_occlusion, bool &r_use_occlusion) {
	FixedFunctionGraphicsDevice &device = _context.get_device();
	if (!device.supports_transient_normal_mapping() || !(p_packet.surface.source.format & RSE::ARRAY_FORMAT_TANGENT)) {
		return false;
	}
	const FixedFunctionSpatialMaterial::FFNormalMapChannel &normal = p_packet.material.normal;
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	const RID target = textures.texture_get_render_target(normal.texture);
	const Error error = textures.texture_get_or_upload_device_texture(normal.texture, r_normal.texture);
	if (error != OK || (target.is_valid() && !textures.render_target_is_ready(target))) {
		_redraw_requested |= error == ERR_SKIP || target.is_valid();
		return false; // A pending optional map leaves geometry visible and requests another frame.
	}
	r_normal.strength = normal.strength;
	r_normal.sampler_flags = normal.sampler_flags;
	r_normal.flip_v = false; // 4.x viewport textures use the native top-left UV domain.
	r_use_occlusion = p_packet.occlusion;
	if (r_use_occlusion) {
		const FixedFunctionSpatialMaterial::FFOcclusionChannel &occlusion = p_packet.material.occlusion;
		if (!device.supports_transient_normal_mapped_occlusion() || !_get_occlusion_texture(occlusion.texture, r_occlusion.texture)) {
			return false;
		}
		r_occlusion.weights = occlusion.weights;
		r_occlusion.light_affect = occlusion.light_affect;
		r_occlusion.sampler_flags = occlusion.sampler_flags;
	}
	return true;
}

Error FixedFunctionSceneRenderer::_normal_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, const Basis &p_world_from_model, FixedFunctionTransientNormalMapVertex *r_vertices, bool p_flip_v, bool p_occlusion) {
	FixedFunctionLightingVertex lighting[3];
	FFModelTangentFrame frames[3];
	const Error error = _lighting_triangle(p_packet, p_data, p_indices, lighting, frames);
	if (error != OK) {
		return error;
	}
	for (int i = 0; i < 3; i++) {
		const Error pack_error = _pack_normal_vertex(p_packet, p_indices[i], p_world_from_model, frames[i], lighting[i], r_vertices[i], p_flip_v, p_occlusion);
		if (pack_error != OK) {
			return pack_error;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_pack_normal_vertex(const FFDrawPacket &p_packet, uint32_t p_index, const Basis &p_world_from_model, const FFModelTangentFrame &p_frame, const FixedFunctionLightingVertex &p_lighting, FixedFunctionTransientNormalMapVertex &r_vertex, bool p_flip_v, bool p_occlusion) {
	const FixedFunctionSpatialMaterial::FFNormalMapChannel &normal = p_packet.material.normal;
	const FixedFunctionSpatialMaterial::FFOcclusionChannel &occlusion = p_packet.material.occlusion;
	Vector3 tangent, bitangent;
	if (!p_packet.deformed_tangents.is_empty()) {
		tangent = p_packet.deformed_tangents[p_index].tangent;
		bitangent = p_packet.deformed_tangents[p_index].bitangent;
	} else {
		float sign;
		if (!_unpack_tangent(p_frame.tangent, tangent, sign)) {
			return ERR_UNAVAILABLE;
		}
		bitangent = _unit_normal(p_frame.normal.cross(tangent)) * sign;
	}
	tangent = _unit_normal(p_world_from_model.xform(tangent));
	bitangent = _unit_normal(p_world_from_model.xform(bitangent));
	const Vector3 world_normal(p_lighting.nx, p_lighting.ny, p_lighting.nz);
	if (tangent == Vector3() || bitangent == Vector3() || world_normal.cross(tangent).length_squared() < real_t(0.000001)) {
		return ERR_UNAVAILABLE;
	}
	Vector2 normal_uv = FixedFunctionMeshStorage::surface_read_uv(p_packet.surface, p_index, normal.uv_set) * normal.uv_scale + normal.uv_offset;
	if (p_flip_v) {
		normal_uv.y = 1 - normal_uv.y;
	}
	Vector2 occlusion_uv;
	if (p_occlusion) {
		occlusion_uv = FixedFunctionMeshStorage::surface_read_uv(p_packet.surface, p_index, occlusion.uv_set) * occlusion.uv_scale + occlusion.uv_offset;
	}
	const real_t coordinates[] = { normal_uv.x, normal_uv.y, occlusion_uv.x, occlusion_uv.y };
	for (int component = 0; component < 4; component++) {
		if (!(Math::abs(coordinates[component]) <= real_t(1.0e10))) {
			return ERR_UNAVAILABLE;
		}
	}
	r_vertex.lighting = p_lighting;
	r_vertex.tx = float(tangent.x);
	r_vertex.ty = float(tangent.y);
	r_vertex.tz = float(tangent.z);
	r_vertex.bx = float(bitangent.x);
	r_vertex.by = float(bitangent.y);
	r_vertex.bz = float(bitangent.z);
	r_vertex.normal_u = float(normal_uv.x);
	r_vertex.normal_v = float(normal_uv.y);
	r_vertex.occlusion_u = float(occlusion_uv.x);
	r_vertex.occlusion_v = float(occlusion_uv.y);
	return OK;
}
