/**************************************************************************/
/*  fixed_function_x11.h                                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

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
