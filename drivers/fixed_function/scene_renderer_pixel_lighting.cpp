#include "scene_renderer.h"

#include <string.h>

double FixedFunctionSceneRenderer::_local_light_score(const FFLocalLight &p_light, const AABB &p_bounds) {
	double distance_squared = 0;
	double center_distance_squared = 0;
	double center_dot = 0;
	double radius_squared = 0;
	for (int axis = 0; axis < 3; axis++) {
		if (Math::is_nan(p_light.position[axis]) || Math::is_inf(p_light.position[axis])) {
			return 0;
		}
		const real_t nearest = CLAMP(p_light.position[axis], p_bounds.position[axis], p_bounds.position[axis] + p_bounds.size[axis]);
		const double relative = (double(p_light.position[axis]) - nearest) / p_light.range;
		distance_squared += relative * relative;
		const double center_relative = double(p_bounds.position[axis]) + double(p_bounds.size[axis]) * 0.5 - p_light.position[axis];
		center_distance_squared += center_relative * center_relative;
		center_dot += center_relative * p_light.direction[axis];
		radius_squared += double(p_bounds.size[axis]) * p_bounds.size[axis] * 0.25;
	}
	if (distance_squared >= 1) {
		return 0;
	}
	double weight = Math::pow(1.0 - Math::sqrt(distance_squared), double(p_light.attenuation));
	if (p_light.spot) {
		if (p_light.spot_attenuation == 0) {
			return 0;
		}
		// Bound the best cone angle over the AABB's enclosing sphere. The closest
		// distance and best angle may occur at different points: ranking is conservative.
		if (center_distance_squared > radius_squared) {
			const double center_distance = Math::sqrt(center_distance_squared);
			const double angle = Math::acos(CLAMP(center_dot / center_distance, -1.0, 1.0));
			const double span = Math::asin(CLAMP(Math::sqrt(radius_squared) / center_distance, 0.0, 1.0));
			const double cosine = Math::cos(MAX(angle - span, 0.0));
			if (cosine <= p_light.cos_spot_angle) {
				return 0;
			}
			const double rim = CLAMP((1.0 - cosine) / (1.0 - p_light.cos_spot_angle), 0.0, 1.0);
			weight *= 1.0 - Math::pow(rim, double(p_light.spot_attenuation));
		}
	}
	// Absolute luminance ranks darkening lights by effect size while preserving
	// their signed radiance for the actual lighting calculation.
	return weight * (0.2126 * Math::abs(p_light.radiance[0]) + 0.7152 * Math::abs(p_light.radiance[1]) + 0.0722 * Math::abs(p_light.radiance[2]));
}

void FixedFunctionSceneRenderer::_select_pixel_light(FFPixelLightCandidate *p_selected, uint32_t &r_count, uint32_t p_limit, const FFPixelLightCandidate &p_candidate) {
	if (!(p_candidate.score > 0)) {
		return;
	}
	uint32_t position = 0;
	while (position < r_count && (p_selected[position].score > p_candidate.score || (p_selected[position].score == p_candidate.score && p_selected[position].instance_id <= p_candidate.instance_id))) {
		position++;
	}
	if (position == p_limit) {
		return;
	}
	const uint32_t count = MIN(r_count + 1, p_limit);
	for (uint32_t index = count - 1; index > position; index--) {
		p_selected[index] = p_selected[index - 1];
	}
	p_selected[position] = p_candidate;
	r_count = count;
}

bool FixedFunctionSceneRenderer::_prepare_pixel_lighting(const FFDrawPacket &p_packet, FixedFunctionGraphicsDevice::FFPixelLighting &r_lighting) const {
	const uint32_t limit = MIN(_context.get_device().get_max_per_pixel_lights(), uint32_t(FixedFunctionGraphicsDevice::MAX_PIXEL_LIGHTS));
	if (!limit || (p_packet.material.lighting_flags & (FixedFunctionSpatialMaterial::LIGHTING_UNSHADED | FixedFunctionSpatialMaterial::LIGHTING_VERTEX)) || !_per_pixel_lighting) {
		return false;
	}
	if (p_packet.multimesh_local_lighting) {
		uint32_t candidates = 0;
		// Individual copies can rank different lights. A shared list must fit all of them.
		return _select_lights(p_packet, limit, r_lighting, &candidates) && candidates <= limit;
	}
	return _select_lights(p_packet, limit, r_lighting);
}

