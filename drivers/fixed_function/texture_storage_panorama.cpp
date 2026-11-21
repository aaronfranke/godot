#include "core/math/math_funcs.h"
#include "drivers/fixed_function/texture_storage.h"

#include <string.h>

FixedFunctionTextureStorage::FFTexture *FixedFunctionTextureStorage::_get_panorama_averages(RID p_texture) {
	FFTexture *texture = _get_texture(p_texture);
	if (!texture || texture->free_pending) {
		return nullptr;
	}
	if (texture->proxy) {
		texture = texture->proxy;
	}
	if (texture->free_pending || texture->image.is_null()) {
		return nullptr;
	}
	if (texture->panorama_average_rgba) {
		return texture;
	}

	Size2i size = texture->image_size;
	const Image::Format format = texture->image->get_format();
	const bool rgbe = format == Image::FORMAT_RGBE9995;
	int pixel_size = 4;
	Vector<uint8_t> bytes;
	if (format == Image::FORMAT_RGB8 || format == Image::FORMAT_RGBA8 || rgbe) {
		// Sample common panorama formats directly from the immutable CPU image.
		// A procedural sky does not need a second full-size LDR copy.
		pixel_size = format == Image::FORMAT_RGB8 ? 3 : 4;
		bytes = texture->image->get_data();
	} else if (!texture_get_cpu_rgba8(p_texture, size, bytes)) {
		return nullptr;
	}
	if (size.x <= 0 || size.y <= 0 || uint64_t(size.x) * size.y * pixel_size > uint64_t(bytes.size())) {
		return nullptr;
	}

	enum {
		MAX_COLUMNS = 256,
		MAX_ROWS = 128
	};
	const int columns = MIN(size.x, MAX_COLUMNS);
	const int rows = MIN(size.y, MAX_ROWS);
	const uint8_t *data = bytes.ptr();
	double sum[2][3] = {};
	double weight_sum[2] = {};
	double band_top = 1;
	for (int row = 0; row < rows; row++) {
		// Differences in cosine of the polar angle give each band's solid angle. Split an odd
		// equatorial row between both hemispheres, including a one-row panorama.
		const double band_bottom = Math::cos(Math::PI * (row + 1) / rows);
		const double weights[] = { MAX(band_top, 0.0) - MAX(band_bottom, 0.0), MIN(band_top, 0.0) - MIN(band_bottom, 0.0) };
		band_top = band_bottom;
		double row_sum[3] = {};
		const int y = (2 * row + 1) * size.y / (2 * rows);
		for (int column = 0; column < columns; column++) {
			const int x = (2 * column + 1) * size.x / (2 * columns);
			const uint8_t *sample = data + (size_t(y) * size.x + x) * pixel_size;
			if (rgbe) {
				uint32_t packed;
				memcpy(&packed, sample, sizeof(packed));
				const Color color = Color::from_rgbe9995(packed).linear_to_srgb();
				for (int channel = 0; channel < 3; channel++) {
					row_sum[channel] += CLAMP(color[channel], 0.0f, 1.0f);
				}
			} else {
				for (int channel = 0; channel < 3; channel++) {
					row_sum[channel] += sample[channel] / 255.0;
				}
			}
		}
		for (int hemisphere = 0; hemisphere < 2; hemisphere++) {
			for (int channel = 0; channel < 3; channel++) {
				sum[hemisphere][channel] += weights[hemisphere] * row_sum[channel];
			}
			weight_sum[hemisphere] += weights[hemisphere] * columns;
		}
	}
	const Color sky(float(sum[0][0] / weight_sum[0]), float(sum[0][1] / weight_sum[0]), float(sum[0][2] / weight_sum[0]));
	const Color ground(float(sum[1][0] / weight_sum[1]), float(sum[1][1] / weight_sum[1]), float(sum[1][2] / weight_sum[1]));
	const double total_weight = weight_sum[0] + weight_sum[1];
	const Color average(float((sum[0][0] + sum[1][0]) / total_weight), float((sum[0][1] + sum[1][1]) / total_weight), float((sum[0][2] + sum[1][2]) / total_weight));
	texture->panorama_sky_rgba = sky.to_rgba32();
	texture->panorama_ground_rgba = ground.to_rgba32();
	// Opaque alpha validates all three cached colors, including a black panorama.
	texture->panorama_average_rgba = average.to_rgba32();
	return texture;
}

bool FixedFunctionTextureStorage::texture_get_panorama_average(RID p_texture, Color &r_color) {
	const FFTexture *texture = _get_panorama_averages(p_texture);
	if (!texture) {
		return false;
	}
	r_color = Color::hex(texture->panorama_average_rgba);
	return true;
}

bool FixedFunctionTextureStorage::texture_get_panorama_hemispheres(RID p_texture, Color &r_sky, Color &r_ground) {
	const FFTexture *texture = _get_panorama_averages(p_texture);
	if (!texture) {
		return false;
	}
	r_sky = Color::hex(texture->panorama_sky_rgba);
	r_ground = Color::hex(texture->panorama_ground_rgba);
	return true;
}
