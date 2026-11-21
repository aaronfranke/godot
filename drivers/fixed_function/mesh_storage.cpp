#include "mesh_storage.h"

#include "servers/rendering/rendering_server.h"

#include <string.h>

#include <limits>

FixedFunctionMeshStorage::FFSurfaceData::FFSurfaceData() : identity(0), version(0), vertex_stride(0), normal_stride(0), attribute_stride(0), skin_stride(0) {
	memset(offsets, 0, sizeof(offsets));
}

FixedFunctionMeshStorage::FFMesh::FFMesh() : blend_shape_count(0), blend_shape_mode(RSE::BLEND_SHAPE_MODE_NORMALIZED), aabb_dirty(false), materials_dirty(false), free_pending(false) {}

FixedFunctionMeshStorage::FFMesh *FixedFunctionMeshStorage::_get_mesh(RID p_mesh) const {
	return _mesh_owner.owns(p_mesh) ? _mesh_owner.get_or_null(p_mesh) : nullptr;
}

FixedFunctionMeshStorage::FFSurfaceData *FixedFunctionMeshStorage::_get_surface(RID p_mesh, int p_surface) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh && !mesh->free_pending && p_surface >= 0 && p_surface < mesh->surfaces.size() ? mesh->surfaces[p_surface] : nullptr;
}

FixedFunctionMeshStorage::FFMeshInstance *FixedFunctionMeshStorage::_get_instance(RID p_instance) const {
	return _instance_owner.owns(p_instance) ? _instance_owner.get_or_null(p_instance) : nullptr;
}

bool FixedFunctionMeshStorage::_valid_aabb(const AABB &p_aabb, bool p_allow_unused) {
	for (int i = 0; i < 3; i++) {
		if (!Math::is_finite(p_aabb.position[i]) || !Math::is_finite(p_aabb.size[i]) || (!p_allow_unused && p_aabb.size[i] < 0) || !Math::is_finite(p_aabb.position[i] + p_aabb.size[i])) {
			return false;
		}
	}
	return true;
}

Error FixedFunctionMeshStorage::_make_layout(FFSurfaceData &r_data, int p_blend_shape_count) {
	const RenderingServerTypes::SurfaceData &data = r_data.source;
	const uint64_t version_mask = uint64_t(RSE::ARRAY_FLAG_FORMAT_VERSION_MASK) << RSE::ARRAY_FLAG_FORMAT_VERSION_SHIFT;
	const uint64_t known_flags = ((uint64_t(1) << RSE::ARRAY_COMPRESS_FLAGS_BASE) - 1) | RSE::ARRAY_FLAG_USE_2D_VERTICES | RSE::ARRAY_FLAG_USE_DYNAMIC_UPDATE | RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS | RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES | RSE::ARRAY_FLAG_USE_STORAGE_BUFFER | version_mask;
	if ((data.format & ~known_flags) || (data.format & version_mask) != RSE::ARRAY_FLAG_FORMAT_CURRENT_VERSION || !(data.format & RSE::ARRAY_FORMAT_VERTEX) || !data.vertex_count || data.vertex_count > INT32_MAX || data.index_count > INT32_MAX || bool(data.format & RSE::ARRAY_FORMAT_INDEX) != bool(data.index_count) || data.primitive < 0 || data.primitive >= RSE::PRIMITIVE_MAX || bool(data.format & RSE::ARRAY_FORMAT_BONES) != bool(data.format & RSE::ARRAY_FORMAT_WEIGHTS) || ((data.format & RSE::ARRAY_FORMAT_TANGENT) && !(data.format & RSE::ARRAY_FORMAT_NORMAL))) {
		return ERR_INVALID_PARAMETER;
	}
	RenderingServer *server = RenderingServer::get_singleton();
	if (!server) {
		return ERR_UNCONFIGURED;
	}
	// Ask for one vertex first: no upstream uint32 offset multiplication can overflow.
	server->mesh_surface_make_offsets_from_format(data.format, 1, data.index_count ? 1 : 0, r_data.offsets, r_data.vertex_stride, r_data.normal_stride, r_data.attribute_stride, r_data.skin_stride);
	const uint64_t vertex_bytes = uint64_t(data.vertex_count) * (r_data.vertex_stride + r_data.normal_stride);
	const uint64_t attribute_bytes = uint64_t(data.vertex_count) * r_data.attribute_stride;
	const uint64_t skin_bytes = uint64_t(data.vertex_count) * r_data.skin_stride;
	const uint64_t index_bytes = uint64_t(data.index_count) * (data.vertex_count <= 65536 ? 2 : 4);
	const uint64_t blend_bytes = vertex_bytes * uint64_t(p_blend_shape_count);
	if (vertex_bytes > INT32_MAX || attribute_bytes > INT32_MAX || skin_bytes > INT32_MAX || index_bytes > INT32_MAX || blend_bytes > INT32_MAX || vertex_bytes != uint64_t(data.vertex_data.size()) || attribute_bytes != uint64_t(data.attribute_data.size()) || skin_bytes != uint64_t(data.skin_data.size()) || index_bytes != uint64_t(data.index_data.size()) || blend_bytes != uint64_t(data.blend_shape_data.size())) {
		return ERR_INVALID_PARAMETER;
	}
	server->mesh_surface_make_offsets_from_format(data.format, data.vertex_count, data.index_count, r_data.offsets, r_data.vertex_stride, r_data.normal_stride, r_data.attribute_stride, r_data.skin_stride);
	return OK;
}

