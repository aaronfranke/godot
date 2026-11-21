#include "wgl_context.h"

#include <stdint.h>

#ifdef WIN_NT4_ENABLED
const wchar_t FixedFunctionWGLContext::UNICODE_PROC_PROPERTY[] = L"GodotWGLUnicodeProc";
const wchar_t FixedFunctionWGLContext::GL_PROC_PROPERTY[] = L"GodotWGLRuntimeProc";

LRESULT CALLBACK FixedFunctionWGLContext::_unicode_window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam) {
	WNDPROC procedure = reinterpret_cast<WNDPROC>(GetPropW(p_window, GL_PROC_PROPERTY));
	switch (p_message) {
		case WM_CHAR:
		case WM_DEADCHAR:
		case WM_SYSCHAR:
		case WM_SYSDEADCHAR:
		case WM_IME_CHAR:
		case WM_SETTEXT:
		case WM_GETTEXT:
		case WM_GETTEXTLENGTH:
			// The graphics wrapper does not need text; avoid its ANSI conversion.
			procedure = reinterpret_cast<WNDPROC>(GetPropW(p_window, UNICODE_PROC_PROPERTY));
			break;
		case WM_NCDESTROY:
			RemovePropW(p_window, UNICODE_PROC_PROPERTY);
			RemovePropW(p_window, GL_PROC_PROPERTY);
			break;
	}
	return CallWindowProcW(procedure, p_window, p_message, p_wparam, p_lparam);
}

Error FixedFunctionWGLContext::_preserve_unicode(HWND p_window, WNDPROC p_unicode_proc) {
	if (!p_unicode_proc || IsWindowUnicode(p_window)) {
		return OK;
	}
	// NT4's GDI SetPixelFormat installs an ANSI window procedure even on a Unicode
	// window. Keep its graphics handling, but receive text through a Unicode entry point.
	const WNDPROC gl_proc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(p_window, GWLP_WNDPROC));
	if (!SetPropW(p_window, UNICODE_PROC_PROPERTY, reinterpret_cast<HANDLE>(p_unicode_proc))) {
		return ERR_CANT_CREATE;
	}
	if (!SetPropW(p_window, GL_PROC_PROPERTY, reinterpret_cast<HANDLE>(gl_proc))) {
		RemovePropW(p_window, UNICODE_PROC_PROPERTY);
		return ERR_CANT_CREATE;
	}
	SetLastError(ERROR_SUCCESS);
	const LONG_PTR previous = SetWindowLongPtrW(p_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(_unicode_window_proc));
	if (!previous && GetLastError() != ERROR_SUCCESS) {
		RemovePropW(p_window, UNICODE_PROC_PROPERTY);
		RemovePropW(p_window, GL_PROC_PROPERTY);
		return ERR_CANT_CREATE;
	}
	// Pixel format and wrapper belong to the native window, across context recreation.
	// Properties hold procedures only, so no destroyed context can be referenced later.
	return OK;
}

#endif // WIN_NT4_ENABLED

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
#ifdef WIN_NT4_ENABLED
	const WNDPROC unicode_proc = IsWindowUnicode(_window) ? reinterpret_cast<WNDPROC>(GetWindowLongPtrW(_window, GWLP_WNDPROC)) : nullptr;
#endif // WIN_NT4_ENABLED
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
#ifdef WIN_NT4_ENABLED
	const Error unicode_error = _preserve_unicode(_window, unicode_proc);
	if (unicode_error != OK) {
		finalize();
		return unicode_error;
	}
#endif // WIN_NT4_ENABLED
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
