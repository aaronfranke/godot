#include "test_render_targets.h"

#include "drivers/fixed_function/render_target_manager.h"

#include <stdio.h>

#define FF_ALPHA_CHECK(m_condition)                                                \
	do {                                                                           \
		if (!(m_condition)) {                                                      \
			printf("Transparent target FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                          \
		}                                                                          \
	} while (false)

class TestFixedFunctionTransparentPixel {
public:
	static bool matches(uint32_t p_actual, uint32_t p_expected) {
		for (int shift = 0; shift < 32; shift += 8) {
			const int difference = int((p_actual >> shift) & 255) - int((p_expected >> shift) & 255);
			if (difference < -3 || difference > 3) {
				printf("Transparent pixel actual=%08x expected=%08x\n", p_actual, p_expected);
				return false;
			}
		}
		return true;
	}
};

bool TestFixedFunctionRenderTargets::test_transparency(FixedFunctionGraphicsDevice &p_device, void *p_window, bool p_preserves_targets_on_resize) {
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FF_ALPHA_CHECK(context.initialize() == OK);
	return _test_transparency(context, p_device, p_preserves_targets_on_resize);
}

bool TestFixedFunctionRenderTargets::_test_transparency(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, bool p_preserves_targets_on_resize) {
	typedef FixedFunctionGraphicsDevice Device;
	typedef FixedFunctionTextureStorage Storage;
	Storage &storage = p_context.get_texture_storage();
	if (!p_device.get_capabilities().render_target_transparent) {
		RID unavailable;
		FF_ALPHA_CHECK(p_device.texture_create_render_target(32, 32, unavailable, true) == ERR_UNAVAILABLE && !unavailable.is_valid());
		printf("PASS: optional transparent targets unavailable; opaque viewport tests retained.\n");
		return true;
	}
	const RID source = storage.render_target_create();
	const RID nested = storage.render_target_create();
	const RID targets[] = { source, nested };
	for (unsigned i = 0; i < 2; i++) {
		FF_ALPHA_CHECK(storage.render_target_set_size(targets[i], 32, 32) == OK);
		FF_ALPHA_CHECK(storage.render_target_set_flag(targets[i], Storage::RENDER_TARGET_TRANSPARENT, true) == OK);
		FF_ALPHA_CHECK(storage.render_target_allocate(targets[i]) == OK);
	}
	const RID texture = storage.render_target_get_texture(source);
	FF_ALPHA_CHECK(storage.texture_get_format(texture) == Image::FORMAT_RGBA8);
	RID native;
	FF_ALPHA_CHECK(storage.texture_get_device_texture(texture, native) == OK);
	std::vector<uint32_t> pixels(32 * 32);
	Device::FFDrawParameters mix;
	mix.blend_mode = Device::BLEND_MODE_MIX;
	FF_ALPHA_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(source, 0) == OK);
	FF_ALPHA_CHECK(_draw_quad(p_device, 0x80ff0000, RID(), mix) == OK);
	Device::FFDrawParameters clipped = mix;
	clipped.clip_enabled = true;
	clipped.clip_rect = Rect2i(16, 0, 16, 32);
	FF_ALPHA_CHECK(_draw_quad(p_device, 0x800000ff, RID(), clipped) == OK);
	FF_ALPHA_CHECK(p_context.end_render_target() == OK && p_context.end_frame(false) == OK);
	FF_ALPHA_CHECK(p_device.texture_read_render_target(native, pixels.data(), pixels.size()) == OK);
	FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(pixels[8 * 32 + 8], 0x80800000));
	FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(pixels[8 * 32 + 24], 0xc0400080));
	Ref<Image> snapshot = storage.texture_get_data(texture);
	FF_ALPHA_CHECK(snapshot.is_valid());
	snapshot->lock();
	const Color sample = snapshot->get_pixel(8, 8);
	snapshot->unlock();
	FF_ALPHA_CHECK(Math::abs(sample.a - 128.0f / 255) < 0.01f && Math::abs(sample.r - sample.a) < 0.01f);
	// Viewport textures follow GLES: consumers select premultiplied alpha explicitly.
	Device::FFDrawParameters premultiplied;
	premultiplied.blend_mode = Device::BLEND_MODE_PREMULTIPLIED_ALPHA;
	FF_ALPHA_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(nested, 0) == OK);
	FF_ALPHA_CHECK(_draw_quad(p_device, 0xffffffff, native, premultiplied) == OK);
	FF_ALPHA_CHECK(p_context.end_render_target() == OK && p_context.end_frame(false) == OK);
	RID nested_native;
	FF_ALPHA_CHECK(storage.texture_get_device_texture(storage.render_target_get_texture(nested), nested_native) == OK);
	FF_ALPHA_CHECK(p_device.texture_read_render_target(nested_native, pixels.data(), pixels.size()) == OK);
	FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(pixels[8 * 32 + 24], 0xc0400080));
	// Screen blitting must also composite premultiplied data instead of overwriting RGB.
	FixedFunctionRenderTargetManager passes(p_context);
	FF_ALPHA_CHECK(passes.begin_frame(0) == OK && passes.select_render_target(RID()) == OK);
	FF_ALPHA_CHECK(passes.clear_render_target(Color(0, 1, 0)) == OK);
	FF_ALPHA_CHECK(passes.blit_to_screen(source, Rect2(0, 0, p_device.get_width(), p_device.get_height())) == OK);
	FF_ALPHA_CHECK(passes.end_frame(false) == OK);
	std::vector<uint32_t> window(size_t(p_device.get_width()) * p_device.get_height());
	FF_ALPHA_CHECK(p_device.read_frame(window.data(), window.size()) == OK);
	FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(window[window.size() / 2 + p_device.get_width() / 4], 0xff807f00));
	FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(window[window.size() / 2 + p_device.get_width() * 3 / 4], 0xff403f80));
	// A fresh CLEAR_NEVER target still receives deterministic transparent contents.
	const RID empty = storage.render_target_create();
	FF_ALPHA_CHECK(storage.render_target_set_size(empty, 32, 32) == OK);
	FF_ALPHA_CHECK(storage.render_target_set_flag(empty, Storage::RENDER_TARGET_TRANSPARENT, true) == OK);
	FF_ALPHA_CHECK(passes.begin_frame(0) == OK && passes.select_render_target(empty) == OK);
	FF_ALPHA_CHECK(passes.select_render_target(RID()) == OK && passes.end_frame(false) == OK);
	RID empty_native;
	FF_ALPHA_CHECK(storage.texture_get_device_texture(storage.render_target_get_texture(empty), empty_native) == OK);
	FF_ALPHA_CHECK(p_device.texture_read_render_target(empty_native, pixels.data(), pixels.size()) == OK && pixels[0] == 0);
	FF_ALPHA_CHECK(storage.free(empty) == OK);
	// Validate alpha factors independently of RGB, including state transitions and color-only passes.
	const Device::FFBlendMode modes[] = { Device::BLEND_MODE_MIX, Device::BLEND_MODE_ADD, Device::BLEND_MODE_MUL, Device::BLEND_MODE_PREMULTIPLIED_ALPHA, Device::BLEND_MODE_ADD_UNWEIGHTED, Device::BLEND_MODE_DISABLED };
	const uint32_t expected[] = { 0xa0502814, 0x80603018, 0x20100401, 0xa0904824, 0xc0a05028, 0x80804020 };
	for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
		Device::FFDrawParameters state;
		state.blend_mode = modes[i];
		FF_ALPHA_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(source, 0) == OK);
		FF_ALPHA_CHECK(p_device.clear_color(0x40201008) == OK);
		FF_ALPHA_CHECK(_draw_quad(p_device, 0x80804020, RID(), state) == OK);
		FF_ALPHA_CHECK(p_context.end_render_target() == OK && p_context.end_frame(false) == OK);
		FF_ALPHA_CHECK(p_device.texture_read_render_target(native, pixels.data(), pixels.size()) == OK);
		FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(pixels[0], expected[i]));
	}
	Ref<Image> input = memnew(Image);
	input->create(4, 4, false, Image::FORMAT_RGBA8);
	input->fill(Color(1, 0, 0, 0.5f));
	RID input_texture;
	FF_ALPHA_CHECK(p_device.texture_create(input, input_texture) == OK);
	FF_ALPHA_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(source, 0) == OK);
	FF_ALPHA_CHECK(_draw_quad(p_device, 0x80ffffff, input_texture, mix) == OK);
	FF_ALPHA_CHECK(p_context.end_render_target() == OK && p_context.end_frame(false) == OK);
	FF_ALPHA_CHECK(p_device.texture_read_render_target(native, pixels.data(), pixels.size()) == OK);
	FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(pixels[0], 0x40400000));
	FF_ALPHA_CHECK(p_device.texture_free(input_texture) == OK);
	Device::FFDrawParameters cutout = mix;
	cutout.alpha_cutoff_enabled = true;
	cutout.alpha_cutoff = 0.75f;
	FF_ALPHA_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(source, 0x40201008) == OK);
	FF_ALPHA_CHECK(_draw_quad(p_device, 0x80ff0000, RID(), cutout) == OK);
	FF_ALPHA_CHECK(p_context.end_render_target() == OK && p_context.end_frame(false) == OK);
	FF_ALPHA_CHECK(p_device.texture_read_render_target(native, pixels.data(), pixels.size()) == OK);
	FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(pixels[0], 0x40201008));
	Device::FFDrawParameters emission;
	emission.blend_mode = Device::BLEND_MODE_ADD;
	emission.alpha_write_enabled = false;
	FF_ALPHA_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(source, 0) == OK);
	FF_ALPHA_CHECK(_draw_quad(p_device, 0x80ff0000, RID(), mix) == OK);
	FF_ALPHA_CHECK(_draw_quad(p_device, 0x80004000, RID(), emission) == OK);
	FF_ALPHA_CHECK(p_context.end_render_target() == OK && p_context.end_frame(false) == OK);
	FF_ALPHA_CHECK(p_device.texture_read_render_target(native, pixels.data(), pixels.size()) == OK);
	FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(pixels[0], 0x80802000));
	// Reallocation on format changes invalidates stale contents without changing the engine texture RID.
	FF_ALPHA_CHECK(storage.render_target_set_flag(source, Storage::RENDER_TARGET_TRANSPARENT, false) == OK);
	FF_ALPHA_CHECK(!storage.render_target_is_ready(source) && storage.texture_get_data(texture).is_null());
	FF_ALPHA_CHECK(storage.texture_get_format(texture) == Image::FORMAT_RGB8);
	FF_ALPHA_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(source, 0) == OK);
	FF_ALPHA_CHECK(_draw_quad(p_device, 0x80ff0000, RID(), mix) == OK);
	FF_ALPHA_CHECK(p_context.end_render_target() == OK && p_context.end_frame(false) == OK);
	FF_ALPHA_CHECK(storage.texture_get_device_texture(texture, native) == OK);
	FF_ALPHA_CHECK(p_device.texture_read_render_target(native, pixels.data(), pixels.size()) == OK);
	FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(pixels[0], 0xff800000));
	FF_ALPHA_CHECK(storage.render_target_set_flag(source, Storage::RENDER_TARGET_TRANSPARENT, true) == OK);
	FF_ALPHA_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(source, 0) == OK);
	FF_ALPHA_CHECK(_draw_quad(p_device, 0x80ff0000, RID(), mix) == OK);
	FF_ALPHA_CHECK(p_context.end_render_target() == OK && p_context.end_frame(false) == OK);
	FF_ALPHA_CHECK(p_context.resize(CAPTURE_WIDTH + 8, CAPTURE_HEIGHT + 8) == OK && p_context.prepare_resources() == OK);
	FF_ALPHA_CHECK(storage.render_target_is_ready(source) == p_preserves_targets_on_resize);
	FF_ALPHA_CHECK(storage.texture_get_data(texture).is_valid() == p_preserves_targets_on_resize);
	FF_ALPHA_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(source, 0) == OK);
	FF_ALPHA_CHECK(_draw_quad(p_device, 0x80ff0000, RID(), mix) == OK);
	FF_ALPHA_CHECK(p_context.end_render_target() == OK && p_context.end_frame(false) == OK);
	FF_ALPHA_CHECK(storage.texture_get_device_texture(texture, native) == OK);
	FF_ALPHA_CHECK(p_device.texture_read_render_target(native, pixels.data(), pixels.size()) == OK);
	FF_ALPHA_CHECK(TestFixedFunctionTransparentPixel::matches(pixels[0], 0x80800000));
	FF_ALPHA_CHECK(storage.free(source) == OK && storage.free(nested) == OK);
	FF_ALPHA_CHECK(p_context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK && p_context.prepare_resources() == OK);
	printf("PASS: transparent targets, independent alpha blending, nested/screen composition, emission coverage, readback, format changes and resize/recovery.\n");
	return true;
}

#undef FF_ALPHA_CHECK
