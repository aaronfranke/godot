#pragma once

#include "drivers/fixed_function/scene_renderer.h"

class TestFixedFunctionSkyHemispheres {
	static bool _test_averages(FixedFunctionTextureStorage &p_textures);

public:
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window);
};
