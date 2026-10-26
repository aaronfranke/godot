#include "scene_renderer.h"

#include <string.h>
#include <limits>

Vector3 FixedFunctionSceneRenderer::_unit_normal(const Vector3 &p_normal) {
	const real_t length_squared = p_normal.length_squared();
	if (length_squared >= std::numeric_limits<real_t>::min() && length_squared <= std::numeric_limits<real_t>::max()) {
		return p_normal / Math::sqrt(length_squared);
	}
	// Rescale extreme finite vectors whose squared length overflows or underflows.
	real_t scale = 0;
	for (int i = 0; i < 3; i++) {
		if (Math::is_nan(p_normal[i]) || Math::is_inf(p_normal[i])) {
			return Vector3();
		}
		scale = MAX(scale, Math::abs(p_normal[i]));
	}
	if (scale == 0) {
		return Vector3();
	}
	// Normalize after scaling, avoiding overflow/underflow in length_squared().
	const Vector3 scaled = p_normal / scale;
	return scaled / scaled.length();
}

Basis FixedFunctionSceneRenderer::_normal_matrix(const Basis &p_basis) {
	real_t scale = 0;
	for (int column = 0; column < 3; column++) {
		for (int row = 0; row < 3; row++) {
			scale = MAX(scale, Math::abs(p_basis.get_axis(column)[row]));
		}
	}
	Basis result;
	const Vector3 a = scale ? p_basis.get_axis(0) / scale : Vector3();
	const Vector3 b = scale ? p_basis.get_axis(1) / scale : Vector3();
	const Vector3 c = scale ? p_basis.get_axis(2) / scale : Vector3();
	// Cofactors give inverse-transpose directions without dividing by a tiny
	// determinant. Correct reflection sign; rank-deficient transforms remain finite.
	const real_t sign = a.dot(b.cross(c)) < 0 ? -1 : 1;
	result.set_axis(0, b.cross(c) * sign);
	result.set_axis(1, c.cross(a) * sign);
	result.set_axis(2, a.cross(b) * sign);
	return result;
}

Vector3 FixedFunctionSceneRenderer::_read_normal(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, bool p_normalize) {
	if (!(p_surface.format & VS::ARRAY_FORMAT_NORMAL)) {
		return Vector3();
	}
	if (!p_surface.immediate_vertices.empty()) {
		const Vector3 &normal = p_surface.immediate_vertices[p_index].normal;
		return p_normalize ? _unit_normal(normal) : normal;
	}
	const uint8_t *source = p_data + p_surface.offsets[VS::ARRAY_NORMAL] + p_index * p_surface.strides[VS::ARRAY_NORMAL];
	if (p_surface.format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
		Vector2 encoded;
		if ((p_surface.format & VS::ARRAY_COMPRESS_NORMAL) && (p_surface.format & VS::ARRAY_FORMAT_TANGENT) && (p_surface.format & VS::ARRAY_COMPRESS_TANGENT)) {
			int8_t values[2];
			memcpy(values, source, sizeof(values));
			encoded = Vector2(MAX(real_t(values[0]) / 127, real_t(-1)), MAX(real_t(values[1]) / 127, real_t(-1)));
		} else {
			int16_t values[2];
			memcpy(values, source, sizeof(values));
			encoded = Vector2(MAX(real_t(values[0]) / 32767, real_t(-1)), MAX(real_t(values[1]) / 32767, real_t(-1)));
		}
		// The decoder already normalizes this bounded input.
		return VisualServer::oct_to_norm(encoded);
	}
	Vector3 normal;
	if (p_surface.format & VS::ARRAY_COMPRESS_NORMAL) {
		int8_t values[3];
		memcpy(values, source, sizeof(values));
		for (int i = 0; i < 3; i++) {
			normal[i] = MAX(real_t(values[i]) / 127, real_t(-1));
		}
	} else {
		float values[3];
		memcpy(values, source, sizeof(values));
		normal = Vector3(values[0], values[1], values[2]);
	}
	// Missing, zero and nonfinite normals use the same visible geometry fallback.
	return p_normalize ? _unit_normal(normal) : normal;
}

