#pragma once

#include "drivers/fixed_function/render_context.h"

#include <vector>

class TestFixedFunctionTexturePartialUpdates {
	enum {
		TEXTURE_SIZE = 8,
	};
	static Ref<Image> _make_image(Image::Format p_format, const Color &p_color);
	static Ref<Image> _make_patch();
	static bool _check_snapshot(FixedFunctionTextureStorage &p_storage, RID p_texture, Image::Format p_format, const uint32_t *p_expected);
	static bool _render_and_check(FixedFunctionRenderContext &p_context, RID p_texture, const uint32_t *p_expected, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};
