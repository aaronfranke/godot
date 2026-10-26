#include "drivers/fixed_function/tests/test_blending.h"

#include <stdio.h>

#define FF_MASK_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Texture mask FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionBlending::_texture_masks(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
	if (!p_device.supports_texture_mask()) {
		FF_MASK_CHECK(p_device.draw_masked_triangles(nullptr, 0, Projection(), RID(), RID(), 0, FFDrawParameters()) == ERR_UNAVAILABLE);
		return true;
	}
	Ref<Image> image = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	image->fill(Color(0, 0, 0, 0));
	image->set_pixel(1, 1, Color(0, 1, 0, 0));
	RID color, mask;
	FF_MASK_CHECK(p_device.texture_create(image, color) == OK);
	image->fill(Color(1, 0, 0, 0));
	image->set_pixel(0, 0, Color(1, 0, 0, 0.5f));
	FF_MASK_CHECK(p_device.texture_create(image, mask) == OK);
	FixedFunctionMaskedVertex vertices[] = {
		{ { -1, 1, 0.5f, 0x80ffffff, 0.75f, 0.75f }, 0.49f, 0.49f },
		{ { 1, 1, 0.5f, 0x80ffffff, 0.75f, 0.75f }, 0.49f, 0.49f },
		{ { 1, -1, 0.5f, 0x80ffffff, 0.75f, 0.75f }, 0.49f, 0.49f },
		{ { -1, 1, 0.5f, 0x80ffffff, 0.75f, 0.75f }, 0.49f, 0.49f },
		{ { 1, -1, 0.5f, 0x80ffffff, 0.75f, 0.75f }, 0.49f, 0.49f },
		{ { -1, -1, 0.5f, 0x80ffffff, 0.75f, 0.75f }, 0.49f, 0.49f },
	};
	const uint32_t width = p_device.get_width();
	const uint32_t height = p_device.get_height();
	r_pixels.resize(size_t(width) * height);
	FFDrawParameters state;
	state.blend_mode = Device::BLEND_MODE_ADD;
	state.sampler_flags = p_device.get_capabilities().sampler_filter_linear ? Device::SAMPLER_FILTER_LINEAR : 0;
	for (int reset = 0; reset < 2; reset++) {
		uint8_t mask_flags = 0;
		if (reset) {
			FF_MASK_CHECK(p_device.resize(width + 1, height + 1) == OK && p_device.prepare_resources() == OK && p_device.resize(width, height) == OK);
			if (p_device.get_capabilities().sampler_repeat) {
				mask_flags = Device::SAMPLER_REPEAT_U | Device::SAMPLER_REPEAT_V;
				for (int i = 0; i < 6; i++) {
					vertices[i].mask_u += 1;
					vertices[i].mask_v += 1;
				}
			}
		}
		FF_MASK_CHECK(p_device.begin_frame(0xff0000ff) == OK);
		FF_MASK_CHECK(p_device.draw_masked_triangles(vertices, 6, Projection(), color, mask, mask_flags, state) == OK);
		FF_MASK_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FF_MASK_CHECK(_color_matches(r_pixels[size_t(height / 2) * width + width / 2], 0xff0040ff));
		FF_MASK_CHECK(p_device.begin_frame(0xff0000ff) == OK);
		FF_MASK_CHECK(p_device.draw_masked_triangles(vertices, 5, Projection(), color, mask, 0, state) == ERR_INVALID_PARAMETER);
		FF_MASK_CHECK(p_device.draw_masked_triangles(vertices, 6, Projection(), color, RID(), 0, state) == ERR_INVALID_PARAMETER);
		FF_MASK_CHECK(p_device.draw_masked_triangles(vertices, 6, Projection(), color, mask, 0xff, state) == ERR_INVALID_PARAMETER);
		FF_MASK_CHECK(_draw_rectangle(p_device, 0, 0, width, height, 0xffff0000) == OK);
		FF_MASK_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FF_MASK_CHECK(_color_matches(r_pixels[size_t(height / 2) * width + width / 2], 0xffff0000));
	}
	if (p_device.get_capabilities().render_target_opaque) {
		RID target;
		FF_MASK_CHECK(p_device.texture_create_render_target(8, 8, target) == OK);
		FF_MASK_CHECK(p_device.begin_frame(0) == OK);
		FF_MASK_CHECK(p_device.draw_masked_triangles(vertices, 6, Projection(), color, target, 0, state) == ERR_UNCONFIGURED);
		FF_MASK_CHECK(p_device.end_frame(false) == OK && p_device.begin_frame(0, target) == OK);
		FF_MASK_CHECK(p_device.draw_masked_triangles(vertices, 6, Projection(), color, target, 0, state) == ERR_INVALID_PARAMETER);
		FF_MASK_CHECK(p_device.end_frame(false) == OK && p_device.texture_free(target) == OK);
	}
	FF_MASK_CHECK(p_device.texture_free(color) == OK && p_device.texture_free(mask) == OK);
	printf("PASS: independent texture RGB/alpha, separate UVs, ignored emission alpha, state restoration and reset.\n");
	return true;
}

#undef FF_MASK_CHECK
