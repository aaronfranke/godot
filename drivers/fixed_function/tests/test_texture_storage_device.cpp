#include "test_texture_storage_device.h"

#include "test_texture_adaptation.h"

#include <stdio.h>
#include <string.h>

#define FIXED_FUNCTION_STORAGE_DEVICE_CHECK(m_condition)                            \
	do {                                                                            \
		if (!(m_condition)) {                                                       \
			printf("Storage device FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                           \
		}                                                                           \
	} while (false)

const uint32_t TestFixedFunctionTextureStorageDevice::COLORS[4] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };
const uint32_t TestFixedFunctionTextureStorageDevice::FLIPPED_COLORS[4] = { 0xff00ff00, 0xffff0000, 0xffffffff, 0xff0000ff };

Ref<Image> TestFixedFunctionTextureStorageDevice::_make_image() {
	Ref<Image> image;
	image.instance();
	image->create(2, 2, false, Image::FORMAT_RGBA8);
	image->lock();
	image->set_pixel(0, 0, Color(1, 0, 0));
	image->set_pixel(1, 0, Color(0, 1, 0));
	image->set_pixel(0, 1, Color(0, 0, 1));
	image->set_pixel(1, 1, Color(1, 1, 1));
	image->unlock();
	return image;
}

bool TestFixedFunctionTextureStorageDevice::_render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, RID p_texture, const uint32_t *p_colors, std::vector<uint32_t> &r_pixels, float p_uv_span) {
	RID native;
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_context.get_texture_storage().texture_get_device_texture(p_texture, native) == OK);
	const FixedFunctionVertex quad[] = {
		{ -1, 1, 0, 0xffffffff, 0, 0 },
		{ 1, 1, 0, 0xffffffff, p_uv_span, 0 },
		{ 1, -1, 0, 0xffffffff, p_uv_span, p_uv_span },
		{ -1, 1, 0, 0xffffffff, 0, 0 },
		{ 1, -1, 0, 0xffffffff, p_uv_span, p_uv_span },
		{ -1, -1, 0, 0xffffffff, 0, p_uv_span },
	};
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_context.begin_frame(0) == OK);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_context.begin_device_frame(0xff182430) == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.sampler_flags = p_context.get_texture_storage().texture_get_sampler_flags(p_texture);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_device.draw_triangles(quad, 6, CameraMatrix(), native, state) == OK);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_context.end_frame(false) == OK);
	const uint32_t width = p_device.get_width();
	const uint32_t height = p_device.get_height();
	r_pixels.resize(size_t(width) * height);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int i = 0; i < 4; i++) {
		const uint32_t x = (i & 1) ? width * 3 / 4 : width / 4;
		const uint32_t y = (i & 2) ? height * 3 / 4 : height / 4;
		const uint32_t actual = r_pixels[size_t(y) * width + x];
		for (int shift = 0; shift < 32; shift += 8) {
			const int delta = int((actual >> shift) & 255) - int((p_colors[i] >> shift) & 255);
			FIXED_FUNCTION_STORAGE_DEVICE_CHECK(delta >= -3 && delta <= 3);
		}
	}
	return true;
}

bool TestFixedFunctionTextureStorageDevice::_check_snapshot(FixedFunctionTextureStorage &p_storage, RID p_texture, int p_size, Image::Format p_format, bool p_mipmaps, uint32_t p_top_left) {
	Ref<Image> data = p_storage.texture_get_data(p_texture);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(data.is_valid());
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(data->get_width() == p_size && data->get_height() == p_size);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(data->get_format() == p_format && data->has_mipmaps() == p_mipmaps);
	data->lock();
	const uint32_t color = data->get_pixel(0, 0).to_argb32();
	data->unlock();
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(color == p_top_left);
	// Returned Images are independent objects too, not mutable access to the stored snapshot.
	data->fill(Color(0, 0, 0));
	return true;
}

