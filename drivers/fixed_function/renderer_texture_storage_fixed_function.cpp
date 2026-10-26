#include "renderer_texture_storage_fixed_function.h"

void RendererTextureStorageFixedFunction::_unsupported(const char *p_operation) {
	ERR_PRINT(String("Fixed Function texture storage ") + p_operation + " is unavailable.");
}

void RendererTextureStorageFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP) {
		ERR_PRINT(String("Fixed Function texture storage ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

void RendererTextureStorageFixedFunction::_report_msaa_error(Error p_error, const char *p_operation) {
	if (p_error == ERR_UNAVAILABLE) {
		_last_error = p_error;
		FixedFunctionTextureStorage::warn_unsupported_msaa();
		return;
	}
	_report_error(p_error, p_operation);
}

RID RendererTextureStorageFixedFunction::texture_allocate() {
	const RID texture = _texture_storage.texture_create(true);
	_report_error(texture.is_valid() ? OK : ERR_CANT_CREATE, "texture_allocate");
	return texture;
}

void RendererTextureStorageFixedFunction::texture_free(RID p_texture) {
	_report_error(_texture_storage.owns_texture(p_texture) ? _texture_storage.request_free(p_texture) : ERR_INVALID_PARAMETER, "texture_free");
}

void RendererTextureStorageFixedFunction::texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) {
	Error error = p_image.is_valid() ? _texture_storage.texture_request_allocate_2d(p_texture, Size2i(p_image->get_width(), p_image->get_height()), p_image->get_format(), 0, 0) : ERR_INVALID_PARAMETER;
	if (error == OK) {
		error = _texture_storage.texture_request_data(p_texture, p_image);
	}
	_report_error(error, "texture_2d_initialize");
}

void RendererTextureStorageFixedFunction::texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer) {
	_report_error(p_layer == 0 ? _texture_storage.texture_request_data(p_texture, p_image) : ERR_UNAVAILABLE, "texture_2d_update");
}

Ref<Image> RendererTextureStorageFixedFunction::texture_2d_get(RID p_texture) const {
	return _texture_storage.texture_get_data(p_texture);
}

void RendererTextureStorageFixedFunction::texture_proxy_initialize(RID p_texture, RID p_base) {
	_report_error(_texture_storage.texture_set_proxy(p_texture, p_base), "texture_proxy_initialize");
}

void RendererTextureStorageFixedFunction::texture_proxy_update(RID p_texture, RID p_base) {
	_report_error(_texture_storage.texture_set_proxy(p_texture, p_base), "texture_proxy_update");
}

Size2 RendererTextureStorageFixedFunction::texture_size_with_proxy(RID p_texture) {
	return _texture_storage.texture_size_with_proxy(p_texture);
}

RID RendererTextureStorageFixedFunction::render_target_create() {
	const RID target = _texture_storage.render_target_create();
	_report_error(target.is_valid() ? OK : ERR_CANT_CREATE, "render_target_create");
	return target;
}

void RendererTextureStorageFixedFunction::render_target_free(RID p_target) {
	_report_error(_texture_storage.owns_render_target(p_target) ? _texture_storage.request_free(p_target) : ERR_INVALID_PARAMETER, "render_target_free");
}

void RendererTextureStorageFixedFunction::render_target_set_position(RID p_target, int p_x, int p_y) {
	_report_error(_texture_storage.render_target_set_position(p_target, p_x, p_y), "render_target_set_position");
}

Point2i RendererTextureStorageFixedFunction::render_target_get_position(RID p_target) const {
	return _texture_storage.render_target_get_rect(p_target).position;
}

void RendererTextureStorageFixedFunction::render_target_set_size(RID p_target, int p_width, int p_height, uint32_t p_view_count) {
	// Window teardown may disable its viewport using zero size and zero views.
	_report_error(p_view_count == 1 || (p_view_count == 0 && p_width == 0 && p_height == 0) ? _texture_storage.render_target_set_size(p_target, p_width, p_height) : ERR_UNAVAILABLE, "render_target_set_size");
}

Size2i RendererTextureStorageFixedFunction::render_target_get_size(RID p_target) const {
	return _texture_storage.render_target_get_rect(p_target).size;
}

RID RendererTextureStorageFixedFunction::render_target_get_texture(RID p_target) {
	return _texture_storage.render_target_get_texture(p_target);
}

bool RendererTextureStorageFixedFunction::render_target_was_used(RID p_target) const {
	return _texture_storage.render_target_was_used(p_target);
}

void RendererTextureStorageFixedFunction::render_target_set_as_unused(RID p_target) {
	_report_error(_texture_storage.render_target_clear_used(p_target), "render_target_set_as_unused");
}

RendererTextureStorageFixedFunction::RendererTextureStorageFixedFunction(FixedFunctionTextureStorage &p_texture_storage, FixedFunctionRenderTargetManager *p_targets) :
		_texture_storage(p_texture_storage),
		_render_targets(p_targets),
		_last_error(OK) {
}

void RendererTextureStorageFixedFunction::render_target_request_clear(RID p_target, const Color &p_color) {
	_report_error(_texture_storage.render_target_request_clear(p_target, p_color), "render_target_request_clear");
}

bool RendererTextureStorageFixedFunction::render_target_is_clear_requested(RID p_target) {
	return _texture_storage.render_target_is_clear_requested(p_target);
}

Color RendererTextureStorageFixedFunction::render_target_get_clear_request_color(RID p_target) {
	return _texture_storage.render_target_get_clear_request_color(p_target);
}

void RendererTextureStorageFixedFunction::render_target_disable_clear_request(RID p_target) {
	_report_error(_texture_storage.render_target_disable_clear_request(p_target), "render_target_disable_clear_request");
}
RID RendererTextureStorageFixedFunction::canvas_texture_allocate() {
	return _texture_storage.canvas_texture_create();
}

void RendererTextureStorageFixedFunction::canvas_texture_initialize(RID p_rid) {
	_report_error(_texture_storage.owns_canvas_texture(p_rid) ? OK : ERR_INVALID_PARAMETER, "canvas_texture_initialize");
}

void RendererTextureStorageFixedFunction::canvas_texture_free(RID p_rid) {
	_report_error(_texture_storage.canvas_texture_free(p_rid), "canvas_texture_free");
}

void RendererTextureStorageFixedFunction::canvas_texture_set_channel(RID p_canvas_texture, RSE::CanvasTextureChannel p_channel, RID p_texture) {
	_report_error(_texture_storage.canvas_texture_set_channel(p_canvas_texture, p_channel, p_texture), "canvas_texture_set_channel");
}

void RendererTextureStorageFixedFunction::canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_base_color, float p_shininess) {
	_report_error(_texture_storage.canvas_texture_set_shading_parameters(p_canvas_texture, p_base_color, p_shininess), "canvas_texture_set_shading_parameters");
}

