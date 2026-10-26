#include "rasterizer_scene_fixed_function.h"

#include "servers/visual/visual_server_raster.h"

void RasterizerSceneFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP && p_error != ERR_BUSY) {
		_render_targets.report_draw_error(p_error);
		ERR_PRINT(String("Fixed Function scene ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

RID RasterizerSceneFixedFunction::shadow_atlas_create() {
	const RID rid = _storage.shadow_atlas_create();
	_report_error(rid.is_valid() ? OK : ERR_CANT_CREATE, "shadow_atlas_create");
	return rid;
}

void RasterizerSceneFixedFunction::shadow_atlas_set_size(RID p_atlas, int p_size) {
	_report_error(_storage.shadow_atlas_set_size(p_atlas, p_size), "shadow_atlas_set_size");
}

void RasterizerSceneFixedFunction::shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) {
	_report_error(_storage.shadow_atlas_set_subdivision(p_atlas, p_quadrant, p_subdivision), "shadow_atlas_set_quadrant_subdivision");
}

bool RasterizerSceneFixedFunction::shadow_atlas_update_light(RID p_atlas, RID p_light_intance, float p_coverage, uint64_t p_light_version) {
	_report_error(ERR_UNAVAILABLE, "shadow_atlas_update_light");
	return false;
}

int RasterizerSceneFixedFunction::get_directional_light_shadow_size(RID p_light_intance) {
	if (!_light_storage.owns_instance(p_light_intance)) {
		_report_error(ERR_INVALID_PARAMETER, "get_directional_light_shadow_size");
		return 0;
	}
	return _renderer.get_directional_shadow_size();
}

void RasterizerSceneFixedFunction::set_directional_shadow_count(int p_count) {
	Error error = OK;
	if (p_count < 0) {
		error = ERR_INVALID_PARAMETER;
	} else {
		_renderer.begin_directional_shadows();
	}
	_report_error(error, "set_directional_shadow_count");
}

RID RasterizerSceneFixedFunction::environment_create() {
	const RID rid = _storage.environment_create();
	_report_error(rid.is_valid() ? OK : ERR_CANT_CREATE, "environment_create");
	return rid;
}

void RasterizerSceneFixedFunction::environment_set_background(RID p_env, VS::EnvironmentBG p_bg) {
	_report_error(_storage.environment_set_background(p_env, p_bg), "environment_set_background");
}

void RasterizerSceneFixedFunction::environment_set_sky(RID p_env, RID p_sky) {
	_report_error(_storage.environment_set_sky(p_env, p_sky), "environment_set_sky");
}

void RasterizerSceneFixedFunction::environment_set_sky_custom_fov(RID p_env, float p_scale) {
	_report_error(_storage.environment_set_sky_custom_fov(p_env, p_scale), "environment_set_sky_custom_fov");
}

void RasterizerSceneFixedFunction::environment_set_sky_orientation(RID p_env, const Basis &p_orientation) {
	_report_error(_storage.environment_set_sky_orientation(p_env, p_orientation), "environment_set_sky_orientation");
}

void RasterizerSceneFixedFunction::environment_set_bg_color(RID p_env, const Color &p_color) {
	_report_error(_storage.environment_set_color(p_env, p_color), "environment_set_bg_color");
}

void RasterizerSceneFixedFunction::environment_set_bg_energy(RID p_env, float p_energy) {
	_report_error(_storage.environment_set_energy(p_env, p_energy), "environment_set_bg_energy");
}

void RasterizerSceneFixedFunction::environment_set_canvas_max_layer(RID p_env, int p_max_layer) {
	_report_error(_storage.environment_set_canvas_max_layer(p_env, p_max_layer), "environment_set_canvas_max_layer");
}

void RasterizerSceneFixedFunction::environment_set_ambient_light(RID p_env, const Color &p_color, float p_energy, float p_sky_contribution) {
	_report_error(_storage.environment_set_ambient(p_env, p_color, p_energy, p_sky_contribution), "environment_set_ambient_light");
}

void RasterizerSceneFixedFunction::environment_set_camera_feed_id(RID p_env, int p_camera_feed_id) {
	_report_error(_storage.environment_set_camera_feed(p_env, p_camera_feed_id), "environment_set_camera_feed_id");
}

void RasterizerSceneFixedFunction::environment_set_dof_blur_near(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) {
	const Variant values[] = { p_enable, p_distance, p_transition, p_far_amount, p_quality };
	_report_error(_storage.environment_set_effect(p_env, "dof_blur_near", values, sizeof(values) / sizeof(values[0])), "environment_set_dof_blur_near");
}

void RasterizerSceneFixedFunction::environment_set_dof_blur_far(RID p_env, bool p_enable, float p_distance, float p_transition, float p_far_amount, VS::EnvironmentDOFBlurQuality p_quality) {
	const Variant values[] = { p_enable, p_distance, p_transition, p_far_amount, p_quality };
	_report_error(_storage.environment_set_effect(p_env, "dof_blur_far", values, sizeof(values) / sizeof(values[0])), "environment_set_dof_blur_far");
}

void RasterizerSceneFixedFunction::environment_set_glow(RID p_env, bool p_enable, int p_level_flags, float p_intensity, float p_strength, float p_bloom_threshold, VS::EnvironmentGlowBlendMode p_blend_mode, float p_hdr_bleed_threshold, float p_hdr_bleed_scale, float p_hdr_luminance_cap, bool p_bicubic_upscale, bool p_high_quality) {
	const Variant values[] = { p_enable, p_level_flags, p_intensity, p_strength, p_bloom_threshold, p_blend_mode, p_hdr_bleed_threshold, p_hdr_bleed_scale, p_hdr_luminance_cap, p_bicubic_upscale, p_high_quality };
	_report_error(_storage.environment_set_effect(p_env, "glow", values, sizeof(values) / sizeof(values[0])), "environment_set_glow");
}

void RasterizerSceneFixedFunction::environment_set_glow_map(RID p_env, float p_glow_map_strength, RID p_glow_map) {
	const Variant values[] = { p_glow_map_strength, p_glow_map };
	_report_error(_storage.environment_set_effect(p_env, "glow_map", values, sizeof(values) / sizeof(values[0])), "environment_set_glow_map");
}

void RasterizerSceneFixedFunction::environment_set_fog(RID p_env, bool p_enable, float p_begin, float p_end, RID p_gradient_texture) {
	const Variant values[] = { p_enable, p_begin, p_end, p_gradient_texture };
	_report_error(_storage.environment_set_effect(p_env, "fog_legacy", values, sizeof(values) / sizeof(values[0])), "environment_set_fog");
}

void RasterizerSceneFixedFunction::environment_set_ssr(RID p_env, bool p_enable, int p_max_steps, float p_fade_int, float p_fade_out, float p_depth_tolerance, bool p_roughness) {
	const Variant values[] = { p_enable, p_max_steps, p_fade_int, p_fade_out, p_depth_tolerance, p_roughness };
	_report_error(_storage.environment_set_effect(p_env, "ssr", values, sizeof(values) / sizeof(values[0])), "environment_set_ssr");
}

void RasterizerSceneFixedFunction::environment_set_ssao(RID p_env, bool p_enable, float p_radius, float p_intensity, float p_radius2, float p_intensity2, float p_bias, float p_light_affect, float p_ao_channel_affect, const Color &p_color, VS::EnvironmentSSAOQuality p_quality, VS::EnvironmentSSAOBlur p_blur, float p_bilateral_sharpness) {
	const Variant values[] = { p_enable, p_radius, p_intensity, p_radius2, p_intensity2, p_bias, p_light_affect, p_ao_channel_affect, p_color, p_quality, p_blur, p_bilateral_sharpness };
	_report_error(_storage.environment_set_effect(p_env, "ssao", values, sizeof(values) / sizeof(values[0])), "environment_set_ssao");
}

void RasterizerSceneFixedFunction::environment_set_tonemap(RID p_env, VS::EnvironmentToneMapper p_tone_mapper, float p_exposure, float p_white, bool p_auto_exposure, float p_min_luminance, float p_max_luminance, float p_auto_exp_speed, float p_auto_exp_scale) {
	const Variant values[] = { p_tone_mapper, p_exposure, p_white, p_auto_exposure, p_min_luminance, p_max_luminance, p_auto_exp_speed, p_auto_exp_scale };
	_report_error(_storage.environment_set_effect(p_env, "tonemap", values, sizeof(values) / sizeof(values[0])), "environment_set_tonemap");
}

void RasterizerSceneFixedFunction::environment_set_adjustment(RID p_env, bool p_enable, float p_brightness, float p_contrast, float p_saturation, RID p_ramp) {
	const Variant values[] = { p_enable, p_brightness, p_contrast, p_saturation, p_ramp };
	_report_error(_storage.environment_set_effect(p_env, "adjustment", values, sizeof(values) / sizeof(values[0])), "environment_set_adjustment");
}

void RasterizerSceneFixedFunction::environment_set_fog(RID p_env, bool p_enable, const Color &p_color, const Color &p_sun_color, float p_sun_amount) {
	const Variant values[] = { p_enable, p_color, p_sun_color, p_sun_amount };
	_report_error(_storage.environment_set_effect(p_env, "fog", values, sizeof(values) / sizeof(values[0])), "environment_set_fog");
}

void RasterizerSceneFixedFunction::environment_set_fog_depth(RID p_env, bool p_enable, float p_depth_begin, float p_depth_end, float p_depth_curve, bool p_transmit, float p_transmit_curve) {
	const Variant values[] = { p_enable, p_depth_begin, p_depth_end, p_depth_curve, p_transmit, p_transmit_curve };
	_report_error(_storage.environment_set_effect(p_env, "fog_depth", values, sizeof(values) / sizeof(values[0])), "environment_set_fog_depth");
}

void RasterizerSceneFixedFunction::environment_set_fog_height(RID p_env, bool p_enable, float p_min_height, float p_max_height, float p_height_curve) {
	const Variant values[] = { p_enable, p_min_height, p_max_height, p_height_curve };
	_report_error(_storage.environment_set_effect(p_env, "fog_height", values, sizeof(values) / sizeof(values[0])), "environment_set_fog_height");
}

bool RasterizerSceneFixedFunction::is_environment(RID p_env) {
	return _storage.is_environment(p_env);
}

VS::EnvironmentBG RasterizerSceneFixedFunction::environment_get_background(RID p_env) {
	return _storage.environment_get_background(p_env);
}

int RasterizerSceneFixedFunction::environment_get_canvas_max_layer(RID p_env) {
	return _storage.environment_get_canvas_max_layer(p_env);
}

RID RasterizerSceneFixedFunction::light_instance_create(RID p_light) {
	const RID instance = _light_storage.light_instance_create(p_light);
	_report_error(instance.is_valid() ? OK : ERR_INVALID_PARAMETER, "light_instance_create");
	return instance;
}

void RasterizerSceneFixedFunction::light_instance_set_transform(RID p_light_instance, const Transform &p_transform) {
	_report_error(_light_storage.light_instance_set_transform(p_light_instance, p_transform), "light_instance_set_transform");
}

void RasterizerSceneFixedFunction::light_instance_set_shadow_transform(RID p_light_instance, const CameraMatrix &p_projection, const Transform &p_transform, float p_far, float p_split, int p_pass, float p_bias_scale) {
	_report_error(_renderer.set_directional_shadow_transform(p_light_instance, p_projection, p_transform, p_pass, p_bias_scale), "light_instance_set_shadow_transform");
}

void RasterizerSceneFixedFunction::light_instance_mark_visible(RID p_light_instance) {
	_report_error(_light_storage.owns_instance(p_light_instance) ? OK : ERR_INVALID_PARAMETER, "light_instance_mark_visible");
}

RID RasterizerSceneFixedFunction::reflection_atlas_create() {
	const RID rid = _storage.reflection_atlas_create();
	_report_error(rid.is_valid() ? OK : ERR_CANT_CREATE, "reflection_atlas_create");
	return rid;
}

void RasterizerSceneFixedFunction::reflection_atlas_set_size(RID p_ref_atlas, int p_size) {
	_report_error(_storage.reflection_atlas_set_size(p_ref_atlas, p_size), "reflection_atlas_set_size");
}

void RasterizerSceneFixedFunction::reflection_atlas_set_subdivision(RID p_ref_atlas, int p_subdiv) {
	_report_error(_storage.reflection_atlas_set_subdivision(p_ref_atlas, p_subdiv), "reflection_atlas_set_subdivision");
}

RID RasterizerSceneFixedFunction::reflection_probe_instance_create(RID p_probe) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_create");
	return RID();
}

void RasterizerSceneFixedFunction::reflection_probe_instance_set_transform(RID p_instance, const Transform &p_transform) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_set_transform");
}

