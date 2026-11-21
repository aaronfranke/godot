#include "display_server_windows_nt.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/os/os.h"
#include "drivers/win_legacy/common/display/windows_legacy_window.h"

#ifdef GLES3_ENABLED
#include "drivers/gles3/rasterizer_gles3.h"
#include "drivers/win_legacy/nt/display/opengl/gl_context_windows_nt.h"
#endif // GLES3_ENABLED
#include "platform/windows/key_mapping_windows.h"
#include "platform/windows/native_menu_windows.h"

#include <limits.h>
#include <shellapi.h>

using namespace DisplayServerEnums;

Vector<String> DisplayServerWindowsNT::_get_rendering_drivers() {
	Vector<String> drivers;
#ifdef GLES3_ENABLED
	drivers.push_back("opengl3");
#endif // GLES3_ENABLED
	return drivers;
}

DisplayServer *DisplayServerWindowsNT::_create_display_server(const String &p_driver, WindowMode p_mode, VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_size, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error, CreateInstanceFunction p_create_instance) {
	if (p_parent_window || (p_flags & WINDOW_FLAG_TRANSPARENT_BIT) ||
			(p_driver != "opengl3" && OS::get_singleton()->is_separate_thread_rendering_enabled())) {
		r_error = ERR_UNAVAILABLE;
		return nullptr;
	}
	DisplayServerWindowsNT *server = p_create_instance();
	r_error = server->_initialize(p_driver, p_mode, p_vsync, p_flags, p_position, p_size, p_screen);
	if (r_error != OK) {
		memdelete(server);
		return nullptr;
	}
	return server;
}

DWORD DisplayServerWindowsNT::_style(uint32_t p_flags) {
	if (p_flags & WINDOW_FLAG_BORDERLESS_BIT) {
		return WS_POPUP;
	}
	DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
	if (!(p_flags & WINDOW_FLAG_MINIMIZE_DISABLED_BIT)) {
		style |= WS_MINIMIZEBOX;
	}
	if (!(p_flags & WINDOW_FLAG_RESIZE_DISABLED_BIT)) {
		style |= WS_THICKFRAME;
		if (!(p_flags & WINDOW_FLAG_MAXIMIZE_DISABLED_BIT)) {
			style |= WS_MAXIMIZEBOX;
		}
	}
	return style;
}

Error DisplayServerWindowsNT::_initialize(const String &p_driver, WindowMode p_mode, VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Size2i &p_size, int p_screen) {
	WindowsLegacyWindow::initialize_dpi_awareness();
	bool valid_driver = false;
#ifdef GLES3_ENABLED
	valid_driver = p_driver == "opengl3";
#endif // GLES3_ENABLED
	if (!valid_driver) {
		return ERR_UNAVAILABLE;
	}

	WNDCLASSW window_class = {};
	window_class.style = CS_DBLCLKS | CS_OWNDC;
	window_class.lpfnWndProc = _window_proc;
	window_class.hInstance = GetModuleHandleW(nullptr);
	window_class.hCursor = _get_system_cursor(CURSOR_ARROW);
	// Preserve the native class identifier used by existing window automation.
	window_class.lpszClassName = L"GodotWindowNT5";
	if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		return ERR_CANT_CREATE;
	}

	const uint32_t supported = WINDOW_FLAG_BORDERLESS_BIT | WINDOW_FLAG_RESIZE_DISABLED_BIT |
			WINDOW_FLAG_MINIMIZE_DISABLED_BIT | WINDOW_FLAG_MAXIMIZE_DISABLED_BIT | WINDOW_FLAG_ALWAYS_ON_TOP_BIT;
	_flags = p_flags & supported;
	_initial_mode = p_mode;
	const DWORD exstyle = (_flags & WINDOW_FLAG_ALWAYS_ON_TOP_BIT) ? WS_EX_TOPMOST : 0;
	RECT frame = { 0, 0, MAX(1, p_size.x), MAX(1, p_size.y) };
	AdjustWindowRectEx(&frame, _style(_flags), FALSE, exstyle);
	Point2i position;
	if (p_position) {
		position = _get_screens_origin() + *p_position + Point2i(frame.left, frame.top);
	} else {
		HMONITOR monitor = _get_monitor(p_screen);
		const Rect2i work_area = _get_monitor_rect(monitor ? monitor : _get_primary_monitor(), true);
		position = work_area.position + (work_area.size - Size2i(frame.right - frame.left, frame.bottom - frame.top)) / 2;
	}
	_window = CreateWindowExW(exstyle, window_class.lpszClassName, L"Godot", _style(_flags),
			position.x, position.y, frame.right - frame.left, frame.bottom - frame.top,
			nullptr, nullptr, window_class.hInstance, this);
	if (!_window) {
		return ERR_CANT_CREATE;
	}

	// Reconcile the initial frame with the DPI chosen for the actual window.
	window_set_size(p_size, MAIN_WINDOW_ID);
	_ime_context = ImmAssociateContext(_window, nullptr);
	_vsync = p_vsync == VSYNC_DISABLED ? VSYNC_DISABLED : VSYNC_ENABLED;
	_last_client_rect = _get_client_rect();
	_last_client_rect.position -= _get_screens_origin();
