#include "graphics_device.h"

Color FixedFunctionGraphicsDevice::FFFog::get_blend_color(FFBlendMode p_blend_mode) const {
	switch (p_blend_mode) {
		case BLEND_MODE_ADD:
		case BLEND_MODE_ADD_UNWEIGHTED:
			return Color(0, 0, 0);
		case BLEND_MODE_MUL:
			return Color(1, 1, 1);
		default:
			return color;
	}
}

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"

FixedFunctionGraphicsDevice::FixedFunctionGraphicsDevice() :
		_adapter_vendor_id(0) {}

FixedFunctionGraphicsDevice::FFTextureInfo::FFTextureInfo() :
		bytes(0) {}

String FixedFunctionGraphicsDevice::get_adapter_name() const {
	return _adapter_name.is_empty() ? String("Unknown") : _adapter_name;
}

String FixedFunctionGraphicsDevice::get_adapter_vendor() const {
	// Preserve the reported identifier without guessing names from driver descriptions.
	return _adapter_vendor_id ? String("PCI vendor 0x") + String::num_uint64(_adapter_vendor_id, 16).pad_zeros(4) : String("Unknown");
}

Error FixedFunctionGraphicsDevice::texture_get_info(RID p_texture, FFTextureInfo &r_info) const {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::clear_depth(float p_depth) {
	return ERR_UNAVAILABLE;
}

Rect2i FixedFunctionGraphicsDevice::get_draw_viewport(const FFDrawParameters &p_state) const {
	if (!p_state.clip_enabled) {
		return Rect2i(0, 0, get_target_width(), get_target_height());
	}
	// Widen before adding: a valid rectangle may extend beyond signed integer bounds.
	const int64_t left = CLAMP(int64_t(p_state.clip_rect.position.x), int64_t(0), int64_t(get_target_width()));
	const int64_t top = CLAMP(int64_t(p_state.clip_rect.position.y), int64_t(0), int64_t(get_target_height()));
	const int64_t right = CLAMP(int64_t(p_state.clip_rect.position.x) + p_state.clip_rect.size.x, left, int64_t(get_target_width()));
	const int64_t bottom = CLAMP(int64_t(p_state.clip_rect.position.y) + p_state.clip_rect.size.y, top, int64_t(get_target_height()));
	return Rect2i(int(left), int(top), int(right - left), int(bottom - top));
}

Projection FixedFunctionGraphicsDevice::get_viewport_projection(const Projection &p_projection, const Rect2i &p_viewport) const {
	return get_viewport_projection(p_projection, p_viewport, Size2i(get_target_width(), get_target_height()));
}

Projection FixedFunctionGraphicsDevice::get_viewport_projection(const Projection &p_projection, const Rect2i &p_viewport, const Size2i &p_target_size) {
	if (p_viewport == Rect2i(Point2i(), p_target_size)) {
		return p_projection;
	}
	// Cancel the smaller viewport's scale/offset, retaining the original screen mapping.
	// The native homogeneous clipper then clips against the rectangle, including in D3D7.
	const real_t scale_x = real_t(p_target_size.x) / p_viewport.size.x;
	const real_t scale_y = real_t(p_target_size.y) / p_viewport.size.y;
	const real_t offset_x = (real_t(p_target_size.x) - real_t(2) * p_viewport.position.x - p_viewport.size.x) / p_viewport.size.x;
	const real_t offset_y = (real_t(2) * p_viewport.position.y + p_viewport.size.y - real_t(p_target_size.y)) / p_viewport.size.y;
	Projection projection = p_projection;
	for (int column = 0; column < 4; column++) {
		projection.columns[column][0] = p_projection.columns[column][0] * scale_x + p_projection.columns[column][3] * offset_x;
		projection.columns[column][1] = p_projection.columns[column][1] * scale_y + p_projection.columns[column][3] * offset_y;
	}
	return projection;
}

uint32_t FixedFunctionGraphicsDevice::get_alpha_reference(float p_cutoff) {
	// Reject below the threshold, preserving equality for native 8-bit alpha.
	return uint32_t(Math::ceil(CLAMP(p_cutoff, 0.0f, 1.0f) * 255.0f));
}

Error FixedFunctionGraphicsDevice::validate_draw_state(const FFDrawParameters &p_state) const {
	if (p_state.sampler_flags & ~SAMPLER_FLAGS_MASK) {
		return ERR_INVALID_PARAMETER;
	}
	const FFCapabilities &caps = get_capabilities();
	if (p_state.cull_mode >= CULL_MODE_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	if ((p_state.cull_mode == CULL_BACK && !caps.cull_back) || (p_state.cull_mode == CULL_FRONT && !caps.cull_front)) {
		return ERR_UNAVAILABLE;
	}
	if ((p_state.depth_test_enabled || p_state.depth_write_enabled) && !caps.depth_buffer) {
		return ERR_UNAVAILABLE;
	}
	if ((p_state.sampler_flags & SAMPLER_FILTER_LINEAR) && !caps.sampler_filter_linear) {
		return ERR_UNAVAILABLE;
	}
	const bool repeat_u = (p_state.sampler_flags & SAMPLER_REPEAT_U) != 0;
	const bool repeat_v = (p_state.sampler_flags & SAMPLER_REPEAT_V) != 0;
	const bool mirror = p_state.sampler_flags & SAMPLER_REPEAT_MIRRORED;
	if (mirror && !repeat_u && !repeat_v) {
		return ERR_INVALID_PARAMETER;
	}
	const bool can_repeat = mirror ? caps.sampler_repeat_mirrored : caps.sampler_repeat;
	if (((repeat_u || repeat_v) && !can_repeat) || (repeat_u != repeat_v && !caps.sampler_address_independent)) {
		return ERR_UNAVAILABLE;
	}
	if (p_state.clip_enabled && (p_state.clip_rect.size.x < 0 || p_state.clip_rect.size.y < 0)) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_state.alpha_cutoff_enabled) {
		if (!Math::is_finite(p_state.alpha_cutoff)) {
			return ERR_INVALID_PARAMETER;
		}
		if (!get_capabilities().alpha_cutoff) {
			return ERR_UNAVAILABLE;
		}
	}
	switch (p_state.blend_mode) {
		case BLEND_MODE_DISABLED:
			return OK;
		case BLEND_MODE_MIX:
			return get_capabilities().blend_mix ? OK : ERR_UNAVAILABLE;
		case BLEND_MODE_PREMULTIPLIED_ALPHA:
			return get_capabilities().blend_premultiplied_alpha ? OK : ERR_UNAVAILABLE;
		case BLEND_MODE_ADD:
			return get_capabilities().blend_add ? OK : ERR_UNAVAILABLE;
		case BLEND_MODE_ADD_UNWEIGHTED:
			return get_capabilities().blend_add_unweighted ? OK : ERR_UNAVAILABLE;
		case BLEND_MODE_MUL:
			return get_capabilities().blend_mul ? OK : ERR_UNAVAILABLE;
		default:
			return ERR_INVALID_PARAMETER;
	}
}

Error FixedFunctionGraphicsDevice::validate_texture_size(uint32_t p_width, uint32_t p_height) const {
	if (!p_width || !p_height) {
		return ERR_INVALID_PARAMETER;
	}
	const FFCapabilities &caps = get_capabilities();
	if (!caps.max_texture_stages || p_width > caps.max_texture_width || p_height > caps.max_texture_height ||
			(caps.texture_square_only && p_width != p_height) ||
			(caps.texture_power_of_two && ((p_width & (p_width - 1)) || (p_height & (p_height - 1))))) {
		return ERR_UNAVAILABLE;
	}
	return OK;
}

Error FixedFunctionGraphicsDevice::validate_texture_image(const Ref<Image> &p_image) const {
	if (p_image.is_null() || p_image->is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_image->get_format() != Image::FORMAT_RGBA8 && !supports_compressed_texture(p_image->get_format())) {
		return ERR_UNAVAILABLE;
	}
	Size2i upload_size;
	return get_texture_upload_size(p_image->get_width(), p_image->get_height(), upload_size);
}

Error FixedFunctionGraphicsDevice::validate_triangle_indices(uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count) const {
	if (p_vertex_count > 65536 || p_index_count % 3 || p_index_count / 3 > get_capabilities().max_primitives_per_draw || (p_index_count && !p_indices)) {
		return ERR_INVALID_PARAMETER;
	}
	for (uint32_t i = 0; i < p_index_count; i++) {
		uint16_t index;
		memcpy(&index, p_indices + size_t(i) * sizeof(index), sizeof(index));
		if (index >= p_vertex_count) {
			return ERR_INVALID_DATA;
		}
	}
	return OK;
}

Error FixedFunctionGraphicsDevice::draw_indexed_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count, const Projection &p_projection, RID p_texture, const FFDrawParameters &p_state) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::draw_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices, uint32_t p_index_count) {
	return ERR_UNAVAILABLE;
}