bool TestFixedFunctionTextureStorageDevice::_test_compressed(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, Image::Format p_format) {
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	const RID texture = storage.texture_create();
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_allocate_2d(texture, Size2i(8, 8), p_format, 0, 0) == OK);
	const int block_bytes = Image::get_image_data_size(4, 4, p_format, false);
	const int color_offset = block_bytes - 8;
	PoolVector<uint8_t> data;
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(data.resize(Image::get_image_data_size(8, 8, p_format, true)) == OK);
	{
		PoolVector<uint8_t>::Write write = data.write();
		memset(write.ptr(), 0, data.size()); // All lower mip levels are black.
		const uint16_t colors[] = { 0xf800, 0x07e0, 0x001f, 0xffff };
		for (int i = 0; i < 4; i++) {
			memset(write.ptr() + i * block_bytes, 0xff, color_offset); // Opaque BC2/BC3 alpha.
			write[i * block_bytes + color_offset] = uint8_t(colors[i]);
			write[i * block_bytes + color_offset + 1] = uint8_t(colors[i] >> 8);
		}
	}
	Ref<Image> compressed;
	compressed.instance();
	compressed->create(8, 8, true, p_format, data);
	if (!Image::_image_decompress_bc) {
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_request_data(texture, compressed) == ERR_UNAVAILABLE);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_data(texture).is_null());
		return storage.free(texture) == OK;
	}
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(texture, compressed) == OK);
	std::vector<uint32_t> pixels;
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(p_context, p_device, texture, COLORS, pixels));
	{
		PoolVector<uint8_t>::Write write = data.write();
		for (int i = color_offset; i < block_bytes * 4; i += block_bytes * 2) {
			SWAP(write[i], write[i + block_bytes]);
			SWAP(write[i + 1], write[i + block_bytes + 1]);
		}
	}
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_context.begin_frame(0) == OK && p_context.begin_device_frame(0) == OK);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_request_data(texture, compressed) == OK);
	compressed->create(8, 8, true, p_format, data);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_request_data(texture, compressed) == OK);
	const Ref<Image> accepted = storage.texture_get_data(texture);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(accepted.is_valid() && accepted->get_format() == p_format && accepted->has_mipmaps());
	{
		const PoolVector<uint8_t>::Read bytes = accepted->get_data().read(), expected = data.read();
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(accepted->get_data().size() == data.size() && memcmp(bytes.ptr(), expected.ptr(), data.size()) == 0);
	}
	void (*decoder)(Image *) = Image::_image_decompress_bc;
	Image::_image_decompress_bc = nullptr;
	const Error rejected = storage.texture_request_data(texture, compressed);
	Image::_image_decompress_bc = decoder;
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(rejected == ERR_UNAVAILABLE && storage.get_pending_upload_count() == 1);
	const Ref<Image> patch = _make_image();
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_request_data_partial(texture, patch, Rect2i(0, 0, 2, 2), Point2i()) == ERR_UNAVAILABLE);
	RID native;
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(texture, native) == ERR_SKIP);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_context.end_frame(false) == OK && p_context.prepare_resources() == OK);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(p_context, p_device, texture, FLIPPED_COLORS, pixels));
	FixedFunctionGraphicsDevice::FFTextureInfo info;
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(texture, native) == OK && p_device.texture_get_info(native, info) == OK);
	const bool native_blocks = info.bytes == uint64_t(data.size()) || info.bytes == uint64_t(block_bytes * 4);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(!native_blocks || p_device.supports_compressed_texture(p_format));
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK);
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(p_context, p_device, texture, FLIPPED_COLORS, pixels));
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK && p_context.prepare_resources() == OK && storage.free(texture) == OK);
	printf("PASS: %s texture (%s), preserved compressed source, deferred replacement and reset.\n",
			Image::get_format_name(p_format).utf8().get_data(), native_blocks ? "native blocks" : "CPU decoded");
	return true;
}

