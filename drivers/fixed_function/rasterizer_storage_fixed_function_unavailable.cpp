#include "rasterizer_storage_fixed_function.h"

// These families have no owners or rendering implementation in the initial 2D profile.
// Every entry reports unavailable; return values only let callers exit safely after the error.
// Keep this separate from the implemented texture/material/profile delegates.
#define FIXED_FUNCTION_UNAVAILABLE_VOID(m_name, m_signature) \
	void RasterizerStorageFixedFunction::m_name m_signature { _report_error(ERR_UNAVAILABLE, #m_name); }
#define FIXED_FUNCTION_UNAVAILABLE_CREATE(m_type, m_name, m_signature, m_value) \
	m_type RasterizerStorageFixedFunction::m_name m_signature {                 \
		_report_error(ERR_UNAVAILABLE, #m_name);                                \
		return m_value;                                                         \
	}
#define FIXED_FUNCTION_UNAVAILABLE_QUERY(m_type, m_name, m_signature, m_value) \
	m_type RasterizerStorageFixedFunction::m_name m_signature {                \
		_report_unavailable(#m_name);                                          \
		return m_value;                                                        \
	}

// Sky

// Reflection
FIXED_FUNCTION_UNAVAILABLE_CREATE(RID, reflection_probe_create, (), RID())
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_update_mode, (RID p_probe, VS::ReflectionProbeUpdateMode p_mode))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_resolution, (RID p_probe, int p_resolution))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_intensity, (RID p_probe, float p_intensity))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_interior_ambient, (RID p_probe, const Color &p_ambient))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_interior_ambient_energy, (RID p_probe, float p_energy))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_interior_ambient_probe_contribution, (RID p_probe, float p_contrib))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_max_distance, (RID p_probe, float p_distance))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_extents, (RID p_probe, const Vector3 &p_extents))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_origin_offset, (RID p_probe, const Vector3 &p_offset))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_as_interior, (RID p_probe, bool p_enable))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_enable_box_projection, (RID p_probe, bool p_enable))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_enable_shadows, (RID p_probe, bool p_enable))
FIXED_FUNCTION_UNAVAILABLE_VOID(reflection_probe_set_cull_mask, (RID p_probe, uint32_t p_layers))
FIXED_FUNCTION_UNAVAILABLE_QUERY(AABB, reflection_probe_get_aabb, (RID p_probe) const, AABB())
FIXED_FUNCTION_UNAVAILABLE_QUERY(VS::ReflectionProbeUpdateMode, reflection_probe_get_update_mode, (RID p_probe) const, VS::REFLECTION_PROBE_UPDATE_ONCE)
FIXED_FUNCTION_UNAVAILABLE_QUERY(uint32_t, reflection_probe_get_cull_mask, (RID p_probe) const, 0)
FIXED_FUNCTION_UNAVAILABLE_QUERY(Vector3, reflection_probe_get_extents, (RID p_probe) const, Vector3())
FIXED_FUNCTION_UNAVAILABLE_QUERY(Vector3, reflection_probe_get_origin_offset, (RID p_probe) const, Vector3())
FIXED_FUNCTION_UNAVAILABLE_QUERY(float, reflection_probe_get_origin_max_distance, (RID p_probe) const, 0)
FIXED_FUNCTION_UNAVAILABLE_QUERY(bool, reflection_probe_renders_shadows, (RID p_probe) const, false)

// Scene dependencies

