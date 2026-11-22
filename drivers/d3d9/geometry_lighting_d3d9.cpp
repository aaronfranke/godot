#include "graphics_device_d3d9.h"

#include "core/math/math_funcs.h"

Error FixedFunctionGraphicsDeviceD3D9::_draw_lit_geometry(RID p_geometry, const CameraMatrix &p_clip_from_model, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters *p_normal, const FFOcclusionMapParameters *p_occlusion) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!supports_lit_geometry()) {
		return ERR_UNAVAILABLE;
	}
	if (!_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	D3D9Geometry *geometry = _geometry_owner.getptr(p_geometry);
	const D3D9GeometryBatch *first = geometry->batches[0];
	if (p_normal ? !first->source.normal_vertices.size() : !first->source.lighting_vertices.size()) {
		return ERR_INVALID_PARAMETER;
	}
	error = _validate_pixel_lighting(p_lighting);
	if (error != OK) {
		return error;
	}
	// A scalar rescaling preserves normal directions and bounds shader intermediates.
	real_t normal_scale = 0;
	for (int column = 0; column < 3; column++) {
		const Vector3 axis = p_parameters.normal_from_model.get_axis(column);
		for (int row = 0; row < 3; row++) {
			if (Math::is_nan(axis[row]) || Math::is_inf(axis[row])) {
				return ERR_UNAVAILABLE;
			}
			normal_scale = MAX(normal_scale, Math::abs(axis[row]));
		}
	}
	if (!normal_scale) {
		return ERR_UNAVAILABLE;
	}
	Basis normals;
	for (int column = 0; column < 3; column++) {
		normals.set_axis(column, p_parameters.normal_from_model.get_axis(column) / normal_scale);
	}
	// Very anisotropic/singular transforms retain robust CPU normal/face handling.
	if (Math::abs(normals.determinant()) < real_t(0.0001)) {
		return ERR_UNAVAILABLE;
	}
	const Transform world = Transform(p_lighting.camera_basis, Vector3()) * p_parameters.view_from_anchor;
	// Match the retained vertex shader's float4 registers in shaders/lighting.hlsl.
	enum {
		ALBEDO_UV_REGISTER = 12,
		LIT_REGISTER_COUNT = ALBEDO_UV_REGISTER + 1,
		NORMAL_UV_REGISTER = LIT_REGISTER_COUNT,
		UV_SOURCES_REGISTER = NORMAL_UV_REGISTER + 1,
		NORMAL_MAPPED_REGISTER_COUNT = UV_SOURCES_REGISTER + 1,
		OCCLUSION_UV_REGISTER = NORMAL_MAPPED_REGISTER_COUNT,
		OCCLUSION_REGISTER_COUNT = OCCLUSION_UV_REGISTER + 1,
	};
	float constants[OCCLUSION_REGISTER_COUNT * 4] = {};
	int constant_count = LIT_REGISTER_COUNT;
	for (int row = 0; row < 3; row++) {
		double magnitude = Math::abs(double(world.origin[row]));
		for (int column = 0; column < 3; column++) {
			const real_t value = world.basis.get_axis(column)[row];
			magnitude += Math::abs(double(value)) * geometry->extent[column];
			constants[16 + column * 4 + row] = float(value);
			constants[32 + column * 4 + row] = float(normals.get_axis(column)[row]);
		}
		if (!(magnitude <= 1.0e10)) {
			return ERR_UNAVAILABLE;
		}
		constants[28 + row] = float(world.origin[row]);
	}
	for (int channel = 0; channel < 4; channel++) {
		const float value = p_parameters.color[channel];
		if (Math::is_nan(value) || Math::is_inf(value) || Math::abs(value) > 1.0e10f) {
			return ERR_UNAVAILABLE;
		}
		// Match the existing transient lighting vertex's pre-interpolation alpha.
		constants[44 + channel] = channel == 3 ? Math::round(CLAMP(value, 0.0f, 1.0f) * 255.0f) / 255.0f : value;
	}
	float *albedo_uv = constants + ALBEDO_UV_REGISTER * 4;
	albedo_uv[0] = float(p_parameters.albedo_uv_scale.x);
	albedo_uv[1] = float(p_parameters.albedo_uv_scale.y);
	albedo_uv[2] = float(p_parameters.albedo_uv_offset.x);
	albedo_uv[3] = float(p_parameters.albedo_uv_offset.y);
	if (p_normal) {
		if (!_normal_map_vertex_shader || !_normal_map_pixel_shader || !_normal_map_textured_shader || !p_normal->texture.is_valid() || !_texture_owner.owns(p_normal->texture) || p_normal->texture == _render_target) {
			return ERR_UNAVAILABLE;
		}
		const D3D9Texture *texture = _texture_owner.getptr(p_normal->texture);
		if (!texture->native || (texture->render_target && !texture->rendered)) {
			return ERR_UNAVAILABLE;
		}
		FFDrawParameters sampling = p_state;
		sampling.sampler_flags = p_normal->sampler_flags;
		error = validate_draw_state(sampling);
		if (error != OK || Math::is_nan(p_normal->strength) || Math::is_inf(p_normal->strength) || Math::abs(p_normal->strength) > 1.0e10f) {
			return ERR_UNAVAILABLE;
		}
		float *normal_uv = constants + NORMAL_UV_REGISTER * 4;
		normal_uv[0] = float(p_normal->uv_scale.x);
		normal_uv[1] = float(p_normal->uv_scale.y) * (p_normal->flip_v ? -1 : 1);
		normal_uv[2] = float(p_normal->uv_offset.x);
		normal_uv[3] = p_normal->flip_v ? 1 - float(p_normal->uv_offset.y) : float(p_normal->uv_offset.y);
		constants[UV_SOURCES_REGISTER * 4] = p_normal->albedo_uses_normal_uv ? 1.0f : 0.0f;
		constant_count = NORMAL_MAPPED_REGISTER_COUNT;
	}
	const D3D9Texture *occlusion_texture = nullptr;
	if (p_occlusion) {
		if (!p_occlusion->texture.is_valid() || !_texture_owner.owns(p_occlusion->texture)) {
			return ERR_UNAVAILABLE;
		}
		occlusion_texture = _texture_owner.getptr(p_occlusion->texture);
		if (!occlusion_texture->native || occlusion_texture->render_target || !(p_occlusion->light_affect >= 0 && p_occlusion->light_affect <= 1)) {
			return ERR_UNAVAILABLE;
		}
		for (int channel = 0; channel < 4; channel++) {
			if (!(Math::abs(p_occlusion->weights[channel]) <= 1.0e10f)) {
				return ERR_UNAVAILABLE;
			}
		}
		FFDrawParameters sampling = p_state;
		sampling.sampler_flags = p_occlusion->sampler_flags;
		if (validate_draw_state(sampling) != OK) {
			return ERR_UNAVAILABLE;
		}
		error = _create_occlusion_shaders(p_normal != nullptr);
		if (error != OK) {
			return error;
		}
		float *occlusion_uv = constants + OCCLUSION_UV_REGISTER * 4;
		occlusion_uv[0] = float(p_occlusion->uv_scale.x);
		occlusion_uv[1] = float(p_occlusion->uv_scale.y);
		occlusion_uv[2] = float(p_occlusion->uv_offset.x);
		occlusion_uv[3] = float(p_occlusion->uv_offset.y);
		constants[UV_SOURCES_REGISTER * 4 + 1] = p_occlusion->uses_secondary_uv ? 1.0f : 0.0f;
		constant_count = OCCLUSION_REGISTER_COUNT;
	}
	const PoolVector<FixedFunctionLightingVertex>::Read vertices = first->source.lighting_vertices.read();
	const PoolVector<FixedFunctionNormalMapVertex>::Read normal_vertices = first->source.normal_vertices.read();
	Rect2i viewport;
	error = _prepare_draw(p_normal ? static_cast<const void *>(normal_vertices.ptr()) : static_cast<const void *>(vertices.ptr()), first->source.indices.size(), p_texture, p_state, viewport);
	if (error != OK || !viewport.size.x || !viewport.size.y) {
		return error;
	}
	store_camera(_get_draw_projection(p_clip_from_model, viewport), constants);
	for (int i = 0; i < constant_count * 4; i++) {
		if (Math::is_nan(constants[i]) || Math::is_inf(constants[i]) || Math::abs(constants[i]) > 1.0e10f) {
			return ERR_UNAVAILABLE;
		}
	}
	error = _bind_lighting(constants, constant_count, p_lighting, p_texture, p_state, true, p_normal, p_occlusion);
	if (error != OK) {
		if (p_occlusion && error == FAILED && _last_result == D3DERR_INVALIDCALL) {
			return ERR_UNAVAILABLE;
		}
		return error;
	}
	// Every optional upload settles before the first draw, preserving fallback safety.
	for (int i = 0; i < geometry->batches.size(); i++) {
		error = _check_result(_upload_geometry_batch(geometry->batches[i]));
		if (error != OK) {
			return error;
		}
	}
	if (p_occlusion) {
		error = _check_result(_device->SetSamplerState(2, D3DSAMP_SRGBTEXTURE, FALSE));
		if (error == OK) {
			error = _set_sampler_state(p_occlusion->sampler_flags, 2, occlusion_texture->mip_levels);
		}
		if (error == OK) {
			error = _check_result(_device->SetTexture(2, occlusion_texture->native));
		}
		if (error != OK) {
			return _last_result == D3DERR_INVALIDCALL ? ERR_UNAVAILABLE : error;
		}
	}
	error = _check_result(_draw_geometry_batches(geometry));
	if (p_occlusion) {
		// Keep sampler 2 scoped to this draw, including failed submissions.
		const HRESULT unbound = _device->SetTexture(2, nullptr);
		if (error == OK) {
			error = _check_result(unbound);
		}
	}
	return error == ERR_UNAVAILABLE ? FAILED : error;
}

Error FixedFunctionGraphicsDeviceD3D9::draw_lit_geometry(RID p_geometry, const CameraMatrix &p_clip_from_model, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFOcclusionMapParameters *p_occlusion) {
	return _draw_lit_geometry(p_geometry, p_clip_from_model, p_parameters, p_lighting, p_texture, p_state, nullptr, p_occlusion);
}
Error FixedFunctionGraphicsDeviceD3D9::draw_normal_mapped_geometry(RID p_geometry, const CameraMatrix &p_clip_from_model, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters &p_normal, const FFOcclusionMapParameters *p_occlusion) {
	return _draw_lit_geometry(p_geometry, p_clip_from_model, p_parameters, p_lighting, p_texture, p_state, &p_normal, p_occlusion);
}
