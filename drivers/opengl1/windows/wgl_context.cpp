#include "wgl_context.h"

#include <stdint.h>

FixedFunctionWGLContext::FixedFunctionWGLContext(bool p_force_software) :
		_window(nullptr),
		_dc(nullptr),
		_context(nullptr),
		_previous_dc(nullptr),
		_previous_context(nullptr),
		_current_count(0),
		_force_software(p_force_software) {}

FixedFunctionWGLContext::~FixedFunctionWGLContext() {
	finalize();
}

bool FixedFunctionWGLContext::_is_software(const PIXELFORMATDESCRIPTOR &p_format) {
	return (p_format.dwFlags & PFD_GENERIC_FORMAT) && !(p_format.dwFlags & PFD_GENERIC_ACCELERATED);
}

bool FixedFunctionWGLContext::_supports_format(const PIXELFORMATDESCRIPTOR &p_format) const {
	const DWORD required = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	return (p_format.dwFlags & required) == required && (p_format.dwFlags & PFD_STEREO) == 0 &&
			p_format.iPixelType == PFD_TYPE_RGBA && p_format.cColorBits >= 15 &&
			(!_force_software || _is_software(p_format));
}

int FixedFunctionWGLContext::_choose_format(PIXELFORMATDESCRIPTOR &r_format) const {
	const int count = DescribePixelFormat(_dc, 1, sizeof(r_format), &r_format);
	int best = 0;
	int best_score = -1;
	for (int index = 1; index <= count; index++) {
		PIXELFORMATDESCRIPTOR candidate;
		if (!DescribePixelFormat(_dc, index, sizeof(candidate), &candidate) || !_supports_format(candidate)) {
			continue;
		}
		// Prefer hardware and depth. Favor RGB8 or better and avoid needlessly
		// deep formats when the framebuffer already meets the baseline.
		const int score = (_is_software(candidate) ? 0 : 8) + (candidate.cDepthBits >= 16 ? 4 : 0) + (candidate.cColorBits >= 24 ? 2 : 0);
		if (score > best_score) {
			best = index;
			best_score = score;
			r_format = candidate;
		}
	}
	return best;
}

Error FixedFunctionWGLContext::initialize(void *p_window) {
	if (_dc) {
		return ERR_ALREADY_IN_USE;
	}
	if (!p_window) {
		return ERR_INVALID_PARAMETER;
	}
	_window = static_cast<HWND>(p_window);
	_dc = GetDC(_window);
	if (!_dc) {
		_window = nullptr;
		return ERR_CANT_CREATE;
	}
	PIXELFORMATDESCRIPTOR format;
	const int current = GetPixelFormat(_dc);
	bool valid;
	if (current) {
		// Windows lets the owner set a window's pixel format only once.
		valid = DescribePixelFormat(_dc, current, sizeof(format), &format) && _supports_format(format);
	} else {
		const int chosen = _choose_format(format);
		valid = chosen && SetPixelFormat(_dc, chosen, &format);
	}
	if (!valid) {
		finalize();
		return ERR_UNAVAILABLE;
	}
	_context = wglCreateContext(_dc);
	if (!_context) {
		finalize();
		return ERR_CANT_CREATE;
	}
	return OK;
}

Error FixedFunctionWGLContext::make_current() {
	if (!_context) {
		return ERR_UNCONFIGURED;
	}
	if (_current_count) {
		_current_count++;
		return OK;
	}
	const HGLRC previous = wglGetCurrentContext();
	if (previous != _context || wglGetCurrentDC() != _dc) {
		const HDC previous_dc = wglGetCurrentDC();
		if (!wglMakeCurrent(_dc, _context)) {
			return FAILED;
		}
		if (previous != _context) {
			_previous_context = previous;
			_previous_dc = previous_dc;
		}
	}
	_current_count = 1;
	return OK;
}

Error FixedFunctionWGLContext::release_current() {
	if (!_current_count) {
		return ERR_BUG;
	}
	_current_count--;
	if (_current_count || !_previous_context) {
		// Keep our context current when no other renderer was using this thread.
		return OK;
	}
	if (!wglMakeCurrent(_previous_dc, _previous_context)) {
		return FAILED;
	}
	_previous_context = nullptr;
	_previous_dc = nullptr;
	return OK;
}

FixedFunctionOpenGL1Context::ProcAddress FixedFunctionWGLContext::get_proc_address(const char *p_name) const {
	if (!_context || wglGetCurrentContext() != _context) {
		return nullptr;
	}
	const PROC address = wglGetProcAddress(p_name);
	const uintptr_t value = reinterpret_cast<uintptr_t>(address);
	// Some Windows implementations return sentinel values for missing symbols.
	if (value <= 3 || value == UINTPTR_MAX) {
		return nullptr;
	}
	return reinterpret_cast<ProcAddress>(address);
}

Error FixedFunctionWGLContext::present() {
	return _context && SwapBuffers(_dc) ? OK : FAILED;
}

Error FixedFunctionWGLContext::finalize() {
	if (_current_count) {
		return ERR_BUSY;
	}
	Error error = OK;
	if (_context) {
		if (wglGetCurrentContext() == _context && !wglMakeCurrent(_previous_dc, _previous_context)) {
			if (!wglMakeCurrent(nullptr, nullptr)) {
				return FAILED;
			}
			error = FAILED;
		}
		if (!wglDeleteContext(_context)) {
			return FAILED;
		}
	}
	if (_dc) {
		ReleaseDC(_window, _dc);
	}
	_context = nullptr;
	_dc = nullptr;
	_window = nullptr;
	_previous_dc = nullptr;
	_previous_context = nullptr;
	return error;
}
