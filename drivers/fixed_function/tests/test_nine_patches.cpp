#include "test_nine_patches.h"

#include "core/math/math_funcs.h"
#include "servers/rendering/rendering_server_globals.h"

#include <stdio.h>

#include <limits>

#define FF_PATCH_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("NinePatch FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

TestFixedFunctionNinePatches::FFPatchFixture::FFPatchFixture() : server(*RSG::canvas) {
	item = server.canvas_item_allocate();
	server.canvas_item_initialize(item);
}
TestFixedFunctionNinePatches::FFPatchFixture::~FFPatchFixture() {
	server.free(item);
}

uint32_t TestFixedFunctionNinePatches::_texel(int p_x, int p_y) {
	return 0xff000000 | ((32 + p_x * 24) << 16) | ((16 + p_y * 28) << 8) | ((p_x ^ p_y) * 31);
}

Ref<Image> TestFixedFunctionNinePatches::_make_image() {
	Vector<uint8_t> data;
	data.resize(8 * 8 * 4);
	{
		uint8_t *write = data.ptrw();
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				const uint32_t color = _texel(x, y);
				const int offset = (y * 8 + x) * 4;
				write[offset] = (color >> 16) & 255;
				write[offset + 1] = (color >> 8) & 255;
				write[offset + 2] = color & 255;
				write[offset + 3] = 255;
			}
		}
	}
	return Image::create_from_data(8, 8, false, Image::FORMAT_RGBA8, data);
}

Color TestFixedFunctionNinePatches::_sample(real_t p_u, real_t p_v, bool p_filtered) {
	if (!p_filtered) {
		return Color::hex(_texel(int(Math::floor(p_u)), int(Math::floor(p_v))) << 8 | 255);
	}
	const int x = int(Math::floor(p_u - real_t(0.5)));
	const int y = int(Math::floor(p_v - real_t(0.5)));
	const real_t fx = p_u - real_t(0.5) - x;
	const real_t fy = p_v - real_t(0.5) - y;
	const Color a = Color::hex(_texel(CLAMP(x, 0, 7), CLAMP(y, 0, 7)) << 8 | 255);
	const Color b = Color::hex(_texel(CLAMP(x + 1, 0, 7), CLAMP(y, 0, 7)) << 8 | 255);
	const Color c = Color::hex(_texel(CLAMP(x, 0, 7), CLAMP(y + 1, 0, 7)) << 8 | 255);
	const Color d = Color::hex(_texel(CLAMP(x + 1, 0, 7), CLAMP(y + 1, 0, 7)) << 8 | 255);
	return a.lerp(b, fx).lerp(c.lerp(d, fx), fy);
}

real_t TestFixedFunctionNinePatches::_reference_axis(real_t p_pixel, real_t p_destination, real_t p_source, real_t p_scale, RSE::NinePatchAxisMode p_mode) {
	// Per-pixel GLES3 shader mapping, independent of the triangle expansion under test.
	if (p_pixel < p_scale) {
		return p_pixel / p_scale;
	}
	if (p_pixel >= p_destination - p_scale) {
		return p_source - (p_destination - p_pixel) / p_scale;
	}
	const real_t offset = p_pixel - p_scale;
	const real_t destination = p_destination - 2 * p_scale;
	const real_t source = p_source - 2;
	if (p_mode == RSE::NINE_PATCH_STRETCH) {
		return 1 + offset / destination * source;
	}
	if (p_mode == RSE::NINE_PATCH_TILE) {
		return 1 + Math::fmod(offset, source);
	}
	const real_t repeats = MAX(real_t(1), Math::floor(destination / MAX(source, real_t(0.0000001)) + real_t(0.5)));
	const real_t phase = offset / destination * repeats;
	return 1 + (phase - Math::floor(phase)) * source;
}

