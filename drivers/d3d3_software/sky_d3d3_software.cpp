#include "drivers/d3d3_software/graphics_device_d3d3_software.h"

#include <float.h>

uint32_t FixedFunctionGraphicsDeviceD3D3Software::_pack_surface_color(uint32_t p_color, const DDPIXELFORMAT &p_format) {
	const uint32_t masks[] = { p_format.dwRBitMask, p_format.dwGBitMask, p_format.dwBBitMask };
	uint32_t packed = p_format.dwRGBAlphaBitMask;
	for (int channel = 0; channel < 3; channel++) {
		uint32_t mask = masks[channel];
		if (!mask) {
			continue;
		}
		unsigned shift = 0;
		while (!(mask & 1)) {
			mask >>= 1;
			shift++;
		}
		const uint32_t value = (p_color >> (16 - channel * 8)) & 255;
		packed |= uint32_t((uint64_t(value) * mask + 127) / 255) << shift;
	}
	return packed;
}

Error FixedFunctionGraphicsDeviceD3D3Software::draw_sky_hemispheres(real_t p_horizon_y, uint32_t p_sky_color, uint32_t p_ground_color, uint32_t &r_draw_calls) {
	r_draw_calls = 0;
	const Error error = _check_thread();
	if (error != OK || !_frame_active) {
		return error == OK ? ERR_UNCONFIGURED : error;
	}
	if (!(p_horizon_y >= -FLT_MAX && p_horizon_y <= FLT_MAX)) {
		return ERR_INVALID_PARAMETER;
	}
	const int width = _pass->native_size.x;
	const int height = _pass->native_size.y;
	const int split = int(Math::floor(CLAMP(p_horizon_y, real_t(0), real_t(1)) * height + real_t(0.5)));
	DDPIXELFORMAT format = {};
	format.dwSize = sizeof(format);
	const Error pixel_format = _check_result(_pass->color->GetPixelFormat(&format));
	if (pixel_format != OK) {
		return pixel_format;
	}
	if (!(format.dwFlags & DDPF_RGB) || !format.dwRBitMask || !format.dwGBitMask || !format.dwBBitMask) {
		return ERR_UNAVAILABLE;
	}
	RECT rectangles[] = { { 0, 0, width, split }, { 0, split, width, height } };
	const uint32_t colors[] = { p_sky_color, p_ground_color };
	for (int i = 0; i < 2; i++) {
		if (rectangles[i].top == rectangles[i].bottom) {
			continue;
		}
		DDBLTFX fill = {};
		fill.dwSize = sizeof(fill);
		fill.dwFillColor = _pack_surface_color(colors[i], format);
		const Error filled = _check_result(_pass->color->Blt(&rectangles[i], nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fill));
		if (filled != OK) {
			// A partial background must cancel the frame instead of appearing complete.
			return filled == ERR_UNAVAILABLE && r_draw_calls ? FAILED : filled;
		}
		r_draw_calls++;
	}
	return OK;
}
