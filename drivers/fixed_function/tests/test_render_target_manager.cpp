#include "test_render_target_manager.h"

#include "drivers/fixed_function/canvas_item_renderer.h"

#include <stdio.h>

#include <limits>

#define FIXED_FUNCTION_PASS_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Pass FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

Error TestFixedFunctionRenderTargetManager::_draw_color(FixedFunctionGraphicsDevice &p_device, uint32_t p_color, const Rect2i &p_clip) {
	const FixedFunctionVertex vertices[] = {
		{ -1, 1, 0, p_color, 0, 0 },
		{ 1, 1, 0, p_color, 1, 0 },
		{ 1, -1, 0, p_color, 1, 1 },
		{ -1, 1, 0, p_color, 0, 0 },
		{ 1, -1, 0, p_color, 1, 1 },
		{ -1, -1, 0, p_color, 0, 1 },
	};
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.clip_enabled = true;
	state.clip_rect = p_clip;
	return p_device.draw_triangles(vertices, 6, Projection(), RID(), state);
}

bool TestFixedFunctionRenderTargetManager::_matches(const std::vector<uint32_t> &p_pixels, uint32_t p_expected, int p_x, int p_y) {
	const uint32_t actual = p_pixels[size_t(p_y) * CAPTURE_WIDTH + p_x];
	for (int shift = 0; shift < 24; shift += 8) {
		const int delta = int((actual >> shift) & 255) - int((p_expected >> shift) & 255);
		if (delta < -8 || delta > 8 || (actual >> 24) != 255) {
			printf("Pass pixel (%d,%d) actual=%08x expected=%08x\n", p_x, p_y, actual, p_expected);
			return false;
		}
	}
	return true;
}