bool FixedFunctionSceneRenderer::_select_lights(const FFDrawPacket &p_packet, uint32_t p_limit, FixedFunctionGraphicsDevice::FFPixelLighting &r_lighting, uint32_t *r_candidates) const {
	const uint32_t limit = p_limit;
	uint32_t candidates = 0;
	for (int column = 0; column < 3; column++) {
		const Vector3 axis = _camera_basis.get_axis(column);
		for (int row = 0; row < 3; row++) {
			if (Math::is_nan(axis[row]) || Math::is_inf(axis[row]) || Math::abs(axis[row]) > real_t(10000)) {
				return false;
			}
		}
	}
	// SM3 carries unclamped float colors. Extreme values retain the established
	// double-accumulation path rather than overflowing shader intermediates.
	for (int channel = 0; channel < 4; channel++) {
		if (Math::is_nan(p_packet.material.albedo.factor[channel]) || Math::is_inf(p_packet.material.albedo.factor[channel]) || Math::abs(p_packet.material.albedo.factor[channel]) > 1.0e10f) {
			return false;
		}
	}
	if ((p_packet.material.albedo_uses_vertex_color || p_packet.material.albedo_uses_vertex_alpha) && (p_packet.surface.format & VS::ARRAY_FORMAT_COLOR) && !(p_packet.surface.format & VS::ARRAY_COMPRESS_COLOR)) {
		const PoolVector<uint8_t>::Read bytes = p_packet.surface.array.read();
		for (int vertex = 0; vertex < p_packet.surface.vertex_count; vertex++) {
			for (int channel = 0; channel < 4; channel++) {
				if (channel < 3 ? !p_packet.material.albedo_uses_vertex_color : !p_packet.material.albedo_uses_vertex_alpha) {
					continue;
				}
				float value;
				memcpy(&value, bytes.ptr() + p_packet.surface.offsets[VS::ARRAY_COLOR] + vertex * p_packet.surface.strides[VS::ARRAY_COLOR] + channel * 4, 4);
				if (Math::is_nan(value) || Math::is_inf(value) || Math::abs(double(value) * p_packet.material.albedo.factor[channel]) > 1.0e10) {
					return false;
				}
			}
		}
	}
	AABB world_bounds;
	FixedFunctionMeshStorage::FFGeometryBounds shared_bounds;
	if (p_packet.multimesh_local_lighting) {
		for (int axis = 0; axis < 3; axis++) {
			shared_bounds.minimum[axis] = p_packet.multimesh_light_bounds.position[axis];
			shared_bounds.maximum[axis] = double(p_packet.multimesh_light_bounds.position[axis]) + p_packet.multimesh_light_bounds.size[axis];
		}
	} else {
		for (int corner = 0; corner < 8; corner++) {
			const Vector3 view = p_packet.view_from_model.xform(p_packet.surface.aabb.get_endpoint(corner));
			for (int axis = 0; axis < 3; axis++) {
				if (Math::is_nan(view[axis]) || Math::is_inf(view[axis]) || Math::abs(view[axis]) > real_t(1.0e10)) {
					return false;
				}
			}
			const Vector3 world = _camera_basis.xform(view);
			if (corner == 0) {
				world_bounds.position = world;
			} else {
				world_bounds.expand_to(world);
			}
		}
	}
	r_lighting.camera_basis = _camera_basis;
	memset(r_lighting.lights, 0, sizeof(r_lighting.lights));
	r_lighting.light_count = 0;
	r_lighting.shadow = FixedFunctionGraphicsDevice::FFDirectionalShadowParameters();
	for (int channel = 0; channel < 3; channel++) {
		const double value = p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_AMBIENT_DISABLED ? 0 : _ambient[channel];
		if (Math::abs(value) > 1.0e10) {
			return false;
		}
		r_lighting.ambient[channel] = float(value);
	}
	FFPixelLightCandidate selected[FixedFunctionGraphicsDevice::MAX_PIXEL_LIGHTS];
	uint32_t count = 0;
	for (int i = 0; i < _directional_lights.size(); i++) {
		const FFDirectionalLight &source = _directional_lights[i];
		if (!(p_packet.layer_mask & source.cull_mask)) {
			continue;
		}
		FFPixelLightCandidate candidate;
		candidate.score = 0.2126 * Math::abs(source.radiance[0]) + 0.7152 * Math::abs(source.radiance[1]) + 0.0722 * Math::abs(source.radiance[2]);
		candidate.instance_id = source.instance_id;
		candidate.index = i;
		candidate.local = false;
		candidates += candidate.score > 0;
		_select_pixel_light(selected, count, limit, candidate);
	}
	// Directional-only MultiMesh batches already excluded all local lights.
	const int local_count = p_packet.multimesh && !p_packet.multimesh_local_lighting ? 0 : _local_lights.size();
	for (int i = 0; i < local_count; i++) {
		const FFLocalLight &source = _local_lights[i];
		if (!(p_packet.layer_mask & source.cull_mask)) {
			continue;
		}
		FFPixelLightCandidate candidate;
		if (p_packet.multimesh_local_lighting) {
			// Count the full range sphere. The shader handles range and angle per pixel.
			for (int axis = 0; axis < 3; axis++) {
				if (Math::is_nan(source.position[axis]) || Math::is_inf(source.position[axis])) {
					return false;
				}
			}
			if (shared_bounds.range_distance_squared(source.position, source.range) > 1) {
				continue;
			}
			candidate.score = 0.2126 * Math::abs(source.radiance[0]) + 0.7152 * Math::abs(source.radiance[1]) + 0.0722 * Math::abs(source.radiance[2]);
		} else {
			candidate.score = _local_light_score(source, world_bounds);
		}
		candidate.instance_id = source.instance_id;
		candidate.index = i;
		candidate.local = true;
		candidates += candidate.score > 0;
		_select_pixel_light(selected, count, limit, candidate);
	}
	if (r_candidates) {
		*r_candidates = candidates;
	}
	for (uint32_t index = 0; index < count; index++) {
		const FFPixelLightCandidate &candidate = selected[index];
		FixedFunctionGraphicsDevice::FFPixelLight &light = r_lighting.lights[r_lighting.light_count++];
		if (candidate.local) {
			const FFLocalLight &source = _local_lights[candidate.index];
			if (source.range < real_t(1.0e-10) || source.range > real_t(1.0e10) || (source.spot && real_t(1) - source.cos_spot_angle < real_t(1.0e-6))) {
				return false;
			}
			light.color_type[3] = source.spot ? 2.0f : 1.0f;
			light.position_range[3] = float(real_t(1) / source.range);
			light.direction_attenuation[3] = source.attenuation;
			light.cone[0] = float(source.cos_spot_angle);
			light.cone[1] = source.spot_attenuation;
			for (int channel = 0; channel < 3; channel++) {
				if (Math::abs(source.radiance[channel]) > 1.0e10 || Math::abs(source.position[channel]) > real_t(1.0e10) ||
						(p_packet.multimesh_local_lighting && source.radiance[channel] < 0)) {
					return false;
				}
				light.color_type[channel] = float(source.radiance[channel]);
				light.position_range[channel] = float(source.position[channel]);
				light.direction_attenuation[channel] = float(source.direction[channel]);
			}
		} else {
			const FFDirectionalLight &source = _directional_lights[candidate.index];
			if (_shadow_parameters.texture.is_valid() && source.instance_id == _shadow_transform.instance_id && !(p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_SHADOWS_DISABLED)) {
				r_lighting.shadow = _shadow_parameters;
				r_lighting.shadow.light_index = index;
			}
			for (int channel = 0; channel < 3; channel++) {
				// Per-copy ranking can accumulate lights in a different order.
				// Preserve that order when signed terms can cancel each other.
				if (Math::abs(source.radiance[channel]) > 1.0e10 || (p_packet.multimesh_local_lighting && source.radiance[channel] < 0)) {
					return false;
				}
				light.color_type[channel] = float(source.radiance[channel]);
				light.direction_attenuation[channel] = float(source.direction[channel]);
			}
		}
	}
	return true;
}