void RasterizerSceneFixedFunction::reflection_probe_release_atlas_index(RID p_instance) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_release_atlas_index");
}

bool RasterizerSceneFixedFunction::reflection_probe_instance_needs_redraw(RID p_instance) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_needs_redraw");
	return false;
}

bool RasterizerSceneFixedFunction::reflection_probe_instance_has_reflection(RID p_instance) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_has_reflection");
	return false;
}

bool RasterizerSceneFixedFunction::reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_begin_render");
	return false;
}

bool RasterizerSceneFixedFunction::reflection_probe_instance_postprocess_step(RID p_instance) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_postprocess_step");
	return false;
}

RID RasterizerSceneFixedFunction::gi_probe_instance_create() {
	_report_error(ERR_UNAVAILABLE, "gi_probe_instance_create");
	return RID();
}

void RasterizerSceneFixedFunction::gi_probe_instance_set_light_data(RID p_probe, RID p_base, RID p_data) {
	_report_error(ERR_UNAVAILABLE, "gi_probe_instance_set_light_data");
}

void RasterizerSceneFixedFunction::gi_probe_instance_set_transform_to_data(RID p_probe, const Transform &p_xform) {
	_report_error(ERR_UNAVAILABLE, "gi_probe_instance_set_transform_to_data");
}