void RendererTextureStorageFixedFunction::canvas_texture_set_texture_filter(RID p_item, RSE::CanvasItemTextureFilter p_filter) {
	_report_error(_texture_storage.canvas_texture_set_filter(p_item, p_filter), "canvas_texture_set_texture_filter");
}

void RendererTextureStorageFixedFunction::canvas_texture_set_texture_repeat(RID p_item, RSE::CanvasItemTextureRepeat p_repeat) {
	_report_error(_texture_storage.canvas_texture_set_repeat(p_item, p_repeat), "canvas_texture_set_texture_repeat");
}

void RendererTextureStorageFixedFunction::texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RSE::TextureLayeredType p_layered_type) {
	_report_error(ERR_UNAVAILABLE, "texture_2d_layered_initialize");
}

void RendererTextureStorageFixedFunction::texture_3d_initialize(RID p_texture, Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) {
	_report_error(ERR_UNAVAILABLE, "texture_3d_initialize");
}

void RendererTextureStorageFixedFunction::texture_external_initialize(RID p_texture, int p_width, int p_height, uint64_t p_external_buffer) {
	_report_error(ERR_UNAVAILABLE, "texture_external_initialize");
}

void RendererTextureStorageFixedFunction::texture_drawable_initialize(RID p_texture, int p_width, int p_height, RSE::TextureDrawableFormat p_format, const Color &p_color, bool p_with_mipmaps) {
	if (p_width <= 0 || p_height <= 0 || p_width > Image::MAX_WIDTH || p_height > Image::MAX_HEIGHT ||
			p_format < RSE::TEXTURE_DRAWABLE_FORMAT_RGBA8 || p_format > RSE::TEXTURE_DRAWABLE_FORMAT_RGBAF ||
			!Math::is_finite(p_color.r) || !Math::is_finite(p_color.g) || !Math::is_finite(p_color.b) || !Math::is_finite(p_color.a)) {
		_report_error(ERR_INVALID_PARAMETER, "texture_drawable_initialize");
		return;
	}
	// The base color can be sampled as a regular texture with no full-size backing.
	Ref<Image> image = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	image->fill(p_color.clamp());
	texture_2d_initialize(p_texture, image);
	if (_last_error == OK) {
		texture_set_size_override(p_texture, p_width, p_height);
	}
}