bool FixedFunctionSceneRenderer::_uses_vertex_lit_geometry(const FFDrawPacket &p_packet) const {
	return _native_lighting_mode && (!p_packet.occlusion || p_packet.baked_occlusion_image_version) && !p_packet.normal_mapping &&
			!(p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED) && _context.get_device().get_max_vertex_lights();
}

real_t FixedFunctionSceneRenderer::_uniform_basis_scale_squared(const Basis &p_basis) {
	const Vector3 axes[3] = { p_basis.get_axis(0), p_basis.get_axis(1), p_basis.get_axis(2) };
	const real_t scale_squared = axes[0].length_squared();
	if (!(scale_squared > real_t(1.0e-12) && scale_squared < real_t(1.0e12))) {
		return 0;
	}
	const real_t tolerance = scale_squared * real_t(1.0e-4);
	for (int i = 0; i < 3; i++) {
		if (!(Math::abs(axes[i].length_squared() - scale_squared) <= tolerance && Math::abs(axes[i].dot(axes[(i + 1) % 3])) <= tolerance)) {
			return 0;
		}
	}
	return scale_squared;
}

bool FixedFunctionSceneRenderer::_prepare_vertex_lighting(const FFDrawPacket &p_packet, const Vector3 &p_anchor, FixedFunctionGraphicsDevice::FFPixelLighting &r_lighting) const {
	if (!_uses_vertex_lit_geometry(p_packet)) {
		return false;
	}
	// Lighting in model space is only equivalent for a rotation, reflection and uniform scale.
	const Basis world_from_model = _camera_basis * p_packet.view_from_model.basis;
	const Vector3 origin = _camera_basis.xform(p_packet.view_from_model.xform(p_anchor));
	return _prepare_native_lights(p_packet, _context.get_device().get_max_vertex_lights(), world_from_model, origin, p_packet.baked_occlusion_image_version != 0, r_lighting);
}

