#include "test_sampling.h"

#include "core/math/math_funcs.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define FIXED_FUNCTION_SAMPLING_CHECK(m_condition)                            \
	do {                                                                      \
		if (!(m_condition)) {                                                 \
			printf("Sampling FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                     \
		}                                                                     \
	} while (false)

static_assert(sizeof(FixedFunctionGraphicsDevice::FFDrawParameters) == (sizeof(void *) == 8 ? 48 : 36) &&
				offsetof(FixedFunctionGraphicsDevice::FFDrawParameters, sampler_flags) == 2 * sizeof(void *) + 21 &&
				offsetof(FixedFunctionGraphicsDevice::FFDrawParameters, clip_enabled) == 2 * sizeof(void *) + 23,
		"Unexpected draw packet packing.");
static_assert(sizeof(FixedFunctionGraphicsDevice::FFCapabilities) == 32, "Unexpected capability packing.");

bool TestFixedFunctionSampling::_supported(const FixedFunctionGraphicsDevice::FFCapabilities &p_caps, uint8_t p_flags) {
	if ((p_flags & FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR) && !p_caps.sampler_filter_linear) {
		return false;
	}
	const uint8_t repeat = p_flags & (FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V);
	const bool can_repeat = (p_flags & FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED) ? p_caps.sampler_repeat_mirrored : p_caps.sampler_repeat;
	return !repeat || (can_repeat && (repeat == (FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V) || p_caps.sampler_address_independent));
}

uint32_t TestFixedFunctionSampling::_texel(int p_x, int p_y, uint8_t p_flags) {
	const uint32_t colors[4] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };
	const bool mirror = p_flags & FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
	int coords[] = { p_x, p_y };
	const uint8_t repeat[] = { FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U, FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V };
	for (int axis = 0; axis < 2; axis++) {
		if (!(p_flags & repeat[axis])) {
			coords[axis] = CLAMP(coords[axis], 0, 1);
		} else if (mirror) {
			const int index = Math::posmod(coords[axis], 4);
			coords[axis] = MIN(index, 3 - index);
		} else {
			coords[axis] = Math::posmod(coords[axis], 2);
		}
	}
	return colors[coords[1] * 2 + coords[0]];
}

uint32_t TestFixedFunctionSampling::_sample(real_t p_u, real_t p_v, uint8_t p_flags) {
	if (!(p_flags & FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR)) {
		return _texel(int(Math::floor(p_u * 2)), int(Math::floor(p_v * 2)), p_flags);
	}
	// Linear filtering weights texel centers, including the wrap seam between last and first.
	const real_t x = p_u * 2 - real_t(0.5);
	const real_t y = p_v * 2 - real_t(0.5);
	const int left = int(Math::floor(x));
	const int top = int(Math::floor(y));
	const real_t fraction_x = x - left;
	const real_t fraction_y = y - top;
	uint32_t result = 0xff000000;
	for (int shift = 0; shift < 24; shift += 8) {
		real_t channel = 0;
		for (int row = 0; row < 2; row++) {
			for (int column = 0; column < 2; column++) {
				const real_t weight = (column ? fraction_x : 1 - fraction_x) * (row ? fraction_y : 1 - fraction_y);
				channel += ((_texel(left + column, top + row, p_flags) >> shift) & 255) * weight;
			}
		}
		result |= uint32_t(Math::round(channel)) << shift;
	}
	return result;
}

bool TestFixedFunctionSampling::_color_matches(uint32_t p_actual, uint32_t p_expected) {
	for (int shift = 0; shift < 24; shift += 8) {
		const int delta = int((p_actual >> shift) & 255) - int((p_expected >> shift) & 255);
		if (delta < -8 || delta > 8) {
			return false;
		}
	}
	return (p_actual >> 24) == 255;
}

Error TestFixedFunctionSampling::_draw_quad(FixedFunctionGraphicsDevice &p_device, RID p_texture, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_minify) {
	const real_t left = p_minify ? -1 + real_t(8) / p_device.get_width() : -1;
	const real_t right = p_minify ? -1 + real_t(10) / p_device.get_width() : 1;
	const real_t top = p_minify ? 1 - real_t(8) / p_device.get_height() : 1;
	const real_t bottom = p_minify ? 1 - real_t(10) / p_device.get_height() : -1;
	const float uv_min = p_minify ? 0 : -1;
	const float uv_max = p_minify ? 1 : 2;
	const FixedFunctionVertex quad[] = {
		{ float(left), float(top), 0, 0xffffffff, uv_min, uv_min },
		{ float(right), float(top), 0, 0xffffffff, uv_max, uv_min },
		{ float(right), float(bottom), 0, 0xffffffff, uv_max, uv_max },
		{ float(left), float(top), 0, 0xffffffff, uv_min, uv_min },
		{ float(right), float(bottom), 0, 0xffffffff, uv_max, uv_max },
		{ float(left), float(bottom), 0, 0xffffffff, uv_min, uv_max },
	};
	return p_device.draw_triangles(quad, 6, CameraMatrix(), p_texture, p_state);
}

bool TestFixedFunctionSampling::_render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, RID p_texture, std::vector<uint32_t> &r_pixels, bool p_mirror) {
	FIXED_FUNCTION_SAMPLING_CHECK(p_context.begin_frame(0) == OK);
	FIXED_FUNCTION_SAMPLING_CHECK(p_context.begin_device_frame(0xff102030) == OK);
	const int width = p_device.get_width();
	const int height = p_device.get_height();
	FixedFunctionGraphicsDevice::FFDrawParameters invalid;
	invalid.sampler_flags = 0x80;
	FIXED_FUNCTION_SAMPLING_CHECK(_draw_quad(p_device, p_texture, invalid) == ERR_INVALID_PARAMETER);
	invalid.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
	FIXED_FUNCTION_SAMPLING_CHECK(_draw_quad(p_device, p_texture, invalid) == ERR_INVALID_PARAMETER);
	for (int i = 0; i < 9; i++) {
		// Repeat default state after all sampler modes to detect state leaking between draws.
		const int mode = i % 8;
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.sampler_flags = mode;
		if (p_mirror && mode >= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U) {
			state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
		}
		state.clip_enabled = true;
		state.clip_rect = Rect2i((mode % 4) * (width / 4), (mode / 4) * (height / 2), width / 4, height / 2);
		FIXED_FUNCTION_SAMPLING_CHECK(_draw_quad(p_device, p_texture, state) == (_supported(p_device.get_capabilities(), state.sampler_flags) ? OK : ERR_UNAVAILABLE));
	}
	FIXED_FUNCTION_SAMPLING_CHECK(p_context.end_frame(false) == OK);
	r_pixels.resize(size_t(width) * height);
	FIXED_FUNCTION_SAMPLING_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint8_t mode = (y / (height / 2)) * 4 + x / (width / 4);
			if (p_mirror && mode >= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U) {
				mode |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
			}
			const uint32_t expected = _supported(p_device.get_capabilities(), mode) ? _sample((real_t(x) + real_t(0.5)) * 3 / width - 1, (real_t(y) + real_t(0.5)) * 3 / height - 1, mode) : 0xff102030;
			const uint32_t actual = r_pixels[size_t(y) * width + x];
			if (!_color_matches(actual, expected)) {
				printf("Sampling pixel (%d, %d) mode=%u actual=%08x expected=%08x\n", x, y, unsigned(mode), actual, expected);
				return false;
			}
		}
	}
	return true;
}

