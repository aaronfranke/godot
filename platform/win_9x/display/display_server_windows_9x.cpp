#include "display_server_windows_9x.h"

#include "encoding_windows_9x.h"

#include "core/input/input.h"
#include "core/os/os.h"
#include "drivers/win_legacy/common/display/windows_legacy_window.h"

#include "platform/windows/fixed_function/fixed_function_windows.h"
#include "platform/windows/key_mapping_windows.h"

#include <shellapi.h>

using namespace DisplayServerEnums;

Vector<String> DisplayServerWindows9x::_get_rendering_drivers() {
	Vector<String> drivers;
#ifdef D3D7_ENABLED
	drivers.push_back("d3d7");
#endif // D3D7_ENABLED
#ifdef D3D9_ENABLED
	drivers.push_back("d3d9");
#endif // D3D9_ENABLED
#ifdef OPENGL1_ENABLED
	drivers.push_back("opengl1");
#endif // OPENGL1_ENABLED
#ifdef D3D3_SOFTWARE_ENABLED
	drivers.push_back("d3d3_software");
#endif // D3D3_SOFTWARE_ENABLED
	return drivers;
}

void DisplayServerWindows9x::register_driver() {
	register_create_function("windows", _create, _get_rendering_drivers);
}

DisplayServer *DisplayServerWindows9x::_create(const String &p_driver, WindowMode p_mode, VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_size, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error) {
	if (p_parent_window || (p_flags & WINDOW_FLAG_TRANSPARENT_BIT) || OS::get_singleton()->is_separate_thread_rendering_enabled()) {
		r_error = ERR_UNAVAILABLE;
		return nullptr;
	}
	DisplayServerWindows9x *server = memnew(DisplayServerWindows9x);
	r_error = server->_initialize(p_driver, p_mode, p_vsync, p_flags, p_position, p_size);
	if (r_error != OK) {
		memdelete(server);
		return nullptr;
	}
	return server;
}

