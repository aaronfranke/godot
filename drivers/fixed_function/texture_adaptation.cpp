#include "graphics_device.h"

#include <string.h>

Error FixedFunctionGraphicsDevice::get_texture_upload_size(const FFCapabilities &p_caps, uint32_t p_width, uint32_t p_height, Size2i &r_size) {
	if (!p_width || !p_height || p_width > INT32_MAX || p_height > INT32_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	uint32_t maximum_width = MIN(p_caps.max_texture_width, uint32_t(INT32_MAX));
	uint32_t maximum_height = MIN(p_caps.max_texture_height, uint32_t(INT32_MAX));
	if (!p_caps.max_texture_stages || !maximum_width || !maximum_height) {
		return ERR_UNAVAILABLE;
	}
	if (p_caps.texture_power_of_two) {
		// Caps need not themselves be powers of two. Round the limits down before
		// rounding an upload up, so an otherwise valid conversion cannot exceed them.
		uint32_t width = 1, height = 1;
		while (width <= maximum_width / 2) {
			width *= 2;
		}
		while (height <= maximum_height / 2) {
			height *= 2;
		}
		maximum_width = width;
		maximum_height = height;
	}
	uint32_t width = p_width, height = p_height;
	if (p_caps.texture_square_only) {
		width = height = MIN(MAX(width, height), MIN(maximum_width, maximum_height));
	} else {
		// Fit oversized images proportionally before any independent POT rounding.
		if (width > maximum_width) {
			height = MAX(uint32_t(uint64_t(height) * maximum_width / width), 1u);
			width = maximum_width;
		}
		if (height > maximum_height) {
			width = MAX(uint32_t(uint64_t(width) * maximum_height / height), 1u);
			height = maximum_height;
		}
	}
	if (p_caps.texture_power_of_two) {
		width = next_power_of_2(width);
		height = next_power_of_2(height);
	}
	r_size = Size2i(width, height);
	return OK;
}

Error FixedFunctionGraphicsDevice::get_texture_upload_size(uint32_t p_width, uint32_t p_height, Size2i &r_size) const {
	return get_texture_upload_size(get_capabilities(), p_width, p_height, r_size);
}

Rect2i FixedFunctionGraphicsDevice::get_texture_native_clip(const Rect2i &p_clip, const Size2i &p_logical, const Size2i &p_native) {
	// Quantize shared logical edges consistently, so adjacent clips never overlap.
	const int left = (uint64_t(p_clip.position.x) * p_native.x + p_logical.x / 2) / p_logical.x;
	const int top = (uint64_t(p_clip.position.y) * p_native.y + p_logical.y / 2) / p_logical.y;
	const int right = (uint64_t(p_clip.position.x + p_clip.size.x) * p_native.x + p_logical.x / 2) / p_logical.x;
	const int bottom = (uint64_t(p_clip.position.y + p_clip.size.y) * p_native.y + p_logical.y / 2) / p_logical.y;
	return Rect2i(left, top, right - left, bottom - top);
}

Error FixedFunctionGraphicsDevice::copy_texture_readback(const uint8_t *p_source, const Size2i &p_native_size, size_t p_pitch, const Size2i &p_logical_size, uint32_t *r_pixels, size_t p_pixel_count, bool p_preserve_alpha) {
	if (!p_source || !r_pixels || p_native_size.x <= 0 || p_native_size.y <= 0 || p_logical_size.x <= 0 || p_logical_size.y <= 0 || uint64_t(p_native_size.x) * 4 > p_pitch || uint64_t(p_logical_size.x) * p_logical_size.y > p_pixel_count) {
		return ERR_INVALID_PARAMETER;
	}
	const size_t row_bytes = size_t(p_native_size.x) * 4;
	if (p_native_size.y > 1 && p_pitch > (SIZE_MAX - row_bytes) / size_t(p_native_size.y - 1)) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_native_size == p_logical_size) {
		// Preserve the ordinary readback cost: resampling is only for adapted targets.
		for (int y = 0; y < p_logical_size.y; y++) {
			const uint8_t *row = p_source + size_t(y) * p_pitch;
			if (p_preserve_alpha) {
				memcpy(r_pixels, row, row_bytes);
				r_pixels += p_logical_size.x;
			} else {
				for (int x = 0; x < p_logical_size.x; x++) {
					uint32_t pixel;
					memcpy(&pixel, row + size_t(x) * 4, 4);
					*r_pixels++ = pixel | 0xff000000u;
				}
			}
		}
		return OK;
	}
	for (int y = 0; y < p_logical_size.y; y++) {
		const uint32_t source_y = uint32_t((uint64_t(y) * 2 + 1) * p_native_size.y / (uint64_t(p_logical_size.y) * 2));
		const uint8_t *row = p_source + size_t(source_y) * p_pitch;
		for (int x = 0; x < p_logical_size.x; x++) {
			const uint32_t source_x = uint32_t((uint64_t(x) * 2 + 1) * p_native_size.x / (uint64_t(p_logical_size.x) * 2));
			uint32_t pixel;
			memcpy(&pixel, row + size_t(source_x) * 4, 4);
			*r_pixels++ = p_preserve_alpha ? pixel : pixel | 0xff000000u;
		}
	}
	return OK;
}

