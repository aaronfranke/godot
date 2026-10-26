#include "core/math/math_funcs.h"
#include "drivers/fixed_function/texture_storage.h"

void FixedFunctionTextureStorage::_invalidate_msdf(const FFTexture *p_texture) {
	const FFMSDFCoverage *coverage = _msdf_textures.getptr(p_texture);
	if (coverage) {
		for (const RID &mask : coverage->textures) {
			if (mask.is_valid()) {
				_retired_device_textures.push_back(mask);
			}
		}
		_msdf_textures.erase(p_texture);
	}
}

RID FixedFunctionTextureStorage::texture_get_msdf_coverage(RID p_texture, uint8_t p_outline_width) {
	const FFTexture *texture = _get_texture(p_texture);
	if (!texture || texture->free_pending || _clearing) {
		return RID();
	}
	if (texture->proxy) {
		texture = texture->proxy;
	}
	if (texture->render_target || texture->free_pending || texture->upload_pending || texture->image.is_null()) {
		return RID();
	}
	const FFMSDFCoverage *cached = _msdf_textures.getptr(texture);
	int slot = 0;
	if (p_outline_width) {
		slot = 1;
		if (cached) {
			RID nearest;
			int nearest_distance = 256;
			for (int i = 0; i < FFMSDFCoverage::OUTLINE_LIMIT; i++, slot++) {
				const uint8_t width = cached->outline_widths[i];
				if (!width) {
					break;
				}
				const RID mask = cached->textures[slot];
				if (width == p_outline_width) {
					return mask;
				}
				const int distance = Math::abs(int(width) - p_outline_width);
				if (mask.is_valid() && distance < nearest_distance) {
					nearest = mask;
					nearest_distance = distance;
				}
			}
			if (slot > FFMSDFCoverage::OUTLINE_LIMIT) {
				// Keep the admitted widths to avoid rebuilding atlases every frame.
				return nearest;
			}
		}
	} else if (cached && cached->fill_cached) {
		return cached->textures[0];
	}
	if (_updating) {
		return RID();
	}

	FFUpdateScope update_scope(_updating);
	const Ref<Image> snapshot = texture->image;
	Ref<Image> base = snapshot;
	if (base->has_mipmaps()) {
		base = base->duplicate();
		base->clear_mipmaps();
	}
	Ref<Image> rgba;
	Error error = FixedFunctionGraphicsDevice::prepare_rgba8_upload(base, rgba);
	RID coverage;
	if (error == OK) {
		const Vector<uint8_t> pixels = rgba->get_data();
		Vector<uint8_t> mask;
		error = mask.resize(pixels.size());
		if (error == OK) {
			// Bake a fixed-width coverage ramp from the median RGB distance.
			// White RGB lets every device apply the usual glyph tint and opacity.
			uint8_t ramp[256];
			const float offset = p_outline_width / 255.0f;
			for (int i = 0; i < 256; i++) {
				ramp[i] = uint8_t(Math::smoothstep(0.375f, 0.625f, i / 255.0f + offset) * 255 + 0.5f);
			}
			{
				const uint8_t *input = pixels.ptr();
				uint8_t *output = mask.ptrw();
				for (int i = 0; i < mask.size(); i += 4) {
					output[i] = output[i + 1] = output[i + 2] = 255;
					const uint8_t r = input[i], g = input[i + 1], b = input[i + 2];
					uint8_t distance = MAX(MIN(r, g), MIN(MAX(r, g), b));
					if (p_outline_width) {
						// The atlas alpha carries true signed distance for outline coverage.
						distance = MIN(distance, input[i + 3]);
					}
					output[i + 3] = ramp[distance];
				}
			}
			const Ref<Image> atlas = Image::create_from_data(base->get_width(), base->get_height(), false, Image::FORMAT_RGBA8, mask);
			error = _device.texture_create(atlas, coverage, true);
		}
	}
	if (texture->free_pending || texture->image != snapshot) {
		if (coverage.is_valid()) {
			_retired_device_textures.push_back(coverage);
		}
		return RID();
	}
	if (error != ERR_SKIP && error != ERR_BUSY) {
		// Cache a failed conversion for this revision as well, so every glyph
		// sharing a refused atlas can be omitted without repeating the work.
		FFMSDFCoverage &masks = _msdf_textures[texture];
		masks.textures[slot] = coverage;
		if (slot == 0) {
			masks.fill_cached = true;
		} else {
			masks.outline_widths[slot - 1] = p_outline_width;
		}
	}
	return coverage;
}
