#ifndef RASTERIZER_STORAGE_FIXED_FUNCTION_H
#define RASTERIZER_STORAGE_FIXED_FUNCTION_H

#include "light_storage.h"
#include "material_storage.h"
#include "mesh_storage.h"
#include "storage_queries.h"
#include "texture_storage.h"

#include "servers/visual/rasterizer.h"

// The 3.x storage facade delegates supported texture/target, material/mesh/light and profile queries
// to borrowed stores. Unimplemented resource families explicitly report unavailable.
// Texture setters retain CPU requests until native resources can be synchronized.
class RasterizerStorageFixedFunction : public RasterizerStorage {
	FixedFunctionTextureStorage &_texture_storage;
	FixedFunctionMaterialStorage &_material_storage;
	FixedFunctionMeshStorage &_mesh_storage;
	FixedFunctionLightStorage &_light_storage;
	FixedFunctionSceneStorage &_scene_storage;
	FixedFunctionStorageQueries &_queries;
	Error _last_error;

	void _report_error(Error p_error, const char *p_operation);
	static void _report_unavailable(const char *p_operation);
	Error _validate_render_target(RID p_render_target) const;
	RasterizerStorageFixedFunction(const RasterizerStorageFixedFunction &);
	RasterizerStorageFixedFunction &operator=(const RasterizerStorageFixedFunction &);

public:
	RID texture_create() override;
	void texture_allocate(RID p_texture, int p_width, int p_height, int p_depth_3d, Image::Format p_format, VS::TextureType p_type, uint32_t p_flags = VS::TEXTURE_FLAGS_DEFAULT) override;
	void texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) override;
	void texture_set_data_partial(RID p_texture, const Ref<Image> &p_image, int p_src_x, int p_src_y, int p_src_width, int p_src_height, int p_dst_x, int p_dst_y, int p_dst_mip, int p_layer = 0) override;
	Ref<Image> texture_get_data(RID p_texture, int p_layer = 0) const override;
	void texture_set_flags(RID p_texture, uint32_t p_flags) override;
	uint32_t texture_get_flags(RID p_texture) const override;
	Image::Format texture_get_format(RID p_texture) const override;
	VS::TextureType texture_get_type(RID p_texture) const override;
	uint32_t texture_get_texid(RID p_texture) const override;
	uint32_t texture_get_width(RID p_texture) const override;
	uint32_t texture_get_height(RID p_texture) const override;
	uint32_t texture_get_depth(RID p_texture) const override;
	void texture_set_size_override(RID p_texture, int p_width, int p_height, int p_depth_3d) override;
	void texture_bind(RID p_texture, uint32_t p_texture_no) override;
	void texture_set_shrink_all_x2_on_set_data(bool p_enable) override;
	void texture_debug_usage(List<VS::TextureInfo> *r_info) override;
	RID texture_create_radiance_cubemap(RID p_source, int p_resolution = -1) const override;
	void texture_set_detect_3d_callback(RID p_texture, VS::TextureDetectCallback p_callback, void *p_userdata) override;
	void texture_set_detect_srgb_callback(RID p_texture, VS::TextureDetectCallback p_callback, void *p_userdata) override;
	void texture_set_detect_normal_callback(RID p_texture, VS::TextureDetectCallback p_callback, void *p_userdata) override;
	void textures_keep_original(bool p_enable) override;
	void texture_set_path(RID p_texture, const String &p_path) override;
	String texture_get_path(RID p_texture) const override;
	void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) override;
	void texture_set_proxy(RID p_texture, RID p_proxy) override;
	Size2 texture_size_with_proxy(RID p_texture) const override;

	RID shader_create() override;
	void shader_set_code(RID p_shader, const String &p_code) override;
	String shader_get_code(RID p_shader) const override;
	void shader_get_param_list(RID p_shader, List<PropertyInfo> *r_parameters) const override;
	void shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture) override;
	RID shader_get_default_texture_param(RID p_shader, const StringName &p_name) const override;
	void shader_add_custom_define(RID p_shader, const String &p_define) override;
	void shader_get_custom_defines(RID p_shader, Vector<String> *r_defines) const override;
	void shader_remove_custom_define(RID p_shader, const String &p_define) override;
	void set_shader_async_hidden_forbidden(bool p_forbidden) override;
	bool is_shader_async_hidden_forbidden() override;
	RID material_create() override;
	void material_set_render_priority(RID p_material, int p_priority) override;
	void material_set_line_width(RID p_material, float p_width) override;
	void material_set_next_pass(RID p_material, RID p_next_material) override;
	void material_set_shader(RID p_material, RID p_shader) override;
	RID material_get_shader(RID p_material) const override;
	void material_set_param(RID p_material, const StringName &p_name, const Variant &p_value) override;
	Variant material_get_param(RID p_material, const StringName &p_name) const override;
	Variant material_get_param_default(RID p_material, const StringName &p_name) const override;
	bool material_is_animated(RID p_material) override;
	bool material_casts_shadows(RID p_material) override;
	bool material_uses_tangents(RID p_material) override;
	bool material_uses_ensure_correct_normals(RID p_material) override;
	void material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) override;
	void material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) override;

	RID render_target_create() override;
	void render_target_set_position(RID p_render_target, int p_x, int p_y) override;
	void render_target_set_size(RID p_render_target, int p_width, int p_height) override;
	RID render_target_get_texture(RID p_render_target) const override;
	uint32_t render_target_get_depth_texture_id(RID p_render_target) const override;
	void render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id, unsigned int p_depth_id) override;
	void render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) override;
	void render_target_set_msaa(RID p_render_target, VS::ViewportMSAA p_msaa) override;
	void render_target_set_use_fxaa(RID p_render_target, bool p_enabled) override;
	void render_target_set_use_debanding(RID p_render_target, bool p_enabled) override;
	void render_target_set_sharpen_intensity(RID p_render_target, float p_intensity) override;
	bool render_target_was_used(RID p_render_target) override;
	void render_target_clear_used(RID p_render_target) override;
	// True means this facade handles the RID. Accepted releases are deferred; discard the RID.
	bool free(RID p_rid) override;
	void update_dirty_resources() override;
	VS::InstanceType get_base_type(RID p_rid) const override;
	bool has_os_feature(const String &p_feature) const override;
	void set_debug_generate_wireframes(bool p_generate) override;
	void render_info_begin_capture() override;
	void render_info_end_capture() override;
	int get_captured_render_info(VS::RenderInfo p_info) override;
	uint64_t get_render_info(VS::RenderInfo p_info) override;
	String get_video_adapter_name() const override;
	String get_video_adapter_vendor() const override;

	// Sky: unavailable in the initial 2D profile.
	RID sky_create() override;
	void sky_set_texture(RID p_sky, RID p_cube_map, int p_radiance_size) override;

	// Mesh CPU ownership; scene drawing is implemented separately.
	RID mesh_create() override;
	void mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, PoolVector<uint8_t> p_array, int p_vertex_count, PoolVector<uint8_t> p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t>> &p_blend_shapes = Vector<PoolVector<uint8_t>>(), const Vector<AABB> &p_bone_aabbs = Vector<AABB>()) override;
	void mesh_set_blend_shape_count(RID p_mesh, int p_amount) override;
	int mesh_get_blend_shape_count(RID p_mesh) const override;
	void mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode) override;
	VS::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const override;
	void mesh_set_blend_shape_values(RID p_mesh, PoolVector<float> p_values) override;
	PoolVector<float> mesh_get_blend_shape_values(RID p_mesh) const override;
	void mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, PoolVector<uint8_t> p_data) override;
	void mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) override;
	RID mesh_surface_get_material(RID p_mesh, int p_surface) const override;
	int mesh_surface_get_array_len(RID p_mesh, int p_surface) const override;
	int mesh_surface_get_array_index_len(RID p_mesh, int p_surface) const override;
	PoolVector<uint8_t> mesh_surface_get_array(RID p_mesh, int p_surface) const override;
	PoolVector<uint8_t> mesh_surface_get_index_array(RID p_mesh, int p_surface) const override;
	uint32_t mesh_surface_get_format(RID p_mesh, int p_surface) const override;
	VS::PrimitiveType mesh_surface_get_primitive_type(RID p_mesh, int p_surface) const override;
	AABB mesh_surface_get_aabb(RID p_mesh, int p_surface) const override;
	Vector<PoolVector<uint8_t>> mesh_surface_get_blend_shapes(RID p_mesh, int p_surface) const override;
	Vector<AABB> mesh_surface_get_skeleton_aabb(RID p_mesh, int p_surface) const override;
	void mesh_remove_surface(RID p_mesh, int p_index) override;
	int mesh_get_surface_count(RID p_mesh) const override;
	void mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) override;
	AABB mesh_get_custom_aabb(RID p_mesh) const override;
	AABB mesh_get_aabb(RID p_mesh, RID p_skeleton) const override;
	void mesh_clear(RID p_mesh) override;

	// Multimesh: unavailable in the initial 2D profile.
	void multimesh_attach_canvas_item(RID p_multimesh, RID p_canvas_item, bool p_attach) override;
	RID _multimesh_create() override;
	void _multimesh_allocate(RID p_multimesh, int p_instances, VS::MultimeshTransformFormat p_transform_format, VS::MultimeshColorFormat p_color_format, VS::MultimeshCustomDataFormat p_data = VS::MULTIMESH_CUSTOM_DATA_NONE) override;
	int _multimesh_get_instance_count(RID p_multimesh) const override;
	void _multimesh_set_mesh(RID p_multimesh, RID p_mesh) override;
	void _multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform &p_transform) override;
	void _multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) override;
	void _multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) override;
	void _multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) override;
	RID _multimesh_get_mesh(RID p_multimesh) const override;
	Transform _multimesh_instance_get_transform(RID p_multimesh, int p_index) const override;
	Transform2D _multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const override;
	Color _multimesh_instance_get_color(RID p_multimesh, int p_index) const override;
	Color _multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const override;
	void _multimesh_set_as_bulk_array(RID p_multimesh, PoolVector<float> p_array) override;
	void multimesh_set_as_bulk_array_interpolated(RID p_multimesh, PoolVector<float> p_array, PoolVector<float> p_array_prev) override;
	void _multimesh_set_visible_instances(RID p_multimesh, int p_visible) override;
	int _multimesh_get_visible_instances(RID p_multimesh) const override;
	AABB _multimesh_get_aabb(RID p_multimesh) const override;
	MMInterpolator *_multimesh_get_interpolator(RID p_multimesh) const override;

	// Immediate: unavailable in the initial 2D profile.
	RID immediate_create() override;
	void immediate_begin(RID p_immediate, VS::PrimitiveType p_rimitive, RID p_texture = RID()) override;
	void immediate_vertex(RID p_immediate, const Vector3 &p_vertex) override;
	void immediate_normal(RID p_immediate, const Vector3 &p_normal) override;
	void immediate_tangent(RID p_immediate, const Plane &p_tangent) override;
	void immediate_color(RID p_immediate, const Color &p_color) override;
	void immediate_uv(RID p_immediate, const Vector2 &tex_uv) override;
	void immediate_uv2(RID p_immediate, const Vector2 &tex_uv) override;
	void immediate_end(RID p_immediate) override;
	void immediate_clear(RID p_immediate) override;
	void immediate_set_material(RID p_immediate, RID p_material) override;
	RID immediate_get_material(RID p_immediate) const override;
	AABB immediate_get_aabb(RID p_immediate) const override;

	// Skeleton: unavailable in the initial 2D profile.
	RID skeleton_create() override;
	void skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d_skeleton = false) override;
	int skeleton_get_bone_count(RID p_skeleton) const override;
	void skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform &p_transform) override;
	Transform skeleton_bone_get_transform(RID p_skeleton, int p_bone) const override;
	void skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) override;
	Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const override;
	void skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_base_transform) override;
	uint32_t skeleton_get_revision(RID p_skeleton) const override;
	void skeleton_attach_canvas_item(RID p_skeleton, RID p_canvas_item, bool p_attach) override;

	// Light CPU metadata; directional/ambient shading executes in the scene renderer.
	RID light_create(VS::LightType p_type) override;
	void light_set_color(RID p_light, const Color &p_color) override;
	void light_set_param(RID p_light, VS::LightParam p_param, float p_value) override;
	void light_set_shadow(RID p_light, bool p_enabled) override;
	void light_set_shadow_color(RID p_light, const Color &p_color) override;
	void light_set_projector(RID p_light, RID p_texture) override;
	void light_set_negative(RID p_light, bool p_enable) override;
	void light_set_cull_mask(RID p_light, uint32_t p_mask) override;
	void light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) override;
	void light_set_use_gi(RID p_light, bool p_enable) override;
	void light_set_bake_mode(RID p_light, VS::LightBakeMode p_bake_mode) override;
	void light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode) override;
	void light_omni_set_shadow_detail(RID p_light, VS::LightOmniShadowDetail p_detail) override;
	void light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode) override;
	void light_directional_set_blend_splits(RID p_light, bool p_enable) override;
	bool light_directional_get_blend_splits(RID p_light) const override;
	void light_directional_set_shadow_depth_range_mode(RID p_light, VS::LightDirectionalShadowDepthRangeMode p_range_mode) override;
	VS::LightDirectionalShadowDepthRangeMode light_directional_get_shadow_depth_range_mode(RID p_light) const override;
	VS::LightDirectionalShadowMode light_directional_get_shadow_mode(RID p_light) override;
	VS::LightOmniShadowMode light_omni_get_shadow_mode(RID p_light) override;
	bool light_has_shadow(RID p_light) const override;
	VS::LightType light_get_type(RID p_light) const override;
	AABB light_get_aabb(RID p_light) const override;
	float light_get_param(RID p_light, VS::LightParam p_param) override;
	Color light_get_color(RID p_light) override;
	bool light_get_use_gi(RID p_light) override;
	VS::LightBakeMode light_get_bake_mode(RID p_light) override;
	uint64_t light_get_version(RID p_light) const override;

	// Reflection: unavailable in the initial 2D profile.
	RID reflection_probe_create() override;
	void reflection_probe_set_update_mode(RID p_probe, VS::ReflectionProbeUpdateMode p_mode) override;
	void reflection_probe_set_resolution(RID p_probe, int p_resolution) override;
	void reflection_probe_set_intensity(RID p_probe, float p_intensity) override;
	void reflection_probe_set_interior_ambient(RID p_probe, const Color &p_ambient) override;
	void reflection_probe_set_interior_ambient_energy(RID p_probe, float p_energy) override;
	void reflection_probe_set_interior_ambient_probe_contribution(RID p_probe, float p_contrib) override;
	void reflection_probe_set_max_distance(RID p_probe, float p_distance) override;
	void reflection_probe_set_extents(RID p_probe, const Vector3 &p_extents) override;
	void reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) override;
	void reflection_probe_set_as_interior(RID p_probe, bool p_enable) override;
	void reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) override;
	void reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) override;
	void reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) override;
	AABB reflection_probe_get_aabb(RID p_probe) const override;
	VS::ReflectionProbeUpdateMode reflection_probe_get_update_mode(RID p_probe) const override;
	uint32_t reflection_probe_get_cull_mask(RID p_probe) const override;
	Vector3 reflection_probe_get_extents(RID p_probe) const override;
	Vector3 reflection_probe_get_origin_offset(RID p_probe) const override;
	float reflection_probe_get_origin_max_distance(RID p_probe) const override;
	bool reflection_probe_renders_shadows(RID p_probe) const override;

	// Scene dependencies: unavailable in the initial 2D profile.
	void instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) override;
	void instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance) override;
	void instance_add_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) override;
	void instance_remove_dependency(RID p_base, RasterizerScene::InstanceBase *p_instance) override;

	// Gi: unavailable in the initial 2D profile.
	RID gi_probe_create() override;
	void gi_probe_set_bounds(RID p_probe, const AABB &p_bounds) override;
	AABB gi_probe_get_bounds(RID p_probe) const override;
	void gi_probe_set_cell_size(RID p_probe, float p_range) override;
	float gi_probe_get_cell_size(RID p_probe) const override;
	void gi_probe_set_to_cell_xform(RID p_probe, const Transform &p_xform) override;
	Transform gi_probe_get_to_cell_xform(RID p_probe) const override;
	void gi_probe_set_dynamic_data(RID p_probe, PoolVector<int> p_data) override;
	PoolVector<int> gi_probe_get_dynamic_data(RID p_probe) const override;
	void gi_probe_set_dynamic_range(RID p_probe, int p_range) override;
	int gi_probe_get_dynamic_range(RID p_probe) const override;
	void gi_probe_set_energy(RID p_probe, float p_range) override;
	float gi_probe_get_energy(RID p_probe) const override;
	void gi_probe_set_bias(RID p_probe, float p_range) override;
	float gi_probe_get_bias(RID p_probe) const override;
	void gi_probe_set_normal_bias(RID p_probe, float p_range) override;
	float gi_probe_get_normal_bias(RID p_probe) const override;
	void gi_probe_set_propagation(RID p_probe, float p_range) override;
	float gi_probe_get_propagation(RID p_probe) const override;
	void gi_probe_set_interior(RID p_probe, bool p_enable) override;
	bool gi_probe_is_interior(RID p_probe) const override;
	void gi_probe_set_compress(RID p_probe, bool p_enable) override;
	bool gi_probe_is_compressed(RID p_probe) const override;
	uint32_t gi_probe_get_version(RID p_probe) override;
	RID gi_probe_dynamic_data_create(int p_width, int p_height, int p_depth, GIProbeCompression p_compression) override;
	void gi_probe_dynamic_data_update(RID p_gi_probe_data, int p_depth_slice, int p_slice_count, int p_mipmap, const void *p_data) override;

	// Lightmap: unavailable in the initial 2D profile.
	RID lightmap_capture_create() override;
	void lightmap_capture_set_bounds(RID p_capture, const AABB &p_bounds) override;
	AABB lightmap_capture_get_bounds(RID p_capture) const override;
	void lightmap_capture_set_octree(RID p_capture, PoolVector<uint8_t> p_octree) override;
	PoolVector<uint8_t> lightmap_capture_get_octree(RID p_capture) const override;
	void lightmap_capture_set_octree_cell_transform(RID p_capture, const Transform &p_xform) override;
	Transform lightmap_capture_get_octree_cell_transform(RID p_capture) const override;
	void lightmap_capture_set_octree_cell_subdiv(RID p_capture, int p_subdiv) override;
	int lightmap_capture_get_octree_cell_subdiv(RID p_capture) const override;
	void lightmap_capture_set_energy(RID p_capture, float p_energy) override;
	float lightmap_capture_get_energy(RID p_capture) const override;
	void lightmap_capture_set_interior(RID p_capture, bool p_interior) override;
	bool lightmap_capture_is_interior(RID p_capture) const override;
	const PoolVector<LightmapCaptureOctree> *lightmap_capture_get_octree_ptr(RID p_capture) const override;

	// Particles: unavailable in the initial 2D profile.
	RID particles_create() override;
	void particles_set_emitting(RID p_particles, bool p_emitting) override;
	bool particles_get_emitting(RID p_particles) override;
	void particles_set_amount(RID p_particles, int p_amount) override;
	void particles_set_lifetime(RID p_particles, float p_lifetime) override;
	void particles_set_one_shot(RID p_particles, bool p_one_shot) override;
	void particles_set_pre_process_time(RID p_particles, float p_time) override;
	void particles_set_explosiveness_ratio(RID p_particles, float p_ratio) override;
	void particles_set_randomness_ratio(RID p_particles, float p_ratio) override;
	void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) override;
	void particles_set_speed_scale(RID p_particles, float p_scale) override;
	void particles_set_use_local_coordinates(RID p_particles, bool p_enable) override;
	void particles_set_process_material(RID p_particles, RID p_material) override;
	void particles_set_fixed_fps(RID p_particles, int p_fps) override;
	void particles_set_fractional_delta(RID p_particles, bool p_enable) override;
	void particles_restart(RID p_particles) override;
	bool particles_is_inactive(RID p_particles) const override;
	void particles_set_draw_order(RID p_particles, VS::ParticlesDrawOrder p_order) override;
	void particles_set_draw_passes(RID p_particles, int p_count) override;
	void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) override;
	void particles_request_process(RID p_particles) override;
	AABB particles_get_current_aabb(RID p_particles) override;
	AABB particles_get_aabb(RID p_particles) const override;
	void particles_set_emission_transform(RID p_particles, const Transform &p_transform) override;
	int particles_get_draw_passes(RID p_particles) const override;
	RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const override;

	// Canvas shadows and occluders: unavailable in the initial 2D profile.
	RID canvas_light_shadow_buffer_create(int p_width) override;
	RID canvas_light_occluder_create() override;
	void canvas_light_occluder_set_polylines(RID p_occluder, PoolVector<Vector2> p_lines) override;

	// 3.x flag interpretation stays outside common logical storage.
	static Error get_sampler_flags(uint32_t p_flags, const FixedFunctionGraphicsDevice::FFCapabilities &p_caps, uint8_t &r_sampler_flags);

	// Queries do not change this status; it records the last mutating operation.
	Error get_last_error() const { return _last_error; }

	// Construct only as part of the eventual real renderer: RasterizerStorage sets a singleton.
	// The borrowed stores outlive this facade, and all calls stay on their owning thread.
	RasterizerStorageFixedFunction(FixedFunctionTextureStorage &p_texture_storage, FixedFunctionMaterialStorage &p_material_storage, FixedFunctionMeshStorage &p_mesh_storage, FixedFunctionLightStorage &p_light_storage, FixedFunctionSceneStorage &p_scene_storage, FixedFunctionStorageQueries &p_queries);
	~RasterizerStorageFixedFunction() override;
};

#endif // RASTERIZER_STORAGE_FIXED_FUNCTION_H
