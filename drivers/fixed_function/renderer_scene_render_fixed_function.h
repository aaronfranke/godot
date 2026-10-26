#pragma once

#include "drivers/fixed_function/render_target_manager.h"
#include "drivers/fixed_function/scene_renderer.h"
#include "servers/rendering/renderer_scene_render.h"

// Explicit restricted-profile contracts; unsupported resources have no owners or handles.
class RendererSceneRenderFixedFunction : public RendererSceneRender {
	FixedFunctionRenderContext &_context;
	FixedFunctionRenderTargetManager &_render_targets;
	FixedFunctionSceneRenderer _renderer;
	struct FFSky {
		RID material;
	};
	RID_Owner<FFSky> _skies;
	Error _render_sky(const CameraData &p_camera, RID p_environment, RenderingServerTypes::RenderInfo *r_info);
	bool _get_sky_background(RID p_environment, FixedFunctionSceneRenderer::FFSkyBackground &r_background);
	void _prepare_fog(RID p_environment, const Projection &p_projection, FixedFunctionSceneRenderer::FFLightingEnvironment &r_lighting) const;
	HashSet<FixedFunctionGeometryInstance *> _instances;
	Error _last_error;
	bool _disable_lod = false;
	void _report_error(Error p_error, const char *p_operation);
	static void _unsupported(const char *p_operation);

public:
	Error get_last_error() const { return _last_error; }
	RendererSceneRenderFixedFunction(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets);
	~RendererSceneRenderFixedFunction() override;
	RenderGeometryInstance *geometry_instance_create(RID p_base) override;
	void geometry_instance_free(RenderGeometryInstance *p_geometry_instance) override;
	uint32_t geometry_instance_get_pair_mask() override;
	bool supports_visibility_range_fading() const override { return false; }
	uint32_t get_max_lights_total() override;
	uint32_t get_max_lights_per_mesh() override;
	void mesh_generate_pipelines(RID p_mesh, bool p_background_compilation) override;
	uint32_t get_pipeline_compilations(RSE::PipelineSource p_source) override;
	void sdfgi_update(const Ref<RenderSceneBuffers> &p_render_buffers, RID p_environment, const Vector3 &p_world_position) override;
	int sdfgi_get_pending_region_count(const Ref<RenderSceneBuffers> &p_render_buffers) const override;
	AABB sdfgi_get_pending_region_bounds(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const override;
	uint32_t sdfgi_get_pending_region_cascade(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const override;
	RID sky_allocate() override;
	void sky_initialize(RID p_rid) override;
	void sky_set_radiance_size(RID p_sky, int p_radiance_size) override;
	void sky_set_mode(RID p_sky, RSE::SkyMode p_samples) override;
	void sky_set_material(RID p_sky, RID p_material) override;
	Ref<Image> sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size) override;
	void environment_set_volumetric_fog_volume_size(int p_size, int p_depth) override;
	void environment_set_volumetric_fog_filter_active(bool p_enable) override;
	void environment_glow_set_use_bicubic_upscale(bool p_enable) override;
	void environment_set_ssr_half_size(bool p_half_size) override;
	void environment_set_ssr_roughness_quality(RSE::EnvironmentSSRRoughnessQuality p_quality) override;
	void environment_set_ssao_quality(RSE::EnvironmentSSAOQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) override;
	void environment_set_ssil_quality(RSE::EnvironmentSSILQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) override;
	void environment_set_sdfgi_ray_count(RSE::EnvironmentSDFGIRayCount p_ray_count) override;
	void environment_set_sdfgi_frames_to_converge(RSE::EnvironmentSDFGIFramesToConverge p_frames) override;
	void environment_set_sdfgi_frames_to_update_light(RSE::EnvironmentSDFGIFramesToUpdateLight p_update) override;
	Ref<Image> environment_bake_panorama(RID p_env, bool p_bake_irradiance, const Size2i &p_size) override;
	void positional_soft_shadow_filter_set_quality(RSE::ShadowQuality p_quality) override;
	void directional_soft_shadow_filter_set_quality(RSE::ShadowQuality p_quality) override;
	RID fog_volume_instance_create(RID p_fog_volume) override;
	void fog_volume_instance_set_transform(RID p_fog_volume_instance, const Transform3D &p_transform) override;
	void fog_volume_instance_set_active(RID p_fog_volume_instance, bool p_active) override;
	RID fog_volume_instance_get_volume(RID p_fog_volume_instance) const override;
	Vector3 fog_volume_instance_get_position(RID p_fog_volume_instance) const override;
	RID voxel_gi_instance_create(RID p_voxel_gi) override;
	void voxel_gi_instance_set_transform_to_data(RID p_probe, const Transform3D &p_xform) override;
	bool voxel_gi_needs_update(RID p_probe) const override;
	void voxel_gi_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) override;
	void voxel_gi_set_quality(RSE::VoxelGIQuality) override;
	void render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, float p_window_output_max_value, const RenderSDFGIUpdateData *p_sdfgi_update_data = nullptr, RenderingServerTypes::RenderInfo *r_render_info = nullptr) override;
	void render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) override;
	void render_particle_collider_heightfield(RID p_collider, const Transform3D &p_transform, const PagedArray<RenderGeometryInstance *> &p_instances) override;
	void set_scene_pass(uint64_t p_pass) override;
	void set_time(double p_time, double p_step) override;
	void set_debug_draw_mode(RSE::ViewportDebugDraw p_debug_draw) override;
	Ref<RenderSceneBuffers> render_buffers_create() override;
	void gi_set_use_half_resolution(bool p_enable) override;
	void screen_space_roughness_limiter_set_active(bool p_enable, float p_amount, float p_limit) override;
	bool screen_space_roughness_limiter_is_active() const override;
	void sub_surface_scattering_set_quality(RSE::SubSurfaceScatteringQuality p_quality) override;
	void sub_surface_scattering_set_scale(float p_scale, float p_depth_scale) override;
	TypedArray<Image> bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) override;
	PackedByteArray bake_render_area_light_atlas(const TypedArray<RID> &p_area_light_textures, const TypedArray<Rect2> &p_area_light_atlas_texture_rects, const Size2i &p_size, int p_mipmaps) override;
	bool free(RID p_rid) override;
	void sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) override;
	void decals_set_filter(RSE::DecalFilter p_filter) override;
	void light_projectors_set_filter(RSE::LightProjectorFilter p_filter) override;
	void lightmaps_set_bicubic_filter(bool p_enable) override;
	void material_set_use_debanding(bool p_enable) override;
	void update() override;
};
