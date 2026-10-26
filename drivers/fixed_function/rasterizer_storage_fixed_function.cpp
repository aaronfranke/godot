#include "rasterizer_storage_fixed_function.h"

#include "scene_storage.h"

void RasterizerStorageFixedFunction::_report_unavailable(const char *p_operation) {
	ERR_PRINT(String("Fixed Function storage ") + p_operation + " is unavailable in the current profile.");
}

void RasterizerStorageFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP && p_error != ERR_BUSY) {
		ERR_PRINT(String("Fixed Function storage ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

Error RasterizerStorageFixedFunction::_validate_render_target(RID p_render_target) const {
	if (!_texture_storage.owns_render_target(p_render_target)) {
		return ERR_INVALID_PARAMETER;
	}
	return _texture_storage.texture_can_be_referenced(_texture_storage.render_target_get_texture(p_render_target)) ? OK : ERR_ALREADY_IN_USE;
}

RID RasterizerStorageFixedFunction::texture_create() {
	const RID texture = _texture_storage.texture_create(true);
	_report_error(texture.is_valid() ? OK : ERR_CANT_CREATE, "texture_create");
	return texture;
}

Error RasterizerStorageFixedFunction::get_sampler_flags(uint32_t p_flags, const FixedFunctionGraphicsDevice::FFCapabilities &p_caps, uint8_t &r_sampler_flags) {
	const uint32_t known_flags = VS::TEXTURE_FLAGS_DEFAULT | VS::TEXTURE_FLAG_ANISOTROPIC_FILTER | VS::TEXTURE_FLAG_CONVERT_TO_LINEAR | VS::TEXTURE_FLAG_MIRRORED_REPEAT | VS::TEXTURE_FLAG_USED_FOR_STREAMING;
	if (p_flags & ~known_flags) {
		return ERR_INVALID_PARAMETER;
	}
	if (!p_caps.max_texture_stages) {
		return ERR_UNCONFIGURED;
	}
	// Fixed Function samples the stored colors like GLES2. The sRGB hint does
	// not change the sampler or prevent an imported texture from being used.
	const bool mirrored = p_flags & VS::TEXTURE_FLAG_MIRRORED_REPEAT;
	if (!mirrored && (p_flags & VS::TEXTURE_FLAG_REPEAT) && !p_caps.sampler_repeat) {
		return ERR_UNAVAILABLE;
	}
	uint8_t sampler = 0;
	// FILTER is a preference in the 3.x API; use nearest when linear is unavailable.
	if ((p_flags & VS::TEXTURE_FLAG_FILTER) && p_caps.sampler_filter_linear) {
		sampler |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
	}
	if ((p_flags & VS::TEXTURE_FLAG_ANISOTROPIC_FILTER) && p_caps.sampler_max_anisotropy > 1) {
		sampler |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_ANISOTROPIC;
	}
	if (mirrored && p_caps.sampler_repeat_mirrored) {
		sampler |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
	} else if ((mirrored || (p_flags & VS::TEXTURE_FLAG_REPEAT)) && p_caps.sampler_repeat) {
		sampler |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V;
	}
	if (p_flags & VS::TEXTURE_FLAG_MIPMAPS) {
		sampler |= FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
	}
	r_sampler_flags = sampler;
	return OK;
}

void RasterizerStorageFixedFunction::texture_allocate(RID p_texture, int p_width, int p_height, int p_depth_3d, Image::Format p_format, VS::TextureType p_type, uint32_t p_flags) {
	if (p_type < VS::TEXTURE_TYPE_2D || p_type > VS::TEXTURE_TYPE_3D) {
		_report_error(ERR_INVALID_PARAMETER, "texture_allocate");
		return;
	}
	if (p_type != VS::TEXTURE_TYPE_2D) {
		_report_error(ERR_UNAVAILABLE, "texture_allocate");
		return;
	}
	if (p_depth_3d != 0) {
		_report_error(ERR_INVALID_PARAMETER, "texture_allocate");
		return;
	}
	uint8_t sampler = 0;
	Error error = get_sampler_flags(p_flags, _texture_storage.get_device_capabilities(), sampler);
	if (error == OK) {
		Size2i size;
		size.x = p_width;
		size.y = p_height;
		error = _texture_storage.texture_request_allocate_2d(p_texture, size, p_format, p_flags, sampler);
	}
	_report_error(error, "texture_allocate");
}

void RasterizerStorageFixedFunction::texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer) {
	const Error error = p_layer == 0 ? _texture_storage.texture_request_data(p_texture, p_image) : ERR_UNAVAILABLE;
	if (error == ERR_UNAVAILABLE) {
		const FixedFunctionGraphicsDevice::FFCapabilities &caps = _texture_storage.get_device_capabilities();
		if (!caps.render_target_opaque || (p_layer == 0 && (caps.texture_power_of_two || caps.texture_square_only))) {
			_last_error = error;
			WARN_PRINT_ONCE("The Fixed Function device skipped an unsupported texture. Choose a renderer with broader texture support.");
			return;
		}
	}
	_report_error(error, "texture_set_data");
}

void RasterizerStorageFixedFunction::texture_set_data_partial(RID p_texture, const Ref<Image> &p_image, int p_src_x, int p_src_y, int p_src_width, int p_src_height, int p_dst_x, int p_dst_y, int p_dst_mip, int p_layer) {
	if (p_layer != 0 || p_dst_mip != 0) {
		_report_error(ERR_UNAVAILABLE, "texture_set_data_partial");
		return;
	}
	const Rect2i source(p_src_x, p_src_y, p_src_width, p_src_height);
	const Point2i destination(p_dst_x, p_dst_y);
	_report_error(_texture_storage.texture_request_data_partial(p_texture, p_image, source, destination), "texture_set_data_partial");
}

Ref<Image> RasterizerStorageFixedFunction::texture_get_data(RID p_texture, int p_layer) const {
	ERR_FAIL_COND_V_MSG(p_layer != 0, Ref<Image>(), "Fixed Function texture readback supports only layer zero.");
	return _texture_storage.texture_get_data(p_texture);
}

void RasterizerStorageFixedFunction::texture_set_flags(RID p_texture, uint32_t p_flags) {
	uint8_t sampler = 0;
	Error error = get_sampler_flags(p_flags, _texture_storage.get_device_capabilities(), sampler);
	if (error == OK) {
		error = _texture_storage.texture_set_flags(p_texture, p_flags, sampler);
	}
	_report_error(error, "texture_set_flags");
}

uint32_t RasterizerStorageFixedFunction::texture_get_flags(RID p_texture) const {
	return _texture_storage.texture_get_flags(p_texture);
}

Image::Format RasterizerStorageFixedFunction::texture_get_format(RID p_texture) const {
	return _texture_storage.texture_get_format(p_texture);
}

VS::TextureType RasterizerStorageFixedFunction::texture_get_type(RID p_texture) const {
	ERR_FAIL_COND_V(!_texture_storage.owns_texture(p_texture), VS::TEXTURE_TYPE_2D);
	return VS::TEXTURE_TYPE_2D;
}

uint32_t RasterizerStorageFixedFunction::texture_get_width(RID p_texture) const {
	return _texture_storage.texture_get_size(p_texture).x;
}

uint32_t RasterizerStorageFixedFunction::texture_get_height(RID p_texture) const {
	return _texture_storage.texture_get_size(p_texture).y;
}

uint32_t RasterizerStorageFixedFunction::texture_get_depth(RID p_texture) const {
	ERR_FAIL_COND_V(!_texture_storage.owns_texture(p_texture), 0);
	return 0;
}

void RasterizerStorageFixedFunction::texture_set_size_override(RID p_texture, int p_width, int p_height, int p_depth_3d) {
	Size2i size;
	size.x = p_width;
	size.y = p_height;
	_report_error(p_depth_3d == 0 ? _texture_storage.texture_set_size_override(p_texture, size) : ERR_INVALID_PARAMETER, "texture_set_size_override");
}

void RasterizerStorageFixedFunction::texture_set_path(RID p_texture, const String &p_path) {
	_report_error(_texture_storage.texture_set_path(p_texture, p_path), "texture_set_path");
}

String RasterizerStorageFixedFunction::texture_get_path(RID p_texture) const {
	return _texture_storage.texture_get_path(p_texture);
}

uint32_t RasterizerStorageFixedFunction::texture_get_texid(RID p_texture) const {
	uint32_t id = 0;
	const Error error = _queries.texture_get_texid(p_texture, id);
	ERR_FAIL_COND_V_MSG(error != OK, 0, "Fixed Function textures do not expose OpenGL identifiers.");
	return id;
}

void RasterizerStorageFixedFunction::texture_bind(RID p_texture, uint32_t p_texture_no) {
	_report_error(_queries.texture_bind(p_texture, p_texture_no), "texture_bind");
}

void RasterizerStorageFixedFunction::texture_set_shrink_all_x2_on_set_data(bool p_enable) {
	_report_error(_queries.texture_set_shrink_all_x2_on_set_data(p_enable), "texture_set_shrink_all_x2_on_set_data");
}

void RasterizerStorageFixedFunction::textures_keep_original(bool p_enable) {
	_queries.textures_keep_original(p_enable);
	_report_error(OK, "textures_keep_original");
}

void RasterizerStorageFixedFunction::texture_debug_usage(List<VS::TextureInfo> *r_info) {
	const Error error = _queries.texture_debug_usage(r_info);
	ERR_FAIL_COND_MSG(error != OK, "Fixed Function texture debug query failed (Error " + itos(error) + ").");
}

RID RasterizerStorageFixedFunction::texture_create_radiance_cubemap(RID p_source, int p_resolution) const {
	const Error error = _queries.texture_create_radiance_cubemap(p_source, p_resolution);
	ERR_FAIL_COND_V_MSG(error != OK, RID(), "Fixed Function radiance cubemaps are unavailable.");
	return RID();
}

void RasterizerStorageFixedFunction::texture_set_detect_3d_callback(RID p_texture, VS::TextureDetectCallback p_callback, void *p_userdata) {
	_report_error(_texture_storage.texture_set_detect_callback(p_texture, FixedFunctionTextureStorage::DETECT_3D, p_callback, p_userdata), "texture_set_detect_3d_callback");
}

void RasterizerStorageFixedFunction::texture_set_detect_srgb_callback(RID p_texture, VS::TextureDetectCallback p_callback, void *p_userdata) {
	_report_error(_texture_storage.texture_set_detect_callback(p_texture, FixedFunctionTextureStorage::DETECT_SRGB, p_callback, p_userdata), "texture_set_detect_srgb_callback");
}

void RasterizerStorageFixedFunction::texture_set_detect_normal_callback(RID p_texture, VS::TextureDetectCallback p_callback, void *p_userdata) {
	_report_error(_texture_storage.texture_set_detect_callback(p_texture, FixedFunctionTextureStorage::DETECT_NORMAL, p_callback, p_userdata), "texture_set_detect_normal_callback");
}

void RasterizerStorageFixedFunction::texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) {
	_report_error(_texture_storage.texture_set_force_redraw_if_visible(p_texture, p_enable), "texture_set_force_redraw_if_visible");
}

void RasterizerStorageFixedFunction::texture_set_proxy(RID p_texture, RID p_proxy) {
	_report_error(_texture_storage.texture_set_proxy(p_texture, p_proxy), "texture_set_proxy");
}

Size2 RasterizerStorageFixedFunction::texture_size_with_proxy(RID p_texture) const {
	const Size2i size = _texture_storage.texture_size_with_proxy(p_texture);
	return Size2(size.x, size.y);
}

RID RasterizerStorageFixedFunction::shader_create() {
	const RID shader = _material_storage.shader_create();
	_report_error(shader.is_valid() ? OK : ERR_CANT_CREATE, "shader_create");
	return shader;
}

void RasterizerStorageFixedFunction::shader_set_code(RID p_shader, const String &p_code) {
	_report_error(_material_storage.shader_set_code(p_shader, p_code), "shader_set_code");
}

String RasterizerStorageFixedFunction::shader_get_code(RID p_shader) const {
	return _material_storage.shader_get_code(p_shader);
}

void RasterizerStorageFixedFunction::shader_get_param_list(RID p_shader, List<PropertyInfo> *r_parameters) const {
	const Error error = _material_storage.shader_get_param_list(p_shader, r_parameters);
	ERR_FAIL_COND_MSG(error != OK, "Fixed Function shader reflection failed (Error " + itos(error) + ").");
}

void RasterizerStorageFixedFunction::shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture) {
	if (p_texture.is_valid() && !_texture_storage.texture_can_be_referenced(p_texture)) {
		_report_error(ERR_INVALID_PARAMETER, "shader_set_default_texture_param");
		return;
	}
	_report_error(_material_storage.shader_set_default_texture_param(p_shader, p_name, p_texture), "shader_set_default_texture_param");
}

