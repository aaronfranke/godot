#pragma once

#include "drivers/fixed_function/render_target_manager.h"
#include "drivers/fixed_function/texture_storage.h"
#include "servers/rendering/storage/texture_storage.h"

// Branch-native texture/target facade. Unsupported resource kinds reject initialization.
// Void setters accept CPU transactions; native synchronization runs at an idle boundary.
class RendererTextureStorageFixedFunction : public RendererTextureStorage {
	FixedFunctionTextureStorage &_texture_storage;
	FixedFunctionRenderTargetManager *_render_targets;
	Error _last_error;

	void _report_error(Error p_error, const char *p_operation);
	void _report_msaa_error(Error p_error, const char *p_operation);
	static void _unsupported(const char *p_operation);
	RendererTextureStorageFixedFunction(const RendererTextureStorageFixedFunction &);
	RendererTextureStorageFixedFunction &operator=(const RendererTextureStorageFixedFunction &);

public:
	RID texture_allocate() override;
	void texture_free(RID p_texture) override;
	void texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) override;
	void texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) override;
	Ref<Image> texture_2d_get(RID p_texture) const override;
	void texture_proxy_initialize(RID p_texture, RID p_base) override;
	void texture_proxy_update(RID p_texture, RID p_base) override;
	Size2 texture_size_with_proxy(RID p_texture) override;

	RID render_target_create() override;
	void render_target_free(RID p_target) override;
	void render_target_set_position(RID p_target, int p_x, int p_y) override;
	Point2i render_target_get_position(RID p_target) const override;
	void render_target_set_size(RID p_target, int p_width, int p_height, uint32_t p_view_count) override;
	Size2i render_target_get_size(RID p_target) const override;
	RID render_target_get_texture(RID p_target) override;
	bool render_target_was_used(RID p_target) const override;
	void render_target_set_as_unused(RID p_target) override;

	void render_target_request_clear(RID p_target, const Color &p_color) override;
	bool render_target_is_clear_requested(RID p_target) override;
	Color render_target_get_clear_request_color(RID p_target) override;
	void render_target_disable_clear_request(RID p_target) override;

	RID canvas_texture_allocate() override;
	void canvas_texture_initialize(RID p_rid) override;
	void canvas_texture_free(RID p_rid) override;
	void canvas_texture_set_channel(RID p_canvas_texture, RSE::CanvasTextureChannel p_channel, RID p_texture) override;
	void canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_base_color, float p_shininess) override;
	void canvas_texture_set_texture_filter(RID p_item, RSE::CanvasItemTextureFilter p_filter) override;
	void canvas_texture_set_texture_repeat(RID p_item, RSE::CanvasItemTextureRepeat p_repeat) override;
	void texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RSE::TextureLayeredType p_layered_type) override;
	void texture_3d_initialize(RID p_texture, Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) override;
	void texture_external_initialize(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) override;
	void texture_drawable_initialize(RID p_texture, int p_width, int p_height, RSE::TextureDrawableFormat p_format, const Color &p_color, bool p_with_mipmaps) override;
	RID texture_create_from_native_handle(RSE::TextureType p_type, Image::Format p_format, uint64_t p_native_handle, int p_width, int p_height, int p_depth, int p_layers = 1, RSE::TextureLayeredType p_layered_type = RSE::TEXTURE_LAYERED_2D_ARRAY) override;
	void texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) override;
	void texture_external_update(RID p_proxy, int p_width, int p_height, uint64_t p_external_buffer) override;
	void texture_drawable_blit_rect(const TypedArray<RID> &p_textures, const Rect2i &p_rect, RID p_material, const Color &p_modulate, const TypedArray<RID> &p_source_textures, int p_to_mipmap) override;
	void texture_2d_placeholder_initialize(RID p_texture) override;
	void texture_2d_layered_placeholder_initialize(RID p_texture, RSE::TextureLayeredType p_layered_type) override;
	void texture_3d_placeholder_initialize(RID p_texture) override;
	Ref<Image> texture_2d_layer_get(RID p_texture, int p_layer) const override;
	Vector<Ref<Image>> texture_3d_get(RID p_texture) const override;
	void texture_drawable_generate_mipmaps(RID p_texture) override;
	RID texture_drawable_get_default_material() const override;
	void texture_replace(RID p_texture, RID p_by_texture) override;
	void texture_replace_compatible(RID p_texture, RID p_by_texture) override;
	void texture_set_size_override(RID p_texture, int p_width, int p_height) override;
	void texture_set_path(RID p_texture, const String &p_path) override;
	String texture_get_path(RID p_texture) const override;
	Image::Format texture_get_format(RID p_texture) const override;
	void texture_set_detect_3d_callback(RID p_texture, RenderingServerTypes::TextureDetectCallback p_callback, void *p_userdata) override;
	void texture_set_detect_normal_callback(RID p_texture, RenderingServerTypes::TextureDetectCallback p_callback, void *p_userdata) override;
	void texture_set_detect_roughness_callback(RID p_texture, RenderingServerTypes::TextureDetectRoughnessCallback p_callback, void *p_userdata) override;
	void texture_debug_usage(List<RenderingServerTypes::TextureInfo> *r_info) override;
	void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) override;
	void texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RSE::TextureLayeredType p_layer_type = RSE::TEXTURE_LAYERED_2D_ARRAY) override;
	RID texture_get_rd_texture(RID p_texture, bool p_srgb = false) const override;
	uint64_t texture_get_native_handle(RID p_texture, bool p_srgb = false) const override;
	void texture_add_to_area_light_atlas(RID p_texture) override;
	void texture_remove_from_area_light_atlas(RID p_texture) override;
	RID decal_allocate() override;
	void decal_initialize(RID p_rid) override;
	void decal_free(RID p_rid) override;
	void decal_set_size(RID p_decal, const Vector3 &p_size) override;
	void decal_set_texture(RID p_decal, RSE::DecalTexture p_type, RID p_texture) override;
	void decal_set_emission_energy(RID p_decal, float p_energy) override;
	void decal_set_albedo_mix(RID p_decal, float p_mix) override;
	void decal_set_modulate(RID p_decal, const Color &p_modulate) override;
	void decal_set_cull_mask(RID p_decal, uint32_t p_layers) override;
	void decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) override;
	void decal_set_fade(RID p_decal, float p_above, float p_below) override;
	void decal_set_normal_fade(RID p_decal, float p_fade) override;
	AABB decal_get_aabb(RID p_decal) const override;
	uint32_t decal_get_cull_mask(RID p_decal) const override;
	void texture_add_to_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override;
	void texture_remove_from_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override;
	RID decal_instance_create(RID p_decal) override;
	void decal_instance_free(RID p_decal_instance) override;
	void decal_instance_set_transform(RID p_decal_instance, const Transform3D &p_transform) override;
	void decal_instance_set_sorting_offset(RID p_decal_instance, float p_sorting_offset) override;
	void render_target_set_transparent(RID p_render_target, bool p_is_transparent) override;
	bool render_target_get_transparent(RID p_render_target) const override;
	void render_target_set_direct_to_screen(RID p_render_target, bool p_direct_to_screen) override;
	bool render_target_get_direct_to_screen(RID p_render_target) const override;
	void render_target_set_msaa(RID p_render_target, RSE::ViewportMSAA p_msaa) override;
	RSE::ViewportMSAA render_target_get_msaa(RID p_render_target) const override;
	void render_target_set_msaa_needs_resolve(RID p_render_target, bool p_needs_resolve) override;
	bool render_target_get_msaa_needs_resolve(RID p_render_target) const override;
	void render_target_do_msaa_resolve(RID p_render_target) override;
	void render_target_set_use_hdr(RID p_render_target, bool p_use_hdr) override;
	bool render_target_is_using_hdr(RID p_render_target) const override;
	void render_target_set_use_debanding(RID p_render_target, bool p_use_debanding) override;
	bool render_target_is_using_debanding(RID p_render_target) const override;
	void render_target_do_clear_request(RID p_render_target) override;
	void render_target_set_sdf_size_and_scale(RID p_render_target, RSE::ViewportSDFOversize p_size, RSE::ViewportSDFScale p_scale) override;
	Rect2i render_target_get_sdf_rect(RID p_render_target) const override;
	void render_target_mark_sdf_enabled(RID p_render_target, bool p_enabled) override;
	void render_target_set_vrs_mode(RID p_render_target, RSE::ViewportVRSMode p_mode) override;
	RSE::ViewportVRSMode render_target_get_vrs_mode(RID p_render_target) const override;
	void render_target_set_vrs_update_mode(RID p_render_target, RSE::ViewportVRSUpdateMode p_mode) override;
	RSE::ViewportVRSUpdateMode render_target_get_vrs_update_mode(RID p_render_target) const override;
	void render_target_set_vrs_texture(RID p_render_target, RID p_texture) override;
	RID render_target_get_vrs_texture(RID p_render_target) const override;
	void render_target_set_override(RID p_render_target, RID p_color_texture, RID p_depth_texture, RID p_velocity_texture, RID p_velocity_depth_texture) override;
	RID render_target_get_override_color(RID p_render_target) const override;
	RID render_target_get_override_depth(RID p_render_target) const override;
	RID render_target_get_override_velocity(RID p_render_target) const override;
	RID render_target_get_override_velocity_depth(RID p_render_target) const override;
	void render_target_set_render_region(RID p_render_target, const Rect2i &p_render_region) override;
	Rect2i render_target_get_render_region(RID p_render_target) const override;
	void render_target_set_subsampled_enabled(RID p_render_target, bool p_enabled) override;
	bool render_target_is_subsampled_enabled(RID p_render_target) const override;
	void render_target_set_subsampled_allowed(RID p_render_target, bool p_allowed) override;
	bool render_target_is_subsampled_allowed(RID p_render_target) const override;
	void render_target_set_velocity_target_size(RID p_render_target, const Size2i &p_target_size) override;
	Size2i render_target_get_velocity_target_size(RID p_render_target) const override;
	void texture_2d_attach_streaming_state(RID p_texture, RID p_streaming_state) override;

	// Queries do not change the status of the last mutating operation.
	Error get_last_error() const { return _last_error; }
	RendererTextureStorageFixedFunction(FixedFunctionTextureStorage &p_texture_storage, FixedFunctionRenderTargetManager *p_targets = nullptr);
};