bool TestFixedFunctionNinePatches::_test_mapping(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer, RID p_texture, bool p_mirrored, bool p_filtered, std::vector<uint32_t> &r_pixels) {
	FFPatchFixture fixture;
	FFPatchCase cases[18];
	for (int i = 0; i < 18; i++) {
		FFPatchCase &patch = cases[i];
		const int mode = i % 9;
		patch.horizontal = RSE::NinePatchAxisMode(mode % 3);
		patch.vertical = RSE::NinePatchAxisMode(mode / 3);
		patch.center = (mode & 1) == 0;
		patch.source = Rect2(1, 1, 6, 6);
		patch.rect = i < 9 ? Rect2(2 + (mode % 3) * 40, 2 + (mode / 3) * 28, 29, 23) : Rect2(2 + mode * 8, 86, 4, 5);
		fixture.server.canvas_item_add_nine_patch(fixture.item, patch.rect, patch.source, p_texture, Vector2(1, 1), Vector2(1, 1), patch.horizontal, patch.vertical, patch.center, Color(1, 1, 1));
	}
	RendererCanvasRender::Item *item = fixture.server.canvas_item_owner.get_or_null(fixture.item);
	if (p_mirrored) {
		item->final_transform[0] = Vector2(-1, 0);
		item->final_transform.set_origin(Vector2(128, 0));
		item->final_clip_owner = item;
		item->final_clip_rect = Rect2(8, 4, 108, 80);
	}
	const Color modulate(0.75, 1, 0.5, 0.75);
	const Color background(24 / 255.0f, 36 / 255.0f, 48 / 255.0f);
	FF_PATCH_CHECK(p_manager.begin_frame(0) == OK && p_manager.clear_render_target(background) == OK);
	FF_PATCH_CHECK(p_renderer.render_items(item, modulate, p_filtered ? RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR : RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST) == OK);
	FF_PATCH_CHECK(p_manager.end_frame(false) == OK);
	r_pixels.resize(128 * 96);
	FF_PATCH_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	int checked = 0;
	for (int i = 0; i < 18; i++) {
		const FFPatchCase &patch = cases[i];
		const real_t scale = real_t(1);
		for (int y = 0; y < int(patch.rect.size.y); y++) {
			for (int x = 0; x < int(patch.rect.size.x); x++) {
				const int px = p_mirrored ? 127 - int(patch.rect.position.x) - x : int(patch.rect.position.x) + x;
				const int py = int(patch.rect.position.y) + y;
				const real_t local_x = x + real_t(0.5);
				const real_t local_y = y + real_t(0.5);
				const bool center = local_x >= scale && local_x < patch.rect.size.x - scale && local_y >= scale && local_y < patch.rect.size.y - scale;
				const bool clipped = p_mirrored && (px < 8 || px >= 116 || py < 4 || py >= 84);
				Color expected = background;
				if (!clipped && (patch.center || !center)) {
					const real_t u = 1 + _reference_axis(local_x, patch.rect.size.x, 6, scale, patch.horizontal);
					const real_t v = 1 + _reference_axis(local_y, patch.rect.size.y, 6, scale, patch.vertical);
					// Native interpolation can choose either neighbor at an exact nearest-sampling tie.
					if (!p_filtered && (Math::abs(u - Math::round(u)) < 0.0001 || Math::abs(v - Math::round(v)) < 0.0001)) {
						continue;
					}
					const Color source = _sample(u, v, p_filtered) * modulate;
					expected = Color(source.r * source.a + background.r * (1 - source.a), source.g * source.a + background.g * (1 - source.a), source.b * source.a + background.b * (1 - source.a));
				}
				const uint32_t actual = r_pixels[py * 128 + px];
				const uint32_t wanted = expected.to_argb32();
				for (int shift = 0; shift < 24; shift += 8) {
					const int delta = int((actual >> shift) & 255) - int((wanted >> shift) & 255);
					if (delta < -8 || delta > 8 || (actual >> 24) != 255) {
						printf("NinePatch case=%d pixel=(%d,%d) mirrored=%d filtered=%d actual=%08x expected=%08x\n", i, px, py, p_mirrored, p_filtered, actual, wanted);
						return false;
					}
				}
				checked++;
			}
		}
	}
	FF_PATCH_CHECK(checked > 1000);
	return true;
}