Error FixedFunctionSceneRenderer::_prepare_lighting(const Transform &p_camera, RID p_environment, const RID *p_lights, int p_light_count) {
	_camera_basis = p_camera.basis;
	if (p_environment.is_valid()) {
		Color color;
		float energy, sky_contribution;
		const Error error = _context.get_scene_storage().environment_get_ambient(p_environment, color, energy, sky_contribution);
		if (error != OK) {
			return error;
		}
		double sky_weight = CLAMP(sky_contribution, 0.0f, 1.0f);
		Color sky;
		float sky_energy = 0;
		if (sky_weight > 0) {
			Basis orientation;
			float fov;
			const RID panorama = _context.get_scene_storage().environment_get_panorama(p_environment, orientation, fov, sky_energy, false);
			if (!panorama.is_valid() || !_context.get_texture_storage().texture_get_panorama_average(panorama, sky)) {
				sky_weight = 0;
			}
		}
		for (int i = 0; i < 3; i++) {
			_ambient[i] = (double(color[i]) * (1 - sky_weight) + double(sky[i]) * sky_energy * sky_weight) * energy;
		}
	} else if (!_render_targets.is_render_target_transparent()) {
		// Godot 3 uses the viewport clear color as ambient when there is no Environment.
		const Color clear = _render_targets.get_clear_color();
		for (int i = 0; i < 3; i++) {
			_ambient[i] = clear[i];
		}
	}
	FixedFunctionLightStorage &storage = _context.get_light_storage();
	for (int i = 0; i < p_light_count; i++) {
		FixedFunctionLightStorage::FFLightInstanceData instance;
		Error error = storage.light_instance_get_data(p_lights[i], instance);
		if (error != OK) {
			return error;
		}
		if (!instance.light.is_valid()) {
			continue; // A released source leaves its renderer instance detached.
		}
		FixedFunctionLightStorage::FFLightData data;
		error = storage.light_get_data(instance.light, data);
		if (error != OK) {
			return error;
		}
		const double energy = double(data.parameters[VS::LIGHT_PARAM_ENERGY]) * ((data.flags & FixedFunctionLightStorage::LIGHT_NEGATIVE) ? -1 : 1);
		if (data.type == VS::LIGHT_DIRECTIONAL) {
			FFDirectionalLight light;
			// Godot lights emit along local -Z; Lambert needs the direction toward the light.
			light.direction = _unit_normal(instance.transform.basis.get_axis(2));
			if (light.direction == Vector3()) {
				continue;
			}
			for (int channel = 0; channel < 3; channel++) {
				light.radiance[channel] = double(data.color[channel]) * energy;
			}
			light.cull_mask = data.cull_mask;
			light.instance_id = p_lights[i].get_id();
			if (_directional_lights.push_back(light) != OK) {
				return ERR_OUT_OF_MEMORY;
			}
		} else {
			FFLocalLight light;
			light.range = data.parameters[VS::LIGHT_PARAM_RANGE];
			if (light.range <= 0) {
				continue;
			}
			// Keep positions camera-relative in world axes. Camera scale must not distort
			// world-space light ranges or normals; the native float boundary stays unchanged.
			light.position = instance.transform.origin - p_camera.origin;
			light.direction = _unit_normal(-instance.transform.basis.get_axis(2));
			light.spot = data.type == VS::LIGHT_SPOT;
			light.cos_spot_angle = Math::cos(Math::deg2rad(CLAMP(data.parameters[VS::LIGHT_PARAM_SPOT_ANGLE], 0.0f, 89.0f)));
			if (light.spot && (light.direction == Vector3() || light.cos_spot_angle >= 1)) {
				continue;
			}
			// Bound only the execution copy; retain arbitrary finite source metadata.
			light.attenuation = MAX(data.parameters[VS::LIGHT_PARAM_ATTENUATION], 0.0f);
			light.spot_attenuation = MAX(data.parameters[VS::LIGHT_PARAM_SPOT_ATTENUATION], 0.0f);
			// Nonlinear falloff and very narrow cones can amplify distance or direction rounding.
			light.direct_distance = light.attenuation == 1 && (!light.spot || (light.spot_attenuation == 1 && real_t(1) - light.cos_spot_angle >= 1024 * std::numeric_limits<real_t>::epsilon()));
			light.cull_mask = data.cull_mask;
			light.instance_id = p_lights[i].get_id();
			for (int channel = 0; channel < 3; channel++) {
				light.radiance[channel] = double(data.color[channel]) * energy;
			}
			if (_local_lights.push_back(light) != OK) {
				return ERR_OUT_OF_MEMORY;
			}
		}
	}
	return OK;
}

