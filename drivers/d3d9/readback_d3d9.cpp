#include "graphics_device_d3d9.h"

Error FixedFunctionGraphicsDeviceD3D9::_read_surface(IDirect3DSurface9 *p_surface, uint32_t p_width, uint32_t p_height, uint32_t *r_pixels, bool p_preserve_alpha) {
	IDirect3DSurface9 *copy = nullptr;
	D3DSURFACE_DESC description = {};
	HRESULT result = p_surface->GetDesc(&description);
	if (SUCCEEDED(result) && description.Format != D3DFMT_X8R8G8B8 && description.Format != D3DFMT_A8R8G8B8) {
		result = D3DERR_NOTAVAILABLE;
	}
	if (SUCCEEDED(result)) {
		result = _device->CreateOffscreenPlainSurface(description.Width, description.Height, description.Format, D3DPOOL_SYSTEMMEM, &copy, nullptr);
	}
	if (SUCCEEDED(result)) {
		result = _device->GetRenderTargetData(p_surface, copy);
	}
	D3DLOCKED_RECT locked = {};
	Error copy_error = OK;
	if (SUCCEEDED(result)) {
		result = copy->LockRect(&locked, nullptr, D3DLOCK_READONLY);
		if (SUCCEEDED(result)) {
			copy_error = locked.Pitch <= 0 ? ERR_UNAVAILABLE : copy_texture_readback(static_cast<const uint8_t *>(locked.pBits), Size2i(description.Width, description.Height), locked.Pitch, Size2i(p_width, p_height), r_pixels, size_t(p_width) * p_height, p_preserve_alpha);
			result = copy->UnlockRect();
		}
	}
	if (copy) {
		copy->Release();
	}
	const Error error = _check_result(result);
	return error == OK ? copy_error : error;
}

Error FixedFunctionGraphicsDeviceD3D9::texture_read_render_target(RID p_texture, uint32_t *r_pixels, size_t p_pixel_count) {
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
	const D3D9Texture *texture = _texture_owner.getptr(p_texture);
	if (!texture || !texture->render_target) {
		return ERR_INVALID_PARAMETER;
	}
	if (!texture->native || !texture->rendered) {
		return ERR_UNCONFIGURED;
	}
	if (!r_pixels || p_pixel_count < uint64_t(texture->width) * texture->height) {
		return ERR_INVALID_PARAMETER;
	}
	IDirect3DSurface9 *surface = nullptr;
	const HRESULT result = texture->native->GetSurfaceLevel(0, &surface);
	if (FAILED(result)) {
		return _check_result(result);
	}
	const Error read_error = _read_surface(surface, texture->width, texture->height, r_pixels, texture->transparent);
	surface->Release();
	return read_error;
}
