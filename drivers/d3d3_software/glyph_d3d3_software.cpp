#include "core/templates/hashfuncs.h"
#include "drivers/d3d3_software/graphics_device_d3d3_software.h"

FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareGlyph::D3D3SoftwareGlyph() :
		texture_id(0), surface(nullptr), color(0) {}

FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareGlyph::~D3D3SoftwareGlyph() {
	release();
}

void FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareGlyph::release() {
	if (surface) {
		surface->Release();
		surface = nullptr;
	}
	texture_id = 0;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_prepare_binary_glyph_view(D3D3SoftwareTexture &r_texture) {
	D3D3SoftwareTextureView &view = r_texture.binary_glyph;
	if (view.surface && view.surface->IsLost() == DD_OK) {
		return OK;
	}
	view.release();
	const Size2i &size = r_texture.upload_size;
	Error error = _create_texture_view(view, size, _alpha_texture_format);
	if (error == OK) {
		DDSURFACEDESC input = {};
		input.dwSize = sizeof(input);
		error = _check_result(r_texture.alpha.surface->Lock(nullptr, &input, DDLOCK_WAIT | DDLOCK_READONLY, nullptr));
		if (error == OK) {
			DDSURFACEDESC output = {};
			output.dwSize = sizeof(output);
			error = _check_result(view.surface->Lock(nullptr, &output, DDLOCK_WAIT, nullptr));
			if (error == OK) {
				for (int y = 0; y < size.y; y++) {
					const uint32_t *src = reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(input.lpSurface) + ptrdiff_t(y) * input.lPitch);
					uint32_t *dst = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(output.lpSurface) + ptrdiff_t(y) * output.lPitch);
					for (int x = 0; x < size.x; x++) {
						const uint32_t alpha = (src[x] >> 24) >= GLYPH_ALPHA_THRESHOLD ? 0xff000000 : 0;
						dst[x] = alpha | (src[x] & 0xffffff);
					}
				}
				error = _check_result(view.surface->Unlock(nullptr));
			}
			const Error unlocked = _check_result(r_texture.alpha.surface->Unlock(nullptr));
			if (error == OK) {
				error = unlocked;
			}
		}
	}
	if (error != OK) {
		view.release();
	}
	return error;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_create_keyed_glyph(D3D3SoftwareGlyph &r_glyph, IDirectDrawSurface *p_atlas, const Rect2i &p_source, uint32_t p_color) {
	// Reserve a color different from this glyph's ink, including magenta ink.
	const uint32_t key = p_color == 0xff00ff ? 0x00ff00 : 0xff00ff;
	DDSURFACEDESC output = {};
	output.dwSize = sizeof(output);
	output.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	output.dwWidth = p_source.size.x;
	output.dwHeight = p_source.size.y;
	output.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	output.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	output.ddpfPixelFormat.dwFlags = DDPF_RGB;
	output.ddpfPixelFormat.dwRGBBitCount = 32;
	output.ddpfPixelFormat.dwRBitMask = 0xff0000;
	output.ddpfPixelFormat.dwGBitMask = 0x00ff00;
	output.ddpfPixelFormat.dwBBitMask = 0x0000ff;
	Error error = _check_result(_ddraw->CreateSurface(&output, &r_glyph.surface, nullptr));
	if (error != OK) {
		return error;
	}
	DDCOLORKEY color_key = { key, key };
	error = _check_result(r_glyph.surface->SetColorKey(DDCKEY_SRCBLT, &color_key));
	if (error != OK) {
		return error;
	}

	DDSURFACEDESC input = {};
	input.dwSize = sizeof(input);
	error = _check_result(p_atlas->Lock(nullptr, &input, DDLOCK_READONLY | DDLOCK_WAIT, nullptr));
	if (error != OK) {
		return error;
	}
	error = _check_result(r_glyph.surface->Lock(nullptr, &output, DDLOCK_WAIT, nullptr));
	if (error == OK) {
		bool monochrome = true;
		for (int y = 0; y < p_source.size.y && monochrome; y++) {
			const uint8_t *input_row = static_cast<const uint8_t *>(input.lpSurface) + ptrdiff_t(p_source.position.y + y) * input.lPitch;
			const uint32_t *src = reinterpret_cast<const uint32_t *>(input_row) + p_source.position.x;
			uint8_t *output_row = static_cast<uint8_t *>(output.lpSurface) + ptrdiff_t(y) * output.lPitch;
			uint32_t *dst = reinterpret_cast<uint32_t *>(output_row);
			for (int x = 0; x < p_source.size.x; x++) {
				if ((src[x] >> 24) < GLYPH_ALPHA_THRESHOLD) {
					dst[x] = key;
				} else if ((src[x] & 0xffffff) == 0xffffff) {
					dst[x] = p_color;
				} else {
					monochrome = false;
					break;
				}
			}
		}
		error = _check_result(r_glyph.surface->Unlock(nullptr));
		if (error == OK && !monochrome) {
			error = ERR_UNAVAILABLE;
		}
	}
	const Error unlocked = _check_result(p_atlas->Unlock(nullptr));
	return unlocked == OK ? error : unlocked;
}