RID RasterizerStorageFixedFunction::shader_get_default_texture_param(RID p_shader, const StringName &p_name) const {
	return _material_storage.shader_get_default_texture_param(p_shader, p_name);
}

void RasterizerStorageFixedFunction::shader_add_custom_define(RID p_shader, const String &p_define) {
	_report_error(_material_storage.shader_add_custom_define(p_shader, p_define), "shader_add_custom_define");
}

void RasterizerStorageFixedFunction::shader_get_custom_defines(RID p_shader, Vector<String> *r_defines) const {
	const Error error = _material_storage.shader_get_custom_defines(p_shader, r_defines);
	ERR_FAIL_COND_MSG(error != OK, "Fixed Function shader define query failed (Error " + itos(error) + ").");
}

void RasterizerStorageFixedFunction::shader_remove_custom_define(RID p_shader, const String &p_define) {
	_report_error(_material_storage.shader_remove_custom_define(p_shader, p_define), "shader_remove_custom_define");
}

void RasterizerStorageFixedFunction::set_shader_async_hidden_forbidden(bool p_forbidden) {
	_report_error(_material_storage.set_shader_async_hidden_forbidden(p_forbidden), "set_shader_async_hidden_forbidden");
}

bool RasterizerStorageFixedFunction::is_shader_async_hidden_forbidden() {
	return _material_storage.is_shader_async_hidden_forbidden();
}