#ifdef GLES3_ENABLED
	if (p_driver == "opengl3") {
		_gl_context = memnew(GLContextWindowsNT);
		const Error error = _gl_context->initialize(_window, _vsync != VSYNC_DISABLED);
		if (error != OK) {
			return error;
		}
		RasterizerGLES3::make_current(true);
	}
#endif // GLES3_ENABLED
	_native_menu = memnew(NativeMenuWindows);
	_taskbar_created_message = RegisterWindowMessageW(L"TaskbarCreated");
	window_set_size(_last_client_rect.size, MAIN_WINDOW_ID);
	KeyMappingWindows::initialize();
	_update_keyboard_labels();
	Input::get_singleton()->set_event_dispatch_function(_dispatch_input);
	screen_set_keep_on(GLOBAL_GET("display/window/energy_saving/keep_screen_on"));
#ifdef GLES3_ENABLED
	if (_gl_context) {
		// Main does not show a single-window Compatibility display server. Own
		// its initial visibility here, without advertising native subwindows.
		show_window(MAIN_WINDOW_ID);
	}
#endif // GLES3_ENABLED
	return OK;
}

DisplayServerWindowsNT::~DisplayServerWindowsNT() {
	screen_set_keep_on(false);
	_stop_file_dialog();
	_mouse_focused = false;
	_release_mouse_clip();
	_shown = false;
	_clear_status_indicators();
	if (_native_menu) {
		memdelete(_native_menu);
		_native_menu = nullptr;
	}
#ifdef GLES3_ENABLED
	if (_gl_context) {
		memdelete(_gl_context);
		_gl_context = nullptr;
	}
#endif // GLES3_ENABLED
	if (_window) {
		DestroyWindow(_window);
		_window = nullptr;
	}
	_replace_window_icons(nullptr, nullptr);
	for (HCURSOR cursor : _custom_cursors) {
		_destroy_custom_cursor(cursor);
	}
}

String DisplayServerWindowsNT::get_name() const {
	return "Windows";
}

bool DisplayServerWindowsNT::has_feature(Feature p_feature) const {
	if (p_feature == FEATURE_SWAP_BUFFERS) {
		return _gl_context != nullptr;
	}
	if (p_feature == FEATURE_IME) {
		return _ime_context != nullptr;
	}
	if (p_feature == FEATURE_NATIVE_DIALOG_FILE) {
		return true;
	}
	return p_feature == FEATURE_MOUSE || p_feature == FEATURE_CURSOR_SHAPE || p_feature == FEATURE_CUSTOM_CURSOR_SHAPE || p_feature == FEATURE_CLIPBOARD ||
			p_feature == FEATURE_NATIVE_DIALOG || p_feature == FEATURE_NATIVE_DIALOG_INPUT ||
			p_feature == FEATURE_ICON || p_feature == FEATURE_NATIVE_ICON || p_feature == FEATURE_STATUS_INDICATOR;
}

Vector<WindowID> DisplayServerWindowsNT::get_window_list() const {
	Vector<WindowID> windows;
	windows.push_back(MAIN_WINDOW_ID);
	return windows;
}

WindowID DisplayServerWindowsNT::get_window_at_screen_position(const Point2i &p_position) const {
	const Point2i native = p_position + _get_screens_origin();
	const POINT position = { native.x, native.y };
	return WindowFromPoint(position) == _window ? MAIN_WINDOW_ID : INVALID_WINDOW_ID;
}

void DisplayServerWindowsNT::show_window(WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID || _shown) {
		return;
	}
	if (_initial_mode == WINDOW_MODE_FULLSCREEN) {
		_enter_fullscreen();
	}
	_shown = true;
	DragAcceptFiles(_window, _drop_files_callback.is_valid());
	ShowWindow(_window, SW_SHOWNORMAL);
	if (_initial_mode != WINDOW_MODE_FULLSCREEN) {
		window_set_mode(_initial_mode, MAIN_WINDOW_ID);
	}
}

