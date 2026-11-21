#include "renderer_mesh_storage_fixed_function.h"

void RendererMeshStorageFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP) {
		ERR_PRINT(String("Fixed Function RendererMeshStorage ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

void RendererMeshStorageFixedFunction::_unsupported(const char *p_operation) {
	ERR_PRINT(String("Fixed Function RendererMeshStorage ") + p_operation + " is unavailable.");
}

RID RendererMeshStorageFixedFunction::mesh_allocate() {
	return _meshes.mesh_create();
}

void RendererMeshStorageFixedFunction::mesh_initialize(RID p_rid) {
	_report_error(_meshes.owns_mesh(p_rid) ? OK : ERR_INVALID_PARAMETER, "mesh_initialize");
}

void RendererMeshStorageFixedFunction::mesh_free(RID p_rid) {
	_report_error(_meshes.free(p_rid), "mesh_free");
}

void RendererMeshStorageFixedFunction::mesh_set_blend_shape_count(RID p_mesh, int p_blend_shape_count) {
	_report_error(_meshes.mesh_set_blend_shape_count(p_mesh, p_blend_shape_count), "mesh_set_blend_shape_count");
}

void RendererMeshStorageFixedFunction::mesh_add_surface(RID p_mesh, const RenderingServerTypes::SurfaceData &p_surface) {
	_report_error(_meshes.mesh_add_surface(p_mesh, p_surface), "mesh_add_surface");
}

int RendererMeshStorageFixedFunction::mesh_get_blend_shape_count(RID p_mesh) const {
	return _meshes.mesh_get_blend_shape_count(p_mesh);
}

void RendererMeshStorageFixedFunction::mesh_set_blend_shape_mode(RID p_mesh, RSE::BlendShapeMode p_mode) {
	_report_error(_meshes.mesh_set_blend_shape_mode(p_mesh, p_mode), "mesh_set_blend_shape_mode");
}

RSE::BlendShapeMode RendererMeshStorageFixedFunction::mesh_get_blend_shape_mode(RID p_mesh) const {
	return _meshes.mesh_get_blend_shape_mode(p_mesh);
}

void RendererMeshStorageFixedFunction::mesh_surface_update_vertex_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	_report_error(_meshes.mesh_surface_update_region(p_mesh, p_surface, FixedFunctionMeshStorage::BUFFER_VERTEX, p_offset, p_data), "mesh_surface_update_vertex_region");
}

void RendererMeshStorageFixedFunction::mesh_surface_update_attribute_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	_report_error(_meshes.mesh_surface_update_region(p_mesh, p_surface, FixedFunctionMeshStorage::BUFFER_ATTRIBUTE, p_offset, p_data), "mesh_surface_update_attribute_region");
}

void RendererMeshStorageFixedFunction::mesh_surface_update_skin_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	_report_error(_meshes.mesh_surface_update_region(p_mesh, p_surface, FixedFunctionMeshStorage::BUFFER_SKIN, p_offset, p_data), "mesh_surface_update_skin_region");
}

void RendererMeshStorageFixedFunction::mesh_surface_update_index_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
	_report_error(_meshes.mesh_surface_update_region(p_mesh, p_surface, FixedFunctionMeshStorage::BUFFER_INDEX, p_offset, p_data), "mesh_surface_update_index_region");
}

void RendererMeshStorageFixedFunction::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) {
	_report_error(_meshes.mesh_surface_set_material(p_mesh, p_surface, p_material), "mesh_surface_set_material");
}

RID RendererMeshStorageFixedFunction::mesh_surface_get_material(RID p_mesh, int p_surface) const {
	return _meshes.mesh_surface_get_material(p_mesh, p_surface);
}

RenderingServerTypes::SurfaceData RendererMeshStorageFixedFunction::mesh_get_surface(RID p_mesh, int p_surface) const {
	FixedFunctionMeshStorage::FFSurfaceData data;
	_meshes.mesh_surface_get_data(p_mesh, p_surface, data);
	return data.source;
}

RID RendererMeshStorageFixedFunction::mesh_surface_get_vertex_buffer_rd_rid(RID p_mesh, int p_surface) const {
	return RID(); // No RenderingDevice buffer is allocated by this renderer.
}

