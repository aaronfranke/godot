#include "drivers/fixed_function/mesh_storage.h"

#include <limits>

Basis FixedFunctionMeshStorage::_decode_surface_rotation(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, float *r_sign) {
	uint16_t axis_data[2], encoded_angle;
	memcpy(axis_data, p_data + p_surface.offsets[RSE::ARRAY_NORMAL] + p_index * p_surface.normal_stride, sizeof(axis_data));
	memcpy(&encoded_angle, p_data + p_surface.offsets[RSE::ARRAY_VERTEX] + p_index * p_surface.vertex_stride + 6, sizeof(encoded_angle));
	const Vector3 axis = Vector3::octahedron_decode(Vector2(real_t(axis_data[0]) / 65535, real_t(axis_data[1]) / 65535));
	const real_t encoded = real_t(encoded_angle) / 65535;
	if (r_sign) {
		*r_sign = encoded > real_t(0.5) ? 1 : -1;
	}
	return Basis(axis, Math::abs(encoded * 2 - 1) * Math::PI);
}

Vector4 FixedFunctionMeshStorage::surface_get_tangent(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index) {
	if (!(p_surface.source.format & RSE::ARRAY_FORMAT_TANGENT)) {
		return Vector4();
	}
	Vector3 tangent;
	float sign;
	if (p_surface.source.format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
		tangent = _decode_surface_rotation(p_surface, p_data, p_index, &sign).rows[0];
	} else {
		uint16_t values[2];
		memcpy(values, p_data + p_surface.offsets[RSE::ARRAY_TANGENT] + p_index * p_surface.normal_stride, sizeof(values));
		tangent = Vector3::octahedron_tangent_decode(Vector2(real_t(values[0]) / 65535, real_t(values[1]) / 65535), &sign);
	}
	return Vector4(tangent.x, tangent.y, tangent.z, sign);
}

Vector3 FixedFunctionMeshStorage::surface_get_normal(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, Vector4 *r_tangent) {
	const uint64_t format = p_surface.source.format;
	if (!(format & RSE::ARRAY_FORMAT_NORMAL)) {
		if (r_tangent) {
			*r_tangent = Vector4();
		}
		return Vector3();
	}
	if (format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) {
		// The compressed axis and angle encode both rows of the same rotation.
		float sign = 0;
		const bool has_tangent = format & RSE::ARRAY_FORMAT_TANGENT;
		const Basis rotation = _decode_surface_rotation(p_surface, p_data, p_index, r_tangent && has_tangent ? &sign : nullptr);
		if (r_tangent) {
			const Vector3 &tangent = rotation.rows[0];
			*r_tangent = has_tangent ? Vector4(tangent.x, tangent.y, tangent.z, sign) : Vector4();
		}
		return rotation.rows[2];
	}
	if (r_tangent) {
		*r_tangent = surface_get_tangent(p_surface, p_data, p_index);
	}
	uint16_t values[2];
	memcpy(values, p_data + p_surface.offsets[RSE::ARRAY_NORMAL] + p_index * p_surface.normal_stride, sizeof(values));
	return Vector3::octahedron_decode(Vector2(real_t(values[0]) / 65535, real_t(values[1]) / 65535));
}

RID FixedFunctionMeshStorage::skeleton_allocate() {
	return _skeleton_owner.allocate_rid();
}

void FixedFunctionMeshStorage::skeleton_initialize(RID p_skeleton) {
	_skeleton_owner.initialize_rid(p_skeleton);
	_skeletons.push_back(p_skeleton);
}

Error FixedFunctionMeshStorage::skeleton_allocate_data(RID p_skeleton, int p_count, bool p_2d) {
	FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
	if (!skeleton || p_count < 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (skeleton_get_bone_count(p_skeleton) == p_count && skeleton->use_2d == p_2d) {
		return OK;
	}
	Vector<Transform3D> bones;
	Vector<Transform2D> bones_2d;
	const Error error = p_2d ? bones_2d.resize_initialized(p_count) : bones.resize_initialized(p_count);
	if (error != OK) {
		return error;
	}
	skeleton->bones = bones;
	skeleton->bones_2d = bones_2d;
	skeleton->use_2d = p_2d;
	skeleton->dirty = true;
	skeleton->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_SKELETON_DATA);
	return OK;
}

int FixedFunctionMeshStorage::skeleton_get_bone_count(RID p_skeleton) const {
	const FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
	if (!skeleton) {
		return 0;
	}
	return skeleton->use_2d ? skeleton->bones_2d.size() : skeleton->bones.size();
}

