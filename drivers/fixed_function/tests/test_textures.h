#pragma once

#include "core/templates/rid_owner.h"
#include "drivers/fixed_function/graphics_device.h"

#include <stdio.h>
#include <string.h>

#include <vector>

#define FIXED_FUNCTION_TEXTURE_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Texture FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

class TestFixedFunctionTextures {
	static Ref<Image> _make_image() {
		const uint8_t pixels[] = {
			255,
			0,
			0,
			255,
			0,
			255,
			0,
			255,
			0,
			0,
			255,
			255,
			255,
			255,
			255,
			255,
		};
		Vector<uint8_t> data;
		data.resize(sizeof(pixels));
		memcpy(data.ptrw(), pixels, sizeof(pixels));
		return Image::create_from_data(2, 2, false, Image::FORMAT_RGBA8, data);
	}

	static bool _color_matches(uint32_t p_actual, uint32_t p_expected) {
		for (int shift = 0; shift < 24; shift += 8) {
			int difference = int((p_actual >> shift) & 255) - int((p_expected >> shift) & 255);
			if (difference < -8 || difference > 8) {
				return false;
			}
		}
		return (p_actual >> 24) == 255;
	}

	static bool _render_and_check(FixedFunctionGraphicsDevice &p_device, RID p_texture, uint32_t p_modulation, bool p_outside_uvs, std::vector<uint32_t> &r_pixels, RID p_overlay_texture = RID()) {
		FixedFunctionVertex quad[] = {
			{ -0.75f, 0.75f, 0.5f, p_modulation, 0, 0 },
			{ 0.75f, 0.75f, 0.5f, p_modulation, 1, 0 },
			{ 0.75f, -0.75f, 0.5f, p_modulation, 1, 1 },
			{ -0.75f, 0.75f, 0.5f, p_modulation, 0, 0 },
			{ 0.75f, -0.75f, 0.5f, p_modulation, 1, 1 },
			{ -0.75f, -0.75f, 0.5f, p_modulation, 0, 1 },
		};
		if (p_outside_uvs) {
			for (size_t i = 0; i < 6; i++) {
				quad[i].u = quad[i].u * 2.0f - 0.5f;
				quad[i].v = quad[i].v * 2.0f - 0.5f;
			}
		}
		const FixedFunctionVertex triangle[] = {
			{ -0.15f, -0.15f, 0.5f, 0xff4080c0, 0, 0 },
			{ 0, 0.15f, 0.5f, 0xff4080c0, 0, 0 },
			{ 0.15f, -0.15f, 0.5f, 0xff4080c0, 0, 0 },
		};
		Projection transform;
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.begin_frame(0xff182430) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.draw_triangles(quad, 6, transform, p_texture) == OK);
		// Exercise both a second texture and untextured drawing in the same frame.
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.draw_triangles(triangle, 3, transform, p_overlay_texture) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.end_frame(false) == OK);
		const uint32_t width = p_device.get_width();
		const uint32_t height = p_device.get_height();
		r_pixels.resize(size_t(width) * height);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(_color_matches(r_pixels[size_t(2) * width + 2], 0xff182430));
		FIXED_FUNCTION_TEXTURE_CHECK(_color_matches(r_pixels[size_t(height / 4) * width + width / 4], 0xff000000 | (p_modulation & 0xff0000)));
		// Close enough to the texel boundary that linear filtering would mix red and green.
		FIXED_FUNCTION_TEXTURE_CHECK(_color_matches(r_pixels[size_t(height / 4) * width + width * 7 / 16], 0xff000000 | (p_modulation & 0xff0000)));
		FIXED_FUNCTION_TEXTURE_CHECK(_color_matches(r_pixels[size_t(height / 4) * width + width * 3 / 4], 0xff000000 | (p_modulation & 0xff00)));
		FIXED_FUNCTION_TEXTURE_CHECK(_color_matches(r_pixels[size_t(height * 3 / 4) * width + width / 4], 0xff000000 | (p_modulation & 0xff)));
		FIXED_FUNCTION_TEXTURE_CHECK(_color_matches(r_pixels[size_t(height * 3 / 4) * width + width * 3 / 4], p_modulation));
		FIXED_FUNCTION_TEXTURE_CHECK(_color_matches(r_pixels[size_t(height / 2) * width + width / 2], p_overlay_texture.is_valid() ? 0xff4000c0 : 0xff4080c0));
		return true;
	}