Vector3 FixedFunctionMeshStorage::surface_get_position(const FFSurfaceData &p_surface, const uint8_t *p_vertices, uint32_t p_index) {
	const bool is_2d = p_surface.source.format & RSE::ARRAY_FLAG_USE_2D_VERTICES;
	const bool compressed = !is_2d && (p_surface.source.format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES);
	const uint8_t *source = p_vertices + p_surface.offsets[RSE::ARRAY_VERTEX] + p_index * p_surface.vertex_stride;
	Vector3 position;
	for (int i = 0; i < (is_2d ? 2 : 3); i++) {
		if (compressed) {
			uint16_t value;
			memcpy(&value, source + i * 2, 2);
			position[i] = real_t(value) / 65535 * p_surface.source.aabb.size[i] + p_surface.source.aabb.position[i];
		} else {
			float value;
			memcpy(&value, source + i * 4, 4);
			position[i] = value;
		}
	}
	return position;
}

void FixedFunctionMeshStorage::FFPositionBounds::expand_to(const Vector3 &p_position) {
	if (!valid) {
		minimum = maximum = p_position;
		valid = true;
	} else {
		minimum = minimum.min(p_position);
		maximum = maximum.max(p_position);
	}
}

bool FixedFunctionMeshStorage::FFGeometryBounds::transform(const Transform3D &p_transform) {
	FFGeometryBounds result;
	for (int row = 0; row < 3; row++) {
		double lo = p_transform.origin[row];
		double hi = lo;
		double magnitude = Math::abs(lo);
		for (int column = 0; column < 3; column++) {
			const double a = double(p_transform.basis[row][column]) * minimum[column];
			const double b = double(p_transform.basis[row][column]) * maximum[column];
			lo += MIN(a, b);
			hi += MAX(a, b);
			magnitude += MAX(Math::abs(a), Math::abs(b));
		}
		if (!(magnitude <= std::numeric_limits<real_t>::max())) {
			return false;
		}
		// Round outwards for the real_t dot product and addition, including
		// cancellation and subnormal values, before applying the next transform.
		const double padding = 8 * (magnitude * std::numeric_limits<real_t>::epsilon() + std::numeric_limits<real_t>::min());
		result.minimum[row] = lo - padding;
		result.maximum[row] = hi + padding;
	}
	*this = result;
	return true;
}

double FixedFunctionMeshStorage::FFGeometryBounds::range_distance_squared(const Vector3 &p_position, real_t p_range) const {
	// Leave slack for subtraction, scaling and length in the real_t lighting path.
	const double range = double(p_range) * (1 + 32 * double(std::numeric_limits<real_t>::epsilon()));
	double distance_squared = 0;
	for (int axis = 0; axis < 3; axis++) {
		const double position = p_position[axis];
		const double distance = MAX(0.0, MAX(minimum[axis] - position, position - maximum[axis])) / range;
		distance_squared += distance * distance;
	}
	return distance_squared;
}

