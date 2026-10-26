#include "rasterizer_storage_fixed_function.h"

RID RasterizerStorageFixedFunction::mesh_create() {
	return _mesh_storage.mesh_create();
}

void RasterizerStorageFixedFunction::mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, PoolVector<uint8_t> p_array, int p_vertex_count, PoolVector<uint8_t> p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t>> &p_blend_shapes, const Vector<AABB> &p_bone_aabbs) {
	_report_error(_mesh_storage.mesh_add_surface(p_mesh, p_format, p_primitive, p_array, p_vertex_count, p_index_array, p_index_count, p_aabb, p_blend_shapes, p_bone_aabbs), "mesh_add_surface");
}

void RasterizerStorageFixedFunction::mesh_set_blend_shape_count(RID p_mesh, int p_amount) {
	_report_error(_mesh_storage.mesh_set_blend_shape_count(p_mesh, p_amount), "mesh_set_blend_shape_count");
}

int RasterizerStorageFixedFunction::mesh_get_blend_shape_count(RID p_mesh) const {
	return _mesh_storage.mesh_get_blend_shape_count(p_mesh);
}

void RasterizerStorageFixedFunction::mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode) {
	_report_error(_mesh_storage.mesh_set_blend_shape_mode(p_mesh, p_mode), "mesh_set_blend_shape_mode");
}

VS::BlendShapeMode RasterizerStorageFixedFunction::mesh_get_blend_shape_mode(RID p_mesh) const {
	return _mesh_storage.mesh_get_blend_shape_mode(p_mesh);
}

void RasterizerStorageFixedFunction::mesh_set_blend_shape_values(RID p_mesh, PoolVector<float> p_values) {
	_report_error(_mesh_storage.mesh_set_blend_shape_values(p_mesh, p_values), "mesh_set_blend_shape_values");
}

PoolVector<float> RasterizerStorageFixedFunction::mesh_get_blend_shape_values(RID p_mesh) const {
	return _mesh_storage.mesh_get_blend_shape_values(p_mesh);
}

void RasterizerStorageFixedFunction::mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, PoolVector<uint8_t> p_data) {
	_report_error(_mesh_storage.mesh_surface_update_region(p_mesh, p_surface, p_offset, p_data), "mesh_surface_update_region");
}

void RasterizerStorageFixedFunction::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) {
	_report_error(_mesh_storage.mesh_surface_set_material(p_mesh, p_surface, p_material), "mesh_surface_set_material");
}

RID RasterizerStorageFixedFunction::mesh_surface_get_material(RID p_mesh, int p_surface) const {
	return _mesh_storage.mesh_surface_get_material(p_mesh, p_surface);
}

int RasterizerStorageFixedFunction::mesh_surface_get_array_len(RID p_mesh, int p_surface) const {
	FixedFunctionMeshStorage::FFSurfaceData data;
	const Error error = _mesh_storage.mesh_surface_get_data(p_mesh, p_surface, data);
	ERR_FAIL_COND_V_MSG(error != OK, 0, "Invalid Fixed Function surface query.");
	return data.vertex_count;
}

int RasterizerStorageFixedFunction::mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const {
	FixedFunctionMeshStorage::FFSurfaceData data;
	const Error error = _mesh_storage.mesh_surface_get_data(p_mesh, p_surface, data);
	ERR_FAIL_COND_V_MSG(error != OK, 0, "Invalid Fixed Function surface query.");
	return data.index_count;
}

PoolVector<uint8_t> RasterizerStorageFixedFunction::mesh_surface_get_array(RID p_mesh, int p_surface) const {
	FixedFunctionMeshStorage::FFSurfaceData data;
	const Error error = _mesh_storage.mesh_surface_get_data(p_mesh, p_surface, data);
	ERR_FAIL_COND_V_MSG(error != OK, PoolVector<uint8_t>(), "Invalid Fixed Function surface query.");
	return data.array;
}

PoolVector<uint8_t> RasterizerStorageFixedFunction::mesh_surface_get_index_array(RID p_mesh, int p_surface) const {
	FixedFunctionMeshStorage::FFSurfaceData data;
	const Error error = _mesh_storage.mesh_surface_get_data(p_mesh, p_surface, data);
	ERR_FAIL_COND_V_MSG(error != OK, PoolVector<uint8_t>(), "Invalid Fixed Function surface query.");
	return data.index_array;
}

