#ifndef FIXED_FUNCTION_OPENGL1_HEADERS_H
#define FIXED_FUNCTION_OPENGL1_HEADERS_H

#if defined(WINDOWS_ENABLED)
// The Windows SDK GL header uses Win32 types and calling conventions.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Keep this in a separate block after windows.h: include sorting must not hoist it.
#include <GL/gl.h>
#elif defined(__APPLE__)
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#endif // FIXED_FUNCTION_OPENGL1_HEADERS_H