real_t FixedFunctionSceneRenderer::_local_light_diffuse(const FFLocalLight &p_light, const Vector3 &p_position, const Vector3 &p_normal) {
	const Vector3 relative = p_light.position - p_position;
	const real_t length_squared = p_light.direct_distance ? relative.length_squared() : real_t(0);
	Vector3 scaled = relative;
	real_t length;
	real_t normalized_distance;
	if (length_squared >= std::numeric_limits<real_t>::min() && length_squared <= std::numeric_limits<real_t>::max()) {
		length = Math::sqrt(length_squared);
		normalized_distance = length / p_light.range;
	} else {
		real_t scale = 0;
		for (int axis = 0; axis < 3; axis++) {
			if (Math::is_nan(relative[axis]) || Math::is_inf(relative[axis])) {
				return 0;
			}
			scale = MAX(scale, Math::abs(relative[axis]));
		}
		// Avoid undefined light directions at the source and overflow or underflow when squaring.
		if (scale == 0 || scale >= p_light.range) {
			return 0;
		}
		scaled = relative / scale;
		length = scaled.length();
		normalized_distance = (scale / p_light.range) * length;
	}
	if (normalized_distance >= 1) {
		return 0;
	}
	const Vector3 direction = scaled / length;
	real_t diffuse = MAX(p_normal.dot(direction), real_t(0));
	if (diffuse == 0) {
		return 0;
	}
	// GLES2's nonphysical Gouraud falloff, with an explicit finite range boundary.
	const real_t falloff = real_t(1) - normalized_distance;
	diffuse *= p_light.attenuation == 1 ? falloff : Math::pow(falloff, real_t(p_light.attenuation));
	if (p_light.spot) {
		const real_t angle = -direction.dot(p_light.direction);
		if (angle <= p_light.cos_spot_angle) {
			return 0;
		}
		const real_t rim = CLAMP((real_t(1) - angle) / (real_t(1) - p_light.cos_spot_angle), real_t(0), real_t(1));
		const real_t spot_falloff = p_light.spot_attenuation == 1 ? rim : Math::pow(rim, real_t(p_light.spot_attenuation));
		diffuse *= real_t(1) - spot_falloff;
	}
	return diffuse;
}

void FixedFunctionSceneRenderer::_filter_local_lights(FFDrawPacket &r_packet) const {
	if (r_packet.multimesh) {
		// Lit batches with local lights only use the per-pixel path. A refused
		// batch recomputes the CPU light list for each individual copy.
		r_packet.local_lights_filtered = true;
		return;
	}
	r_packet.local_lights_filtered = false;
	if (_local_lights.empty()) {
		return;
	}
	FixedFunctionMeshStorage::FFGeometryBounds bounds;
	for (int axis = 0; axis < 3; axis++) {
		bounds.minimum[axis] = r_packet.surface.position_min[axis];
		bounds.maximum[axis] = r_packet.surface.position_max[axis];
	}
	const bool use_bounds = r_packet.surface.identity && r_packet.surface.immediate_vertices.empty() &&
			bounds.transform(r_packet.view_from_model) && bounds.transform(Transform(_camera_basis, Vector3()));
	if (r_packet.local_light_indices.resize(_local_lights.size()) != OK) {
		return;
	}
	int *indices = r_packet.local_light_indices.ptrw();
	int count = 0;
	for (int i = 0; i < _local_lights.size(); i++) {
		const FFLocalLight &light = _local_lights[i];
		if (!(r_packet.layer_mask & light.cull_mask)) {
			continue;
		}
		bool outside = false;
		if (use_bounds) {
			const double distance_squared = bounds.range_distance_squared(light.position, light.range);
			outside = distance_squared > 1;
			if (!outside && light.spot) {
				const double range = double(light.range) * (1 + 32 * double(std::numeric_limits<real_t>::epsilon()));
				double projection_max = 0;
				double magnitude = 0;
				for (int axis = 0; axis < 3; axis++) {
					const double direction = light.direction[axis];
					const double edge = direction < 0 ? bounds.minimum[axis] : bounds.maximum[axis];
					const double component = ((edge - double(light.position[axis])) / range) * direction;
					projection_max += component;
					magnitude += Math::abs(component);
				}
				// Compare the largest axial projection with the smallest distance times cos(angle).
				// Leave slack for the vertex path's real_t subtraction, normalization and dot product.
				const double padding = 64 * double(std::numeric_limits<real_t>::epsilon()) * (1 + magnitude);
				outside = projection_max + padding < Math::sqrt(distance_squared) * double(light.cos_spot_angle);
			}
		}
		if (!outside) {
			indices[count++] = i;
		}
	}
	if (r_packet.local_light_indices.resize(count) == OK) {
		r_packet.local_lights_filtered = true;
	}
}

bool FixedFunctionSceneRenderer::_has_direct_lights(const FFDrawPacket &p_packet) const {
	const bool local_lights = p_packet.local_lights_filtered ? !p_packet.local_light_indices.empty() : !_local_lights.empty();
	if (local_lights) {
		return true;
	}
	for (int i = 0; i < _directional_lights.size(); i++) {
		if (p_packet.layer_mask & _directional_lights[i].cull_mask) {
			return true;
		}
	}
	return false;
}