Error FixedFunctionMeshStorage::_validate_positions(const FFSurfaceData &p_data, const Vector<uint8_t> &p_vertices, uint64_t p_offset, FFPositionBounds *r_bounds) {
	FFPositionBounds bounds;
	const uint8_t *vertices = p_vertices.ptr();
	for (uint32_t i = 0; i < p_data.source.vertex_count; i++) {
		const Vector3 position = surface_get_position(p_data, vertices + p_offset, i);
		if (!position.is_finite()) {
			return ERR_INVALID_DATA;
		}
		if (r_bounds) {
			bounds.expand_to(position);
		}
	}
	if (r_bounds) {
		*r_bounds = bounds;
	}
	return OK;
}

Error FixedFunctionMeshStorage::_validate_indices(const FFSurfaceData &p_data, const Vector<uint8_t> &p_indices) {
	const uint32_t stride = p_data.source.vertex_count <= 65536 ? 2 : 4;
	if (p_indices.size() % stride) {
		return ERR_INVALID_PARAMETER;
	}
	for (int64_t i = 0; i < p_indices.size(); i += stride) {
		uint32_t index = 0;
		memcpy(&index, p_indices.ptr() + i, stride);
		if (index >= p_data.source.vertex_count) {
			return ERR_INVALID_DATA;
		}
	}
	return OK;
}

RID FixedFunctionMeshStorage::mesh_create() {
	if (_clearing || _clear_pending) {
		return RID();
	}
	FFMesh *mesh = memnew(FFMesh);
	mesh->self = _mesh_owner.make_rid(mesh);
	_meshes.push_back(mesh);
	return mesh->self;
}

Error FixedFunctionMeshStorage::mesh_add_surface(RID p_mesh, const RenderingServerTypes::SurfaceData &p_surface) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->free_pending || _clearing || _clear_pending || !_valid_aabb(p_surface.aabb) || (p_surface.material.is_valid() && !_materials.owns_material(p_surface.material))) {
		return ERR_INVALID_PARAMETER;
	}
	FFSurfaceData candidate;
	candidate.source = p_surface;
	Error error = _make_layout(candidate, mesh->blend_shape_count);
	if (error == OK) {
		error = _validate_positions(candidate, candidate.source.vertex_data, 0, &candidate.position_bounds);
	}
	if (error == OK) {
		error = _validate_indices(candidate, candidate.source.index_data);
	}
	for (int i = 0; error == OK && i < mesh->blend_shape_count; i++) {
		error = _validate_positions(candidate, candidate.source.blend_shape_data, uint64_t(i) * candidate.source.vertex_data.size());
	}
	for (int i = 0; error == OK && i < p_surface.lods.size(); i++) {
		if (!Math::is_finite(p_surface.lods[i].edge_length) || p_surface.lods[i].edge_length < 0 || p_surface.lods[i].index_data.is_empty()) {
			error = ERR_INVALID_PARAMETER;
		} else {
			error = _validate_indices(candidate, p_surface.lods[i].index_data);
		}
	}
	for (int i = 0; error == OK && i < p_surface.bone_aabbs.size(); i++) {
		if (!_valid_aabb(p_surface.bone_aabbs[i], true)) {
			error = ERR_INVALID_PARAMETER;
		}
	}
	if (!p_surface.mesh_to_skeleton_xform.is_finite() || !p_surface.uv_scale.is_finite()) {
		error = ERR_INVALID_PARAMETER;
	}
	if (error != OK) {
		return error;
	}
	if (_next_surface_identity == UINT64_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	candidate.identity = ++_next_surface_identity;
	candidate.version = 1;
	_build_skinning_bounds(candidate);
	FFSurfaceData *surface = memnew(FFSurfaceData(candidate));
	if (mesh->surfaces.push_back(surface)) {
		memdelete(surface);
		return ERR_OUT_OF_MEMORY;
	}
	if (surface->source.material.is_valid()) {
		error = _materials.material_add_geometry_owner(&surface->source.material, &mesh->materials_dirty);
		if (error != OK) {
			mesh->surfaces.remove_at(mesh->surfaces.size() - 1);
			memdelete(surface);
			return error;
		}
	}
	_surfaces[surface->identity] = surface;
	mesh->aabb_dirty = mesh->materials_dirty = true;
	return OK;
}

Error FixedFunctionMeshStorage::mesh_set_blend_shape_count(RID p_mesh, int p_count) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->free_pending || _clearing || _clear_pending || p_count < 0 || !mesh->surfaces.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	mesh->blend_shape_count = p_count;
	mesh->materials_dirty = true;
	return OK;
}

