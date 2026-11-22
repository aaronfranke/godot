#ifndef TEST_FIXED_FUNCTION_STORAGE_QUERIES_H
#define TEST_FIXED_FUNCTION_STORAGE_QUERIES_H

#include "drivers/fixed_function/canvas_item_renderer.h"

#include <vector>

class TestFixedFunctionStorageQueries {
	static void _detect(void *p_userdata);
	static Ref<Image> _image(int p_size);
	static bool _test_callbacks(FixedFunctionTextureStorage &p_storage, RID p_texture, int &r_calls);
	static bool _find_usage(const FixedFunctionTextureStorage &p_storage, RID p_texture, FixedFunctionTextureStorage::FFTextureUsage &r_info);
	static bool _render(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RID p_texture, bool p_previous_frame, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_STORAGE_QUERIES_H
