#ifndef TEST_FIXED_FUNCTION_VERTEX_SUBMISSION_H
#define TEST_FIXED_FUNCTION_VERTEX_SUBMISSION_H

#include "drivers/fixed_function/graphics_device.h"

class TestFixedFunctionVertexSubmission {
public:
	static bool benchmark(FixedFunctionGraphicsDevice &p_device, const char *p_path);
};

#endif // TEST_FIXED_FUNCTION_VERTEX_SUBMISSION_H
