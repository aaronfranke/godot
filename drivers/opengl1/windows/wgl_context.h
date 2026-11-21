#pragma once

#include "drivers/opengl1/opengl1_context.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#include <windows.h>

class FixedFunctionWGLContext : public FixedFunctionOpenGL1Context {
	HWND _window;
	HDC _dc;
	HGLRC _context;
	HDC _previous_dc;
	HGLRC _previous_context;
	unsigned int _current_count;
	bool _force_software;

#ifdef WIN_NT4_ENABLED
	static const wchar_t UNICODE_PROC_PROPERTY[];
	static const wchar_t GL_PROC_PROPERTY[];

	static LRESULT CALLBACK _unicode_window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam);
	static Error _preserve_unicode(HWND p_window, WNDPROC p_unicode_proc);
#endif // WIN_NT4_ENABLED
	static bool _is_software(const PIXELFORMATDESCRIPTOR &p_format);
	bool _supports_format(const PIXELFORMATDESCRIPTOR &p_format) const;
	int _choose_format(PIXELFORMATDESCRIPTOR &r_format) const;

public:
	Error initialize(void *p_window) override;
	Error make_current() override;
	Error release_current() override;
	ProcAddress get_proc_address(const char *p_name) const override;
	Error present() override;
	Error finalize() override;

	explicit FixedFunctionWGLContext(bool p_force_software = false);
	~FixedFunctionWGLContext() override;
};