bool TestFixedFunctionTextureStorageDevice::_test_mipmaps(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device) {
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = p_device.get_capabilities();
	const int width = 32;
	const int height = caps.texture_square_only ? width : 8;
	const uint8_t mip_flags = FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
	const uint32_t red[] = { 0xffff0000, 0xffff0000, 0xffff0000, 0xffff0000 };
	const uint32_t blue[] = { 0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff };
	const RID single = storage.texture_create();
	Ref<Image> image;
	image.instance();
	image->create(width, height, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 0));
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(single, image) == OK && storage.texture_set_sampler_flags(single, mip_flags) == OK);
	RID native;
	FixedFunctionGraphicsDevice::FFTextureInfo base;
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(single, native) == OK && p_device.texture_get_info(native, base) == OK && base.bytes > 0);
	std::vector<uint32_t> pixels;
	const Image::Format formats[] = { Image::FORMAT_RGB8, Image::FORMAT_DXT1 };
	for (int kind = 0; kind < 2; kind++) {
		if (kind == 1 && !Image::_image_decompress_bc) {
			continue;
		}
		const Image::Format format = formats[kind];
		PoolVector<uint8_t> data;
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(data.resize(Image::get_image_data_size(width, height, format, true)) == OK);
		{
			PoolVector<uint8_t>::Write bytes = data.write();
			memset(bytes.ptr(), 0, data.size());
			int w = width, h = height;
			const int levels = Image::get_image_required_mipmaps(width, height, format) + 1;
			for (int level = 0; level < levels; level++) {
				const int start = Image::get_image_mipmap_offset(width, height, format, level);
				const int size = Image::get_image_data_size(w, h, format, false);
				// Red at level zero, green in the middle, blue at the two smallest levels.
				if (format == Image::FORMAT_RGB8) {
					const int channel = level == 0 ? 0 : 1 + (w <= 2);
					for (int i = 0; i < size; i += 3) {
						bytes[start + i + channel] = 255;
					}
				} else {
					uint16_t color = w <= 2 ? 0x001f : 0x07e0;
					if (level == 0) {
						color = 0xf800;
					}
					for (int i = 0; i < size; i += 8) {
						bytes[start + i] = uint8_t(color);
						bytes[start + i + 1] = uint8_t(color >> 8);
					}
				}
				w = MAX(w >> 1, 1);
				h = MAX(h >> 1, 1);
			}
		}
		image->create(width, height, true, format, data);
		const RID texture = storage.texture_create();
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_request_data(texture, image) == OK && p_context.prepare_resources() == OK);
		FixedFunctionGraphicsDevice::FFTextureInfo info;
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(texture, native) == OK && p_device.texture_get_info(native, info) == OK);
		const bool mipmapped = caps.sampler_mip_filters != 0;
		const uint64_t pixels_in_upload = Image::get_image_data_size(width, height, Image::FORMAT_RGBA8, mipmapped) / 4;
		const uint64_t decoded_bytes = pixels_in_upload * (base.bytes / (width * height));
		const uint64_t native_bytes = Image::get_image_data_size(width, height, format, mipmapped);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(info.bytes == decoded_bytes || (p_device.supports_compressed_texture(format) && info.bytes == native_bytes));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(p_context, p_device, texture, red, pixels, 128));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_sampler_flags(texture, mip_flags) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(p_context, p_device, texture, mipmapped ? blue : red, pixels, 128));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(p_context, p_device, single, red, pixels, 128));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(p_context, p_device, texture, mipmapped ? blue : red, pixels, 128));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK && p_context.prepare_resources() == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.free(texture) == OK);
	}
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.free(single) == OK);
	printf("PASS: supplied mip levels, minification, sampler opt-out, single-level fallback and reset (native mip filtering %s).\n", caps.sampler_mip_filters ? "enabled" : "unavailable");
	return true;
}