Error FixedFunctionMeshStorage::skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform3D &p_transform) {
	FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
	if (!skeleton || skeleton->use_2d || p_bone < 0 || p_bone >= skeleton->bones.size() || !p_transform.is_finite()) {
		return ERR_INVALID_PARAMETER;
	}
	if (skeleton->bones[p_bone] != p_transform) {
		skeleton->bones.set(p_bone, p_transform);
		skeleton->dirty = true;
	}
	return OK;
}

Transform3D FixedFunctionMeshStorage::skeleton_bone_get_transform(RID p_skeleton, int p_bone) const {
	const FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
	ERR_FAIL_COND_V(!skeleton || skeleton->use_2d || p_bone < 0 || p_bone >= skeleton->bones.size(), Transform3D());
	return skeleton->bones[p_bone];
}

void FixedFunctionMeshStorage::skeleton_update_dependency(RID p_skeleton, DependencyTracker *p_tracker) {
	FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
	if (skeleton) {
		p_tracker->update_dependency(&skeleton->dependency);
	}
}

void FixedFunctionMeshStorage::skeleton_free(RID p_skeleton) {
	FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
	if (!skeleton || skeleton->freeing) {
		return;
	}
	skeleton->freeing = true;
	for (FFMeshInstance *instance : _instances) {
		if (instance->skeleton == p_skeleton) {
			instance->skeleton = RID();
		}
	}
	_skeletons.erase(p_skeleton);
	skeleton->dependency.deleted_notify(p_skeleton);
	_skeleton_owner.free(p_skeleton);
}

void FixedFunctionMeshStorage::_build_skinning_bounds(FFSurfaceData &r_surface) {
	const RenderingServerTypes::SurfaceData &source = r_surface.source;
	if (source.bone_aabbs.is_empty() || !(source.format & RSE::ARRAY_FORMAT_BONES) || (source.format & RSE::ARRAY_FLAG_USE_2D_VERTICES)) {
		return;
	}
	Vector<FFPositionBounds> bounds;
	if (bounds.resize_initialized(MIN(source.bone_aabbs.size(), int64_t(UINT16_MAX) + 1)) != OK) {
		return;
	}
	FFPositionBounds *bone_bounds = bounds.ptrw();
	const uint8_t *vertex_data = source.vertex_data.ptr();
	const uint8_t *skin_data = source.skin_data.ptr();
	const int influences = source.format & RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS ? 8 : 4;
	// Imported boxes describe source geometry. Build the culling boxes from the
	// decoded positions and quantized weights that the CPU skinning path uses.
	for (uint32_t i = 0; i < source.vertex_count; i++, skin_data += r_surface.skin_stride) {
		const Vector3 position = surface_get_position(r_surface, vertex_data, i);
		for (int j = 0; j < influences; j++) {
			uint16_t index, weight;
			memcpy(&index, skin_data + r_surface.offsets[RSE::ARRAY_BONES] + j * sizeof(index), sizeof(index));
			memcpy(&weight, skin_data + r_surface.offsets[RSE::ARRAY_WEIGHTS] + j * sizeof(weight), sizeof(weight));
			if (!weight) {
				continue;
			}
			if (index >= bounds.size()) {
				return; // Incomplete palette metadata uses exact posed bounds.
			}
			bone_bounds[index].expand_to(position);
		}
	}
	r_surface.skinning_bounds = bounds;
}

