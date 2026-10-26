#include "test_render_targets.h"

#include <stdio.h>

#define FF_READBACK_CHECK(m_condition)                                   \
	do {                                                                 \
		if (!(m_condition)) {                                            \
			printf("Readback FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                \
		}                                                                \
	} while (false)

bool TestFixedFunctionRenderTargets::_readback_pattern(Ref<Image> p_image, const Size2i &p_size, bool p_flipped) {
	FF_READBACK_CHECK(p_image.is_valid() && p_image->get_width() == p_size.x && p_image->get_height() == p_size.y && p_image->get_format() == Image::FORMAT_RGBA8 && !p_image->has_mipmaps());
	const Color colors[] = { Color(1, 0, 0), Color(0, 1, 0), Color(0, 0, 1), Color(1, 1, 1) };
	p_image->lock();
	bool matches = true;
	for (int i = 0; i < 4; i++) {
		const int x = (i & 1) ? p_size.x * 3 / 4 : p_size.x / 4;
		int y = (i & 2) ? p_size.y * 3 / 4 : p_size.y / 4;
		if (p_flipped)
			y = p_size.y - 1 - y;
		const Color actual = p_image->get_pixel(x, y);
		for (int channel = 0; channel < 4; channel++)
			matches &= Math::abs(actual[channel] - colors[i][channel]) < 0.04f;
	}
	p_image->unlock();
	FF_READBACK_CHECK(matches);
	return true;
}

bool TestFixedFunctionRenderTargets::_test_readback(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, RID p_target) {
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	const RID texture = storage.render_target_get_texture(p_target);
	const Size2i size = storage.render_target_get_rect(p_target).size;
	RID native;
	FF_READBACK_CHECK(storage.texture_get_device_texture(texture, native) == OK);
	std::vector<uint32_t> window(size_t(p_device.get_width()) * p_device.get_height()), after(window.size());
	FF_READBACK_CHECK(p_device.read_frame(window.data(), window.size()) == OK);
	std::vector<uint32_t> pixels(size_t(size.x) * size.y, 0x12345678);
	FF_READBACK_CHECK(p_device.texture_read_render_target(RID(), pixels.data(), pixels.size()) == ERR_INVALID_PARAMETER);
	FF_READBACK_CHECK(p_device.texture_read_render_target(native, pixels.data(), pixels.size() - 1) == ERR_INVALID_PARAMETER);
	FF_READBACK_CHECK(p_device.texture_read_render_target(native, nullptr, pixels.size()) == ERR_INVALID_PARAMETER && pixels.front() == 0x12345678);
	FF_READBACK_CHECK(p_device.texture_read_render_target(native, pixels.data(), pixels.size()) == OK);
	FF_READBACK_CHECK(pixels[size_t(size.y / 4) * size.x + size.x / 4] == 0xffff0000 && pixels[size_t(size.y * 3 / 4) * size.x + size.x / 4] == 0xff0000ff);
	const Ref<Image> snapshot = storage.texture_get_data(texture);
	FF_READBACK_CHECK(_readback_pattern(snapshot, size, true));
	FF_READBACK_CHECK(p_device.read_frame(after.data(), after.size()) == OK && after == window);
	FF_READBACK_CHECK(storage.render_target_is_ready(p_target));
	FF_READBACK_CHECK(storage.render_target_set_flag(p_target, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP, true) == OK);
	FF_READBACK_CHECK(_readback_pattern(storage.texture_get_data(texture), size, false));
	FF_READBACK_CHECK(_readback_pattern(snapshot, size, true));
	FF_READBACK_CHECK(storage.render_target_set_flag(p_target, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP, false) == OK);
	Ref<Image> writable = storage.texture_get_data(texture);
	writable->fill(Color(0, 0, 0));
	FF_READBACK_CHECK(_readback_pattern(storage.texture_get_data(texture), size, true));
	FF_READBACK_CHECK(storage.render_target_set_failed(p_target, true) == OK && storage.texture_get_data(texture).is_null());
	FF_READBACK_CHECK(storage.render_target_set_failed(p_target, false) == OK);
	FF_READBACK_CHECK(storage.render_target_set_size(p_target, size.x + 1, size.y) == OK && storage.texture_get_data(texture).is_null());
	FF_READBACK_CHECK(storage.render_target_set_size(p_target, size.x, size.y) == OK && _readback_pattern(storage.texture_get_data(texture), size, true));
	FF_READBACK_CHECK(p_context.begin_frame(0) == OK && p_context.begin_device_frame(0xff182430) == OK);
	FF_READBACK_CHECK(p_device.texture_read_render_target(native, pixels.data(), pixels.size()) == ERR_BUSY && storage.texture_get_data(texture).is_null());
	FF_READBACK_CHECK(_draw_quad(p_device, 0xff00ffff) == OK && p_context.end_frame(false) == OK);
	FF_READBACK_CHECK(p_device.read_frame(after.data(), after.size()) == OK && after[after.size() / 2] == 0xff00ffff);
	FF_READBACK_CHECK(_readback_pattern(storage.texture_get_data(texture), size, true));
	// A non-power-of-two target exposes logical pixels rather than backing padding.
	const RID temporary = storage.render_target_create();
	const RID temporary_texture = storage.render_target_get_texture(temporary);
	const Size2i temporary_size(50, 34);
	FF_READBACK_CHECK(storage.render_target_set_size(temporary, temporary_size.x, temporary_size.y) == OK && storage.render_target_allocate(temporary) == OK);
	FF_READBACK_CHECK(storage.texture_get_data(temporary_texture).is_null());
	RID temporary_native;
	FF_READBACK_CHECK(storage.texture_get_device_texture(temporary_texture, temporary_native) == OK);
	FF_READBACK_CHECK(p_device.texture_read_render_target(temporary_native, pixels.data(), pixels.size()) == ERR_UNCONFIGURED);
	FF_READBACK_CHECK(p_context.begin_frame(0) == OK && p_context.begin_render_target(temporary, 0xff182430) == OK);
	FF_READBACK_CHECK(storage.texture_get_data(temporary_texture).is_null() && p_device.texture_read_render_target(native, pixels.data(), pixels.size()) == ERR_BUSY);
	FF_READBACK_CHECK(_draw_pattern(p_device) && p_context.end_render_target() == OK && p_context.end_frame(false) == OK);
	const Ref<Image> retained = storage.texture_get_data(temporary_texture);
	FF_READBACK_CHECK(_readback_pattern(retained, temporary_size, true));
	FF_READBACK_CHECK(storage.free(temporary) == OK && _readback_pattern(retained, temporary_size, true));
	FF_READBACK_CHECK(p_device.texture_read_render_target(temporary_native, pixels.data(), pixels.size()) == ERR_INVALID_PARAMETER);
	printf("PASS: completed viewport readback, native/image orientation, VFLIP, opaque RGBA snapshots, NPOT backing, invalid/busy/stale targets, state preservation and independent lifetime.\n");
	return true;
}

#undef FF_READBACK_CHECK
