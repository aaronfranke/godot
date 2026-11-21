#include "drivers/fixed_function/mesh_storage.h"

RID FixedFunctionMeshStorage::multimesh_allocate() {
	return _multimesh_owner.allocate_rid();
}

void FixedFunctionMeshStorage::multimesh_initialize(RID p_multimesh) {
	_multimesh_owner.initialize_rid(p_multimesh);
	_multimeshes.push_back(p_multimesh);
}

void FixedFunctionMeshStorage::multimesh_free(RID p_multimesh) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || multimesh->freeing) {
		return;
	}
	multimesh->freeing = true;
	_multimeshes.erase(p_multimesh);
	multimesh->dependency.deleted_notify(p_multimesh);
	_multimesh_owner.free(p_multimesh);
}

Error FixedFunctionMeshStorage::multimesh_allocate_data(RID p_multimesh, int p_count, bool p_colors, bool p_custom_data, bool p_transform_2d) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	const int stride = (p_transform_2d ? 8 : 12) + (p_colors ? 4 : 0) + (p_custom_data ? 4 : 0);
	if (!multimesh || p_count < 0 || p_count > INT32_MAX / stride) {
		return ERR_INVALID_PARAMETER;
	}
	Vector<float> buffer;
	const Error error = buffer.resize_initialized(int64_t(p_count) * stride);
	if (error != OK) {
		return error;
	}
	multimesh->buffer = buffer;
	multimesh->count = p_count;
	multimesh->visible = MIN(multimesh->visible, p_count);
	multimesh->stride = stride;
	multimesh->transform_2d = p_transform_2d;
	multimesh->colors = p_colors;
	multimesh->custom_data = p_custom_data;
	multimesh->mesh_dirty = true;
	multimesh->geometry_version++;
	multimesh->geometry_changed_frame = _frame_number;
	return OK;
}

Error FixedFunctionMeshStorage::multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || (p_mesh.is_valid() && !owns_mesh(p_mesh))) {
		return ERR_INVALID_PARAMETER;
	}
	if (multimesh->mesh != p_mesh) {
		multimesh->mesh = p_mesh;
		multimesh->mesh_dirty = true;
		multimesh->geometry_version++;
		multimesh->geometry_changed_frame = _frame_number;
	}
	return OK;
}

RID FixedFunctionMeshStorage::multimesh_get_mesh(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	return multimesh ? multimesh->mesh : RID();
}

int FixedFunctionMeshStorage::multimesh_get_instance_count(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	return multimesh ? multimesh->count : 0;
}

int FixedFunctionMeshStorage::multimesh_get_visible_instances(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	return multimesh ? multimesh->visible : 0;
}

int FixedFunctionMeshStorage::multimesh_get_visible_count(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh) {
		return 0;
	}
	return multimesh->visible < 0 ? multimesh->count : multimesh->visible;
}

uint64_t FixedFunctionMeshStorage::multimesh_get_geometry_version(RID p_multimesh) const {
	enum { GEOMETRY_SETTLE_FRAMES = 4 };
	const FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || multimesh->freeing || multimesh->transform_2d || _frame_number - multimesh->geometry_changed_frame <= GEOMETRY_SETTLE_FRAMES) {
		return 0;
	}
	return multimesh->geometry_version;
}

Error FixedFunctionMeshStorage::multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || p_visible < -1 || p_visible > multimesh->count) {
		return ERR_INVALID_PARAMETER;
	}
	if (multimesh->visible != p_visible) {
		multimesh->visible = p_visible;
		multimesh->geometry_version++;
		multimesh->geometry_changed_frame = _frame_number;
	}
	multimesh->bounds_dirty = true;
	return OK;
}

Error FixedFunctionMeshStorage::multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || p_buffer.size() != multimesh->buffer.size()) {
		return ERR_INVALID_PARAMETER;
	}
	for (float value : p_buffer) {
		if (!Math::is_finite(value)) {
			return ERR_INVALID_DATA;
		}
	}
	multimesh->buffer = p_buffer;
	multimesh->bounds_dirty = true;
	multimesh->geometry_version++;
	multimesh->geometry_changed_frame = _frame_number;
	return OK;
}

Vector<float> FixedFunctionMeshStorage::multimesh_get_buffer(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	return multimesh ? multimesh->buffer : Vector<float>();
}