int FixedFunctionMeshStorage::mesh_get_blend_shape_count(RID p_mesh) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh ? mesh->blend_shape_count : 0;
}

Error FixedFunctionMeshStorage::mesh_set_blend_shape_mode(RID p_mesh, RSE::BlendShapeMode p_mode) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->free_pending || _clearing || _clear_pending || (p_mode != RSE::BLEND_SHAPE_MODE_NORMALIZED && p_mode != RSE::BLEND_SHAPE_MODE_RELATIVE)) {
		return ERR_INVALID_PARAMETER;
	}
	mesh->blend_shape_mode = p_mode;
	return OK;
}

RSE::BlendShapeMode FixedFunctionMeshStorage::mesh_get_blend_shape_mode(RID p_mesh) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh ? mesh->blend_shape_mode : RSE::BLEND_SHAPE_MODE_NORMALIZED;
}

Error FixedFunctionMeshStorage::mesh_surface_update_region(RID p_mesh, int p_surface, FFBuffer p_buffer, int p_offset, const Vector<uint8_t> &p_data) {
	FFSurfaceData *surface = _get_surface(p_mesh, p_surface);
	if (!surface || _clearing || _clear_pending || p_offset < 0) {
		return ERR_INVALID_PARAMETER;
	}
	Vector<uint8_t> *destination = nullptr;
	switch (p_buffer) {
		case BUFFER_VERTEX:
			destination = &surface->source.vertex_data;
			break;
		case BUFFER_ATTRIBUTE:
			destination = &surface->source.attribute_data;
			break;
		case BUFFER_SKIN:
			destination = &surface->source.skin_data;
			break;
		case BUFFER_INDEX:
			destination = &surface->source.index_data;
			break;
		default:
			return ERR_INVALID_PARAMETER;
	}
	if (uint64_t(p_offset) + uint64_t(p_data.size()) > uint64_t(destination->size())) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_data.is_empty()) {
		return OK;
	}
	if (surface->version == UINT64_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	Vector<uint8_t> candidate = *destination;
	memcpy(candidate.ptrw() + p_offset, p_data.ptr(), p_data.size());
	Error error = OK;
	if (p_buffer == BUFFER_VERTEX) {
		error = _validate_positions(*surface, candidate, 0, &surface->position_bounds);
	} else if (p_buffer == BUFFER_INDEX) {
		error = _validate_indices(*surface, candidate);
	}
	if (error == OK) {
		// Region edits retain caller-supplied bounds, matching the other renderers.
		*destination = candidate;
		surface->version++;
		if (p_buffer == BUFFER_VERTEX || p_buffer == BUFFER_SKIN) {
			// Edited positions or weights use exact deformation bounds.
			surface->skinning_bounds.clear();
		}
	}
	return error;
}

Error FixedFunctionMeshStorage::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) {
	FFSurfaceData *surface = _get_surface(p_mesh, p_surface);
	if (!surface || _clearing || _clear_pending || (p_material.is_valid() && !_materials.owns_material(p_material))) {
		return ERR_INVALID_PARAMETER;
	}
	if (surface->source.material == p_material) {
		return OK;
	}
	_materials.material_remove_geometry_owner(&surface->source.material);
	surface->source.material = p_material;
	FFMesh *mesh = _get_mesh(p_mesh);
	mesh->materials_dirty = true;
	return p_material.is_valid() ? _materials.material_add_geometry_owner(&surface->source.material, &mesh->materials_dirty) : OK;
}

RID FixedFunctionMeshStorage::mesh_surface_get_material(RID p_mesh, int p_surface) const {
	const FFSurfaceData *surface = _get_surface(p_mesh, p_surface);
	return surface ? surface->source.material : RID();
}

Error FixedFunctionMeshStorage::mesh_surface_get_data(RID p_mesh, int p_surface, FFSurfaceData &r_data) const {
	const FFSurfaceData *surface = _get_surface(p_mesh, p_surface);
	if (!surface) {
		return ERR_INVALID_PARAMETER;
	}
	r_data = *surface;
	return OK;
}

void FixedFunctionMeshStorage::_delete_surface(FFSurfaceData *p_surface) {
	_surfaces.erase(p_surface->identity);
	_materials.material_remove_geometry_owner(&p_surface->source.material);
	memdelete(p_surface);
}

