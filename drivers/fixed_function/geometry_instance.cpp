#include "geometry_instance.h"

void FixedFunctionGeometryInstance::_mark_dirty() {
	data->dirty_dependencies = true;
}

void FixedFunctionGeometryInstance::_dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
	static_cast<FixedFunctionGeometryInstance *>(p_tracker->userdata)->_mark_dirty();
}

void FixedFunctionGeometryInstance::_dependency_deleted(const RID &p_dependency, DependencyTracker *p_tracker) {
	FixedFunctionGeometryInstance *instance = static_cast<FixedFunctionGeometryInstance *>(p_tracker->userdata);
	if (instance->data->base == p_dependency) {
		instance->data->base = RID();
	}
	if (instance->data->material_override == p_dependency) {
		instance->data->material_override = RID();
	}
	if (instance->data->material_overlay == p_dependency) {
		instance->data->material_overlay = RID();
	}
	for (int i = 0; i < instance->data->surface_materials.size(); i++) {
		if (instance->data->surface_materials[i] == p_dependency) {
			instance->data->surface_materials.set(i, RID());
		}
	}
	instance->_mark_dirty();
}

Error FixedFunctionGeometryInstance::update_dependencies() {
	if (!data->dirty_dependencies) {
		return OK;
	}
	DependencyTracker &tracker = data->dependency_tracker;
	tracker.update_begin();
	FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = _context.get_material_storage();
	RID mesh = data->base;
	if (meshes.owns_multimesh(mesh)) {
		meshes.multimesh_update_dependency(mesh, &tracker);
		mesh = meshes.multimesh_get_mesh(mesh);
	}
	if (meshes.owns_mesh(mesh)) {
		meshes.mesh_update_dependency(mesh, &tracker);
		for (int i = 0; i < meshes.mesh_get_surface_count(mesh); i++) {
			const RID material = meshes.mesh_surface_get_material(mesh, i);
			if (materials.owns_material(material)) {
				materials.material_update_dependency(material, &tracker);
			}
		}
	}
	for (RID material : data->surface_materials) {
		if (materials.owns_material(material)) {
			materials.material_update_dependency(material, &tracker);
		}
	}
	if (materials.owns_material(data->material_override)) {
		materials.material_update_dependency(data->material_override, &tracker);
	}
	if (materials.owns_material(data->material_overlay)) {
		materials.material_update_dependency(data->material_overlay, &tracker);
	}
	tracker.update_end();
	data->dirty_dependencies = false;
	return OK;
}

// Keep drawing these meshes with the regular material and lighting paths.
void FixedFunctionGeometryInstance::set_use_lightmap(RID p_lightmap_instance, const Rect2 &p_lightmap_uv_scale, int p_lightmap_slice_index) {}
void FixedFunctionGeometryInstance::set_lightmap_capture(const Color *p_sh9) {}
void FixedFunctionGeometryInstance::clear_light_instances() {
	_paired_features &= ~PAIRED_LIGHT;
}
void FixedFunctionGeometryInstance::pair_light_instance(const RID p_light_instance, RSE::LightType p_type, uint32_t p_placement) {
	if (p_light_instance.is_valid()) {
		_paired_features |= PAIRED_LIGHT;
	}
}
void FixedFunctionGeometryInstance::pair_reflection_probe_instances(const RID *p_instances, uint32_t p_count) {
	_paired_features = (_paired_features & ~PAIRED_REFLECTION) | (p_count ? PAIRED_REFLECTION : 0);
}
void FixedFunctionGeometryInstance::pair_decal_instances(const RID *p_instances, uint32_t p_count) {
	_paired_features = (_paired_features & ~PAIRED_DECAL) | (p_count ? PAIRED_DECAL : 0);
}
void FixedFunctionGeometryInstance::pair_voxel_gi_instances(const RID *p_instances, uint32_t p_count) {
	_paired_features = (_paired_features & ~PAIRED_VOXEL_GI) | (p_count ? PAIRED_VOXEL_GI : 0);
}
void FixedFunctionGeometryInstance::set_softshadow_projector_pairing(bool p_softshadow, bool p_projector) {
	_paired_features = (_paired_features & ~PAIRED_SHADOW_OR_PROJECTOR) | ((p_softshadow || p_projector) ? PAIRED_SHADOW_OR_PROJECTOR : 0);
}

FixedFunctionGeometryInstance::FixedFunctionGeometryInstance(FixedFunctionRenderContext &p_context, RID p_base) : _context(p_context), _paired_features(0) {
	data = memnew(RenderGeometryInstanceBase::Data);
	data->base = p_base;
	data->base_type = _context.get_mesh_storage().owns_multimesh(p_base) ? RSE::INSTANCE_MULTIMESH : RSE::INSTANCE_MESH;
	data->dependency_tracker.userdata = this;
	data->dependency_tracker.changed_callback = _dependency_changed;
	data->dependency_tracker.deleted_callback = _dependency_deleted;
	_mark_dirty();
	update_dependencies();
}
FixedFunctionGeometryInstance::~FixedFunctionGeometryInstance() {
	memdelete(data);
}
