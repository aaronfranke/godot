#include "test_render_context.h"

#include "drivers/fixed_function/renderer_compositor_fixed_function.h"

#include <stdio.h>

#include <type_traits>

#define FIXED_FUNCTION_CONTEXT_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Context FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

static_assert(!std::is_abstract<RendererCompositorFixedFunction>::value, "The restricted compositor must match the branch interface.");

Ref<Image> TestFixedFunctionRenderContext::_make_image() {
	Vector<uint8_t> data;
	data.resize(4 * 4 * 4);
	uint8_t *pixels = data.ptrw();
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			const int offset = (y * 4 + x) * 4;
			if (x < 2) {
				pixels[offset] = y < 2 ? 255 : 0;
			} else {
				pixels[offset] = y < 2 ? 0 : 255;
			}
			pixels[offset + 1] = x < 2 ? 0 : 255;
			pixels[offset + 2] = y >= 2 && x < 2 ? 255 : 0;
			if (y < 2) {
				pixels[offset + 3] = 255;
			} else {
				pixels[offset + 3] = x < 2 ? 128 : 0;
			}
		}
	}
	return Image::create_from_data(4, 4, false, Image::FORMAT_RGBA8, data);
}

bool TestFixedFunctionRenderContext::_read_pixels(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
	r_pixels.resize(size_t(p_device.get_width()) * p_device.get_height());
	FIXED_FUNCTION_CONTEXT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	return true;
}

bool TestFixedFunctionRenderContext::_pixel_matches(const std::vector<uint32_t> &p_pixels, int p_width, int p_x, int p_y, uint32_t p_expected) {
	const uint32_t actual = p_pixels[size_t(p_y) * p_width + p_x];
	for (int shift = 0; shift < 24; shift += 8) {
		const int delta = int((actual >> shift) & 255) - int((p_expected >> shift) & 255);
		if (delta < -3 || delta > 3 || (actual >> 24) != 255) {
			printf("Context pixel (%d, %d): actual=0x%08x expected=0x%08x\n", p_x, p_y, actual, p_expected);
			return false;
		}
	}
	return true;
}

