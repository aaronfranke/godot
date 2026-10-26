#include "texture_storage.h"

#include "core/math/math_funcs.h"

void FixedFunctionTextureStorage::_invalidate_distance_field(const FFTexture *p_texture) {
	Map<const FFTexture *, RID>::Element *coverage = _distance_field_textures.find(p_texture);
	if (coverage) {
		if (coverage->get().is_valid()) {
			_retired_device_textures.push_back(coverage->get());
		}
		_distance_field_textures.erase(coverage);
	}
}

RID FixedFunctionTextureStorage::texture_get_distance_field(RID p_texture) {
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
	const Map<const FFTexture *, RID>::Element *cached = _distance_field_textures.find(texture);
	if (cached) {
		return cached->get();
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
		const PoolVector<uint8_t> pixels = rgba->get_data();
		PoolVector<uint8_t> mask;
		error = mask.resize(pixels.size());
		if (error == OK) {
			// Bake the GLES distance ramp into the atlas once per accepted image.
			// White RGB lets every device apply the usual glyph tint and opacity.
			uint8_t ramp[256];
			for (int i = 0; i < 256; i++) {
				ramp[i] = uint8_t(Math::smoothstep(0.375f, 0.625f, i / 255.0f) * 255 + 0.5f);
			}
			{
				const PoolVector<uint8_t>::Read input = pixels.read();
				PoolVector<uint8_t>::Write output = mask.write();
				for (int i = 0; i < mask.size(); i += 4) {
					output[i] = output[i + 1] = output[i + 2] = 255;
					output[i + 3] = ramp[input[i + 3]];
				}
			}
			Ref<Image> atlas;
			atlas.instance();
			atlas->create(base->get_width(), base->get_height(), false, Image::FORMAT_RGBA8, mask);
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
		_distance_field_textures.insert(texture, coverage);
	}
	return coverage;
}