bool FixedFunctionSceneRenderer::_prepare_streamed_vertex_lighting(const FFDrawPacket &p_packet, bool p_occlusion, FixedFunctionGraphicsDevice::FFPixelLighting &r_lighting, Basis &r_normal_from_model) const {
	const uint32_t limit = _context.get_device().get_max_streamed_vertex_lights();
	const bool vertex_color = (p_packet.surface.format & VS::ARRAY_FORMAT_COLOR) && (p_packet.material.albedo_uses_vertex_color || p_packet.material.albedo_uses_vertex_alpha);
	if (!limit || !_native_lighting_mode || _shadow_pass || vertex_color || p_packet.normal_mapping ||
			(p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED) ||
			(p_occlusion && p_packet.material.occlusion.light_affect != 0.0f)) {
		return false;
	}
	// Deformation and instance scale are already reflected in the positions and inverse-
	// transpose normals. Only the camera's basis must preserve angles and relative distances.
	if (!_prepare_native_lights(p_packet, limit, _camera_basis, Vector3(), p_occlusion, r_lighting) || !r_lighting.light_count) {
		// The CPU ambient-only path can skip reading normals entirely.
		return false;
	}
	// Normalization after the combined transform also removes uniform camera scale.
	r_normal_from_model = _camera_basis.transposed() * p_packet.normal_from_model;
	return true;
}

