#pragma once

#include "drivers/fixed_function/graphics_device.h"

#include <limits.h>
#include <stdio.h>

#include <vector>

#define FIXED_FUNCTION_CLIP_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Clipping FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

class TestFixedFunctionClipping {
	typedef FixedFunctionGraphicsDevice::FFDrawParameters FFDrawParameters;

	static uint32_t _texel(int p_x, int p_y) {
		return (p_x + p_y) % 2 ? 0xff00ffff : 0xffff00ff;
	}

	static Ref<Image> _make_image() {
		Vector<uint8_t> data;
		data.resize(16 * 16 * 4);
		uint8_t *pixels = data.ptrw();
		for (int y = 0; y < 16; y++) {
			for (int x = 0; x < 16; x++) {
				const uint32_t color = _texel(x, y);
				const int offset = (y * 16 + x) * 4;
				pixels[offset] = uint8_t(color >> 16);
				pixels[offset + 1] = uint8_t(color >> 8);
				pixels[offset + 2] = uint8_t(color);
				pixels[offset + 3] = 255;
			}
		}
		return Image::create_from_data(16, 16, false, Image::FORMAT_RGBA8, data);
	}

	static Projection _canvas_projection(uint32_t p_width, uint32_t p_height) {
		Projection projection;
		projection.columns[0][0] = real_t(2) / p_width;
		projection.columns[1][1] = real_t(-2) / p_height;
		// Canvas integer edges surround pixel centers at (x + 0.5, y + 0.5).
		projection.columns[3][0] = -1;
		projection.columns[3][1] = 1;
		return projection;
	}

	static Error _draw_rectangle(FixedFunctionGraphicsDevice &p_device, const Rect2i &p_rect, uint32_t p_color, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters()) {
		const float left = float(p_rect.position.x);
		const float top = float(p_rect.position.y);
		const float right = left + p_rect.size.x;
		const float bottom = top + p_rect.size.y;
		const FixedFunctionVertex vertices[] = {
			{ left, top, 0.5f, p_color, 0, 0 },
			{ right, top, 0.5f, p_color, 1, 0 },
			{ right, bottom, 0.5f, p_color, 1, 1 },
			{ left, top, 0.5f, p_color, 0, 0 },
			{ right, bottom, 0.5f, p_color, 1, 1 },
			{ left, bottom, 0.5f, p_color, 0, 1 },
		};
		return p_device.draw_triangles(vertices, 6, _canvas_projection(p_device.get_width(), p_device.get_height()), p_texture, p_state);
	}

	static bool _read_pixels(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
		FIXED_FUNCTION_CLIP_CHECK(p_device.end_frame(false) == OK);
		r_pixels.resize(size_t(p_device.get_width()) * p_device.get_height());
		FIXED_FUNCTION_CLIP_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		return true;
	}

	static bool _color_matches(uint32_t p_actual, uint32_t p_expected, uint32_t p_x, uint32_t p_y) {
		for (int shift = 0; shift < 24; shift += 8) {
			const int difference = int((p_actual >> shift) & 255) - int((p_expected >> shift) & 255);
			if (difference < -12 || difference > 12 || (p_actual >> 24) != 255) {
				printf("Clipping pixel (%u, %u): actual=0x%08x expected=0x%08x\n", p_x, p_y, p_actual, p_expected);
				return false;
			}
		}
		return true;
	}

