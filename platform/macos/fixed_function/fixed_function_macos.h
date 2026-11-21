#pragma once

#include "core/error/error_list.h"
#include "core/math/vector2i.h"

class FixedFunctionGraphicsDeviceOpenGL1;

class FixedFunctionMacOS {
	class Context;
	class Compositor;

	Context *_context = nullptr;
	FixedFunctionGraphicsDeviceOpenGL1 *_device = nullptr;

public:
	Error install(void *p_view, const Size2i &p_size, bool p_vsync);
	void resize(const Size2i &p_size);
	void set_use_vsync(bool p_enabled);
	bool is_using_vsync() const;
	int64_t get_context_handle() const;

	~FixedFunctionMacOS();
};