int64_t DisplayServerWindowsNT::window_get_native_handle(HandleType p_type, WindowID p_window) const {
	if (p_window != MAIN_WINDOW_ID) {
		return 0;
	}
#ifdef GLES3_ENABLED
	if (_gl_context) {
		if (p_type == WINDOW_VIEW) {
			return reinterpret_cast<intptr_t>(_gl_context->get_dc());
		}
		if (p_type == OPENGL_CONTEXT) {
			return reinterpret_cast<intptr_t>(_gl_context->get_context());
		}
	}
#endif // GLES3_ENABLED
	return p_type == WINDOW_HANDLE ? reinterpret_cast<intptr_t>(_window) : 0;
}

void DisplayServerWindowsNT::gl_window_make_current(WindowID p_window) {
#ifdef GLES3_ENABLED
	if (p_window == MAIN_WINDOW_ID && _gl_context) {
		ERR_FAIL_COND_MSG(_gl_context->make_current() != OK, "Could not make the NT OpenGL context current.");
	}
#endif // GLES3_ENABLED
}

void DisplayServerWindowsNT::release_rendering_thread() {
#ifdef GLES3_ENABLED
	if (_gl_context) {
		_gl_context->release_current();
	}
#endif // GLES3_ENABLED
}

void DisplayServerWindowsNT::swap_buffers() {
#ifdef GLES3_ENABLED
	if (_gl_context) {
		ERR_FAIL_COND_MSG(_gl_context->swap_buffers() != OK, "Could not present the NT OpenGL window.");
	}
#endif // GLES3_ENABLED
}

void DisplayServerWindowsNT::window_attach_instance_id(ObjectID p_instance, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_instance_id = p_instance;
	}
}

ObjectID DisplayServerWindowsNT::window_get_attached_instance_id(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID ? _instance_id : ObjectID();
}

void DisplayServerWindowsNT::window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_rect_changed = p_callable;
	}
}

void DisplayServerWindowsNT::window_set_window_event_callback(const Callable &p_callable, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_window_event = p_callable;
	}
}

void DisplayServerWindowsNT::window_set_input_event_callback(const Callable &p_callable, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_input_event = p_callable;
	}
}

void DisplayServerWindowsNT::window_set_drop_files_callback(const Callable &p_callable, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_drop_files_callback = p_callable;
		if (_shown) {
			DragAcceptFiles(_window, _drop_files_callback.is_valid());
		}
	}
}

void DisplayServerWindowsNT::window_set_mouse_passthrough(const Vector<Vector2> &p_region, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID || (p_region.is_empty() && _mouse_passthrough_polygon.is_empty())) {
		return;
	}
	if (_apply_mouse_passthrough_polygon(p_region, _get_client_rect().position, true)) {
		_mouse_passthrough_polygon = p_region;
	}
}

bool DisplayServerWindowsNT::_apply_mouse_passthrough_polygon(const Vector<Vector2> &p_polygon, const Point2i &p_client_origin, bool p_polygon_changed) {
	HRGN region = nullptr;
	Point2i offset;
	if (!p_polygon.is_empty()) {
		ERR_FAIL_COND_V_MSG(p_polygon.size() < 3 || p_polygon.size() > INT_MAX, false, "Invalid mouse passthrough polygon vertex count.");
		const int count = int(p_polygon.size());
		RECT frame;
		ERR_FAIL_COND_V(!GetWindowRect(_window, &frame), false);
		offset = p_client_origin - Point2i(frame.left, frame.top);
		if (!p_polygon_changed && offset == _mouse_passthrough_offset) {
			return true;
		}

		// GDI represents region coordinates as signed 27-bit integers.
		const double MIN_REGION_COORDINATE = -(1 << 26);
		const double MAX_REGION_COORDINATE = (1 << 26) - 1;
		Vector<POINT> points;
		ERR_FAIL_COND_V(points.resize(count) != OK, false);
		POINT *native = points.ptrw();
		for (int i = 0; i < count; ++i) {
			const double x = double(p_polygon[i].x) + offset.x;
			const double y = double(p_polygon[i].y) + offset.y;
			ERR_FAIL_COND_V_MSG(!(x >= MIN_REGION_COORDINATE && x <= MAX_REGION_COORDINATE && y >= MIN_REGION_COORDINATE && y <= MAX_REGION_COORDINATE), false,
					"Mouse passthrough polygon coordinates are outside the Win32 region range.");
			native[i] = { LONG(x), LONG(y) };
		}
		region = CreatePolygonRgn(points.ptr(), count, ALTERNATE);
		ERR_FAIL_NULL_V(region, false);
	}
	// The window owns the region after a successful call.
	if (!SetWindowRgn(_window, region, TRUE)) {
		if (region) {
			DeleteObject(region);
		}
		ERR_FAIL_V_MSG(false, "Could not set the Windows NT mouse passthrough region.");
	}
	_mouse_passthrough_offset = offset;
	return true;
}