RID RasterizerStorageFixedFunction::material_create() {
	const RID material = _material_storage.material_create();
	_report_error(material.is_valid() ? OK : ERR_CANT_CREATE, "material_create");
	return material;
}

void RasterizerStorageFixedFunction::material_set_render_priority(RID p_material, int p_priority) {
	_report_error(_material_storage.material_set_render_priority(p_material, p_priority), "material_set_render_priority");
}

void RasterizerStorageFixedFunction::material_set_line_width(RID p_material, float p_width) {
	_report_error(_material_storage.material_set_line_width(p_material, p_width), "material_set_line_width");
}

void RasterizerStorageFixedFunction::material_set_next_pass(RID p_material, RID p_next_material) {
	_report_error(_material_storage.material_set_next_pass(p_material, p_next_material), "material_set_next_pass");
}

void RasterizerStorageFixedFunction::material_set_shader(RID p_material, RID p_shader) {
	_report_error(_material_storage.material_set_shader(p_material, p_shader), "material_set_shader");
}

RID RasterizerStorageFixedFunction::material_get_shader(RID p_material) const {
	return _material_storage.material_get_shader(p_material);
}

void RasterizerStorageFixedFunction::material_set_param(RID p_material, const StringName &p_name, const Variant &p_value) {
	_report_error(_material_storage.material_set_param(p_material, p_name, p_value), "material_set_param");
}