bool TestFixedFunctionTextureStorageDevice::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	{
		FixedFunctionRenderContext uninitialized(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
		uninitialized.get_texture_storage().texture_create();
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(uninitialized.finalize() == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(uninitialized.get_texture_storage().get_texture_count() == 0);
	}
	{
		FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
		FixedFunctionTextureStorage &storage = context.get_texture_storage();
		const RID source = storage.texture_create();
		const RID proxy = storage.texture_create();
		const RID target = storage.render_target_create();
		const RID target_texture = storage.render_target_get_texture(target);
		Ref<Image> image = _make_image();
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(source, image) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_data(source).is_null());
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.initialize() == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(TestFixedFunctionTextureAdaptation::test(context, p_device, r_pixels));
		RID native;
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(source, native) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(!native.is_valid());
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_proxy(proxy, source) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(source, image) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_size(source) == Size2i(2, 2));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(proxy, native) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(native.is_valid() && native != source && native != proxy);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(!p_device.texture_is_render_target_ready(native));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(native, native) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(!native.is_valid());
		native = RID(); // Borrowed handles must not survive replacement/free.
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_size(source, Size2i(8, 8)) == ERR_ALREADY_IN_USE);
		image->fill(Color(1, 0, 1));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_check_snapshot(storage, source, 2, Image::FORMAT_RGBA8, false, COLORS[0]));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_check_snapshot(storage, source, 2, Image::FORMAT_RGBA8, false, COLORS[0]));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(context, p_device, proxy, COLORS, r_pixels));

		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(source, Ref<Image>()) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_test_compressed(context, p_device, Image::FORMAT_DXT1));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_test_compressed(context, p_device, Image::FORMAT_DXT3));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_test_compressed(context, p_device, Image::FORMAT_DXT5));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_test_mipmaps(context, p_device));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(target_texture, image) == ERR_ALREADY_IN_USE);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(proxy, native) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(target_texture, native) == (p_device.get_capabilities().render_target_opaque ? ERR_UNCONFIGURED : ERR_UNAVAILABLE));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(!native.is_valid() && !storage.render_target_was_used(target));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(target, native) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(!native.is_valid());
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(context, p_device, proxy, COLORS, r_pixels));

		Ref<Image> replacement = _make_image();
		replacement->flip_x();
		replacement->resize(4, 4, Image::INTERPOLATE_NEAREST);
		replacement->convert(Image::FORMAT_RGB8);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(replacement->generate_mipmaps() == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(source, replacement) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.owns_texture(source) && storage.texture_get_proxy(proxy) == source);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_size_with_proxy(proxy) == Size2i(4, 4));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(replacement->get_format() == Image::FORMAT_RGB8 && replacement->has_mipmaps());
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_check_snapshot(storage, source, 4, Image::FORMAT_RGB8, true, FLIPPED_COLORS[0]));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_check_snapshot(storage, source, 4, Image::FORMAT_RGB8, true, FLIPPED_COLORS[0]));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(context, p_device, proxy, FLIPPED_COLORS, r_pixels));

		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.begin_device_frame(0) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(proxy, native) == OK);
		RID retained = native;
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(source, image) == ERR_BUSY);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.free(source) == ERR_BUSY);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.clear() == ERR_BUSY);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.get_texture_count() == 3 && storage.get_render_target_count() == 1);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_proxy(proxy) == source);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(source, native) == OK && native == retained);
		native = RID();
		retained = RID();
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.end_frame(false) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_check_snapshot(storage, source, 4, Image::FORMAT_RGB8, true, FLIPPED_COLORS[0]));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(context, p_device, proxy, FLIPPED_COLORS, r_pixels));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.resize(160, 120) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(source, image) == ERR_BUSY);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_size_with_proxy(proxy) == Size2i(4, 4));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.prepare_resources() == OK && !context.is_frame_active());
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(source, replacement) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(context, p_device, proxy, FLIPPED_COLORS, r_pixels));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(context, p_device, proxy, FLIPPED_COLORS, r_pixels));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.free(source) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(!storage.texture_get_proxy(proxy).is_valid());
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_get_device_texture(proxy, native) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(!native.is_valid());
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.clear() == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.get_texture_count() == 0 && storage.get_render_target_count() == 0);

		const RID final_texture = storage.texture_create();
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(final_texture, _make_image()) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(_render_and_check(context, p_device, final_texture, COLORS, r_pixels));
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.begin_device_frame(0) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.finalize() == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.get_texture_count() == 0 && p_device.get_width() == 0);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.initialize() == OK);
		const RID last_texture = storage.texture_create();
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(storage.texture_set_data(last_texture, _make_image()) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_STORAGE_DEVICE_CHECK(context.begin_device_frame(0) == OK);
		// Context destruction closes this frame, releases storage textures, then finalizes.
	}
	FIXED_FUNCTION_STORAGE_DEVICE_CHECK(p_device.get_width() == 0);
	printf("PASS: storage-backed draws, stable logical identity, image snapshots/conversion, atomic replacement, busy retries, reset recovery, and context cleanup.\n");
	return true;
}

#undef FIXED_FUNCTION_STORAGE_DEVICE_CHECK