Error FixedFunctionGraphicsDevice::prepare_texture_upload(const Ref<Image> &p_image, Ref<Image> &r_upload) const {
	Error error = validate_texture_image(p_image);
	if (error != OK) {
		return error;
	}
	Size2i size;
	error = get_texture_upload_size(p_image->get_width(), p_image->get_height(), size);
	if (error != OK) {
		return error;
	}
	const Size2i source_size(p_image->get_width(), p_image->get_height());
	if (size == source_size && (p_image->get_format() == Image::FORMAT_RGBA8 || can_upload_compressed_texture(p_image))) {
		r_upload = p_image;
		return OK;
	}
	// Retain authored mip data, including BC blocks, when a legal existing level
	// exactly matches the upload. No decode/recompression or new mip filtering is needed.
	int width = source_size.x, height = source_size.y;
	for (int level = 1; level <= p_image->get_mipmap_count(); level++) {
		width = MAX(width / 2, 1);
		height = MAX(height / 2, 1);
		if (size != Size2i(width, height) || (p_image->is_compressed() && (width % 4 || height % 4))) {
			continue;
		}
		const PoolVector<uint8_t> data = p_image->get_data();
		const int offset = p_image->get_mipmap_offset(level);
		PoolVector<uint8_t> tail;
		error = tail.resize(data.size() - offset);
		if (error != OK) {
			return error;
		}
		{
			PoolVector<uint8_t>::Read read = data.read();
			PoolVector<uint8_t>::Write write = tail.write();
			memcpy(write.ptr(), read.ptr() + offset, tail.size());
		}
		Ref<Image> upload;
		upload.instance();
		upload->create(width, height, level < p_image->get_mipmap_count(), p_image->get_format(), tail);
		r_upload = upload;
		return OK;
	}
	Ref<Image> rgba;
	error = prepare_rgba8_upload(p_image, rgba);
	if (error != OK) {
		return error;
	}
	if (uint64_t(size.x) * size.y > INT32_MAX / 4) {
		return ERR_OUT_OF_MEMORY;
	}
	Ref<Image> upload = rgba->duplicate();
	upload->clear_mipmaps();
	// Resample the entire normalized UV domain. Padding alone would break repeat
	// and logical atlas regions, while changing the source would break CPU ownership.
	upload->resize(size.x, size.y, Image::INTERPOLATE_BILINEAR);
	if (upload->get_width() != size.x || upload->get_height() != size.y) {
		return ERR_OUT_OF_MEMORY;
	}
	if (p_image->has_mipmaps() && get_capabilities().sampler_mip_filters != MIP_FILTER_NONE) {
		error = upload->generate_mipmaps();
		if (error != OK) {
			return error;
		}
	}
	r_upload = upload;
	return OK;
}