void RasterizerSceneFixedFunction::gi_probe_instance_set_bounds(RID p_probe, const Vector3 &p_bounds) {
	_report_error(ERR_UNAVAILABLE, "gi_probe_instance_set_bounds");
}

void RasterizerSceneFixedFunction::render_scene(const Transform &p_cam_transform, const CameraMatrix &p_cam_projection, const int p_eye, bool p_cam_ortogonal, InstanceBase **p_cull_result, int p_cull_count, RID *p_light_cull_result, int p_light_cull_count, RID *p_reflection_probe_cull_result, int p_reflection_probe_cull_count, RID p_environment, RID p_shadow_atlas, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass) {
	Error error = _storage.validate_empty_scene(p_environment, p_shadow_atlas, p_reflection_atlas, 0, 0, p_reflection_probe_cull_count);
	// The ordinary camera path uses -1; the empty-scene path uses 0.
	if (error == OK && (p_eye != 0 || p_reflection_probe.is_valid() || (p_reflection_probe_pass != -1 && p_reflection_probe_pass != 0))) {
		error = ERR_UNAVAILABLE;
	}
	const VS::EnvironmentBG background = error == OK && p_environment.is_valid() ? _storage.environment_get_background(p_environment) : VS::ENV_BG_CLEAR_COLOR;
	if (!_render_targets.is_render_target_transparent() && (background == VS::ENV_BG_COLOR || background == VS::ENV_BG_COLOR_SKY)) {
		Color color = _storage.environment_get_color(p_environment);
		const float energy = _storage.environment_get_energy(p_environment);
		color.r = CLAMP(double(color.r) * energy, 0.0, 1.0);
		color.g = CLAMP(double(color.g) * energy, 0.0, 1.0);
		color.b = CLAMP(double(color.b) * energy, 0.0, 1.0);
		error = _render_targets.clear_render_target(color);
	}
	if (error == OK) {
		error = _renderer.render(p_cam_transform, p_cam_projection, p_cull_result, p_cull_count, p_environment, p_light_cull_result, p_light_cull_count);
		if (_renderer.is_redraw_requested()) {
			VisualServerRaster::redraw_request(false);
		}
	}
	_report_error(error, "render_scene");
}