// Gi
FIXED_FUNCTION_UNAVAILABLE_CREATE(RID, gi_probe_create, (), RID())
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_bounds, (RID p_probe, const AABB &p_bounds))
FIXED_FUNCTION_UNAVAILABLE_QUERY(AABB, gi_probe_get_bounds, (RID p_probe) const, AABB())
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_cell_size, (RID p_probe, float p_range))
FIXED_FUNCTION_UNAVAILABLE_QUERY(float, gi_probe_get_cell_size, (RID p_probe) const, 0)
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_to_cell_xform, (RID p_probe, const Transform &p_xform))
FIXED_FUNCTION_UNAVAILABLE_QUERY(Transform, gi_probe_get_to_cell_xform, (RID p_probe) const, Transform())
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_dynamic_data, (RID p_probe, PoolVector<int> p_data))
FIXED_FUNCTION_UNAVAILABLE_QUERY(PoolVector<int>, gi_probe_get_dynamic_data, (RID p_probe) const, PoolVector<int>())
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_dynamic_range, (RID p_probe, int p_range))
FIXED_FUNCTION_UNAVAILABLE_QUERY(int, gi_probe_get_dynamic_range, (RID p_probe) const, 0)
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_energy, (RID p_probe, float p_range))
FIXED_FUNCTION_UNAVAILABLE_QUERY(float, gi_probe_get_energy, (RID p_probe) const, 0)
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_bias, (RID p_probe, float p_range))
FIXED_FUNCTION_UNAVAILABLE_QUERY(float, gi_probe_get_bias, (RID p_probe) const, 0)
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_normal_bias, (RID p_probe, float p_range))
FIXED_FUNCTION_UNAVAILABLE_QUERY(float, gi_probe_get_normal_bias, (RID p_probe) const, 0)
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_propagation, (RID p_probe, float p_range))
FIXED_FUNCTION_UNAVAILABLE_QUERY(float, gi_probe_get_propagation, (RID p_probe) const, 0)
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_interior, (RID p_probe, bool p_enable))
FIXED_FUNCTION_UNAVAILABLE_QUERY(bool, gi_probe_is_interior, (RID p_probe) const, false)
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_set_compress, (RID p_probe, bool p_enable))
FIXED_FUNCTION_UNAVAILABLE_QUERY(bool, gi_probe_is_compressed, (RID p_probe) const, false)
FIXED_FUNCTION_UNAVAILABLE_QUERY(uint32_t, gi_probe_get_version, (RID p_probe), 0)
FIXED_FUNCTION_UNAVAILABLE_CREATE(RID, gi_probe_dynamic_data_create, (int p_width, int p_height, int p_depth, GIProbeCompression p_compression), RID())
FIXED_FUNCTION_UNAVAILABLE_VOID(gi_probe_dynamic_data_update, (RID p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data))

// Lightmap
FIXED_FUNCTION_UNAVAILABLE_CREATE(RID, lightmap_capture_create, (), RID())
FIXED_FUNCTION_UNAVAILABLE_VOID(lightmap_capture_set_bounds, (RID p_capture, const AABB &p_bounds))
FIXED_FUNCTION_UNAVAILABLE_QUERY(AABB, lightmap_capture_get_bounds, (RID p_capture) const, AABB())
FIXED_FUNCTION_UNAVAILABLE_VOID(lightmap_capture_set_octree, (RID p_capture, PoolVector<uint8_t> p_octree))
FIXED_FUNCTION_UNAVAILABLE_QUERY(PoolVector<uint8_t>, lightmap_capture_get_octree, (RID p_capture) const, PoolVector<uint8_t>())
FIXED_FUNCTION_UNAVAILABLE_VOID(lightmap_capture_set_octree_cell_transform, (RID p_capture, const Transform &p_xform))
FIXED_FUNCTION_UNAVAILABLE_QUERY(Transform, lightmap_capture_get_octree_cell_transform, (RID p_capture) const, Transform())
FIXED_FUNCTION_UNAVAILABLE_VOID(lightmap_capture_set_octree_cell_subdiv, (RID p_capture, int p_subdiv))
FIXED_FUNCTION_UNAVAILABLE_QUERY(int, lightmap_capture_get_octree_cell_subdiv, (RID p_capture) const, 0)
FIXED_FUNCTION_UNAVAILABLE_VOID(lightmap_capture_set_energy, (RID p_capture, float p_energy))
FIXED_FUNCTION_UNAVAILABLE_QUERY(float, lightmap_capture_get_energy, (RID p_capture) const, 0)
FIXED_FUNCTION_UNAVAILABLE_VOID(lightmap_capture_set_interior, (RID p_capture, bool p_interior))
FIXED_FUNCTION_UNAVAILABLE_QUERY(bool, lightmap_capture_is_interior, (RID p_capture) const, false)
FIXED_FUNCTION_UNAVAILABLE_QUERY(const PoolVector<RasterizerStorage::LightmapCaptureOctree> *, lightmap_capture_get_octree_ptr, (RID p_capture) const, nullptr)