uint32_t RasterizerStorageFixedFunction::mesh_surface_get_format(RID p_mesh, int p_surface) const {
	FixedFunctionMeshStorage::FFSurfaceData data;
	const Error error = _mesh_storage.mesh_surface_get_data(p_mesh, p_surface, data);
	ERR_FAIL_COND_V_MSG(error != OK, 0, "Invalid Fixed Function surface query.");
	return data.format;
}

VS::PrimitiveType RasterizerStorageFixedFunction::mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const {
	FixedFunctionMeshStorage::FFSurfaceData data;
	const Error error = _mesh_storage.mesh_surface_get_data(p_mesh, p_surface, data);
	ERR_FAIL_COND_V_MSG(error != OK, VS::PRIMITIVE_TRIANGLES, "Invalid Fixed Function surface query.");
	return data.primitive;
}

AABB RasterizerStorageFixedFunction::mesh_surface_get_aabb(RID p_mesh, int p_surface) const {
	FixedFunctionMeshStorage::FFSurfaceData data;
	const Error error = _mesh_storage.mesh_surface_get_data(p_mesh, p_surface, data);
	ERR_FAIL_COND_V_MSG(error != OK, AABB(), "Invalid Fixed Function surface query.");
	return data.aabb;
}

Vector<PoolVector<uint8_t>> RasterizerStorageFixedFunction::mesh_surface_get_blend_shapes(RID p_mesh, int p_surface) const {
	FixedFunctionMeshStorage::FFSurfaceData data;
	const Error error = _mesh_storage.mesh_surface_get_data(p_mesh, p_surface, data);
	ERR_FAIL_COND_V_MSG(error != OK, Vector<PoolVector<uint8_t>>(), "Invalid Fixed Function surface query.");
	return data.blend_shapes;
}

Vector<AABB> RasterizerStorageFixedFunction::mesh_surface_get_skeleton_aabb(RID p_mesh, int p_surface) const {
	FixedFunctionMeshStorage::FFSurfaceData data;
	const Error error = _mesh_storage.mesh_surface_get_data(p_mesh, p_surface, data);
	ERR_FAIL_COND_V_MSG(error != OK, Vector<AABB>(), "Invalid Fixed Function surface query.");
	return data.bone_aabbs;
}

void RasterizerStorageFixedFunction::mesh_remove_surface(RID p_mesh, int p_index) {
	_report_error(_mesh_storage.mesh_remove_surface(p_mesh, p_index), "mesh_remove_surface");
}

int RasterizerStorageFixedFunction::mesh_get_surface_count(RID p_mesh) const {
	return _mesh_storage.mesh_get_surface_count(p_mesh);
}

void RasterizerStorageFixedFunction::mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) {
	_report_error(_mesh_storage.mesh_set_custom_aabb(p_mesh, p_aabb), "mesh_set_custom_aabb");
}

AABB RasterizerStorageFixedFunction::mesh_get_custom_aabb(RID p_mesh) const {
	return _mesh_storage.mesh_get_custom_aabb(p_mesh);
}

AABB RasterizerStorageFixedFunction::mesh_get_aabb(RID p_mesh, RID p_skeleton) const {
	AABB aabb;
	const Error error = _mesh_storage.mesh_get_aabb(p_mesh, p_skeleton, aabb);
	ERR_FAIL_COND_V_MSG(error != OK, AABB(), "Unsupported or invalid Fixed Function mesh bounds query.");
	return aabb;
}

void RasterizerStorageFixedFunction::mesh_clear(RID p_mesh) {
	_report_error(_mesh_storage.mesh_clear(p_mesh), "mesh_clear");
}

void RasterizerStorageFixedFunction::instance_add_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) {
	_report_error(_light_storage.owns_light(p_base) ? _light_storage.instance_add_dependency(p_base, p_instance) : _mesh_storage.instance_add_dependency(p_base, p_instance), "instance_add_dependency");
}

void RasterizerStorageFixedFunction::instance_remove_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) {
	_report_error(_light_storage.has_dependency(p_base, p_instance) ? _light_storage.instance_remove_dependency(p_base, p_instance) : _mesh_storage.instance_remove_dependency(p_base, p_instance), "instance_remove_dependency");
}