bool TestFixedFunctionSampling::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FIXED_FUNCTION_SAMPLING_CHECK(context.initialize() == OK);
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = p_device.get_capabilities();
	printf("Sampler caps: linear=%s repeat=%s mirrored=%s independent_uv=%s\n", caps.sampler_filter_linear ? "yes" : "no", caps.sampler_repeat ? "yes" : "no", caps.sampler_repeat_mirrored ? "yes" : "no", caps.sampler_address_independent ? "yes" : "no");
	const uint8_t colors[] = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255 };
	PoolVector<uint8_t> data;
	data.resize(sizeof(colors));
	{
		PoolVector<uint8_t>::Write write = data.write();
		memcpy(write.ptr(), colors, sizeof(colors));
	}
	Ref<Image> image;
	image.instance();
	image->create(2, 2, false, Image::FORMAT_RGBA8, data);
	RID texture;
	FIXED_FUNCTION_SAMPLING_CHECK(p_device.texture_create(image, texture) == OK);
	FIXED_FUNCTION_SAMPLING_CHECK(_render_and_check(context, p_device, texture, r_pixels));
	if (caps.sampler_filter_linear) {
		FIXED_FUNCTION_SAMPLING_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_SAMPLING_CHECK(context.begin_device_frame(0xff102030) == OK);
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
		FIXED_FUNCTION_SAMPLING_CHECK(_draw_quad(p_device, texture, state, true) == OK);
		FIXED_FUNCTION_SAMPLING_CHECK(context.end_frame(false) == OK);
		FIXED_FUNCTION_SAMPLING_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FIXED_FUNCTION_SAMPLING_CHECK(_color_matches(r_pixels[4 * CAPTURE_WIDTH + 4], 0xff808080));
	}
	FIXED_FUNCTION_SAMPLING_CHECK(context.resize(136, 104) == OK);
	FIXED_FUNCTION_SAMPLING_CHECK(_render_and_check(context, p_device, texture, r_pixels));
	FIXED_FUNCTION_SAMPLING_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FIXED_FUNCTION_SAMPLING_CHECK(context.set_use_vsync(true) == OK);
	FIXED_FUNCTION_SAMPLING_CHECK(_render_and_check(context, p_device, texture, r_pixels, true));
	FIXED_FUNCTION_SAMPLING_CHECK(_render_and_check(context, p_device, texture, r_pixels));
	FIXED_FUNCTION_SAMPLING_CHECK(p_device.texture_free(texture) == OK);
	FIXED_FUNCTION_SAMPLING_CHECK(context.finalize() == OK);
	printf("PASS: sampler modes, CPU bilinear reference, negative UVs, wrap and mirror seams, independent addressing, minification/magnification, state restoration, and recovery.\n");
	return true;
}

#undef FIXED_FUNCTION_SAMPLING_CHECK