RID RendererMeshStorageFixedFunction::mesh_surface_get_attribute_buffer_rd_rid(RID p_mesh, int p_surface) const {
	return RID(); // No RenderingDevice buffer is allocated by this renderer.
}

RID RendererMeshStorageFixedFunction::mesh_surface_get_skin_buffer_rd_rid(RID p_mesh, int p_surface) const {
	return RID(); // No RenderingDevice buffer is allocated by this renderer.
}

RID RendererMeshStorageFixedFunction::mesh_surface_get_index_buffer_rd_rid(RID p_mesh, int p_surface) const {
	return RID(); // No RenderingDevice buffer is allocated by this renderer.
}

int RendererMeshStorageFixedFunction::mesh_get_surface_count(RID p_mesh) const {
	return _meshes.mesh_get_surface_count(p_mesh);
}

void RendererMeshStorageFixedFunction::mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) {
	_report_error(_meshes.mesh_set_custom_aabb(p_mesh, p_aabb), "mesh_set_custom_aabb");
}

AABB RendererMeshStorageFixedFunction::mesh_get_custom_aabb(RID p_mesh) const {
	return _meshes.mesh_get_custom_aabb(p_mesh);
}

AABB RendererMeshStorageFixedFunction::mesh_get_aabb(RID p_mesh, RID p_skeleton) {
	AABB aabb;
	_report_error(_meshes.mesh_get_aabb(p_mesh, p_skeleton, aabb), "mesh_get_aabb");
	return aabb;
}

void RendererMeshStorageFixedFunction::mesh_set_path(RID p_mesh, const String &p_path) {
	_report_error(_meshes.mesh_set_path(p_mesh, p_path), "mesh_set_path");
}

String RendererMeshStorageFixedFunction::mesh_get_path(RID p_mesh) const {
	return _meshes.mesh_get_path(p_mesh);
}

void RendererMeshStorageFixedFunction::mesh_set_shadow_mesh(RID p_mesh, RID p_shadow_mesh) {
	_report_error(_meshes.mesh_set_shadow_mesh(p_mesh, p_shadow_mesh), "mesh_set_shadow_mesh");
}

void RendererMeshStorageFixedFunction::mesh_surface_remove(RID p_mesh, int p_surface) {
	_report_error(_meshes.mesh_remove_surface(p_mesh, p_surface), "mesh_surface_remove");
}

void RendererMeshStorageFixedFunction::mesh_clear(RID p_mesh) {
	_report_error(_meshes.mesh_clear(p_mesh), "mesh_clear");
}

void RendererMeshStorageFixedFunction::mesh_debug_usage(List<RenderingServerTypes::MeshInfo> *r_info) {
	_meshes.mesh_debug_usage(r_info);
}

bool RendererMeshStorageFixedFunction::mesh_needs_instance(RID p_mesh, bool p_has_skeleton) {
	return p_has_skeleton || _meshes.mesh_get_blend_shape_count(p_mesh) > 0;
}

RID RendererMeshStorageFixedFunction::mesh_instance_create(RID p_base) {
	return _meshes.mesh_instance_create(p_base);
}

void RendererMeshStorageFixedFunction::mesh_instance_free(RID p_rid) {
	_report_error(_meshes.free(p_rid), "mesh_instance_free");
}

void RendererMeshStorageFixedFunction::mesh_instance_set_skeleton(RID p_mesh_instance, RID p_skeleton) {
	_report_error(_meshes.mesh_instance_set_skeleton(p_mesh_instance, p_skeleton), "mesh_instance_set_skeleton");
}

void RendererMeshStorageFixedFunction::mesh_instance_set_blend_shape_weight(RID p_mesh_instance, int p_shape, float p_weight) {
	_report_error(_meshes.mesh_instance_set_blend_shape_weight(p_mesh_instance, p_shape, p_weight), "mesh_instance_set_blend_shape_weight");
}

void RendererMeshStorageFixedFunction::mesh_instance_check_for_update(RID p_mesh_instance) {
	_report_error(_meshes.owns_mesh_instance(p_mesh_instance) ? OK : ERR_INVALID_PARAMETER, "mesh_instance_check_for_update");
}

void RendererMeshStorageFixedFunction::mesh_instance_set_canvas_item_transform(RID p_mesh_instance, const Transform2D &p_transform) {
	_report_error(_meshes.mesh_instance_set_canvas_transform(p_mesh_instance, p_transform), "mesh_instance_set_canvas_item_transform");
}

