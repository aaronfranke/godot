#include "test_render_targets.h"

#include "scene/main/viewport.h"

#include <limits.h>
#include <stdio.h>

#include <limits>

#define FIXED_FUNCTION_TARGET_CHECK(m_condition)                            \
	do {                                                                    \
		if (!(m_condition)) {                                               \
			printf("Target FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                   \
		}                                                                   \
	} while (false)

Error TestFixedFunctionRenderTargets::_draw_quad(FixedFunctionGraphicsDevice &p_device, uint32_t p_color, RID p_texture, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	const FixedFunctionVertex quad[] = {
		{ -1, 1, 0, p_color, 0, 0 },
		{ 1, 1, 0, p_color, 1, 0 },
		{ 1, -1, 0, p_color, 1, 1 },
		{ -1, 1, 0, p_color, 0, 0 },
		{ 1, -1, 0, p_color, 1, 1 },
		{ -1, -1, 0, p_color, 0, 1 },
	};
	return p_device.draw_triangles(quad, 6, CameraMatrix(), p_texture, p_state);
}

bool TestFixedFunctionRenderTargets::_draw_pattern(FixedFunctionGraphicsDevice &p_device) {
	const int width = p_device.get_target_width();
	const int height = p_device.get_target_height();
	const uint32_t colors[4] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };
	for (int i = 0; i < 4; i++) {
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.clip_enabled = true;
		state.clip_rect = Rect2i((i & 1) ? width / 2 : 1, (i & 2) ? height / 2 : 1, width / 2 - 1, height / 2 - 1);
		FIXED_FUNCTION_TARGET_CHECK(_draw_quad(p_device, colors[i], RID(), state) == OK);
	}
	return true;
}

bool TestFixedFunctionRenderTargets::_test_defaults(FixedFunctionTextureStorage &p_storage, RID p_target, bool p_transparent) {
	typedef FixedFunctionTextureStorage Storage;
	for (int i = 0; i < Storage::RENDER_TARGET_FLAG_MAX; i++) {
		FIXED_FUNCTION_TARGET_CHECK(!p_storage.render_target_get_flag(p_target, Storage::FFRenderTargetFlag(i)));
	}
	// Exercise actual Node defaults without replacing the test process's active server.
	Viewport *viewport = memnew(Viewport);
	const bool node_defaults = viewport->get_hdr() && viewport->get_msaa() == Viewport::MSAA_DISABLED && !viewport->get_vflip() && !viewport->has_transparent_background() && !viewport->is_using_render_direct_to_screen() && !viewport->is_using_32_bpc_depth();
	const bool accepted = p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_HDR, viewport->get_hdr()) == OK &&
			p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_VFLIP, viewport->get_vflip()) == OK &&
			p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_TRANSPARENT, viewport->has_transparent_background()) == OK &&
			p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_DIRECT_TO_SCREEN, viewport->is_using_render_direct_to_screen()) == OK &&
			p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_USE_32_BPC_DEPTH, viewport->is_using_32_bpc_depth()) == OK &&
			p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_KEEP_3D_LINEAR, viewport->get_keep_3d_linear()) == OK &&
			p_storage.render_target_set_msaa(p_target, 0) == OK &&
			p_storage.render_target_set_fxaa(p_target, viewport->get_use_fxaa()) == OK &&
			p_storage.render_target_set_debanding(p_target, viewport->get_use_debanding()) == OK &&
			p_storage.render_target_set_sharpen_intensity(p_target, viewport->get_sharpen_intensity()) == OK;
	memdelete(viewport);
	FIXED_FUNCTION_TARGET_CHECK(node_defaults && accepted);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_get_flag(p_target, Storage::RENDER_TARGET_HDR));
	const Storage::FFRenderTargetFlag hints[] = { Storage::RENDER_TARGET_NO_3D, Storage::RENDER_TARGET_NO_3D_EFFECTS, Storage::RENDER_TARGET_NO_SAMPLING, Storage::RENDER_TARGET_KEEP_3D_LINEAR };
	for (unsigned int i = 0; i < sizeof(hints) / sizeof(hints[0]); i++) {
		FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_flag(p_target, hints[i], true) == OK && p_storage.render_target_get_flag(p_target, hints[i]));
	}
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_VFLIP, true) == OK);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_get_flag(p_target, Storage::RENDER_TARGET_VFLIP));
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_VFLIP, false) == OK);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_TRANSPARENT, true) == (p_transparent ? OK : ERR_UNAVAILABLE));
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_get_flag(p_target, Storage::RENDER_TARGET_TRANSPARENT) == p_transparent);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_TRANSPARENT, false) == OK);
	const Storage::FFRenderTargetFlag unavailable[] = { Storage::RENDER_TARGET_DIRECT_TO_SCREEN, Storage::RENDER_TARGET_USE_32_BPC_DEPTH };
	for (unsigned int i = 0; i < sizeof(unavailable) / sizeof(unavailable[0]); i++) {
		FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_flag(p_target, unavailable[i], true) == ERR_UNAVAILABLE);
		FIXED_FUNCTION_TARGET_CHECK(!p_storage.render_target_get_flag(p_target, unavailable[i]));
		FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_flag(p_target, unavailable[i], false) == OK);
	}
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_flag(p_target, Storage::FFRenderTargetFlag(-1), true) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_flag(p_target, Storage::RENDER_TARGET_FLAG_MAX, false) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_msaa(p_target, 2) == ERR_UNAVAILABLE);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_fxaa(p_target, true) == ERR_UNAVAILABLE && p_storage.render_target_set_debanding(p_target, true) == ERR_UNAVAILABLE);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_sharpen_intensity(p_target, 0.5f) == ERR_UNAVAILABLE);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_sharpen_intensity(p_target, -1) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_sharpen_intensity(p_target, std::numeric_limits<float>::quiet_NaN()) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_sharpen_intensity(p_target, std::numeric_limits<float>::infinity()) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_fxaa(RID(), false) == ERR_INVALID_PARAMETER);
	const RID closing = p_storage.render_target_create();
	FIXED_FUNCTION_TARGET_CHECK(p_storage.request_free(closing) == OK);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_flag(closing, Storage::RENDER_TARGET_HDR, true) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_msaa(closing, 0) == ERR_ALREADY_IN_USE && p_storage.render_target_set_fxaa(closing, false) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.render_target_set_debanding(closing, false) == ERR_ALREADY_IN_USE && p_storage.render_target_set_sharpen_intensity(closing, 0) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_TARGET_CHECK(p_storage.update_pending(true) == OK);
	printf("PASS: actual Viewport defaults, LDR HDR fallback, usage hints, unsupported options, validation and closing-target protection.\n");
	return true;
}

