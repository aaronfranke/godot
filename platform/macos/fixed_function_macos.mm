/**************************************************************************/
/*  fixed_function_macos.mm                                               */
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

#include "fixed_function_macos.h"

#include "gl_manager_macos_legacy.h"

#include "core/os/os.h"
#include "drivers/fixed_function/renderer_compositor_fixed_function.h"
#include "drivers/opengl1/graphics_device_opengl1.h"

#include <dlfcn.h>

GODOT_PUSH_IGNORE_DEPRECATION()

class FixedFunctionMacOS::Context : public FixedFunctionOpenGL1Context {
	GLManagerLegacy_MacOS _manager{ true };
	NSOpenGLContext *_context = nil;
	CGLContextObj _previous_context = nullptr;
	int _current_count = 0;
	Size2i _initial_size;
	bool _vsync;

public:
	Context(const Size2i &p_size, bool p_vsync) :
			_initial_size(p_size),
			_vsync(p_vsync) {}

	Error initialize(void *p_view) override {
		Error error = _manager.initialize();
		if (error != OK) {
			return error;
		}
		error = _manager.window_create(DisplayServerEnums::MAIN_WINDOW_ID, (__bridge id)p_view, _initial_size.width, _initial_size.height);
		if (error != OK) {
			return error;
		}
		_context = _manager.get_context(DisplayServerEnums::MAIN_WINDOW_ID);
		resize(_initial_size);
		set_use_vsync(_vsync);
		return OK;
	}

	Error make_current() override {
		ERR_FAIL_NULL_V(_context, ERR_UNCONFIGURED);
		if (_current_count == 0) {
			CGLContextObj previous = CGLGetCurrentContext();
			CGLContextObj context = [_context CGLContextObj];
			if (previous != context && CGLSetCurrentContext(context) != kCGLNoError) {
				return FAILED;
			}
			_previous_context = previous;
		}
		_current_count++;
		return OK;
	}

	Error release_current() override {
		ERR_FAIL_COND_V(_current_count == 0, ERR_BUG);
		if (--_current_count == 0) {
			CGLContextObj previous = _previous_context;
			_previous_context = nullptr;
			if (CGLGetCurrentContext() != previous && CGLSetCurrentContext(previous) != kCGLNoError) {
				return FAILED;
			}
		}
		return OK;
	}

	ProcAddress get_proc_address(const char *p_name) const override {
		return reinterpret_cast<ProcAddress>(dlsym(RTLD_DEFAULT, p_name));
	}

	Error present() override {
		[_context flushBuffer];
		return OK;
	}

	Error finalize() override {
		ERR_FAIL_COND_V(_current_count != 0, ERR_BUSY);
		if (_context) {
			if (CGLGetCurrentContext() == [_context CGLContextObj] && CGLSetCurrentContext(nullptr) != kCGLNoError) {
				return FAILED;
			}
			[_context clearDrawable];
			_manager.window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
			_context = nil;
		}
		return OK;
	}

	void resize(const Size2i &p_size) {
		if (p_size.width > 0 && p_size.height > 0) {
			_manager.window_resize(DisplayServerEnums::MAIN_WINDOW_ID, p_size.width, p_size.height);
		}
	}

	void set_use_vsync(bool p_enabled) {
		_vsync = p_enabled;
		GLint swap_interval = p_enabled ? 1 : 0;
		[_context setValues:&swap_interval forParameter:NSOpenGLContextParameterSwapInterval];
	}

	bool is_using_vsync() const { return _vsync; }
	int64_t get_context_handle() const { return reinterpret_cast<intptr_t>((__bridge void *)_context); }
};

GODOT_POP_IGNORE_DEPRECATION()

class FixedFunctionMacOS::Compositor : public RendererCompositorFixedFunction {
	static Compositor *_pending;

	static RendererCompositor *_take() {
		Compositor *compositor = _pending;
		_pending = nullptr;
		return compositor;
	}

public:
	using RendererCompositorFixedFunction::RendererCompositorFixedFunction;

	void initialize() override {
		if (!is_initialized()) {
			RendererCompositorFixedFunction::initialize();
		}
	}

	void bind() {
		_pending = this;
		make_current(_take);
	}

	static void unbind() {
		if (_pending) {
			memdelete(_pending);
			_pending = nullptr;
		}
		if (_create_func == _take) {
			_create_func = nullptr;
		}
	}
};

FixedFunctionMacOS::Compositor *FixedFunctionMacOS::Compositor::_pending = nullptr;

Error FixedFunctionMacOS::install(void *p_view, const Size2i &p_size, bool p_vsync) {
	ERR_FAIL_COND_V(_device || RendererCompositor::get_singleton(), ERR_ALREADY_IN_USE);
	uint8_t features = FixedFunctionGraphicsDeviceOpenGL1::FEATURE_ALL;
	if (OS::get_singleton()->get_cmdline_user_args().find("--opengl1-disable-npot")) {
		features &= ~FixedFunctionGraphicsDeviceOpenGL1::FEATURE_NPOT_TEXTURE;
	}
	_context = memnew(Context(p_size, p_vsync));
	_device = memnew(FixedFunctionGraphicsDeviceOpenGL1(*_context, features));
	Compositor *compositor = memnew(Compositor(*_device, p_view, p_size.width, p_size.height, p_vsync));
	compositor->initialize();
	const Error error = compositor->get_last_error();
	if (error != OK) {
		memdelete(compositor);
		return error;
	}
	compositor->bind();
	return OK;
}

void FixedFunctionMacOS::resize(const Size2i &p_size) {
	RendererCompositorFixedFunction *compositor = RendererCompositorFixedFunction::get_singleton();
	if (compositor && compositor->is_initialized()) {
		ERR_FAIL_COND_MSG(compositor->resize(p_size.width, p_size.height) != OK, "Could not resize the Fixed Function backbuffer.");
		_context->resize(p_size);
	}
}

void FixedFunctionMacOS::set_use_vsync(bool p_enabled) {
	_context->set_use_vsync(p_enabled);
}

bool FixedFunctionMacOS::is_using_vsync() const {
	return _context->is_using_vsync();
}

int64_t FixedFunctionMacOS::get_context_handle() const {
	return _context->get_context_handle();
}

FixedFunctionMacOS::~FixedFunctionMacOS() {
	if (_device) {
		Compositor::unbind();
		memdelete(_device);
		memdelete(_context);
	}
}
