#include "scene_renderer.h"

#include <string.h>

void FixedFunctionSceneRenderer::_read_skin_weights(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, real_t *r_weights) {
	const uint8_t *weights = p_data + p_surface.offsets[VS::ARRAY_WEIGHTS] + p_index * p_surface.strides[VS::ARRAY_WEIGHTS];
	const bool packed = p_surface.format & VS::ARRAY_COMPRESS_WEIGHTS;
	for (int i = 0; i < VS::ARRAY_WEIGHTS_SIZE; i++) {
		if (packed) {
			uint16_t weight;
			memcpy(&weight, weights + i * sizeof(weight), sizeof(weight));
			r_weights[i] = real_t(weight) / 65535;
		} else {
			float weight;
			memcpy(&weight, weights + i * sizeof(weight), sizeof(weight));
			r_weights[i] = weight;
		}
	}
}

void FixedFunctionSceneRenderer::_skin_vertex(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, const Vector<Transform> &p_bones, const real_t *p_weights, FFDeformedVertex &r_vertex, FFDeformedTangentFrame *r_tangent) {
	const uint8_t *indices = p_data + p_surface.offsets[VS::ARRAY_BONES] + p_index * p_surface.strides[VS::ARRAY_BONES];
	const bool wide = p_surface.format & VS::ARRAY_FLAG_USE_16_BIT_BONES;
	const Transform *influences[VS::ARRAY_WEIGHTS_SIZE];
	real_t weights[VS::ARRAY_WEIGHTS_SIZE];
	int count = 0;
	double total = 0;
	for (int i = 0; i < VS::ARRAY_WEIGHTS_SIZE; i++) {
		uint16_t bone;
		if (wide) {
			memcpy(&bone, indices + i * sizeof(bone), sizeof(bone));
		} else {
			bone = indices[i];
		}
		const real_t weight = p_weights[i];
		if (bone < p_bones.size() && weight > 0 && !Math::is_inf(weight)) {
			influences[count] = p_bones.ptr() + bone;
			weights[count++] = weight;
			total += weight;
		}
	}
	// Empty or invalid influences leave the morphed vertex as it is.
	if (!count || Math::is_inf(total)) {
		return;
	}
	Transform skin = *influences[0];
	if (count > 1) {
		const real_t first_weight = real_t(weights[0] / total);
		skin.basis = skin.basis * first_weight;
		skin.origin *= first_weight;
		for (int i = 1; i < count; i++) {
			const real_t weight = real_t(weights[i] / total);
			skin.basis = skin.basis + influences[i]->basis * weight;
			skin.origin += influences[i]->origin * weight;
		}
	}
	const Vector3 position = skin.xform(r_vertex.position);
	if (!Math::is_nan(position.x) && !Math::is_inf(position.x) &&
			!Math::is_nan(position.y) && !Math::is_inf(position.y) &&
			!Math::is_nan(position.z) && !Math::is_inf(position.z)) {
		r_vertex.position = position;
		if (r_vertex.normal != Vector3()) {
			r_vertex.normal = _unit_normal(_normal_matrix(skin.basis).xform(r_vertex.normal));
		}
		if (r_tangent) {
			// Directions use the linear skin transform; normals use its inverse transpose.
			r_tangent->tangent = _unit_normal(skin.basis.xform(r_tangent->tangent));
			r_tangent->bitangent = _unit_normal(skin.basis.xform(r_tangent->bitangent));
		}
	}
}