bool TestFixedFunctionRenderTargets::_render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, RID p_source, RID p_copy, std::vector<uint32_t> &r_pixels) {
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	FIXED_FUNCTION_TARGET_CHECK(p_context.begin_frame(0.25) == OK);
	const uint64_t frame = p_context.get_frame_number();
	const double time = p_context.get_total_time();
	FIXED_FUNCTION_TARGET_CHECK(p_context.begin_render_target(p_source, 0x00182430) == OK);
	FIXED_FUNCTION_TARGET_CHECK(!storage.render_target_is_ready(p_source));
	FIXED_FUNCTION_TARGET_CHECK(p_context.prepare_resources() == ERR_BUSY);
	const Rect2i rect = storage.render_target_get_rect(p_source);
	FIXED_FUNCTION_TARGET_CHECK(p_device.get_target_width() == uint32_t(rect.size.x) && p_device.get_target_height() == uint32_t(rect.size.y));
	FIXED_FUNCTION_TARGET_CHECK(p_context.begin_device_frame(0) == ERR_BUSY);
	FIXED_FUNCTION_TARGET_CHECK(p_context.end_frame(false) == ERR_BUSY);
	RID source;
	FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(storage.render_target_get_texture(p_source), source) == OK);
	// Metadata-only hints/defaults remain legal during a pass and preserve its native identity.
	FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_flag(p_source, FixedFunctionTextureStorage::RENDER_TARGET_HDR, true) == OK);
	FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_flag(p_source, FixedFunctionTextureStorage::RENDER_TARGET_NO_SAMPLING, true) == OK);
	FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_fxaa(p_source, false) == OK);
	if (!p_device.get_capabilities().render_target_transparent) {
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_flag(p_source, FixedFunctionTextureStorage::RENDER_TARGET_TRANSPARENT, true) == ERR_UNAVAILABLE);
	}
	RID retained;
	FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(storage.render_target_get_texture(p_source), retained) == OK && retained == source);
	FIXED_FUNCTION_TARGET_CHECK(_draw_quad(p_device, 0xffffffff, source) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_TARGET_CHECK(storage.free(p_source) == ERR_BUSY);
	FIXED_FUNCTION_TARGET_CHECK(_draw_pattern(p_device));
	FIXED_FUNCTION_TARGET_CHECK(p_context.end_render_target() == OK);
	FIXED_FUNCTION_TARGET_CHECK(storage.render_target_is_ready(p_source));
	FIXED_FUNCTION_TARGET_CHECK(p_context.end_render_target() == ERR_UNCONFIGURED);
	FIXED_FUNCTION_TARGET_CHECK(p_context.begin_render_target(p_copy, 0x00182430) == OK);
	FIXED_FUNCTION_TARGET_CHECK(!storage.render_target_is_ready(p_copy) && storage.render_target_is_ready(p_source));
	FixedFunctionGraphicsDevice::FFDrawParameters blend;
	blend.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
	FIXED_FUNCTION_TARGET_CHECK(_draw_quad(p_device, 0x80ffffff, source, blend) == OK);
	FIXED_FUNCTION_TARGET_CHECK(p_context.end_render_target() == OK);
	FIXED_FUNCTION_TARGET_CHECK(storage.render_target_is_ready(p_copy));
	FIXED_FUNCTION_TARGET_CHECK(p_context.begin_device_frame(0xffff00ff) == OK);
	RID copy;
	FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(storage.render_target_get_texture(p_copy), copy) == OK);
	FIXED_FUNCTION_TARGET_CHECK(_draw_quad(p_device, 0xffffffff, copy, blend) == OK);
	FIXED_FUNCTION_TARGET_CHECK(p_context.end_frame(false) == OK);
	FIXED_FUNCTION_TARGET_CHECK(p_context.get_frame_number() == frame && p_context.get_total_time() == time && p_context.get_frame_delta_time() == 0.25);
	const uint32_t width = p_device.get_width();
	const uint32_t height = p_device.get_height();
	r_pixels.resize(size_t(width) * height);
	FIXED_FUNCTION_TARGET_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	// No-op preparation must neither clear a readable window nor invalidate completed targets.
	const std::vector<uint32_t> saved_pixels = r_pixels;
	FIXED_FUNCTION_TARGET_CHECK(p_context.prepare_resources() == OK && p_context.prepare_resources() == OK);
	FIXED_FUNCTION_TARGET_CHECK(storage.render_target_is_ready(p_source) && storage.render_target_is_ready(p_copy));
	FIXED_FUNCTION_TARGET_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK && r_pixels == saved_pixels);
	FIXED_FUNCTION_TARGET_CHECK(p_context.get_frame_number() == frame && p_context.get_total_time() == time && p_context.get_frame_delta_time() == 0.25);
	const uint32_t expected[5] = { 0xff8c1218, 0xff0c9218, 0xff0c1298, 0xff8c9298, 0xff182430 };
	for (int i = 0; i < 5; i++) {
		uint32_t x = 0;
		if (i != 4) {
			x = (i & 1) ? width * 3 / 4 : width / 4;
		}
		uint32_t y = 0;
		if (i != 4) {
			y = (i & 2) ? height * 3 / 4 : height / 4;
		}
		const uint32_t actual = r_pixels[size_t(y) * width + x];
		for (int shift = 0; shift < 32; shift += 8) {
			const int delta = int((actual >> shift) & 255) - int((expected[i] >> shift) & 255);
			if (delta < -8 || delta > 8) {
				printf("Target pixel %d actual=%08x expected=%08x\n", i, actual, expected[i]);
				return false;
			}
		}
	}
	return true;
}