	static bool _check_pixel_alignment(FixedFunctionGraphicsDevice &p_device, RID p_texture, std::vector<uint32_t> &r_pixels) {
		FIXED_FUNCTION_CLIP_CHECK(p_device.begin_frame(0xff182430) == OK);
		const int width = p_device.get_width();
		const int height = p_device.get_height();
		const Rect2i tiles[] = { Rect2i(0, 0, 16, 16), Rect2i(17, 19, 16, 16), Rect2i(width - 16, height - 16, 16, 16), Rect2i(37, 41, 16, 16) };
		FFDrawParameters state;
		state.clip_rect = Rect2i(41, 45, 7, 5);
		for (int i = 0; i < 4; i++) {
			state.clip_enabled = i == 3;
			FIXED_FUNCTION_CLIP_CHECK(_draw_rectangle(p_device, tiles[i], 0xffffffff, p_texture, state) == OK);
		}
		FIXED_FUNCTION_CLIP_CHECK(_draw_rectangle(p_device, Rect2i(width / 2, 0, 1, height), 0xffffffff) == OK);
		FIXED_FUNCTION_CLIP_CHECK(_draw_rectangle(p_device, Rect2i(0, height / 2, width, 1), 0xff00ff00) == OK);
		FIXED_FUNCTION_CLIP_CHECK(_read_pixels(p_device, r_pixels));
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				uint32_t expected = 0xff182430;
				for (int i = 0; i < 4; i++) {
					if (tiles[i].has_point(Point2i(x, y)) && (i != 3 || state.clip_rect.has_point(Point2i(x, y)))) {
						expected = _texel(x - tiles[i].position.x, y - tiles[i].position.y);
					}
				}
				if (x == width / 2) {
					expected = 0xffffffff;
				}
				if (y == height / 2) {
					expected = 0xff00ff00;
				}
				FIXED_FUNCTION_CLIP_CHECK(_color_matches(r_pixels[size_t(y) * width + x], expected, x, y));
			}
		}
		return true;
	}

	static bool _draw_pattern(FixedFunctionGraphicsDevice &p_device, RID p_texture, const FFDrawParameters &p_state) {
		FIXED_FUNCTION_CLIP_CHECK(_draw_rectangle(p_device, Rect2i(0, 0, p_device.get_width(), p_device.get_height()), 0xffffffff, p_texture, p_state) == OK);
		// Nonconstant homogeneous W and vertex colors must retain their interpolation when clipped.
		const FixedFunctionVertex triangle[] = {
			{ -0.9f, -0.8f, 0.4f, 0xffff0000, 0, 0 },
			{ 0.0f, 0.9f, -1.2f, 0xff00ff00, 0, 0 },
			{ 0.9f, -0.6f, 0.8f, 0xff0000ff, 0, 0 },
		};
		Projection perspective;
		perspective.columns[0][3] = real_t(0.25);
		FIXED_FUNCTION_CLIP_CHECK(p_device.draw_triangles(triangle, 3, perspective, RID(), p_state) == OK);
		FFDrawParameters blended = p_state;
		if (p_device.get_capabilities().blend_mix) {
			blended.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		}
		if (p_device.get_capabilities().alpha_cutoff) {
			blended.alpha_cutoff_enabled = true;
		}
		FIXED_FUNCTION_CLIP_CHECK(_draw_rectangle(p_device, Rect2i(7, 11, p_device.get_width() - 14, p_device.get_height() / 3), 0x80ffffff, p_texture, blended) == OK);
		return true;
	}

	static bool _render_and_check(FixedFunctionGraphicsDevice &p_device, RID p_texture, std::vector<uint32_t> &r_pixels) {
		FIXED_FUNCTION_CLIP_CHECK(p_device.begin_frame(0xff182430) == OK);
		const int width = p_device.get_width();
		const int height = p_device.get_height();
		std::vector<uint32_t> reference;
		FIXED_FUNCTION_CLIP_CHECK(_draw_pattern(p_device, p_texture, FFDrawParameters()));
		FIXED_FUNCTION_CLIP_CHECK(_read_pixels(p_device, reference));
		const Rect2i clips[] = {
			Rect2i(13, 17, width / 2, height / 2),
			Rect2i(0, 0, 1, 1),
			Rect2i(width - 1, height - 1, 1, 1),
			Rect2i(width / 2, 0, 1, height),
			Rect2i(0, height / 2, width, 1),
			Rect2i(-7, -11, 23, 27),
			Rect2i(width - 9, height - 5, 30, 40),
			Rect2i(0, 0, width, height),
			Rect2i(-16, -16, INT_MAX, INT_MAX),
			Rect2i(17, 19, 0, 8),
			Rect2i(17, 19, 8, 0),
			Rect2i(width, 0, 12, 12),
			Rect2i(INT_MAX - 2, 0, INT_MAX, height),
			Rect2i(INT_MIN, 0, INT_MAX, height),
		};
		for (size_t i = 0; i < sizeof(clips) / sizeof(clips[0]); i++) {
			FFDrawParameters state;
			state.clip_enabled = true;
			state.clip_rect = clips[i];
			FIXED_FUNCTION_CLIP_CHECK(p_device.begin_frame(0xff182430) == OK);
			FIXED_FUNCTION_CLIP_CHECK(_draw_pattern(p_device, p_texture, state));
			// Ordinary draws after clipping must restore both viewport and screen mapping.
			FIXED_FUNCTION_CLIP_CHECK(_draw_rectangle(p_device, Rect2i(0, height - 1, 1, 1), 0xffffffff) == OK);
			FIXED_FUNCTION_CLIP_CHECK(_draw_rectangle(p_device, Rect2i(width - 1, 0, 1, 1), 0xffffffff) == OK);
			FIXED_FUNCTION_CLIP_CHECK(_read_pixels(p_device, r_pixels));
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					const bool inside = x >= int64_t(clips[i].position.x) && y >= int64_t(clips[i].position.y) &&
							x < int64_t(clips[i].position.x) + clips[i].size.x && y < int64_t(clips[i].position.y) + clips[i].size.y;
					uint32_t expected = inside ? reference[size_t(y) * width + x] : 0xff182430;
					if ((x == 0 && y == height - 1) || (x == width - 1 && y == 0)) {
						expected = 0xffffffff;
					}
					if (!_color_matches(r_pixels[size_t(y) * width + x], expected, x, y)) {
						printf("Clip case %u failed\n", unsigned(i + 1));
						return false;
					}
				}
			}
		}
		// Negative sizes are invalid only when clipping is enabled, including empty draws.
		FFDrawParameters state;
		state.clip_rect = Rect2i(0, 0, -1, 1);
		FIXED_FUNCTION_CLIP_CHECK(p_device.begin_frame(0xff182430) == OK);
		FIXED_FUNCTION_CLIP_CHECK(_draw_pattern(p_device, p_texture, state));
		state.clip_enabled = true;
		FIXED_FUNCTION_CLIP_CHECK(_draw_rectangle(p_device, Rect2i(0, 0, width, height), 0xffff0000, RID(), state) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_CLIP_CHECK(p_device.draw_triangles(nullptr, 0, Projection(), RID(), state) == ERR_INVALID_PARAMETER);
		state.clip_rect = Rect2i(0, 0, 1, -1);
		FIXED_FUNCTION_CLIP_CHECK(p_device.draw_triangles(nullptr, 0, Projection(), RID(), state) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_CLIP_CHECK(_read_pixels(p_device, r_pixels));
		for (size_t i = 0; i < reference.size(); i++) {
			FIXED_FUNCTION_CLIP_CHECK(_color_matches(r_pixels[i], reference[i], i % width, i / width));
		}
		// A frame clear must cover the whole target even if the previous frame ended clipped.
		state.clip_rect = Rect2i(13, 17, width / 2, height / 2);
		FIXED_FUNCTION_CLIP_CHECK(p_device.begin_frame(0xff182430) == OK);
		FIXED_FUNCTION_CLIP_CHECK(_draw_pattern(p_device, p_texture, state));
		FIXED_FUNCTION_CLIP_CHECK(_read_pixels(p_device, r_pixels));
		FIXED_FUNCTION_CLIP_CHECK(p_device.begin_frame(0xffffffff) == OK);
		FIXED_FUNCTION_CLIP_CHECK(_read_pixels(p_device, r_pixels));
		for (size_t i = 0; i < r_pixels.size(); i++) {
			FIXED_FUNCTION_CLIP_CHECK(r_pixels[i] == 0xffffffff);
		}
		// Leave an illustrative clipped pattern for the optional capture.
		FIXED_FUNCTION_CLIP_CHECK(p_device.begin_frame(0xff182430) == OK);
		FIXED_FUNCTION_CLIP_CHECK(_draw_pattern(p_device, p_texture, state));
		FIXED_FUNCTION_CLIP_CHECK(_read_pixels(p_device, r_pixels));
		return true;
	}

public:
	static bool test(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
		const uint32_t width = p_device.get_width();
		const uint32_t height = p_device.get_height();
		RID texture;
		FIXED_FUNCTION_CLIP_CHECK(p_device.texture_create(_make_image(), texture) == OK);
		FIXED_FUNCTION_CLIP_CHECK(_check_pixel_alignment(p_device, texture, r_pixels));
		FIXED_FUNCTION_CLIP_CHECK(_render_and_check(p_device, texture, r_pixels));
		FIXED_FUNCTION_CLIP_CHECK(p_device.resize(width + 63, height + 47) == OK);
		FIXED_FUNCTION_CLIP_CHECK(_check_pixel_alignment(p_device, texture, r_pixels));
		FIXED_FUNCTION_CLIP_CHECK(_render_and_check(p_device, texture, r_pixels));
		FIXED_FUNCTION_CLIP_CHECK(p_device.set_use_vsync(true) == OK);
		FIXED_FUNCTION_CLIP_CHECK(_render_and_check(p_device, texture, r_pixels));
		FIXED_FUNCTION_CLIP_CHECK(p_device.set_use_vsync(false) == OK);
		FIXED_FUNCTION_CLIP_CHECK(p_device.resize(0, 0) == OK);
		FIXED_FUNCTION_CLIP_CHECK(p_device.begin_frame(0) == ERR_SKIP);
		FIXED_FUNCTION_CLIP_CHECK(p_device.resize(width, height) == OK);
		FIXED_FUNCTION_CLIP_CHECK(_check_pixel_alignment(p_device, texture, r_pixels));
		FIXED_FUNCTION_CLIP_CHECK(_render_and_check(p_device, texture, r_pixels));
		FIXED_FUNCTION_CLIP_CHECK(p_device.texture_free(texture) == OK);
		printf("PASS: rectangle clipping, exact boundaries, perspective/UV preservation, pixel alignment, state restoration, and reset recovery.\n");
		return true;
	}
};

#undef FIXED_FUNCTION_CLIP_CHECK
