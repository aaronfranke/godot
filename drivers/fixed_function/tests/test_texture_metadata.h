#ifndef TEST_FIXED_FUNCTION_TEXTURE_METADATA_H
#define TEST_FIXED_FUNCTION_TEXTURE_METADATA_H

#include "drivers/fixed_function/render_target_manager.h"

#include <vector>

class TestFixedFunctionTextureMetadata {
	static Ref<Image> _make_image();
	static bool _test_flag_mapping();
	static bool _render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RID p_texture, std::vector<uint32_t> &r_pixels, bool p_tile = false);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_TEXTURE_METADATA_H
