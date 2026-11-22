#ifndef TEST_FIXED_FUNCTION_GRAPHICS_DEVICE_OPENGL1_H
#define TEST_FIXED_FUNCTION_GRAPHICS_DEVICE_OPENGL1_H

#include "core/list.h"
#include "core/ustring.h"

class MainLoop;

class TestFixedFunctionGraphicsDeviceOpenGL1 {
public:
	static MainLoop *test(const List<String> &p_args);
};

#endif // TEST_FIXED_FUNCTION_GRAPHICS_DEVICE_OPENGL1_H
