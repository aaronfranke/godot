#pragma once

// Use the shared SDK wrapper for WIN32_LEAN_AND_MEAN and conflicting Win32 macros.
#include "platform/windows/windows.h"

// The Microsoft SDK hides these numeric key identifiers below WINVER 0x0500.
// Keep the shared key table buildable without raising our native API baseline.
#ifndef VK_BROWSER_BACK
#define VK_BROWSER_BACK 0xA6
#define VK_BROWSER_FORWARD 0xA7
#define VK_BROWSER_REFRESH 0xA8
#define VK_BROWSER_STOP 0xA9
#define VK_BROWSER_SEARCH 0xAA
#define VK_BROWSER_FAVORITES 0xAB
#define VK_BROWSER_HOME 0xAC
#define VK_VOLUME_MUTE 0xAD
#define VK_VOLUME_DOWN 0xAE
#define VK_VOLUME_UP 0xAF
#define VK_MEDIA_NEXT_TRACK 0xB0
#define VK_MEDIA_PREV_TRACK 0xB1
#define VK_MEDIA_STOP 0xB2
#define VK_MEDIA_PLAY_PAUSE 0xB3
#define VK_LAUNCH_MAIL 0xB4
#define VK_LAUNCH_MEDIA_SELECT 0xB5
#define VK_LAUNCH_APP1 0xB6
#define VK_LAUNCH_APP2 0xB7
#endif // VK_BROWSER_BACK

// Windows 98 monitor APIs are resolved optionally by the D3D7 driver. Modern
// Microsoft headers omit their ABI types when compiling below WINVER 0x0500.
#ifndef MONITOR_DEFAULTTONEAREST
#define MONITOR_DEFAULTTONEAREST 0x00000002
#ifndef HMONITOR_DECLARED
#define HMONITOR_DECLARED
DECLARE_HANDLE(HMONITOR);
#endif // HMONITOR_DECLARED
typedef struct tagMONITORINFO {
	DWORD cbSize;
	RECT rcMonitor;
	RECT rcWork;
	DWORD dwFlags;
} MONITORINFO;
#endif // MONITOR_DEFAULTTONEAREST