const float *FixedFunctionMeshStorage::_multimesh_instance_data(RID p_multimesh, int p_index) const {
	const FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || p_index < 0 || p_index >= multimesh->count) {
		return nullptr;
	}
	return multimesh->buffer.ptr() + size_t(p_index) * multimesh->stride;
}

Transform3D FixedFunctionMeshStorage::_multimesh_transform(const float *p_data) {
	const Basis basis(p_data[0], p_data[1], p_data[2], p_data[4], p_data[5], p_data[6], p_data[8], p_data[9], p_data[10]);
	return Transform3D(basis, Vector3(p_data[3], p_data[7], p_data[11]));
}

Transform3D FixedFunctionMeshStorage::multimesh_get_transform(RID p_multimesh, int p_index) const {
	const float *data = _multimesh_instance_data(p_multimesh, p_index);
	if (!data) {
		return Transform3D();
	}
	if (_multimesh_owner.get_or_null(p_multimesh)->transform_2d) {
		const bool collapsed = data[0] == 0 && data[1] == 0 && data[4] == 0 && data[5] == 0;
		const Basis basis(data[0], data[1], 0, data[4], data[5], 0, 0, 0, collapsed ? 0 : 1);
		return Transform3D(basis, Vector3(data[3], data[7], 0));
	}
	return _multimesh_transform(data);
}

Error FixedFunctionMeshStorage::multimesh_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || multimesh->transform_2d || p_index < 0 || p_index >= multimesh->count || !p_transform.is_finite()) {
		return ERR_INVALID_PARAMETER;
	}
	float *data = multimesh->buffer.ptrw() + size_t(p_index) * multimesh->stride;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			data[i * 4 + j] = float(p_transform.basis.rows[i][j]);
		}
		data[i * 4 + 3] = float(p_transform.origin[i]);
	}
	multimesh->bounds_dirty = true;
	multimesh->geometry_version++;
	multimesh->geometry_changed_frame = _frame_number;
	return OK;
}

Transform2D FixedFunctionMeshStorage::multimesh_get_transform_2d(RID p_multimesh, int p_index) const {
	const float *data = _multimesh_instance_data(p_multimesh, p_index);
	if (!data || !_multimesh_owner.get_or_null(p_multimesh)->transform_2d) {
		return Transform2D();
	}
	return Transform2D(Vector2(data[0], data[4]), Vector2(data[1], data[5]), Vector2(data[3], data[7]));
}

Error FixedFunctionMeshStorage::multimesh_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || !multimesh->transform_2d || p_index < 0 || p_index >= multimesh->count || !p_transform.is_finite()) {
		return ERR_INVALID_PARAMETER;
	}
	const float values[] = { float(p_transform[0].x), float(p_transform[1].x), 0, float(p_transform[2].x), float(p_transform[0].y), float(p_transform[1].y), 0, float(p_transform[2].y) };
	for (float value : values) {
		if (!Math::is_finite(value)) {
			return ERR_INVALID_DATA;
		}
	}
	float *data = multimesh->buffer.ptrw() + size_t(p_index) * multimesh->stride;
	for (int i = 0; i < 8; i++) {
		data[i] = values[i];
	}
	multimesh->bounds_dirty = true;
	return OK;
}

Color FixedFunctionMeshStorage::multimesh_get_channel(RID p_multimesh, int p_index, bool p_custom_data) const {
	const FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	const float *data = _multimesh_instance_data(p_multimesh, p_index);
	if (!data || !(p_custom_data ? multimesh->custom_data : multimesh->colors)) {
		return p_custom_data ? Color(0, 0, 0, 0) : Color(1, 1, 1, 1);
	}
	data += (multimesh->transform_2d ? 8 : 12) + (p_custom_data && multimesh->colors ? 4 : 0);
	return Color(data[0], data[1], data[2], data[3]);
}

Error FixedFunctionMeshStorage::multimesh_set_channel(RID p_multimesh, int p_index, const Color &p_value, bool p_custom_data) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || p_index < 0 || p_index >= multimesh->count ||
			!(p_custom_data ? multimesh->custom_data : multimesh->colors)) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < 4; i++) {
		if (!Math::is_finite(p_value[i])) {
			return ERR_INVALID_DATA;
		}
	}
	const int offset = (multimesh->transform_2d ? 8 : 12) + (p_custom_data && multimesh->colors ? 4 : 0);
	float *data = multimesh->buffer.ptrw() + size_t(p_index) * multimesh->stride + offset;
	for (int i = 0; i < 4; i++) {
		data[i] = p_value[i];
	}
	return OK;
}