void RendererMeshStorageFixedFunction::update_mesh_instances() {
	_last_error = OK; // CPU deformation uses the latest pose during draw preparation.
}

void RendererMeshStorageFixedFunction::multimesh_allocate_data(RID p_multimesh, int p_instances, RSE::MultimeshTransformFormat p_transform_format, bool p_use_colors, bool p_use_custom_data, bool p_use_indirect) {
	if (p_use_indirect || (p_transform_format != RSE::MULTIMESH_TRANSFORM_2D && p_transform_format != RSE::MULTIMESH_TRANSFORM_3D)) {
		_report_error(ERR_UNAVAILABLE, "multimesh_allocate_data");
		return;
	}
	const int transform_size = p_transform_format == RSE::MULTIMESH_TRANSFORM_2D ? 8 : 12;
	if (p_instances < 0 || p_instances > INT32_MAX / (transform_size + (p_use_colors ? 4 : 0) + (p_use_custom_data ? 4 : 0))) {
		_report_error(ERR_INVALID_PARAMETER, "multimesh_allocate_data");
		return;
	}
	RendererMeshStorage::multimesh_allocate_data(p_multimesh, p_instances, p_transform_format, p_use_colors, p_use_custom_data, p_use_indirect);
}

RID RendererMeshStorageFixedFunction::_multimesh_allocate() {
	return _meshes.multimesh_allocate();
}

void RendererMeshStorageFixedFunction::_multimesh_initialize(RID p_rid) {
	_meshes.multimesh_initialize(p_rid);
}

void RendererMeshStorageFixedFunction::_multimesh_free(RID p_rid) {
	_meshes.multimesh_free(p_rid);
}

void RendererMeshStorageFixedFunction::_multimesh_allocate_data(RID p_multimesh, int p_instances, RSE::MultimeshTransformFormat p_transform_format, bool p_use_colors, bool p_use_custom_data, bool p_use_indirect) {
	_report_error(_meshes.multimesh_allocate_data(p_multimesh, p_instances, p_use_colors, p_use_custom_data, p_transform_format == RSE::MULTIMESH_TRANSFORM_2D), "multimesh_allocate_data");
}

int RendererMeshStorageFixedFunction::_multimesh_get_instance_count(RID p_multimesh) const {
	return _meshes.multimesh_get_instance_count(p_multimesh);
}

void RendererMeshStorageFixedFunction::_multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
	_report_error(_meshes.multimesh_set_mesh(p_multimesh, p_mesh), "multimesh_set_mesh");
}

void RendererMeshStorageFixedFunction::_multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) {
	_report_error(_meshes.multimesh_set_transform(p_multimesh, p_index, p_transform), "multimesh_instance_set_transform");
}

void RendererMeshStorageFixedFunction::_multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
	_report_error(_meshes.multimesh_set_transform_2d(p_multimesh, p_index, p_transform), "multimesh_instance_set_transform_2d");
}

void RendererMeshStorageFixedFunction::_multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) {
	_report_error(_meshes.multimesh_set_channel(p_multimesh, p_index, p_color, false), "multimesh_instance_set_color");
}

void RendererMeshStorageFixedFunction::_multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) {
	_report_error(_meshes.multimesh_set_channel(p_multimesh, p_index, p_color, true), "multimesh_instance_set_custom_data");
}

void RendererMeshStorageFixedFunction::_multimesh_set_custom_aabb(RID p_multimesh, const AABB &p_aabb) {
	_report_error(_meshes.multimesh_set_custom_aabb(p_multimesh, p_aabb), "multimesh_set_custom_aabb");
}

AABB RendererMeshStorageFixedFunction::_multimesh_get_custom_aabb(RID p_multimesh) const {
	return _meshes.multimesh_get_custom_aabb(p_multimesh);
}

RID RendererMeshStorageFixedFunction::_multimesh_get_mesh(RID p_multimesh) const {
	return _meshes.multimesh_get_mesh(p_multimesh);
}

Transform3D RendererMeshStorageFixedFunction::_multimesh_instance_get_transform(RID p_multimesh, int p_index) const {
	return _meshes.multimesh_get_transform(p_multimesh, p_index);
}

