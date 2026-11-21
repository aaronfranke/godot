#include "gl_context_windows_nt.h"

#include "core/error/error_macros.h"

bool GLContextWindowsNT::_valid_proc(PROC p_proc) {
	const uintptr_t address = reinterpret_cast<uintptr_t>(p_proc);
	// Some ICDs return sentinel values instead of null for unavailable functions.
	return address > 3 && address != UINTPTR_MAX;
}

Error GLContextWindowsNT::initialize(HWND p_window, bool p_vsync) {
	ERR_FAIL_COND_V(_window, ERR_ALREADY_IN_USE);
	_window = p_window;
	_dc = GetDC(_window);
	ERR_FAIL_NULL_V(_dc, ERR_CANT_CREATE);
	_use_vsync.set_to(p_vsync);

	PIXELFORMATDESCRIPTOR format = {};
	format.nSize = sizeof(format);
	format.nVersion = 1;
	format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	format.iPixelType = PFD_TYPE_RGBA;
	format.cColorBits = 24;
	format.cAlphaBits = 8;
	format.cDepthBits = 24;
	format.cStencilBits = 8;
	const int pixel_format = ChoosePixelFormat(_dc, &format);
	if (!pixel_format || !SetPixelFormat(_dc, pixel_format, &format)) {
		return ERR_CANT_CREATE;
	}

	_context = wglCreateContext(_dc);
	if (!_context || make_current() != OK) {
		return ERR_CANT_CREATE;
	}
	const PROC proc = wglGetProcAddress("wglCreateContextAttribsARB");
	if (!_valid_proc(proc)) {
		return ERR_UNAVAILABLE;
	}
	typedef HGLRC(WINAPI * CreateContextAttribsProc)(HDC, HGLRC, const int *);
	const CreateContextAttribsProc create_context = reinterpret_cast<CreateContextAttribsProc>(proc);
	enum {
		CONTEXT_MAJOR_VERSION = 0x2091,
		CONTEXT_MINOR_VERSION = 0x2092,
		CONTEXT_FLAGS = 0x2094,
		CONTEXT_PROFILE_MASK = 0x9126,
		CONTEXT_FORWARD_COMPATIBLE_BIT = 0x0002,
		CONTEXT_CORE_PROFILE_BIT = 0x0001,
	};
	const int attributes[] = {
		CONTEXT_MAJOR_VERSION, 3,
		CONTEXT_MINOR_VERSION, 3,
		CONTEXT_FLAGS, CONTEXT_FORWARD_COMPATIBLE_BIT,
		CONTEXT_PROFILE_MASK, CONTEXT_CORE_PROFILE_BIT,
		0
	};
	HGLRC core_context = create_context(_dc, nullptr, attributes);
	if (!core_context) {
		return ERR_UNAVAILABLE;
	}
	if (!wglMakeCurrent(_dc, core_context)) {
		wglDeleteContext(core_context);
		return ERR_CANT_CREATE;
	}
	wglDeleteContext(_context);
	_context = core_context;
	const PROC swap_interval = wglGetProcAddress("wglSwapIntervalEXT");
	if (_valid_proc(swap_interval)) {
		_swap_interval = reinterpret_cast<SwapIntervalProc>(swap_interval);
	}
	return OK;
}

Error GLContextWindowsNT::make_current() {
	if (!_context) {
		return ERR_UNCONFIGURED;
	}
	if (wglGetCurrentContext() != _context || wglGetCurrentDC() != _dc) {
		return wglMakeCurrent(_dc, _context) ? OK : ERR_CANT_CREATE;
	}
	return OK;
}

void GLContextWindowsNT::release_current() {
	if (_context && wglGetCurrentContext() == _context) {
		ERR_FAIL_COND_MSG(!wglMakeCurrent(nullptr, nullptr), "Could not release the NT OpenGL rendering context.");
	}
}

Error GLContextWindowsNT::swap_buffers() {
	const Error error = make_current();
	if (error != OK) {
		return error;
	}
	// Apply on the context's owning thread. Window API calls may come from the
	// main thread while Compatibility renders on a separate thread.
	const int interval = _use_vsync.is_set() ? 1 : 0;
	if (_applied_vsync != interval) {
		if (!_swap_interval || !_swap_interval(interval)) {
			WARN_PRINT_ONCE("The OpenGL driver could not apply the requested VSync mode.");
		}
		_applied_vsync = interval;
	}
	return SwapBuffers(_dc) ? OK : ERR_CANT_CREATE;
}

void GLContextWindowsNT::set_use_vsync(bool p_enabled) {
	_use_vsync.set_to(p_enabled);
}

HDC GLContextWindowsNT::get_dc() const {
	return _dc;
}

HGLRC GLContextWindowsNT::get_context() const {
	return _context;
}

GLContextWindowsNT::~GLContextWindowsNT() {
	if (_context) {
		release_current();
		if (!wglDeleteContext(_context)) {
			ERR_PRINT("Could not delete the NT OpenGL rendering context.");
		}
	}
	if (_dc) {
		ReleaseDC(_window, _dc);
	}
}
