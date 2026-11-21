#include "test_texture_partial_updates.h"

#include "drivers/fixed_function/renderer_texture_storage_fixed_function.h"
#include "servers/rendering/renderer_compositor.h"

#include <limits.h>
#include <stdio.h>

#include <type_traits>

#define FIXED_FUNCTION_PARTIAL_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Partial FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

static_assert(!std::is_abstract<RendererTextureStorageFixedFunction>::value, "The restricted texture storage boundary must be concrete.");

Ref<Image> TestFixedFunctionTexturePartialUpdates::_make_image(Image::Format p_format, const Color &p_color) {
	Ref<Image> image;
	image = Image::create_empty(TEXTURE_SIZE, TEXTURE_SIZE, false, p_format);
	image->fill(p_color);
	return image;
}

Ref<Image> TestFixedFunctionTexturePartialUpdates::_make_patch() {
	Ref<Image> image;
	image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 1));
	image->set_pixel(1, 1, Color(1, 0, 0));
	image->set_pixel(2, 1, Color(0, 1, 0));
	image->set_pixel(1, 2, Color(0, 0, 1));
	image->set_pixel(2, 2, Color(1, 1, 1));
	return image;
}

bool TestFixedFunctionTexturePartialUpdates::_check_snapshot(FixedFunctionTextureStorage &p_storage, RID p_texture, Image::Format p_format, const uint32_t *p_expected) {
	Ref<Image> snapshot = p_storage.texture_get_data(p_texture);
	FIXED_FUNCTION_PARTIAL_CHECK(snapshot.is_valid() && snapshot->get_format() == p_format);
	FIXED_FUNCTION_PARTIAL_CHECK(snapshot->get_width() == TEXTURE_SIZE && snapshot->get_height() == TEXTURE_SIZE && !snapshot->has_mipmaps());
	uint32_t actual[TEXTURE_SIZE * TEXTURE_SIZE];
	for (int y = 0; y < TEXTURE_SIZE; y++) {
		for (int x = 0; x < TEXTURE_SIZE; x++) {
			actual[y * TEXTURE_SIZE + x] = snapshot->get_pixel(x, y).to_argb32();
		}
	}
	for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; i++) {
		FIXED_FUNCTION_PARTIAL_CHECK(actual[i] == p_expected[i]);
	}
	return true;
}

