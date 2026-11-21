#include "renderer_particles_storage_fixed_function.h"

RID RendererParticlesStorageFixedFunction::particles_allocate() {
	_last_error = ERR_UNAVAILABLE;
	WARN_PRINT_ONCE("Fixed Function does not support GPU particles.");
	return RID();
}

void RendererParticlesStorageFixedFunction::particles_initialize(RID p_rid) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_free(RID p_rid) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_mode(RID p_particles, RSE::ParticlesMode p_mode) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_emitting(RID p_particles, bool p_emitting) {
	_last_error = ERR_UNAVAILABLE;
}

bool RendererParticlesStorageFixedFunction::particles_get_emitting(RID p_particles) {
	return false;
}

void RendererParticlesStorageFixedFunction::particles_set_amount(RID p_particles, int p_amount) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_amount_ratio(RID p_particles, float p_amount_ratio) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_lifetime(RID p_particles, double p_lifetime) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_one_shot(RID p_particles, bool p_one_shot) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_pre_process_time(RID p_particles, double p_time) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_request_process_time(RID p_particles, real_t p_request_process_time, real_t p_request_process_time_residual) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_explosiveness_ratio(RID p_particles, real_t p_ratio) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_randomness_ratio(RID p_particles, real_t p_ratio) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_speed_scale(RID p_particles, double p_scale) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_use_local_coordinates(RID p_particles, bool p_enable) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_process_material(RID p_particles, RID p_material) {
	_last_error = ERR_UNAVAILABLE;
}

RID RendererParticlesStorageFixedFunction::particles_get_process_material(RID p_particles) const {
	return RID();
}

void RendererParticlesStorageFixedFunction::particles_set_fixed_fps(RID p_particles, int p_fps) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_interpolate(RID p_particles, bool p_enable) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_fractional_delta(RID p_particles, bool p_enable) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_collision_base_size(RID p_particles, real_t p_size) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_transform_align(RID p_particles, RSE::ParticlesTransformAlign p_transform_align) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_transform_align_channel_filter(RID p_particles, RSE::ParticlesTransformAlignCustomSrc p_channel_filter) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_transform_align_axis(RID p_particles, RSE::ParticlesTransformAlignAxis p_rotation_axis) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_seed(RID p_particles, uint32_t p_seed) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_trails(RID p_particles, bool p_enable, double p_length) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_trail_bind_poses(RID p_particles, const Vector<Transform3D> &p_bind_poses) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_restart(RID p_particles) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_subemitter(RID p_particles, RID p_subemitter_particles) {
	_last_error = ERR_UNAVAILABLE;
}

bool RendererParticlesStorageFixedFunction::particles_is_inactive(RID p_particles) const {
	return true;
}

void RendererParticlesStorageFixedFunction::particles_set_draw_order(RID p_particles, RSE::ParticlesDrawOrder p_order) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_draw_passes(RID p_particles, int p_count) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_request_process(RID p_particles) {
	_last_error = ERR_UNAVAILABLE;
}

AABB RendererParticlesStorageFixedFunction::particles_get_current_aabb(RID p_particles) {
	return AABB();
}

AABB RendererParticlesStorageFixedFunction::particles_get_aabb(RID p_particles) const {
	return AABB();
}

void RendererParticlesStorageFixedFunction::particles_set_emission_transform(RID p_particles, const Transform3D &p_transform) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_emitter_velocity(RID p_particles, const Vector3 &p_velocity) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_set_interp_to_end(RID p_particles, float p_interp_to_end) {
	_last_error = ERR_UNAVAILABLE;
}

int RendererParticlesStorageFixedFunction::particles_get_draw_passes(RID p_particles) const {
	return 0;
}

RID RendererParticlesStorageFixedFunction::particles_get_draw_pass_mesh(RID p_particles, int p_pass) const {
	return RID();
}

void RendererParticlesStorageFixedFunction::particles_set_view_axis(RID p_particles, const Vector3 &p_axis, const Vector3 &p_up_axis) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_add_collision(RID p_particles, RID p_particles_collision_instance) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_remove_collision(RID p_particles, RID p_particles_collision_instance) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::update_particles() {
	_last_error = OK;
}

RID RendererParticlesStorageFixedFunction::particles_collision_allocate() {
	_last_error = ERR_UNAVAILABLE;
	WARN_PRINT_ONCE("Fixed Function does not support GPU particle collisions.");
	return RID();
}

void RendererParticlesStorageFixedFunction::particles_collision_initialize(RID p_rid) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_free(RID p_rid) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_set_collision_type(RID p_particles_collision, RSE::ParticlesCollisionType p_type) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_set_cull_mask(RID p_particles_collision, uint32_t p_cull_mask) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_set_sphere_radius(RID p_particles_collision, real_t p_radius) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_set_box_extents(RID p_particles_collision, const Vector3 &p_extents) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_set_attractor_strength(RID p_particles_collision, real_t p_strength) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_set_attractor_directionality(RID p_particles_collision, real_t p_directionality) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_set_attractor_attenuation(RID p_particles_collision, real_t p_curve) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_set_field_texture(RID p_particles_collision, RID p_texture) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_height_field_update(RID p_particles_collision) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_set_height_field_resolution(RID p_particles_collision, RSE::ParticlesCollisionHeightfieldResolution p_resolution) {
	_last_error = ERR_UNAVAILABLE;
}

AABB RendererParticlesStorageFixedFunction::particles_collision_get_aabb(RID p_particles_collision) const {
	return AABB();
}

bool RendererParticlesStorageFixedFunction::particles_collision_is_heightfield(RID p_particles_collision) const {
	return false;
}

uint32_t RendererParticlesStorageFixedFunction::particles_collision_get_height_field_mask(RID p_particles_collision) const {
	return 0;
}

void RendererParticlesStorageFixedFunction::particles_collision_set_height_field_mask(RID p_particles_collision, uint32_t p_heightfield_mask) {
	_last_error = ERR_UNAVAILABLE;
}

uint32_t RendererParticlesStorageFixedFunction::particles_collision_get_cull_mask(RID p_particles_collision) const {
	return 0;
}

RID RendererParticlesStorageFixedFunction::particles_collision_instance_create(RID p_collision) {
	_last_error = ERR_UNAVAILABLE;
	return RID();
}

void RendererParticlesStorageFixedFunction::particles_collision_instance_free(RID p_rid) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_instance_set_transform(RID p_collision_instance, const Transform3D &p_transform) {
	_last_error = ERR_UNAVAILABLE;
}

void RendererParticlesStorageFixedFunction::particles_collision_instance_set_active(RID p_collision_instance, bool p_active) {
	_last_error = ERR_UNAVAILABLE;
}

RendererParticlesStorageFixedFunction::RendererParticlesStorageFixedFunction() : _last_error(OK) {}