Error FixedFunctionMeshStorage::mesh_remove_surface(RID p_mesh, int p_surface) {
	FFMesh *mesh = _get_mesh(p_mesh);
	FFSurfaceData *surface = _get_surface(p_mesh, p_surface);
	if (!surface || _clearing || _clear_pending) {
		return ERR_INVALID_PARAMETER;
	}
	mesh->surfaces.remove_at(p_surface);
	_delete_surface(surface);
	mesh->aabb_dirty = mesh->materials_dirty = true;
	return OK;
}

int FixedFunctionMeshStorage::mesh_get_surface_count(RID p_mesh) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh && !mesh->free_pending ? mesh->surfaces.size() : 0;
}

Error FixedFunctionMeshStorage::mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->free_pending || _clearing || _clear_pending || !_valid_aabb(p_aabb)) {
		return ERR_INVALID_PARAMETER;
	}
	mesh->custom_aabb = p_aabb;
	mesh->aabb_dirty = true;
	return OK;
}

AABB FixedFunctionMeshStorage::mesh_get_custom_aabb(RID p_mesh) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh ? mesh->custom_aabb : AABB();
}

Error FixedFunctionMeshStorage::mesh_get_aabb(RID p_mesh, RID p_skeleton, AABB &r_aabb) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->free_pending) {
		return ERR_INVALID_PARAMETER;
	}
	if (mesh->custom_aabb != AABB()) {
		r_aabb = mesh->custom_aabb;
		return OK;
	}
	AABB aabb;
	for (int i = 0; i < mesh->surfaces.size(); i++) {
		if (i == 0) {
			aabb = mesh->surfaces[i]->source.aabb;
		} else {
			aabb.merge_with(mesh->surfaces[i]->source.aabb);
		}
	}
	if (p_skeleton.is_valid() || mesh->blend_shape_count) {
		const FFSkeleton *skeleton = _skeleton_owner.get_or_null(p_skeleton);
		// The cull API receives the mesh and skeleton, so include all active poses
		// sharing that pair. Base bounds cover mesh users without a deformation instance.
		for (const FFMeshInstance *instance : _instances) {
			if (instance->base != p_mesh || instance->skeleton != p_skeleton) {
				continue;
			}
			bool active_morph = false;
			for (float weight : instance->blend_weights) {
				if (weight != 0) {
					active_morph = true;
					break;
				}
			}
			for (const FFSurfaceData *surface : mesh->surfaces) {
				if (!active_morph && skeleton && _merge_skinning_bounds(*surface, *skeleton, aabb)) {
					continue;
				}
				Vector<FFDeformedVertex> vertices;
				AABB bounds;
				const Error error = mesh_instance_deform(instance->self, *surface, vertices, bounds, nullptr, nullptr, true);
				if (error != OK) {
					return error;
				}
				if (!vertices.is_empty()) {
					aabb.merge_with(bounds);
				}
			}
		}
	}
	r_aabb = aabb;
	return OK;
}

Error FixedFunctionMeshStorage::mesh_clear(RID p_mesh) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->free_pending || _clearing || _clear_pending) {
		return ERR_INVALID_PARAMETER;
	}
	while (!mesh->surfaces.is_empty()) {
		mesh_remove_surface(p_mesh, mesh->surfaces.size() - 1);
	}
	return OK;
}

Error FixedFunctionMeshStorage::mesh_set_path(RID p_mesh, const String &p_path) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->free_pending || _clearing || _clear_pending) {
		return ERR_INVALID_PARAMETER;
	}
	mesh->path = p_path;
	return OK;
}

String FixedFunctionMeshStorage::mesh_get_path(RID p_mesh) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh ? mesh->path : String();
}

Error FixedFunctionMeshStorage::mesh_set_shadow_mesh(RID p_mesh, RID p_shadow) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->free_pending || _clearing || _clear_pending || p_mesh == p_shadow || (p_shadow.is_valid() && !owns_mesh(p_shadow))) {
		return ERR_INVALID_PARAMETER;
	}
	mesh->shadow_mesh = p_shadow; // Retained CPU metadata; shadow drawing remains unavailable.
	return OK;
}

