#include "scene_renderer.h"

#include <string.h>

Vector3 FixedFunctionSceneRenderer::_vertex_normal(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index) {
	if (!p_packet.deformed_vertices.empty()) {
		return p_packet.deformed_vertices[p_index].normal;
	}
	const FixedFunctionMeshStorage::FFSurfaceData &surface = p_packet.surface;
	// Packed normals are bounded and will be normalized after the normal transform.
	const bool packed = surface.immediate_vertices.empty() && (surface.format & VS::ARRAY_COMPRESS_NORMAL);
	return _read_normal(surface, p_data, p_index, !packed);
}

Vector2 FixedFunctionSceneRenderer::_vertex_uv(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, int p_uv_set) {
	return p_packet.morph_attributes.empty() ? FixedFunctionMeshStorage::surface_read_uv(p_packet.surface, p_data, p_index, p_uv_set) : p_packet.morph_attributes[p_index].uv[p_uv_set];
}

Error FixedFunctionSceneRenderer::_prepare_deformation(FFDrawPacket &r_packet, const Vector<Transform> &p_bones, const PoolVector<float> &p_weights, VS::BlendShapeMode p_mode, bool p_tangents) {
	struct FFActiveShape {
		PoolVector<uint8_t>::Read data;
		float weight;
	};
	const FixedFunctionMeshStorage::FFSurfaceData &surface = r_packet.surface;
	Vector<FFActiveShape> shapes;
	const PoolVector<float>::Read blend_weights = p_weights.read();
	const int shape_count = MIN(surface.blend_shapes.size(), p_weights.size());
	for (int i = 0; i < shape_count; i++) {
		const float weight = blend_weights[i];
		if (weight != 0 && !Math::is_nan(weight) && !Math::is_inf(weight)) {
			const FFActiveShape shape = { surface.blend_shapes[i].read(), weight };
			if (shapes.push_back(shape)) {
				return ERR_OUT_OF_MEMORY;
			}
		}
	}
	const uint32_t skin_format = VS::ARRAY_FORMAT_BONES | VS::ARRAY_FORMAT_WEIGHTS;
	const bool skin = !p_bones.empty() && (surface.format & skin_format) == skin_format;
	const bool morph = !shapes.empty();
	if (!skin && !morph) {
		return OK;
	}
	if (!_geometry_upload_fits(surface.vertex_count, sizeof(FFDeformedVertex))) {
		return ERR_OUT_OF_MEMORY;
	}
	Error error = r_packet.deformed_vertices.resize(surface.vertex_count);
	if (error != OK) {
		return error;
	}
	// Optional frames have their own storage: ordinary skinning and older backends
	// retain the existing position/normal cost. Allocation refusal keeps base lighting.
	if (p_tangents && (surface.format & VS::ARRAY_FORMAT_TANGENT) && _geometry_upload_fits(surface.vertex_count, sizeof(FFDeformedTangentFrame))) {
		if (r_packet.deformed_tangents.resize(surface.vertex_count) != OK) {
			r_packet.deformed_tangents.clear();
		}
	}
	const bool uv0 = morph && (surface.format & VS::ARRAY_FORMAT_TEX_UV);
	const bool uv1 = morph && (surface.format & VS::ARRAY_FORMAT_TEX_UV2);
	const bool color = morph && (surface.format & VS::ARRAY_FORMAT_COLOR) &&
			(r_packet.material.albedo_uses_vertex_color || r_packet.material.albedo_uses_vertex_alpha);
	if (uv0 || uv1 || color) {
		if (!_geometry_upload_fits(surface.vertex_count, sizeof(FFMorphAttributes))) {
			return ERR_OUT_OF_MEMORY;
		}
		error = r_packet.morph_attributes.resize(surface.vertex_count);
		if (error != OK) {
			return error;
		}
	}
	FFDeformedVertex *vertices = r_packet.deformed_vertices.ptrw();
	FFDeformedTangentFrame *frames = r_packet.deformed_tangents.ptrw();
	FFMorphAttributes *attributes = r_packet.morph_attributes.ptrw();
	const PoolVector<uint8_t>::Read source = surface.array.read();
	const bool shaded = !(r_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED);
	const bool normalized = p_mode == VS::BLEND_SHAPE_MODE_NORMALIZED;
	AABB bounds;
	for (int i = 0; i < surface.vertex_count; i++) {
		FFDeformedVertex &vertex = vertices[i];
		const Vector3 base_position = FixedFunctionMeshStorage::surface_read_position(surface, source.ptr(), i);
		vertex.position = base_position;
		if (shaded) {
			vertex.normal = _read_normal(surface, source.ptr(), i, !morph);
		}
		Vector3 tangent;
		float tangent_sign = 0;
		if (frames) {
			_read_tangent_data(surface, source.ptr(), i, tangent, tangent_sign);
		}
		const Vector3 tangent_offset = normalized ? tangent : Vector3();
		real_t skin_weights[VS::ARRAY_WEIGHTS_SIZE];
		if (skin) {
			_read_skin_weights(surface, source.ptr(), i, skin_weights);
		}
		const Vector3 normal_offset = normalized ? vertex.normal : Vector3();
		Vector2 uv_offset[2];
		Color color_offset(0, 0, 0, 0);
		real_t weight_offset[VS::ARRAY_WEIGHTS_SIZE] = {};
		if (attributes) {
			if (uv0) {
				attributes[i].uv[0] = FixedFunctionMeshStorage::surface_read_uv(surface, source.ptr(), i, 0);
			}
			if (uv1) {
				attributes[i].uv[1] = FixedFunctionMeshStorage::surface_read_uv(surface, source.ptr(), i, 1);
			}
			if (color) {
				attributes[i].color = FixedFunctionMeshStorage::surface_read_color(surface, source.ptr(), i);
			}
			if (normalized) {
				uv_offset[0] = attributes[i].uv[0];
				uv_offset[1] = attributes[i].uv[1];
				color_offset = attributes[i].color;
			}
		}
		if (skin && normalized && morph) {
			for (int j = 0; j < VS::ARRAY_WEIGHTS_SIZE; j++) {
				weight_offset[j] = skin_weights[j];
			}
		}
		const Vector3 position_offset = normalized ? base_position : Vector3();
		for (int j = 0; j < shapes.size(); j++) {
			const uint8_t *target = shapes[j].data.ptr();
			const float weight = shapes[j].weight;
			// Normalized blending is base + sum((target - base) * weight).
			// Relative targets already contain the deltas.
			vertex.position += (FixedFunctionMeshStorage::surface_read_position(surface, target, i) - position_offset) * weight;
			if (shaded) {
				vertex.normal += (_read_normal(surface, target, i, false) - normal_offset) * weight;
			}
			if (frames) {
				Vector3 target_tangent;
				float unused_sign;
				_read_tangent_data(surface, target, i, target_tangent, unused_sign);
				// Like GLES3, morph tangent.xyz and retain the base bitangent sign.
				tangent += (target_tangent - tangent_offset) * weight;
			}
			if (uv0) {
				attributes[i].uv[0] += (FixedFunctionMeshStorage::surface_read_uv(surface, target, i, 0) - uv_offset[0]) * weight;
			}
			if (uv1) {
				attributes[i].uv[1] += (FixedFunctionMeshStorage::surface_read_uv(surface, target, i, 1) - uv_offset[1]) * weight;
			}
			if (color) {
				attributes[i].color += (FixedFunctionMeshStorage::surface_read_color(surface, target, i) - color_offset) * weight;
			}
			if (skin) {
				real_t target_weights[VS::ARRAY_WEIGHTS_SIZE];
				_read_skin_weights(surface, target, i, target_weights);
				for (int k = 0; k < VS::ARRAY_WEIGHTS_SIZE; k++) {
					skin_weights[k] += (target_weights[k] - weight_offset[k]) * weight;
				}
			}
		}
		if (morph) {
			if (Math::is_nan(vertex.position.x) || Math::is_inf(vertex.position.x) ||
					Math::is_nan(vertex.position.y) || Math::is_inf(vertex.position.y) ||
					Math::is_nan(vertex.position.z) || Math::is_inf(vertex.position.z)) {
				vertex.position = base_position;
			}
			vertex.normal = _unit_normal(vertex.normal);
		}
		if (frames && !Math::is_nan(tangent_sign) && !Math::is_inf(tangent_sign) && tangent_sign != 0) {
			frames[i].tangent = _unit_normal(tangent);
			frames[i].bitangent = _unit_normal(vertex.normal.cross(frames[i].tangent)) * (tangent_sign < 0 ? -1 : 1);
		}
		if (skin) {
			_skin_vertex(surface, source.ptr(), i, p_bones, skin_weights, vertex, frames ? frames + i : nullptr);
		}
		if (i == 0) {
			bounds.position = vertex.position;
			r_packet.surface.position_min = vertex.position;
			r_packet.surface.position_max = vertex.position;
		} else {
			bounds.expand_to(vertex.position);
			for (int axis = 0; axis < 3; axis++) {
				r_packet.surface.position_min[axis] = MIN(r_packet.surface.position_min[axis], vertex.position[axis]);
				r_packet.surface.position_max[axis] = MAX(r_packet.surface.position_max[axis], vertex.position[axis]);
			}
		}
	}
	r_packet.surface.aabb = bounds;
	return OK;
}