public:
	static bool test(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
		const uint32_t original_width = p_device.get_width();
		const uint32_t original_height = p_device.get_height();
		RID texture;
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_create(Ref<Image>(), texture) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_free(RID()) == ERR_INVALID_PARAMETER);
		Ref<Image> image = Image::create_empty(2, 2, false, Image::FORMAT_RGB8);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_create(image, texture) == ERR_UNAVAILABLE);
		image = _make_image();
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_create(image, texture) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(texture.is_valid());
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_create(image, texture) == ERR_INVALID_PARAMETER);

		int foreign_data = 0;
		RID_PtrOwner<int> foreign_owner;
		RID foreign = foreign_owner.make_rid(&foreign_data);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_free(foreign) == ERR_INVALID_PARAMETER);
		Projection transform;
		const FixedFunctionVertex triangle[] = {
			{ -0.5f, -0.5f, 0.5f, 0xffffffff, 0, 0 },
			{ 0, 0.5f, 0.5f, 0xffffffff, 0, 0 },
			{ 0.5f, -0.5f, 0.5f, 0xffffffff, 0, 0 },
		};
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.begin_frame(0) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.draw_triangles(triangle, 3, transform, foreign) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_free(texture) == ERR_BUSY);
		RID during_frame;
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_create(image, during_frame) == ERR_BUSY);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.end_frame(false) == OK);
		foreign_owner.free(foreign);

		// The upload owns a snapshot, including when D3D7 later recreates the device.
		image->set_pixel(0, 0, Color(1, 0, 1));
		image.unref();
		FIXED_FUNCTION_TEXTURE_CHECK(_render_and_check(p_device, texture, 0xffffffff, false, r_pixels));
		FIXED_FUNCTION_TEXTURE_CHECK(_render_and_check(p_device, texture, 0xff808080, false, r_pixels));
		FIXED_FUNCTION_TEXTURE_CHECK(_render_and_check(p_device, texture, 0xffffffff, true, r_pixels));
		Ref<Image> overlay_image = _make_image();
		overlay_image->fill(Color(1, 0, 1));
		RID overlay;
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_create(overlay_image, overlay) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(_render_and_check(p_device, texture, 0xffffffff, false, r_pixels, overlay));
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_free(overlay) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.resize(original_width + 64, original_height + 48) == OK);
		RID pending;
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_create(Ref<Image>(), pending) == ERR_BUSY);
		FIXED_FUNCTION_TEXTURE_CHECK(_render_and_check(p_device, texture, 0xffffffff, false, r_pixels));
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.set_use_vsync(true) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(_render_and_check(p_device, texture, 0xffffffff, false, r_pixels));
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.resize(0, 0) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.begin_frame(0) == ERR_SKIP);
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.resize(original_width, original_height) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(_render_and_check(p_device, texture, 0xffffffff, false, r_pixels));
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_free(texture) == OK);
		texture = RID();
		FIXED_FUNCTION_TEXTURE_CHECK(p_device.texture_create(_make_image(), texture) == OK);
		FIXED_FUNCTION_TEXTURE_CHECK(_render_and_check(p_device, texture, 0xffffffff, false, r_pixels));
		// Leave this texture owned by the device to exercise automatic finalize cleanup.
		printf("PASS: texture upload/snapshot, UVs, clamp, modulation, mixed draws, ownership, recovery, and release.\n");
		return true;
	}
};

#undef FIXED_FUNCTION_TEXTURE_CHECK