void DisplayServerWindowsNT::_send_window_event(WindowEvent p_event) {
	if (_window_event.is_valid()) {
		_window_event.call(int(p_event));
	}
}

void DisplayServerWindowsNT::window_set_title(const String &p_title, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		SetWindowTextW(_window, (LPCWSTR)p_title.utf16().get_data());
	}
}

Rect2i DisplayServerWindowsNT::_get_client_rect() const {
	RECT client = {};
	POINT position = {};
	if (!GetClientRect(_window, &client) || !ClientToScreen(_window, &position)) {
		return Rect2i();
	}
	return Rect2i(position.x, position.y, client.right, client.bottom);
}

Point2i DisplayServerWindowsNT::window_get_position(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID ? _get_client_rect().position - _get_screens_origin() : Point2i();
}

Point2i DisplayServerWindowsNT::window_get_position_with_decorations(WindowID p_window) const {
	RECT frame = {};
	if (p_window != MAIN_WINDOW_ID || !GetWindowRect(_window, &frame)) {
		return Point2i();
	}
	return Point2i(frame.left, frame.top) - _get_screens_origin();
}

void DisplayServerWindowsNT::window_set_position(const Point2i &p_position, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID || _fullscreen) {
		return;
	}
	const Point2i frame = p_position + _get_screens_origin() + window_get_position_with_decorations(p_window) - window_get_position(p_window);
	SetWindowPos(_window, nullptr, frame.x, frame.y, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
}

Size2i DisplayServerWindowsNT::_get_frame_size(const Size2i &p_client_size) const {
	// The existing non-client frame has the window's actual monitor DPI. The
	// legacy AdjustWindowRectEx instead uses the system DPI on modern Windows.
	RECT window_rect = {};
	RECT client = {};
	if (GetWindowRect(_window, &window_rect) && GetClientRect(_window, &client)) {
		return p_client_size + Size2i(window_rect.right - window_rect.left - client.right, window_rect.bottom - window_rect.top - client.bottom);
	}
	RECT frame = { 0, 0, p_client_size.x, p_client_size.y };
	AdjustWindowRectEx(&frame, GetWindowLongPtrW(_window, GWL_STYLE), FALSE, GetWindowLongPtrW(_window, GWL_EXSTYLE));
	return Size2i(frame.right - frame.left, frame.bottom - frame.top);
}

Size2i DisplayServerWindowsNT::_get_size_limit() const {
	return _max_size;
}

void DisplayServerWindowsNT::window_set_max_size(Size2i p_size, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_max_size = p_size;
	}
}

Size2i DisplayServerWindowsNT::window_get_max_size(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID ? _get_size_limit() : Size2i();
}

void DisplayServerWindowsNT::window_set_min_size(Size2i p_size, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_min_size = p_size;
	}
}

Size2i DisplayServerWindowsNT::window_get_min_size(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID ? _min_size : Size2i();
}

void DisplayServerWindowsNT::window_set_size(Size2i p_size, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID || _fullscreen) {
		return;
	}
	p_size.x = MAX(p_size.x, MAX(1, _min_size.x));
	p_size.y = MAX(p_size.y, MAX(1, _min_size.y));
	const Size2i limit = _get_size_limit();
	if (limit.x > 0) {
		p_size.x = MIN(p_size.x, limit.x);
	}
	if (limit.y > 0) {
		p_size.y = MIN(p_size.y, limit.y);
	}
	const Size2i frame = _get_frame_size(p_size);
	SetWindowPos(_window, nullptr, 0, 0, frame.x, frame.y, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
	// Windows can reject a size request without changing the client (for example,
	// a maximized window). Feed the actual rectangle back to the scene Window anyway.
	_update_client_rect(true);
}

Size2i DisplayServerWindowsNT::window_get_size(WindowID p_window) const {
	if (p_window != MAIN_WINDOW_ID) {
		return Size2i();
	}
	// Keep the last drawable size when a minimized client reports 0x0.
	return IsIconic(_window) ? _last_client_rect.size : _get_client_rect().size;
}