void FixedFunctionMeshStorage::mesh_debug_usage(List<RenderingServerTypes::MeshInfo> *r_info) const {
	if (!r_info) {
		return;
	}
	for (const FFMesh *mesh : _meshes) {
		RenderingServerTypes::MeshInfo info;
		info.mesh = mesh->self;
		info.path = mesh->path;
		for (const FFSurfaceData *surface : mesh->surfaces) {
			info.vertex_count += surface->source.vertex_count;
		}
		// Per-mesh native allocations remain empty; transient upload streams are device-owned.
		r_info->push_back(info);
	}
}

Error FixedFunctionMeshStorage::mesh_update_dependency(RID p_mesh, DependencyTracker *p_tracker) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->free_pending || !p_tracker) {
		return ERR_INVALID_PARAMETER;
	}
	p_tracker->update_dependency(&mesh->dependency);
	return OK;
}

Error FixedFunctionMeshStorage::update_dependencies() {
	if (_notifying || _clearing) {
		return ERR_BUSY;
	}
	Vector<FFDependencyEvent> events;
	for (FFMesh *mesh : _meshes) {
		if (mesh->aabb_dirty || mesh->materials_dirty) {
			const FFDependencyEvent event = { mesh->self, mesh->aabb_dirty, mesh->materials_dirty };
			events.push_back(event);
			mesh->aabb_dirty = mesh->materials_dirty = false;
		}
	}
	_notifying = true;
	for (const FFDependencyEvent &event : events) {
		FFMesh *mesh = _get_mesh(event.mesh);
		if (!mesh || mesh->free_pending) {
			continue;
		}
		if (event.aabb) {
			mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
		}
		if (event.materials && !mesh->free_pending) {
			mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
		}
	}
	const List<RID> skeletons(_skeletons);
	for (RID rid : skeletons) {
		FFSkeleton *skeleton = _skeleton_owner.get_or_null(rid);
		if (skeleton && skeleton->dirty) {
			skeleton->dirty = false;
			skeleton->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_SKELETON_BONES);
		}
	}
	const List<RID> multimeshes(_multimeshes);
	for (RID rid : multimeshes) {
		FFMultiMesh *multimesh = _multimesh_owner.get_or_null(rid);
		if (multimesh && (multimesh->mesh_dirty || multimesh->bounds_dirty)) {
			const Dependency::DependencyChangedNotification change = multimesh->mesh_dirty ? Dependency::DEPENDENCY_CHANGED_MULTIMESH : Dependency::DEPENDENCY_CHANGED_AABB;
			multimesh->mesh_dirty = multimesh->bounds_dirty = false;
			multimesh->dependency.changed_notify(change);
		}
	}
	_notifying = false;
	_retire_pending();
	for (const FFMesh *mesh : _meshes) {
		if (mesh->aabb_dirty || mesh->materials_dirty || mesh->free_pending) {
			return ERR_SKIP;
		}
	}
	return OK;
}

RID FixedFunctionMeshStorage::mesh_instance_create(RID p_mesh) {
	const FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->free_pending || _clearing || _clear_pending) {
		return RID();
	}
	FFMeshInstance *instance = memnew(FFMeshInstance);
	instance->base = p_mesh;
	if (instance->blend_weights.resize_initialized(mesh->blend_shape_count) != OK) {
		memdelete(instance);
		return RID();
	}
	instance->self = _instance_owner.make_rid(instance);
	_instances.push_back(instance);
	return instance->self;
}

Error FixedFunctionMeshStorage::mesh_instance_set_skeleton(RID p_instance, RID p_skeleton) {
	FFMeshInstance *instance = _get_instance(p_instance);
	if (!instance || _clearing || _clear_pending) {
		return ERR_INVALID_PARAMETER;
	}
	instance->skeleton = p_skeleton;
	FFMesh *mesh = _get_mesh(instance->base);
	if (mesh) {
		mesh->aabb_dirty = true;
	}
	return OK;
}

Error FixedFunctionMeshStorage::mesh_instance_set_blend_shape_weight(RID p_instance, int p_shape, float p_weight) {
	FFMeshInstance *instance = _get_instance(p_instance);
	if (!instance || _clearing || _clear_pending || p_shape < 0 || p_shape >= instance->blend_weights.size() || !Math::is_finite(p_weight)) {
		return ERR_INVALID_PARAMETER;
	}
	if (instance->blend_weights[p_shape] != p_weight) {
		instance->blend_weights.set(p_shape, p_weight);
		FFMesh *mesh = _get_mesh(instance->base);
		if (mesh) {
			mesh->aabb_dirty = true;
		}
	}
	return OK;
}