RID RendererTextureStorageFixedFunction::texture_create_from_native_handle(RSE::TextureType p_type, Image::Format p_format, uint64_t p_native_handle, int p_width, int p_height, int p_depth, int p_layers, RSE::TextureLayeredType p_layered_type) {
	_report_error(ERR_UNAVAILABLE, "texture_create_from_native_handle");
	return RID();
}

void RendererTextureStorageFixedFunction::texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) {
	_report_error(ERR_UNAVAILABLE, "texture_3d_update");
}

void RendererTextureStorageFixedFunction::texture_external_update(RID p_proxy, int p_width, int p_height, uint64_t p_external_buffer) {
	_report_error(ERR_UNAVAILABLE, "texture_external_update");
}

void RendererTextureStorageFixedFunction::texture_drawable_blit_rect(const TypedArray<RID> &p_textures, const Rect2i &p_rect, RID p_material, const Color &p_modulate, const TypedArray<RID> &p_source_textures, int p_to_mipmap) {
	_last_error = ERR_UNAVAILABLE;
	WARN_PRINT_ONCE("Fixed Function keeps drawable textures at their base color. Texture blits are unavailable.");
}

void RendererTextureStorageFixedFunction::texture_2d_placeholder_initialize(RID p_texture) {
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 1, 1));
	texture_2d_initialize(p_texture, image);
}

void RendererTextureStorageFixedFunction::texture_2d_layered_placeholder_initialize(RID p_texture, RSE::TextureLayeredType p_layered_type) {
	_report_error(ERR_UNAVAILABLE, "texture_2d_layered_placeholder_initialize");
}

void RendererTextureStorageFixedFunction::texture_3d_placeholder_initialize(RID p_texture) {
	_report_error(ERR_UNAVAILABLE, "texture_3d_placeholder_initialize");
}

Ref<Image> RendererTextureStorageFixedFunction::texture_2d_layer_get(RID p_texture, int p_layer) const {
	_unsupported("texture_2d_layer_get");
	return Ref<Image>();
}

Vector<Ref<Image>> RendererTextureStorageFixedFunction::texture_3d_get(RID p_texture) const {
	_unsupported("texture_3d_get");
	return Vector<Ref<Image>>();
}

void RendererTextureStorageFixedFunction::texture_drawable_generate_mipmaps(RID p_texture) {
	_last_error = ERR_UNAVAILABLE;
	WARN_PRINT_ONCE("Fixed Function does not generate mipmaps for drawable textures.");
}

RID RendererTextureStorageFixedFunction::texture_drawable_get_default_material() const {
	// DrawableTexture2D asks during construction, including editor resource previews.
	// The static base-color texture needs no texture-blit material.
	return RID();
}

void RendererTextureStorageFixedFunction::texture_replace(RID p_texture, RID p_by_texture) {
	_report_error(_texture_storage.texture_replace(p_texture, p_by_texture), "texture_replace");
}

void RendererTextureStorageFixedFunction::texture_replace_compatible(RID p_texture, RID p_by_texture) {
	_report_error(_texture_storage.texture_replace(p_texture, p_by_texture), "texture_replace_compatible");
}

void RendererTextureStorageFixedFunction::texture_set_size_override(RID p_texture, int p_width, int p_height) {
	_report_error(_texture_storage.texture_set_size_override(p_texture, Size2i(p_width, p_height)), "texture_set_size_override");
}

