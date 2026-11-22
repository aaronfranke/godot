#include "mesh_storage.h"

#include "servers/visual/visual_server_canvas.h"
#include "servers/visual/visual_server_globals.h"

#include <string.h>
#include <limits>

bool FixedFunctionMeshStorage::FFGeometryBounds::transform(const Transform &p_transform) {
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

FixedFunctionMeshStorage::FFSurfaceData::FFSurfaceData() :
		identity(0), version(0), format(0), vertex_count(0), index_count(0), primitive(VS::PRIMITIVE_TRIANGLES) {
	memset(offsets, 0, sizeof(offsets));
	memset(strides, 0, sizeof(strides));
}

FixedFunctionMeshStorage::FFMesh::FFMesh() :
		blend_shape_count(0), blend_shape_mode(VS::BLEND_SHAPE_MODE_NORMALIZED), aabb_dirty(false), materials_dirty(false) {
}

FixedFunctionMeshStorage::FFMesh *FixedFunctionMeshStorage::_get_mesh(RID p_mesh) const {
	return _mesh_owner.owns(p_mesh) ? _mesh_owner.getornull(p_mesh) : nullptr;
}

FixedFunctionMeshStorage::FFSurface *FixedFunctionMeshStorage::_get_surface(RID p_mesh, int p_surface) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh && p_surface >= 0 && p_surface < mesh->surfaces.size() ? mesh->surfaces[p_surface] : nullptr;
}

bool FixedFunctionMeshStorage::_valid_aabb(const AABB &p_aabb, bool p_allow_unused) {
	for (int i = 0; i < 3; i++) {
		if (Math::is_nan(p_aabb.position[i]) || Math::is_inf(p_aabb.position[i]) || Math::is_nan(p_aabb.size[i]) || Math::is_inf(p_aabb.size[i]) || (!p_allow_unused && p_aabb.size[i] < 0) || Math::is_nan(p_aabb.position[i] + p_aabb.size[i]) || Math::is_inf(p_aabb.position[i] + p_aabb.size[i])) {
			return false;
		}
	}
	return true;
}

