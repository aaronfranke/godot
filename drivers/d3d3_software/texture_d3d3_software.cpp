#include "core/math/math_funcs_binary.h"
#include "drivers/d3d3_software/graphics_device_d3d3_software.h"

#include <string.h>

FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareTextureView::D3D3SoftwareTextureView() :
		surface(nullptr),
		texture(nullptr),
		device_id(0),
		handle(0) {}

FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareTextureView::~D3D3SoftwareTextureView() {
	release();
}

void FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareTextureView::release() {
	if (texture) {
		texture->Release();
		texture = nullptr;
	}
	if (surface) {
		surface->Release();
		surface = nullptr;
	}
	handle = 0;
	device_id = 0;
}

FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareTexture::D3D3SoftwareTexture() :
		target(nullptr),
		keyed_sprite(nullptr),
		uv_scale(1, 1),
		bytes(0),
		sprite_blit_eligible(false) {}

FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareTexture::~D3D3SoftwareTexture() {
	if (keyed_sprite) {
		keyed_sprite->Release();
	}
	if (target) {
		// Release the texture interface before the device and its color surface.
		opaque.release();
		memdelete(target);
	}
}

uint64_t FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareTexture::get_bytes() const {
	const uint64_t rgba_bytes = uint64_t(upload_size.x) * upload_size.y * 4;
	return bytes + (binary_glyph.surface ? rgba_bytes : 0) + (keyed_sprite ? rgba_bytes : 0);
}

