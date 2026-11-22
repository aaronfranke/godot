#include "rasterizer_storage_fixed_function.h"

RID RasterizerStorageFixedFunction::skeleton_create() {
	const RID skeleton = _mesh_storage.skeleton_create();
	_report_error(skeleton.is_valid() ? OK : ERR_CANT_CREATE, "skeleton_create");
	return skeleton;
}

void RasterizerStorageFixedFunction::skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton) {
	_report_error(_mesh_storage.skeleton_allocate(p_skeleton, p_bones, p_2d_skeleton), "skeleton_allocate");
}

int RasterizerStorageFixedFunction::skeleton_get_bone_count(RID p_skeleton) const {
	return _mesh_storage.skeleton_get_bone_count(p_skeleton);
}

void RasterizerStorageFixedFunction::skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform) {
	_report_error(_mesh_storage.skeleton_set_bone_transform(p_skeleton, p_bone, p_transform), "skeleton_bone_set_transform");
}

Transform RasterizerStorageFixedFunction::skeleton_bone_get_transform(RID p_skeleton, int p_bone) const {
	return _mesh_storage.skeleton_get_bone_transform(p_skeleton, p_bone);
}

void RasterizerStorageFixedFunction::skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) {
	_report_error(_mesh_storage.skeleton_set_bone_transform_2d(p_skeleton, p_bone, p_transform), "skeleton_bone_set_transform_2d");
}

Transform2D RasterizerStorageFixedFunction::skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const {
	return _mesh_storage.skeleton_get_bone_transform_2d(p_skeleton, p_bone);
}

void RasterizerStorageFixedFunction::skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_transform) {
	_report_error(_mesh_storage.skeleton_set_base_transform_2d(p_skeleton, p_transform), "skeleton_set_base_transform_2d");
}

void RasterizerStorageFixedFunction::skeleton_attach_canvas_item(RID p_skeleton, RID p_canvas_item, bool p_attach) {
	_report_error(_mesh_storage.skeleton_attach_canvas_item(p_skeleton, p_canvas_item, p_attach), "skeleton_attach_canvas_item");
}

uint32_t RasterizerStorageFixedFunction::skeleton_get_revision(RID p_skeleton) const {
	return _mesh_storage.skeleton_get_revision(p_skeleton);
}

void RasterizerStorageFixedFunction::instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) {
	_report_error(_mesh_storage.instance_add_skeleton(p_skeleton, p_instance), "instance_add_skeleton");
}

void RasterizerStorageFixedFunction::instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) {
	_report_error(_mesh_storage.instance_remove_skeleton(p_skeleton, p_instance), "instance_remove_skeleton");
}
