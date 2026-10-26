#include "test_texture_metadata.h"

#include "drivers/fixed_function/canvas_item_renderer.h"
#include "drivers/fixed_function/rasterizer_storage_fixed_function.h"

#include <stdio.h>
#include <type_traits>

#define FIXED_FUNCTION_METADATA_CHECK(m_condition)                                    \
	do {                                                                              \
		if (!(m_condition)) {                                                         \
			printf("Texture metadata FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                             \
		}                                                                             \
	} while (false)

static_assert(!std::is_abstract<RasterizerStorageFixedFunction>::value, "The restricted storage boundary must be concrete.");

Ref<Image> TestFixedFunctionTextureMetadata::_make_image() {
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

bool TestFixedFunctionTextureMetadata::_test_flag_mapping() {
	const uint32_t bits[] = { VS::TEXTURE_FLAG_FILTER, VS::TEXTURE_FLAG_REPEAT, VS::TEXTURE_FLAG_MIPMAPS, VS::TEXTURE_FLAG_ANISOTROPIC_FILTER, VS::TEXTURE_FLAG_USED_FOR_STREAMING, VS::TEXTURE_FLAG_CONVERT_TO_LINEAR };
	const int bit_count = sizeof(bits) / sizeof(bits[0]);
	for (int profile = 0; profile < 32; profile++) {
		FixedFunctionGraphicsDevice::FFCapabilities caps;
		caps.max_texture_stages = 1;
		caps.sampler_filter_linear = (profile & 1) != 0;
		caps.sampler_repeat = (profile & 2) != 0;
		caps.sampler_address_independent = (profile & 4) != 0;
		caps.sampler_repeat_mirrored = (profile & 8) != 0;
		caps.sampler_max_anisotropy = (profile & 16) ? 4 : 1;
		for (int mask = 0; mask < (1 << bit_count); mask++) {
			uint32_t flags = 0;
			for (int i = 0; i < bit_count; i++) {
				if (mask & (1 << i)) {
					flags |= bits[i];
				}
			}
			uint8_t sampler = 128;
			const Error error = RasterizerStorageFixedFunction::get_sampler_flags(flags, caps, sampler);
			if ((mask & 2) && !caps.sampler_repeat) {
				FIXED_FUNCTION_METADATA_CHECK(error == ERR_UNAVAILABLE && sampler == 128);
			} else {
				uint8_t expected = (mask & 1) && caps.sampler_filter_linear ? FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR : 0;
				if (mask & 2) {
					expected |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V;
				}
				if (mask & 4) {
					expected |= FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
				}
				if ((mask & 8) && caps.sampler_max_anisotropy > 1) {
					expected |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_ANISOTROPIC;
				}
				FIXED_FUNCTION_METADATA_CHECK(error == OK && sampler == expected);
			}
		}
		uint8_t sampler = 128;
		uint8_t mirrored = 0;
		if (caps.sampler_repeat_mirrored || caps.sampler_repeat) {
			mirrored |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V;
		}
		if (caps.sampler_repeat_mirrored) {
			mirrored |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
		}
		FIXED_FUNCTION_METADATA_CHECK(RasterizerStorageFixedFunction::get_sampler_flags(VS::TEXTURE_FLAG_MIRRORED_REPEAT, caps, sampler) == OK && sampler == mirrored);
		FIXED_FUNCTION_METADATA_CHECK(RasterizerStorageFixedFunction::get_sampler_flags(VS::TEXTURE_FLAG_MIRRORED_REPEAT | VS::TEXTURE_FLAG_REPEAT, caps, sampler) == OK && sampler == mirrored);
		sampler = 128;
		FIXED_FUNCTION_METADATA_CHECK(RasterizerStorageFixedFunction::get_sampler_flags(1u << 31, caps, sampler) == ERR_INVALID_PARAMETER && sampler == 128);
	}
	FixedFunctionGraphicsDevice::FFCapabilities uninitialized;
	uint8_t sampler = 128;
	FIXED_FUNCTION_METADATA_CHECK(RasterizerStorageFixedFunction::get_sampler_flags(0, uninitialized, sampler) == ERR_UNCONFIGURED && sampler == 128);
	return true;
}

bool TestFixedFunctionTextureMetadata::_render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RID p_texture, std::vector<uint32_t> &r_pixels, bool p_tile) {
	FixedFunctionCanvasItemRenderer renderer(p_context, p_manager);
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	RasterizerCanvas::Item item;
	RasterizerCanvas::Item::CommandRect *rect = memnew(RasterizerCanvas::Item::CommandRect);
	rect->rect = Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT);
	rect->source = Rect2(0, 0, 16, 16);
	rect->texture = p_texture;
	rect->flags = RasterizerCanvas::CANVAS_RECT_REGION;
	if (p_tile) {
		rect->flags |= RasterizerCanvas::CANVAS_RECT_TILE;
	}
	rect->modulate = Color(1, 1, 1);
	item.commands.push_back(rect);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_size_with_proxy(p_texture) == Size2i(8, 8));
	FIXED_FUNCTION_METADATA_CHECK(p_manager.begin_frame(0) == OK);
	FIXED_FUNCTION_METADATA_CHECK(renderer.render_items(&item, Color(1, 1, 1)) == OK);
	FIXED_FUNCTION_METADATA_CHECK(p_manager.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_METADATA_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	const uint32_t colors[] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };
	const uint8_t sampler = storage.texture_get_sampler_flags(p_texture);
	const bool linear = (sampler & FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR) != 0;
	const bool repeat = (sampler & FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U) || (p_tile && p_context.get_device().get_capabilities().sampler_repeat);
	const bool mirror = sampler & FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
	const auto address = [repeat, mirror](int p_texel) {
		if (!repeat) {
			return CLAMP(p_texel, 0, 1);
		}
		const int index = Math::posmod(p_texel, mirror ? 4 : 2);
		return mirror ? MIN(index, 3 - index) : index;
	};
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			// The source rectangle covers twice the overridden logical dimensions.
			// Reference the original 2x2 base level, even when a mip chain was supplied.
			const double sx = (double(x) + 0.5) * 4 / double(CAPTURE_WIDTH) - (linear ? 0.5 : 0);
			const double sy = (double(y) + 0.5) * 4 / double(CAPTURE_HEIGHT) - (linear ? 0.5 : 0);
			const int left = int(Math::floor(sx));
			const int top = int(Math::floor(sy));
			const double fx = linear ? sx - left : 0;
			const double fy = linear ? sy - top : 0;
			const uint32_t actual = r_pixels[size_t(y) * CAPTURE_WIDTH + x];
			FIXED_FUNCTION_METADATA_CHECK((actual >> 24) == 255);
			for (int shift = 0; shift < 24; shift += 8) {
				double expected = 0;
				for (int row = 0; row < 2; row++) {
					for (int column = 0; column < 2; column++) {
						const int tx = address(left + column);
						const int ty = address(top + row);
						expected += ((colors[ty * 2 + tx] >> shift) & 255) * (column ? fx : 1 - fx) * (row ? fy : 1 - fy);
					}
				}
				const int delta = int((actual >> shift) & 255) - int(expected + 0.5);
				FIXED_FUNCTION_METADATA_CHECK(delta >= -8 && delta <= 8);
			}
		}
	}
	return true;
}