FixedFunctionGraphicsDevice::FFGeometryDrawInfo::FFGeometryDrawInfo() :
		vertex_count(0), primitive_count(0), draw_calls(0), lod_count(0) {}

Error FixedFunctionGraphicsDevice::pack_geometry_color(const Color &p_color, uint32_t &r_color) {
	Color color = p_color;
	for (int i = 0; i < 4; i++) {
		if (Math::is_nan(color[i]) || Math::is_inf(color[i])) {
			return ERR_INVALID_PARAMETER;
		}
		color[i] = CLAMP(color[i], 0.0f, 1.0f);
	}
	r_color = color.to_argb32();
	return OK;
}

Error FixedFunctionGraphicsDevice::geometry_create(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::geometry_create_lod_batch(const FixedFunctionGeometryData::FFGeometryBatch &p_batch, RID &r_geometry) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::geometry_free(RID p_geometry) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::geometry_get_draw_info(RID p_geometry, FFGeometryDrawInfo &r_info) const {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::draw_geometry(RID p_geometry, const Projection &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state, const Color *p_color_override, uint32_t p_lod) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::geometry_create_lit(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) {
	return ERR_UNAVAILABLE;
}
Error FixedFunctionGraphicsDevice::draw_lit_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFOcclusionMapParameters *p_occlusion, uint32_t p_lod) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::draw_vertex_lit_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state, uint32_t p_lod) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::draw_vertex_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, const Color &p_color, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices, uint32_t p_index_count) {
	return ERR_UNAVAILABLE;
}