void RasterizerSceneFixedFunction::render_shadow(RID p_light, RID p_shadow_atlas, int p_pass, InstanceBase **p_cull_result, int p_cull_count) {
	_report_error(_renderer.render_directional_shadow(p_light, p_pass, p_cull_result, p_cull_count), "render_shadow");
	if (_renderer.is_redraw_requested()) {
		VisualServerRaster::redraw_request(false);
	}
}

void RasterizerSceneFixedFunction::set_scene_pass(uint64_t p_pass) {
	_scene_pass = p_pass;
}

void RasterizerSceneFixedFunction::set_debug_draw_mode(VS::ViewportDebugDraw p_debug_draw) {
	const bool supported = p_debug_draw == VS::VIEWPORT_DEBUG_DRAW_DISABLED || p_debug_draw == VS::VIEWPORT_DEBUG_DRAW_UNSHADED;
	// The viewport sets its mode every frame. Draw an unsupported mode normally and say so once.
	_renderer.set_debug_unshaded(p_debug_draw == VS::VIEWPORT_DEBUG_DRAW_UNSHADED);
	if (!supported && !_debug_draw_warned) {
		_debug_draw_warned = true;
		WARN_PRINT("Fixed Function supports only the unshaded debug draw mode. Rendering normally.");
	}
	_report_error(OK, "set_debug_draw_mode");
}

bool RasterizerSceneFixedFunction::free(RID p_rid) {
	if (_light_storage.owns_instance(p_rid)) {
		_report_error(_light_storage.free(p_rid), "free");
		return true;
	}
	if (!_storage.owns(p_rid)) {
		return false;
	}
	_report_error(_storage.free(p_rid), "free");
	return true;
}

RasterizerSceneFixedFunction::RasterizerSceneFixedFunction(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets) :
		_last_error(OK), _scene_pass(0), _storage(p_context.get_scene_storage()), _light_storage(p_context.get_light_storage()), _render_targets(p_render_targets), _renderer(p_context, p_render_targets) {}