bool TestFixedFunctionTextureMetadata::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	FIXED_FUNCTION_METADATA_CHECK(_test_flag_mapping());
	RasterizerStorage *const singleton = RasterizerStorage::base_singleton;
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FixedFunctionRenderTargetManager manager(context);
	FIXED_FUNCTION_METADATA_CHECK(context.initialize() == OK);
	FixedFunctionTextureStorage &storage = context.get_texture_storage();
	const RID texture = storage.texture_create();
	const RID proxy = storage.texture_create();
	const RID target = storage.render_target_create();
	const RID target_texture = storage.render_target_get_texture(target);
	uint32_t flags = VS::TEXTURE_FLAGS_DEFAULT | VS::TEXTURE_FLAG_ANISOTROPIC_FILTER | VS::TEXTURE_FLAG_USED_FOR_STREAMING;
	if (!p_device.get_capabilities().sampler_repeat) {
		flags &= ~VS::TEXTURE_FLAG_REPEAT;
	}
	uint8_t sampler = 0;
	FIXED_FUNCTION_METADATA_CHECK(RasterizerStorageFixedFunction::get_sampler_flags(flags, p_device.get_capabilities(), sampler) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_path(texture, "res://metadata_test.png") == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_allocate_2d(texture, Size2i(2, 2), Image::FORMAT_RGBA8, flags, sampler) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_size(texture) == Size2i(2, 2));
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_image_size(texture) == Size2i(2, 2));
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_format(texture) == Image::FORMAT_RGBA8 && storage.texture_get_flags(texture) == flags);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_path(texture) == "res://metadata_test.png");
	RID native;
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_device_texture(texture, native) == ERR_UNCONFIGURED && !native.is_valid());
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_size(texture, Size2i(4, 4)) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_size_override(texture, Size2i(8, 8)) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_proxy(proxy, texture) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_path(proxy, "res://wrapper") == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_path(proxy) == "res://wrapper" && storage.texture_get_flags(proxy) == 0);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_sampler_flags(proxy) == sampler);
	Ref<Image> image = _make_image();
	FIXED_FUNCTION_METADATA_CHECK(image->generate_mipmaps() == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_data(texture, image) == OK);
	image->fill(Color(0, 0, 0));
	Ref<Image> snapshot = storage.texture_get_data(texture);
	FIXED_FUNCTION_METADATA_CHECK(snapshot.is_valid() && snapshot->has_mipmaps() && snapshot->get_width() == 2);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_size(texture) == Size2i(8, 8) && storage.texture_get_image_size(texture) == Size2i(2, 2));
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_device_texture(texture, native) == OK);
	const RID original_native = native;
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_allocate_2d(texture, Size2i(0, 2), Image::FORMAT_RGBA8, 0, 0) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_allocate_2d(texture, Size2i(Image::MAX_WIDTH + 1, 2), Image::FORMAT_RGBA8, 0, 0) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_allocate_2d(texture, Size2i(2, 2), Image::FORMAT_MAX, 0, 0) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_allocate_2d(texture, Size2i(2, 2), Image::FORMAT_RGBA8, 0, 128) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_allocate_2d(target_texture, Size2i(2, 2), Image::FORMAT_RGBA8, 0, 0) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_size_override(target_texture, Size2i(8, 8)) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_format(target_texture) == Image::FORMAT_RGB8);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_size_override(texture, Size2i(-1, 8)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_flags(texture, 0, 128) == ERR_INVALID_PARAMETER && storage.texture_get_flags(texture) == flags);
	Ref<Image> mismatched = _make_image();
	mismatched->convert(Image::FORMAT_RGB8);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_data(texture, mismatched) == ERR_INVALID_PARAMETER);
	mismatched->convert(Image::FORMAT_RGBA8);
	mismatched->resize(8, 8);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_data(texture, mismatched) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_device_texture(texture, native) == OK && native == original_native);
	FIXED_FUNCTION_METADATA_CHECK(_render_and_check(context, manager, proxy, r_pixels));
	const std::vector<uint32_t> capture = r_pixels;
	const uint32_t mirror_flags = flags | VS::TEXTURE_FLAG_MIRRORED_REPEAT;
	uint8_t mirror_sampler = 0;
	FIXED_FUNCTION_METADATA_CHECK(RasterizerStorageFixedFunction::get_sampler_flags(mirror_flags, p_device.get_capabilities(), mirror_sampler) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_flags(texture, mirror_flags, mirror_sampler) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_flags(texture) == mirror_flags && storage.texture_get_sampler_flags(proxy) == mirror_sampler);
	FIXED_FUNCTION_METADATA_CHECK(_render_and_check(context, manager, proxy, r_pixels, true));
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_flags(texture, flags, sampler) == OK);
	FIXED_FUNCTION_METADATA_CHECK(context.begin_frame(0) == OK);
	FIXED_FUNCTION_METADATA_CHECK(context.begin_device_frame(0) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_allocate_2d(texture, Size2i(4, 4), Image::FORMAT_RGB8, 0, 0) == ERR_BUSY);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_format(texture) == Image::FORMAT_RGBA8 && storage.texture_get_flags(texture) == flags);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_size(texture) == Size2i(8, 8) && storage.texture_get_image_size(texture) == Size2i(2, 2));
	Ref<Image> retained = storage.texture_get_data(texture);
	FIXED_FUNCTION_METADATA_CHECK(retained.is_valid() && retained->has_mipmaps());
	// Sampler changes are metadata-only, even while the native pass is open.
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_flags(texture, 0, 0) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_flags(texture) == 0 && storage.texture_get_sampler_flags(proxy) == 0);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_device_texture(texture, native) == OK && native == original_native);
	FIXED_FUNCTION_METADATA_CHECK(context.end_frame(false) == OK);
	FIXED_FUNCTION_METADATA_CHECK(_render_and_check(context, manager, proxy, r_pixels));
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_flags(texture, flags, sampler) == OK);
	FIXED_FUNCTION_METADATA_CHECK(_render_and_check(context, manager, proxy, r_pixels));
	FIXED_FUNCTION_METADATA_CHECK(r_pixels == capture);
	FIXED_FUNCTION_METADATA_CHECK(context.resize(160, 120) == OK);
	// Pending recovery allows old backing to be released, but still prevents uploads.
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_allocate_2d(texture, Size2i(4, 4), Image::FORMAT_RGB8, 0, 0) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_data(texture).is_null());
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_format(texture) == Image::FORMAT_RGB8 && storage.texture_get_flags(texture) == 0);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_size(texture) == Size2i(4, 4) && storage.texture_size_with_proxy(proxy) == Size2i(4, 4));
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_path(texture) == "res://metadata_test.png" && storage.texture_get_proxy(proxy) == texture);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_device_texture(texture, native) == ERR_UNCONFIGURED && !native.is_valid());
	mismatched->resize(4, 4);
	mismatched->convert(Image::FORMAT_RGB8);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_data(texture, mismatched) == ERR_BUSY);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_data(texture).is_null());
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_size(texture) == Size2i(4, 4) && storage.texture_get_format(texture) == Image::FORMAT_RGB8);
	FIXED_FUNCTION_METADATA_CHECK(context.prepare_resources() == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_data(texture, mismatched) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_allocate_2d(texture, Size2i(2, 2), Image::FORMAT_RGBA8, flags, sampler) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_data(texture, snapshot) == OK);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_set_size_override(texture, Size2i(8, 8)) == OK);
	FIXED_FUNCTION_METADATA_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FIXED_FUNCTION_METADATA_CHECK(_render_and_check(context, manager, proxy, r_pixels));
	FIXED_FUNCTION_METADATA_CHECK(r_pixels == capture);
	FIXED_FUNCTION_METADATA_CHECK(storage.texture_get_flags(texture) == flags && storage.texture_get_path(texture) == "res://metadata_test.png");
	FIXED_FUNCTION_METADATA_CHECK(RasterizerStorage::base_singleton == singleton);
	printf("PASS: texture declarations, flags/fallbacks, original formats/sizes, logical overrides, paths, proxies, allocation rollback/retry, base-level sampling, and recovery.\n");
	return true;
}

#undef FIXED_FUNCTION_METADATA_CHECK