void FixedFunctionSceneRenderer::_shade_vertex(const FFDrawPacket &p_packet, const Vector3 &p_normal, FFSpatialVertex &r_vertex, float p_occlusion) const {
	const float direct_scale = 1 + (p_occlusion - 1) * p_packet.material.occlusion.light_affect;
	double illumination[3];
	for (int i = 0; i < 3; i++) {
		illumination[i] = (p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_AMBIENT_DISABLED) ? 0 : _ambient[i] * p_occlusion;
	}
	for (int i = 0; i < _directional_lights.size(); i++) {
		const FFDirectionalLight &light = _directional_lights[i];
		if (!(p_packet.layer_mask & light.cull_mask)) {
			continue;
		}
		const real_t diffuse = MAX(p_normal.dot(light.direction), real_t(0)) * direct_scale;
		for (int channel = 0; channel < 3; channel++) {
			illumination[channel] += light.radiance[channel] * diffuse;
		}
	}
	const int local_count = p_packet.local_lights_filtered ? p_packet.local_light_indices.size() : _local_lights.size();
	if (local_count) {
		const Vector3 position = _camera_basis.xform(r_vertex.position);
		const int *indices = p_packet.local_light_indices.ptr();
		const FFLocalLight *lights = _local_lights.ptr();
		for (int i = 0; i < local_count; i++) {
			const FFLocalLight &light = lights[p_packet.local_lights_filtered ? indices[i] : i];
			if (!p_packet.local_lights_filtered && !(p_packet.layer_mask & light.cull_mask)) {
				continue;
			}
			const real_t diffuse = _local_light_diffuse(light, position, p_normal) * direct_scale;
			for (int channel = 0; channel < 3; channel++) {
				illumination[channel] += light.radiance[channel] * diffuse;
			}
		}
	}
	// Classic Gouraud Lambert in the existing LDR color space. Accumulate signed
	// lights before clamping this native draw copy; source colors/alpha stay intact.
	for (int i = 0; i < 3; i++) {
		r_vertex.color[i] = float(CLAMP(double(r_vertex.color[i]) * illumination[i], 0.0, 1.0));
	}
}