Error FixedFunctionGraphicsDeviceD3D3Software::blit_canvas_glyph(RID p_texture, const Rect2 &p_uv, const Rect2 &p_bounds, uint32_t p_color, const FFDrawParameters &p_state) {
	if (!p_state.texture_rgb_enabled || !p_state.texture_alpha_enabled ||
			p_state.blend_mode != BLEND_MODE_MIX || (p_color >> 24) != 255) {
		return ERR_UNAVAILABLE;
	}
	const Error error = _check_canvas_blt(p_state);
	if (error != OK) {
		return error;
	}
	if (!_pass->rgb888_fill || !_texture_owner.owns(p_texture)) {
		return ERR_UNAVAILABLE;
	}
	const D3D3SoftwareTexture *texture = _texture_owner.get_or_null(p_texture);
	if (texture->upload_size != texture->size || !texture->alpha.surface) {
		return ERR_UNAVAILABLE;
	}
	const Vector2 size = texture->size;
	const Rect2 source(p_uv.position * size, p_uv.size * size);
	const Point2 end = source.position + source.size;
	const Rect2 dest = _get_native_canvas_bounds(p_bounds);
	if (source.position.x < 0 || source.position.y < 0 || end.x > size.x || end.y > size.y ||
			source.size.x <= 0 || source.size.y <= 0 || source.size.x > real_t(MAX_GLYPH_SIZE) || source.size.y > real_t(MAX_GLYPH_SIZE) ||
			source.position != source.position.floor() || source.size != source.size.floor() ||
			dest.position != dest.position.floor() || dest.size != source.size) {
		return ERR_UNAVAILABLE;
	}
	RECT dst;
	if (!_get_clipped_blt_rect(dest, _get_native_draw_viewport(p_state), dst)) {
		return ERR_UNAVAILABLE;
	}
	if (dst.left == dst.right || dst.top == dst.bottom) {
		return OK;
	}
	if (!_glyph_cache) {
		_glyph_cache = memnew_arr(D3D3SoftwareGlyph, GLYPH_CACHE_SIZE);
		if (!_glyph_cache) {
			return ERR_UNAVAILABLE;
		}
	}
	const uint32_t color = p_color & 0xffffff;
	const uint64_t texture_id = p_texture.get_id();
	const Rect2i region = source;
	uint32_t hash = hash_djb2_one_32(uint32_t(texture_id));
	hash = hash_djb2_one_32(uint32_t(texture_id >> 32), hash);
	hash = hash_djb2_one_32(region.position.x, hash);
	hash = hash_djb2_one_32(region.position.y, hash);
	hash = hash_djb2_one_32(region.size.x, hash);
	hash = hash_djb2_one_32(region.size.y, hash);
	hash = hash_djb2_one_32(color, hash);
	D3D3SoftwareGlyph &glyph = _glyph_cache[hash_djb2_one_32(hash >> 16, hash) % GLYPH_CACHE_SIZE];
	if (glyph.texture_id != texture_id || glyph.source != region || glyph.color != color ||
			(glyph.surface && glyph.surface->IsLost() != DD_OK)) {
		D3D3SoftwareGlyph replacement;
		const Error created = _create_keyed_glyph(replacement, texture->alpha.surface, region, color);
		if (created != OK && created != ERR_UNAVAILABLE) {
			return ERR_UNAVAILABLE;
		}
		if (created == ERR_UNAVAILABLE) {
			// Remember glyphs that need the ordinary path, such as colored bitmap glyphs.
			replacement.release();
		}
		SWAP(glyph.surface, replacement.surface);
		glyph.texture_id = texture_id;
		glyph.source = region;
		glyph.color = color;
	}
	if (!glyph.surface) {
		return ERR_UNAVAILABLE;
	}
	const LONG x = LONG(dest.position.x);
	const LONG y = LONG(dest.position.y);
	RECT src = { dst.left - x, dst.top - y, dst.right - x, dst.bottom - y };
	const Error blit = _check_result(_pass->color->Blt(&dst, glyph.surface, &src, DDBLT_KEYSRC | DDBLT_WAIT, nullptr));
	return blit == OK || blit == ERR_SKIP ? blit : ERR_UNAVAILABLE;
}
