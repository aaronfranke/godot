/**************************************************************************/
/*  fixed_function_osx.mm                                                 */
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

#include "fixed_function_osx.h"

#include "core/os/os.h"
#include "drivers/fixed_function/rasterizer_fixed_function.h"
#include "drivers/opengl1/graphics_device_opengl1.h"

#import <AppKit/NSOpenGL.h>
#import <OpenGL/OpenGL.h>
#include <dlfcn.h>

class FixedFunctionOSX::Context : public FixedFunctionOpenGL1Context {
	NSOpenGLContext *_context = nil;
	CGLContextObj _previous_context = nullptr;
	int _current_count = 0;

public:
	Error initialize(void *p_context) override {
		_context = [(NSOpenGLContext *)p_context retain];
		return _context ? OK : ERR_INVALID_PARAMETER;
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
			[_context release];
			_context = nil;
		}
		return OK;
	}
};

class FixedFunctionOSX::RasterizerRoot : public RasterizerFixedFunction {
	FixedFunctionOSX &_owner;
	static RasterizerRoot *_pending;

	static Rasterizer *_take() {
		ERR_FAIL_NULL_V(_pending, nullptr);
		RasterizerRoot *rasterizer = _pending;
		_pending = nullptr;
		return rasterizer;
	}

public:
	RasterizerRoot(FixedFunctionOSX &p_owner, void *p_context, const Size2i &p_size, bool p_vsync) :
			RasterizerFixedFunction(*p_owner._device, p_context, p_size.width, p_size.height, p_vsync),
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

FixedFunctionOSX::RasterizerRoot *FixedFunctionOSX::RasterizerRoot::_pending = nullptr;

Error FixedFunctionOSX::install(void *p_context, const Size2i &p_size, bool p_vsync) {
	ERR_FAIL_COND_V(_device || VisualServer::get_singleton() || RasterizerStorage::base_singleton, ERR_ALREADY_IN_USE);
	uint8_t features = FixedFunctionGraphicsDeviceOpenGL1::FEATURE_ALL;
	if (OS::get_singleton()->get_cmdline_args().find("--opengl1-disable-npot")) {
		features &= ~FixedFunctionGraphicsDeviceOpenGL1::FEATURE_NPOT_TEXTURE;
	}
	_context = memnew(Context);
	_device = memnew(FixedFunctionGraphicsDeviceOpenGL1(*_context, features));
	_rasterizer = memnew(RasterizerRoot(*this, p_context, p_size, p_vsync));
	_rasterizer->initialize();
	const Error error = _rasterizer->get_last_error();
	if (error != OK) {
		memdelete(_rasterizer);
		return error;
	}
	_rasterizer->bind();
	return OK;
}

void FixedFunctionOSX::resize(const Size2i &p_size) {
	if (_rasterizer && _rasterizer->is_initialized()) {
		ERR_FAIL_COND_MSG(_rasterizer->resize(p_size.width, p_size.height) != OK, "Could not resize the Fixed Function backbuffer.");
	}
}

FixedFunctionOSX::~FixedFunctionOSX() {
	if (_rasterizer && RasterizerRoot::is_pending(_rasterizer)) {
		memdelete(_rasterizer);
	}
	ERR_FAIL_COND(_rasterizer);
	if (_device) {
		RasterizerRoot::unbind();
		RasterizerStorage::base_singleton = nullptr;
		memdelete(_device);
		memdelete(_context);
	}
}