Size2i DisplayServerWindowsNT::window_get_size_with_decorations(WindowID p_window) const {
	RECT frame = {};
	if (p_window != MAIN_WINDOW_ID || !GetWindowRect(_window, &frame)) {
		return Size2i();
	}
	return Size2i(frame.right - frame.left, frame.bottom - frame.top);
}

bool DisplayServerWindowsNT::_set_fullscreen_bounds(HMONITOR p_monitor, UINT p_flags) {
	const Rect2i rect = _get_monitor_rect(p_monitor);
	if (!rect.has_area()) {
		return false;
	}
	if (p_flags == 0 && rect == _get_client_rect()) {
		return true;
	}
	return SetWindowPos(_window, nullptr, rect.position.x, rect.position.y, rect.size.x, rect.size.y,
			SWP_NOACTIVATE | SWP_NOZORDER | p_flags);
}

void DisplayServerWindowsNT::_enter_fullscreen() {
	const HMONITOR monitor = _get_window_monitor(_window, MONITOR_DEFAULTTONEAREST);
	_windowed_placement.length = sizeof(WINDOWPLACEMENT);
	if (!GetWindowPlacement(_window, &_windowed_placement)) {
		ERR_PRINT_ONCE("Could not save the Windows NT window placement for fullscreen.");
		return;
	}
	if (IsIconic(_window) || IsZoomed(_window)) {
		ShowWindow(_window, SW_RESTORE);
	}
	_fullscreen = true;
	SetWindowLongPtrW(_window, GWL_STYLE, WS_POPUP | (_shown ? WS_VISIBLE : 0));
	if (!_set_fullscreen_bounds(monitor, SWP_FRAMECHANGED)) {
		_leave_fullscreen(_windowed_placement.showCmd);
		ERR_PRINT_ONCE("Could not enter Windows NT borderless fullscreen.");
	}
}

void DisplayServerWindowsNT::_leave_fullscreen(int p_show_command) {
	_fullscreen = false;
	_fullscreen_bounds_dirty = false;
	SetWindowLongPtrW(_window, GWL_STYLE, _style(_flags) | (_shown ? WS_VISIBLE : 0));
	SetWindowPos(_window, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);
	_windowed_placement.showCmd = p_show_command;
	if (!SetWindowPlacement(_window, &_windowed_placement)) {
		ERR_PRINT_ONCE("Could not restore the Windows NT window placement after fullscreen.");
	}
}

void DisplayServerWindowsNT::window_set_mode(WindowMode p_mode, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID) {
		return;
	}
	if (p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
		WARN_PRINT_ONCE("Exclusive fullscreen is not supported by the Windows NT display server. Use borderless fullscreen instead.");
		return;
	}
	if (!_shown) {
		_initial_mode = p_mode;
		return;
	}
	if (window_get_mode(p_window) == p_mode) {
		return;
	}
	if (p_mode == WINDOW_MODE_FULLSCREEN) {
		if (_fullscreen) {
			ShowWindow(_window, SW_RESTORE);
		} else {
			_enter_fullscreen();
		}
		return;
	}
	int command = SW_RESTORE;
	if (p_mode == WINDOW_MODE_MINIMIZED) {
		command = SW_MINIMIZE;
	} else if (p_mode == WINDOW_MODE_MAXIMIZED) {
		command = SW_MAXIMIZE;
	}
	if (_fullscreen && p_mode != WINDOW_MODE_MINIMIZED) {
		_leave_fullscreen(p_mode == WINDOW_MODE_MAXIMIZED ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
		return;
	}
	ShowWindow(_window, command);
}

WindowMode DisplayServerWindowsNT::window_get_mode(WindowID p_window) const {
	if (p_window == MAIN_WINDOW_ID) {
		if (IsIconic(_window)) {
			return WINDOW_MODE_MINIMIZED;
		}
		if (_fullscreen) {
			return WINDOW_MODE_FULLSCREEN;
		}
		if (IsZoomed(_window)) {
			return WINDOW_MODE_MAXIMIZED;
		}
	}
	return WINDOW_MODE_WINDOWED;
}

void DisplayServerWindowsNT::window_set_vsync_mode(VSyncMode p_mode, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID) {
		return;
	}
#ifdef GLES3_ENABLED
	if (_gl_context) {
		_gl_context->set_use_vsync(p_mode != VSYNC_DISABLED);
		_vsync = p_mode == VSYNC_DISABLED ? VSYNC_DISABLED : VSYNC_ENABLED;
		return;
	}
