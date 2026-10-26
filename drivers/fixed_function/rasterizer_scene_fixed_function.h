#ifndef RASTERIZER_SCENE_FIXED_FUNCTION_H
#define RASTERIZER_SCENE_FIXED_FUNCTION_H

#include "render_target_manager.h"
#include "scene_renderer.h"
#include "scene_storage.h"

// Scene boundary for unlit and Lambert meshes, skeleton poses and backgrounds.
class RasterizerSceneFixedFunction : public RasterizerScene {
	Error _last_error;
	// Fill the inherited x86_32 vptr alignment gap before the 64-bit counter.
	uint64_t _scene_pass;
	bool _debug_draw_warned = false;
	FixedFunctionSceneStorage &_storage;
	FixedFunctionLightStorage &_light_storage;
	FixedFunctionRenderTargetManager &_render_targets;
	FixedFunctionSceneRenderer _renderer;
	void _report_error(Error p_error, const char *p_operation);
	RasterizerSceneFixedFunction(const RasterizerSceneFixedFunction &);
	RasterizerSceneFixedFunction &operator=(const RasterizerSceneFixedFunction &);

public:
	RID shadow_atlas_create() override;
	void shadow_atlas_set_size(RID p_atlas, int p_size) override;
	void shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) override;
	bool shadow_atlas_update_light(RID p_atlas, RID p_light_intance, float p_coverage, uint64_t p_light_version) override;
	int get_directional_light_shadow_size(RID p_light_intance) override;
	void set_directional_shadow_count(int p_count) override;
	RID environment_create() override;
	void environment_set_background(RID p_env, VS::EnvironmentBG p_bg) override;
	void environment_set_sky(RID p_env, RID p_sky) override;
	void environment_set_sky_custom_fov(RID p_env, float p_scale) override;
	void environment_set_sky_orientation(RID p_env, const Basis &p_orientation) override;
	void environment_set_bg_color(RID p_env, const Color &p_color) override;
	void environment_set_bg_energy(RID p_env, float p_energy) override;
	void environment_set_canvas_max_layer(RID p_env, int p_max_layer) override;
	void environment_set_ambient_light(RID p_env, const Color &p_color, float p_energy, float p_sky_contribution) override;
	void environment_set_camera_feed_id(RID p_env, int p_camera_feed_id) override;
	void environment_set_dof_blur_near(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) override;
	void environment_set_dof_blur_far(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) override;
	void environment_set_glow(RID p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, VS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale, bool p_high_quality) override;
	void environment_set_glow_map(RID p_env, float p_glow_map_strength, RID p_glow_map) override;
	void environment_set_fog(RID p_env, bool p_enable, float p_begin, float p_end, RID p_gradient_texture) override;
	void environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance, bool p_roughness) override;
	void environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, VS::EnvironmentSSAOQuality p_quality, VS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) override;
	void environment_set_tonemap(RID p_env, VS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale) override;
	void environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RID p_ramp) override;
	void environment_set_fog(RID p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) override;
	void environment_set_fog_depth(RID p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) override;
	void environment_set_fog_height(RID p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) override;
	bool is_environment(RID p_env) override;
	VS::EnvironmentBG environment_get_background(RID p_env) override;
	int environment_get_canvas_max_layer(RID p_env) override;
	RID light_instance_create(RID p_light) override;
	void light_instance_set_transform(RID p_light_instance, const Transform &p_transform) override;
	void light_instance_set_shadow_transform(RID p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale) override;
	void light_instance_mark_visible(RID p_light_instance) override;
	RID reflection_atlas_create() override;
	void reflection_atlas_set_size(RID p_ref_atlas, int p_size) override;
	void reflection_atlas_set_subdivision(RID p_ref_atlas, int p_subdiv) override;
	RID reflection_probe_instance_create(RID p_probe) override;
	void reflection_probe_instance_set_transform(RID p_instance, const Transform &p_transform) override;
	void reflection_probe_release_atlas_index(RID p_instance) override;
	bool reflection_probe_instance_needs_redraw(RID p_instance) override;
	bool reflection_probe_instance_has_reflection(RID p_instance) override;
	bool reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas) override;
	bool reflection_probe_instance_postprocess_step(RID p_instance) override;
	RID gi_probe_instance_create() override;
	void gi_probe_instance_set_light_data(RID p_probe, RID p_base, RID p_data) override;
	void gi_probe_instance_set_transform_to_data(RID p_probe, const Transform &p_xform) override;
	void gi_probe_instance_set_bounds(RID p_probe, const Vector3 &p_bounds) override;
	void render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, const int p_eye, bool p_cam_ortogonal, InstanceBase **p_cull_result, int p_cull_count, RID *p_light_cull_result, int p_light_cull_count, RID *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RID p_environment, RID p_shadow_atlas, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass) override;
	void render_shadow(RID p_light, RID p_shadow_atlas, int p_pass, InstanceBase **p_cull_result, int p_cull_count) override;
	void set_scene_pass(uint64_t p_pass) override;
	void set_debug_draw_mode(VS::ViewportDebugDraw p_debug_draw) override;
	bool free(RID p_rid) override;
	bool light_instances_can_render_shadow_cube() const override { return false; }
	Error get_last_error() const { return _last_error; }
	uint64_t get_scene_pass() const { return _scene_pass; }
	RasterizerSceneFixedFunction(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets);
};

#endif // RASTERIZER_SCENE_FIXED_FUNCTION_H
