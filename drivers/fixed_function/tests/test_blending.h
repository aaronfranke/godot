#ifndef TEST_FIXED_FUNCTION_BLENDING_H
#define TEST_FIXED_FUNCTION_BLENDING_H

#include "drivers/fixed_function/graphics_device.h"

#include <stdio.h>

#include <limits>
#include <vector>

#define FIXED_FUNCTION_BLEND_CHECK(m_condition)                               \
	do {                                                                      \
		if (!(m_condition)) {                                                 \
			printf("Blending FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                     \
		}                                                                     \
	} while (false)

class TestFixedFunctionBlending {
	typedef FixedFunctionGraphicsDevice::FFDrawParameters FFDrawParameters;
	typedef FixedFunctionGraphicsDevice Device;
	static bool _texture_masks(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels);

	static Ref<Image> _make_image(bool p_premultiplied) {
		PoolVector<uint8_t> data;
		data.resize(16);
		{
			PoolVector<uint8_t>::Write write = data.write();
			uint8_t *pixels = write.ptr();
			for (int i = 0; i < 4; i++) {
				// Binary texture alpha is representable even by the A1R5G5B5 fallback.
				const bool transparent = i % 2 == 0;
				pixels[i * 4] = 0;
				pixels[i * 4 + 1] = p_premultiplied && transparent ? 0 : 255;
				pixels[i * 4 + 2] = 0;
				pixels[i * 4 + 3] = transparent ? 0 : 255;
			}
		}
		Ref<Image> image;
		image.instance();
		image->create(2, 2, false, Image::FORMAT_RGBA8, data);
		return image;
	}

	static Error _draw_rectangle(FixedFunctionGraphicsDevice &p_device, uint32_t p_left, uint32_t p_top, uint32_t p_right, uint32_t p_bottom, uint32_t p_color, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters()) {
		const float left = float(p_left) * 2.0f / p_device.get_width() - 1.0f;
		const float right = float(p_right) * 2.0f / p_device.get_width() - 1.0f;
		const float top = 1.0f - float(p_top) * 2.0f / p_device.get_height();
		const float bottom = 1.0f - float(p_bottom) * 2.0f / p_device.get_height();
		const FixedFunctionVertex vertices[] = {
			{ left, top, 0.5f, p_color, 0, 0 },
			{ right, top, 0.5f, p_color, 1, 0 },
			{ right, bottom, 0.5f, p_color, 1, 1 },
			{ left, top, 0.5f, p_color, 0, 0 },
			{ right, bottom, 0.5f, p_color, 1, 1 },
			{ left, bottom, 0.5f, p_color, 0, 1 },
		};
		return p_device.draw_triangles(vertices, 6, CameraMatrix(), p_texture, p_state);
	}

	static bool _color_matches(uint32_t p_actual, uint32_t p_expected) {
		// Allow native 16-bit target/texture quantization and blend rounding.
		for (int shift = 0; shift < 24; shift += 8) {
			int difference = int((p_actual >> shift) & 255) - int((p_expected >> shift) & 255);
			if (difference < -12 || difference > 12) {
				printf("Blending pixel: actual=0x%08x expected=0x%08x\n", p_actual, p_expected);
				return false;
			}
		}
		return (p_actual >> 24) == 255;
	}

	static bool _render_and_check(FixedFunctionGraphicsDevice &p_device, RID p_straight_texture, RID p_premultiplied_texture, std::vector<uint32_t> &r_pixels) {
		FIXED_FUNCTION_BLEND_CHECK(p_device.begin_frame(0xff0000ff) == OK);
		const uint32_t width = p_device.get_width();
		const uint32_t height = p_device.get_height();
		const FixedFunctionGraphicsDevice::FFCapabilities &caps = p_device.get_capabilities();
		for (uint32_t column = 0; column < 4; column++) {
			const bool premultiplied = (column % 2) != 0;
			const bool textured = column >= 2;
			const bool supported = premultiplied ? caps.blend_premultiplied_alpha : caps.blend_mix;
			FFDrawParameters state;
			state.blend_mode = premultiplied ? FixedFunctionGraphicsDevice::BLEND_MODE_PREMULTIPLIED_ALPHA : FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
			const uint32_t left = column * width / 4 + 8;
			const uint32_t right = (column + 1) * width / 4 - 8;
			uint32_t color;
			RID texture;
			if (textured) {
				color = premultiplied ? 0x80808080 : 0x80ffffff;
				texture = premultiplied ? p_premultiplied_texture : p_straight_texture;
			} else {
				color = premultiplied ? 0x80800000 : 0x80ff0000;
			}
			const Error result = _draw_rectangle(p_device, left, 8, right, height - 8, color, texture, state);
			FIXED_FUNCTION_BLEND_CHECK(result == (supported ? OK : ERR_UNAVAILABLE));
			if (!supported) {
				continue;
			}
			// Alpha zero preserves the destination, including with nonzero straight RGB.
			FIXED_FUNCTION_BLEND_CHECK(_draw_rectangle(p_device, left, height / 4, right, height / 3, premultiplied ? 0 : 0x00ffffff, RID(), state) == OK);
			// An ordered second translucent layer must blend with the first layer.
			uint32_t overlay = color;
			if (!textured) {
				overlay = premultiplied ? 0x80008000 : 0x8000ff00;
			}
			FIXED_FUNCTION_BLEND_CHECK(_draw_rectangle(p_device, left, height / 2, right, height * 3 / 4, overlay, texture, state) == OK);
			FIXED_FUNCTION_BLEND_CHECK(_draw_rectangle(p_device, left, height / 12, right, height / 6, 0xffff0000, RID(), state) == OK);
			FFDrawParameters invalid;
			invalid.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MAX;
			FIXED_FUNCTION_BLEND_CHECK(p_device.draw_triangles(nullptr, 0, CameraMatrix(), RID(), invalid) == ERR_INVALID_PARAMETER);
			// Default opaque drawing must replace the destination even with alpha zero.
			FIXED_FUNCTION_BLEND_CHECK(_draw_rectangle(p_device, left, height * 7 / 8, right, height - 8, 0x00ffffff) == OK);
		}
		FIXED_FUNCTION_BLEND_CHECK(p_device.end_frame(false) == OK);
		r_pixels.resize(size_t(width) * height);
		FIXED_FUNCTION_BLEND_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FIXED_FUNCTION_BLEND_CHECK(_color_matches(r_pixels[size_t(2) * width + 2], 0xff0000ff));
		for (uint32_t column = 0; column < 4; column++) {
			const bool supported = column % 2 ? caps.blend_premultiplied_alpha : caps.blend_mix;
			const uint32_t left = column * width / 4 + 8;
			const uint32_t right = (column + 1) * width / 4 - 8;
			const uint32_t x = column >= 2 ? left + (right - left) * 3 / 4 : (left + right) / 2;
			if (!supported) {
				FIXED_FUNCTION_BLEND_CHECK(_color_matches(r_pixels[size_t(height / 2) * width + x], 0xff0000ff));
				continue;
			}
			const uint32_t base = column >= 2 ? 0xff00807f : 0xff80007f;
			FIXED_FUNCTION_BLEND_CHECK(_color_matches(r_pixels[size_t(height * 3 / 8) * width + x], base));
			FIXED_FUNCTION_BLEND_CHECK(_color_matches(r_pixels[size_t(height * 7 / 24) * width + x], base));
			FIXED_FUNCTION_BLEND_CHECK(_color_matches(r_pixels[size_t(height * 5 / 8) * width + x], column >= 2 ? 0xff00c03f : 0xff40803f));
			if (column >= 2) {
				FIXED_FUNCTION_BLEND_CHECK(_color_matches(r_pixels[size_t(height * 3 / 8) * width + left + (right - left) / 4], 0xff0000ff));
			}
			FIXED_FUNCTION_BLEND_CHECK(_color_matches(r_pixels[size_t(height / 8) * width + x], 0xffff0000));
			FIXED_FUNCTION_BLEND_CHECK(_color_matches(r_pixels[size_t(height * 11 / 12) * width + x], 0xffffffff));
		}
		return true;
	}

	static bool _render_modes_and_cutoff(FixedFunctionGraphicsDevice &p_device, RID p_texture, std::vector<uint32_t> &r_pixels) {
		FIXED_FUNCTION_BLEND_CHECK(p_device.begin_frame(0xff182430) == OK);
		const uint32_t width = p_device.get_width();
		const uint32_t height = p_device.get_height();
		const Device::FFCapabilities &caps = p_device.get_capabilities();
		const float below_modulated_alpha = 126.0f / 255.0f;
		const float equal_alpha = 128.0f / 255.0f;
		const float above_alpha = 129.0f / 255.0f;
		const float between_alphas = 128.25f / 255.0f;
		const struct {
			float cutoff;
			uint32_t color;
			uint32_t background;
			uint32_t expected_left;
			uint32_t expected_right;
			Device::FFBlendMode mode;
			bool textured;
			bool cutoff_enabled;
			bool repeat;
		} cases[] = {
			// Add scales source RGB by alpha, adds the destination, and saturates.
			{ 0.5f, 0x80ff0000, 0xff204060, 0xffa04060, 0xffa04060, Device::BLEND_MODE_ADD, false, false, false },
			{ 0.5f, 0x00ff0000, 0xff204060, 0xff204060, 0xff204060, Device::BLEND_MODE_ADD, false, false, false },
			{ 0.5f, 0xffff0000, 0xff204060, 0xffff4060, 0xffff4060, Device::BLEND_MODE_ADD, false, false, false },
			{ 0.5f, 0x80ff0000, 0xff204060, 0xffff4060, 0xffff4060, Device::BLEND_MODE_ADD, false, false, true },
			// Multiply ignores alpha, including alpha zero, as in Godot's GLES renderer.
			{ 0.5f, 0x00808080, 0xff80c0ff, 0xff406080, 0xff406080, Device::BLEND_MODE_MUL, false, false, false },
			{ 0.5f, 0x00808080, 0xff80c0ff, 0xff203040, 0xff203040, Device::BLEND_MODE_MUL, false, false, true },
			{ 0.5f, 0x80ffffff, 0xff204060, 0xff204060, 0xff20c060, Device::BLEND_MODE_ADD, true, false, false },
			{ 0.5f, 0x80ffffff, 0xff204060, 0xff004000, 0xff004000, Device::BLEND_MODE_MUL, true, false, false },
			// Check equality on raw vertex alpha. Modulated alpha can round down by one.
			{ equal_alpha, 0x8000ff00, 0xff204060, 0xff00ff00, 0xff00ff00, Device::BLEND_MODE_DISABLED, false, true, false },
			{ below_modulated_alpha, 0x80ffffff, 0xff204060, 0xff204060, 0xff00ff00, Device::BLEND_MODE_DISABLED, true, true, false },
			{ above_alpha, 0x80ffffff, 0xff204060, 0xff204060, 0xff204060, Device::BLEND_MODE_DISABLED, true, true, false },
			{ 0.5f, 0x40ffffff, 0xff204060, 0xff204060, 0xff204060, Device::BLEND_MODE_ADD, true, true, false },
			{ below_modulated_alpha, 0x80ffffff, 0xff204060, 0xff204060, 0xff20c060, Device::BLEND_MODE_ADD, true, true, false },
			{ below_modulated_alpha, 0x80ffffff, 0xff204060, 0xff204060, 0xff004000, Device::BLEND_MODE_MUL, true, true, false },
			{ equal_alpha, 0x80ff0000, 0xff204060, 0xff902030, 0xff902030, Device::BLEND_MODE_MIX, false, true, false },
			{ equal_alpha, 0x80800000, 0xff204060, 0xff902030, 0xff902030, Device::BLEND_MODE_PREMULTIPLIED_ALPHA, false, true, false },
			{ 0.0f, 0x0000ff00, 0xff204060, 0xff00ff00, 0xff00ff00, Device::BLEND_MODE_DISABLED, false, true, false },
			// Finite out-of-range cutoffs follow the shader comparison; disabled cutoff is ignored.
			{ -1.0f, 0x0000ff00, 0xff204060, 0xff00ff00, 0xff00ff00, Device::BLEND_MODE_DISABLED, false, true, false },
			{ 1.0f, 0xff00ff00, 0xff204060, 0xff00ff00, 0xff00ff00, Device::BLEND_MODE_DISABLED, false, true, false },
			{ 1.1f, 0xff00ff00, 0xff204060, 0xff204060, 0xff204060, Device::BLEND_MODE_DISABLED, false, true, false },
			{ std::numeric_limits<float>::quiet_NaN(), 0x0000ff00, 0xff204060, 0xff00ff00, 0xff00ff00, Device::BLEND_MODE_DISABLED, false, false, false },
			{ 0.5f, 0x40ff0000, 0xff204060, 0xff204060, 0xff204060, Device::BLEND_MODE_MIX, false, true, false },
			{ 0.5f, 0x40400000, 0xff204060, 0xff204060, 0xff204060, Device::BLEND_MODE_PREMULTIPLIED_ALPHA, false, true, false },
			{ 0.5f, 0x00808080, 0xff204060, 0xff204060, 0xff204060, Device::BLEND_MODE_MUL, false, true, false },
			{ between_alphas, 0x8000ff00, 0xff204060, 0xff204060, 0xff204060, Device::BLEND_MODE_DISABLED, false, true, false },
		};
		const uint32_t columns = 4;
		const uint32_t count = sizeof(cases) / sizeof(cases[0]);
		const uint32_t rows = (count + columns - 1) / columns;
		bool supported_cases[count];
		for (uint32_t i = 0; i < count; i++) {
			const uint32_t left = (i % columns) * width / columns + 4;
			const uint32_t right = (i % columns + 1) * width / columns - 4;
			const uint32_t top = (i / columns) * height / rows + 4;
			const uint32_t bottom = (i / columns + 1) * height / rows - 4;
			FFDrawParameters state;
			state.blend_mode = cases[i].mode;
			state.alpha_cutoff_enabled = cases[i].cutoff_enabled;
			state.alpha_cutoff = cases[i].cutoff;
			bool &supported = supported_cases[i];
			supported = !state.alpha_cutoff_enabled || caps.alpha_cutoff;
			switch (state.blend_mode) {
				case Device::BLEND_MODE_ADD:
					supported = supported && caps.blend_add;
					break;
				case Device::BLEND_MODE_MUL:
					supported = supported && caps.blend_mul;
					break;
				case Device::BLEND_MODE_MIX:
					supported = supported && caps.blend_mix;
					break;
				case Device::BLEND_MODE_PREMULTIPLIED_ALPHA:
					supported = supported && caps.blend_premultiplied_alpha;
					break;
				default:
					break;
			}
			FIXED_FUNCTION_BLEND_CHECK(_draw_rectangle(p_device, left, top, right, bottom, cases[i].background) == OK);
			const Error expected = supported ? OK : ERR_UNAVAILABLE;
			FIXED_FUNCTION_BLEND_CHECK(_draw_rectangle(p_device, left, top, right, bottom, cases[i].color, cases[i].textured ? p_texture : RID(), state) == expected);
			if (cases[i].repeat) {
				FIXED_FUNCTION_BLEND_CHECK(_draw_rectangle(p_device, left, top, right, bottom, cases[i].color, cases[i].textured ? p_texture : RID(), state) == expected);
			}
			// This alpha-zero opaque marker checks that both blending and cutoff are disabled again.
			FIXED_FUNCTION_BLEND_CHECK(_draw_rectangle(p_device, left, bottom - 4, right, bottom, 0x00ffffff) == OK);
		}
		const float invalid_cutoffs[] = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };
		for (size_t i = 0; i < sizeof(invalid_cutoffs) / sizeof(invalid_cutoffs[0]); i++) {
			FFDrawParameters invalid;
			invalid.alpha_cutoff_enabled = true;
			invalid.alpha_cutoff = invalid_cutoffs[i];
			FIXED_FUNCTION_BLEND_CHECK(_draw_rectangle(p_device, 0, 0, width, height, 0xffff0000, RID(), invalid) == ERR_INVALID_PARAMETER);
			FIXED_FUNCTION_BLEND_CHECK(p_device.draw_triangles(nullptr, 0, CameraMatrix(), RID(), invalid) == ERR_INVALID_PARAMETER);
		}
		FIXED_FUNCTION_BLEND_CHECK(p_device.end_frame(false) == OK);
		r_pixels.resize(size_t(width) * height);
		FIXED_FUNCTION_BLEND_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		for (uint32_t i = 0; i < count; i++) {
			const uint32_t left = (i % columns) * width / columns + 4;
			const uint32_t right = (i % columns + 1) * width / columns - 4;
			const uint32_t top = (i / columns) * height / rows + 4;
			const uint32_t bottom = (i / columns + 1) * height / rows - 4;
			const bool supported = supported_cases[i];
			// Sample the blend above the four-pixel marker, including on the smallest probe surface.
			const size_t sample_row = size_t((top + bottom - 4) / 2) * width;
			const bool pixels_match = _color_matches(r_pixels[sample_row + left + (right - left) / 4], supported ? cases[i].expected_left : cases[i].background) &&
					_color_matches(r_pixels[sample_row + left + (right - left) * 3 / 4], supported ? cases[i].expected_right : cases[i].background) &&
					_color_matches(r_pixels[size_t(bottom - 2) * width + (left + right) / 2], 0xffffffff);
			if (!pixels_match) {
				printf("Blend/cutoff case %u failed\n", i + 1);
			}
			FIXED_FUNCTION_BLEND_CHECK(pixels_match);
		}
		return true;
	}

public:
	static bool test(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
		const uint32_t width = p_device.get_width();
		const uint32_t height = p_device.get_height();
		printf("Blend caps: mix=%s premultiplied_alpha=%s\n", p_device.get_capabilities().blend_mix ? "yes" : "no", p_device.get_capabilities().blend_premultiplied_alpha ? "yes" : "no");
		printf("Draw caps: add=%s mul=%s alpha_cutoff=%s\n", p_device.get_capabilities().blend_add ? "yes" : "no", p_device.get_capabilities().blend_mul ? "yes" : "no", p_device.get_capabilities().alpha_cutoff ? "yes" : "no");
		RID straight_texture;
		RID premultiplied_texture;
		FIXED_FUNCTION_BLEND_CHECK(p_device.texture_create(_make_image(false), straight_texture) == OK);
		FIXED_FUNCTION_BLEND_CHECK(p_device.texture_create(_make_image(true), premultiplied_texture) == OK);
		FIXED_FUNCTION_BLEND_CHECK(_render_and_check(p_device, straight_texture, premultiplied_texture, r_pixels));
		FIXED_FUNCTION_BLEND_CHECK(_render_modes_and_cutoff(p_device, straight_texture, r_pixels));
		FIXED_FUNCTION_BLEND_CHECK(p_device.resize(width + 64, height + 48) == OK);
		FIXED_FUNCTION_BLEND_CHECK(_render_and_check(p_device, straight_texture, premultiplied_texture, r_pixels));
		FIXED_FUNCTION_BLEND_CHECK(_render_modes_and_cutoff(p_device, straight_texture, r_pixels));
		FIXED_FUNCTION_BLEND_CHECK(p_device.set_use_vsync(false) == OK);
		FIXED_FUNCTION_BLEND_CHECK(_render_and_check(p_device, straight_texture, premultiplied_texture, r_pixels));
		FIXED_FUNCTION_BLEND_CHECK(_render_modes_and_cutoff(p_device, straight_texture, r_pixels));
		FIXED_FUNCTION_BLEND_CHECK(p_device.resize(0, 0) == OK);
		FIXED_FUNCTION_BLEND_CHECK(p_device.begin_frame(0) == ERR_SKIP);
		FIXED_FUNCTION_BLEND_CHECK(p_device.resize(width, height) == OK);
		FIXED_FUNCTION_BLEND_CHECK(_render_and_check(p_device, straight_texture, premultiplied_texture, r_pixels));
		FIXED_FUNCTION_BLEND_CHECK(_render_modes_and_cutoff(p_device, straight_texture, r_pixels));
		FIXED_FUNCTION_BLEND_CHECK(p_device.texture_free(straight_texture) == OK);
		FIXED_FUNCTION_BLEND_CHECK(p_device.texture_free(premultiplied_texture) == OK);
		FIXED_FUNCTION_BLEND_CHECK(_texture_masks(p_device, r_pixels));
		printf("PASS: supported alpha/add/multiply modes, independent cutoff, textured modulation, ordered layers, state switching, and reset recovery.\n");
		return true;
	}
};

#undef FIXED_FUNCTION_BLEND_CHECK

#endif // TEST_FIXED_FUNCTION_BLENDING_H
