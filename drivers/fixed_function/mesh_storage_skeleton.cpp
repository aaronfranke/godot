#include "mesh_storage.h"

FixedFunctionMeshStorage::FFSkeleton *FixedFunctionMeshStorage::_get_skeleton(RID p_skeleton) const {
	return _skeleton_owner.owns(p_skeleton) ? _skeleton_owner.getptr(p_skeleton) : nullptr;
}

RID FixedFunctionMeshStorage::skeleton_create() {
	if (_clearing) {
		return RID();
	}
	FFSkeleton *skeleton = memnew(FFSkeleton);
	skeleton->self = _skeleton_owner.make_rid(skeleton);
	_skeletons.push_back(skeleton);
	return skeleton->self;
}

Error FixedFunctionMeshStorage::skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d) {
	FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	// Godot's widest packed mesh bone index is uint16_t.
	if (_clearing || !skeleton || p_bones < 0 || p_bones > 65536) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_2d == skeleton->use_2d && p_bones == skeleton_get_bone_count(p_skeleton)) {
		return OK;
	}
	Vector<Transform> bones;
	Vector<Transform2D> bones_2d;
	const Error error = p_2d ? bones_2d.resize(p_bones) : bones.resize(p_bones);
	if (error != OK) {
		return error;
	}
	skeleton->bones = bones;
	skeleton->bones_2d = bones_2d;
	skeleton->use_2d = p_2d;
	skeleton->revision++;
	skeleton->dirty = true;
	return OK;
}

int FixedFunctionMeshStorage::skeleton_get_bone_count(RID p_skeleton) const {
	const FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	if (!skeleton) {
		return 0;
	}
	return skeleton->use_2d ? skeleton->bones_2d.size() : skeleton->bones.size();
}

Error FixedFunctionMeshStorage::skeleton_set_bone_transform(RID p_skeleton, int p_bone, const Transform &p_transform) {
	FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	if (_clearing || !skeleton || skeleton->use_2d || p_bone < 0 || p_bone >= skeleton->bones.size()) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < 3; i++) {
		if (Math::is_nan(p_transform.origin[i]) || Math::is_inf(p_transform.origin[i])) {
			return ERR_INVALID_PARAMETER;
		}
		for (int j = 0; j < 3; j++) {
			if (Math::is_nan(p_transform.basis[i][j]) || Math::is_inf(p_transform.basis[i][j])) {
				return ERR_INVALID_PARAMETER;
			}
		}
	}
	if (skeleton->bones[p_bone] != p_transform) {
		skeleton->bones.write[p_bone] = p_transform;
		skeleton->revision++;
		skeleton->dirty = true;
	}
	return OK;
}

Transform FixedFunctionMeshStorage::skeleton_get_bone_transform(RID p_skeleton, int p_bone) const {
	const FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	ERR_FAIL_COND_V(!skeleton || skeleton->use_2d || p_bone < 0 || p_bone >= skeleton->bones.size(), Transform());
	return skeleton->bones[p_bone];
}

uint32_t FixedFunctionMeshStorage::skeleton_get_revision(RID p_skeleton) const {
	const FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	return skeleton ? skeleton->revision : 0;
}

Error FixedFunctionMeshStorage::skeleton_get_pose(RID p_skeleton, Vector<Transform> &r_bones) const {
	const FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	if (!skeleton) {
		return ERR_INVALID_PARAMETER;
	}
	if (skeleton->use_2d) {
		return ERR_UNAVAILABLE;
	}
	r_bones = skeleton->bones;
	return OK;
}

Error FixedFunctionMeshStorage::instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) {
	FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	if (_clearing || !skeleton || !p_instance || p_instance->skeleton != p_skeleton) {
		return ERR_INVALID_PARAMETER;
	}
	if (_skeleton_registrations.has(p_instance)) {
		return ERR_ALREADY_IN_USE;
	}
	if (_next_generation == UINT64_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	const FFRegistration registration = { ++_next_generation, p_skeleton };
	_skeleton_registrations[p_instance] = registration;
	skeleton->instances[p_instance] = registration.generation;
	return OK;
}

Error FixedFunctionMeshStorage::instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) {
	FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	const Map<RasterizerScene::InstanceBase *, FFRegistration>::Element *registration = _skeleton_registrations.find(p_instance);
	if (!skeleton || !registration || registration->get().owner != p_skeleton) {
		return ERR_INVALID_PARAMETER;
	}
	_skeleton_registrations.erase(p_instance);
	skeleton->instances.erase(p_instance);
	return OK;
}

AABB FixedFunctionMeshStorage::_skinned_surface_aabb(const FFSurfaceData &p_surface, const FFSkeleton &p_skeleton, const AABB &p_bounds) {
	AABB bounds = p_bounds;
	const uint32_t skin_format = VS::ARRAY_FORMAT_BONES | VS::ARRAY_FORMAT_WEIGHTS;
	if ((p_surface.format & skin_format) != skin_format) {
		return bounds;
	}
	// Positive normalized influences are bounded by the union of their transformed
	// boxes. Keep the bind bounds for unweighted vertices and skeleton detachment.
	const Vector<AABB> &bone_aabbs = p_surface.bone_aabbs;
	// Morphs can change positions and weights before skinning. Rest per-bone boxes
	// cannot bound all combinations, so use the full morph envelope for each bone.
	const bool use_bone_bounds = !bone_aabbs.empty() && p_surface.blend_shapes.empty();
	const int count = use_bone_bounds ? MIN(p_skeleton.bones.size(), bone_aabbs.size()) : p_skeleton.bones.size();
	for (int i = 0; i < count; i++) {
		const AABB &source = use_bone_bounds ? bone_aabbs[i] : p_bounds;
		if (source.size.x < 0) {
			continue;
		}
		const AABB posed = p_skeleton.bones[i].xform(source);
		if (_valid_aabb(posed)) {
			bounds.merge_with(posed);
		}
	}
	return bounds;
}

void FixedFunctionMeshStorage::_free_skeleton(FFSkeleton *p_skeleton) {
	_invalidate_skeleton_canvas_items(p_skeleton, true);
	for (Map<RasterizerScene::InstanceBase *, uint64_t>::Element *E = p_skeleton->instances.front(); E; E = E->next()) {
		E->key()->skeleton = RID();
		_skeleton_registrations.erase(E->key());
	}
	_skeleton_owner.free(p_skeleton->self);
	_skeletons.erase(p_skeleton);
	memdelete(p_skeleton);
}