DWORD DisplayServerWindows9x::_style(uint32_t p_flags) {
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

Error DisplayServerWindows9x::_initialize(const String &p_driver, WindowMode p_mode, VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Size2i &p_size) {
	WindowsLegacyWindow::initialize_dpi_awareness();
	int backend = -1;
	for (int i = 0; i < FixedFunctionWindows::get_driver_count(); ++i) {
		if (p_driver == FixedFunctionWindows::get_driver_name(i)) {
			backend = i;
			break;
		}
	}
	if (backend < 0) {
		return ERR_UNAVAILABLE;
	}
	WNDCLASSA window_class = {};
	window_class.style = CS_OWNDC | CS_DBLCLKS;
	window_class.lpfnWndProc = _window_proc;
	window_class.hInstance = GetModuleHandleA(nullptr);
	window_class.hCursor = LoadCursorA(nullptr, MAKEINTRESOURCEA(32512));
	window_class.lpszClassName = "GodotWindow9x";
	if (!RegisterClassA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		return ERR_CANT_CREATE;
	}
	const uint32_t supported = WINDOW_FLAG_BORDERLESS_BIT | WINDOW_FLAG_RESIZE_DISABLED_BIT |
			WINDOW_FLAG_MINIMIZE_DISABLED_BIT | WINDOW_FLAG_MAXIMIZE_DISABLED_BIT | WINDOW_FLAG_ALWAYS_ON_TOP_BIT;
	_flags = p_flags & supported;
	_initial_mode = p_mode;
	const DWORD exstyle = (_flags & WINDOW_FLAG_ALWAYS_ON_TOP_BIT) ? WS_EX_TOPMOST : 0;
	RECT frame = { 0, 0, MAX(1, p_size.x), MAX(1, p_size.y) };
	if (!AdjustWindowRectEx(&frame, _style(_flags), FALSE, exstyle)) {
		return ERR_CANT_CREATE;
	}
	const Size2i outer_size(frame.right - frame.left, frame.bottom - frame.top);
	const Rect2i usable = screen_get_usable_rect(0);
	Point2i position = usable.position + (usable.size - outer_size) / 2;
	if (p_position) {
		position = *p_position + Point2i(frame.left, frame.top);
	}
	_window = CreateWindowExA(exstyle, window_class.lpszClassName, "Godot", _style(_flags),
			position.x, position.y, outer_size.x, outer_size.y, nullptr, nullptr, window_class.hInstance, this);
	if (!_window) {
		return ERR_CANT_CREATE;
	}
	_vsync = p_vsync == VSYNC_DISABLED ? VSYNC_DISABLED : VSYNC_ENABLED;
	_renderer = memnew(FixedFunctionWindows);
	_last_client_rect = _get_client_rect();
	const Error error = _renderer->install(backend, _window, _last_client_rect.size, _vsync != VSYNC_DISABLED);
	if (error != OK) {
		return error;
	}
	window_set_size(_last_client_rect.size, MAIN_WINDOW_ID);
	KeyMappingWindows::initialize();
	Input::get_singleton()->set_event_dispatch_function(_dispatch_input);
	return OK;
}

DisplayServerWindows9x::~DisplayServerWindows9x() {
	_shown = false;
	_release_mouse_clip();
	if (GetCapture() == _window) {
		ReleaseCapture();
	}
	if (_renderer) {
		memdelete(_renderer);
		_renderer = nullptr;
	}
	if (_window) {
		DestroyWindow(_window);
		_window = nullptr;
	}
	for (HCURSOR cursor : _custom_cursors) {
		_destroy_custom_cursor(cursor);
	}
}

String DisplayServerWindows9x::get_name() const {
	return "Windows";
}

bool DisplayServerWindows9x::has_feature(Feature p_feature) const {
	return p_feature == FEATURE_MOUSE || p_feature == FEATURE_CURSOR_SHAPE || p_feature == FEATURE_CUSTOM_CURSOR_SHAPE || p_feature == FEATURE_CLIPBOARD;
}

int DisplayServerWindows9x::get_screen_count() const {
	// Start with the primary display; secondary-monitor enumeration is a separate gate.
	return 1;
}

int DisplayServerWindows9x::get_primary_screen() const {
	return 0;
}

Point2i DisplayServerWindows9x::screen_get_position(int p_screen) const {
	return Point2i();
}

Size2i DisplayServerWindows9x::screen_get_size(int p_screen) const {
	return Size2i(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
}

Rect2i DisplayServerWindows9x::screen_get_usable_rect(int p_screen) const {
	RECT rect;
	if (SystemParametersInfoA(SPI_GETWORKAREA, 0, &rect, 0)) {
		return Rect2i(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
	}
	return Rect2i(Point2i(), screen_get_size(p_screen));
}

int DisplayServerWindows9x::screen_get_dpi(int p_screen) const {
	HDC dc = GetDC(nullptr);
	const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
	if (dc) {
		ReleaseDC(nullptr, dc);
	}
	return dpi;
}

float DisplayServerWindows9x::screen_get_refresh_rate(int p_screen) const {
	DEVMODEA mode = {};
	mode.dmSize = sizeof(mode);
	if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1) {
		return float(mode.dmDisplayFrequency);
	}
	return SCREEN_REFRESH_RATE_FALLBACK;
}

Vector<WindowID> DisplayServerWindows9x::get_window_list() const {
	Vector<WindowID> windows;
	windows.push_back(MAIN_WINDOW_ID);
	return windows;
}

WindowID DisplayServerWindows9x::get_window_at_screen_position(const Point2i &p_position) const {
	const POINT point = { p_position.x, p_position.y };
	return WindowFromPoint(point) == _window ? MAIN_WINDOW_ID : INVALID_WINDOW_ID;
}

void DisplayServerWindows9x::show_window(WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID || _shown) {
		return;
	}
	_shown = true;
	DragAcceptFiles(_window, _drop_files_callback.is_valid());
	ShowWindow(_window, SW_SHOWNORMAL);
	window_set_mode(_initial_mode, p_window);
}

int64_t DisplayServerWindows9x::window_get_native_handle(HandleType p_type, WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID && p_type == WINDOW_HANDLE ? reinterpret_cast<intptr_t>(_window) : 0;
}

void DisplayServerWindows9x::window_attach_instance_id(ObjectID p_instance, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_instance_id = p_instance;
	}
}

