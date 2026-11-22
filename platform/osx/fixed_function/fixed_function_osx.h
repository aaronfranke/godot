#ifndef FIXED_FUNCTION_OSX_H
#define FIXED_FUNCTION_OSX_H

#include "core/error_list.h"
#include "core/math/vector2.h"

class FixedFunctionGraphicsDeviceOpenGL1;

class FixedFunctionOSX {
	class Context;
	class RasterizerRoot;

	Context *_context = nullptr;
	FixedFunctionGraphicsDeviceOpenGL1 *_device = nullptr;
	RasterizerRoot *_rasterizer = nullptr;

public:
	Error install(void *p_context, const Size2i &p_size, bool p_vsync);
	void resize(const Size2i &p_size);

	~FixedFunctionOSX();
};

#endif // FIXED_FUNCTION_OSX_H
