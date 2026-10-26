#pragma once

#if defined(WINDOWS_ENABLED)
// The Windows SDK GL header uses Win32 types and calling conventions.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Keep the GL header in a separate block so include sorting preserves the Win32 dependency.
#include <GL/gl.h>
#elif defined(__APPLE__)
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
