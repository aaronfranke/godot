#include "graphics_device_d3d7.h"

#include <string.h>

Error FixedFunctionGraphicsDeviceD3D7::_read_surface(IDirectDrawSurface7 *p_surface, const Size2i &p_size, const Size2i &p_native_size, uint32_t *r_pixels) {
	DDSURFACEDESC2 description = {};
	description.dwSize = sizeof(description);
	const Error error = _check_result(p_surface->Lock(nullptr, &description, DDLOCK_READONLY | DDLOCK_WAIT, nullptr));
	if (error != OK) {
		return error;
	}
	const DDPIXELFORMAT &format = description.ddpfPixelFormat;
	if (!(format.dwFlags & DDPF_RGB) || (format.dwRGBBitCount != 16 && format.dwRGBBitCount != 24 && format.dwRGBBitCount != 32) || !format.dwRBitMask || !format.dwGBitMask || !format.dwBBitMask) {
		p_surface->Unlock(nullptr);
		return ERR_UNAVAILABLE;
	}
	const uint32_t pixel_size = format.dwRGBBitCount / 8;
	for (int y = 0; y < p_size.y; y++) {
		// Read the logical extent, expanding a scaled backing by nearest sampling.
		const uint32_t native_y = uint64_t(y) * p_native_size.y / p_size.y;
		const uint8_t *row = static_cast<const uint8_t *>(description.lpSurface) + ptrdiff_t(native_y) * description.lPitch;
		for (int x = 0; x < p_size.x; x++) {
			const uint32_t native_x = uint64_t(x) * p_native_size.x / p_size.x;
			uint32_t pixel = 0;
			memcpy(&pixel, row + size_t(native_x) * pixel_size, pixel_size);
			r_pixels[size_t(y) * p_size.x + x] = 0xff000000u | (_unpack_channel(pixel, format.dwRBitMask) << 16) | (_unpack_channel(pixel, format.dwGBitMask) << 8) | _unpack_channel(pixel, format.dwBBitMask);
		}
	}
	return _check_result(p_surface->Unlock(nullptr));
}

Error FixedFunctionGraphicsDeviceD3D7::texture_read_render_target(RID p_texture, uint32_t *r_pixels, size_t p_pixel_count) {
	const Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active || _resetting || _reset_pending) {
		return ERR_BUSY;
	}
	if (!_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D7Texture *texture = _texture_owner.getptr(p_texture);
	if (!texture || !texture->render_target) {
		return ERR_INVALID_PARAMETER;
	}
	if (!texture->native || !texture->rendered) {
		return ERR_UNCONFIGURED;
	}
	if (!r_pixels || p_pixel_count < uint64_t(texture->width) * texture->height) {
		return ERR_INVALID_PARAMETER;
	}
	return _read_surface(texture->native, Size2i(texture->width, texture->height), texture->native_size, r_pixels);
}
