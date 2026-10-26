#ifndef TEST_FIXED_FUNCTION_PIXEL_LIGHTING_H
#define TEST_FIXED_FUNCTION_PIXEL_LIGHTING_H

#include "drivers/fixed_function/scene_renderer.h"

class TestFixedFunctionPixelLighting {
	static bool _test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);

public:
	static bool test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
};

#endif // TEST_FIXED_FUNCTION_PIXEL_LIGHTING_H
