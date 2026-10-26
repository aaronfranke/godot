#include "scene_renderer.h"

#include <float.h>
#include <string.h>

bool FixedFunctionSceneRenderer::_prepare_normal_map(FFDrawPacket &p_packet) {
	const FixedFunctionSpatialMaterial::FFNormalMapChannel &normal = p_packet.material.normal;
	if (!FixedFunctionMeshStorage::surface_get_triangle_count(p_packet.surface) ||
			!normal.texture.is_valid() || !normal.strength || !_context.get_device().supports_normal_mapping() ||
			!_normal_mapping_enabled || !_per_pixel_lighting ||
			(p_packet.material.lighting_flags & (FixedFunctionSpatialMaterial::LIGHTING_UNSHADED | FixedFunctionSpatialMaterial::LIGHTING_VERTEX)) ||
			normal.uv_set < 0 || normal.uv_set > 1 || !(p_packet.surface.format & VS::ARRAY_FORMAT_NORMAL) ||
			!(p_packet.surface.format & (normal.uv_set ? VS::ARRAY_FORMAT_TEX_UV2 : VS::ARRAY_FORMAT_TEX_UV))) {
		return false;
	}
	const int corners = p_packet.surface.index_count ? p_packet.surface.index_count : p_packet.surface.vertex_count;
	if (!(p_packet.surface.format & VS::ARRAY_FORMAT_TANGENT) && corners / 3 > MAX_GENERATED_TANGENT_TRIANGLES) {
		return false;
	}
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	if (!textures.owns_texture(normal.texture)) {
		return false;
	}
	const RID target = textures.texture_get_render_target(normal.texture);
	if (target.is_valid()) {
		if (target == _render_targets.get_current_render_target() || textures.render_target_has_failed(target) || textures.render_target_mark_used(target) != OK) {
			return false; // An unavailable optional map must not discard base geometry.
		}
	}
	return true;
}

bool FixedFunctionSceneRenderer::_read_tangent(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, Vector3 &r_tangent, float &r_sign) {
	if (!(p_surface.format & VS::ARRAY_FORMAT_TANGENT)) {
		return false;
	}
	_read_tangent_data(p_surface, p_data, p_index, r_tangent, r_sign);
	r_tangent = _unit_normal(r_tangent);
	if (r_tangent == Vector3() || Math::is_nan(r_sign) || Math::is_inf(r_sign) || r_sign == 0) {
		return false;
	}
	r_sign = r_sign < 0 ? -1 : 1;
	return true;
}

void FixedFunctionSceneRenderer::_read_tangent_data(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, Vector3 &r_tangent, float &r_sign) {
	if (!p_surface.immediate_vertices.empty()) {
		const Plane &tangent = p_surface.immediate_vertices[p_index].tangent;
		r_tangent = tangent.normal;
		r_sign = float(tangent.d);
		return;
	}
	const uint8_t *source = p_data + p_surface.offsets[VS::ARRAY_TANGENT] + p_index * p_surface.strides[VS::ARRAY_TANGENT];
	Vector3 tangent;
	float sign = 0;
	if (p_surface.format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
		Vector2 encoded;
		if (p_surface.format & VS::ARRAY_COMPRESS_TANGENT) {
			int8_t values[2];
			memcpy(values, source, sizeof(values));
			encoded = Vector2(MAX(real_t(values[0]) / 127, real_t(-1)), MAX(real_t(values[1]) / 127, real_t(-1)));
		} else {
			int16_t values[2];
			memcpy(values, source, sizeof(values));
			encoded = Vector2(MAX(real_t(values[0]) / 32767, real_t(-1)), MAX(real_t(values[1]) / 32767, real_t(-1)));
		}
		tangent = VisualServer::oct_to_tangent(encoded, &sign);
	} else if (p_surface.format & VS::ARRAY_COMPRESS_TANGENT) {
		int8_t values[4];
		memcpy(values, source, sizeof(values));
		tangent = Vector3(MAX(real_t(values[0]) / 127, real_t(-1)), MAX(real_t(values[1]) / 127, real_t(-1)), MAX(real_t(values[2]) / 127, real_t(-1)));
		sign = values[3];
	} else {
		float values[4];
		memcpy(values, source, sizeof(values));
		tangent = Vector3(values[0], values[1], values[2]);
		sign = values[3];
	}
	r_tangent = tangent;
	r_sign = sign;
}

Error FixedFunctionSceneRenderer::_pack_normal_geometry_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, const Vector3 &p_normal, FixedFunctionNormalMapVertex &r_vertex) {
	if (p_normal == Vector3()) {
		return ERR_UNAVAILABLE;
	}
	Vector3 tangent;
	float sign = 0;
	if (p_packet.surface.format & VS::ARRAY_FORMAT_TANGENT) {
		const Vector3 normal(r_vertex.lighting.nx, r_vertex.lighting.ny, r_vertex.lighting.nz);
		if (!_read_tangent(p_packet.surface, p_data, p_index, tangent, sign) || normal.cross(tangent).length_squared() < real_t(0.000001)) {
			return ERR_UNAVAILABLE;
		}
	}
	const Vector2 uv = _vertex_uv(p_packet, p_data, p_index, p_packet.material.normal.uv_set);
	if (!(Math::abs(uv.x) <= FLT_MAX && Math::abs(uv.y) <= FLT_MAX)) {
		return ERR_UNAVAILABLE;
	}
	r_vertex.tx = float(tangent.x);
	r_vertex.ty = float(tangent.y);
	r_vertex.tz = float(tangent.z);
	r_vertex.tw = sign;
	r_vertex.normal_u = float(uv.x);
	r_vertex.normal_v = float(uv.y);
	return OK;
}