void RendererTextureStorageFixedFunction::texture_set_path(RID p_texture, const String &p_path) {
	_report_error(_texture_storage.texture_set_path(p_texture, p_path), "texture_set_path");
}

String RendererTextureStorageFixedFunction::texture_get_path(RID p_texture) const {
	return _texture_storage.texture_get_path(p_texture);
}

Image::Format RendererTextureStorageFixedFunction::texture_get_format(RID p_texture) const {
	return _texture_storage.texture_get_format(p_texture);
}

void RendererTextureStorageFixedFunction::texture_set_detect_3d_callback(RID p_texture, RenderingServerTypes::TextureDetectCallback p_callback, void *p_userdata) {
	_report_error(_texture_storage.texture_set_detect_callback(p_texture, FixedFunctionTextureStorage::DETECT_3D, p_callback, p_userdata), "texture_set_detect_3d_callback");
}

void RendererTextureStorageFixedFunction::texture_set_detect_normal_callback(RID p_texture, RenderingServerTypes::TextureDetectCallback p_callback, void *p_userdata) {
	_report_error(_texture_storage.texture_set_detect_callback(p_texture, FixedFunctionTextureStorage::DETECT_NORMAL, p_callback, p_userdata), "texture_set_detect_normal_callback");
}

void RendererTextureStorageFixedFunction::texture_set_detect_roughness_callback(RID p_texture, RenderingServerTypes::TextureDetectRoughnessCallback p_callback, void *p_userdata) {
	_report_error(_texture_storage.texture_set_detect_roughness_callback(p_texture, p_callback, p_userdata), "texture_set_detect_roughness_callback");
}

void RendererTextureStorageFixedFunction::texture_debug_usage(List<RenderingServerTypes::TextureInfo> *r_info) {
	if (!r_info) {
		_report_error(ERR_INVALID_PARAMETER, "texture_debug_usage");
		return;
	}
	List<FixedFunctionTextureStorage::FFTextureUsage> rows;
	const Error error = _texture_storage.texture_debug_usage(&rows);
	_report_error(error, "texture_debug_usage");
	if (error != OK) {
		return;
	}
	for (const FixedFunctionTextureStorage::FFTextureUsage &row : rows) {
		RenderingServerTypes::TextureInfo info;
		info.texture = row.texture;
		info.width = row.size.x;
		info.height = row.size.y;
		info.depth = 0;
		info.type = RSE::TEXTURE_TYPE_2D;
		info.format = row.format;
		info.bytes = row.native_bytes;
		info.path = row.path;
		r_info->push_back(info);
	}
}

void RendererTextureStorageFixedFunction::texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) {
	_report_error(_texture_storage.texture_set_force_redraw_if_visible(p_texture, p_enable), "texture_set_force_redraw_if_visible");
}

void RendererTextureStorageFixedFunction::texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RSE::TextureLayeredType p_layer_type) {
	_report_error(ERR_UNAVAILABLE, "texture_rd_initialize");
}

RID RendererTextureStorageFixedFunction::texture_get_rd_texture(RID p_texture, bool p_srgb) const {
	_unsupported("texture_get_rd_texture");
	return RID();
}

uint64_t RendererTextureStorageFixedFunction::texture_get_native_handle(RID p_texture, bool p_srgb) const {
	_unsupported("texture_get_native_handle");
	return 0;
}

void RendererTextureStorageFixedFunction::texture_add_to_area_light_atlas(RID p_texture) {
	_report_error(ERR_UNAVAILABLE, "texture_add_to_area_light_atlas");
}

void RendererTextureStorageFixedFunction::texture_remove_from_area_light_atlas(RID p_texture) {
	_report_error(ERR_UNAVAILABLE, "texture_remove_from_area_light_atlas");
}

RID RendererTextureStorageFixedFunction::decal_allocate() {
	_report_error(ERR_UNAVAILABLE, "decal_allocate");
	return RID();
}

void RendererTextureStorageFixedFunction::decal_initialize(RID p_rid) {
	_report_error(ERR_UNAVAILABLE, "decal_initialize");
}

void RendererTextureStorageFixedFunction::decal_free(RID p_rid) {
	_report_error(ERR_UNAVAILABLE, "decal_free");
}