Error FixedFunctionMeshStorage::_make_layout(FFSurfaceData &r_data) {
	const uint32_t format = r_data.format;
	const uint32_t known_flags = (VS::ARRAY_FLAG_USE_VERTEX_CACHE_OPTIMIZATION << 1) - 1;
	if ((format & ~known_flags) || !(format & VS::ARRAY_FORMAT_VERTEX) || r_data.vertex_count <= 0 || r_data.index_count < 0 || bool(format & VS::ARRAY_FORMAT_INDEX) != (r_data.index_count > 0) || r_data.primitive < 0 || r_data.primitive >= VS::PRIMITIVE_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	// Preflight in 64 bits before calling the upstream helper, which multiplies ints.
	uint64_t stride = (format & VS::ARRAY_FLAG_USE_2D_VERTICES) ? 2 : 3;
	stride *= (format & VS::ARRAY_COMPRESS_VERTEX) ? 2 : 4;
	if (stride == 6) {
		stride = 8;
	}
	if (format & VS::ARRAY_FORMAT_NORMAL) {
		if (format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
			stride += ((format & VS::ARRAY_COMPRESS_NORMAL) && (format & VS::ARRAY_FORMAT_TANGENT) && (format & VS::ARRAY_COMPRESS_TANGENT)) ? 2 : 4;
		} else {
			stride += (format & VS::ARRAY_COMPRESS_NORMAL) ? 4 : 12;
		}
	}
	if (format & VS::ARRAY_FORMAT_TANGENT) {
		if (format & VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
			stride += ((format & VS::ARRAY_COMPRESS_TANGENT) && (format & VS::ARRAY_FORMAT_NORMAL) && (format & VS::ARRAY_COMPRESS_NORMAL)) ? 2 : 4;
		} else {
			stride += (format & VS::ARRAY_COMPRESS_TANGENT) ? 4 : 16;
		}
	}
	if (format & VS::ARRAY_FORMAT_COLOR) {
		stride += (format & VS::ARRAY_COMPRESS_COLOR) ? 4 : 16;
	}
	if (format & VS::ARRAY_FORMAT_TEX_UV) {
		stride += (format & VS::ARRAY_COMPRESS_TEX_UV) ? 4 : 8;
	}
	if (format & VS::ARRAY_FORMAT_TEX_UV2) {
		stride += (format & VS::ARRAY_COMPRESS_TEX_UV2) ? 4 : 8;
	}
	if (format & VS::ARRAY_FORMAT_BONES) {
		stride += (format & VS::ARRAY_FLAG_USE_16_BIT_BONES) ? 8 : 4;
	}
	if (format & VS::ARRAY_FORMAT_WEIGHTS) {
		stride += (format & VS::ARRAY_COMPRESS_WEIGHTS) ? 8 : 16;
	}
	const uint64_t array_size = stride * uint64_t(r_data.vertex_count);
	const uint64_t index_size = uint64_t(r_data.index_count) * (r_data.vertex_count >= 65536 ? 4 : 2);
	if (array_size > INT32_MAX || index_size > INT32_MAX || array_size != uint64_t(r_data.array.size()) || index_size != uint64_t(r_data.index_array.size())) {
		return ERR_INVALID_PARAMETER;
	}
	VisualServer *server = VisualServer::get_singleton();
	if (!server) {
		return ERR_UNCONFIGURED;
	}
	server->mesh_surface_make_offsets_from_format(format, r_data.vertex_count, r_data.index_count, r_data.offsets, r_data.strides);
	return OK;
}

Vector3 FixedFunctionMeshStorage::surface_read_position(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index) {
	if (!p_surface.immediate_vertices.empty()) {
		return p_surface.immediate_vertices[p_index].position;
	}
	Vector3 position;
	const bool compressed = p_surface.format & VS::ARRAY_COMPRESS_VERTEX;
	const int dimensions = (p_surface.format & VS::ARRAY_FLAG_USE_2D_VERTICES) ? 2 : 3;
	for (int i = 0; i < dimensions; i++) {
		const uint8_t *source = p_data + p_surface.offsets[VS::ARRAY_VERTEX] + p_index * p_surface.strides[VS::ARRAY_VERTEX] + i * (compressed ? 2 : 4);
		float component;
		if (compressed) {
			uint16_t half;
			memcpy(&half, source, 2);
			component = Math::half_to_float(half);
		} else {
			memcpy(&component, source, 4);
		}
		position[i] = component;
	}
	return position;
}

Error FixedFunctionMeshStorage::_validate_positions(const FFSurfaceData &p_data, const PoolVector<uint8_t> &p_array, PoolVector<Vector3> *r_positions, Vector3 *r_min, Vector3 *r_max) {
	if (p_array.size() != p_data.array.size()) {
		return ERR_INVALID_PARAMETER;
	}
	PoolVector<Vector3> positions;
	if (r_positions && positions.resize(p_data.vertex_count) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	PoolVector<Vector3>::Write output;
	if (r_positions) {
		output = positions.write();
	}
	const PoolVector<uint8_t>::Read input = p_array.read();
	Vector3 minimum;
	Vector3 maximum;
	for (int i = 0; i < p_data.vertex_count; i++) {
		const Vector3 position = surface_read_position(p_data, input.ptr(), i);
		for (int axis = 0; axis < 3; axis++) {
			if (Math::is_nan(position[axis]) || Math::is_inf(position[axis])) {
				return ERR_INVALID_DATA;
			}
			if (r_min) {
				minimum[axis] = i ? MIN(minimum[axis], position[axis]) : position[axis];
				maximum[axis] = i ? MAX(maximum[axis], position[axis]) : position[axis];
			}
		}
		if (r_positions) {
			output[i] = position;
		}
	}
	output.release();
	if (r_positions) {
		*r_positions = positions;
	}
	if (r_min) {
		*r_min = minimum;
		*r_max = maximum;
	}
	return OK;
}

Error FixedFunctionMeshStorage::_validate_indices(const FFSurfaceData &p_data) {
	if (!p_data.index_count) {
		return OK;
	}
	const PoolVector<uint8_t>::Read input = p_data.index_array.read();
	for (int i = 0; i < p_data.index_count; i++) {
		uint32_t index;
		if (p_data.vertex_count >= 65536) {
			memcpy(&index, input.ptr() + i * 4, sizeof(index));
		} else {
			uint16_t small;
			memcpy(&small, input.ptr() + i * 2, sizeof(small));
			index = small;
		}
		if (index >= uint32_t(p_data.vertex_count)) {
			return ERR_INVALID_DATA;
		}
	}
	return OK;
}

Error FixedFunctionMeshStorage::_validate_morphs(const FFSurfaceData &p_data, const PoolVector<uint8_t> &p_base, AABB *r_bounds) {
	const PoolVector<uint8_t>::Read base = p_base.read();
	Vector3 extent[2];
	AABB base_bounds;
	for (int shape = 0; shape < p_data.blend_shapes.size(); shape++) {
		const PoolVector<uint8_t> &array = p_data.blend_shapes[shape];
		if (array.size() != p_base.size()) {
			return ERR_INVALID_PARAMETER;
		}
		const PoolVector<uint8_t>::Read target = array.read();
		Vector3 normalized, relative;
		for (int vertex = 0; vertex < p_data.vertex_count; vertex++) {
			const Vector3 position = surface_read_position(p_data, target.ptr(), vertex);
			const Vector3 base_position = surface_read_position(p_data, base.ptr(), vertex);
			const Vector3 delta = position - base_position;
			if (shape == 0) {
				if (vertex == 0) {
					base_bounds.position = base_position;
				} else {
					base_bounds.expand_to(base_position);
				}
			}
			for (int axis = 0; axis < 3; axis++) {
				if (Math::is_nan(position[axis]) || Math::is_inf(position[axis])) {
					return ERR_INVALID_DATA;
				}
				normalized[axis] = MAX(normalized[axis], Math::abs(delta[axis]));
				relative[axis] = MAX(relative[axis], Math::abs(position[axis]));
			}
		}
		extent[VS::BLEND_SHAPE_MODE_NORMALIZED] += normalized;
		extent[VS::BLEND_SHAPE_MODE_RELATIVE] += relative;
	}
	// Per-instance weights are absent from the server's mesh AABB query. Bound
	// independent weights in [-1, 1] without changing the public surface AABB.
	for (int mode = 0; mode < 2; mode++) {
		const AABB expanded(base_bounds.position - extent[mode], base_bounds.size + extent[mode] * 2);
		r_bounds[mode] = p_data.aabb;
		if (_valid_aabb(expanded)) {
			r_bounds[mode].merge_with(expanded);
		}
	}
	return OK;
}

RID FixedFunctionMeshStorage::mesh_create() {
	if (_clearing) {
		return RID();
	}
	FFMesh *mesh = memnew(FFMesh);
	mesh->self = _mesh_owner.make_rid(mesh);
	mesh->dependency_owner = mesh->self;
	_meshes.push_back(mesh);
	return mesh->self;
}

Error FixedFunctionMeshStorage::mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t>> &p_blend_shapes, const Vector<AABB> &p_bone_aabbs) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || _clearing || !_valid_aabb(p_aabb) || p_blend_shapes.size() != mesh->blend_shape_count) {
		return ERR_INVALID_PARAMETER;
	}
	FFSurfaceData data;
	data.format = p_format;
	data.primitive = p_primitive;
	data.array = p_array;
	data.index_array = p_index_array;
	data.vertex_count = p_vertex_count;
	data.index_count = p_index_count;
	data.aabb = p_aabb;
	data.blend_shapes = p_blend_shapes;
	data.bone_aabbs = p_bone_aabbs;
	Error error = _make_layout(data);
	if (error == OK) {
		error = _validate_positions(data, data.array, nullptr, &data.position_min, &data.position_max);
	}
	if (error == OK) {
		error = _validate_indices(data);
	}
	AABB morph_bounds[2];
	if (error == OK && !data.blend_shapes.empty()) {
		error = _validate_morphs(data, data.array, morph_bounds);
	}
	for (int i = 0; error == OK && i < data.bone_aabbs.size(); i++) {
		// Godot uses negative sizes as an unused-bone sentinel.
		if (!_valid_aabb(data.bone_aabbs[i], true)) {
			error = ERR_INVALID_PARAMETER;
		}
	}
	if (error != OK) {
		return error;
	}
	if (_next_surface_identity == UINT64_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	data.identity = ++_next_surface_identity;
	data.version = 1;
	FFSurface *surface = memnew(FFSurface);
	surface->data = data;
	surface->morph_bounds[0] = morph_bounds[0];
	surface->morph_bounds[1] = morph_bounds[1];
	if (mesh->surfaces.push_back(surface)) {
		memdelete(surface);
		return ERR_OUT_OF_MEMORY;
	}
	_surfaces[data.identity] = surface;
	mesh->aabb_dirty = true;
	mesh->materials_dirty = true;
	return OK;
}

