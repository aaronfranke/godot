#include "drivers/d3d3_software/graphics_device_d3d3_software.h"

#include <string.h>

Error FixedFunctionGraphicsDeviceD3D3Software::_read_surface(const D3D3SoftwareRenderTarget &p_target, uint32_t *r_pixels) {
	DDSURFACEDESC description = {};
	description.dwSize = sizeof(description);
	const Error locked = _check_result(p_target.color->Lock(nullptr, &description, DDLOCK_WAIT | DDLOCK_READONLY, nullptr));
	if (locked != OK) {
		return locked;
	}
	const DDPIXELFORMAT &format = description.ddpfPixelFormat;
	if (!(format.dwFlags & DDPF_RGB) || (format.dwRGBBitCount != 16 && format.dwRGBBitCount != 24 && format.dwRGBBitCount != 32) || !format.dwRBitMask || !format.dwGBitMask || !format.dwBBitMask) {
		p_target.color->Unlock(nullptr);
		return ERR_UNAVAILABLE;
	}
	const uint32_t pixel_size = format.dwRGBBitCount / 8;
	const Size2i size = p_target.size;
	for (int y = 0; y < size.y; y++) {
		const uint8_t *row = static_cast<const uint8_t *>(description.lpSurface) + ptrdiff_t(uint64_t(y) * p_target.native_size.y / size.y) * description.lPitch;
		for (int x = 0; x < size.x; x++) {
			uint32_t pixel = 0;
			memcpy(&pixel, row + size_t(uint64_t(x) * p_target.native_size.x / size.x) * pixel_size, pixel_size);
			r_pixels[size_t(y) * size.x + x] = 0xff000000u |
					(_unpack_channel(pixel, format.dwRBitMask) << 16) |
					(_unpack_channel(pixel, format.dwGBitMask) << 8) |
					_unpack_channel(pixel, format.dwBBitMask);
		}
	}
	return _check_result(p_target.color->Unlock(nullptr));
}

Error FixedFunctionGraphicsDeviceD3D3Software::texture_read_render_target(RID p_texture, uint32_t *r_pixels, size_t p_pixel_count) {
	const Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (!_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D3SoftwareTexture *texture = _texture_owner.get_or_null(p_texture);
	if (!texture->target) {
		return ERR_INVALID_PARAMETER;
	}
	if (!texture->target->readable || texture->target->is_lost()) {
		return ERR_UNCONFIGURED;
	}
	const Size2i size = texture->target->size;
	if (!r_pixels || p_pixel_count < uint64_t(size.x) * size.y) {
		return ERR_INVALID_PARAMETER;
	}
	return _read_surface(*texture->target, r_pixels);
}
