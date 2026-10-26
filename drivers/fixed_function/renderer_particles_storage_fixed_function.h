#pragma once

#include "drivers/fixed_function/render_context.h"
#include "servers/rendering/storage/particles_storage.h"

// Explicit restricted-profile contracts; unsupported resources have no owners or handles.
class RendererParticlesStorageFixedFunction : public RendererParticlesStorage {
	Error _last_error;

public:
	RendererParticlesStorageFixedFunction();
	Error get_last_error() const { return _last_error; }

	RID particles_allocate() override;
	void particles_initialize(RID p_rid) override;
	void particles_free(RID p_rid) override;
	void particles_set_mode(RID p_particles, RSE::ParticlesMode p_mode) override;
	void particles_set_emitting(RID p_particles, bool p_emitting) override;
	bool particles_get_emitting(RID p_particles) override;
	void particles_set_amount(RID p_particles, int p_amount) override;
	void particles_set_amount_ratio(RID p_particles, float p_amount_ratio) override;
	void particles_set_lifetime(RID p_particles, double p_lifetime) override;
	void particles_set_one_shot(RID p_particles, bool p_one_shot) override;
	void particles_set_pre_process_time(RID p_particles, double p_time) override;
	void particles_request_process_time(RID p_particles, real_t p_request_process_time, real_t p_request_process_time_residual = 0.0) override;
	void particles_set_explosiveness_ratio(RID p_particles, real_t p_ratio) override;
	void particles_set_randomness_ratio(RID p_particles, real_t p_ratio) override;
	void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) override;
	void particles_set_speed_scale(RID p_particles, double p_scale) override;
	void particles_set_use_local_coordinates(RID p_particles, bool p_enable) override;
	void particles_set_process_material(RID p_particles, RID p_material) override;
	RID particles_get_process_material(RID p_particles) const override;
	void particles_set_fixed_fps(RID p_particles, int p_fps) override;
	void particles_set_interpolate(RID p_particles, bool p_enable) override;
	void particles_set_fractional_delta(RID p_particles, bool p_enable) override;
	void particles_set_collision_base_size(RID p_particles, real_t p_size) override;
	void particles_set_transform_align(RID p_particles, RSE::ParticlesTransformAlign p_transform_align) override;
	void particles_set_transform_align_channel_filter(RID p_particles, RSE::ParticlesTransformAlignCustomSrc p_channel_filter) override;
	void particles_set_transform_align_axis(RID p_particles, RSE::ParticlesTransformAlignAxis p_rotation_axis) override;
	void particles_set_seed(RID p_particles, uint32_t p_seed) override;
	void particles_set_trails(RID p_particles, bool p_enable, double p_length) override;
	void particles_set_trail_bind_poses(RID p_particles, const Vector<Transform3D> &p_bind_poses) override;
	void particles_restart(RID p_particles) override;
	void particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) override;
	void particles_set_subemitter(RID p_particles, RID p_subemitter_particles) override;
	bool particles_is_inactive(RID p_particles) const override;
	void particles_set_draw_order(RID p_particles, RSE::ParticlesDrawOrder p_order) override;
	void particles_set_draw_passes(RID p_particles, int p_count) override;
	void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) override;
	void particles_request_process(RID p_particles) override;
	AABB particles_get_current_aabb(RID p_particles) override;
	AABB particles_get_aabb(RID p_particles) const override;
	void particles_set_emission_transform(RID p_particles, const Transform3D &p_transform) override;
	void particles_set_emitter_velocity(RID p_particles, const Vector3 &p_velocity) override;
	void particles_set_interp_to_end(RID p_particles, float p_interp_to_end) override;
	int particles_get_draw_passes(RID p_particles) const override;
	RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const override;
	void particles_set_view_axis(RID p_particles, const Vector3 &p_axis, const Vector3 &p_up_axis) override;
	void particles_add_collision(RID p_particles, RID p_particles_collision_instance) override;
	void particles_remove_collision(RID p_particles, RID p_particles_collision_instance) override;
	void update_particles() override;
	RID particles_collision_allocate() override;
	void particles_collision_initialize(RID p_rid) override;
	void particles_collision_free(RID p_rid) override;
	void particles_collision_set_collision_type(RID p_particles_collision, RSE::ParticlesCollisionType p_type) override;
	void particles_collision_set_cull_mask(RID p_particles_collision, uint32_t p_cull_mask) override;
	void particles_collision_set_sphere_radius(RID p_particles_collision, real_t p_radius) override;
	void particles_collision_set_box_extents(RID p_particles_collision, const Vector3 &p_extents) override;
	void particles_collision_set_attractor_strength(RID p_particles_collision, real_t p_strength) override;
	void particles_collision_set_attractor_directionality(RID p_particles_collision, real_t p_directionality) override;
	void particles_collision_set_attractor_attenuation(RID p_particles_collision, real_t p_curve) override;
	void particles_collision_set_field_texture(RID p_particles_collision, RID p_texture) override;
	void particles_collision_height_field_update(RID p_particles_collision) override;
	void particles_collision_set_height_field_resolution(RID p_particles_collision, RSE::ParticlesCollisionHeightfieldResolution p_resolution) override;
	AABB particles_collision_get_aabb(RID p_particles_collision) const override;
	bool particles_collision_is_heightfield(RID p_particles_collision) const override;
	uint32_t particles_collision_get_height_field_mask(RID p_particles_collision) const override;
	void particles_collision_set_height_field_mask(RID p_particles_collision, uint32_t p_heightfield_mask) override;
	uint32_t particles_collision_get_cull_mask(RID p_particles_collision) const override;
	RID particles_collision_instance_create(RID p_collision) override;
	void particles_collision_instance_free(RID p_rid) override;
	void particles_collision_instance_set_transform(RID p_collision_instance, const Transform3D &p_transform) override;
	void particles_collision_instance_set_active(RID p_collision_instance, bool p_active) override;
};