bool TestFixedFunctionRenderContext::_test_stretch_modes(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, const Ref<Image> &p_image, const Color &p_background, std::vector<uint32_t> &r_pixels) {
	Ref<Image> wide = p_image->duplicate();
	wide->resize(8, 4, Image::INTERPOLATE_NEAREST);
	// A white edge stripe distinguishes cropping from nonuniform stretching.
	for (int y = 0; y < 4; y++) {
		wide->set_pixel(0, y, Color(1, 1, 1));
	}
	FIXED_FUNCTION_CONTEXT_CHECK(p_context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	struct StretchCase {
		RSE::SplashStretchMode mode;
		uint32_t corner;
		uint32_t upper;
		uint32_t lower;
	};
	const StretchCase cases[] = {
		{ RSE::SPLASH_STRETCH_MODE_DISABLED, 0xff182430, 0xff182430, 0xff182430 },
		{ RSE::SPLASH_STRETCH_MODE_KEEP, 0xffff0000, 0xffff0000, 0xff0c1298 },
		{ RSE::SPLASH_STRETCH_MODE_KEEP_WIDTH, 0xff182430, 0xffffffff, 0xff182430 },
		{ RSE::SPLASH_STRETCH_MODE_KEEP_HEIGHT, 0xffff0000, 0xffff0000, 0xff0c1298 },
		{ RSE::SPLASH_STRETCH_MODE_COVER, 0xffff0000, 0xffff0000, 0xff0c1298 },
		{ RSE::SPLASH_STRETCH_MODE_IGNORE, 0xffffffff, 0xffffffff, 0xffffffff },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		FIXED_FUNCTION_CONTEXT_CHECK(p_context.prepare_resources() == OK && !p_context.is_frame_active());
		FIXED_FUNCTION_CONTEXT_CHECK(p_context.draw_boot_image(wide, p_background, cases[i].mode, false) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(_read_pixels(p_device, r_pixels));
		if (!_pixel_matches(r_pixels, CAPTURE_WIDTH, 0, 0, cases[i].corner) ||
				!_pixel_matches(r_pixels, CAPTURE_WIDTH, 8, 24, cases[i].upper) ||
				!_pixel_matches(r_pixels, CAPTURE_WIDTH, 8, 88, cases[i].lower)) {
			printf("Context stretch case %u failed.\n", unsigned(i));
			return false;
		}
	}
	FIXED_FUNCTION_CONTEXT_CHECK(p_context.draw_boot_image(wide, p_background, RSE::SplashStretchMode(RSE::SPLASH_STRETCH_MODE_IGNORE + 1), false) == ERR_INVALID_PARAMETER);
	return true;
}

bool TestFixedFunctionRenderContext::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	const uint32_t background = 0xff182430;
	const Color background_color(24 / 255.0f, 36 / 255.0f, 48 / 255.0f);
	const Ref<Image> image = _make_image();
	RendererCompositor *const compositor = RendererCompositor::get_singleton();
	{
		FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
		FIXED_FUNCTION_CONTEXT_CHECK(context.resize(128, 96) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_CONTEXT_CHECK(context.prepare_resources() == ERR_UNCONFIGURED);
		FIXED_FUNCTION_CONTEXT_CHECK(context.initialize() == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_last_error() == OK && context.is_initialized());
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_frame_number() == 1);
		FIXED_FUNCTION_CONTEXT_CHECK(context.prepare_resources() == OK && !context.is_frame_active());
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_frame_number() == 1 && context.get_total_time() == 0 && context.get_frame_delta_time() == 0);
		{
			FixedFunctionRenderContext other(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
			FIXED_FUNCTION_CONTEXT_CHECK(other.initialize() == ERR_ALREADY_IN_USE);
		}
		FIXED_FUNCTION_CONTEXT_CHECK(p_device.get_width() == CAPTURE_WIDTH);
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_frame(0.25) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(!context.is_frame_active());
		FIXED_FUNCTION_CONTEXT_CHECK(context.draw_boot_image(image, background_color, RSE::SPLASH_STRETCH_MODE_DISABLED, false) == ERR_BUSY);
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_device_frame(background) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.prepare_resources() == ERR_BUSY);
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_device_frame(0xffffffff) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.end_frame(false) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_last_error() == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(_read_pixels(p_device, r_pixels));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 128, 0, 0, background));
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_frame(0.5) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_device_frame(background) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.resize(160, 120) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.set_use_vsync(true) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.end_frame(false) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_total_time() == 0.75 && context.get_frame_delta_time() == 0.5 && context.get_frame_number() == 3);
		FIXED_FUNCTION_CONTEXT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == ERR_BUSY);
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.end_frame(false) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(_read_pixels(p_device, r_pixels));
		FIXED_FUNCTION_CONTEXT_CHECK(p_device.get_width() == 160 && p_device.get_height() == 120);
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 160, 80, 60, 0xff000000));

		FIXED_FUNCTION_CONTEXT_CHECK(context.resize(0, 0) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_frame(0.25) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_device_frame(background) == ERR_SKIP);
		FIXED_FUNCTION_CONTEXT_CHECK(!context.is_frame_active());
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_device_frame(background) == ERR_SKIP);
		FIXED_FUNCTION_CONTEXT_CHECK(context.end_frame(false) == ERR_SKIP);
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_last_error() == ERR_SKIP);
		const uint64_t engine_frame = context.get_frame_number();
		const double engine_delta = context.get_frame_delta_time();
		const double engine_time = context.get_total_time();
		FIXED_FUNCTION_CONTEXT_CHECK(context.draw_boot_image(image, background_color, RSE::SPLASH_STRETCH_MODE_DISABLED, false) == ERR_SKIP);
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_frame_number() == engine_frame && context.get_frame_delta_time() == engine_delta && context.get_total_time() == engine_time);
		FIXED_FUNCTION_CONTEXT_CHECK(context.resize(129, 97) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.draw_boot_image(image, background_color, RSE::SPLASH_STRETCH_MODE_DISABLED, false) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(_read_pixels(p_device, r_pixels));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 129, 61, 46, background));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 129, 62, 46, 0xffff0000));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 129, 64, 46, 0xff00ff00));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 129, 62, 48, 0xff0c1298));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 129, 64, 48, background));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 129, 66, 46, background));

		FIXED_FUNCTION_CONTEXT_CHECK(context.resize(80, 120) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.draw_boot_image(image, background_color, RSE::SPLASH_STRETCH_MODE_KEEP, false) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(_read_pixels(p_device, r_pixels));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 80, 8, 8, background));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 80, 8, 28, 0xffff0000));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 80, 72, 28, 0xff00ff00));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 80, 8, 92, 0xff0c1298));

		Ref<Image> filtered;
		filtered = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
		filtered->set_pixel(0, 0, Color(1, 0, 0));
		filtered->set_pixel(1, 0, Color(0, 1, 0));
		filtered->set_pixel(0, 1, Color(0, 0, 1));
		filtered->set_pixel(1, 1, Color(1, 1, 1));
		FIXED_FUNCTION_CONTEXT_CHECK(context.draw_boot_image(filtered, background_color, RSE::SPLASH_STRETCH_MODE_KEEP, false, true) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(_read_pixels(p_device, r_pixels));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 80, 40, 60, p_device.get_capabilities().sampler_filter_linear ? 0xff808383 : 0xffffffff));

		Ref<Image> rgb_image = image->duplicate();
		rgb_image->convert(Image::FORMAT_RGB8);
		FIXED_FUNCTION_CONTEXT_CHECK(context.draw_boot_image(rgb_image, background_color, RSE::SPLASH_STRETCH_MODE_DISABLED, false) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(rgb_image->get_format() == Image::FORMAT_RGB8);
		FIXED_FUNCTION_CONTEXT_CHECK(_read_pixels(p_device, r_pixels));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 80, 38, 58, 0xffff0000));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 80, 40, 60, 0xffffff00));

		FIXED_FUNCTION_CONTEXT_CHECK(_test_stretch_modes(context, p_device, image, background_color, r_pixels));
		FIXED_FUNCTION_CONTEXT_CHECK(context.set_use_vsync(true) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.draw_boot_image(image, background_color, RSE::SPLASH_STRETCH_MODE_KEEP) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_last_error() == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_CONTEXT_CHECK(context.set_use_vsync(false) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.draw_boot_image(image, background_color, RSE::SPLASH_STRETCH_MODE_KEEP, false) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(_read_pixels(p_device, r_pixels));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 128, 8, 8, background));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 128, 24, 8, 0xffff0000));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 128, 104, 8, 0xff00ff00));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 128, 24, 80, 0xff0c1298));
		FIXED_FUNCTION_CONTEXT_CHECK(_pixel_matches(r_pixels, 128, 104, 80, background));
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_frame_number() == engine_frame && context.get_frame_delta_time() == engine_delta);
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_total_time() == engine_time);
		FIXED_FUNCTION_CONTEXT_CHECK(context.finalize() == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_last_error() == OK && !context.is_initialized());
		FIXED_FUNCTION_CONTEXT_CHECK(context.finalize() == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_last_error() == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.initialize() == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.get_last_error() == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_frame(0) == OK);
		FIXED_FUNCTION_CONTEXT_CHECK(context.begin_device_frame(background) == OK);
		// The destructor must close this active frame and release the device.
	}
	FIXED_FUNCTION_CONTEXT_CHECK(p_device.get_width() == 0);
	FIXED_FUNCTION_CONTEXT_CHECK(RendererCompositor::get_singleton() == compositor);
	printf("PASS: render-context frame lifecycle, idle resource preparation, deferred begin, suspension, timing, boot-image placement/blending/conversion, resize, vsync, presentation, and cleanup.\n");
	return true;
}

#undef FIXED_FUNCTION_CONTEXT_CHECK