Variant RasterizerStorageFixedFunction::material_get_param(RID p_material, const StringName &p_name) const {
	return _material_storage.material_get_param(p_material, p_name);
}

Variant RasterizerStorageFixedFunction::material_get_param_default(RID p_material, const StringName &p_name) const {
	return _material_storage.material_get_param_default(p_material, p_name);
}

bool RasterizerStorageFixedFunction::material_is_animated(RID p_material) {
	FixedFunctionShaderUsage::FFFeatures features;
	const Error error = _material_storage.material_get_features(p_material, features);
	ERR_FAIL_COND_V_MSG(error != OK, false, "Fixed Function material query failed (Error " + itos(error) + ").");
	return features.animated;
}

bool RasterizerStorageFixedFunction::material_casts_shadows(RID p_material) {
	FixedFunctionShaderUsage::FFFeatures features;
	const Error error = _material_storage.material_get_features(p_material, features);
	ERR_FAIL_COND_V_MSG(error != OK, false, "Fixed Function material query failed (Error " + itos(error) + ").");
	return features.casts_shadows;
}

bool RasterizerStorageFixedFunction::material_uses_tangents(RID p_material) {
	FixedFunctionShaderUsage::FFFeatures features;
	const Error error = _material_storage.material_get_features(p_material, features, false);
	ERR_FAIL_COND_V_MSG(error != OK, false, "Fixed Function material query failed (Error " + itos(error) + ").");
	return features.uses_tangents;
}