ObjectID DisplayServerWindows9x::window_get_attached_instance_id(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID ? _instance_id : ObjectID();
}

void DisplayServerWindows9x::window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_rect_changed = p_callable;
	}
}

void DisplayServerWindows9x::window_set_window_event_callback(const Callable &p_callable, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_window_event = p_callable;
	}
}

void DisplayServerWindows9x::window_set_input_event_callback(const Callable &p_callable, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_input_event = p_callable;
	}
}

void DisplayServerWindows9x::window_set_title(const String &p_title, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		CharString title;
		// Window chrome can substitute unsupported glyphs; filesystem paths cannot.
		if (Windows9xEncoding::encode(p_title, title, true) == OK) {
			SetWindowTextA(_window, title.get_data());
		}
	}
}

int DisplayServerWindows9x::window_get_current_screen(WindowID p_window) const {
	return 0;
}

void DisplayServerWindows9x::window_set_current_screen(int p_screen, WindowID p_window) {
	ERR_FAIL_COND(p_screen > 0);
}

Rect2i DisplayServerWindows9x::_get_client_rect() const {
	RECT rect;
	POINT origin = {};
	if (!GetClientRect(_window, &rect) || !ClientToScreen(_window, &origin)) {
		return Rect2i();
	}
	return Rect2i(origin.x, origin.y, rect.right, rect.bottom);
}

Point2i DisplayServerWindows9x::window_get_position(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID ? _get_client_rect().position : Point2i();
}

Point2i DisplayServerWindows9x::window_get_position_with_decorations(WindowID p_window) const {
	RECT rect;
	return p_window == MAIN_WINDOW_ID && GetWindowRect(_window, &rect) ? Point2i(rect.left, rect.top) : Point2i();
}