Error FixedFunctionSceneRenderer::_read_lighting_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, FFSpatialVertex *r_vertices, Vector3 *r_normals, Vector3 *r_model_normals, const Basis *p_normal_from_model) {
	const Basis &normal_from_model = p_normal_from_model ? *p_normal_from_model : p_packet.normal_from_model;
	Vector3 positions[3];
	bool needs_face = false;
	for (int i = 0; i < 3; i++) {
		const Error error = _read_vertex(p_packet, p_data, p_indices[i], r_vertices[i], &positions[i]);
		if (error != OK) {
			return error;
		}
		const Vector3 model_normal = _vertex_normal(p_packet, p_data, p_indices[i]);
		if (r_model_normals) {
			r_model_normals[i] = model_normal;
		}
		r_normals[i] = _unit_normal(normal_from_model.xform(model_normal));
		needs_face |= r_normals[i] == Vector3();
	}
	Vector3 face;
	if (needs_face) {
		// Godot front faces are clockwise. Normalize edges before crossing to avoid
		// overflow; derive in model space so mirrors match authored normal transforms.
		const Vector3 edge1 = _unit_normal(positions[1] - positions[0]);
		const Vector3 edge2 = _unit_normal(positions[2] - positions[0]);
		face = _unit_normal(normal_from_model.xform(edge2.cross(edge1)));
	}
	for (int i = 0; i < 3; i++) {
		if (r_normals[i] == Vector3()) {
			r_normals[i] = face;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_shade_and_pack_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, const Vector3 &p_normal, FFSpatialVertex &r_vertex, FixedFunctionVertex &r_packed, const FFOcclusionSample *p_occlusion) const {
	float occlusion = 1;
	if (p_occlusion) {
		const FixedFunctionSpatialMaterial::FFOcclusionChannel &channel = p_packet.material.occlusion;
		occlusion = _sample_occlusion(*p_occlusion, channel, _vertex_uv(p_packet, p_data, p_index, channel.uv_set));
	}
	_shade_vertex(p_packet, p_normal, r_vertex, occlusion);
	return _pack_vertex(r_vertex, r_packed);
}

Error FixedFunctionSceneRenderer::_pack_ambient_indexed_vertices(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_sources, uint32_t p_count, FixedFunctionVertex *r_vertices, const FFOcclusionSample *p_occlusion) const {
	for (uint32_t i = 0; i < p_count; i++) {
		const uint32_t index = p_sources ? p_sources[i] : i;
		FFSpatialVertex vertex;
		Error error = _read_vertex(p_packet, p_data, index, vertex);
		if (error != OK) {
			return error;
		}
		error = _shade_and_pack_vertex(p_packet, p_data, index, Vector3(), vertex, r_vertices[i], p_occlusion);
		if (error != OK) {
			return error;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_submit_indexed_lit_triangles(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint8_t *p_indices, const CameraMatrix &p_projection, RID p_texture, bool p_first_surface, const FFOcclusionSample *p_occlusion) {
	const int vertex_count = p_packet.surface.vertex_count;
	if (_indexed_lit_vertices.size() < vertex_count && _indexed_lit_vertices.resize(vertex_count) != OK) {
		return ERR_UNAVAILABLE;
	}
	FixedFunctionVertex *output = _indexed_lit_vertices.ptrw();
	if (_has_direct_lights(p_packet)) {
		for (int i = 0; i < vertex_count; i++) {
			const Vector3 normal = _unit_normal(p_packet.normal_from_model.xform(_vertex_normal(p_packet, p_data, i)));
			FFSpatialVertex vertex;
			if (normal == Vector3() || _read_vertex(p_packet, p_data, i, vertex) != OK ||
					_shade_and_pack_vertex(p_packet, p_data, i, normal, vertex, output[i], p_occlusion) != OK) {
				// Face normals depend on the triangle. Unreferenced vertices also
				// must not cause a draw failure. Retry through the corner path.
				return ERR_UNAVAILABLE;
			}
		}
	} else if (_pack_ambient_indexed_vertices(p_packet, p_data, nullptr, vertex_count, output, p_occlusion) != OK) {
		return ERR_UNAVAILABLE;
	}
	const uint32_t index_count = p_packet.surface.index_count / 3 * 3;
	FixedFunctionGraphicsDevice &device = _context.get_device();
	const uint32_t batch_indices = MIN(device.get_capabilities().max_primitives_per_draw, index_count / 3) * 3;
	bool first_surface = p_first_surface;
	for (uint32_t offset = 0; offset < index_count;) {
		const uint32_t amount = MIN(index_count - offset, batch_indices);
		const Error error = device.draw_indexed_triangles(output, vertex_count, p_indices + offset * sizeof(uint16_t), amount, p_projection, p_texture, p_packet.material.state);
		if (error != OK) {
			// A late refusal cannot restart the packet after an earlier batch drew.
			return error == ERR_UNAVAILABLE && offset > 0 ? FAILED : error;
		}
		_context.get_storage_queries().note_scene_draw(amount, !_instances_drawn[p_packet.instance_index], first_surface);
		_instances_drawn.write[p_packet.instance_index] = 1;
		first_surface = false;
		offset += amount;
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_lit_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, FixedFunctionVertex *r_vertices, const FFOcclusionSample *p_occlusion) const {
	FFSpatialVertex vertices[3];
	Vector3 normals[3];
	const Error error = _read_lighting_triangle(p_packet, p_data, p_indices, vertices, normals);
	if (error != OK) {
		return error;
	}
	for (int i = 0; i < 3; i++) {
		const Error pack_error = _shade_and_pack_vertex(p_packet, p_data, p_indices[i], normals[i], vertices[i], r_vertices[i], p_occlusion);
		if (pack_error != OK) {
			return pack_error;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_cached_lit_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, FixedFunctionVertex *r_vertices, const FFOcclusionSample *p_occlusion, FFLitVertexCacheEntry *p_cache, uint32_t p_cache_mask) const {
	for (int i = 0; i < 3; i++) {
		const uint32_t index = p_indices[i];
		FFLitVertexCacheEntry &entry = p_cache[index & p_cache_mask];
		if (entry.index != index) {
			const Vector3 normal = _unit_normal(p_packet.normal_from_model.xform(_vertex_normal(p_packet, p_data, index)));
			if (normal == Vector3()) {
				// A synthesized normal depends on the face. Never cache that result by index.
				return _lit_triangle(p_packet, p_data, p_indices, r_vertices, p_occlusion);
			}
			FFSpatialVertex vertex;
			Error error = _read_vertex(p_packet, p_data, index, vertex);
			if (error != OK) {
				return error;
			}
			error = _shade_and_pack_vertex(p_packet, p_data, index, normal, vertex, entry.vertex, p_occlusion);
			if (error != OK) {
				return error;
			}
			entry.index = index;
		}
		r_vertices[i] = entry.vertex;
	}
	return OK;
}