bool FixedFunctionMeshStorage::_merge_skinning_bounds(const FFSurfaceData &p_surface, const FFSkeleton &p_skeleton, AABB &r_bounds) {
	if (p_surface.skinning_bounds.is_empty() || p_skeleton.use_2d || p_skeleton.bones.is_empty()) {
		return false;
	}
	const Transform3D &to_skeleton = p_surface.source.mesh_to_skeleton_xform;
	const real_t determinant = to_skeleton.basis.determinant();
	if (determinant == 0 || !Math::is_finite(determinant)) {
		return false;
	}
	const Transform3D from_skeleton = to_skeleton.affine_inverse();
	if (!from_skeleton.is_finite()) {
		return false;
	}
	const auto magnitude = [](const Transform3D &p_transform) {
		double norm = 1.0;
		for (int axis = 0; axis < 3; axis++) {
			const Vector3 row = p_transform.basis[axis].abs();
			norm = MAX(norm, double(row.x) + row.y + row.z + Math::abs(double(p_transform.origin[axis])));
		}
		return norm;
	};

	// Nonnegative weights are normalized after missing bones are dropped, so the
	// posed vertex is inside the union of its individual bone-transformed boxes.
	// Keep decoded rest positions for zero-weight and nonfinite skinning fallbacks.
	const FFPositionBounds &rest = p_surface.position_bounds;
	AABB bounds(rest.minimum, rest.maximum - rest.minimum);
	double bone_magnitude = 1.0;
	const int count = MIN(p_surface.skinning_bounds.size(), p_skeleton.bones.size());
	for (int i = 0; i < count; i++) {
		const FFPositionBounds &bone_bounds = p_surface.skinning_bounds[i];
		if (!bone_bounds.valid) {
			continue;
		}
		const Transform3D &bone = p_skeleton.bones[i];
		const AABB box = (from_skeleton * bone * to_skeleton).xform(AABB(bone_bounds.minimum, bone_bounds.maximum - bone_bounds.minimum));
		if (!_valid_aabb(box)) {
			return false;
		}
		bounds.merge_with(box);
		bone_magnitude = MAX(bone_magnitude, magnitude(bone));
	}
	// Pad for rounding in the weighted accumulation, affine products, and box
	// endpoints, including cancellation when converting between spaces.
	const Vector3 extent = rest.minimum.abs().max(rest.maximum.abs());
	const double scale = MAX(1.0, double(extent[extent.max_axis_index()])) * magnitude(to_skeleton) * bone_magnitude * magnitude(from_skeleton);
	if (!Math::is_finite(scale) || scale > double(std::numeric_limits<real_t>::max()) / (8.0 * UINT16_MAX)) {
		return false;
	}
	const AABB merged = r_bounds.merge(bounds.grow(real_t(scale * (64 * std::numeric_limits<real_t>::epsilon()))));
	if (!_valid_aabb(merged)) {
		return false;
	}
	r_bounds = merged;
	return true;
}