#endif // GLES3_ENABLED
}

VSyncMode DisplayServerWindowsNT::window_get_vsync_mode(WindowID) const {
	return _vsync;
}

bool DisplayServerWindowsNT::window_is_maximize_allowed(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID && (_style(_flags) & WS_MAXIMIZEBOX);
}

void DisplayServerWindowsNT::window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID) {
		return;
	}
	switch (p_flag) {
		case WINDOW_FLAG_ALWAYS_ON_TOP:
		case WINDOW_FLAG_BORDERLESS:
		case WINDOW_FLAG_RESIZE_DISABLED:
		case WINDOW_FLAG_MINIMIZE_DISABLED:
		case WINDOW_FLAG_MAXIMIZE_DISABLED:
			break;
		default:
			return;
	}
	if (window_get_flag(p_flag, p_window) == p_enabled) {
		return;
	}
	const Rect2i client = _get_client_rect();
	const uint32_t mask = 1u << p_flag;
	_flags = p_enabled ? _flags | mask : _flags & ~mask;
	const HWND order = (_flags & WINDOW_FLAG_ALWAYS_ON_TOP_BIT) ? HWND_TOPMOST : HWND_NOTOPMOST;
	if (_fullscreen) {
		if (p_flag == WINDOW_FLAG_ALWAYS_ON_TOP) {
			SetWindowPos(_window, order, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		}
		return;
	}
	SetWindowLongPtrW(_window, GWL_STYLE, _style(_flags) | (_shown ? WS_VISIBLE : 0));
	SetWindowPos(_window, order, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
	if (window_get_mode(p_window) == WINDOW_MODE_WINDOWED) {
		window_set_size(client.size, p_window);
		window_set_position(client.position, p_window);
	}
}

bool DisplayServerWindowsNT::window_get_flag(WindowFlags p_flag, WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID && p_flag >= 0 && p_flag < WINDOW_FLAG_MAX && (_flags & (1u << p_flag));
}

void DisplayServerWindowsNT::window_request_attention(WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		FLASHWINFO flash = { sizeof(FLASHWINFO), _window, FLASHW_TRAY, 3, 0 };
		FlashWindowEx(&flash);
	}
}

void DisplayServerWindowsNT::window_move_to_foreground(WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		SetForegroundWindow(_window);
	}
}

bool DisplayServerWindowsNT::window_is_focused(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID && GetFocus() == _window;
}

bool DisplayServerWindowsNT::window_can_draw(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID && _shown && !IsIconic(_window);
}

bool DisplayServerWindowsNT::can_any_window_draw() const {
	return window_can_draw(MAIN_WINDOW_ID);
}

void DisplayServerWindowsNT::_update_client_rect(bool p_force_callback) {
	if (!can_any_window_draw()) {
		return;
	}
	if (_fullscreen_bounds_dirty) {
		_fullscreen_bounds_dirty = false;
		if (_fullscreen && !_set_fullscreen_bounds(_get_window_monitor(_window, MONITOR_DEFAULTTONEAREST))) {
			ERR_PRINT_ONCE("Could not update Windows NT fullscreen bounds after a display change.");
		}
	}
	Rect2i client = _get_client_rect();
	if (client.has_area() && !_mouse_passthrough_polygon.is_empty()) {
		_apply_mouse_passthrough_polygon(_mouse_passthrough_polygon, client.position, false);
	}
	client.position -= _get_screens_origin();
	if (!client.has_area() || (client == _last_client_rect && !p_force_callback)) {
		return;
	}
	_last_client_rect = client;
	if (_rect_changed.is_valid()) {
		_rect_changed.call(client);
	}
}

LRESULT CALLBACK DisplayServerWindowsNT::_window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam) {
	if (p_message == WM_NCCREATE) {
		const CREATESTRUCTW *create = reinterpret_cast<const CREATESTRUCTW *>(p_lparam);
		auto *server = static_cast<DisplayServerWindowsNT *>(create->lpCreateParams);
		server->_window = p_window;
		SetWindowLongPtrW(p_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(server));
	}
	auto *server = reinterpret_cast<DisplayServerWindowsNT *>(GetWindowLongPtrW(p_window, GWLP_USERDATA));
	if (server && server->_shown) {
		return server->_handle_message(p_message, p_wparam, p_lparam);
	}
	return DefWindowProcW(p_window, p_message, p_wparam, p_lparam);
}