bool RasterizerStorageFixedFunction::material_uses_ensure_correct_normals(RID p_material) {
	FixedFunctionShaderUsage::FFFeatures features;
	const Error error = _material_storage.material_get_features(p_material, features, false);
	ERR_FAIL_COND_V_MSG(error != OK, false, "Fixed Function material query failed (Error " + itos(error) + ").");
	return features.ensure_correct_normals;
}

void RasterizerStorageFixedFunction::material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) {
	_report_error(_material_storage.material_add_instance_owner(p_material, p_instance), "material_add_instance_owner");
}

void RasterizerStorageFixedFunction::material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) {
	_report_error(_material_storage.material_remove_instance_owner(p_material, p_instance), "material_remove_instance_owner");
}

RID RasterizerStorageFixedFunction::render_target_create() {
	const RID target = _texture_storage.render_target_create();
	_report_error(target.is_valid() ? OK : ERR_CANT_CREATE, "render_target_create");
	return target;
}

void RasterizerStorageFixedFunction::render_target_set_position(RID p_render_target, int p_x, int p_y) {
	_report_error(_texture_storage.render_target_set_position(p_render_target, p_x, p_y), "render_target_set_position");
}

void RasterizerStorageFixedFunction::render_target_set_size(RID p_render_target, int p_width, int p_height) {
	_report_error(_texture_storage.render_target_set_size(p_render_target, p_width, p_height), "render_target_set_size");
}

RID RasterizerStorageFixedFunction::render_target_get_texture(RID p_render_target) const {
	return _texture_storage.render_target_get_texture(p_render_target);
}

uint32_t RasterizerStorageFixedFunction::render_target_get_depth_texture_id(RID p_render_target) const {
	ERR_FAIL_COND_V(_validate_render_target(p_render_target) != OK, 0);
	// No depth texture exists in this profile. Never expose a COM pointer as a GL name.
	return 0;
}

void RasterizerStorageFixedFunction::render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id, unsigned int p_depth_id) {
	Error error = _validate_render_target(p_render_target);
	if (error == OK && (p_texture_id || p_depth_id)) {
		error = ERR_UNAVAILABLE;
	}
	_report_error(error, "render_target_set_external_texture");
}