Error FixedFunctionMeshStorage::multimesh_set_custom_aabb(RID p_multimesh, const AABB &p_aabb) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || !_valid_aabb(p_aabb)) {
		return ERR_INVALID_PARAMETER;
	}
	multimesh->custom_aabb = p_aabb;
	multimesh->bounds_dirty = true;
	return OK;
}

AABB FixedFunctionMeshStorage::multimesh_get_custom_aabb(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	return multimesh ? multimesh->custom_aabb : AABB();
}

AABB FixedFunctionMeshStorage::multimesh_get_aabb(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	const int count = multimesh_get_visible_count(p_multimesh);
	if (!count || !multimesh->mesh.is_valid()) {
		return AABB();
	}
	if (multimesh->custom_aabb != AABB()) {
		return multimesh->custom_aabb;
	}
	AABB base;
	if (mesh_get_aabb(multimesh->mesh, RID(), base) != OK) {
		return AABB();
	}
	AABB bounds;
	bool has_bounds = false;
	for (int i = 0; i < count; i++) {
		const Transform3D transform = multimesh_get_transform(p_multimesh, i);
		if (transform.basis == Basis(0, 0, 0, 0, 0, 0, 0, 0, 0)) {
			continue;
		}
		const AABB transformed = transform.xform(base);
		if (has_bounds) {
			bounds.merge_with(transformed);
		} else {
			bounds = transformed;
			has_bounds = true;
		}
	}
	return bounds;
}

bool FixedFunctionMeshStorage::multimesh_get_geometry_bounds(RID p_multimesh, const FFSurfaceData &p_surface, FFGeometryBounds &r_bounds) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (!multimesh || !p_surface.position_bounds.valid) {
		return false;
	}
	if (multimesh->bounds_geometry_version != multimesh->geometry_version || multimesh->bounds_surface_identity != p_surface.identity || multimesh->bounds_surface_version != p_surface.version) {
		multimesh->bounds_geometry_version = multimesh->geometry_version;
		multimesh->bounds_surface_identity = p_surface.identity;
		multimesh->bounds_surface_version = p_surface.version;
		multimesh->geometry_bounds_valid = false;
		// Native light selection uses the authored AABB. CPU lighting uses the
		// decoded positions, which can extend beyond it after a region edit.
		const Vector3 minimum = p_surface.source.aabb.position.min(p_surface.position_bounds.minimum);
		const Vector3 maximum = p_surface.source.aabb.get_end().max(p_surface.position_bounds.maximum);
		const FFGeometryBounds source = { { minimum.x, minimum.y, minimum.z }, { maximum.x, maximum.y, maximum.z } };
		FFGeometryBounds bounds = {};
		bool has_bounds = false;
		const int count = multimesh_get_visible_count(p_multimesh);
		for (int i = 0; i < count; i++) {
			const Transform3D copy = multimesh_get_transform(p_multimesh, i);
			if (copy.basis == Basis(0, 0, 0, 0, 0, 0, 0, 0, 0)) {
				continue;
			}
			FFGeometryBounds copy_bounds = source;
			if (!copy.is_finite() || !copy_bounds.transform(copy)) {
				return false;
			}
			if (!has_bounds) {
				bounds = copy_bounds;
				has_bounds = true;
				continue;
			}
			for (int axis = 0; axis < 3; axis++) {
				bounds.minimum[axis] = MIN(bounds.minimum[axis], copy_bounds.minimum[axis]);
				bounds.maximum[axis] = MAX(bounds.maximum[axis], copy_bounds.maximum[axis]);
			}
		}
		multimesh->geometry_bounds = bounds;
		multimesh->geometry_bounds_valid = has_bounds;
	}
	if (!multimesh->geometry_bounds_valid) {
		return false;
	}
	r_bounds = multimesh->geometry_bounds;
	return true;
}

void FixedFunctionMeshStorage::multimesh_update_dependency(RID p_multimesh, DependencyTracker *p_tracker) {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	if (multimesh) {
		p_tracker->update_dependency(&multimesh->dependency);
		if (multimesh->mesh.is_valid()) {
			mesh_update_dependency(multimesh->mesh, p_tracker);
		}
	}
}

RendererMeshStorage::MultiMeshInterpolator *FixedFunctionMeshStorage::multimesh_get_interpolator(RID p_multimesh) const {
	FFMultiMesh *multimesh = _multimesh_owner.get_or_null(p_multimesh);
	return multimesh ? &multimesh->interpolator : nullptr;
}