void RendererTextureStorageFixedFunction::decal_set_size(RID p_decal, const Vector3 &p_size) {
	_report_error(ERR_UNAVAILABLE, "decal_set_size");
}

void RendererTextureStorageFixedFunction::decal_set_texture(RID p_decal, RSE::DecalTexture p_type, RID p_texture) {
	_report_error(ERR_UNAVAILABLE, "decal_set_texture");
}

void RendererTextureStorageFixedFunction::decal_set_emission_energy(RID p_decal, float p_energy) {
	_report_error(ERR_UNAVAILABLE, "decal_set_emission_energy");
}

void RendererTextureStorageFixedFunction::decal_set_albedo_mix(RID p_decal, float p_mix) {
	_report_error(ERR_UNAVAILABLE, "decal_set_albedo_mix");
}

void RendererTextureStorageFixedFunction::decal_set_modulate(RID p_decal, const Color &p_modulate) {
	_report_error(ERR_UNAVAILABLE, "decal_set_modulate");
}

void RendererTextureStorageFixedFunction::decal_set_cull_mask(RID p_decal, uint32_t p_layers) {
	_report_error(ERR_UNAVAILABLE, "decal_set_cull_mask");
}

void RendererTextureStorageFixedFunction::decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) {
	_report_error(ERR_UNAVAILABLE, "decal_set_distance_fade");
}

void RendererTextureStorageFixedFunction::decal_set_fade(RID p_decal, float p_above, float p_below) {
	_report_error(ERR_UNAVAILABLE, "decal_set_fade");
}

void RendererTextureStorageFixedFunction::decal_set_normal_fade(RID p_decal, float p_fade) {
	_report_error(ERR_UNAVAILABLE, "decal_set_normal_fade");
}

AABB RendererTextureStorageFixedFunction::decal_get_aabb(RID p_decal) const {
	_unsupported("decal_get_aabb");
	return AABB();
}

uint32_t RendererTextureStorageFixedFunction::decal_get_cull_mask(RID p_decal) const {
	_unsupported("decal_get_cull_mask");
	return 0;
}

void RendererTextureStorageFixedFunction::texture_add_to_decal_atlas(RID p_texture, bool p_panorama_to_dp) {
	_report_error(ERR_UNAVAILABLE, "texture_add_to_decal_atlas");
}

void RendererTextureStorageFixedFunction::texture_remove_from_decal_atlas(RID p_texture, bool p_panorama_to_dp) {
	_report_error(ERR_UNAVAILABLE, "texture_remove_from_decal_atlas");
}

RID RendererTextureStorageFixedFunction::decal_instance_create(RID p_decal) {
	_report_error(ERR_UNAVAILABLE, "decal_instance_create");
	return RID();
}

void RendererTextureStorageFixedFunction::decal_instance_free(RID p_decal_instance) {
	_report_error(ERR_UNAVAILABLE, "decal_instance_free");
}

void RendererTextureStorageFixedFunction::decal_instance_set_transform(RID p_decal_instance, const Transform3D &p_transform) {
	_report_error(ERR_UNAVAILABLE, "decal_instance_set_transform");
}

void RendererTextureStorageFixedFunction::decal_instance_set_sorting_offset(RID p_decal_instance, float p_sorting_offset) {
	_report_error(ERR_UNAVAILABLE, "decal_instance_set_sorting_offset");
}

void RendererTextureStorageFixedFunction::render_target_set_transparent(RID p_render_target, bool p_is_transparent) {
	_report_error(_texture_storage.render_target_set_flag(p_render_target, FixedFunctionTextureStorage::RENDER_TARGET_TRANSPARENT, p_is_transparent), "render_target_set_transparent");
}

bool RendererTextureStorageFixedFunction::render_target_get_transparent(RID p_render_target) const {
	return false;
}

void RendererTextureStorageFixedFunction::render_target_set_direct_to_screen(RID p_render_target, bool p_direct_to_screen) {
	_report_error(_texture_storage.render_target_set_flag(p_render_target, FixedFunctionTextureStorage::RENDER_TARGET_DIRECT_TO_SCREEN, p_direct_to_screen), "render_target_set_direct_to_screen");
}