void DisplayServerWindows9x::window_set_position(const Point2i &p_position, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID || _fullscreen) {
		return;
	}
	RECT rect;
	if (GetWindowRect(_window, &rect)) {
		const Point2i outer = p_position + Point2i(rect.left, rect.top) - _get_client_rect().position;
		SetWindowPos(_window, nullptr, outer.x, outer.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

Size2i DisplayServerWindows9x::_get_frame_size(const Size2i &p_size) const {
	RECT outer, client;
	// A display runtime can alter the effective frame. Measure it rather than
	// assuming the SDK's calculated margins still match this existing window.
	if (!IsIconic(_window) && GetWindowRect(_window, &outer) && GetClientRect(_window, &client) && client.right > 0 && client.bottom > 0) {
		return p_size + Size2i(MAX(0L, outer.right - outer.left - client.right), MAX(0L, outer.bottom - outer.top - client.bottom));
	}
	RECT rect = { 0, 0, p_size.x, p_size.y };
	AdjustWindowRectEx(&rect, GetWindowLongA(_window, GWL_STYLE), FALSE, GetWindowLongA(_window, GWL_EXSTYLE));
	return Size2i(rect.right - rect.left, rect.bottom - rect.top);
}

Size2i DisplayServerWindows9x::_get_size_limit() const {
	Size2i limit = _max_size;
	const Size2i device = _renderer ? _renderer->get_max_window_size() : Size2i();
	if (device.x > 0) {
		limit.x = limit.x > 0 ? MIN(limit.x, device.x) : device.x;
	}
	if (device.y > 0) {
		limit.y = limit.y > 0 ? MIN(limit.y, device.y) : device.y;
	}
	return limit;
}

void DisplayServerWindows9x::window_set_max_size(Size2i p_size, WindowID p_window) {
	ERR_FAIL_COND(p_size.x < 0 || p_size.y < 0);
	if (p_window == MAIN_WINDOW_ID) {
		_max_size = p_size;
		window_set_size(window_get_size(p_window), p_window);
	}
}

Size2i DisplayServerWindows9x::window_get_max_size(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID ? _get_size_limit() : Size2i();
}

void DisplayServerWindows9x::window_set_min_size(Size2i p_size, WindowID p_window) {
	ERR_FAIL_COND(p_size.x < 0 || p_size.y < 0);
	if (p_window == MAIN_WINDOW_ID) {
		_min_size = p_size;
		window_set_size(window_get_size(p_window), p_window);
	}
}

Size2i DisplayServerWindows9x::window_get_min_size(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID ? _min_size : Size2i();
}

void DisplayServerWindows9x::window_set_size(Size2i p_size, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID || _fullscreen) {
		return;
	}
	p_size.x = MAX(MAX(1, _min_size.x), p_size.x);
	p_size.y = MAX(MAX(1, _min_size.y), p_size.y);
	const Size2i limit = _get_size_limit();
	if (limit.x > 0) {
		p_size.x = MIN(p_size.x, limit.x);
	}
	if (limit.y > 0) {
		p_size.y = MIN(p_size.y, limit.y);
	}
	const Size2i outer = _get_frame_size(p_size);
	SetWindowPos(_window, nullptr, 0, 0, outer.x, outer.y, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

Size2i DisplayServerWindows9x::window_get_size(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID ? _get_client_rect().size : Size2i();
}

Size2i DisplayServerWindows9x::window_get_size_with_decorations(WindowID p_window) const {
	RECT rect;
	return p_window == MAIN_WINDOW_ID && GetWindowRect(_window, &rect) ? Size2i(rect.right - rect.left, rect.bottom - rect.top) : Size2i();
}

void DisplayServerWindows9x::window_set_mode(WindowMode p_mode, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID) {
		return;
	}
	if (!_shown) {
		_initial_mode = p_mode;
		return;
	}
	if (window_get_mode(p_window) == p_mode) {
		return;
	}
	const bool fullscreen = p_mode == WINDOW_MODE_FULLSCREEN || p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
	if (fullscreen && !_fullscreen) {
		_windowed_placement.length = sizeof(_windowed_placement);
		if (!GetWindowPlacement(_window, &_windowed_placement)) {
			return;
		}
		if (IsIconic(_window) || IsZoomed(_window)) {
			ShowWindow(_window, SW_RESTORE);
		}
		_fullscreen = true;
		SetWindowLongA(_window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		const Size2i size = screen_get_size(0);
		// Both modes use borderless fullscreen; the desktop display mode is unchanged.
		SetWindowPos(_window, nullptr, 0, 0, size.x, size.y, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	} else if (fullscreen) {
		ShowWindow(_window, SW_RESTORE);
	} else {
		if (_fullscreen && p_mode != WINDOW_MODE_MINIMIZED) {
			_fullscreen = false;
			SetWindowLongA(_window, GWL_STYLE, _style(_flags) | WS_VISIBLE);
			// Recalculate the restored non-client frame before restoring its placement.
			SetWindowPos(_window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			_windowed_placement.showCmd = p_mode == WINDOW_MODE_MAXIMIZED ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
			SetWindowPlacement(_window, &_windowed_placement);
			return;
		}
		int command = SW_RESTORE;
		if (p_mode == WINDOW_MODE_MINIMIZED) {
			command = SW_MINIMIZE;
		} else if (p_mode == WINDOW_MODE_MAXIMIZED && window_is_maximize_allowed(p_window)) {
			command = SW_MAXIMIZE;
		}
		ShowWindow(_window, command);
	}
}

WindowMode DisplayServerWindows9x::window_get_mode(WindowID p_window) const {
	if (!_shown) {
		return _initial_mode;
	}
	if (IsIconic(_window)) {
		return WINDOW_MODE_MINIMIZED;
	}
	if (_fullscreen) {
		return WINDOW_MODE_FULLSCREEN;
	}
	return IsZoomed(_window) ? WINDOW_MODE_MAXIMIZED : WINDOW_MODE_WINDOWED;
}

void DisplayServerWindows9x::window_set_vsync_mode(VSyncMode p_mode, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		const VSyncMode mode = p_mode == VSYNC_DISABLED ? VSYNC_DISABLED : VSYNC_ENABLED;
		if (_renderer->set_use_vsync(mode != VSYNC_DISABLED) == OK) {
			_vsync = mode;
		}
	}
}

VSyncMode DisplayServerWindows9x::window_get_vsync_mode(WindowID p_window) const {
	return _vsync;
}

bool DisplayServerWindows9x::window_is_maximize_allowed(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID && !(_flags & (WINDOW_FLAG_RESIZE_DISABLED_BIT | WINDOW_FLAG_MAXIMIZE_DISABLED_BIT));
}

void DisplayServerWindows9x::window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window) {
	if (p_window != MAIN_WINDOW_ID || (p_flag != WINDOW_FLAG_BORDERLESS && p_flag != WINDOW_FLAG_RESIZE_DISABLED && p_flag != WINDOW_FLAG_MINIMIZE_DISABLED && p_flag != WINDOW_FLAG_MAXIMIZE_DISABLED && p_flag != WINDOW_FLAG_ALWAYS_ON_TOP)) {
		return;
	}
	const Rect2i client = _get_client_rect();
	if (p_enabled) {
		_flags |= 1U << p_flag;
	} else {
		_flags &= ~(1U << p_flag);
	}
	if (!_fullscreen) {
		SetWindowLongA(_window, GWL_STYLE, _style(_flags) | (_shown ? WS_VISIBLE : 0));
		SetWindowPos(_window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		window_set_size(client.size, p_window);
		window_set_position(client.position, p_window);
	}
	const HWND order = _flags & WINDOW_FLAG_ALWAYS_ON_TOP_BIT ? HWND_TOPMOST : HWND_NOTOPMOST;
	SetWindowPos(_window, order, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool DisplayServerWindows9x::window_get_flag(WindowFlags p_flag, WindowID p_window) const {
	ERR_FAIL_INDEX_V(p_flag, WINDOW_FLAG_MAX, false);
	return p_window == MAIN_WINDOW_ID && (_flags & (1U << p_flag));
}

void DisplayServerWindows9x::window_request_attention(WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		FlashWindow(_window, TRUE);
	}
}

void DisplayServerWindows9x::window_move_to_foreground(WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		SetForegroundWindow(_window);
	}
}

bool DisplayServerWindows9x::window_is_focused(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID && GetFocus() == _window;
}

bool DisplayServerWindows9x::window_can_draw(WindowID p_window) const {
	return p_window == MAIN_WINDOW_ID && _shown && !IsIconic(_window);
}

bool DisplayServerWindows9x::can_any_window_draw() const {
	return window_can_draw(MAIN_WINDOW_ID);
}

void DisplayServerWindows9x::_update_client_rect() {
	if (!_renderer || !can_any_window_draw()) {
		return;
	}
	if (_display_changed) {
		_display_changed = false;
		_renderer->notify_display_changed();
		if (_fullscreen) {
			const Size2i size = screen_get_size(0);
			SetWindowPos(_window, nullptr, 0, 0, size.x, size.y, SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}
	const Rect2i client = _get_client_rect();
	if (!client.has_area() || client == _last_client_rect) {
		return;
	}
	if (client.size != _last_client_rect.size) {
		const Error error = _renderer->resize(client.size);
		if (error != OK) {
			if (error != ERR_BUSY && error != ERR_SKIP) {
				ERR_PRINT_ONCE("Could not resize the Windows 9x Fixed Function surface.");
			}
			return;
		}
	}
	_last_client_rect = client;
	if (_rect_changed.is_valid()) {
		_rect_changed.call(client);
	}
}

LRESULT CALLBACK DisplayServerWindows9x::_window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam) {
	if (p_message == WM_NCCREATE) {
		const CREATESTRUCTA *create = reinterpret_cast<const CREATESTRUCTA *>(p_lparam);
		auto *server = static_cast<DisplayServerWindows9x *>(create->lpCreateParams);
		server->_window = p_window;
		// win_9x is x86_32 only, so LONG holds the native pointer.
		SetWindowLongA(p_window, GWL_USERDATA, reinterpret_cast<LONG>(server));
	}
	auto *server = reinterpret_cast<DisplayServerWindows9x *>(GetWindowLongA(p_window, GWL_USERDATA));
	if (server && server->_shown) {
		return server->_handle_message(p_message, p_wparam, p_lparam);
	}
	return DefWindowProcA(p_window, p_message, p_wparam, p_lparam);
}
