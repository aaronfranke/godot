/**************************************************************************/
/*  fixed_function_x11.cpp                                                */
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

#include "fixed_function_x11.h"

#include "core/os/os.h"
#include "drivers/fixed_function/rasterizer_fixed_function.h"
#include "drivers/opengl1/graphics_device_opengl1.h"

#include <GL/glx.h>

#include <cstring>

class FixedFunctionGLXContext : public FixedFunctionOpenGL1Context {
	Display *_display;
	XVisualInfo _visual = {};
	::Window _window = None;
	GLXContext _context = nullptr;
	int _current_count = 0;

	Display *_previous_display = nullptr;
	GLXContext _previous_context = nullptr;
	GLXDrawable _previous_draw = None;
	GLXDrawable _previous_read = None;

	PFNGLXSWAPINTERVALEXTPROC _swap_interval_ext = nullptr;
	PFNGLXSWAPINTERVALMESAPROC _swap_interval_mesa = nullptr;
	bool _vsync = true;
	bool _update_swap_interval = true;

	static bool _context_creation_failed;
	static int _handle_context_error(Display *, XErrorEvent *) {
		_context_creation_failed = true;
		return 0;
	}

	static bool _has_extension(const char *p_extensions, const char *p_name) {
		if (!p_extensions) {
			return false;
		}
		const size_t length = strlen(p_name);
		for (const char *match = strstr(p_extensions, p_name); match; match = strstr(match + length, p_name)) {
			if ((match == p_extensions || match[-1] == ' ') && (match[length] == ' ' || match[length] == '\0')) {
				return true;
			}
		}
		return false;
	}

public:
	explicit FixedFunctionGLXContext(Display *p_display) :
			_display(p_display) {}

	Error choose_visual(XVisualInfo &r_visual) {
		int error_base, event_base;
		ERR_FAIL_COND_V_MSG(!glXQueryExtension(_display, &error_base, &event_base), ERR_UNAVAILABLE, "OpenGL1 requires GLX on the X11 display.");
		int attributes[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1, GLX_DEPTH_SIZE, 16, None };
		XVisualInfo *visual = glXChooseVisual(_display, DefaultScreen(_display), attributes);
		ERR_FAIL_NULL_V_MSG(visual, ERR_UNAVAILABLE, "OpenGL1 could not find a double-buffered GLX visual with a depth buffer.");
		_visual = *visual;
		r_visual = *visual;
		XFree(visual);
		return OK;
	}

	Error initialize(void *p_window) override {
		_window = *static_cast<::Window *>(p_window);
		// Trap GLX protocol errors before the X11 event thread is started.
		XSync(_display, False);
		_context_creation_failed = false;
		XErrorHandler previous_handler = XSetErrorHandler(_handle_context_error);
		_context = glXCreateContext(_display, &_visual, nullptr, True);
		XSync(_display, False);
		if (_context_creation_failed && _context) {
			glXDestroyContext(_display, _context);
			XSync(_display, False);
			_context = nullptr;
		}
		XSetErrorHandler(previous_handler);
		ERR_FAIL_NULL_V_MSG(_context, ERR_CANT_CREATE, "Could not create the legacy GLX context for OpenGL1.");

		const char *extensions = glXQueryExtensionsString(_display, _visual.screen);
		if (_has_extension(extensions, "GLX_EXT_swap_control")) {
			_swap_interval_ext = reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(get_proc_address("glXSwapIntervalEXT"));
		}
		if (!_swap_interval_ext && glXIsDirect(_display, _context) && _has_extension(extensions, "GLX_MESA_swap_control")) {
			_swap_interval_mesa = reinterpret_cast<PFNGLXSWAPINTERVALMESAPROC>(get_proc_address("glXSwapIntervalMESA"));
		}
		return OK;
	}

	Error make_current() override {
		ERR_FAIL_NULL_V(_context, ERR_UNCONFIGURED);
		if (_current_count == 0) {
			GLXContext previous_context = glXGetCurrentContext();
			const GLXDrawable previous_draw = glXGetCurrentDrawable();
			const GLXDrawable previous_read = previous_context ? glXGetCurrentReadDrawable() : None;
			if (previous_context != _context || previous_draw != _window || previous_read != _window) {
				Display *previous_display = previous_context ? glXGetCurrentDisplay() : nullptr;
				ERR_FAIL_COND_V(!glXMakeCurrent(_display, _window, _context), ERR_CANT_CREATE);
				_previous_context = previous_context;
				_previous_display = previous_display;
				_previous_draw = previous_draw;
				_previous_read = previous_read;
			}
		}
		++_current_count;
		return OK;
	}

	Error release_current() override {
		ERR_FAIL_COND_V(_current_count == 0, ERR_UNCONFIGURED);
		if (--_current_count > 0 || !_previous_context) {
			// Keep our context current when no other context was displaced.
			return OK;
		}
		const Bool restored = _previous_draw == _previous_read
				? glXMakeCurrent(_previous_display, _previous_draw, _previous_context)
				: glXMakeContextCurrent(_previous_display, _previous_draw, _previous_read, _previous_context);
		ERR_FAIL_COND_V(!restored, FAILED);
		_previous_context = nullptr;
		return OK;
	}

