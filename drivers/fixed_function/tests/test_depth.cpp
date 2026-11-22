#include "test_depth.h"

#include <stdio.h>

#include <limits>

#define FF_DEPTH_CHECK(m_condition)                                        \
	do {                                                                   \
		if (!(m_condition)) {                                              \
			printf("Depth FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                  \
		}                                                                  \
	} while (false)

Error TestFixedFunctionDepth::_draw(FixedFunctionGraphicsDevice &p_device, uint32_t p_color, float p_z, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, RID p_texture, const CameraMatrix &p_projection) {
	const FixedFunctionVertex vertices[] = {
		{ -1, 1, p_z, p_color, 0, 0 }, { 1, 1, p_z, p_color, 1, 0 }, { 1, -1, p_z, p_color, 1, 1 }, { -1, 1, p_z, p_color, 0, 0 }, { 1, -1, p_z, p_color, 1, 1 }, { -1, -1, p_z, p_color, 0, 1 }
	};
	return p_device.draw_triangles(vertices, 6, p_projection, p_texture, p_state);
}

bool TestFixedFunctionDepth::_finish(FixedFunctionRenderContext &p_context, std::vector<uint32_t> &r_pixels, uint32_t p_left, uint32_t p_right) {
	FF_DEPTH_CHECK(p_context.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FF_DEPTH_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	const uint32_t expected[] = { p_left, p_right };
	for (int i = 0; i < 2; i++) {
		const uint32_t actual = r_pixels[(CAPTURE_HEIGHT / 2) * CAPTURE_WIDTH + CAPTURE_WIDTH * (i ? 3 : 1) / 4];
		for (int shift = 0; shift < 32; shift += 8) {
			const int delta = int((actual >> shift) & 255) - int((expected[i] >> shift) & 255);
			if (delta < -8 || delta > 8) {
				printf("Depth pixel half=%d actual=%08x expected=%08x\n", i, actual, expected[i]);
				return false;
			}
		}
	}
	return true;
}

bool TestFixedFunctionDepth::_test_targets(FixedFunctionRenderContext &p_context, std::vector<uint32_t> &r_pixels, bool p_preserves_targets_on_resize) {
	FixedFunctionGraphicsDevice &device = p_context.get_device();
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	if (!device.get_capabilities().render_target_opaque) {
		return true;
	}
	const RID a = storage.render_target_create();
	const RID b = storage.render_target_create();
	// D3D7 depth must match scaled native backing, not oversized logical dimensions.
	const bool scaled = device.get_capabilities().render_target_scaled && !device.get_capabilities().texture_square_only && device.get_capabilities().max_texture_width >= 2048;
	FF_DEPTH_CHECK(storage.render_target_set_size(a, scaled ? 4096 : 32, scaled ? 64 : 32) == OK);
	FF_DEPTH_CHECK(storage.render_target_set_size(b, 32, 32) == OK);
	FF_DEPTH_CHECK(storage.render_target_allocate(a) == OK);
	const RID texture_a = storage.render_target_get_texture(a);
	RID native_a;
	FF_DEPTH_CHECK(storage.texture_get_device_texture(texture_a, native_a) == OK);
	FixedFunctionGraphicsDevice::FFTextureInfo color_only;
	FF_DEPTH_CHECK(device.texture_get_info(native_a, color_only) == OK);
	const uint64_t initial_memory = device.get_texture_memory_usage();
	FixedFunctionGraphicsDevice::FFDrawParameters depth;
	depth.depth_test_enabled = depth.depth_write_enabled = true;
	FF_DEPTH_CHECK(p_context.begin_frame(0) == OK);
	FF_DEPTH_CHECK(p_context.begin_render_target(a, 0xff000000, true, true) == OK);
	FF_DEPTH_CHECK(_draw(device, 0xffff0000, -0.5f, depth) == OK && p_context.end_render_target() == OK);
	FixedFunctionGraphicsDevice::FFTextureInfo with_depth;
	FF_DEPTH_CHECK(device.texture_get_info(native_a, with_depth) == OK && with_depth.size == color_only.size && with_depth.bytes > color_only.bytes);
	FF_DEPTH_CHECK(device.get_texture_memory_usage() > initial_memory);
	FF_DEPTH_CHECK(p_context.begin_render_target(b, 0xff000000, true, true) == OK);
	FF_DEPTH_CHECK(_draw(device, 0xff0000ff, 0.5f, depth) == OK && p_context.end_render_target() == OK);
	FF_DEPTH_CHECK(p_context.begin_render_target(a, 0xff000000, false, true) == OK);
	FF_DEPTH_CHECK(_draw(device, 0xff00ff00, 0.5f, depth) == OK && p_context.end_render_target() == OK);
	FF_DEPTH_CHECK(p_context.begin_render_target(b, 0xff000000, false, true) == OK);
	FF_DEPTH_CHECK(_draw(device, 0xff00ff00, 0, depth) == OK && p_context.end_render_target() == OK);
	// Sample the color planes in a normal 2D window pass; depth stays target-local.
	FF_DEPTH_CHECK(p_context.begin_device_frame(0xff000000) == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters sample;
	sample.clip_enabled = true;
	sample.clip_rect = Rect2i(0, 0, CAPTURE_WIDTH / 2, CAPTURE_HEIGHT);
	FF_DEPTH_CHECK(_draw(device, 0xffffffff, 0.95f, sample, native_a) == OK);
	RID native_b;
	FF_DEPTH_CHECK(storage.texture_get_device_texture(storage.render_target_get_texture(b), native_b) == OK);
	sample.clip_rect.position.x = CAPTURE_WIDTH / 2;
	FF_DEPTH_CHECK(_draw(device, 0xffffffff, 0.95f, sample, native_b) == OK);
	FF_DEPTH_CHECK(_finish(p_context, r_pixels, 0xffff0000, 0xff00ff00));
	FF_DEPTH_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK);
	FF_DEPTH_CHECK(storage.render_target_is_ready(a) == p_preserves_targets_on_resize && storage.render_target_is_ready(b) == p_preserves_targets_on_resize);
	FF_DEPTH_CHECK(p_context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK && p_context.prepare_resources() == OK);
	FF_DEPTH_CHECK(storage.render_target_allocate(a) == OK);
	FF_DEPTH_CHECK(storage.render_target_get_texture(a) == texture_a);
	FF_DEPTH_CHECK(storage.texture_get_device_texture(texture_a, native_a) == OK);
	FF_DEPTH_CHECK(device.texture_get_info(native_a, with_depth) == OK);
	FF_DEPTH_CHECK(p_preserves_targets_on_resize ? with_depth.bytes > color_only.bytes : with_depth.bytes == color_only.bytes);
	FF_DEPTH_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(a, 0xff000000, true, true) == OK);
	FF_DEPTH_CHECK(_draw(device, 0xff00ff00, 0.75f, depth) == OK && p_context.end_render_target() == OK);
	FF_DEPTH_CHECK(p_context.begin_device_frame(0xff000000) == OK);
	FF_DEPTH_CHECK(_draw(device, 0xffffffff, 0, FixedFunctionGraphicsDevice::FFDrawParameters(), native_a) == OK);
	FF_DEPTH_CHECK(_finish(p_context, r_pixels, 0xff00ff00, 0xff00ff00));
	FF_DEPTH_CHECK(storage.free(a) == OK && storage.free(b) == OK);
	FF_DEPTH_CHECK(device.get_texture_memory_usage() == 0);
	return true;
}

bool TestFixedFunctionDepth::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels, bool p_preserves_targets_on_resize) {
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FF_DEPTH_CHECK(context.initialize() == OK);
	FF_DEPTH_CHECK(p_device.clear_depth() == ERR_UNCONFIGURED);
	FixedFunctionGraphicsDevice::FFDrawParameters depth;
	depth.depth_test_enabled = depth.depth_write_enabled = true;
	FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0xff000000) == OK);
	FF_DEPTH_CHECK(p_device.clear_depth() == ERR_UNCONFIGURED);
	FF_DEPTH_CHECK(_draw(p_device, 0xffff0000, 0, depth) == (p_device.get_capabilities().depth_buffer ? ERR_UNCONFIGURED : ERR_UNAVAILABLE));
	FF_DEPTH_CHECK(context.begin_device_frame(0, false, true) == ERR_BUSY);
	FF_DEPTH_CHECK(context.end_frame(false) == OK);
	if (!p_device.get_capabilities().depth_buffer) {
		FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0, true, true) == ERR_UNAVAILABLE);
		FF_DEPTH_CHECK(context.cancel_frame() == OK);
		printf("SKIP: optional depth buffers unavailable; color-only pass remains usable.\n");
		return true;
	}
	// First depth allocation on a color load initializes depth without clearing color.
	FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0xffff00ff, false, true) == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters first_load = depth;
	first_load.clip_enabled = true;
	first_load.clip_rect = Rect2i(0, 0, CAPTURE_WIDTH / 2, CAPTURE_HEIGHT);
	FF_DEPTH_CHECK(_draw(p_device, 0xff0000ff, 0.5f, first_load) == OK);
	FF_DEPTH_CHECK(_finish(context, r_pixels, 0xff0000ff, 0xff000000));
	FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0xff000000, true, true) == OK);
	FF_DEPTH_CHECK(_draw(p_device, 0xff0000ff, 0.5f, depth) == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters clipped = depth;
	clipped.clip_enabled = true;
	clipped.clip_rect = Rect2i(0, 0, CAPTURE_WIDTH / 2, CAPTURE_HEIGHT);
	FF_DEPTH_CHECK(_draw(p_device, 0xffff0000, -0.5f, clipped) == OK);
	FF_DEPTH_CHECK(_draw(p_device, 0xff00ff00, 0.75f, depth) == OK);
	clipped.clip_rect.position.x = CAPTURE_WIDTH / 2;
	FF_DEPTH_CHECK(_draw(p_device, 0xffffff00, 0.5f, clipped) == OK); // LESS_EQUAL preserves equality.
	FF_DEPTH_CHECK(_finish(context, r_pixels, 0xffff0000, 0xffffff00));
	const std::vector<uint32_t> capture = r_pixels;
	FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0, false, true) == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters read_only = depth;
	read_only.depth_write_enabled = false;
	FF_DEPTH_CHECK(_draw(p_device, 0xffffffff, -0.75f, read_only) == OK);
	FF_DEPTH_CHECK(_draw(p_device, 0xff00ff00, 0, depth) == OK);
	FF_DEPTH_CHECK(_finish(context, r_pixels, 0xffffffff, 0xff00ff00));
	FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0, false, true) == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters write_only = depth;
	write_only.depth_test_enabled = false;
	FF_DEPTH_CHECK(_draw(p_device, 0xffffff00, 0.75f, write_only) == OK);
	FF_DEPTH_CHECK(_draw(p_device, 0xff0000ff, 0, depth) == OK);
	FF_DEPTH_CHECK(_finish(context, r_pixels, 0xff0000ff, 0xff0000ff));
	FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0, false, true) == OK);
	FF_DEPTH_CHECK(_draw(p_device, 0xffffffff, 0.95f, FixedFunctionGraphicsDevice::FFDrawParameters()) == OK);
	FF_DEPTH_CHECK(_draw(p_device, 0xffff0000, 0.5f, depth) == OK);
	FF_DEPTH_CHECK(_finish(context, r_pixels, 0xffffffff, 0xffffffff));
	FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0, false, true) == OK);
	FF_DEPTH_CHECK(p_device.clear_color(0xffff00ff) == OK);
	FF_DEPTH_CHECK(_draw(p_device, 0xffff0000, 0.5f, depth) == OK);
	FF_DEPTH_CHECK(_finish(context, r_pixels, 0xffff00ff, 0xffff00ff));
	FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0, false, true) == OK);
	FF_DEPTH_CHECK(p_device.clear_depth(std::numeric_limits<float>::quiet_NaN()) == ERR_INVALID_PARAMETER);
	FF_DEPTH_CHECK(p_device.clear_depth(std::numeric_limits<float>::infinity()) == ERR_INVALID_PARAMETER);
	FF_DEPTH_CHECK(p_device.clear_depth(-0.1f) == ERR_INVALID_PARAMETER && p_device.clear_depth(1.1f) == ERR_INVALID_PARAMETER);
	FF_DEPTH_CHECK(p_device.clear_depth() == OK);
	clipped.clip_rect.position.x = 0;
	FF_DEPTH_CHECK(_draw(p_device, 0xffff0000, -0.5f, clipped) == OK);
	FF_DEPTH_CHECK(p_device.clear_depth(0) == OK); // Must clear outside that previous clip too.
	FF_DEPTH_CHECK(_draw(p_device, 0xff00ff00, -0.75f, depth) == OK);
	FF_DEPTH_CHECK(_finish(context, r_pixels, 0xffff0000, 0xffff00ff));
	if (p_device.get_capabilities().alpha_cutoff) {
		FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0xff000000, true, true) == OK);
		FixedFunctionGraphicsDevice::FFDrawParameters cutout = depth;
		cutout.alpha_cutoff_enabled = true;
		FF_DEPTH_CHECK(_draw(p_device, 0x40ff0000, -0.75f, cutout) == OK);
		FF_DEPTH_CHECK(_draw(p_device, 0xff0000ff, 0.5f, depth) == OK);
		FF_DEPTH_CHECK(_finish(context, r_pixels, 0xff0000ff, 0xff0000ff));
	}
	// Right-handed perspective depth must reject the farther surface in either submission order.
	CameraMatrix projection;
	projection.set_perspective(75, real_t(CAPTURE_WIDTH) / real_t(CAPTURE_HEIGHT), 0.1, 50);
	for (int reverse = 0; reverse < 2; reverse++) {
		FF_DEPTH_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0xff000000, true, true) == OK);
		for (int i = 0; i < 2; i++) {
			const bool depth_near = (i != 0) != (reverse != 0);
			FF_DEPTH_CHECK(_draw(p_device, depth_near ? 0xffff0000 : 0xff0000ff, depth_near ? -1 : -2, depth, RID(), projection) == OK);
		}
		FF_DEPTH_CHECK(_finish(context, r_pixels, 0xffff0000, 0xffff0000));
	}
	FF_DEPTH_CHECK(_test_targets(context, r_pixels, p_preserves_targets_on_resize));
	r_pixels = capture;
	printf("PASS: optional depth, LEQUAL, independent test/write, color-only overlays, full-depth clears, right-handed perspective, target-local loads, scaled backing, memory accounting, reset and release.\n");
	return true;
}

#undef FF_DEPTH_CHECK