HRESULT CALLBACK FixedFunctionGraphicsDeviceD3D3Software::_select_texture_format(DDSURFACEDESC *p_surface, void *p_context) {
	const DDPIXELFORMAT &candidate = p_surface->ddpfPixelFormat;
	FixedFunctionGraphicsDeviceD3D3Software *device = static_cast<FixedFunctionGraphicsDeviceD3D3Software *>(p_context);
	const bool rgb888 = candidate.dwRGBBitCount == 32 && candidate.dwRBitMask == 0xff0000 && candidate.dwGBitMask == 0xff00 && candidate.dwBBitMask == 0xff;
	if (candidate.dwFlags == (DDPF_RGB | DDPF_ALPHAPIXELS) && rgb888 && candidate.dwRGBAlphaBitMask == 0xff000000) {
		device->_alpha_texture_format = candidate;
	}
	if (candidate.dwFlags != DDPF_RGB) {
		return D3DENUMRET_OK;
	}
	DDPIXELFORMAT &chosen = device->_texture_format;
	if (rgb888) {
		chosen = candidate;
		return D3DENUMRET_OK;
	}
	if (chosen.dwRGBBitCount != 32 && candidate.dwRGBBitCount == 16 && candidate.dwBBitMask == 0x1f &&
			((candidate.dwRBitMask == 0xf800 && candidate.dwGBitMask == 0x7e0) || (candidate.dwRBitMask == 0x7c00 && candidate.dwGBitMask == 0x3e0 && !chosen.dwSize))) {
		chosen = candidate;
	}
	return D3DENUMRET_OK;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_create_texture_view(D3D3SoftwareTextureView &r_view, const Size2i &p_size, const DDPIXELFORMAT &p_format) {
	DDSURFACEDESC description = {};
	description.dwSize = sizeof(description);
	description.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	description.dwWidth = p_size.x;
	description.dwHeight = p_size.y;
	description.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;
	description.ddpfPixelFormat = p_format;
	HRESULT result = _ddraw->CreateSurface(&description, &r_view.surface, nullptr);
	if (SUCCEEDED(result)) {
		result = r_view.surface->QueryInterface(IID_IDirect3DTexture, reinterpret_cast<void **>(&r_view.texture));
	}
	return _check_result(result);
}

Error FixedFunctionGraphicsDeviceD3D3Software::_upload_texture_view(D3D3SoftwareTextureView &r_view, const Size2i &p_size, const DDPIXELFORMAT &p_format, const uint8_t *p_rgba, bool p_use_alpha) {
	const Error created = _create_texture_view(r_view, p_size, p_format);
	if (created != OK) {
		return created;
	}
	DDSURFACEDESC description = {};
	description.dwSize = sizeof(description);
	const Error locked = _check_result(r_view.surface->Lock(nullptr, &description, DDLOCK_WAIT, nullptr));
	if (locked != OK) {
		return locked;
	}
	const bool rgb565 = p_format.dwGBitMask == 0x7e0;
	for (int y = 0; y < p_size.y; y++) {
		const uint8_t *src = p_rgba + size_t(y) * p_size.x * 4;
		uint8_t *row = static_cast<uint8_t *>(description.lpSurface) + ptrdiff_t(y) * description.lPitch;
		if (p_format.dwRGBBitCount == 32) {
			uint32_t *dest = reinterpret_cast<uint32_t *>(row);
			for (int x = 0; x < p_size.x; x++, src += 4) {
				const uint32_t alpha = p_use_alpha ? uint32_t(src[3]) << 24 : 0xff000000;
				dest[x] = (alpha & p_format.dwRGBAlphaBitMask) | (uint32_t(src[0]) << 16) | (uint32_t(src[1]) << 8) | src[2];
			}
		} else {
			uint16_t *dest = reinterpret_cast<uint16_t *>(row);
			for (int x = 0; x < p_size.x; x++, src += 4) {
				const uint16_t red = src[0] >> 3;
				const uint16_t green = src[1] >> (rgb565 ? 2 : 3);
				dest[x] = (red << (rgb565 ? 11 : 10)) | (green << 5) | (src[2] >> 3);
			}
		}
	}
	return _check_result(r_view.surface->Unlock(nullptr));
}

uint32_t FixedFunctionGraphicsDeviceD3D3Software::_probe_texture_extent(bool p_horizontal, const DDPIXELFORMAT &p_format) {
	// Probe narrow surfaces rather than allocating a full 4096 × 4096 atlas at startup.
	for (int extent = 4096; extent > 256; extent /= 2) {
		D3D3SoftwareTextureView view;
		const Size2i size = p_horizontal ? Size2i(extent, 8) : Size2i(8, extent);
		if (_create_texture_view(view, size, p_format) == OK && SUCCEEDED(view.texture->GetHandle(_window_target.device, &view.handle))) {
			return extent;
		}
	}
	return 256;
}

Error FixedFunctionGraphicsDeviceD3D3Software::texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame) {
	Error error = _check_thread();
	if (error != OK || (_frame_active && !p_allow_during_frame)) {
		return error == OK ? ERR_BUSY : error;
	}
	Ref<Image> upload;
	error = prepare_texture_upload(p_image, upload);
	if (error != OK) {
		return error;
	}
	const Size2i source_size(p_image->get_width(), p_image->get_height());
	const Size2i upload_size(upload->get_width(), upload->get_height());
	const Image::AlphaMode alpha = _capabilities.blend_mix ? upload->detect_alpha() : Image::ALPHA_NONE;
	const Vector<uint8_t> bytes = upload->get_data();
	const uint8_t *source = bytes.ptr();
	D3D3SoftwareTexture *texture = memnew(D3D3SoftwareTexture);
	texture->size = source_size;
	texture->upload_size = upload_size;
	texture->sprite_blit_eligible = alpha == Image::ALPHA_BIT && source_size == upload_size && uint64_t(upload_size.x) * upload_size.y <= MAX_KEYED_SPRITE_PIXELS;
	const DDPIXELFORMAT &opaque_format = _capabilities.blend_mix ? _alpha_texture_format : _texture_format;
	texture->bytes = upload_size.x * upload_size.y * (opaque_format.dwRGBBitCount / 8);
	error = _upload_texture_view(texture->opaque, upload_size, opaque_format, source, false);
	if (error == OK && alpha != Image::ALPHA_NONE) {
		error = _upload_texture_view(texture->alpha, upload_size, _alpha_texture_format, source);
		texture->bytes += upload_size.x * upload_size.y * (_alpha_texture_format.dwRGBBitCount / 8);
	}
	if (error != OK) {
		memdelete(texture);
		return error;
	}
	texture->rid = _texture_owner.make_rid(texture);
	_textures.push_back(texture);
	r_texture = texture->rid;
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D3Software::texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent) {
	Error error = _check_thread();
	if (error != OK || _frame_active) {
		return error == OK ? ERR_BUSY : error;
	}
	if (!p_width || !p_height) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_transparent || !_capabilities.render_target_opaque || p_width > INT32_MAX || p_height > INT32_MAX) {
		return ERR_UNAVAILABLE;
	}
	Size2i limit(MIN(uint32_t(MAX_RENDER_TARGET_LIMIT), _capabilities.max_texture_width), MIN(uint32_t(MAX_RENDER_TARGET_LIMIT), _capabilities.max_texture_height));
	if (_capabilities.texture_square_only) {
		limit.x = limit.y = MIN(limit.x, limit.y);
	}
	const Size2i size(p_width, p_height);
	const Size2i native_size = _fit_render_size(size, limit);
	Size2i backing_size(Math::next_power_of_2(uint32_t(native_size.x)), Math::next_power_of_2(uint32_t(native_size.y)));
	if (_capabilities.texture_square_only) {
		backing_size.x = backing_size.y = MAX(backing_size.x, backing_size.y);
	}
	if (FixedFunctionGraphicsDevice::validate_texture_size(backing_size.x, backing_size.y) != OK) {
		return ERR_UNAVAILABLE;
	}
	D3D3SoftwareTexture *texture = memnew(D3D3SoftwareTexture);
	texture->size = size;
	texture->uv_scale = Vector2(native_size) / Vector2(backing_size);
	texture->target = memnew(D3D3SoftwareRenderTarget);
	texture->target->size = size;
	texture->target->native_size = native_size;
	DDSURFACEDESC description = {};
	description.dwSize = sizeof(description);
	description.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	description.dwWidth = backing_size.x;
	description.dwHeight = backing_size.y;
	description.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_3DDEVICE | DDSCAPS_SYSTEMMEMORY;
	description.ddpfPixelFormat = _texture_format;
	error = _check_result(_ddraw->CreateSurface(&description, &texture->target->color, nullptr));
	if (error == OK) {
		error = _create_pass(*texture->target, backing_size);
	}
	if (error == OK) {
		error = _check_result(texture->target->color->QueryInterface(IID_IDirect3DTexture, reinterpret_cast<void **>(&texture->opaque.texture)));
	}
	if (error != OK) {
		memdelete(texture);
		return error == FAILED ? ERR_UNAVAILABLE : error;
	}
	texture->bytes = backing_size.x * backing_size.y * (_texture_format.dwRGBBitCount / 8 + 2);
	texture->rid = _texture_owner.make_rid(texture);
	_textures.push_back(texture);
	r_texture = texture->rid;
	return OK;
}

