#include "drivers/fixed_function/scene_renderer.h"

#include <float.h>
#include <string.h>

bool FixedFunctionSceneRenderer::_prepare_normal_map(FFDrawPacket &p_packet) {
	const FixedFunctionSpatialMaterial::FFNormalMapChannel &normal = p_packet.material.normal;
	if (!FixedFunctionMeshStorage::surface_get_triangle_count(p_packet.surface) ||
			!normal.texture.is_valid() || !normal.strength || !_context.get_device().supports_normal_mapping() ||
			!_normal_mapping_enabled || !_per_pixel_lighting ||
			(p_packet.material.lighting_flags & (FixedFunctionSpatialMaterial::LIGHTING_UNSHADED | FixedFunctionSpatialMaterial::LIGHTING_VERTEX)) ||
			normal.uv_set < 0 || normal.uv_set > 1 || !(p_packet.surface.source.format & RSE::ARRAY_FORMAT_NORMAL) ||
			!(p_packet.surface.source.format & (normal.uv_set ? RSE::ARRAY_FORMAT_TEX_UV2 : RSE::ARRAY_FORMAT_TEX_UV))) {
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

bool FixedFunctionSceneRenderer::_unpack_tangent(const Vector4 &p_frame, Vector3 &r_tangent, float &r_sign) {
	const Vector3 tangent(p_frame.x, p_frame.y, p_frame.z);
	const float sign = float(p_frame.w);
	r_tangent = _unit_normal(tangent);
	if (r_tangent == Vector3() || Math::is_nan(sign) || Math::is_inf(sign) || sign == 0) {
		return false;
	}
	r_sign = sign < 0 ? -1 : 1;
	return true;
}

Error FixedFunctionSceneRenderer::_pack_normal_geometry_vertex(const Vector4 &p_tangent, const Vector2 &p_uv, bool p_has_tangent, FixedFunctionNormalMapVertex &r_vertex) {
	Vector3 tangent;
	float sign = 1;
	if (p_has_tangent && !_unpack_tangent(p_tangent, tangent, sign)) {
		return ERR_UNAVAILABLE;
	}
	const Vector3 normal(r_vertex.lighting.nx, r_vertex.lighting.ny, r_vertex.lighting.nz);
	if (_unit_normal(normal) == Vector3() || (p_has_tangent && normal.cross(tangent).length_squared() < real_t(0.000001))) {
		return ERR_UNAVAILABLE;
	}
	if (!(Math::abs(p_uv.x) <= FLT_MAX && Math::abs(p_uv.y) <= FLT_MAX)) {
		return ERR_UNAVAILABLE;
	}
	r_vertex.tx = float(tangent.x);
	r_vertex.ty = float(tangent.y);
	r_vertex.tz = float(tangent.z);
	r_vertex.tw = sign;
	r_vertex.normal_u = float(p_uv.x);
	r_vertex.normal_v = float(p_uv.y);
	return OK;
}
