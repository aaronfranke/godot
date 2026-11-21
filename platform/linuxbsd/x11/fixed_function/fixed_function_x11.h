#pragma once

#include "core/error/error_list.h"
#include "core/math/vector2i.h"

#ifdef SOWRAP_ENABLED
#include "x11/dynwrappers/xlib-so_wrap.h"
#else
#include <X11/Xutil.h>
#endif // SOWRAP_ENABLED

class FixedFunctionGLXContext;
class FixedFunctionGraphicsDeviceOpenGL1;

class FixedFunctionX11 {
	class Compositor;

	FixedFunctionGLXContext *_context = nullptr;
	FixedFunctionGraphicsDeviceOpenGL1 *_device = nullptr;
	Compositor *_compositor = nullptr;
	::Window _window = None;

public:
	explicit FixedFunctionX11(Display *p_display);
	~FixedFunctionX11();

	Error choose_visual(XVisualInfo &r_visual);
	Error install(::Window p_window, const Size2i &p_size, bool p_vsync);
	void resize(const Size2i &p_size);
	void set_use_vsync(bool p_enabled);
	bool is_using_vsync() const;
	int64_t get_context_handle() const;
	VisualID get_visual_id() const;
};
