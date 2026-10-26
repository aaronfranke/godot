#include "renderer_gi_fixed_function.h"

void RendererGIFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP) {
		ERR_PRINT(String("Fixed Function RendererGI ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

void RendererGIFixedFunction::_unsupported(const char *p_operation) {
	ERR_PRINT(String("Fixed Function RendererGI ") + p_operation + " is unavailable.");
}

RID RendererGIFixedFunction::voxel_gi_allocate() {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_allocate");
	return RID();
}

void RendererGIFixedFunction::voxel_gi_free(RID p_rid) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_free");
}

void RendererGIFixedFunction::voxel_gi_initialize(RID p_rid) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_initialize");
}

void RendererGIFixedFunction::voxel_gi_allocate_data(RID p_voxel_gi, const Transform3D &p_to_cell_xform, const AABB &p_aabb, const Vector3i &p_octree_size, const Vector<uint8_t> &p_octree_cells, const Vector<uint8_t> &p_data_cells, const Vector<uint8_t> &p_distance_field, const Vector<int> &p_level_counts) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_allocate_data");
}

AABB RendererGIFixedFunction::voxel_gi_get_bounds(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_bounds");
	return AABB();
}

Vector3i RendererGIFixedFunction::voxel_gi_get_octree_size(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_octree_size");
	return Vector3i();
}

Vector<uint8_t> RendererGIFixedFunction::voxel_gi_get_octree_cells(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_octree_cells");
	return Vector<uint8_t>();
}

Vector<uint8_t> RendererGIFixedFunction::voxel_gi_get_data_cells(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_data_cells");
	return Vector<uint8_t>();
}

Vector<uint8_t> RendererGIFixedFunction::voxel_gi_get_distance_field(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_distance_field");
	return Vector<uint8_t>();
}

Vector<int> RendererGIFixedFunction::voxel_gi_get_level_counts(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_level_counts");
	return Vector<int>();
}

Transform3D RendererGIFixedFunction::voxel_gi_get_to_cell_xform(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_to_cell_xform");
	return Transform3D();
}

void RendererGIFixedFunction::voxel_gi_set_dynamic_range(RID p_voxel_gi, float p_range) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_set_dynamic_range");
}

float RendererGIFixedFunction::voxel_gi_get_dynamic_range(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_dynamic_range");
	return 0;
}

void RendererGIFixedFunction::voxel_gi_set_propagation(RID p_voxel_gi, float p_range) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_set_propagation");
}

float RendererGIFixedFunction::voxel_gi_get_propagation(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_propagation");
	return 0;
}

void RendererGIFixedFunction::voxel_gi_set_energy(RID p_voxel_gi, float p_energy) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_set_energy");
}

float RendererGIFixedFunction::voxel_gi_get_energy(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_energy");
	return 0;
}

void RendererGIFixedFunction::voxel_gi_set_baked_exposure_normalization(RID p_voxel_gi, float p_baked_exposure) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_set_baked_exposure_normalization");
}

float RendererGIFixedFunction::voxel_gi_get_baked_exposure_normalization(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_baked_exposure_normalization");
	return 0;
}

void RendererGIFixedFunction::voxel_gi_set_bias(RID p_voxel_gi, float p_bias) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_set_bias");
}

float RendererGIFixedFunction::voxel_gi_get_bias(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_bias");
	return 0;
}

void RendererGIFixedFunction::voxel_gi_set_normal_bias(RID p_voxel_gi, float p_range) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_set_normal_bias");
}

float RendererGIFixedFunction::voxel_gi_get_normal_bias(RID p_voxel_gi) const {
	_unsupported("voxel_gi_get_normal_bias");
	return 0;
}

void RendererGIFixedFunction::voxel_gi_set_interior(RID p_voxel_gi, bool p_enable) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_set_interior");
}

bool RendererGIFixedFunction::voxel_gi_is_interior(RID p_voxel_gi) const {
	_unsupported("voxel_gi_is_interior");
	return false;
}

void RendererGIFixedFunction::voxel_gi_set_use_two_bounces(RID p_voxel_gi, bool p_enable) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_set_use_two_bounces");
}

bool RendererGIFixedFunction::voxel_gi_is_using_two_bounces(RID p_voxel_gi) const {
	_unsupported("voxel_gi_is_using_two_bounces");
	return false;
}

uint32_t RendererGIFixedFunction::voxel_gi_get_version(RID p_probe) const {
	_unsupported("voxel_gi_get_version");
	return 0;
}

void RendererGIFixedFunction::sdfgi_reset() {
	// Resetting an unallocated optional feature is harmless housekeeping.
	_last_error = OK;
}

RendererGIFixedFunction::RendererGIFixedFunction() : _last_error(OK) {}
