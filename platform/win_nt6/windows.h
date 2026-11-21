#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#ifdef _MSC_VER
// MSVC lacks include_next. Enter the SDK's um directory from an SDK include path.
#include <../um/windows.h>
#else
#include_next <windows.h>
#endif // _MSC_VER

#undef CONNECT_DEFERRED // The SDK macro conflicts with Object::CONNECT_DEFERRED.
