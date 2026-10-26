#include "rasterizer_storage_fixed_function.h"

RID RasterizerStorageFixedFunction::_multimesh_create() {
	const RID multimesh = _mesh_storage.multimesh_create();
	_report_error(multimesh.is_valid() ? OK : ERR_CANT_CREATE, "multimesh_create");
	return multimesh;
}

void RasterizerStorageFixedFunction::_multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, VS::MultimeshColorFormat p_color_format, VS::MultimeshCustomDataFormat p_data) {
	_report_error(_mesh_storage.multimesh_allocate(p_multimesh, p_instances, p_color_format, p_data, p_transform_format), "multimesh_allocate");
}

int RasterizerStorageFixedFunction::_multimesh_get_instance_count(RID p_multimesh) const {
	return _mesh_storage.multimesh_get_instance_count(p_multimesh);
}

void RasterizerStorageFixedFunction::_multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
	_report_error(_mesh_storage.multimesh_set_mesh(p_multimesh, p_mesh), "multimesh_set_mesh");
}

RID RasterizerStorageFixedFunction::_multimesh_get_mesh(RID p_multimesh) const {
	return _mesh_storage.multimesh_get_mesh(p_multimesh);
}

void RasterizerStorageFixedFunction::_multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform) {
	const bool is_3d = _mesh_storage.multimesh_get_transform_format(p_multimesh) == VS::MULTIMESH_TRANSFORM_3D;
	_report_error(is_3d ? _mesh_storage.multimesh_set_transform(p_multimesh, p_index, p_transform) : ERR_INVALID_PARAMETER, "multimesh_instance_set_transform");
}

Transform RasterizerStorageFixedFunction::_multimesh_instance_get_transform(RID p_multimesh, int p_index) const {
	ERR_FAIL_COND_V(_mesh_storage.multimesh_get_transform_format(p_multimesh) != VS::MULTIMESH_TRANSFORM_3D, Transform());
	return _mesh_storage.multimesh_get_transform(p_multimesh, p_index);
}

void RasterizerStorageFixedFunction::_multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
	const bool is_2d = _mesh_storage.multimesh_get_transform_format(p_multimesh) == VS::MULTIMESH_TRANSFORM_2D;
	Transform transform;
	transform.basis[0] = Vector3(p_transform[0].x, p_transform[1].x, 0);
	transform.basis[1] = Vector3(p_transform[0].y, p_transform[1].y, 0);
	transform.origin = Vector3(p_transform[2].x, p_transform[2].y, 0);
	_report_error(is_2d ? _mesh_storage.multimesh_set_transform(p_multimesh, p_index, transform) : ERR_INVALID_PARAMETER, "multimesh_instance_set_transform_2d");
}

Transform2D RasterizerStorageFixedFunction::_multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const {
	ERR_FAIL_COND_V(_mesh_storage.multimesh_get_transform_format(p_multimesh) != VS::MULTIMESH_TRANSFORM_2D, Transform2D());
	return FixedFunctionMeshStorage::multimesh_to_transform_2d(_mesh_storage.multimesh_get_transform(p_multimesh, p_index));
}

void RasterizerStorageFixedFunction::multimesh_attach_canvas_item(RID p_multimesh, RID p_canvas_item, bool p_attach) {
	_report_error(_mesh_storage.multimesh_attach_canvas_item(p_multimesh, p_canvas_item, p_attach), "multimesh_attach_canvas_item");
}
void RasterizerStorageFixedFunction::_multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) {
	_report_error(_mesh_storage.multimesh_set_color(p_multimesh, p_index, p_color, false), "multimesh_instance_set_color");
}

Color RasterizerStorageFixedFunction::_multimesh_instance_get_color(RID p_multimesh, int p_index) const {
	return _mesh_storage.multimesh_get_color(p_multimesh, p_index, false);
}

void RasterizerStorageFixedFunction::_multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) {
	_report_error(_mesh_storage.multimesh_set_color(p_multimesh, p_index, p_color, true), "multimesh_instance_set_custom_data");
}

Color RasterizerStorageFixedFunction::_multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const {
	return _mesh_storage.multimesh_get_color(p_multimesh, p_index, true);
}

void RasterizerStorageFixedFunction::_multimesh_set_as_bulk_array(RID p_multimesh, PoolVector<float> p_array) {
	_report_error(_mesh_storage.multimesh_set_bulk(p_multimesh, p_array), "multimesh_set_as_bulk_array");
}

void RasterizerStorageFixedFunction::multimesh_set_as_bulk_array_interpolated(RID p_multimesh, PoolVector<float> p_array, PoolVector<float> p_array_prev) {
	// Fixed Function presents the latest physics sample.
	_report_error(p_array.size() == p_array_prev.size() ? _mesh_storage.multimesh_set_bulk(p_multimesh, p_array) : ERR_INVALID_PARAMETER, "multimesh_set_as_bulk_array_interpolated");
}

void RasterizerStorageFixedFunction::_multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
	_report_error(_mesh_storage.multimesh_set_visible(p_multimesh, p_visible), "multimesh_set_visible_instances");
}

int RasterizerStorageFixedFunction::_multimesh_get_visible_instances(RID p_multimesh) const {
	return _mesh_storage.multimesh_get_visible(p_multimesh);
}

AABB RasterizerStorageFixedFunction::_multimesh_get_aabb(RID p_multimesh) const {
	AABB bounds;
	const Error error = _mesh_storage.multimesh_get_aabb(p_multimesh, bounds);
	ERR_FAIL_COND_V(error != OK, AABB());
	return bounds;
}

RasterizerStorage::MMInterpolator *RasterizerStorageFixedFunction::_multimesh_get_interpolator(RID p_multimesh) const {
	return nullptr;
}