void RasterizerStorageFixedFunction::render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) {
	FixedFunctionTextureStorage::FFRenderTargetFlag flag;
	switch (p_flag) {
		case RENDER_TARGET_VFLIP:
			flag = FixedFunctionTextureStorage::RENDER_TARGET_VFLIP;
			break;
		case RENDER_TARGET_TRANSPARENT:
			flag = FixedFunctionTextureStorage::RENDER_TARGET_TRANSPARENT;
			break;
		case RENDER_TARGET_NO_3D_EFFECTS:
			flag = FixedFunctionTextureStorage::RENDER_TARGET_NO_3D_EFFECTS;
			break;
		case RENDER_TARGET_NO_3D:
			flag = FixedFunctionTextureStorage::RENDER_TARGET_NO_3D;
			break;
		case RENDER_TARGET_NO_SAMPLING:
			flag = FixedFunctionTextureStorage::RENDER_TARGET_NO_SAMPLING;
			break;
		case RENDER_TARGET_HDR:
			flag = FixedFunctionTextureStorage::RENDER_TARGET_HDR;
			break;
		case RENDER_TARGET_KEEP_3D_LINEAR:
			flag = FixedFunctionTextureStorage::RENDER_TARGET_KEEP_3D_LINEAR;
			break;
		case RENDER_TARGET_DIRECT_TO_SCREEN:
			flag = FixedFunctionTextureStorage::RENDER_TARGET_DIRECT_TO_SCREEN;
			break;
		case RENDER_TARGET_USE_32_BPC_DEPTH:
			flag = FixedFunctionTextureStorage::RENDER_TARGET_USE_32_BPC_DEPTH;
			break;
		default:
			_report_error(ERR_INVALID_PARAMETER, "render_target_set_flag");
			return;
	}
	_report_error(_texture_storage.render_target_set_flag(p_render_target, flag, p_value), "render_target_set_flag");
}

void RasterizerStorageFixedFunction::render_target_set_msaa(RID p_render_target, VS::ViewportMSAA p_msaa) {
	if (p_msaa < VS::VIEWPORT_MSAA_DISABLED || p_msaa > VS::VIEWPORT_MSAA_EXT_4X) {
		_report_error(ERR_INVALID_PARAMETER, "render_target_set_msaa");
		return;
	}
	const uint32_t samples[] = { 0, 2, 4, 8, 16, 2, 4 };
	const Error error = _texture_storage.render_target_set_msaa(p_render_target, samples[p_msaa]);
	if (error == ERR_UNAVAILABLE) {
		_last_error = error;
		WARN_PRINT_ONCE("Fixed Function does not support viewport MSAA. Rendering without MSAA.");
		return;
	}
	_report_error(error, "render_target_set_msaa");
}

void RasterizerStorageFixedFunction::render_target_set_use_fxaa(RID p_render_target, bool p_enabled) {
	_report_error(_texture_storage.render_target_set_fxaa(p_render_target, p_enabled), "render_target_set_use_fxaa");
}

void RasterizerStorageFixedFunction::render_target_set_use_debanding(RID p_render_target, bool p_enabled) {
	_report_error(_texture_storage.render_target_set_debanding(p_render_target, p_enabled), "render_target_set_use_debanding");
}

void RasterizerStorageFixedFunction::render_target_set_sharpen_intensity(RID p_render_target, float p_intensity) {
	_report_error(_texture_storage.render_target_set_sharpen_intensity(p_render_target, p_intensity), "render_target_set_sharpen_intensity");
}

bool RasterizerStorageFixedFunction::render_target_was_used(RID p_render_target) {
	return _texture_storage.render_target_was_used(p_render_target);
}

RID RasterizerStorageFixedFunction::sky_create() {
	const RID sky = _scene_storage.sky_create();
	_report_error(sky.is_valid() ? OK : ERR_CANT_CREATE, "sky_create");
	return sky;
}

void RasterizerStorageFixedFunction::sky_set_texture(RID p_sky, RID p_panorama, int p_radiance_size) {
	if (p_radiance_size < 0 || (p_panorama.is_valid() && !_texture_storage.texture_can_be_referenced(p_panorama))) {
		_report_error(ERR_INVALID_PARAMETER, "sky_set_texture");
		return;
	}
	// The visible panorama uses the source texture directly. Radiance is a separate path.
	_report_error(_scene_storage.sky_set_panorama(p_sky, p_panorama), "sky_set_texture");
}

void RasterizerStorageFixedFunction::render_target_clear_used(RID p_render_target) {
	_report_error(_texture_storage.render_target_clear_used(p_render_target), "render_target_clear_used");
}

