#pragma once

#include "core/error/error_list.h"

// The platform owns the window. The provider owns the graphics context and
// restores any context displaced by an outermost make_current/release_current pair.
class FixedFunctionOpenGL1Context {
public:
	typedef void (*ProcAddress)();

	virtual Error initialize(void *p_window) = 0;
	virtual Error make_current() = 0;
	virtual Error release_current() = 0;
	// Resolve optional GL entry points only while this provider's context is current.
	virtual ProcAddress get_proc_address(const char *p_name) const = 0;
	virtual Error present() = 0;
	virtual Error finalize() = 0;
	virtual ~FixedFunctionOpenGL1Context() {}
};
