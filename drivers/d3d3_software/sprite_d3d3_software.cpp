#include "graphics_device_d3d3_software.h"

Error FixedFunctionGraphicsDeviceD3D3Software::_prepare_keyed_sprite(D3D3SoftwareTexture &r_texture) {
	enum { COLOR_KEY = 0xff00ff };
	D3D3SoftwareTextureView sprite;
	DDSURFACEDESC output = {};
	output.dwSize = sizeof(output);
	output.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	output.dwWidth = r_texture.upload_size.x;
	output.dwHeight = r_texture.upload_size.y;
	output.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	output.ddpfPixelFormat = _alpha_texture_format;
	output.ddpfPixelFormat.dwFlags = DDPF_RGB;
	output.ddpfPixelFormat.dwRGBAlphaBitMask = 0;
	Error error = _check_result(_ddraw->CreateSurface(&output, &sprite.surface, nullptr));
	if (error != OK) {
		return error;
	}
	DDCOLORKEY key = { COLOR_KEY, COLOR_KEY };
	error = _check_result(sprite.surface->SetColorKey(DDCKEY_SRCBLT, &key));
	if (error != OK) {
		return error;
	}
	DDSURFACEDESC input = {};
	input.dwSize = sizeof(input);
	error = _check_result(r_texture.alpha.surface->Lock(nullptr, &input, DDLOCK_WAIT | DDLOCK_READONLY, nullptr));
	if (error != OK) {
		return error;
	}
	error = _check_result(sprite.surface->Lock(nullptr, &output, DDLOCK_WAIT, nullptr));
	if (error == OK) {
		bool compatible = true;
		for (int y = 0; y < r_texture.upload_size.y && compatible; y++) {
			const uint32_t *src = reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(input.lpSurface) + ptrdiff_t(y) * input.lPitch);
			uint32_t *dst = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(output.lpSurface) + ptrdiff_t(y) * output.lPitch);
			for (int x = 0; x < r_texture.upload_size.x; x++) {
				const uint32_t color = src[x] & 0xffffff;
				const uint32_t alpha = src[x] >> 24;
				// Image::detect_alpha allows almost-binary values; a color key requires exact coverage.
				if (alpha != 0 && (alpha != 255 || color == COLOR_KEY)) {
					compatible = false;
					break;
				}
				dst[x] = alpha ? color : uint32_t(COLOR_KEY);
			}
		}
		error = _check_result(sprite.surface->Unlock(nullptr));
		if (error == OK && !compatible) {
			error = ERR_UNAVAILABLE;
		}
	}
	const Error unlocked = _check_result(r_texture.alpha.surface->Unlock(nullptr));
	if (unlocked != OK) {
		return unlocked;
	}
	if (error == OK) {
		SWAP(r_texture.keyed_sprite, sprite.surface);
	}
	return error;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_blit_canvas_sprite(D3D3SoftwareTexture &r_texture, const FixedFunctionVertex *p_vertices, const CameraMatrix &p_clip_from_model, const Rect2i &p_viewport) {
	if (!_pass->rgb888_fill || _pass->native_size != _pass->size) {
		return ERR_UNAVAILABLE;
	}
	const FixedFunctionVertex &first = p_vertices[0];
	const FixedFunctionVertex &last = p_vertices[2];
	const Vector2 texture_size(r_texture.upload_size);
	const Point2 start = Point2(first.u, first.v) * texture_size;
	const Point2 end = Point2(last.u, last.v) * texture_size;
	const Rect2 dest(first.x, first.y, last.x - first.x, last.y - first.y);
	if (!(start.x >= 0 && start.y >= 0 && end.x <= texture_size.x && end.y <= texture_size.y && end.x > start.x && end.y > start.y) ||
			Math::is_inf(dest.position.x) || Math::is_inf(dest.position.y) ||
			start != start.floor() || end != end.floor() || dest.position != dest.position.floor() || dest.size != end - start) {
		return ERR_UNAVAILABLE;
	}
	const bool right[] = { false, true, true, false, true, false };
	const bool bottom[] = { false, false, true, false, true, true };
	for (int i = 0; i < 6; i++) {
		const FixedFunctionVertex &v = p_vertices[i];
		if (v.color != 0xffffffff || v.z != 0 || v.x != (right[i] ? last.x : first.x) || v.u != (right[i] ? last.u : first.u) ||
				v.y != (bottom[i] ? last.y : first.y) || v.v != (bottom[i] ? last.v : first.v)) {
			return ERR_UNAVAILABLE;
		}
	}
	CameraMatrix canvas;
	canvas.matrix[0][0] = real_t(2) / _pass->size.x;
	canvas.matrix[1][1] = real_t(-2) / _pass->size.y;
	canvas.matrix[3][0] = -1;
	canvas.matrix[3][1] = 1;
	if (!_projection_matches(p_clip_from_model, canvas)) {
		return ERR_UNAVAILABLE;
	}
	RECT dst;
	if (!_get_clipped_blt_rect(dest, p_viewport, dst)) {
		return ERR_UNAVAILABLE;
	}
	if (dst.left == dst.right || dst.top == dst.bottom) {
		return OK;
	}
	if (!r_texture.keyed_sprite || r_texture.keyed_sprite->IsLost() != DD_OK) {
		const Error prepared = _prepare_keyed_sprite(r_texture);
		if (prepared != OK) {
			r_texture.sprite_blit_eligible = prepared == ERR_SKIP;
			return prepared == ERR_SKIP ? prepared : ERR_UNAVAILABLE;
		}
	}
	const LONG offset_x = LONG(start.x - dest.position.x);
	const LONG offset_y = LONG(start.y - dest.position.y);
	RECT src = { dst.left + offset_x, dst.top + offset_y, dst.right + offset_x, dst.bottom + offset_y };
	const Error copied = _check_result(_pass->color->Blt(&dst, r_texture.keyed_sprite, &src, DDBLT_KEYSRC | DDBLT_WAIT, nullptr));
	return copied == OK || copied == ERR_SKIP ? copied : ERR_UNAVAILABLE;
}
