#include "renderer_scene_render_fixed_function.h"

#include "core/variant/typed_array.h"
#include "drivers/fixed_function/render_scene_buffers_fixed_function.h"
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/storage/camera_attributes_storage.h"
#include "servers/rendering/storage/texture_storage.h"

#include <float.h>

void RendererSceneRenderFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP) {
		ERR_PRINT(String("Fixed Function RendererSceneRender ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

void RendererSceneRenderFixedFunction::_unsupported(const char *p_operation) {
	ERR_PRINT(String("Fixed Function RendererSceneRender ") + p_operation + " is unavailable.");
}

RenderGeometryInstance *RendererSceneRenderFixedFunction::geometry_instance_create(RID p_base) {
	if (!_context.get_mesh_storage().owns_mesh(p_base) && !_context.get_mesh_storage().owns_multimesh(p_base)) {
		_report_error(ERR_UNAVAILABLE, "geometry_instance_create");
		return nullptr;
	}
	FixedFunctionGeometryInstance *instance = memnew(FixedFunctionGeometryInstance(_context, p_base));
	_instances.insert(instance);
	return instance;
}

void RendererSceneRenderFixedFunction::geometry_instance_free(RenderGeometryInstance *p_geometry_instance) {
	FixedFunctionGeometryInstance *instance = static_cast<FixedFunctionGeometryInstance *>(p_geometry_instance);
	if (!_instances.has(instance)) {
		_report_error(ERR_INVALID_PARAMETER, "geometry_instance_free");
		return;
	}
	_instances.erase(instance);
	memdelete(instance);
	_last_error = OK;
}

uint32_t RendererSceneRenderFixedFunction::geometry_instance_get_pair_mask() {
	// Consume the visible scene light list and rank per draw, without upstream preselection.
	return 0;
}

uint32_t RendererSceneRenderFixedFunction::get_max_lights_total() {
	return UINT32_MAX;
}

uint32_t RendererSceneRenderFixedFunction::get_max_lights_per_mesh() {
	return UINT32_MAX;
}

void RendererSceneRenderFixedFunction::mesh_generate_pipelines(RID p_mesh, bool p_background_compilation) {
	_report_error(_context.get_mesh_storage().owns_mesh(p_mesh) ? OK : ERR_INVALID_PARAMETER, "mesh_generate_pipelines");
}

uint32_t RendererSceneRenderFixedFunction::get_pipeline_compilations(RSE::PipelineSource p_source) {
	return 0;
}

void RendererSceneRenderFixedFunction::sdfgi_update(const Ref<RenderSceneBuffers> &p_render_buffers, RID p_environment, const Vector3 &p_world_position) {
	// The scene server calls this for every 3D viewport, including renderers without SDFGI.
}

int RendererSceneRenderFixedFunction::sdfgi_get_pending_region_count(const Ref<RenderSceneBuffers> &p_render_buffers) const {
	return 0;
}

AABB RendererSceneRenderFixedFunction::sdfgi_get_pending_region_bounds(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const {
	_unsupported("sdfgi_get_pending_region_bounds");
	return AABB();
}

uint32_t RendererSceneRenderFixedFunction::sdfgi_get_pending_region_cascade(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const {
	return 0;
}

RID RendererSceneRenderFixedFunction::sky_allocate() {
	return _skies.allocate_rid();
}

void RendererSceneRenderFixedFunction::sky_initialize(RID p_rid) {
	_skies.initialize_rid(p_rid);
}

void RendererSceneRenderFixedFunction::sky_set_radiance_size(RID p_sky, int p_radiance_size) {
	_report_error(_skies.owns(p_sky) ? OK : ERR_INVALID_PARAMETER, "sky_set_radiance_size");
}

void RendererSceneRenderFixedFunction::sky_set_mode(RID p_sky, RSE::SkyMode p_samples) {
	_report_error(_skies.owns(p_sky) ? OK : ERR_INVALID_PARAMETER, "sky_set_mode");
}

void RendererSceneRenderFixedFunction::sky_set_material(RID p_sky, RID p_material) {
	FFSky *sky = _skies.get_or_null(p_sky);
	if (sky) {
		sky->material = p_material;
	}
	_report_error(sky ? OK : ERR_INVALID_PARAMETER, "sky_set_material");
}

Ref<Image> RendererSceneRenderFixedFunction::sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size) {
	_report_error(ERR_UNAVAILABLE, "sky_bake_panorama");
	return Ref<Image>();
}

void RendererSceneRenderFixedFunction::environment_set_volumetric_fog_volume_size(int p_size, int p_depth) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::environment_set_volumetric_fog_filter_active(bool p_enable) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::environment_glow_set_use_bicubic_upscale(bool p_enable) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::environment_set_ssr_half_size(bool p_half_size) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::environment_set_ssr_roughness_quality(RSE::EnvironmentSSRRoughnessQuality p_quality) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::environment_set_ssao_quality(RSE::EnvironmentSSAOQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::environment_set_ssil_quality(RSE::EnvironmentSSILQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::environment_set_sdfgi_ray_count(RSE::EnvironmentSDFGIRayCount p_ray_count) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::environment_set_sdfgi_frames_to_converge(RSE::EnvironmentSDFGIFramesToConverge p_frames) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::environment_set_sdfgi_frames_to_update_light(RSE::EnvironmentSDFGIFramesToUpdateLight p_update) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

Ref<Image> RendererSceneRenderFixedFunction::environment_bake_panorama(RID p_env, bool p_bake_irradiance, const Size2i &p_size) {
	_report_error(ERR_UNAVAILABLE, "environment_bake_panorama");
	return Ref<Image>();
}

void RendererSceneRenderFixedFunction::positional_soft_shadow_filter_set_quality(RSE::ShadowQuality p_quality) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::directional_soft_shadow_filter_set_quality(RSE::ShadowQuality p_quality) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

RID RendererSceneRenderFixedFunction::fog_volume_instance_create(RID p_fog_volume) {
	_report_error(ERR_UNAVAILABLE, "fog_volume_instance_create");
	return RID();
}

void RendererSceneRenderFixedFunction::fog_volume_instance_set_transform(RID p_fog_volume_instance, const Transform3D &p_transform) {
	_report_error(ERR_UNAVAILABLE, "fog_volume_instance_set_transform");
}

void RendererSceneRenderFixedFunction::fog_volume_instance_set_active(RID p_fog_volume_instance, bool p_active) {
	_report_error(ERR_UNAVAILABLE, "fog_volume_instance_set_active");
}

RID RendererSceneRenderFixedFunction::fog_volume_instance_get_volume(RID p_fog_volume_instance) const {
	_unsupported("fog_volume_instance_get_volume");
	return RID();
}

Vector3 RendererSceneRenderFixedFunction::fog_volume_instance_get_position(RID p_fog_volume_instance) const {
	_unsupported("fog_volume_instance_get_position");
	return Vector3();
}

RID RendererSceneRenderFixedFunction::voxel_gi_instance_create(RID p_voxel_gi) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_instance_create");
	return RID();
}

void RendererSceneRenderFixedFunction::voxel_gi_instance_set_transform_to_data(RID p_probe, const Transform3D &p_xform) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_instance_set_transform_to_data");
}

bool RendererSceneRenderFixedFunction::voxel_gi_needs_update(RID p_probe) const {
	_unsupported("voxel_gi_needs_update");
	return false;
}

void RendererSceneRenderFixedFunction::voxel_gi_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) {
	_report_error(ERR_UNAVAILABLE, "voxel_gi_update");
}

void RendererSceneRenderFixedFunction::voxel_gi_set_quality(RSE::VoxelGIQuality) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::_prepare_fog(RID p_environment, const Projection &p_projection, FixedFunctionSceneRenderer::FFLightingEnvironment &r_lighting) const {
	if (!environment_get_fog_enabled(p_environment)) {
		return;
	}
	const double density = environment_get_fog_density(p_environment);
	if (!(density > 0) || Math::is_inf(density)) {
		return;
	}

	double begin = 0;
	double end;
	if (environment_get_fog_mode(p_environment) == RSE::ENV_FOG_MODE_DEPTH) {
		begin = environment_get_fog_depth_begin(p_environment);
		end = environment_get_fog_depth_end(p_environment);
		if (end <= 0) {
			end = p_projection.get_z_far();
		}
		// At the authored end, the linear fade reaches the requested density.
		end = begin + (end - begin) / density;
	} else {
		// Match the exponential profile's half-opacity distance with a linear ramp.
		end = 2.0 * Math::log(2.0) / density;
	}
	if (!(end > begin) || begin < -FLT_MAX || end > FLT_MAX || float(end) <= float(begin)) {
		return;
	}

	Color color = environment_get_fog_light_color(p_environment).srgb_to_linear();
	const float energy = environment_get_fog_light_energy(p_environment);
	for (int channel = 0; channel < 3; channel++) {
		const double component = double(color[channel]) * energy;
		if (Math::is_nan(component)) {
			return;
		}
		color[channel] = CLAMP(component, 0.0, 1.0);
	}
	r_lighting.fog_color = color.linear_to_srgb();
	r_lighting.fog_begin = float(begin);
	r_lighting.fog_end = float(end);
}

void RendererSceneRenderFixedFunction::render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, float p_window_output_max_value, const RenderSDFGIUpdateData *p_sdfgi_update_data, RenderingServerTypes::RenderInfo *r_render_info) {
	Error error = OK;
	Ref<RenderSceneBuffersFixedFunction> buffers = p_render_buffers;
	if (buffers.is_null()) {
		error = ERR_INVALID_PARAMETER;
	} else {
		// Associate unsupported scene output with its viewport before isolating the failure.
		error = _render_targets.select_render_target(buffers->get_render_target());
		if (error == OK) {
			error = buffers->get_error();
		}
	}
	// Ordinary cameras use pass -1; empty scenes use 0. Neither is a reflection request.
	// The scene server also supplies SDFGI update data when no update is requested.
	if (error == OK && (p_reflection_probes.size() || p_voxel_gi_instances.size() || p_decals.size() || p_fog_volumes.size() || p_occluder_debug_tex.is_valid() || p_reflection_probe.is_valid() || (p_reflection_probe_pass != -1 && p_reflection_probe_pass != 0) || p_render_sdfgi_region_count || (p_sdfgi_update_data && p_sdfgi_update_data->update_static) || (p_camera_data && p_camera_data->view_count != 1))) {
		error = ERR_UNAVAILABLE;
	}
	if (error == OK && p_compositor.is_valid()) {
		WARN_PRINT_ONCE("Fixed Function draws the scene without compositor effects.");
	}
	FixedFunctionSceneRenderer::FFMeshLOD mesh_lod = {};
	const FixedFunctionSceneRenderer::FFMeshLOD *lod = nullptr;
	if (!_disable_lod && p_camera_data && p_screen_mesh_lod_threshold > 0) {
		mesh_lod.camera_position = p_camera_data->main_transform.origin;
		mesh_lod.distance_multiplier = float(p_camera_data->main_projection.get_lod_multiplier());
		mesh_lod.screen_threshold = p_screen_mesh_lod_threshold;
		mesh_lod.orthogonal = p_camera_data->is_orthogonal;
		lod = &mesh_lod;
	}
	_renderer.begin_directional_shadows();
	if (error == OK && (p_render_shadow_count < 0 || (p_render_shadow_count && !p_render_shadows))) {
		error = ERR_INVALID_PARAMETER;
	}
	for (int shadow = 0; error == OK && shadow < p_render_shadow_count; shadow++) {
		const RenderShadowData &data = p_render_shadows[shadow];
		const int size = RSG::light_storage->get_directional_light_shadow_size(data.light);
		if (!size) {
			continue; // Disabled or unavailable optional shadows retain ordinary scene output.
		}
		for (uint32_t i = 0; i < data.instances.size(); i++) {
			if (!_instances.has(static_cast<FixedFunctionGeometryInstance *>(data.instances[i]))) {
				error = ERR_INVALID_PARAMETER;
				break;
			}
		}
		if (error == OK) {
			error = _renderer.render_directional_shadow(data.light, data.pass, data.instances, size, r_render_info, lod);
			if (_renderer.is_redraw_requested()) {
				RenderingServerDefault::redraw_request();
			}
		}
	}
	RSE::EnvironmentBG background = RSE::ENV_BG_CLEAR_COLOR;
	if (error == OK && p_environment.is_valid()) {
		if (!is_environment(p_environment)) {
			error = ERR_INVALID_PARAMETER;
		} else {
			background = environment_get_background(p_environment);
			// TODO: Implement optional environment effects without suppressing base geometry.
			// The LDR approximation retains their CPU settings but does not execute them.
		}
	}
	if (background != RSE::ENV_BG_CLEAR_COLOR && background != RSE::ENV_BG_COLOR && background != RSE::ENV_BG_CANVAS && background != RSE::ENV_BG_SKY) {
		error = ERR_UNAVAILABLE;
	}
	if (error == OK && !_render_targets.is_render_target_transparent() && background == RSE::ENV_BG_COLOR) {
		Color color = environment_get_bg_color(p_environment);
		const float energy = environment_get_bg_energy_multiplier(p_environment);
		color.r = CLAMP(double(color.r) * energy, 0.0, 1.0);
		color.g = CLAMP(double(color.g) * energy, 0.0, 1.0);
		color.b = CLAMP(double(color.b) * energy, 0.0, 1.0);
		error = _render_targets.clear_render_target(color);
	}
	if (error == OK && !_render_targets.is_render_target_transparent() && background == RSE::ENV_BG_SKY && p_camera_data) {
		error = _render_sky(*p_camera_data, p_environment, r_render_info);
		if (_renderer.is_redraw_requested()) {
			RenderingServerDefault::redraw_request();
		}
	}
	if (error == OK && p_instances.size()) {
		if (!p_camera_data) {
			error = ERR_INVALID_PARAMETER;
		} else {
			for (uint32_t i = 0; i < p_instances.size(); i++) {
				if (!_instances.has(static_cast<FixedFunctionGeometryInstance *>(p_instances[i]))) {
					error = ERR_INVALID_PARAMETER;
					break;
				}
			}
			if (error == OK) {
				FixedFunctionSceneRenderer::FFLightingEnvironment lighting;
				if (p_environment.is_valid()) {
					_prepare_fog(p_environment, p_camera_data->main_projection, lighting);
					const RSE::EnvironmentAmbientSource source = environment_get_ambient_source(p_environment);
					const RSE::EnvironmentBG ambient_background = environment_get_background(p_environment);
					if (source == RSE::ENV_AMBIENT_SOURCE_BG && (ambient_background == RSE::ENV_BG_CLEAR_COLOR || ambient_background == RSE::ENV_BG_COLOR)) {
						lighting.ambient = ambient_background == RSE::ENV_BG_COLOR ? environment_get_bg_color(p_environment) : RSG::texture_storage->get_default_clear_color();
						lighting.ambient_energy = environment_get_bg_energy_multiplier(p_environment);
					} else if (source != RSE::ENV_AMBIENT_SOURCE_DISABLED) {
						lighting.ambient = environment_get_ambient_light(p_environment);
						lighting.ambient_energy = environment_get_ambient_light_energy(p_environment);
						if (source == RSE::ENV_AMBIENT_SOURCE_SKY || (source == RSE::ENV_AMBIENT_SOURCE_BG && ambient_background == RSE::ENV_BG_SKY)) {
							const float contribution = CLAMP(environment_get_ambient_sky_contribution(p_environment), 0.0f, 1.0f);
							FixedFunctionSceneRenderer::FFSkyBackground sky;
							if (contribution > 0 && _get_sky_background(p_environment, sky) && _renderer.get_sky_ambient(sky, lighting.sky_ambient)) {
								lighting.sky_contribution = contribution;
								lighting.sky_energy = sky.energy;
							}
						}
					}
				} else if (!_render_targets.is_render_target_transparent()) {
					// The viewport color is available before its queued clear is consumed.
					lighting.ambient = _context.get_texture_storage().render_target_get_clear_request_color(buffers->get_render_target());
				}
				if (p_camera_attributes.is_valid()) {
					if (!RSG::camera_attributes->owns_camera_attributes(p_camera_attributes)) {
						error = ERR_INVALID_PARAMETER;
					} else {
						lighting.exposure_normalization = RSG::camera_attributes->camera_attributes_get_exposure_normalization_factor(p_camera_attributes);
					}
				}
				Vector<RID> lights;
				if (error == OK) {
					error = lights.resize(p_lights.size());
				}
				if (error == OK) {
					for (uint32_t i = 0; i < p_lights.size(); i++) {
						lights.write[i] = p_lights[i];
					}
					error = _renderer.render(p_camera_data->main_transform, p_camera_data->main_projection, p_instances, r_render_info, lighting, lights.ptr(), lights.size(), lod);
				}
				if (_renderer.is_redraw_requested()) {
					RenderingServerDefault::redraw_request();
				}
			}
		}
	}
	if (error != OK) {
		_render_targets.report_draw_error(error);
	}
	_report_error(error, "render_scene");
}

void RendererSceneRenderFixedFunction::render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) {
	_report_error(ERR_UNAVAILABLE, "render_material");
}

void RendererSceneRenderFixedFunction::render_particle_collider_heightfield(RID p_collider, const Transform3D &p_transform, const PagedArray<RenderGeometryInstance *> &p_instances) {
	_report_error(ERR_UNAVAILABLE, "render_particle_collider_heightfield");
}

void RendererSceneRenderFixedFunction::set_scene_pass(uint64_t p_pass) {
	_last_error = OK;
}

void RendererSceneRenderFixedFunction::set_time(double p_time, double p_step) {
	_last_error = OK;
}

void RendererSceneRenderFixedFunction::set_debug_draw_mode(RSE::ViewportDebugDraw p_debug_draw) {
	_renderer.set_debug_unshaded(p_debug_draw == RSE::VIEWPORT_DEBUG_DRAW_UNSHADED);
	_disable_lod = p_debug_draw == RSE::VIEWPORT_DEBUG_DRAW_DISABLE_LOD;
	_report_error(p_debug_draw == RSE::VIEWPORT_DEBUG_DRAW_DISABLED || p_debug_draw == RSE::VIEWPORT_DEBUG_DRAW_UNSHADED || _disable_lod ? OK : ERR_UNAVAILABLE, "set_debug_draw_mode");
}

Ref<RenderSceneBuffers> RendererSceneRenderFixedFunction::render_buffers_create() {
	Ref<RenderSceneBuffersFixedFunction> buffers;
	buffers.instantiate();
	return buffers;
}

void RendererSceneRenderFixedFunction::gi_set_use_half_resolution(bool p_enable) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::screen_space_roughness_limiter_set_active(bool p_enable, float p_amount, float p_limit) {
	_last_error = OK; // No 3D roughness pass exists in this profile.
}

bool RendererSceneRenderFixedFunction::screen_space_roughness_limiter_is_active() const {
	return false;
}

void RendererSceneRenderFixedFunction::sub_surface_scattering_set_quality(RSE::SubSurfaceScatteringQuality p_quality) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::sub_surface_scattering_set_scale(float p_scale, float p_depth_scale) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

TypedArray<Image> RendererSceneRenderFixedFunction::bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) {
	_report_error(ERR_UNAVAILABLE, "bake_render_uv2");
	return TypedArray<Image>();
}