bool RendererTextureStorageFixedFunction::render_target_get_direct_to_screen(RID p_render_target) const {
	return false;
}

void RendererTextureStorageFixedFunction::render_target_set_msaa(RID p_render_target, RSE::ViewportMSAA p_msaa) {
	if (p_msaa < RSE::VIEWPORT_MSAA_DISABLED || p_msaa >= RSE::VIEWPORT_MSAA_MAX) {
		_report_error(ERR_INVALID_PARAMETER, "render_target_set_msaa");
		return;
	}
	_report_msaa_error(_texture_storage.render_target_set_msaa(p_render_target, uint32_t(p_msaa)), "render_target_set_msaa");
}

RSE::ViewportMSAA RendererTextureStorageFixedFunction::render_target_get_msaa(RID p_render_target) const {
	return RSE::VIEWPORT_MSAA_DISABLED;
}

void RendererTextureStorageFixedFunction::render_target_set_msaa_needs_resolve(RID p_render_target, bool p_needs_resolve) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_set_msaa_needs_resolve");
		return;
	}
	_report_msaa_error(p_needs_resolve ? ERR_UNAVAILABLE : OK, "render_target_set_msaa_needs_resolve");
}

bool RendererTextureStorageFixedFunction::render_target_get_msaa_needs_resolve(RID p_render_target) const {
	return false;
}

void RendererTextureStorageFixedFunction::render_target_do_msaa_resolve(RID p_render_target) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_do_msaa_resolve");
		return;
	}
	_report_error(OK, "render_target_do_msaa_resolve");
}

void RendererTextureStorageFixedFunction::render_target_set_use_hdr(RID p_render_target, bool p_use_hdr) {
	_report_error(_texture_storage.render_target_set_flag(p_render_target, FixedFunctionTextureStorage::RENDER_TARGET_HDR, p_use_hdr), "render_target_set_use_hdr");
}

bool RendererTextureStorageFixedFunction::render_target_is_using_hdr(RID p_render_target) const {
	return false;
}

void RendererTextureStorageFixedFunction::render_target_set_use_debanding(RID p_render_target, bool p_use_debanding) {
	_report_error(_texture_storage.render_target_set_debanding(p_render_target, p_use_debanding), "render_target_set_use_debanding");
}

bool RendererTextureStorageFixedFunction::render_target_is_using_debanding(RID p_render_target) const {
	return false;
}

void RendererTextureStorageFixedFunction::render_target_do_clear_request(RID p_render_target) {
	_report_error(_render_targets ? _render_targets->select_render_target(p_render_target) : ERR_UNCONFIGURED, "render_target_do_clear_request");
	if (_last_error == OK) {
		_report_error(_render_targets->begin_draw(), "render_target_do_clear_request");
	}
}

void RendererTextureStorageFixedFunction::render_target_set_sdf_size_and_scale(RID p_render_target, RSE::ViewportSDFOversize p_size, RSE::ViewportSDFScale p_scale) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_set_sdf_size_and_scale");
		return;
	}
	_report_error(p_size >= 0 && p_size < RSE::VIEWPORT_SDF_OVERSIZE_MAX && p_scale >= 0 && p_scale < RSE::VIEWPORT_SDF_SCALE_MAX ? OK : ERR_INVALID_PARAMETER, "render_target_set_sdf_size_and_scale");
}

Rect2i RendererTextureStorageFixedFunction::render_target_get_sdf_rect(RID p_render_target) const {
	return _texture_storage.render_target_get_rect(p_render_target);
}

void RendererTextureStorageFixedFunction::render_target_mark_sdf_enabled(RID p_render_target, bool p_enabled) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_mark_sdf_enabled");
		return;
	}
	_report_error(p_enabled ? ERR_UNAVAILABLE : OK, "render_target_mark_sdf_enabled");
}

void RendererTextureStorageFixedFunction::render_target_set_vrs_mode(RID p_render_target, RSE::ViewportVRSMode p_mode) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_set_vrs_mode");
		return;
	}
	_report_error(p_mode == RSE::VIEWPORT_VRS_DISABLED ? OK : ERR_UNAVAILABLE, "render_target_set_vrs_mode");
}

