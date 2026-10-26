#ifndef TEST_FIXED_FUNCTION_TEXTURE_STORAGE_DEVICE_H
#define TEST_FIXED_FUNCTION_TEXTURE_STORAGE_DEVICE_H

#include "drivers/fixed_function/render_context.h"

#include <vector>

class TestFixedFunctionTextureStorageDevice {
	static const uint32_t COLORS[4];
	static const uint32_t FLIPPED_COLORS[4];

	static Ref<Image> _make_image();
	static bool _render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, RID p_texture, const uint32_t *p_colors, std::vector<uint32_t> &r_pixels, float p_uv_span = 1.0f);
	static bool _check_snapshot(FixedFunctionTextureStorage &p_storage, RID p_texture, int p_size, Image::Format p_format, bool p_mipmaps, uint32_t p_top_left);
	static bool _test_compressed(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, Image::Format p_format);
	static bool _test_mipmaps(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};

	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_TEXTURE_STORAGE_DEVICE_H