bool FixedFunctionSceneRenderer::_prepare_native_lights(const FFDrawPacket &p_packet, uint32_t p_limit, const Basis &p_world_from_input, const Vector3 &p_origin, bool p_occlusion, FixedFunctionGraphicsDevice::FFPixelLighting &r_lighting) const {
	const real_t scale_squared = _uniform_basis_scale_squared(p_world_from_input);
	if (scale_squared == 0) {
		return false;
	}
	const uint32_t limit = MIN(p_limit, uint32_t(FixedFunctionGraphicsDevice::MAX_PIXEL_LIGHTS));
	uint32_t candidates = 0;
	// Keep CPU lighting when the native path cannot draw all the lights.
	if (!_select_lights(p_packet, limit, r_lighting, &candidates) || candidates > limit) {
		return false;
	}
	r_lighting.vertex_ambient_occlusion = p_occlusion;
	if (r_lighting.vertex_ambient_occlusion && !_context.get_device().supports_vertex_ambient_occlusion(p_packet.material.albedo.factor * r_lighting.ambient)) {
		return false;
	}
	const real_t scale = Math::sqrt(scale_squared);
	for (uint32_t i = 0; i < r_lighting.light_count; i++) {
		FixedFunctionGraphicsDevice::FFPixelLight &light = r_lighting.lights[i];
		const bool local = light.color_type[3] != 0;
		if (local && _native_lighting_mode < 2) {
			return false;
		}
		// The inverse of an orthogonal basis with uniform scale is its transpose over scale squared.
		const Vector3 direction = p_world_from_input.xform_inv(Vector3(light.direction_attenuation[0], light.direction_attenuation[1], light.direction_attenuation[2])) / scale;
		for (int axis = 0; axis < 3; axis++) {
			light.direction_attenuation[axis] = float(direction[axis]);
		}
		if (local) {
			const Vector3 position = p_world_from_input.xform_inv(Vector3(light.position_range[0], light.position_range[1], light.position_range[2]) - p_origin) / scale_squared;
			for (int axis = 0; axis < 3; axis++) {
				if (!(Math::abs(position[axis]) <= real_t(1.0e10))) {
					return false;
				}
				light.position_range[axis] = float(position[axis]);
			}
			// Reciprocal range in the input vertices' units.
			light.position_range[3] = float(real_t(light.position_range[3]) * scale);
		}
	}
	r_lighting.camera_basis = Basis();
	r_lighting.shadow = FixedFunctionGraphicsDevice::FFDirectionalShadowParameters();
	return true;
}

Error FixedFunctionSceneRenderer::_lighting_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, FixedFunctionLightingVertex *r_vertices, Vector3 *r_model_normals, const Basis *p_normal_from_model) {
	FFSpatialVertex vertices[3];
	Vector3 normals[3];
	const Error error = _read_lighting_triangle(p_packet, p_data, p_indices, vertices, normals, r_model_normals, p_normal_from_model);
	if (error != OK) {
		return error;
	}
	for (int i = 0; i < 3; i++) {
		const Error pack_error = _pack_lighting_vertex(vertices[i], normals[i], r_vertices[i]);
		if (pack_error != OK) {
			return pack_error;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_pack_lighting_vertex(const FFSpatialVertex &p_vertex, const Vector3 &p_normal, FixedFunctionLightingVertex &r_vertex) {
	const real_t values[] = { p_vertex.position.x, p_vertex.position.y, p_vertex.position.z, p_vertex.uv.x, p_vertex.uv.y };
	for (int component = 0; component < 5; component++) {
		if (!(Math::abs(values[component]) <= real_t(1.0e10))) {
			return ERR_UNAVAILABLE;
		}
	}
	r_vertex.x = float(p_vertex.position.x);
	r_vertex.y = float(p_vertex.position.y);
	r_vertex.z = float(p_vertex.position.z);
	r_vertex.nx = float(p_normal.x);
	r_vertex.ny = float(p_normal.y);
	r_vertex.nz = float(p_normal.z);
	r_vertex.u = float(p_vertex.uv.x);
	r_vertex.v = float(p_vertex.uv.y);
	r_vertex.r = p_vertex.color.r;
	r_vertex.g = p_vertex.color.g;
	r_vertex.b = p_vertex.color.b;
	// Match baseline 8-bit vertex alpha before interpolation and modulation.
	r_vertex.a = Math::round(CLAMP(p_vertex.color.a, 0.0f, 1.0f) * 255.0f) / 255.0f;
	return OK;
}

Error FixedFunctionSceneRenderer::_lighting_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, FixedFunctionLightingVertex &r_vertex, Vector3 *r_model_normal, const Basis *p_normal_from_model) {
	const Vector3 model_normal = _vertex_normal(p_packet, p_data, p_index);
	const Basis &normal_from_model = p_normal_from_model ? *p_normal_from_model : p_packet.normal_from_model;
	const Vector3 normal = _unit_normal(normal_from_model.xform(model_normal));
	if (normal == Vector3()) {
		return ERR_UNAVAILABLE;
	}
	if (r_model_normal) {
		*r_model_normal = model_normal;
	}
	FFSpatialVertex vertex;
	const Error error = _read_vertex(p_packet, p_data, p_index, vertex);
	return error == OK ? _pack_lighting_vertex(vertex, normal, r_vertex) : error;
}