Error FixedFunctionMeshStorage::mesh_instance_deform(RID p_instance, const FFSurfaceData &p_surface, Vector<FFDeformedVertex> &r_vertices, AABB &r_bounds, Vector<FFDeformedTangentFrame> *r_tangents, FFPositionBounds *r_position_bounds, bool p_positions_only) const {
	if (r_position_bounds) {
		*r_position_bounds = FFPositionBounds();
	}
	if (r_tangents) {
		r_tangents->clear();
	}
	const FFMeshInstance *instance = _get_instance(p_instance);
	const FFMesh *mesh = instance ? _get_mesh(instance->base) : nullptr;
	if (!mesh || mesh->free_pending) {
		return ERR_INVALID_PARAMETER;
	}
	const RenderingServerTypes::SurfaceData &source = p_surface.source;
	const FFSkeleton *skeleton = instance->skeleton.is_valid() ? _skeleton_owner.get_or_null(instance->skeleton) : nullptr;
	if (skeleton && skeleton->use_2d && (source.format & RSE::ARRAY_FLAG_USE_2D_VERTICES)) {
		return mesh_instance_deform_2d(p_instance, p_surface, r_vertices, r_bounds);
	}
	const bool skinning = skeleton && !skeleton->use_2d && !(source.format & RSE::ARRAY_FLAG_USE_2D_VERTICES) && !skeleton->bones.is_empty() && (source.format & RSE::ARRAY_FORMAT_BONES) &&
			source.mesh_to_skeleton_xform.basis.determinant() != 0;
	const int shape_count = MIN(instance->blend_weights.size(), source.vertex_data.is_empty() ? int64_t(0) : source.blend_shape_data.size() / source.vertex_data.size());
	LocalVector<int> active_shapes;
	for (int i = 0; i < shape_count; i++) {
		if (instance->blend_weights[i] != 0) {
			active_shapes.push_back(i);
		}
	}
	if (!skinning && active_shapes.is_empty()) {
		return OK;
	}
	const Error error = r_vertices.resize(source.vertex_count);
	if (error != OK) {
		return error;
	}
	FFDeformedTangentFrame *frames = nullptr;
	if (r_tangents && (source.format & RSE::ARRAY_FORMAT_TANGENT) && r_tangents->resize_initialized(source.vertex_count) == OK) {
		frames = r_tangents->ptrw();
	}
	const bool normalized = mesh->blend_shape_mode == RSE::BLEND_SHAPE_MODE_NORMALIZED;
	const bool use_normals = !p_positions_only || r_tangents;
	const int influences = source.format & RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS ? 8 : 4;
	const Transform3D from_skeleton = skinning ? source.mesh_to_skeleton_xform.affine_inverse() : Transform3D();
	FFDeformedVertex *vertices = r_vertices.ptrw();
	const uint8_t *base_data = source.vertex_data.ptr();
	for (uint32_t i = 0; i < source.vertex_count; i++) {
		Vector4 base_frame;
		const Vector3 base_position = surface_get_position(p_surface, base_data, i);
		const Vector3 base_normal = use_normals ? surface_get_normal(p_surface, base_data, i, frames ? &base_frame : nullptr) : Vector3();
		Vector3 position = base_position;
		Vector3 normal = base_normal;
		const Vector3 base_tangent(base_frame.x, base_frame.y, base_frame.z);
		Vector3 tangent = base_tangent;
		const real_t tangent_sign = base_frame.w;
		for (int shape : active_shapes) {
			const uint8_t *shape_data = source.blend_shape_data.ptr() + shape * source.vertex_data.size();
			const float weight = instance->blend_weights[shape];
			const Vector3 target_position = surface_get_position(p_surface, shape_data, i);
			position += (normalized ? target_position - base_position : target_position) * weight;
			Vector4 target_frame;
			if (use_normals) {
				const Vector3 target_normal = surface_get_normal(p_surface, shape_data, i, frames ? &target_frame : nullptr);
				normal += (normalized ? target_normal - base_normal : target_normal) * weight;
			}
			if (frames) {
				const Vector3 target_tangent(target_frame.x, target_frame.y, target_frame.z);
				// Match 4.x's skeleton shader: blend xyz and retain the base tangent sign.
				tangent += (normalized ? target_tangent - base_tangent : target_tangent) * weight;
			}
		}
		if (!position.is_finite()) {
			position = base_position;
		}
		if (frames && tangent.is_finite() && normal.is_finite() && Math::is_finite(tangent_sign) && tangent_sign != 0) {
			frames[i].tangent = tangent.normalized();
			frames[i].bitangent = normal.cross(tangent).normalized() * (tangent_sign < 0 ? -1 : 1);
		}
		if (skinning) {
			const uint8_t *skin_data = source.skin_data.ptr() + size_t(i) * p_surface.skin_stride;
			Basis weighted_basis;
			weighted_basis.set_zero();
			Vector3 weighted_origin;
			uint32_t total = 0;
			for (int j = 0; j < influences; j++) {
				uint16_t index, weight;
				memcpy(&index, skin_data + p_surface.offsets[RSE::ARRAY_BONES] + j * sizeof(index), sizeof(index));
				memcpy(&weight, skin_data + p_surface.offsets[RSE::ARRAY_WEIGHTS] + j * sizeof(weight), sizeof(weight));
				if (!weight || index >= skeleton->bones.size()) {
					continue;
				}
				const Transform3D &bone = skeleton->bones[index];
				weighted_basis = weighted_basis + bone.basis * weight;
				weighted_origin += bone.origin * weight;
				total += weight;
			}
			if (total) {
				const real_t inv_total = real_t(1) / total;
				const Transform3D weighted_bone(weighted_basis * inv_total, weighted_origin * inv_total);
				const Transform3D deform = from_skeleton * weighted_bone * source.mesh_to_skeleton_xform;
				const Vector3 skinned = deform.xform(position);
				if (skinned.is_finite()) {
					position = skinned;
					if (frames) {
						const Vector3 t = deform.basis.xform(frames[i].tangent);
						const Vector3 b = deform.basis.xform(frames[i].bitangent);
						frames[i].tangent = t.is_finite() ? t.normalized() : Vector3();
						frames[i].bitangent = b.is_finite() ? b.normalized() : Vector3();
					}
					if (use_normals) {
						const real_t determinant = deform.basis.determinant();
						normal = determinant != 0 && Math::is_finite(determinant) ? deform.basis.inverse().transposed().xform(normal) : Vector3();
					}
				}
			}
		}
		if (r_position_bounds) {
			r_position_bounds->expand_to(position);
		}
		vertices[i].position = position;
		vertices[i].normal = use_normals && normal.is_finite() ? normal.normalized() : Vector3();
		if (i == 0) {
			r_bounds = AABB(position, Vector3());
		} else {
			r_bounds.expand_to(position);
		}
	}
	return OK;
}