bool FixedFunctionGraphicsDevice::can_pack_vertex_ambient_color(const Color &p_ambient) {
	for (int channel = 0; channel < 3; channel++) {
		if (!(p_ambient[channel] >= 0 && p_ambient[channel] <= 1)) {
			return false;
		}
	}
	return true;
}

bool FixedFunctionGraphicsDevice::pack_vertex_ambient_color(const Color &p_ambient, uint32_t &r_color) {
	if (!can_pack_vertex_ambient_color(p_ambient)) {
		return false;
	}
	r_color = Color(p_ambient.r, p_ambient.g, p_ambient.b, 0).to_argb32();
	return true;
}

float FixedFunctionGraphicsDevice::get_vertex_light_quadratic_attenuation(float p_attenuation) {
	// Least-squares fits over the range: 3.0, 6.0, 14.5 and 25.0 for exponents 0.5, 1, 2 and 3.
	return p_attenuation > 0 ? 6.0f * Math::pow(p_attenuation, 1.3f) : 0.0f;
}

float FixedFunctionGraphicsDevice::get_vertex_light_spot_falloff(float p_spot_attenuation) {
	// Solve (1 - 0.5)^falloff = 1 - 0.5^attenuation. Attenuation 1 is exact.
	const float edge = 1.0f - Math::pow(0.5f, MAX(p_spot_attenuation, 0.001f));
	return CLAMP(Math::log(MAX(edge, 1.0e-6f)) / Math::log(0.5f), 0.01f, 64.0f);
}

FixedFunctionGraphicsDevice::FFLitGeometryParameters::FFLitGeometryParameters() : albedo_uv_scale(1, 1), color(1, 1, 1, 1) {}

FixedFunctionGraphicsDevice::FFNormalMapParameters::FFNormalMapParameters() : uv_scale(1, 1), strength(1), sampler_flags(0), flip_v(false), albedo_uses_normal_uv(false) {}

FixedFunctionGraphicsDevice::FFDirectionalShadowParameters::FFDirectionalShadowParameters() : color(0, 0, 0), bias(0), normal_bias(0), light_index(MAX_PIXEL_LIGHTS) {}
Error FixedFunctionGraphicsDevice::geometry_create_normal_mapped(const Vector<FixedFunctionNormalMapVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) {
	return ERR_UNAVAILABLE;
}
Error FixedFunctionGraphicsDevice::draw_normal_mapped_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters &p_normal, const FFOcclusionMapParameters *p_occlusion, uint32_t p_lod) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::prepare_rgba8_upload(const Ref<Image> &p_image, Ref<Image> &r_upload, bool p_rgbe_to_srgb) {
	if (p_image.is_null() || p_image->is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_image->get_format() == Image::FORMAT_RGBA8) {
		r_upload = p_image;
		return OK;
	}
	const int levels = p_image->get_mipmap_count() + 1;
	const bool mipmaps = levels > 1;
	const Vector<uint8_t> source = p_image->get_data();
	Vector<uint8_t> rgba;
	if (mipmaps) {
		const Error error = rgba.resize(Image::get_image_data_size(p_image->get_width(), p_image->get_height(), Image::FORMAT_RGBA8, true));
		if (error != OK) {
			return error;
		}
	}
	Ref<Image> level;
	int width = p_image->get_width(), height = p_image->get_height();
	int64_t source_offset = 0, rgba_offset = 0;
	for (int i = 0; i < levels; i++) {
		// Image::convert rebuilds mipmaps. Convert each supplied level independently,
		// including rectangular compressed tails where one dimension has reached one.
		const int64_t size = Image::get_image_data_size(width, height, p_image->get_format(), false);
		Vector<uint8_t> bytes;
		if (mipmaps) {
			if (size > source.size() - source_offset) {
				return ERR_INVALID_DATA;
			}
			const Error error = bytes.resize(size);
			if (error != OK) {
				return error;
			}
			memcpy(bytes.ptrw(), source.ptr() + source_offset, size);
		} else {
			bytes = source;
		}
		level = Image::create_from_data(width, height, false, p_image->get_format(), bytes);
		if (level->is_empty()) {
			return ERR_INVALID_DATA;
		}
		if (level->is_compressed()) {
			const Error error = level->decompress();
			if (error != OK) {
				return error;
			}
		}
		if (level->is_compressed() || level->get_width() != width || level->get_height() != height) {
			return ERR_INVALID_DATA;
		}
		if (p_rgbe_to_srgb && level->get_format() == Image::FORMAT_RGBE9995) {
			// Encode linear HDR panorama colors for LDR presentation.
			level = level->rgbe_to_srgb();
			if (level.is_null()) {
				return ERR_OUT_OF_MEMORY;
			}
		}
		level->convert(Image::FORMAT_RGBA8);
		const int64_t rgba_size = int64_t(width) * height * 4;
		const Vector<uint8_t> converted = level->get_data();
		if (level->get_format() != Image::FORMAT_RGBA8 || converted.size() != rgba_size) {
			return ERR_INVALID_DATA;
		}
		if (!mipmaps) {
			r_upload = level;
			return OK;
		}
		memcpy(rgba.ptrw() + rgba_offset, converted.ptr(), rgba_size);
		source_offset += size;
		rgba_offset += rgba_size;
		width = MAX(width >> 1, 1);
		height = MAX(height >> 1, 1);
	}
	r_upload = Image::create_from_data(p_image->get_width(), p_image->get_height(), true, Image::FORMAT_RGBA8, rgba);
	return r_upload->is_empty() ? ERR_INVALID_DATA : OK;
}

