#pragma once

#include "drivers/fixed_function/graphics_device.h"

class TestFixedFunctionVertexSubmission {
public:
	static bool benchmark(FixedFunctionGraphicsDevice &p_device, const char *p_path);
};
