#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/os/thread.h"

using namespace DisplayServerEnums;

BOOL CALLBACK DisplayServerWindowsNT::_enum_monitor(HMONITOR p_monitor, HDC, LPRECT, LPARAM p_data) {
	reinterpret_cast<Vector<HMONITOR> *>(p_data)->push_back(p_monitor);
	return TRUE;
}

Vector<HMONITOR> DisplayServerWindowsNT::_get_monitors() {
	Vector<HMONITOR> monitors;
	EnumDisplayMonitors(nullptr, nullptr, _enum_monitor, reinterpret_cast<LPARAM>(&monitors));
	return monitors;
}

HMONITOR DisplayServerWindowsNT::_get_primary_monitor() {
	const POINT origin = {};
	return _get_point_monitor(origin, MONITOR_DEFAULTTOPRIMARY);
}

HMONITOR DisplayServerWindowsNT::_get_window_monitor(HWND p_window, DWORD p_flags) {
	return MonitorFromWindow(p_window, p_flags);
}

HMONITOR DisplayServerWindowsNT::_get_point_monitor(const POINT &p_position, DWORD p_flags) {
	return MonitorFromPoint(p_position, p_flags);
}

bool DisplayServerWindowsNT::_get_monitor_info(HMONITOR p_monitor, MONITORINFO *r_info) {
	return p_monitor && GetMonitorInfoW(p_monitor, r_info);
}

Point2i DisplayServerWindowsNT::_get_screens_origin() {
	return Point2i(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN));
}

HMONITOR DisplayServerWindowsNT::_get_monitor(int p_screen) const {
	switch (p_screen) {
		case SCREEN_PRIMARY:
			return _get_primary_monitor();
		case SCREEN_OF_MAIN_WINDOW:
			return _window ? _get_window_monitor(_window, MONITOR_DEFAULTTONEAREST) : _get_primary_monitor();
		case SCREEN_WITH_KEYBOARD_FOCUS:
			return _get_window_monitor(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
		case SCREEN_WITH_MOUSE_FOCUS: {
			POINT position = {};
			GetCursorPos(&position);
			return _get_point_monitor(position, MONITOR_DEFAULTTONEAREST);
		}
	}
	const Vector<HMONITOR> monitors = _get_monitors();
	return p_screen >= 0 && p_screen < monitors.size() ? monitors[p_screen] : nullptr;
}

Rect2i DisplayServerWindowsNT::_get_monitor_rect(HMONITOR p_monitor, bool p_work_area) {
	MONITORINFO info = {};
	info.cbSize = sizeof(info);
	if (!_get_monitor_info(p_monitor, &info)) {
		return Rect2i();
	}
	const RECT &rect = p_work_area ? info.rcWork : info.rcMonitor;
	return Rect2i(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

int DisplayServerWindowsNT::get_screen_count() const {
	return _get_monitors().size();
}

int DisplayServerWindowsNT::get_primary_screen() const {
	return MAX(0, _get_monitors().find(_get_primary_monitor()));
}

Point2i DisplayServerWindowsNT::screen_get_position(int p_screen) const {
	const Rect2i rect = _get_monitor_rect(_get_monitor(p_screen));
	return rect.has_area() ? rect.position - _get_screens_origin() : Point2i();
}

Size2i DisplayServerWindowsNT::screen_get_size(int p_screen) const {
	return _get_monitor_rect(_get_monitor(p_screen)).size;
}

Rect2i DisplayServerWindowsNT::screen_get_usable_rect(int p_screen) const {
	Rect2i rect = _get_monitor_rect(_get_monitor(p_screen), true);
	if (rect.has_area()) {
		rect.position -= _get_screens_origin();
	}
	return rect;
}

int DisplayServerWindowsNT::screen_get_dpi(int) const {
	HDC dc = GetDC(nullptr);
	if (!dc) {
		return 96;
	}
	const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
	ReleaseDC(nullptr, dc);
	return dpi > 0 ? dpi : 96;
}

float DisplayServerWindowsNT::screen_get_refresh_rate(int p_screen) const {
	const HMONITOR monitor = _get_monitor(p_screen);
	MONITORINFOEXW info = {};
	info.cbSize = sizeof(info);
	if (!_get_monitor_info(monitor, reinterpret_cast<MONITORINFO *>(&info))) {
		return SCREEN_REFRESH_RATE_FALLBACK;
	}
	DEVMODEW mode = {};
	mode.dmSize = sizeof(mode);
	if (EnumDisplaySettingsW(info.szDevice[0] ? info.szDevice : nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1) {
		return mode.dmDisplayFrequency;
	}
	return SCREEN_REFRESH_RATE_FALLBACK;
}

void DisplayServerWindowsNT::screen_set_keep_on(bool p_enable) {
	// The execution state belongs to the thread that makes the request.
	ERR_FAIL_COND(!Thread::is_main_thread());
	if (_keep_screen_on == p_enable) {
		return;
	}

	EXECUTION_STATE state = ES_CONTINUOUS;
	if (p_enable) {
		state |= ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED;
	}
	const bool changed = SetThreadExecutionState(state) != 0;
	if (!changed) {
		ERR_PRINT_ONCE("Could not change the Windows NT screen idle request.");
		return;
	}
	_keep_screen_on = p_enable;
}

bool DisplayServerWindowsNT::screen_is_kept_on() const {
	return _keep_screen_on;
}

int DisplayServerWindowsNT::window_get_current_screen(WindowID p_window) const {
	if (p_window != MAIN_WINDOW_ID) {
		return get_primary_screen();
	}
	const int screen = _get_monitors().find(_get_window_monitor(_window, MONITOR_DEFAULTTONEAREST));
	return screen >= 0 ? screen : get_primary_screen();
}

void DisplayServerWindowsNT::window_set_current_screen(int p_screen, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID || IsIconic(_window)) {
		return;
	}
	const HMONITOR current = _get_window_monitor(_window, MONITOR_DEFAULTTONEAREST);
	const HMONITOR target = _get_monitor(p_screen);
	if (!target || target == current) {
		return;
	}
	if (_fullscreen) {
		if (!_set_fullscreen_bounds(target)) {
			ERR_PRINT_ONCE("Could not move the Windows NT fullscreen window to the requested screen.");
		}
		return;
	}
	const Rect2i work_area = _get_monitor_rect(target, true);
	if (!work_area.has_area()) {
		return;
	}
	const bool maximized = IsZoomed(_window);
	if (maximized) {
		ShowWindow(_window, SW_RESTORE);
	}
	const Rect2i client = _get_client_rect();
	const Point2i offset = client.position - _get_monitor_rect(current, true).position;
	const Size2i room = (work_area.size - client.size).max(Size2i());
	window_set_position(work_area.position - _get_screens_origin() + offset.clamp(Point2i(), room), p_window);
	if (maximized) {
		ShowWindow(_window, SW_MAXIMIZE);
	}
}
