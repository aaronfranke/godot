#include "drivers/fixed_function/mesh_storage.h"

Error FixedFunctionMeshStorage::skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) {
	FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
	if (!skeleton || !skeleton->use_2d || p_bone < 0 || p_bone >= skeleton->bones_2d.size() || !p_transform.is_finite()) {
		return ERR_INVALID_PARAMETER;
	}
	if (skeleton->bones_2d[p_bone] != p_transform) {
		skeleton->bones_2d.set(p_bone, p_transform);
		skeleton->dirty = true;
	}
	return OK;
}

Transform2D FixedFunctionMeshStorage::skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const {
	const FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
	ERR_FAIL_COND_V(!skeleton || !skeleton->use_2d || p_bone < 0 || p_bone >= skeleton->bones_2d.size(), Transform2D());
	return skeleton->bones_2d[p_bone];
}

Error FixedFunctionMeshStorage::skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_transform) {
	FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
	if (!skeleton || !p_transform.is_finite()) {
		return ERR_INVALID_PARAMETER;
	}
	if (skeleton->base_transform_2d != p_transform) {
		skeleton->base_transform_2d = p_transform;
		skeleton->dirty = true;
	}
	return OK;
}

Error FixedFunctionMeshStorage::skeleton_get_pose_2d(RID p_skeleton, Vector<Transform2D> &r_bones, Transform2D &r_base) const {
	const FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
	if (!skeleton) {
		return ERR_INVALID_PARAMETER;
	}
	if (!skeleton->use_2d) {
		return ERR_UNAVAILABLE;
	}
	r_bones = skeleton->bones_2d;
	r_base = skeleton->base_transform_2d;
	return OK;
}

Error FixedFunctionMeshStorage::mesh_instance_deform_2d(RID p_instance, const FFSurfaceData &p_surface, Vector<FFDeformedVertex> &r_vertices, AABB &r_bounds, const Transform2D *p_item_transform) const {
	const FFMeshInstance *instance = _get_instance(p_instance);
	const FFSkeleton *skeleton = instance && instance->skeleton.is_valid() ? _skeleton_owner.get_or_null(instance->skeleton) : nullptr;
	const RenderingServerTypes::SurfaceData &source = p_surface.source;
	if (!skeleton || !skeleton->use_2d || skeleton->bones_2d.is_empty() || !(source.format & RSE::ARRAY_FLAG_USE_2D_VERTICES) || !(source.format & RSE::ARRAY_FORMAT_BONES)) {
		return OK;
	}
	// Culling uses the surface's bind mapping. Drawing uses the current item frame,
	// matching GLES when a CanvasItem moves without rebuilding its mesh surface.
	const Transform3D &bind = source.mesh_to_skeleton_xform;
	Transform2D into_skeleton(bind.basis.rows[0][0], bind.basis.rows[1][0], bind.basis.rows[0][1], bind.basis.rows[1][1], bind.origin.x, bind.origin.y);
	if (p_item_transform) {
		if (skeleton->base_transform_2d.determinant() == 0) {
			return OK;
		}
		into_skeleton = skeleton->base_transform_2d.affine_inverse() * *p_item_transform;
	}
	if (!into_skeleton.is_finite() || !Math::is_finite(into_skeleton.determinant()) || into_skeleton.determinant() == 0) {
		return OK;
	}
	const Transform2D from_skeleton = into_skeleton.affine_inverse();
	const Error error = r_vertices.resize(source.vertex_count);
	if (error != OK) {
		return error;
	}
	FFDeformedVertex *vertices = r_vertices.ptrw();
	const int influences = source.format & RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS ? 8 : 4;
	for (uint32_t i = 0; i < source.vertex_count; i++) {
		const Vector3 original = surface_get_position(p_surface, source.vertex_data.ptr(), i);
		const Vector2 point = into_skeleton.xform(Vector2(original.x, original.y));
		const uint8_t *skin = source.skin_data.ptr() + size_t(i) * p_surface.skin_stride;
		Vector2 weighted;
		uint32_t total = 0;
		for (int j = 0; j < influences; j++) {
			uint16_t index, weight;
			memcpy(&index, skin + p_surface.offsets[RSE::ARRAY_BONES] + j * sizeof(index), sizeof(index));
			memcpy(&weight, skin + p_surface.offsets[RSE::ARRAY_WEIGHTS] + j * sizeof(weight), sizeof(weight));
			if (weight && index < skeleton->bones_2d.size()) {
				weighted += skeleton->bones_2d[index].xform(point) * weight;
				total += weight;
			}
		}
		Vector3 position = original;
		if (total) {
			const Vector2 result = from_skeleton.xform(weighted / total);
			if (result.is_finite()) {
				position = Vector3(result.x, result.y, original.z);
			}
		}
		vertices[i].position = position;
		vertices[i].normal = Vector3();
		if (i == 0) {
			r_bounds = AABB(position, Vector3());
		} else {
			r_bounds.expand_to(position);
		}
	}
	return OK;
}
