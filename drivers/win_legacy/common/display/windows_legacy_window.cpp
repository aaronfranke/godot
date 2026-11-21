#include "windows_legacy_window.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

void WindowsLegacyWindow::initialize_dpi_awareness() {
	// Keep native coordinates and rendering pixels consistent on modern hosts.
	// Old Windows already uses pixels and does not export these functions.
	HMODULE user32 = GetModuleHandleA("user32.dll");
	typedef BOOL(WINAPI * SetDPIContextFunction)(HANDLE);
	const auto set_dpi_context = reinterpret_cast<SetDPIContextFunction>(reinterpret_cast<void *>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
	const bool per_monitor = set_dpi_context && set_dpi_context(reinterpret_cast<HANDLE>(intptr_t(-4))); // Per-monitor V2.
	if (!per_monitor) {
		typedef BOOL(WINAPI * SetDPIAwareFunction)();
		const auto set_dpi_aware = reinterpret_cast<SetDPIAwareFunction>(reinterpret_cast<void *>(GetProcAddress(user32, "SetProcessDPIAware")));
		if (set_dpi_aware) {
			set_dpi_aware();
		}
	}
}