bool FixedFunctionGraphicsDevice::can_upload_compressed_texture(const Ref<Image> &p_image) const {
	// The DXT base level needs whole blocks. Smaller mip levels still occupy whole blocks.
	return p_image.is_valid() && supports_compressed_texture(p_image->get_format()) && p_image->get_width() % 4 == 0 && p_image->get_height() % 4 == 0;
}

bool FixedFunctionGraphicsDevice::copy_compressed_texture_rows(void *p_destination, int p_pitch, const uint8_t *p_source, uint32_t p_row_bytes, uint32_t p_rows) {
	if (!p_destination || p_pitch <= 0 || uint32_t(p_pitch) < p_row_bytes) {
		return false;
	}
	uint8_t *destination = static_cast<uint8_t *>(p_destination);
	for (uint32_t row = 0; row < p_rows; row++) {
		memcpy(destination + size_t(row) * p_pitch, p_source + size_t(row) * p_row_bytes, p_row_bytes);
	}
	return true;
}

FixedFunctionGraphicsDevice::FFMipFilter FixedFunctionGraphicsDevice::get_mip_filter(uint8_t p_flags, uint8_t p_levels) const {
	if (!(p_flags & SAMPLER_MIPMAPS) || p_levels <= 1) {
		return MIP_FILTER_NONE;
	}
	const uint8_t supported = get_capabilities().sampler_mip_filters;
	if ((p_flags & SAMPLER_FILTER_LINEAR) && (supported & MIP_FILTER_LINEAR)) {
		return MIP_FILTER_LINEAR;
	}
	if (supported & MIP_FILTER_POINT) {
		return MIP_FILTER_POINT;
	}
	return (supported & MIP_FILTER_LINEAR) ? MIP_FILTER_LINEAR : MIP_FILTER_NONE;
}

uint8_t FixedFunctionGraphicsDevice::get_sampler_anisotropy(uint8_t p_flags) const {
	const uint8_t limit = get_capabilities().sampler_max_anisotropy;
	if (!(p_flags & SAMPLER_FILTER_ANISOTROPIC) || limit <= 1) {
		return 1;
	}
	const int level = GLOBAL_GET("rendering/textures/default_filters/anisotropic_filtering_level");
	return uint8_t(MIN(1 << CLAMP(level, 0, 4), int(limit)));
}

Error FixedFunctionGraphicsDevice::draw_masked_triangles(const FixedFunctionMaskedVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_model, RID p_color_texture, RID p_mask_texture, uint8_t p_mask_sampler_flags, const FFDrawParameters &p_state) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::draw_normal_mapped_triangles(const FixedFunctionTransientNormalMapVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters &p_normal, const FFOcclusionMapParameters *p_occlusion, const uint16_t *p_indices, uint32_t p_index_count) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::draw_occlusion_mapped_triangles(const FixedFunctionTransientOcclusionVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFOcclusionMapParameters &p_occlusion, const uint16_t *p_indices, uint32_t p_index_count) {
	return ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDevice::texture_read_render_target(RID p_texture, uint32_t *r_pixels, size_t p_pixel_count) {
	return ERR_UNAVAILABLE;
}
