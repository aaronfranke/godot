#include "test_graphics_device_d3d9.h"

#include <stdio.h>
#include <string.h>

#define FF_SHADOW_CHECK(m_condition)                                        \
	do {                                                                    \
		if (!(m_condition)) {                                               \
			printf("D3D9 shadow FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                   \
		}                                                                   \
	} while (false)

bool TestFixedFunctionGraphicsDeviceD3D9::_test_directional_shadows(HWND p_window) {
	FixedFunctionGraphicsDeviceD3D9 device;
	FF_SHADOW_CHECK(device.initialize(p_window, 128, 96, false) == OK);
	if (!device.supports_directional_shadows()) {
		printf("PASS: optional D3D9 directional shadows unavailable; unshadowed profile retained.\n");
		return device.finalize() == OK;
	}
	FixedFunctionVertex caster[3] = {
		{ -0.8f, -0.8f, -0.5f, 0xffffffff, 0, 0 },
		{ 0.8f, -0.8f, -0.5f, 0xffffffff, 1, 0 },
		{ 0, 0.8f, -0.5f, 0xffffffff, 0.5f, 1 },
	};
	FixedFunctionLightingVertex receiver[3];
	for (int i = 0; i < 3; i++) {
		const FixedFunctionLightingVertex vertex = { caster[i].x, caster[i].y, 0.5f, 0, 0, 1, caster[i].u, caster[i].v, 1, 1, 1, 1 };
		receiver[i] = vertex;
	}
	FixedFunctionGraphicsDevice::FFPixelLighting lighting;
	lighting.camera_basis = Basis();
	lighting.ambient = Color(0.2f, 0.2f, 0.2f);
	lighting.light_count = 1;
	memset(lighting.lights, 0, sizeof(lighting.lights));
	for (int channel = 0; channel < 3; channel++) {
		lighting.lights[0].color_type[channel] = 0.6f;
	}
	lighting.lights[0].direction_attenuation[2] = 1;
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.depth_test_enabled = state.depth_write_enabled = true;
	RID map;
	FF_SHADOW_CHECK(device.texture_create_render_target(64, 64, map) == OK);
	Ref<Image> transparent = memnew(Image(2, 2, false, Image::FORMAT_RGBA8));
	transparent->fill(Color(1, 1, 1, 0));
	RID alpha_texture;
	FF_SHADOW_CHECK(device.texture_create(transparent, alpha_texture) == OK);
	std::vector<uint32_t> map_pixels(64 * 64), pixels;
	FF_SHADOW_CHECK(device.begin_frame(0xff101820, RID(), true, true) == OK);
	FF_SHADOW_CHECK(device.draw_shadow_triangles(caster, 3, CameraMatrix(), RID(), state) == ERR_INVALID_PARAMETER);
	FF_SHADOW_CHECK(device.end_frame(false) == OK);
	for (int stage = 0; stage < 7; stage++) {
		if (stage == 4) {
			device._shadow_shaders.unavailable = true;
			FF_SHADOW_CHECK(!device.supports_directional_shadows());
		} else if (stage == 5) {
			device._shadow_shaders.unavailable = false;
			FF_SHADOW_CHECK(device.resize(160, 112) == OK && !device.texture_is_render_target_ready(map));
		}
		if (stage <= 3 || stage == 6) {
			FF_SHADOW_CHECK(device.begin_frame(0xffffffff, map, true, true) == OK);
			FixedFunctionGraphicsDevice::FFDrawParameters coverage = state;
			coverage.alpha_cutoff_enabled = stage == 2 || stage == 3;
			coverage.texture_alpha_enabled = stage != 3;
			FF_SHADOW_CHECK(device.draw_shadow_triangles(caster, 3, CameraMatrix(), coverage.alpha_cutoff_enabled ? alpha_texture : RID(), coverage) == OK);
			FF_SHADOW_CHECK(device.end_frame(false) == OK && device.texture_is_render_target_ready(map));
			FF_SHADOW_CHECK(device.texture_read_render_target(map, map_pixels.data(), map_pixels.size()) == OK);
			if (stage == 1) {
				const uint32_t pixel = map_pixels[32 * 64 + 32];
				const double depth = double((pixel >> 16) & 255) / 255 + double((pixel >> 8) & 255) / 65025 + double(pixel & 255) / 16581375;
				FF_SHADOW_CHECK(depth > 0.24999 && depth < 0.25001);
			}
		}
		lighting.shadow.texture = stage == 0 ? RID() : map;
		lighting.shadow.light_index = 0;
		FF_SHADOW_CHECK(device.begin_frame(0xff101820, RID(), true, true) == OK);
		FF_SHADOW_CHECK(device.draw_lit_triangles(receiver, 3, CameraMatrix(), lighting, RID(), state) == OK);
		FF_SHADOW_CHECK(device.end_frame(false) == OK);
		pixels.resize(device.get_width() * device.get_height());
		FF_SHADOW_CHECK(device.read_frame(pixels.data(), pixels.size()) == OK);
		const uint32_t sample = pixels[(device.get_height() / 2) * device.get_width() + device.get_width() / 2];
		const int red = (sample >> 16) & 255;
		const bool shadowed = stage == 1 || stage == 3 || stage == 6;
		FF_SHADOW_CHECK(shadowed ? red >= 49 && red <= 53 : red >= 201 && red <= 207);
	}
	// A sun in the final active slot shares the budget with omni/spot lights.
	// Its shadow must not suppress those local lights or index the wrong constant block.
	lighting.light_count = FixedFunctionGraphicsDevice::MAX_PIXEL_LIGHTS;
	for (uint32_t index = 0; index < lighting.light_count; index++) {
		FixedFunctionGraphicsDevice::FFPixelLight &light = lighting.lights[index];
		memset(&light, 0, sizeof(light));
		for (int channel = 0; channel < 3; channel++) {
			light.color_type[channel] = index == lighting.light_count - 1 ? 0.6f : 0.02f;
		}
		if (index == lighting.light_count - 1) {
			light.direction_attenuation[2] = 1;
		} else {
			light.color_type[3] = index == 3 ? 2.0f : 1.0f;
			light.position_range[2] = 2;
			light.position_range[3] = 0.05f;
			light.direction_attenuation[2] = -1;
			light.direction_attenuation[3] = 1;
			light.cone[0] = 0.8f;
			light.cone[1] = 1;
		}
	}
	for (int variant = 0; variant < 3; variant++) {
		lighting.shadow.texture = variant == 1 ? RID() : map;
		lighting.shadow.light_index = variant == 2 ? 3 : lighting.light_count - 1;
		FF_SHADOW_CHECK(device.begin_frame(0xff101820, RID(), true, true) == OK);
		FF_SHADOW_CHECK(device.draw_lit_triangles(receiver, 3, CameraMatrix(), lighting, RID(), state) == OK);
		FF_SHADOW_CHECK(device.end_frame(false) == OK);
		FF_SHADOW_CHECK(device.read_frame(pixels.data(), pixels.size()) == OK);
		const uint32_t sample = pixels[(device.get_height() / 2) * device.get_width() + device.get_width() / 2];
		const int red = (sample >> 16) & 255;
		FF_SHADOW_CHECK(variant == 0 ? red >= 81 && red <= 87 : red >= 234 && red <= 240);
	}
	FF_SHADOW_CHECK(device.texture_free(alpha_texture) == OK && device.texture_free(map) == OK);
	FF_SHADOW_CHECK(device.finalize() == OK);
	printf("PASS: RGB24 depth, clip convention, coverage alpha, ambient/local-light separation, eight active slots, optional refusal, reset invalidation and shadow redraw.\n");
	return true;
}

#undef FF_SHADOW_CHECK