Transform2D RendererMeshStorageFixedFunction::_multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const {
	return _meshes.multimesh_get_transform_2d(p_multimesh, p_index);
}

Color RendererMeshStorageFixedFunction::_multimesh_instance_get_color(RID p_multimesh, int p_index) const {
	return _meshes.multimesh_get_channel(p_multimesh, p_index, false);
}

Color RendererMeshStorageFixedFunction::_multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const {
	return _meshes.multimesh_get_channel(p_multimesh, p_index, true);
}

void RendererMeshStorageFixedFunction::_multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer) {
	_report_error(_meshes.multimesh_set_buffer(p_multimesh, p_buffer), "multimesh_set_buffer");
}

RID RendererMeshStorageFixedFunction::_multimesh_get_command_buffer_rd_rid(RID p_multimesh) const {
	_unsupported("_multimesh_get_command_buffer_rd_rid");
	return RID();
}

RID RendererMeshStorageFixedFunction::_multimesh_get_buffer_rd_rid(RID p_multimesh) const {
	_unsupported("_multimesh_get_buffer_rd_rid");
	return RID();
}

Vector<float> RendererMeshStorageFixedFunction::_multimesh_get_buffer(RID p_multimesh) const {
	return _meshes.multimesh_get_buffer(p_multimesh);
}

void RendererMeshStorageFixedFunction::_multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
	_report_error(_meshes.multimesh_set_visible_instances(p_multimesh, p_visible), "multimesh_set_visible_instances");
}

int RendererMeshStorageFixedFunction::_multimesh_get_visible_instances(RID p_multimesh) const {
	return _meshes.multimesh_get_visible_instances(p_multimesh);
}

AABB RendererMeshStorageFixedFunction::_multimesh_get_aabb(RID p_multimesh) {
	return _meshes.multimesh_get_aabb(p_multimesh);
}

RendererMeshStorage::MultiMeshInterpolator *RendererMeshStorageFixedFunction::_multimesh_get_interpolator(RID p_multimesh) const {
	return _meshes.multimesh_get_interpolator(p_multimesh);
}

RID RendererMeshStorageFixedFunction::skeleton_allocate() {
	return _meshes.skeleton_allocate();
}

void RendererMeshStorageFixedFunction::skeleton_initialize(RID p_rid) {
	_meshes.skeleton_initialize(p_rid);
}

void RendererMeshStorageFixedFunction::skeleton_free(RID p_rid) {
	_meshes.skeleton_free(p_rid);
}

void RendererMeshStorageFixedFunction::skeleton_allocate_data(RID p_skeleton, int p_bones, bool p_2d_skeleton) {
	_report_error(_meshes.skeleton_allocate_data(p_skeleton, p_bones, p_2d_skeleton), "skeleton_allocate_data");
}

int RendererMeshStorageFixedFunction::skeleton_get_bone_count(RID p_skeleton) const {
	return _meshes.skeleton_get_bone_count(p_skeleton);
}

void RendererMeshStorageFixedFunction::skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform3D &p_transform) {
	_report_error(_meshes.skeleton_bone_set_transform(p_skeleton, p_bone, p_transform), "skeleton_bone_set_transform");
}

Transform3D RendererMeshStorageFixedFunction::skeleton_bone_get_transform(RID p_skeleton, int p_bone) const {
	return _meshes.skeleton_bone_get_transform(p_skeleton, p_bone);
}

void RendererMeshStorageFixedFunction::skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) {
	_report_error(_meshes.skeleton_bone_set_transform_2d(p_skeleton, p_bone, p_transform), "skeleton_bone_set_transform_2d");
}

Transform2D RendererMeshStorageFixedFunction::skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const {
	return _meshes.skeleton_bone_get_transform_2d(p_skeleton, p_bone);
}

void RendererMeshStorageFixedFunction::skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) {
	_report_error(_meshes.skeleton_set_base_transform_2d(p_skeleton, p_base_transform), "skeleton_set_base_transform_2d");
}

void RendererMeshStorageFixedFunction::skeleton_update_dependency(RID p_base, DependencyTracker *p_instance) {
	_meshes.skeleton_update_dependency(p_base, p_instance);
}

RendererMeshStorageFixedFunction::RendererMeshStorageFixedFunction(FixedFunctionRenderContext &p_context) : _meshes(p_context.get_mesh_storage()), _last_error(OK) {}