bool TestFixedFunctionTexturePartialUpdates::_render_and_check(FixedFunctionRenderContext &p_context, RID p_texture, const uint32_t *p_expected, std::vector<uint32_t> &r_pixels) {
	FixedFunctionGraphicsDevice &device = p_context.get_device();
	RID native;
	FIXED_FUNCTION_PARTIAL_CHECK(p_context.get_texture_storage().texture_get_device_texture(p_texture, native) == OK);
	const FixedFunctionVertex vertices[] = {
		{ -1, 1, 0, 0xffffffff, 0, 0 },
		{ 1, 1, 0, 0xffffffff, 1, 0 },
		{ 1, -1, 0, 0xffffffff, 1, 1 },
		{ -1, 1, 0, 0xffffffff, 0, 0 },
		{ 1, -1, 0, 0xffffffff, 1, 1 },
		{ -1, -1, 0, 0xffffffff, 0, 1 },
	};
	FIXED_FUNCTION_PARTIAL_CHECK(p_context.begin_frame(0) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(p_context.begin_device_frame(0xff182430) == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
	FIXED_FUNCTION_PARTIAL_CHECK(device.draw_triangles(vertices, 6, Projection(), native, state) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(p_context.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_PARTIAL_CHECK(device.get_width() == CAPTURE_WIDTH && device.get_height() == CAPTURE_HEIGHT);
	FIXED_FUNCTION_PARTIAL_CHECK(device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			const uint32_t source = p_expected[(y * TEXTURE_SIZE / CAPTURE_HEIGHT) * TEXTURE_SIZE + x * TEXTURE_SIZE / CAPTURE_WIDTH];
			const uint32_t actual = r_pixels[size_t(y) * CAPTURE_WIDTH + x];
			const uint32_t alpha = source >> 24;
			const uint32_t background = 0xff182430;
			FIXED_FUNCTION_PARTIAL_CHECK((actual >> 24) == 255);
			for (int shift = 0; shift < 24; shift += 8) {
				const uint32_t expected = (((source >> shift) & 255) * alpha + ((background >> shift) & 255) * (255 - alpha) + 127) / 255;
				const int delta = int((actual >> shift) & 255) - int(expected);
				FIXED_FUNCTION_PARTIAL_CHECK(delta >= -8 && delta <= 8);
			}
		}
	}
	return true;
}

bool TestFixedFunctionTexturePartialUpdates::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	RendererCompositor *const original_compositor = RendererCompositor::get_singleton();
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FIXED_FUNCTION_PARTIAL_CHECK(context.initialize() == OK);
	FixedFunctionTextureStorage &storage = context.get_texture_storage();
	const RID texture = storage.texture_create();
	const RID proxy = storage.texture_create();
	const RID target = storage.render_target_create();
	Ref<Image> patch = _make_patch();
	const Rect2i crop(1, 1, 2, 2);
	const Point2i destination(3, 2);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_proxy(proxy, texture) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, crop, destination) == ERR_UNCONFIGURED);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data(texture, _make_image(Image::FORMAT_RGBA8, Color(24 / 255.0f, 36 / 255.0f, 48 / 255.0f))) == OK);
	uint32_t expected[TEXTURE_SIZE * TEXTURE_SIZE];
	for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; i++) {
		expected[i] = 0xff182430;
	}
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(proxy, patch, crop, destination) == ERR_UNCONFIGURED);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(storage.render_target_get_texture(target), patch, crop, destination) == ERR_ALREADY_IN_USE);
	RID native_before;
	RID native_after;
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_get_device_texture(texture, native_before) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, Ref<Image>(), crop, destination) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, Rect2i(-1, 1, 2, 2), destination) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, Rect2i(3, 1, 2, 2), destination) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, crop, Point2i(7, 2)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, Rect2i(INT_MAX, 0, 2, 2), destination) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, crop, Point2i(INT_MAX, 0)) == ERR_INVALID_PARAMETER);
	Vector<uint8_t> compressed_data;
	compressed_data.resize(8);
	Ref<Image> compressed;
	compressed = Image::create_from_data(4, 4, false, Image::FORMAT_DXT1, compressed_data);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, compressed, crop, destination) == ERR_UNAVAILABLE);
	Ref<Image> wrong_format = _make_image(Image::FORMAT_RGB8, Color(1, 0, 0));
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, wrong_format, crop, destination) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_get_device_texture(texture, native_after) == OK && native_after == native_before);
	FIXED_FUNCTION_PARTIAL_CHECK(_check_snapshot(storage, texture, Image::FORMAT_RGBA8, expected));
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_sampler_flags(texture, FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, crop, destination) == OK);
	expected[2 * TEXTURE_SIZE + 3] = 0xffff0000;
	expected[2 * TEXTURE_SIZE + 4] = 0xff00ff00;
	expected[3 * TEXTURE_SIZE + 3] = 0xff0000ff;
	expected[3 * TEXTURE_SIZE + 4] = 0xffffffff;
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_get_sampler_flags(proxy) == FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_get_proxy(proxy) == texture && storage.texture_size_with_proxy(proxy) == Size2i(TEXTURE_SIZE, TEXTURE_SIZE));
	patch->fill(Color(1, 0, 1));
	FIXED_FUNCTION_PARTIAL_CHECK(_check_snapshot(storage, texture, Image::FORMAT_RGBA8, expected));
	FIXED_FUNCTION_PARTIAL_CHECK(_render_and_check(context, proxy, expected, r_pixels));
	patch = _make_patch();
	FIXED_FUNCTION_PARTIAL_CHECK(patch->generate_mipmaps() == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, crop, destination) == OK && patch->has_mipmaps());
	FIXED_FUNCTION_PARTIAL_CHECK(context.begin_frame(0) == OK && context.begin_device_frame(0) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_get_device_texture(texture, native_before) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, crop, Point2i(1, 5)) == ERR_BUSY);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, Rect2i(4, 4, 0, 0), Point2i(TEXTURE_SIZE, TEXTURE_SIZE)) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_get_device_texture(texture, native_after) == OK && native_after == native_before);
	FIXED_FUNCTION_PARTIAL_CHECK(context.end_frame(false) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(_check_snapshot(storage, texture, Image::FORMAT_RGBA8, expected));
	Ref<Image> mipped = _make_image(Image::FORMAT_RGBA8, Color(0, 0, 1));
	FIXED_FUNCTION_PARTIAL_CHECK(mipped->generate_mipmaps() == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data(texture, mipped) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_get_device_texture(texture, native_before) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, crop, destination) == ERR_UNAVAILABLE);
	Ref<Image> retained_mips = storage.texture_get_data(texture);
	FIXED_FUNCTION_PARTIAL_CHECK(retained_mips->has_mipmaps());
	const Vector<uint8_t> old_bytes = mipped->get_data();
	const Vector<uint8_t> retained_bytes = retained_mips->get_data();
	FIXED_FUNCTION_PARTIAL_CHECK(retained_bytes.size() == old_bytes.size());
	{
		const uint8_t *old_read = old_bytes.ptr();
		const uint8_t *retained_read = retained_bytes.ptr();
		for (int i = 0; i < old_bytes.size(); i++) {
			FIXED_FUNCTION_PARTIAL_CHECK(old_read[i] == retained_read[i]);
		}
	}
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_get_device_texture(texture, native_after) == OK && native_after == native_before);
	// RGB8 and LA8 retain their CPU formats while partial uploads use the RGBA8 device path.
	const Image::Format formats[] = { Image::FORMAT_RGB8, Image::FORMAT_LA8 };
	for (int format = 0; format < 2; format++) {
		Ref<Image> image = _make_image(formats[format], Color(1, 1, 1));
		Ref<Image> part = _make_image(formats[format], format == 0 ? Color(1, 0, 0) : Color(1, 1, 1, 128 / 255.0f));
		FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data(texture, image) == OK);
		FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, part, Rect2i(0, 0, 2, 2), Point2i(1, 1)) == OK);
		for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; i++) {
			expected[i] = 0xffffffff;
		}
		for (int y = 1; y < 3; y++) {
			for (int x = 1; x < 3; x++) {
				expected[y * TEXTURE_SIZE + x] = format == 0 ? 0xffff0000 : 0x80ffffff;
			}
		}
		FIXED_FUNCTION_PARTIAL_CHECK(_check_snapshot(storage, texture, formats[format], expected));
		FIXED_FUNCTION_PARTIAL_CHECK(_render_and_check(context, proxy, expected, r_pixels));
	}
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data(texture, _make_image(Image::FORMAT_RGBA8, Color(0, 0, 0))) == OK);
	for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; i++) {
		expected[i] = 0xff000000;
	}
	patch = _make_patch();
	FIXED_FUNCTION_PARTIAL_CHECK(context.resize(160, 120) == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, crop, destination) == ERR_BUSY);
	FIXED_FUNCTION_PARTIAL_CHECK(_check_snapshot(storage, texture, Image::FORMAT_RGBA8, expected));
	FIXED_FUNCTION_PARTIAL_CHECK(context.prepare_resources() == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, patch, crop, destination) == OK);
	expected[2 * TEXTURE_SIZE + 3] = 0xffff0000;
	expected[2 * TEXTURE_SIZE + 4] = 0xff00ff00;
	expected[3 * TEXTURE_SIZE + 3] = 0xff0000ff;
	expected[3 * TEXTURE_SIZE + 4] = 0xffffffff;
	// A source snapshot may share CPU data; patching must not modify it or untouched texels.
	Ref<Image> shared = storage.texture_get_data(texture);
	FIXED_FUNCTION_PARTIAL_CHECK(storage.texture_set_data_partial(texture, shared, Rect2i(3, 2, 2, 2), Point2i(5, 5)) == OK);
	expected[5 * TEXTURE_SIZE + 5] = 0xffff0000;
	expected[5 * TEXTURE_SIZE + 6] = 0xff00ff00;
	expected[6 * TEXTURE_SIZE + 5] = 0xff0000ff;
	expected[6 * TEXTURE_SIZE + 6] = 0xffffffff;
	shared->fill(Color(1, 0, 1));
	FIXED_FUNCTION_PARTIAL_CHECK(_check_snapshot(storage, texture, Image::FORMAT_RGBA8, expected));
	FIXED_FUNCTION_PARTIAL_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK && context.prepare_resources() == OK);
	FIXED_FUNCTION_PARTIAL_CHECK(_render_and_check(context, proxy, expected, r_pixels));
	FIXED_FUNCTION_PARTIAL_CHECK(RendererCompositor::get_singleton() == original_compositor);
	printf("PASS: partial texture uploads, strict/overflow-safe regions, snapshots/proxies, RGB8/LA8 conversion, busy rollback/retry, reset recovery and unchanged compositor singleton.\n");
	return true;
}

#undef FIXED_FUNCTION_PARTIAL_CHECK