bool TestFixedFunctionRenderTargets::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	r_pixels.clear();
	{
		FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
		FIXED_FUNCTION_TARGET_CHECK(context.initialize() == OK);
		printf("Opaque render targets: %s\n", p_device.get_capabilities().render_target_opaque ? "yes" : "no");
		if (!p_device.get_capabilities().render_target_opaque) {
			printf("SKIP: device exposes no supported opaque render-target format.\n");
			return true;
		}
		FixedFunctionTextureStorage &storage = context.get_texture_storage();
		const RID target = storage.render_target_create();
		FIXED_FUNCTION_TARGET_CHECK(_test_defaults(storage, target, p_device.get_capabilities().render_target_transparent));
		FIXED_FUNCTION_TARGET_CHECK(_test_transparency(context, p_device));
		const RID copy = storage.render_target_create();
		const RID texture = storage.render_target_get_texture(target);
		const RID proxy = storage.texture_create();
		FIXED_FUNCTION_TARGET_CHECK(storage.texture_set_proxy(proxy, texture) == OK);
		const int target_height = p_device.get_capabilities().texture_square_only ? 32 : 16;
		// Exercise restoration to a window smaller than its offscreen target when caps allow it.
		const int copy_width = p_device.get_capabilities().max_texture_width >= 256 && p_device.get_capabilities().max_texture_height >= uint32_t(target_height * 8) ? 256 : 64;
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_size(target, 32, target_height) == OK);
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_position(target, 11, -7) == OK);
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_size(copy, copy_width, target_height * (copy_width / 32)) == OK);
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_allocate(target) == OK);
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_allocate(copy) == OK);
		FIXED_FUNCTION_TARGET_CHECK(!storage.render_target_is_ready(target) && !storage.render_target_is_ready(copy));
		FIXED_FUNCTION_TARGET_CHECK(!p_device.texture_is_render_target_ready(proxy));
		RID native;
		FIXED_FUNCTION_TARGET_CHECK(p_device.texture_create_render_target(0, 16, native) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_TARGET_CHECK(!native.is_valid());
		native = proxy;
		FIXED_FUNCTION_TARGET_CHECK(p_device.texture_create_render_target(16, 16, native) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_TARGET_CHECK(native == proxy);
		native = RID();
		FIXED_FUNCTION_TARGET_CHECK(p_device.begin_frame(0, proxy) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(proxy, native) == OK);
		FIXED_FUNCTION_TARGET_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_TARGET_CHECK(context.begin_device_frame(0) == OK);
		FIXED_FUNCTION_TARGET_CHECK(_draw_quad(p_device, 0xffffffff, native) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_TARGET_CHECK(context.end_frame(false) == OK);
		FIXED_FUNCTION_TARGET_CHECK(p_device.begin_frame(0, native) == OK);
		FIXED_FUNCTION_TARGET_CHECK(p_device.end_frame(true) == ERR_UNAVAILABLE);
		FIXED_FUNCTION_TARGET_CHECK(p_device.read_frame(nullptr, 0) == ERR_UNCONFIGURED);
		native = RID();
		FIXED_FUNCTION_TARGET_CHECK(_render_and_check(context, p_device, target, copy, r_pixels));
		FIXED_FUNCTION_TARGET_CHECK(_test_readback(context, p_device, target));
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_get_texture(target) == texture && storage.texture_get_proxy(proxy) == texture);
		RID before_hint;
		FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(texture, before_hint) == OK);
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_flag(target, FixedFunctionTextureStorage::RENDER_TARGET_HDR, false) == OK && storage.render_target_is_ready(target));
		RID after_hint;
		FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(texture, after_hint) == OK && after_hint == before_hint);
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_size(target, INT_MAX, INT_MAX) == OK);
		FIXED_FUNCTION_TARGET_CHECK(!storage.render_target_is_ready(target));
		if (p_device.get_capabilities().render_target_scaled) {
			FIXED_FUNCTION_TARGET_CHECK(storage.render_target_allocate(target) == OK);
			FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(proxy, native) == OK);
			FIXED_FUNCTION_TARGET_CHECK(native.is_valid() && !storage.render_target_is_ready(target));
		} else {
			FIXED_FUNCTION_TARGET_CHECK(storage.render_target_allocate(target) == ERR_UNAVAILABLE);
			FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(proxy, native) == ERR_UNCONFIGURED);
			FIXED_FUNCTION_TARGET_CHECK(!native.is_valid());
		}
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_size(target, 32, target_height) == OK);
		if (p_device.get_capabilities().render_target_scaled) {
			FIXED_FUNCTION_TARGET_CHECK(_render_and_check(context, p_device, target, copy, r_pixels));
		}
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_is_ready(target));
		FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(proxy, native) == OK);
		native = RID();

		FIXED_FUNCTION_TARGET_CHECK(context.resize(0, 0) == OK);
		const uint64_t suspended_frame = context.get_frame_number();
		const double suspended_time = context.get_total_time();
		FIXED_FUNCTION_TARGET_CHECK(context.prepare_resources() == ERR_SKIP && !context.is_frame_active());
		FIXED_FUNCTION_TARGET_CHECK(context.get_frame_number() == suspended_frame && context.get_total_time() == suspended_time);
		FIXED_FUNCTION_TARGET_CHECK(!storage.render_target_is_ready(target) && storage.texture_get_data(texture).is_null());
		FIXED_FUNCTION_TARGET_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_TARGET_CHECK(context.begin_render_target(target, 0) == ERR_SKIP);
		FIXED_FUNCTION_TARGET_CHECK(context.end_frame(false) == ERR_SKIP);
		FIXED_FUNCTION_TARGET_CHECK(context.resize(160, 120) == OK);
		FIXED_FUNCTION_TARGET_CHECK(context.prepare_resources() == OK && !context.is_frame_active());
		FIXED_FUNCTION_TARGET_CHECK(!storage.render_target_is_ready(target) && !storage.render_target_is_ready(copy));
		FIXED_FUNCTION_TARGET_CHECK(!storage.render_target_was_used(target));
		FIXED_FUNCTION_TARGET_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_TARGET_CHECK(context.begin_device_frame(0) == OK);
		FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(proxy, native) == OK);
		FIXED_FUNCTION_TARGET_CHECK(_draw_quad(p_device, 0xffffffff, native) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_TARGET_CHECK(context.end_frame(false) == OK);
		native = RID();
		FIXED_FUNCTION_TARGET_CHECK(_render_and_check(context, p_device, target, copy, r_pixels));
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_size(target, 64, target_height * 2) == OK);
		FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(proxy, native) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_TARGET_CHECK(!native.is_valid());
		FIXED_FUNCTION_TARGET_CHECK(_render_and_check(context, p_device, target, copy, r_pixels));
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_get_texture(target) == texture);
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_size(target, 0, 0) == OK);
		FIXED_FUNCTION_TARGET_CHECK(!storage.render_target_is_ready(target));
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_allocate(target) == OK);
		FIXED_FUNCTION_TARGET_CHECK(storage.texture_get_device_texture(proxy, native) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_TARGET_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_TARGET_CHECK(context.begin_render_target(target, 0) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_TARGET_CHECK(context.end_frame(false) == OK);
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_size(target, 32, target_height) == OK);
		FIXED_FUNCTION_TARGET_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
		// First offscreen begin prepares recovery before allocating fresh backing; no window pass.
		FIXED_FUNCTION_TARGET_CHECK(_render_and_check(context, p_device, target, copy, r_pixels));
		FIXED_FUNCTION_TARGET_CHECK(storage.free(target) == OK);
		FIXED_FUNCTION_TARGET_CHECK(!storage.texture_get_proxy(proxy).is_valid());
		FIXED_FUNCTION_TARGET_CHECK(storage.free(copy) == OK);
		const RID remaining = storage.render_target_create();
		FIXED_FUNCTION_TARGET_CHECK(storage.render_target_set_size(remaining, 32, target_height) == OK);
		FIXED_FUNCTION_TARGET_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_TARGET_CHECK(context.begin_render_target(remaining, 0) == OK);
		// Context destruction closes this offscreen pass before freeing target/device resources.
	}
	FIXED_FUNCTION_TARGET_CHECK(p_device.get_width() == 0);
	printf("PASS: offscreen passes, idle preparation/readiness, target-sized clipping, opaque composition, feedback rejection, stable viewport textures, reset invalidation/redraw, resize, suspension, and cleanup.\n");
	return true;
}

#undef FIXED_FUNCTION_TARGET_CHECK
