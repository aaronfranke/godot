#pragma once

#include "core/error_list.h"
#include "core/safe_refcount.h"

#include <windows.h>

// WGL extension entry points are supplied by the graphics driver at runtime.
class GLContextWindowsNT {
	typedef BOOL(WINAPI *SwapIntervalProc)(int);

	HWND _window = nullptr;
	HDC _dc = nullptr;
	HGLRC _context = nullptr;
	SwapIntervalProc _swap_interval = nullptr;
	int _applied_vsync = -1;
	SafeFlag _use_vsync;

	static bool _valid_proc(PROC p_proc);

public:
	Error initialize(HWND p_window, bool p_vsync, bool p_modern = true);
	Error make_current();
	void release_current();
	Error swap_buffers();
	void set_use_vsync(bool p_enabled);
	HDC get_dc() const;
	HGLRC get_context() const;
	~GLContextWindowsNT();
};
