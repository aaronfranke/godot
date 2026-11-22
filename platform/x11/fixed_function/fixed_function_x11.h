#ifndef FIXED_FUNCTION_X11_H
#define FIXED_FUNCTION_X11_H

#include "core/error_list.h"
#include "core/math/vector2.h"

#include <X11/Xutil.h>

#undef CursorShape

class FixedFunctionGLXContext;
class FixedFunctionGraphicsDeviceOpenGL1;

class FixedFunctionX11 {
	class RasterizerRoot;

	FixedFunctionGLXContext *_context = nullptr;
	FixedFunctionGraphicsDeviceOpenGL1 *_device = nullptr;
	RasterizerRoot *_rasterizer = nullptr;
	::Window _window = None;

public:
	explicit FixedFunctionX11(Display *p_display);
	~FixedFunctionX11();

	Error choose_visual(XVisualInfo &r_visual);
	Error install(::Window p_window, const Size2i &p_size, bool p_vsync);
	void resize(const Size2i &p_size);
	void set_use_vsync(bool p_enabled);
	void *get_context_handle() const;
};

#endif // FIXED_FUNCTION_X11_H