Error FixedFunctionMeshStorage::mesh_set_blend_shape_count(RID p_mesh, int p_count) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || _clearing || p_count < 0 || !mesh->surfaces.empty()) {
		return ERR_INVALID_PARAMETER;
	}
	mesh->blend_shape_count = p_count;
	mesh->blend_values = PoolVector<float>();
	return OK;
}

int FixedFunctionMeshStorage::mesh_get_blend_shape_count(RID p_mesh) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh ? mesh->blend_shape_count : 0;
}

Error FixedFunctionMeshStorage::mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || _clearing || (p_mode != VS::BLEND_SHAPE_MODE_NORMALIZED && p_mode != VS::BLEND_SHAPE_MODE_RELATIVE)) {
		return ERR_INVALID_PARAMETER;
	}
	if (mesh->blend_shape_mode != p_mode) {
		mesh->blend_shape_mode = p_mode;
		mesh->aabb_dirty = true;
	}
	return OK;
}

VS::BlendShapeMode FixedFunctionMeshStorage::mesh_get_blend_shape_mode(RID p_mesh) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh ? mesh->blend_shape_mode : VS::BLEND_SHAPE_MODE_NORMALIZED;
}

Error FixedFunctionMeshStorage::mesh_set_blend_shape_values(RID p_mesh, const PoolVector<float> &p_values) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || _clearing || (p_values.size() && p_values.size() != mesh->blend_shape_count)) {
		return ERR_INVALID_PARAMETER;
	}
	const PoolVector<float>::Read values = p_values.read();
	for (int i = 0; i < p_values.size(); i++) {
		if (Math::is_nan(values[i]) || Math::is_inf(values[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	mesh->blend_values = p_values;
	return OK;
}

PoolVector<float> FixedFunctionMeshStorage::mesh_get_blend_shape_values(RID p_mesh) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh ? mesh->blend_values : PoolVector<float>();
}

Error FixedFunctionMeshStorage::mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data) {
	FFSurface *surface = _get_surface(p_mesh, p_surface);
	if (!surface || _clearing || p_offset < 0 || uint64_t(p_offset) + uint64_t(p_data.size()) > uint64_t(surface->data.array.size())) {
		return ERR_INVALID_PARAMETER;
	}
	if (!p_data.size()) {
		return OK;
	}
	if (surface->data.version == UINT64_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	PoolVector<uint8_t> array = surface->data.array;
	{
		PoolVector<uint8_t>::Write write = array.write();
		const PoolVector<uint8_t>::Read read = p_data.read();
		memcpy(write.ptr() + p_offset, read.ptr(), p_data.size());
	}
	Vector3 minimum;
	Vector3 maximum;
	Error error = _validate_positions(surface->data, array, nullptr, &minimum, &maximum);
	AABB morph_bounds[2];
	if (error == OK && !surface->data.blend_shapes.empty()) {
		error = _validate_morphs(surface->data, array, morph_bounds);
	}
	if (error == OK) {
		// As in GLES, region writes retain the caller-supplied surface bounds.
		surface->data.array = array;
		surface->data.position_min = minimum;
		surface->data.position_max = maximum;
		surface->data.version++;
		if (!surface->data.blend_shapes.empty()) {
			surface->morph_bounds[0] = morph_bounds[0];
			surface->morph_bounds[1] = morph_bounds[1];
			_get_mesh(p_mesh)->aabb_dirty = true;
		}
	}
	return error;
}

Error FixedFunctionMeshStorage::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) {
	FFMesh *mesh = _get_mesh(p_mesh);
	FFSurface *surface = _get_surface(p_mesh, p_surface);
	if (!surface || _clearing || (p_material.is_valid() && !_materials.owns_material(p_material))) {
		return ERR_INVALID_PARAMETER;
	}
	if (surface->material == p_material) {
		return OK;
	}
	_materials.material_remove_geometry_owner(&surface->material);
	surface->material = p_material;
	if (p_material.is_valid()) {
		const Error error = _materials.material_add_geometry_owner(&surface->material, &mesh->materials_dirty);
		if (error != OK) {
			surface->material = RID();
			mesh->materials_dirty = true;
			return error;
		}
	}
	mesh->materials_dirty = true;
	return OK;
}

RID FixedFunctionMeshStorage::mesh_surface_get_material(RID p_mesh, int p_surface) const {
	const FFSurface *surface = _get_surface(p_mesh, p_surface);
	return surface ? surface->material : RID();
}

Error FixedFunctionMeshStorage::mesh_surface_get_data(RID p_mesh, int p_surface, FFSurfaceData &r_data) const {
	const FFSurface *surface = _get_surface(p_mesh, p_surface);
	if (!surface) {
		return ERR_INVALID_PARAMETER;
	}
	r_data = surface->data;
	return OK;
}

Error FixedFunctionMeshStorage::mesh_surface_get_positions(RID p_mesh, int p_surface, PoolVector<Vector3> &r_positions) const {
	const FFSurface *surface = _get_surface(p_mesh, p_surface);
	return surface ? _validate_positions(surface->data, surface->data.array, &r_positions) : ERR_INVALID_PARAMETER;
}

bool FixedFunctionMeshStorage::surface_is_current(uint64_t p_identity, uint64_t p_version) const {
	const Map<uint64_t, FFSurface *>::Element *surface = _surfaces.find(p_identity);
	return surface && surface->get()->data.version == p_version;
}

void FixedFunctionMeshStorage::_delete_surface(FFSurface *p_surface) {
	_surfaces.erase(p_surface->data.identity);
	_materials.material_remove_geometry_owner(&p_surface->material);
	memdelete(p_surface);
}

Error FixedFunctionMeshStorage::mesh_remove_surface(RID p_mesh, int p_surface) {
	FFMesh *mesh = _get_mesh(p_mesh);
	FFSurface *surface = _get_surface(p_mesh, p_surface);
	if (!surface || _clearing) {
		return ERR_INVALID_PARAMETER;
	}
	mesh->surfaces.remove(p_surface);
	_delete_surface(surface);
	mesh->aabb_dirty = true;
	mesh->materials_dirty = true;
	return OK;
}

int FixedFunctionMeshStorage::mesh_get_surface_count(RID p_mesh) const {
	const FFMesh *mesh = _get_mesh(p_mesh);
	return mesh ? mesh->surfaces.size() : 0;
}

Error FixedFunctionMeshStorage::mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || _clearing || !_valid_aabb(p_aabb)) {
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
	if (!mesh) {
		return ERR_INVALID_PARAMETER;
	}
	if (mesh->custom_aabb != AABB()) {
		r_aabb = mesh->custom_aabb;
		return OK;
	}
	const FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	if (p_skeleton.is_valid() && !skeleton) {
		return ERR_INVALID_PARAMETER;
	}
	AABB aabb;
	for (int i = 0; i < mesh->surfaces.size(); i++) {
		const FFSurface &surface = *mesh->surfaces[i];
		AABB bounds = surface.data.blend_shapes.empty() ? surface.data.aabb : surface.morph_bounds[mesh->blend_shape_mode];
		if (skeleton) {
			bounds = _skinned_surface_aabb(surface.data, *skeleton, bounds);
		}
		if (i == 0) {
			aabb = bounds;
		} else {
			aabb.merge_with(bounds);
		}
	}
	r_aabb = aabb;
	return OK;
}