// Particles
RID RasterizerStorageFixedFunction::particles_create() {
	_last_error = ERR_UNAVAILABLE;
	WARN_PRINT_ONCE("Fixed Function does not support GPU particles.");
	return RID();
}

// Callers still configure and query particle nodes after allocation fails.
#define FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(m_name, m_signature) \
	void RasterizerStorageFixedFunction::m_name m_signature { _last_error = ERR_UNAVAILABLE; }
#define FIXED_FUNCTION_INACTIVE_PARTICLE_QUERY(m_type, m_name, m_signature, m_value) \
	m_type RasterizerStorageFixedFunction::m_name m_signature { return m_value; }

FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_emitting, (RID p_particles, bool p_emitting))
FIXED_FUNCTION_INACTIVE_PARTICLE_QUERY(bool, particles_get_emitting, (RID p_particles), false)
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_amount, (RID p_particles, int p_amount))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_lifetime, (RID p_particles, float p_lifetime))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_one_shot, (RID p_particles, bool p_one_shot))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_pre_process_time, (RID p_particles, float p_time))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_explosiveness_ratio, (RID p_particles, float p_ratio))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_randomness_ratio, (RID p_particles, float p_ratio))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_custom_aabb, (RID p_particles, const AABB &p_aabb))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_speed_scale, (RID p_particles, float p_scale))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_use_local_coordinates, (RID p_particles, bool p_enable))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_process_material, (RID p_particles, RID p_material))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_fixed_fps, (RID p_particles, int p_fps))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_fractional_delta, (RID p_particles, bool p_enable))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_restart, (RID p_particles))
FIXED_FUNCTION_INACTIVE_PARTICLE_QUERY(bool, particles_is_inactive, (RID p_particles) const, true)
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_draw_order, (RID p_particles, VS::ParticlesDrawOrder p_order))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_draw_passes, (RID p_particles, int p_count))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_draw_pass_mesh, (RID p_particles, int p_pass, RID p_mesh))
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_request_process, (RID p_particles))
FIXED_FUNCTION_INACTIVE_PARTICLE_QUERY(AABB, particles_get_current_aabb, (RID p_particles), AABB())
FIXED_FUNCTION_INACTIVE_PARTICLE_QUERY(AABB, particles_get_aabb, (RID p_particles) const, AABB())
FIXED_FUNCTION_INACTIVE_PARTICLE_VOID(particles_set_emission_transform, (RID p_particles, const Transform &p_transform))
FIXED_FUNCTION_INACTIVE_PARTICLE_QUERY(int, particles_get_draw_passes, (RID p_particles) const, 0)
FIXED_FUNCTION_INACTIVE_PARTICLE_QUERY(RID, particles_get_draw_pass_mesh, (RID p_particles, int p_pass) const, RID())

#undef FIXED_FUNCTION_INACTIVE_PARTICLE_VOID
#undef FIXED_FUNCTION_INACTIVE_PARTICLE_QUERY

// Canvas shadows and occluders
FIXED_FUNCTION_UNAVAILABLE_CREATE(RID, canvas_light_shadow_buffer_create, (int p_width), RID())
FIXED_FUNCTION_UNAVAILABLE_CREATE(RID, canvas_light_occluder_create, (), RID())
FIXED_FUNCTION_UNAVAILABLE_VOID(canvas_light_occluder_set_polylines, (RID p_occluder, PoolVector<Vector2> p_lines))

#undef FIXED_FUNCTION_UNAVAILABLE_VOID
#undef FIXED_FUNCTION_UNAVAILABLE_CREATE
#undef FIXED_FUNCTION_UNAVAILABLE_QUERY