bool TestFixedFunctionNinePatches::_test_limits(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer, RID p_texture, std::vector<uint32_t> &r_pixels) {
	FFPatchFixture fixture;
	fixture.server.canvas_item_add_nine_patch(fixture.item, Rect2(0, 0, 256, 256), Rect2(0, 0, 1, 1), p_texture, Vector2(), Vector2(), RSE::NINE_PATCH_TILE, RSE::NINE_PATCH_TILE, true, Color(1, 1, 1));
	RendererCanvasRender::Item *item = fixture.server.canvas_item_owner.get_or_null(fixture.item);
	FF_PATCH_CHECK(p_manager.begin_frame(0) == OK);
	FF_PATCH_CHECK(p_renderer.render_items(item, Color(1, 1, 1), RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST) == OK); // Exactly 65,536 quads.
	FF_PATCH_CHECK(p_manager.end_frame(false) == OK);
	r_pixels.resize(128 * 96);
	FF_PATCH_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	const uint32_t actual = r_pixels[20 * 128 + 20];
	const uint32_t expected_color = _texel(0, 0);
	for (int shift = 0; shift < 24; shift += 8) {
		const int delta = int((actual >> shift) & 255) - int((expected_color >> shift) & 255);
		FF_PATCH_CHECK(delta >= -8 && delta <= 8); // Permit legacy texture/target quantization.
	}
	FF_PATCH_CHECK((actual >> 24) == 255);
	RendererCanvasRender::Item::CommandNinePatch *patch = static_cast<RendererCanvasRender::Item::CommandNinePatch *>(item->commands);
	for (int invalid = 0; invalid < 8; invalid++) {
		patch->rect = invalid == 0 ? Rect2(0, 0, 65537, 1) : Rect2(0, 0, 257, 256);
		patch->source = Rect2(0, 0, 1, 1);
		patch->margin[SIDE_LEFT] = 0;
		patch->margin[SIDE_RIGHT] = 0;
		patch->axis_x = RSE::NINE_PATCH_TILE;
		if (invalid == 2) {
			patch->margin[SIDE_LEFT] = 0.5f;
			patch->margin[SIDE_RIGHT] = 0.5f; // No repeatable source interior.
		} else if (invalid == 3) {
			patch->rect.position.x = std::numeric_limits<real_t>::quiet_NaN();
		} else if (invalid == 4) {
			patch->rect.size.y = std::numeric_limits<real_t>::infinity();
		} else if (invalid == 5) {
			patch->axis_x = RSE::NinePatchAxisMode(100);
		} else if (invalid == 6) {
			patch->margin[SIDE_LEFT] = -1;
		} else if (invalid == 7) {
			patch->source.size.x = std::numeric_limits<real_t>::quiet_NaN();
		}
		FF_PATCH_CHECK(p_manager.begin_frame(0) == OK && p_manager.begin_draw() == OK);
		const Error expected = invalid < 2 ? ERR_UNAVAILABLE : ERR_INVALID_PARAMETER;
		const Error actual_error = p_renderer.render_items(item, Color(1, 1, 1), RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
		if (actual_error != expected) {
			printf("NinePatch invalid case=%d actual=%d expected=%d\n", invalid, actual_error, expected);
		}
		FF_PATCH_CHECK(actual_error == expected);
		FF_PATCH_CHECK(p_renderer.get_render_info().info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] == 0);
		FF_PATCH_CHECK(p_manager.end_frame(false) == expected);
		FF_PATCH_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == ERR_UNCONFIGURED);
	}
	// A huge but unused horizontal middle must not consume the command's work budget.
	patch->rect = Rect2(0, 0, 1000000, 4);
	patch->source = Rect2(0, 0, 3, 3);
	patch->margin[SIDE_LEFT] = 1;
	patch->margin[SIDE_RIGHT] = 1;
	patch->axis_x = RSE::NINE_PATCH_TILE;
	patch->draw_center = false;
	FF_PATCH_CHECK(p_manager.begin_frame(0) == OK && p_renderer.render_items(item, Color(1, 1, 1), RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST) == OK && p_manager.end_frame(false) == OK);
	// Match the GLES3 TILE_FIT divisor floor for a positive sub-texel source region.
	patch->rect = Rect2(0, 0, 0.001, 1);
	patch->source = Rect2(0, 0, 0.00000001, 1);
	patch->margin[SIDE_LEFT] = 0;
	patch->margin[SIDE_RIGHT] = 0;
	patch->axis_x = RSE::NINE_PATCH_TILE_FIT;
	patch->axis_y = RSE::NINE_PATCH_STRETCH;
	patch->draw_center = true;
	FF_PATCH_CHECK(p_manager.begin_frame(0) == OK && p_renderer.render_items(item, Color(1, 1, 1), RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST) == OK && p_manager.end_frame(false) == OK);
	return true;
}

bool TestFixedFunctionNinePatches::test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer, std::vector<uint32_t> &r_pixels) {
	const RID texture = p_context.get_texture_storage().texture_create();
	FF_PATCH_CHECK(p_context.get_texture_storage().texture_set_data(texture, _make_image()) == OK);
	bool success = true;
	const int filter_modes = p_context.get_device().get_capabilities().sampler_filter_linear ? 2 : 1;
	for (int filtered = 0; filtered < filter_modes && success; filtered++) {
		success = p_context.get_texture_storage().texture_set_sampler_flags(texture, filtered ? FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR : 0) == OK;
		for (int mirrored = 0; mirrored < 2 && success; mirrored++) {
			success = _test_mapping(p_context, p_manager, p_renderer, texture, mirrored != 0, filtered != 0, r_pixels);
		}
	}
	success = success && p_context.get_texture_storage().texture_set_sampler_flags(texture, 0) == OK;
	if (success) {
		success = _test_limits(p_context, p_manager, p_renderer, texture, r_pixels);
	}
	FF_PATCH_CHECK(success);
	FF_PATCH_CHECK(p_context.get_texture_storage().free(texture) == OK);
	printf("PASS: nine-patch stretch/tile/tile-fit pixels, source regions, shrinking, center omission, modulation, reflection/clipping, work limit and rejection before draw.\n");
	return true;
}

#undef FF_PATCH_CHECK