PackedByteArray RendererSceneRenderFixedFunction::bake_render_area_light_atlas(const TypedArray<RID> &p_area_light_textures, const TypedArray<Rect2> &p_area_light_atlas_texture_rects, const Size2i &p_size, int p_mipmaps) {
	_report_error(ERR_UNAVAILABLE, "bake_render_area_light_atlas");
	return PackedByteArray();
}

bool RendererSceneRenderFixedFunction::free(RID p_rid) {
	if (_skies.owns(p_rid)) {
		_skies.free(p_rid);
		return true;
	}
	if (RSG::camera_attributes->owns_camera_attributes(p_rid)) {
		RSG::camera_attributes->camera_attributes_free(p_rid);
		return true;
	}
	if (is_environment(p_rid)) {
		environment_free(p_rid);
		return true;
	}
	if (is_compositor(p_rid)) {
		compositor_free(p_rid);
		return true;
	}
	if (is_compositor_effect(p_rid)) {
		compositor_effect_free(p_rid);
		return true;
	}
	return false;
}

void RendererSceneRenderFixedFunction::sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) {
	_report_error(ERR_UNAVAILABLE, "sdfgi_set_debug_probe_select");
}

void RendererSceneRenderFixedFunction::decals_set_filter(RSE::DecalFilter p_filter) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::light_projectors_set_filter(RSE::LightProjectorFilter p_filter) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::lightmaps_set_bicubic_filter(bool p_enable) {
	_last_error = OK; // Global quality hint for an unavailable 3D pass; no resources are allocated.
}

void RendererSceneRenderFixedFunction::material_set_use_debanding(bool p_enable) {
	_last_error = OK; // Optional quality hint for the unavailable debanding pass.
}

void RendererSceneRenderFixedFunction::update() {
	_last_error = OK;
}

RendererSceneRenderFixedFunction::RendererSceneRenderFixedFunction(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets) : _context(p_context), _render_targets(p_targets), _renderer(p_context, p_targets), _last_error(OK) {}

RendererSceneRenderFixedFunction::~RendererSceneRenderFixedFunction() {
	while (!_instances.is_empty()) {
		FixedFunctionGeometryInstance *instance = *_instances.begin();
		_instances.erase(instance);
		memdelete(instance);
	}
}