bool RasterizerStorageFixedFunction::free(RID p_rid) {
	if (_scene_storage.owns_sky(p_rid)) {
		_report_error(_scene_storage.free(p_rid), "free");
		return true;
	}
	if (_light_storage.owns_light(p_rid)) {
		_report_error(_light_storage.free(p_rid), "free");
		return true;
	}
	if (_mesh_storage.owns_mesh(p_rid) || _mesh_storage.owns_skeleton(p_rid) || _mesh_storage.owns_multimesh(p_rid) || _mesh_storage.owns_immediate(p_rid)) {
		_report_error(_mesh_storage.free(p_rid), "free");
		return true;
	}
	if (_material_storage.owns_shader(p_rid) || _material_storage.owns_material(p_rid)) {
		_report_error(_material_storage.free(p_rid), "free");
		return true;
	}
	if (!_texture_storage.owns_texture(p_rid) && !_texture_storage.owns_render_target(p_rid)) {
		return false;
	}
	_report_error(_texture_storage.request_free(p_rid), "free");
	return true;
}

void RasterizerStorageFixedFunction::update_dirty_resources() {
	// Target records remain alive until the context reaches a safe frame boundary.
	Error error = _material_storage.update_dependencies();
	if (error == OK) {
		error = _mesh_storage.update_dependencies();
	}
	if (error == OK) {
		error = _light_storage.update_dependencies();
	}
	_report_error(error == OK ? _texture_storage.update_pending() : error, "update_dirty_resources");
}

VS::InstanceType RasterizerStorageFixedFunction::get_base_type(RID p_rid) const {
	return _queries.get_base_type(p_rid);
}

bool RasterizerStorageFixedFunction::has_os_feature(const String &p_feature) const {
	return _queries.has_os_feature(p_feature);
}

void RasterizerStorageFixedFunction::set_debug_generate_wireframes(bool p_generate) {
	_report_error(_queries.set_debug_generate_wireframes(p_generate), "set_debug_generate_wireframes");
}

void RasterizerStorageFixedFunction::render_info_begin_capture() {
	_report_error(_queries.render_info_begin_capture(), "render_info_begin_capture");
}

void RasterizerStorageFixedFunction::render_info_end_capture() {
	_report_error(_queries.render_info_end_capture(), "render_info_end_capture");
}

int RasterizerStorageFixedFunction::get_captured_render_info(VS::RenderInfo p_info) {
	int value = 0;
	const Error error = _queries.get_captured_render_info(p_info, value);
	ERR_FAIL_COND_V_MSG(error != OK, 0, "Invalid Fixed Function render-info capture query.");
	return value;
}

uint64_t RasterizerStorageFixedFunction::get_render_info(VS::RenderInfo p_info) {
	uint64_t value = 0;
	const Error error = _queries.get_render_info(p_info, value);
	ERR_FAIL_COND_V_MSG(error != OK, 0, "Invalid Fixed Function render-info query.");
	return value;
}

String RasterizerStorageFixedFunction::get_video_adapter_name() const { return _queries.get_video_adapter_name(); }
String RasterizerStorageFixedFunction::get_video_adapter_vendor() const { return _queries.get_video_adapter_vendor(); }

RasterizerStorageFixedFunction::RasterizerStorageFixedFunction(FixedFunctionTextureStorage &p_texture_storage, FixedFunctionMaterialStorage &p_material_storage, FixedFunctionMeshStorage &p_mesh_storage, FixedFunctionLightStorage &p_light_storage, FixedFunctionSceneStorage &p_scene_storage, FixedFunctionStorageQueries &p_queries) :
		_texture_storage(p_texture_storage),
		_material_storage(p_material_storage),
		_mesh_storage(p_mesh_storage),
		_light_storage(p_light_storage),
		_scene_storage(p_scene_storage),
		_queries(p_queries),
		_last_error(OK) {
}

RasterizerStorageFixedFunction::~RasterizerStorageFixedFunction() {
	// Upstream's base destructor leaves this pointer behind. Do not retain a dead facade.
	if (base_singleton == this) {
		base_singleton = nullptr;
	}
}