Error FixedFunctionMeshStorage::mesh_instance_set_canvas_transform(RID p_instance, const Transform2D &p_transform) {
	FFMeshInstance *instance = _get_instance(p_instance);
	if (!instance || _clearing || _clear_pending || !p_transform.is_finite()) {
		return ERR_INVALID_PARAMETER;
	}
	instance->canvas_transform = p_transform;
	return OK;
}

Error FixedFunctionMeshStorage::mesh_instance_get_draw_status(RID p_instance, RID p_mesh) const {
	const FFMeshInstance *instance = _get_instance(p_instance);
	return instance && instance->base == p_mesh ? OK : ERR_INVALID_PARAMETER;
}

void FixedFunctionMeshStorage::_free_mesh(FFMesh *p_mesh) {
	for (RID rid : _multimeshes) {
		FFMultiMesh *multimesh = _multimesh_owner.get_or_null(rid);
		if (multimesh && multimesh->mesh == p_mesh->self) {
			multimesh->mesh = RID();
			multimesh->mesh_dirty = true;
		}
	}
	for (FFMeshInstance *instance : _instances) {
		if (instance->base == p_mesh->self) {
			instance->base = RID();
		}
	}
	for (FFMesh *mesh : _meshes) {
		if (mesh->shadow_mesh == p_mesh->self) {
			mesh->shadow_mesh = RID();
		}
	}
	for (FFSurfaceData *surface : p_mesh->surfaces) {
		_delete_surface(surface);
	}
	_mesh_owner.free(p_mesh->self);
	_meshes.erase(p_mesh);
	const bool was_notifying = _notifying;
	_notifying = true;
	p_mesh->dependency.deleted_notify(p_mesh->self);
	_notifying = was_notifying;
	memdelete(p_mesh);
}

void FixedFunctionMeshStorage::_retire_pending() {
	if (_clear_pending) {
		clear();
		return;
	}
	Vector<RID> retired;
	for (const FFMesh *mesh : _meshes) {
		if (mesh->free_pending) {
			retired.push_back(mesh->self);
		}
	}
	for (RID rid : retired) {
		FFMesh *mesh = _get_mesh(rid);
		if (mesh) {
			_free_mesh(mesh);
		}
	}
}

Error FixedFunctionMeshStorage::free(RID p_rid) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFMeshInstance *instance = _get_instance(p_rid);
	if (instance) {
		_instance_owner.free(p_rid);
		_instances.erase(instance);
		memdelete(instance);
		return OK;
	}
	FFMesh *mesh = _get_mesh(p_rid);
	if (!mesh) {
		return ERR_INVALID_PARAMETER;
	}
	if (mesh->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	if (_notifying) {
		mesh->free_pending = true;
		for (const FFSurfaceData *surface : mesh->surfaces) {
			_surfaces.erase(surface->identity);
		}
	} else {
		_free_mesh(mesh);
		_retire_pending();
	}
	return OK;
}

void FixedFunctionMeshStorage::clear() {
	if (_clearing) {
		return;
	}
	if (_notifying) {
		_clear_pending = true;
		_surfaces.clear();
		for (FFMesh *mesh : _meshes) {
			mesh->free_pending = true;
		}
		return;
	}
	_clear_pending = false;
	_clearing = true;
	while (_meshes.front()) {
		_free_mesh(_meshes.front()->get());
	}
	while (_instances.front()) {
		FFMeshInstance *instance = _instances.front()->get();
		_instance_owner.free(instance->self);
		_instances.pop_front();
		memdelete(instance);
	}
	while (_multimeshes.front()) {
		multimesh_free(_multimeshes.front()->get());
	}
	while (_skeletons.front()) {
		skeleton_free(_skeletons.front()->get());
	}
	_clearing = false;
}

FixedFunctionMeshStorage::FixedFunctionMeshStorage(FixedFunctionMaterialStorage &p_materials) : _next_surface_identity(0), _materials(p_materials), _clearing(false), _notifying(false), _clear_pending(false) {}
FixedFunctionMeshStorage::~FixedFunctionMeshStorage() {
	clear();
}

bool FixedFunctionMeshStorage::surface_is_current(uint64_t p_identity, uint64_t p_version) const {
	const RBMap<uint64_t, FFSurfaceData *>::Element *surface = _surfaces.find(p_identity);
	return surface && surface->get()->version == p_version;
}