Error FixedFunctionMeshStorage::mesh_clear(RID p_mesh) {
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || _clearing) {
		return ERR_INVALID_PARAMETER;
	}
	while (!mesh->surfaces.empty()) {
		mesh_remove_surface(p_mesh, mesh->surfaces.size() - 1);
	}
	return OK;
}

Error FixedFunctionMeshStorage::instance_add_dependency(RID p_mesh, RasterizerScene::InstanceBase *p_instance) {
	if (!p_instance || _clearing) {
		return ERR_INVALID_PARAMETER;
	}
	FFMesh *mesh = p_instance->base_type == VS::INSTANCE_MESH ? _get_mesh(p_mesh) : nullptr;
	if (p_instance->base_type == VS::INSTANCE_IMMEDIATE && owns_immediate(p_mesh)) {
		mesh = _get_dependency_mesh(p_mesh);
	}
	FFMultiMesh *multimesh = p_instance->base_type == VS::INSTANCE_MULTIMESH ? _get_multimesh(p_mesh) : nullptr;
	if (!mesh && !multimesh) {
		return ERR_INVALID_PARAMETER;
	}
	if (_registrations.has(p_instance)) {
		return ERR_ALREADY_IN_USE;
	}
	if (_next_generation == UINT64_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	const FFRegistration registration = { ++_next_generation, p_mesh };
	_registrations[p_instance] = registration;
	if (mesh) {
		mesh->instances[p_instance] = registration.generation;
		mesh->aabb_dirty = mesh->materials_dirty = true;
	} else {
		multimesh->instances[p_instance] = registration.generation;
		multimesh->aabb_dirty = multimesh->materials_dirty = true;
	}
	return OK;
}

Error FixedFunctionMeshStorage::instance_remove_dependency(RID p_mesh, RasterizerScene::InstanceBase *p_instance) {
	const Map<RasterizerScene::InstanceBase *, FFRegistration>::Element *registration = _registrations.find(p_instance);
	if (!registration || registration->get().owner != p_mesh) {
		return ERR_INVALID_PARAMETER;
	}
	_registrations.erase(p_instance);
	FFMesh *mesh = _get_dependency_mesh(p_mesh);
	if (mesh) {
		mesh->instances.erase(p_instance);
	} else if (FFMultiMesh *multimesh = _get_multimesh(p_mesh)) {
		multimesh->instances.erase(p_instance);
	}
	return OK;
}

Error FixedFunctionMeshStorage::update_dependencies() {
	if (_notifying || _clearing) {
		return ERR_BUSY;
	}
	Vector<FFDependencyEvent> pending;
	for (List<FFMesh *>::Element *E = _meshes.front(); E; E = E->next()) {
		FFMesh *mesh = E->get();
		if (!mesh->aabb_dirty && !mesh->materials_dirty) {
			continue;
		}
		for (Map<RasterizerScene::InstanceBase *, uint64_t>::Element *I = mesh->instances.front(); I; I = I->next()) {
			const FFDependencyEvent event = { I->get(), mesh->dependency_owner, I->key(), mesh->aabb_dirty, mesh->materials_dirty, false };
			if (pending.push_back(event)) {
				return ERR_OUT_OF_MEMORY;
			}
		}
	}
	for (List<FFSkeleton *>::Element *E = _skeletons.front(); E; E = E->next()) {
		FFSkeleton *skeleton = E->get();
		if (!skeleton->dirty) {
			continue;
		}
		_invalidate_skeleton_canvas_items(skeleton);
		for (Map<RasterizerScene::InstanceBase *, uint64_t>::Element *I = skeleton->instances.front(); I; I = I->next()) {
			const FFDependencyEvent event = { I->get(), skeleton->self, I->key(), true, false, true };
			if (pending.push_back(event)) {
				return ERR_OUT_OF_MEMORY;
			}
		}
	}
	for (List<FFMultiMesh *>::Element *E = _multimeshes.front(); E; E = E->next()) {
		const FFMultiMesh *multimesh = E->get();
		const FFMesh *mesh = _get_mesh(multimesh->mesh);
		const bool aabb = multimesh->aabb_dirty || (mesh && mesh->aabb_dirty);
		const bool materials = multimesh->materials_dirty || (mesh && mesh->materials_dirty);
		if (aabb && VSG::canvas) {
			for (int i = 0; i < multimesh->canvas_items.size(); i++) {
				const RID item = multimesh->canvas_items[i];
				VisualServerCanvas::Item *canvas_item = VSG::canvas->canvas_item_owner.getornull(item);
				if (canvas_item) {
					canvas_item->rect_dirty = true;
					VSG::canvas->_canvas_item_invalidate_local_bound(item);
				}
			}
		}
		if (aabb || materials) {
			for (const Map<RasterizerScene::InstanceBase *, uint64_t>::Element *I = multimesh->instances.front(); I; I = I->next()) {
				const FFDependencyEvent event = { I->get(), multimesh->self, I->key(), aabb, materials, false };
				if (pending.push_back(event)) {
					return ERR_OUT_OF_MEMORY;
				}
			}
		}
	}
	for (List<FFMesh *>::Element *E = _meshes.front(); E; E = E->next()) {
		E->get()->aabb_dirty = false;
		E->get()->materials_dirty = false;
	}
	for (List<FFSkeleton *>::Element *E = _skeletons.front(); E; E = E->next()) {
		E->get()->dirty = false;
	}
	for (List<FFMultiMesh *>::Element *E = _multimeshes.front(); E; E = E->next()) {
		E->get()->aabb_dirty = E->get()->materials_dirty = false;
	}
	_notifying = true;
	for (int i = 0; i < pending.size(); i++) {
		const FFDependencyEvent event = pending[i];
		const Map<RasterizerScene::InstanceBase *, FFRegistration> &registrations = event.skeleton ? _skeleton_registrations : _registrations;
		const Map<RasterizerScene::InstanceBase *, FFRegistration>::Element *current = registrations.find(event.instance);
		if (current && current->get().generation == event.generation && current->get().owner == event.owner) {
			event.instance->base_changed(event.aabb, event.materials);
		}
	}
	_notifying = false;
	for (List<FFMesh *>::Element *E = _meshes.front(); E; E = E->next()) {
		if (E->get()->aabb_dirty || E->get()->materials_dirty) {
			return ERR_SKIP;
		}
	}
	for (List<FFSkeleton *>::Element *E = _skeletons.front(); E; E = E->next()) {
		if (E->get()->dirty) {
			return ERR_SKIP;
		}
	}
	for (List<FFMultiMesh *>::Element *E = _multimeshes.front(); E; E = E->next()) {
		if (E->get()->aabb_dirty || E->get()->materials_dirty) {
			return ERR_SKIP;
		}
	}
	return OK;
}

void FixedFunctionMeshStorage::_free_mesh(FFMesh *p_mesh) {
	_meshes.erase(p_mesh);
	_mesh_owner.free(p_mesh->self);
	for (List<FFMultiMesh *>::Element *E = _multimeshes.front(); E; E = E->next()) {
		FFMultiMesh *multimesh = E->get();
		if (multimesh->mesh == p_mesh->self) {
			multimesh->mesh = RID();
			multimesh->aabb_dirty = multimesh->materials_dirty = true;
		}
	}
	for (int i = 0; i < p_mesh->surfaces.size(); i++) {
		_delete_surface(p_mesh->surfaces[i]);
	}
	p_mesh->surfaces.clear();
	_detach_base_instances(p_mesh->instances);
	memdelete(p_mesh);
}

void FixedFunctionMeshStorage::_detach_base_instances(Map<RasterizerScene::InstanceBase *, uint64_t> &r_instances) {
	// Retire the RID before callbacks, but keep RID_Data alive: release-build owns()
	// reads its owner pointer. Callbacks may detach/rebind/free other instances.
	while (r_instances.front()) {
		const Map<RasterizerScene::InstanceBase *, uint64_t>::Element *owner = r_instances.front();
		RasterizerScene::InstanceBase *instance = owner->key();
		const uint64_t generation = owner->get();
		r_instances.erase(instance);
		const Map<RasterizerScene::InstanceBase *, FFRegistration>::Element *current = _registrations.find(instance);
		if (current && current->get().generation == generation) {
			instance->base_removed();
			current = _registrations.find(instance);
			if (current && current->get().generation == generation) {
				_registrations.erase(instance);
			}
		}
	}
}

Error FixedFunctionMeshStorage::free(RID p_mesh) {
	if (FFImmediate *immediate = _get_immediate(p_mesh)) {
		_free_immediate(immediate);
		return OK;
	}
	if (FFMultiMesh *multimesh = _get_multimesh(p_mesh)) {
		_free_multimesh(multimesh);
		return OK;
	}
	if (FFSkeleton *skeleton = _get_skeleton(p_mesh)) {
		_free_skeleton(skeleton);
		return OK;
	}
	FFMesh *mesh = _get_mesh(p_mesh);
	if (!mesh || mesh->dependency_owner != p_mesh) {
		return ERR_INVALID_PARAMETER;
	}
	_free_mesh(mesh);
	return OK;
}

void FixedFunctionMeshStorage::clear() {
	if (_clearing) {
		return;
	}
	_clearing = true;
	while (_immediates.front()) {
		_free_immediate(_immediates.front()->get());
	}
	while (_multimeshes.front()) {
		_free_multimesh(_multimeshes.front()->get());
	}
	while (_meshes.front()) {
		_free_mesh(_meshes.front()->get());
	}
	while (_skeletons.front()) {
		_free_skeleton(_skeletons.front()->get());
	}
	_clearing = false;
}

FixedFunctionMeshStorage::FixedFunctionMeshStorage(FixedFunctionMaterialStorage &p_materials) :
		_next_generation(0), _next_surface_identity(0), _materials(p_materials), _clearing(false), _notifying(false) {
}

FixedFunctionMeshStorage::~FixedFunctionMeshStorage() {
	clear();
}