RSE::ViewportVRSMode RendererTextureStorageFixedFunction::render_target_get_vrs_mode(RID p_render_target) const {
	return RSE::VIEWPORT_VRS_DISABLED;
}

void RendererTextureStorageFixedFunction::render_target_set_vrs_update_mode(RID p_render_target, RSE::ViewportVRSUpdateMode p_mode) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_set_vrs_update_mode");
		return;
	}
	_report_error(p_mode >= 0 && p_mode < RSE::VIEWPORT_VRS_UPDATE_MAX ? OK : ERR_INVALID_PARAMETER, "render_target_set_vrs_update_mode");
}

RSE::ViewportVRSUpdateMode RendererTextureStorageFixedFunction::render_target_get_vrs_update_mode(RID p_render_target) const {
	return RSE::VIEWPORT_VRS_UPDATE_ONCE;
}

void RendererTextureStorageFixedFunction::render_target_set_vrs_texture(RID p_render_target, RID p_texture) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_set_vrs_texture");
		return;
	}
	_report_error(p_texture.is_valid() ? ERR_UNAVAILABLE : OK, "render_target_set_vrs_texture");
}

RID RendererTextureStorageFixedFunction::render_target_get_vrs_texture(RID p_render_target) const {
	return RID();
}

void RendererTextureStorageFixedFunction::render_target_set_override(RID p_render_target, RID p_color_texture, RID p_depth_texture, RID p_velocity_texture, RID p_velocity_depth_texture) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_set_override");
		return;
	}
	_report_error(p_color_texture.is_valid() || p_depth_texture.is_valid() || p_velocity_texture.is_valid() || p_velocity_depth_texture.is_valid() ? ERR_UNAVAILABLE : OK, "render_target_set_override");
}

RID RendererTextureStorageFixedFunction::render_target_get_override_color(RID p_render_target) const {
	return RID();
}

RID RendererTextureStorageFixedFunction::render_target_get_override_depth(RID p_render_target) const {
	return RID();
}

RID RendererTextureStorageFixedFunction::render_target_get_override_velocity(RID p_render_target) const {
	return RID();
}

RID RendererTextureStorageFixedFunction::render_target_get_override_velocity_depth(RID p_render_target) const {
	return RID();
}

void RendererTextureStorageFixedFunction::render_target_set_render_region(RID p_render_target, const Rect2i &p_render_region) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_set_render_region");
		return;
	}
	_report_error(p_render_region == Rect2i() ? OK : ERR_UNAVAILABLE, "render_target_set_render_region");
}

Rect2i RendererTextureStorageFixedFunction::render_target_get_render_region(RID p_render_target) const {
	return Rect2i();
}

void RendererTextureStorageFixedFunction::render_target_set_subsampled_enabled(RID p_render_target, bool p_enabled) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_set_subsampled_enabled");
		return;
	}
	_report_error(p_enabled ? ERR_UNAVAILABLE : OK, "render_target_set_subsampled_enabled");
}

bool RendererTextureStorageFixedFunction::render_target_is_subsampled_enabled(RID p_render_target) const {
	return false;
}

void RendererTextureStorageFixedFunction::render_target_set_subsampled_allowed(RID p_render_target, bool p_allowed) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_set_subsampled_allowed");
		return;
	}
	_last_error = OK; // Permission alone does not enable the unavailable subsampling pass.
}

bool RendererTextureStorageFixedFunction::render_target_is_subsampled_allowed(RID p_render_target) const {
	return false;
}

void RendererTextureStorageFixedFunction::render_target_set_velocity_target_size(RID p_render_target, const Size2i &p_target_size) {
	const Error target_error = _texture_storage.render_target_check(p_render_target);
	if (target_error != OK) {
		_report_error(target_error, "render_target_set_velocity_target_size");
		return;
	}
	_report_error(p_target_size == Size2i() ? OK : ERR_UNAVAILABLE, "render_target_set_velocity_target_size");
}

Size2i RendererTextureStorageFixedFunction::render_target_get_velocity_target_size(RID p_render_target) const {
	return Size2i();
}

void RendererTextureStorageFixedFunction::texture_2d_attach_streaming_state(RID p_texture, RID p_streaming_state) {
	_report_error(ERR_UNAVAILABLE, "texture_2d_attach_streaming_state");
}