	ProcAddress get_proc_address(const char *p_name) const override {
		return glXGetProcAddressARB(reinterpret_cast<const GLubyte *>(p_name));
	}

	Error present() override {
		if (_update_swap_interval) {
			if (_swap_interval_ext) {
				_swap_interval_ext(_display, _window, _vsync ? 1 : 0);
			} else if (_swap_interval_mesa) {
				_swap_interval_mesa(_vsync ? 1 : 0);
			}
			_update_swap_interval = false;
		}
		glXSwapBuffers(_display, _window);
		return OK;
	}

	Error finalize() override {
		ERR_FAIL_COND_V(_current_count != 0, ERR_BUSY);
		if (_context) {
			if (glXGetCurrentContext() == _context && !glXMakeCurrent(_display, None, nullptr)) {
				return FAILED;
			}
			glXDestroyContext(_display, _context);
			_context = nullptr;
		}
		_previous_context = nullptr;
		return OK;
	}

	void set_use_vsync(bool p_enabled) {
		if (_vsync != p_enabled) {
			_vsync = p_enabled;
			_update_swap_interval = true;
		}
	}

	GLXContext get_context() const { return _context; }
};

bool FixedFunctionGLXContext::_context_creation_failed = false;

class FixedFunctionX11::RasterizerRoot : public RasterizerFixedFunction {
	FixedFunctionX11 &_owner;
	static RasterizerRoot *_pending;

	static Rasterizer *_take() {
		ERR_FAIL_NULL_V(_pending, nullptr);
		RasterizerRoot *rasterizer = _pending;
		_pending = nullptr;
		return rasterizer;
	}

public:
	RasterizerRoot(FixedFunctionX11 &p_owner, const Size2i &p_size, bool p_vsync) :
			RasterizerFixedFunction(*p_owner._device, &p_owner._window, p_size.width, p_size.height, p_vsync),
			_owner(p_owner) {}

	void bind() {
		_pending = this;
		_create_func = _take;
	}

	static void unbind() {
		if (_create_func == _take) {
			_create_func = nullptr;
		}
	}

	void initialize() override {
		if (!is_initialized()) {
			RasterizerFixedFunction::initialize();
		}
	}

	~RasterizerRoot() override {
		if (_pending == this) {
			_pending = nullptr;
		}
		_owner._rasterizer = nullptr;
	}

	static bool is_pending(const RasterizerRoot *p_rasterizer) { return _pending == p_rasterizer; }
};

FixedFunctionX11::RasterizerRoot *FixedFunctionX11::RasterizerRoot::_pending = nullptr;

FixedFunctionX11::FixedFunctionX11(Display *p_display) :
		_context(memnew(FixedFunctionGLXContext(p_display))) {}

FixedFunctionX11::~FixedFunctionX11() {
	// Once taken by the VisualServer, the rasterizer is destroyed before this binding.
	if (_rasterizer && RasterizerRoot::is_pending(_rasterizer)) {
		memdelete(_rasterizer);
	}
	ERR_FAIL_COND(_rasterizer);
	if (_device) {
		RasterizerRoot::unbind();
		RasterizerStorage::base_singleton = nullptr;
		memdelete(_device);
	}
	memdelete(_context);
}

Error FixedFunctionX11::choose_visual(XVisualInfo &r_visual) {
	return _context->choose_visual(r_visual);
}

Error FixedFunctionX11::install(::Window p_window, const Size2i &p_size, bool p_vsync) {
	ERR_FAIL_COND_V(_device || VisualServer::get_singleton() || RasterizerStorage::base_singleton, ERR_ALREADY_IN_USE);
	_window = p_window;
	uint8_t features = FixedFunctionGraphicsDeviceOpenGL1::FEATURE_ALL;
	if (OS::get_singleton()->get_cmdline_args().find("--opengl1-disable-npot")) {
		features &= ~FixedFunctionGraphicsDeviceOpenGL1::FEATURE_NPOT_TEXTURE;
	}
	_device = memnew(FixedFunctionGraphicsDeviceOpenGL1(*_context, features));
	_context->set_use_vsync(p_vsync);
	_rasterizer = memnew(RasterizerRoot(*this, p_size, p_vsync));
	_rasterizer->initialize();
	const Error error = _rasterizer->get_last_error();
	if (error != OK) {
		memdelete(_rasterizer);
		return error;
	}
	_rasterizer->bind();
	return OK;
}

void FixedFunctionX11::resize(const Size2i &p_size) {
	if (_rasterizer) {
		ERR_FAIL_COND_MSG(_rasterizer->resize(p_size.width, p_size.height) != OK, "Could not resize the Fixed Function backbuffer.");
	}
}

void FixedFunctionX11::set_use_vsync(bool p_enabled) {
	_context->set_use_vsync(p_enabled);
}

void *FixedFunctionX11::get_context_handle() const {
	return _context->get_context();
}