bool TestFixedFunctionRenderTargetManager::_compose(FixedFunctionRenderTargetManager &p_manager, FixedFunctionGraphicsDevice &p_device, RID p_left, RID p_right, std::vector<uint32_t> &r_pixels) {
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_frame(0.25) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(p_left) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.clear_render_target(Color(1, 0, 0)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_draw() == OK);
	FIXED_FUNCTION_PASS_CHECK(_draw_color(p_device, 0xff0000ff, Rect2i(0, 16, 32, 16)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.blit_to_screen(p_left, Rect2(0, 0, 64, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(p_right) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.clear_render_target(Color(0, 1, 0)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_draw() == OK);
	FIXED_FUNCTION_PASS_CHECK(_draw_color(p_device, 0xffff00ff, Rect2i(4, 4, 8, 8)) == OK);
	// Clear must restore the full viewport rather than preserve the last clipped draw.
	FIXED_FUNCTION_PASS_CHECK(p_manager.clear_render_target(Color(0, 1, 0)) == OK);
	FIXED_FUNCTION_PASS_CHECK(_draw_color(p_device, 0xffffffff, Rect2i(0, 16, 32, 16)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.blit_to_screen(p_right, Rect2(64, 0, 64, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_PASS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			uint32_t expected;
			if (y < 48) {
				expected = x < 64 ? 0xffff0000 : 0xff00ff00;
			} else {
				expected = x < 64 ? 0xff0000ff : 0xffffffff;
			}
			FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, expected, x, y));
		}
	}
	return true;
}

bool TestFixedFunctionRenderTargetManager::_test_failed_viewports(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RID p_bad, RID p_good, std::vector<uint32_t> &r_pixels) {
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	FixedFunctionGraphicsDevice &device = p_context.get_device();
	RID native;
	FIXED_FUNCTION_PASS_CHECK(storage.texture_get_device_texture(storage.render_target_get_texture(p_bad), native) == OK);
	const RID proxy = storage.texture_create();
	FIXED_FUNCTION_PASS_CHECK(storage.texture_set_proxy(proxy, storage.render_target_get_texture(p_bad)) == OK);
	RendererCanvasRender::Item item;
	RendererCanvasRender::Item::CommandRect *rect = item.alloc_command<RendererCanvasRender::Item::CommandRect>();
	rect->texture = proxy;
	rect->rect = Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT);
	rect->modulate = Color(1, 1, 1);
	FixedFunctionCanvasItemRenderer canvas(p_context, p_manager);
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.clear_render_target(Color(24 / 255.0f, 36 / 255.0f, 48 / 255.0f)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_draw() == OK);
	FIXED_FUNCTION_PASS_CHECK(_draw_color(device, 0xffff0000, Rect2i(0, 0, 16, 16)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(p_bad) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.clear_render_target(Color(1, 0, 1)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_draw() == OK);
	FIXED_FUNCTION_PASS_CHECK(_draw_color(device, 0xff00ffff, Rect2i(0, 0, 32, 32)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.report_draw_error(ERR_UNAVAILABLE) == ERR_UNAVAILABLE);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_has_failed(p_bad) && !storage.render_target_is_ready(p_bad));
	FIXED_FUNCTION_PASS_CHECK(!storage.render_target_needs_redraw(p_bad));
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_draw() == ERR_UNAVAILABLE);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(p_good) == OK);
	FIXED_FUNCTION_PASS_CHECK(!device.texture_is_render_target_ready(native));
	FIXED_FUNCTION_PASS_CHECK(p_manager.clear_render_target(Color(0, 1, 0)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.blit_to_screen(p_bad, Rect2(0, 0, 64, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(canvas.render_items(&item, Color(1, 1, 1)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.blit_to_screen(p_good, Rect2(64, 0, 64, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_PASS_CHECK(device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xffff0000, 8, 8));
	FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xff182430, 32, 32));
	FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xff00ff00, 96, 48));
	// Failure survives the frame boundary, including sampling through a texture proxy.
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.clear_render_target(Color(24 / 255.0f, 36 / 255.0f, 48 / 255.0f)) == OK);
	FIXED_FUNCTION_PASS_CHECK(canvas.render_items(&item, Color(1, 1, 1)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.end_frame(false) == OK);
	FIXED_FUNCTION_PASS_CHECK(device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xff182430, 8, 8));
	// A rejected clear-only viewport must not be opened or published while switching away.
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(p_bad) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.clear_render_target(Color(1, 0, 1)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.report_draw_error(ERR_UNAVAILABLE) == ERR_UNAVAILABLE);
	FIXED_FUNCTION_PASS_CHECK(p_manager.end_frame(false) == OK);
	FIXED_FUNCTION_PASS_CHECK(!storage.render_target_is_ready(p_bad));
	// A later supported pass replaces failure state and becomes sampleable again.
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(p_bad) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.clear_render_target(Color(0, 0, 1)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(!storage.render_target_has_failed(p_bad) && storage.render_target_is_ready(p_bad));
	FIXED_FUNCTION_PASS_CHECK(canvas.render_items(&item, Color(1, 1, 1)) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.end_frame(false) == OK);
	FIXED_FUNCTION_PASS_CHECK(device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xff0000ff, 8, 8));
	// Only unsupported offscreen output is isolated; invalid draws still cancel the frame.
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(p_bad) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_draw() == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.report_draw_error(ERR_INVALID_PARAMETER) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(RID()) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PASS_CHECK(p_manager.end_frame(false) == ERR_INVALID_PARAMETER);
	// Native drawing/recovery skips must still invalidate all prior window composition.
	FIXED_FUNCTION_PASS_CHECK(p_manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(p_bad) == OK && p_manager.begin_draw() == OK);
	FIXED_FUNCTION_PASS_CHECK(p_manager.report_draw_error(ERR_SKIP) == ERR_SKIP);
	FIXED_FUNCTION_PASS_CHECK(p_manager.select_render_target(RID()) == ERR_SKIP);
	FIXED_FUNCTION_PASS_CHECK(p_manager.end_frame(false) == ERR_SKIP);
	FIXED_FUNCTION_PASS_CHECK(storage.free(proxy) == OK);
	printf("PASS: unsupported viewport isolation, discarded partial contents, proxy omission across frames and later redraw.\n");
	return true;
}

bool TestFixedFunctionRenderTargetManager::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FixedFunctionRenderTargetManager manager(context);
	FIXED_FUNCTION_PASS_CHECK(manager.begin_draw() == ERR_UNCONFIGURED);
	FIXED_FUNCTION_PASS_CHECK(context.initialize() == OK);
	if (!p_device.get_capabilities().render_target_opaque) {
		r_pixels.clear();
		printf("SKIP: pass composition requires opaque render targets.\n");
		return true;
	}
	FixedFunctionTextureStorage &storage = context.get_texture_storage();
	const RID left = storage.render_target_create();
	const RID right = storage.render_target_create();
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_set_size(left, 32, 32) == OK);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_set_size(right, 32, 32) == OK);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_allocate(left) == OK);
	RID native_left;
	FIXED_FUNCTION_PASS_CHECK(storage.texture_get_device_texture(storage.render_target_get_texture(left), native_left) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_device.clear_color(0) == ERR_UNCONFIGURED);
	FIXED_FUNCTION_PASS_CHECK(p_device.begin_frame(0, RID(), false) == ERR_UNCONFIGURED);
	FIXED_FUNCTION_PASS_CHECK(p_device.begin_frame(0, native_left, false) == ERR_UNCONFIGURED);
	FIXED_FUNCTION_PASS_CHECK(_compose(manager, p_device, left, right, r_pixels));
	const std::vector<uint32_t> capture = r_pixels;
	FIXED_FUNCTION_PASS_CHECK(context.get_total_time() == 0.25 && context.get_frame_number() == 2);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_was_used(left) && storage.render_target_was_used(right));
	if (p_device.get_capabilities().sampler_filter_linear) {
		const RID left_texture = storage.render_target_get_texture(left);
		FIXED_FUNCTION_PASS_CHECK(storage.texture_set_sampler_flags(left_texture, FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR) == OK);
		FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK);
		FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(left, Rect2(0, 0, 128, 96)) == OK);
		FIXED_FUNCTION_PASS_CHECK(manager.end_frame(false) == OK);
		FIXED_FUNCTION_PASS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		// Pixel-center bilinear samples straddle rows 15/16 of the 32-pixel source.
		FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xffaa0055, 64, 47));
		FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xff5500aa, 64, 48));
		FIXED_FUNCTION_PASS_CHECK(storage.texture_set_sampler_flags(left_texture, 0) == OK);
	}
	// Positioned integer scaling preserves the surrounding window and the requested source orientation.
	for (int flip = 0; flip < 2; flip++) {
		FIXED_FUNCTION_PASS_CHECK(storage.render_target_set_flag(left, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP, flip != 0) == OK);
		FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK && manager.clear_render_target(Color(1, 0, 1)) == OK);
		FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(left, Rect2(17, 13, 64, 64)) == OK && manager.end_frame(false) == OK);
		FIXED_FUNCTION_PASS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, flip ? 0xff0000ff : 0xffff0000, 20, 20));
		FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, flip ? 0xffff0000 : 0xff0000ff, 20, 70));
		FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xffff00ff, 16, 13) && _matches(r_pixels, 0xffff00ff, 81, 13));
	}
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_set_flag(left, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP, false) == OK);
	// An empty first viewport with CLEAR_NEVER must settle, rather than retrying ONCE forever.
	const RID empty = storage.render_target_create();
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_set_size(empty, 16, 16) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(empty) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_is_ready(empty) && !storage.render_target_needs_redraw(empty));
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(empty, Rect2(0, 0, 128, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.end_frame(false) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xff000000, x, y));
		}
	}
	FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(empty) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(RID()) == OK);
	// A ready, untouched target needs no native scene, preserving idle window readback.
	FIXED_FUNCTION_PASS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.end_frame(false) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(left) == OK);
	// Reopen without a clear: existing contents must survive (VIEWPORT_CLEAR_NEVER).
	FIXED_FUNCTION_PASS_CHECK(manager.begin_draw() == OK);
	FIXED_FUNCTION_PASS_CHECK(_draw_color(p_device, 0xff00ff00, Rect2i(0, 0, 16, 16)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(left, Rect2(0, 0, 128, 96), 1) == ERR_UNAVAILABLE);
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(left, Rect2(0, 0, -1, 96)) == ERR_INVALID_PARAMETER);
	const real_t nan = std::numeric_limits<real_t>::quiet_NaN();
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(left, Rect2(nan, 0, 128, 96)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PASS_CHECK(manager.clear_render_target(Color(nan, 0, 0)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(left, Rect2(0, 0, 0, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(left, Rect2(0, 0, 128, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.end_frame(false) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xff00ff00, 0, 0));
	FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xffff0000, 127, 0));
	FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xff0000ff, 0, 95));
	// A queued clear must execute even when there are no canvas or scene draws.
	FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(right) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.clear_render_target(Color(4, 2, -1)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_is_ready(right));
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(right, Rect2(0, 0, 128, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.end_frame(false) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xffffff00, x, y));
		}
	}
	FIXED_FUNCTION_PASS_CHECK(p_device.begin_frame(0, RID(), false) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_device.end_frame(true) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_device.begin_frame(0, RID(), false) == ERR_UNCONFIGURED);
	// A reset between screen blits loses earlier composition: skip instead of presenting it.
	FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(left, Rect2(0, 0, 64, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(right) == OK);
	FIXED_FUNCTION_PASS_CHECK(context.resize(160, 120) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.clear_render_target(Color(0, 1, 0)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(right, Rect2(64, 0, 64, 96)) == ERR_SKIP);
	FIXED_FUNCTION_PASS_CHECK(manager.end_frame(true) == ERR_SKIP);
	FIXED_FUNCTION_PASS_CHECK(!context.is_frame_active() && context.get_frame_status() == ERR_UNCONFIGURED);
	FIXED_FUNCTION_PASS_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	// 4.x requests clears through texture storage, including targets with no commands.
	FIXED_FUNCTION_PASS_CHECK(!storage.render_target_is_clear_requested(left));
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_request_clear(RID(), Color(1, 0, 0)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_request_clear(left, Color(nan, 0, 0)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_request_clear(left, Color(4, -1, 0.5)) == OK);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_get_clear_request_color(left) == Color(1, 0, 0.5));
	FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(left) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(!storage.render_target_is_clear_requested(left));
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(left, Rect2(0, 0, 128, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.end_frame(false) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xffff0080, x, y));
		}
	}
	// An active target's clear also ignores clipping left by an earlier draw.
	FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(left) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.begin_draw() == OK);
	FIXED_FUNCTION_PASS_CHECK(_draw_color(p_device, 0xff000000, Rect2i(0, 0, 1, 1)) == OK);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_request_clear(left, Color(0, 1, 1)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.begin_draw() == OK);
	FIXED_FUNCTION_PASS_CHECK(!storage.render_target_is_clear_requested(left));
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.blit_to_screen(left, Rect2(0, 0, 128, 96)) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.end_frame(false) == OK);
	FIXED_FUNCTION_PASS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FIXED_FUNCTION_PASS_CHECK(_matches(r_pixels, 0xff00ffff, 0, 0) && _matches(r_pixels, 0xff00ffff, 127, 95));
	// Suspension must preserve a pending clear for the next drawable frame.
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_request_clear(left, Color(1, 0, 0)) == OK);
	FIXED_FUNCTION_PASS_CHECK(context.resize(0, 0) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(left) == ERR_SKIP);
	FIXED_FUNCTION_PASS_CHECK(manager.end_frame(false) == ERR_SKIP);
	FIXED_FUNCTION_PASS_CHECK(storage.render_target_is_clear_requested(left));
	FIXED_FUNCTION_PASS_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(left) == OK);
	FIXED_FUNCTION_PASS_CHECK(manager.select_render_target(RID()) == OK);
	FIXED_FUNCTION_PASS_CHECK(!storage.render_target_is_clear_requested(left));
	FIXED_FUNCTION_PASS_CHECK(manager.end_frame(false) == OK);
	FIXED_FUNCTION_PASS_CHECK(_test_failed_viewports(context, manager, left, right, r_pixels));
	FIXED_FUNCTION_PASS_CHECK(_compose(manager, p_device, left, right, r_pixels));
	FIXED_FUNCTION_PASS_CHECK(r_pixels == capture);
	printf("PASS: interleaved viewport composition, load preservation, full-target clears, clear-only passes, presentation invalidation, reset cancellation, and recovery.\n");
	return true;
}