bool FixedFunctionGraphicsDeviceD3D3Software::texture_is_render_target_ready(RID p_texture) const {
	if (!_texture_owner.owns(p_texture)) {
		return false;
	}
	const D3D3SoftwareRenderTarget *target = _texture_owner.get_or_null(p_texture)->target;
	return target && target->readable && !target->is_lost();
}

Error FixedFunctionGraphicsDeviceD3D3Software::texture_get_info(RID p_texture, FFTextureInfo &r_info) const {
	if (!_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D3SoftwareTexture *texture = _texture_owner.get_or_null(p_texture);
	r_info.size = texture->size;
	r_info.bytes = texture->get_bytes();
	return OK;
}

uint64_t FixedFunctionGraphicsDeviceD3D3Software::get_texture_memory_usage() const {
	uint64_t bytes = 0;
	for (const List<D3D3SoftwareTexture *>::Element *entry = _textures.front(); entry; entry = entry->next()) {
		bytes += entry->get()->get_bytes();
	}
	if (_glyph_cache) {
		for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
			const D3D3SoftwareGlyph &glyph = _glyph_cache[i];
			if (glyph.surface) {
				bytes += uint64_t(glyph.source.size.x) * glyph.source.size.y * 4;
			}
		}
	}
	return bytes;
}

Error FixedFunctionGraphicsDeviceD3D3Software::texture_free(RID p_texture) {
	const Error error = _check_thread();
	if (error != OK || _frame_active) {
		return error == OK ? ERR_BUSY : error;
	}
	if (!_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	if (_glyph_cache) {
		for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
			if (_glyph_cache[i].texture_id == p_texture.get_id()) {
				_glyph_cache[i].release();
			}
		}
	}
	D3D3SoftwareTexture *texture = _texture_owner.get_or_null(p_texture);
	_textures.erase(texture);
	_texture_owner.free(p_texture);
	memdelete(texture);
	return OK;
}

uint32_t FixedFunctionGraphicsDeviceD3D3Software::_unpack_channel(uint32_t p_pixel, uint32_t p_mask) {
	return uint32_t((uint64_t(p_pixel & p_mask) * 255 + p_mask / 2) / p_mask);
}

Error FixedFunctionGraphicsDeviceD3D3Software::read_frame(uint32_t *p_pixels, size_t p_pixel_count) {
	const Error error = _check_thread();
	if (error != OK || _frame_active || !_window_target.readable) {
		return error == OK ? ERR_UNCONFIGURED : error;
	}
	const Size2i size = _window_target.size;
	if (!p_pixels || p_pixel_count < uint64_t(size.x) * size.y) {
		return ERR_INVALID_PARAMETER;
	}
	return _read_surface(_window_target, p_pixels);
}
