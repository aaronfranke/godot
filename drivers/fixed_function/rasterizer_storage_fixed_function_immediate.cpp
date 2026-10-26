#include "rasterizer_storage_fixed_function.h"

RID RasterizerStorageFixedFunction::immediate_create() {
	const RID immediate = _mesh_storage.immediate_create();
	_report_error(immediate.is_valid() ? OK : ERR_CANT_CREATE, "immediate_create");
	return immediate;
}

void RasterizerStorageFixedFunction::immediate_begin(RID p_immediate, VS::PrimitiveType p_primitive, RID p_texture) {
	_report_error(p_texture.is_valid() && !_texture_storage.texture_can_be_referenced(p_texture) ? ERR_INVALID_PARAMETER : _mesh_storage.immediate_begin(p_immediate, p_primitive, p_texture), "immediate_begin");
}

void RasterizerStorageFixedFunction::immediate_vertex(RID p_immediate, const Vector3 &p_vertex) {
	_report_error(_mesh_storage.immediate_vertex(p_immediate, p_vertex), "immediate_vertex");
}

void RasterizerStorageFixedFunction::immediate_normal(RID p_immediate, const Vector3 &p_normal) {
	_report_error(_mesh_storage.immediate_normal(p_immediate, p_normal), "immediate_normal");
}

void RasterizerStorageFixedFunction::immediate_tangent(RID p_immediate, const Plane &p_tangent) {
	_report_error(_mesh_storage.immediate_tangent(p_immediate, p_tangent), "immediate_tangent");
}

void RasterizerStorageFixedFunction::immediate_color(RID p_immediate, const Color &p_color) {
	_report_error(_mesh_storage.immediate_color(p_immediate, p_color), "immediate_color");
}

void RasterizerStorageFixedFunction::immediate_uv(RID p_immediate, const Vector2 &tex_uv) {
	_report_error(_mesh_storage.immediate_uv(p_immediate, tex_uv), "immediate_uv");
}

void RasterizerStorageFixedFunction::immediate_uv2(RID p_immediate, const Vector2 &tex_uv) {
	_report_error(_mesh_storage.immediate_uv(p_immediate, tex_uv, true), "immediate_uv2");
}

void RasterizerStorageFixedFunction::immediate_end(RID p_immediate) {
	_report_error(_mesh_storage.immediate_end(p_immediate), "immediate_end");
}

void RasterizerStorageFixedFunction::immediate_clear(RID p_immediate) {
	_report_error(_mesh_storage.immediate_clear(p_immediate), "immediate_clear");
}

void RasterizerStorageFixedFunction::immediate_set_material(RID p_immediate, RID p_material) {
	_report_error(_mesh_storage.immediate_set_material(p_immediate, p_material), "immediate_set_material");
}

RID RasterizerStorageFixedFunction::immediate_get_material(RID p_immediate) const {
	return _mesh_storage.immediate_get_material(p_immediate);
}

AABB RasterizerStorageFixedFunction::immediate_get_aabb(RID p_immediate) const {
	AABB bounds;
	const Error error = _mesh_storage.immediate_get_aabb(p_immediate, bounds);
	ERR_FAIL_COND_V_MSG(error != OK, AABB(), "Invalid Fixed Function immediate bounds query.");
	return bounds;
}
