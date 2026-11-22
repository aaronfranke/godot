#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/os/thread.h"

BOOL CALLBACK DisplayServerWindowsNT::_enum_monitor(HMONITOR p_monitor, HDC, LPRECT, LPARAM p_data) {
	reinterpret_cast<Vector<HMONITOR> *>(p_data)->push_back(p_monitor);
	return TRUE;
}

Vector<HMONITOR> DisplayServerWindowsNT::_get_monitors() {
	Vector<HMONITOR> monitors;
	EnumDisplayMonitors(nullptr, nullptr, _enum_monitor, reinterpret_cast<LPARAM>(&monitors));
	return monitors;
}

HMONITOR DisplayServerWindowsNT::_get_window_monitor(HWND p_window, DWORD p_flags) {
	return MonitorFromWindow(p_window, p_flags);
}

bool DisplayServerWindowsNT::_get_monitor_info(HMONITOR p_monitor, MONITORINFO *r_info) {
	return p_monitor && GetMonitorInfoW(p_monitor, r_info);
}

HMONITOR DisplayServerWindowsNT::_get_monitor(int p_screen) const {
	if (p_screen == -1) {
		return _get_window_monitor(_window, MONITOR_DEFAULTTONEAREST);
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

int DisplayServerWindowsNT::get_current_screen() const {
	return MAX(0, _get_monitors().find(_get_monitor(-1)));
}

void DisplayServerWindowsNT::set_current_screen(int p_screen) {
	const HMONITOR current = _get_monitor(-1);
	const HMONITOR target = _get_monitor(p_screen);
	if (!target || target == current || is_minimized()) {
		return;
	}
	if (_fullscreen) {
		if (!_set_fullscreen_bounds(target)) {
			ERR_PRINT_ONCE("Could not move the Windows NT fullscreen window to the requested screen.");
		}
		return;
	}
	const Rect2i work_area = _get_monitor_rect(target, true);
	if (work_area.has_no_area()) {
		return;
	}
	const bool maximized = is_maximized();
	if (maximized) {
		ShowWindow(_window, SW_RESTORE);
	}
	const Point2i offset = Point2i(get_position()) - _get_monitor_rect(current, true).position;
	const Size2i available = work_area.size - get_frame_size();
	set_position(work_area.position + Point2i(CLAMP(offset.x, 0, MAX(0, available.x)), CLAMP(offset.y, 0, MAX(0, available.y))));
	if (maximized) {
		ShowWindow(_window, SW_MAXIMIZE);
	}
}

Point2i DisplayServerWindowsNT::get_screen_position(int p_screen) const {
	return _get_monitor_rect(_get_monitor(p_screen), false).position;
}

Size2i DisplayServerWindowsNT::get_screen_size(int p_screen) const {
	return _get_monitor_rect(_get_monitor(p_screen), false).size;
}

int DisplayServerWindowsNT::get_screen_dpi() const {
	HDC dc = GetDC(nullptr);
	if (!dc) {
		return 96;
	}
	const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
	ReleaseDC(nullptr, dc);
	return dpi > 0 ? dpi : 96;
}

float DisplayServerWindowsNT::get_screen_refresh_rate(int p_screen, float p_fallback) const {
	const HMONITOR monitor = _get_monitor(p_screen);
	MONITORINFOEXW info = {};
	info.cbSize = sizeof(info);
	if (!_get_monitor_info(monitor, reinterpret_cast<MONITORINFO *>(&info))) {
		return p_fallback;
	}
	DEVMODEW mode = {};
	mode.dmSize = sizeof(mode);
	if (EnumDisplaySettingsW(info.szDevice[0] ? info.szDevice : nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1) {
		return float(mode.dmDisplayFrequency);
	}
	return p_fallback;
}

bool DisplayServerWindowsNT::_set_fullscreen_bounds(HMONITOR p_monitor, UINT p_flags) {
	const Rect2i rect = _get_monitor_rect(p_monitor, false);
	if (rect.has_no_area()) {
		return false;
	}
	if (!p_flags && rect == _get_client_rect()) {
		return true;
	}
	return SetWindowPos(_window, nullptr, rect.position.x, rect.position.y, rect.size.x, rect.size.y,
			SWP_NOACTIVATE | SWP_NOZORDER | p_flags);
}

void DisplayServerWindowsNT::_enter_fullscreen() {
	const HMONITOR monitor = _get_monitor(-1);
	_windowed_placement.length = sizeof(_windowed_placement);
	if (!GetWindowPlacement(_window, &_windowed_placement)) {
		ERR_PRINT_ONCE("Could not save the Windows NT placement before fullscreen.");
		return;
	}
	if (!_shown) {
		_windowed_placement.showCmd = _start_maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
	}
	_windowed_style = GetWindowLongPtrW(_window, GWL_STYLE) & ~(WS_MINIMIZE | WS_MAXIMIZE | WS_VISIBLE);
	if (is_minimized() || is_maximized()) {
		ShowWindow(_window, SW_RESTORE);
	}
	_fullscreen = true;
	SetWindowLongPtrW(_window, GWL_STYLE, WS_POPUP | (_shown ? WS_VISIBLE : 0));
	if (!_set_fullscreen_bounds(monitor, SWP_FRAMECHANGED)) {
		_leave_fullscreen();
		ERR_PRINT_ONCE("Could not enter Windows NT borderless fullscreen.");
	}
}

void DisplayServerWindowsNT::_leave_fullscreen() {
	_fullscreen = false;
	_fullscreen_bounds_dirty = false;
	SetWindowLongPtrW(_window, GWL_STYLE, _windowed_style | (_shown ? WS_VISIBLE : 0));
	SetWindowPos(_window, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);
	if (!SetWindowPlacement(_window, &_windowed_placement)) {
		ERR_PRINT_ONCE("Could not restore the Windows NT placement after fullscreen.");
	}
}

void DisplayServerWindowsNT::set_fullscreen(bool p_enabled) {
	if (!_shown) {
		_start_fullscreen = p_enabled;
	} else if (_fullscreen != p_enabled) {
		if (p_enabled) {
			_enter_fullscreen();
		} else {
			_leave_fullscreen();
		}
	}
}

bool DisplayServerWindowsNT::is_fullscreen() const {
	return _shown ? _fullscreen : _start_fullscreen;
}

DWORD DisplayServerWindowsNT::_get_windowed_frame_style(bool p_borderless, bool p_resizable) {
	if (p_borderless) {
		return WS_POPUP;
	}
	return WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | (p_resizable ? WS_THICKFRAME | WS_MAXIMIZEBOX : 0);
}

bool DisplayServerWindowsNT::set_windowed_frame(bool p_borderless, bool p_resizable) {
	const DWORD FRAME_STYLES = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX;
	const DWORD previous_style = _fullscreen ? _windowed_style : GetWindowLongPtrW(_window, GWL_STYLE);
	const DWORD next_style = (previous_style & ~FRAME_STYLES) | _get_windowed_frame_style(p_borderless, p_resizable);
	if (previous_style == next_style) {
		return true;
	}

	const bool use_placement = _fullscreen || is_minimized() || is_maximized();
	WINDOWPLACEMENT placement = {};
	placement.length = sizeof(placement);
	Rect2i client;
	RECT original_frame = {};
	if (use_placement) {
		if (_fullscreen) {
			placement = _windowed_placement;
		} else {
			ERR_FAIL_COND_V(!GetWindowPlacement(_window, &placement), false);
		}
		// The saved normal bounds include the old frame.
		original_frame = placement.rcNormalPosition;
		RECT old_margin = {};
		RECT new_margin = {};
		const DWORD exstyle = GetWindowLongPtrW(_window, GWL_EXSTYLE);
		ERR_FAIL_COND_V(!AdjustWindowRectEx(&old_margin, previous_style & FRAME_STYLES, FALSE, exstyle), false);
		ERR_FAIL_COND_V(!AdjustWindowRectEx(&new_margin, next_style & FRAME_STYLES, FALSE, exstyle), false);
		placement.rcNormalPosition.left += new_margin.left - old_margin.left;
		placement.rcNormalPosition.top += new_margin.top - old_margin.top;
		placement.rcNormalPosition.right += new_margin.right - old_margin.right;
		placement.rcNormalPosition.bottom += new_margin.bottom - old_margin.bottom;
		if (_fullscreen) {
			_windowed_placement = placement;
			_windowed_style = next_style;
			return true;
		}
	} else {
		client = _get_client_rect();
		ERR_FAIL_COND_V(client.has_no_area() || !GetWindowRect(_window, &original_frame), false);
	}

	const bool shaped = !_mouse_passthrough_polygon.empty();
	if (shaped) {
		// A region based on the old frame can interfere with the new native layout.
		ERR_FAIL_COND_V(!SetWindowRgn(_window, nullptr, FALSE), false);
	}
	SetLastError(ERROR_SUCCESS);
	const bool style_set = SetWindowLongPtrW(_window, GWL_STYLE, next_style) || GetLastError() == ERROR_SUCCESS;
	const UINT FRAME_FLAGS = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE;
	bool applied = style_set && SetWindowPos(_window, nullptr, 0, 0, 0, 0, FRAME_FLAGS);
	if (applied && use_placement) {
		applied = SetWindowPlacement(_window, &placement);
	}
	if (applied && shaped) {
		applied = _apply_mouse_passthrough_polygon(_mouse_passthrough_polygon, true);
	}
	if (applied && !use_placement) {
		// Measure the native frame after the style change, including compatibility DPI scaling.
		const Rect2i actual = _get_client_rect();
		RECT frame;
		applied = !actual.has_no_area() && GetWindowRect(_window, &frame);
		if (applied && actual != client) {
			const Point2i move = client.position - actual.position;
			const Size2i resize = client.size - actual.size;
			applied = SetWindowPos(_window, nullptr, frame.left + move.x, frame.top + move.y,
					frame.right - frame.left + resize.x, frame.bottom - frame.top + resize.y, SWP_NOACTIVATE | SWP_NOZORDER);
		}
	}
	if (!applied) {
		SetWindowLongPtrW(_window, GWL_STYLE, previous_style);
		const UINT restore_flags = use_placement ? FRAME_FLAGS : SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER;
		SetWindowPos(_window, nullptr, original_frame.left, original_frame.top,
				original_frame.right - original_frame.left, original_frame.bottom - original_frame.top, restore_flags);
		if (use_placement) {
			placement.rcNormalPosition = original_frame;
			SetWindowPlacement(_window, &placement);
		}
		if (shaped) {
			_apply_mouse_passthrough_polygon(_mouse_passthrough_polygon, true);
		}
		ERR_FAIL_V_MSG(false, "Could not apply the Windows NT window frame bounds.");
	}
	return true;
}

void DisplayServerWindowsNT::set_always_on_top(bool p_enabled) {
	if (is_always_on_top() == p_enabled) {
		return;
	}
	const HWND order = p_enabled ? HWND_TOPMOST : HWND_NOTOPMOST;
	if (!SetWindowPos(_window, order, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE)) {
		ERR_PRINT_ONCE("Could not change the Windows NT window's topmost state.");
	}
}

bool DisplayServerWindowsNT::is_always_on_top() const {
	return (GetWindowLongPtrW(_window, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

bool DisplayServerWindowsNT::set_keep_screen_on(bool p_enabled) {
	// The execution state belongs to the thread that makes the request.
	ERR_FAIL_COND_V(Thread::get_caller_id() != Thread::get_main_id(), false);
	if (_keep_screen_on == p_enabled) {
		return true;
	}

	EXECUTION_STATE state = ES_CONTINUOUS;
	if (p_enabled) {
		state |= ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED;
	}
	const bool changed = SetThreadExecutionState(state) != 0;
	if (!changed) {
		ERR_PRINT_ONCE("Could not change the Windows NT screen idle request.");
		return false;
	}
	_keep_screen_on = p_enabled;
	return true;
}
