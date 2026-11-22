#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Legacy Windows includes.
#include "drivers/win_legacy/common/display/windows_legacy_window.h"

// Godot includes.
#include "core/os/main_loop.h"
#ifdef GLES_ENABLED
#include "drivers/gles2/rasterizer_gles2.h"
#include "drivers/gles3/rasterizer_gles3.h"
#include "drivers/win_legacy/nt/display/opengl/gl_context_windows_nt.h"
#endif // GLES_ENABLED
#include "core/project_settings.h"
#include "main/input_default.h"
#include "main/main.h"

#include <shellapi.h>
#include <windowsx.h>

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif // WM_MOUSEHWHEEL

int DisplayServerWindowsNT::get_driver_count() {
	int count = 0;
#ifdef GLES_ENABLED
	count += 2; // Preserve Godot 3.x's GLES3=0, GLES2=1 video driver indices.
#endif // GLES_ENABLED
	return count;
}

bool DisplayServerWindowsNT::is_gl_driver(int p_driver) {
#ifdef GLES_ENABLED
	return p_driver >= 0 && p_driver < 2;
#else
	return false;
#endif // GLES_ENABLED
}

const char *DisplayServerWindowsNT::get_driver_name(int p_driver) {
	ERR_FAIL_INDEX_V(p_driver, get_driver_count(), "");
#ifdef GLES_ENABLED
	if (is_gl_driver(p_driver)) {
		return p_driver == 0 ? "GLES3" : "GLES2";
	}
#endif // GLES_ENABLED
	return "";
}

Error DisplayServerWindowsNT::initialize(const OS::VideoMode &p_mode, int p_driver, InputDefault &p_input) {
	WindowsLegacyWindow::initialize_dpi_awareness();
	HINSTANCE instance = GetModuleHandleW(nullptr);
	WNDCLASSW window_class = {};
	window_class.style = CS_DBLCLKS | CS_OWNDC;
	window_class.lpfnWndProc = _window_proc;
	window_class.hInstance = instance;
	window_class.hCursor = _get_system_cursor(OS::CURSOR_ARROW);
	window_class.lpszClassName = L"GodotWindowNT";
	if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		return ERR_CANT_CREATE;
	}

	_input = &p_input;
	_ignore_unfocused_joypads = GLOBAL_DEF_RST("input_devices/joypads/ignore_joypad_on_unfocused_application", false);
	_joypad.set_input_enabled(p_input, !_ignore_unfocused_joypads);
	_start_fullscreen = p_mode.fullscreen;
	_start_maximized = p_mode.maximized;
	const DWORD style = _get_windowed_frame_style(p_mode.borderless_window, p_mode.resizable);
	const DWORD exstyle = p_mode.always_on_top ? WS_EX_TOPMOST : 0;
	RECT frame = { 0, 0, MAX(1, p_mode.width), MAX(1, p_mode.height) };
	AdjustWindowRectEx(&frame, style, FALSE, exstyle);
	const int width = frame.right - frame.left;
	const int height = frame.bottom - frame.top;
	RECT work_area = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
	const int x = work_area.left + MAX(0, (work_area.right - work_area.left - width) / 2);
	const int y = work_area.top + MAX(0, (work_area.bottom - work_area.top - height) / 2);
	HWND window = CreateWindowExW(exstyle, window_class.lpszClassName, L"Godot", style, x, y, width, height, nullptr, nullptr, instance, this);
	if (!window) {
		return ERR_CANT_CREATE;
	}
	_ime_context = ImmAssociateContext(window, nullptr);
	RECT client = {};
	GetClientRect(window, &client);
	_client_size = Size2i(client.right, client.bottom);
	_render_size = _client_size;
#ifdef GLES_ENABLED
	if (is_gl_driver(p_driver)) {
		_gl_context = memnew(GLContextWindowsNT);
		Error error = _gl_context->initialize(window, p_mode.use_vsync, p_driver == 0);
		if (error != OK) {
			return error;
		}
		if (p_driver == 0) {
			error = RasterizerGLES3::is_viable();
			if (error == OK) {
				RasterizerGLES3::register_config();
				RasterizerGLES3::make_current();
			}
		} else {
			error = RasterizerGLES2::is_viable();
			if (error == OK) {
				RasterizerGLES2::register_config();
				RasterizerGLES2::make_current();
			}
		}
		return error;
	}
#endif // GLES_ENABLED
	return ERR_UNAVAILABLE;
}

void DisplayServerWindowsNT::make_rendering_thread() {
#ifdef GLES_ENABLED
	if (_gl_context) {
		ERR_FAIL_COND(_gl_context->make_current() != OK);
	}
#endif // GLES_ENABLED
}

void DisplayServerWindowsNT::release_rendering_thread() {
#ifdef GLES_ENABLED
	if (_gl_context) {
		_gl_context->release_current();
	}
#endif // GLES_ENABLED
}

void DisplayServerWindowsNT::swap_buffers() {
#ifdef GLES_ENABLED
	if (_gl_context && _gl_context->swap_buffers() != OK) {
		ERR_PRINT_ONCE("Could not present the Windows NT OpenGL window.");
	}
#endif // GLES_ENABLED
}

void DisplayServerWindowsNT::show(MainLoop &p_main_loop) {
	_main_loop = &p_main_loop;
	set_size(_client_size);
	if (OS::get_singleton()->is_no_window_mode_enabled()) {
		return;
	}
	if (_start_fullscreen) {
		_enter_fullscreen();
	}
	_shown = true;
	DragAcceptFiles(_window, TRUE);
	ShowWindow(_window, !_fullscreen && _start_maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
}

void DisplayServerWindowsNT::_pump_messages() {
	MSG message;
	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
		if (message.message == WM_QUIT) {
			_quit = true;
			break;
		}
		if (!_drop_input) {
			TranslateMessage(&message);
		}
		DispatchMessageW(&message);
	}
	// Finish native activation and drain stale motion messages before recapturing.
	if (_mouse_focus_pending && GetForegroundWindow() == _window) {
		_mouse_focus_pending = false;
		_update_mouse_mode(true);
	}
}

void DisplayServerWindowsNT::process_events() {
	if (_input_reset_pending) {
		_input_reset_pending = false;
		_release_keys();
		_release_mouse_buttons();
		if (_ignore_unfocused_joypads) {
			DWORD foreground_process = 0;
			GetWindowThreadProcessId(GetForegroundWindow(), &foreground_process);
			_joypad.set_input_enabled(*_input, foreground_process == GetCurrentProcessId());
		}
	}
	_pump_messages();
	_flush_keys();
	if (!_quit && _main_loop) {
		_joypad.process(*_input);
	}
	_input->flush_buffered_events();
	_update_window();
}

void DisplayServerWindowsNT::process_and_drop_events() {
	if (_drop_input) {
		return;
	}
	_drop_input = true;
	if (!_input_reset_pending) {
		// Defer releases until normal input dispatch resumes, even if focus never changes.
		_input_reset_pending = true;
		_keys.clear();
		_high_surrogate = 0;
		_cancel_ime();
		if (GetCapture() == _window) {
			ReleaseCapture();
		}
	}
	_pump_messages();
	_mouse_position_valid = false;
	_update_window();
	_drop_input = false;
}

void DisplayServerWindowsNT::_update_window() {
	// Like the 4.x display server, sample the settled client rect after dispatch.
	// A themed window procedure can send WM_SIZE before GetClientRect changes.
	RECT client = {};
	if (_window && !is_minimized() && GetClientRect(_window, &client)) {
		_client_size = Size2i(client.right, client.bottom);
	}
	if (can_draw()) {
		if (_display_change_pending) {
			_display_change_pending = false;
			Main::force_redraw();
		}
		if (_fullscreen_bounds_dirty) {
			_fullscreen_bounds_dirty = false;
			if (_fullscreen && !_set_fullscreen_bounds(_get_monitor(-1))) {
				ERR_PRINT_ONCE("Could not update Windows NT fullscreen bounds.");
			}
		}
		if (!_mouse_passthrough_polygon.empty()) {
			_apply_mouse_passthrough_polygon(_mouse_passthrough_polygon, false);
		}
		// A native resize can produce several WM_SIZE messages before this frame.
		if (_client_size != _render_size) {
			_render_size = _client_size;
			Main::force_redraw();
		}
	}
}

void DisplayServerWindowsNT::stop_events() {
	const bool was_running = _main_loop != nullptr;
	_main_loop = nullptr;
	set_keep_screen_on(false);
	ime_set_active(false);
	if (was_running && _window) {
		DragAcceptFiles(_window, FALSE);
	}
	if (_input) {
		_joypad.close(*_input);
	}
	_mouse_focused = false;
	_mouse_focus_pending = false;
	_release_mouse_clip();
	_update_mouse_cursor();
}

bool DisplayServerWindowsNT::is_quit_requested() const {
	return _quit;
}

bool DisplayServerWindowsNT::can_draw() const {
	return _window && !is_minimized() && _client_size.x > 0 && _client_size.y > 0;
}

HWND DisplayServerWindowsNT::get_window() const {
	return _window;
}

void DisplayServerWindowsNT::set_title(const String &p_title) {
	SetWindowTextW(_window, p_title.c_str());
}

void DisplayServerWindowsNT::set_position(const Point2 &p_position) {
	if (!_fullscreen) {
		SetWindowPos(_window, nullptr, int(p_position.x), int(p_position.y),
				0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
	}
}

Point2 DisplayServerWindowsNT::get_position() const {
	RECT frame;
	return GetWindowRect(_window, &frame) ? Point2(frame.left, frame.top) : Point2();
}

void DisplayServerWindowsNT::set_mouse_passthrough(const PoolVector2Array &p_polygon) {
	if (p_polygon.empty() && _mouse_passthrough_polygon.empty()) {
		return;
	}
	if (_apply_mouse_passthrough_polygon(p_polygon, true)) {
		_mouse_passthrough_polygon = p_polygon;
	}
}

bool DisplayServerWindowsNT::_apply_mouse_passthrough_polygon(const PoolVector2Array &p_polygon, bool p_polygon_changed) {
	HRGN region = nullptr;
	Point2i offset;
	if (!p_polygon.empty()) {
		const int count = p_polygon.size();
		ERR_FAIL_COND_V_MSG(count < 3, false, "Invalid mouse passthrough polygon vertex count.");
		RECT frame;
		POINT client_origin = {};
		ERR_FAIL_COND_V(!GetWindowRect(_window, &frame) || !ClientToScreen(_window, &client_origin), false);
		offset = Point2i(client_origin.x - frame.left, client_origin.y - frame.top);
		if (!p_polygon_changed && offset == _mouse_passthrough_offset) {
			return true;
		}

		// GDI represents region coordinates as signed 27-bit integers.
		const double MIN_REGION_COORDINATE = -(1 << 26);
		const double MAX_REGION_COORDINATE = (1 << 26) - 1;
		Vector<POINT> points;
		ERR_FAIL_COND_V(points.resize(count) != OK, false);
		POINT *native = points.ptrw();
		const PoolVector2Array::Read vertices = p_polygon.read();
		for (int i = 0; i < count; ++i) {
			const double x = double(vertices[i].x) + offset.x;
			const double y = double(vertices[i].y) + offset.y;
			ERR_FAIL_COND_V_MSG(!(x >= MIN_REGION_COORDINATE && x <= MAX_REGION_COORDINATE && y >= MIN_REGION_COORDINATE && y <= MAX_REGION_COORDINATE), false,
					"Mouse passthrough polygon coordinates are outside the Win32 region range.");
			native[i] = { LONG(x), LONG(y) };
		}
		region = CreatePolygonRgn(points.ptr(), count, ALTERNATE);
		ERR_FAIL_NULL_V(region, false);
	}
	// Windows takes ownership of the region after a successful call.
	if (!SetWindowRgn(_window, region, _shown)) {
		if (region) {
			DeleteObject(region);
		}
		ERR_FAIL_V_MSG(false, "Could not set the Windows NT mouse passthrough region.");
	}
	_mouse_passthrough_offset = offset;
	return true;
}

Rect2i DisplayServerWindowsNT::_get_client_rect() const {
	RECT client;
	POINT origin = {};
	if (!GetClientRect(_window, &client) || !ClientToScreen(_window, &origin)) {
		return Rect2i();
	}
	return Rect2i(origin.x, origin.y, client.right, client.bottom);
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

void DisplayServerWindowsNT::set_min_size(const Size2i &p_size) {
	ERR_FAIL_COND_MSG(p_size.x < 0 || p_size.y < 0, "Minimum window size must be non-negative.");
	ERR_FAIL_COND_MSG((_max_size.x > 0 && p_size.x > _max_size.x) || (_max_size.y > 0 && p_size.y > _max_size.y),
			"Minimum window size cannot be greater than maximum window size.");
	_min_size = p_size;
}

Size2i DisplayServerWindowsNT::get_min_size() const {
	return _min_size;
}

void DisplayServerWindowsNT::set_max_size(const Size2i &p_size) {
	ERR_FAIL_COND_MSG(p_size.x < 0 || p_size.y < 0, "Maximum window size must be non-negative.");
	ERR_FAIL_COND_MSG((p_size.x > 0 && p_size.x < _min_size.x) || (p_size.y > 0 && p_size.y < _min_size.y),
			"Maximum window size cannot be less than minimum window size.");
	_max_size = p_size;
}

Size2i DisplayServerWindowsNT::get_max_size() const {
	return _max_size;
}

void DisplayServerWindowsNT::set_size(const Size2i &p_size) {
	if (_fullscreen) {
		return;
	}
	Size2i size(MAX(MAX(1, _min_size.x), p_size.x), MAX(MAX(1, _min_size.y), p_size.y));
	const Size2i limit = _get_size_limit();
	if (limit.x > 0) {
		size.x = MIN(size.x, limit.x);
	}
	if (limit.y > 0) {
		size.y = MIN(size.y, limit.y);
	}
	if (size != _client_size) {
		const Size2i frame = _get_frame_size(size);
		SetWindowPos(_window, nullptr, 0, 0, frame.x, frame.y, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
	}
}

Size2i DisplayServerWindowsNT::get_size() const {
	return _client_size;
}

Size2i DisplayServerWindowsNT::get_frame_size() const {
	RECT frame;
	return GetWindowRect(_window, &frame) ? Size2i(frame.right - frame.left, frame.bottom - frame.top) : Size2i();
}

void DisplayServerWindowsNT::set_minimized(bool p_enabled) {
	if (is_minimized() != p_enabled) {
		ShowWindow(_window, p_enabled ? SW_MINIMIZE : SW_RESTORE);
	}
}

bool DisplayServerWindowsNT::is_minimized() const {
	return IsIconic(_window);
}

void DisplayServerWindowsNT::set_maximized(bool p_enabled) {
	if (!_shown) {
		_start_maximized = p_enabled;
		return;
	}
	if (_fullscreen) {
		return;
	}
	if (is_maximized() != p_enabled) {
		ShowWindow(_window, p_enabled ? SW_MAXIMIZE : SW_RESTORE);
	}
}

bool DisplayServerWindowsNT::is_maximized() const {
	return IsZoomed(_window);
}

bool DisplayServerWindowsNT::is_focused() const {
	return GetFocus() == _window;
}

void DisplayServerWindowsNT::request_attention() {
	FLASHWINFO flash = { sizeof(FLASHWINFO), _window, FLASHW_TRAY, 3, 0 };
	FlashWindowEx(&flash);
}

void DisplayServerWindowsNT::move_to_foreground() {
	SetForegroundWindow(_window);
}

void DisplayServerWindowsNT::set_use_vsync(bool p_enabled) {
#ifdef GLES_ENABLED
	if (_gl_context) {
		_gl_context->set_use_vsync(p_enabled);
	}
#endif // GLES_ENABLED
}

Point2 DisplayServerWindowsNT::get_mouse_position() const {
	return _mouse_position;
}

int DisplayServerWindowsNT::get_mouse_button_state() const {
	return _mouse_buttons;
}

void DisplayServerWindowsNT::_notify(int p_notification) {
	if (_main_loop && !_drop_input) {
		_main_loop->notification(p_notification);
	}
}

LRESULT CALLBACK DisplayServerWindowsNT::_window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam) {
	DisplayServerWindowsNT *display = reinterpret_cast<DisplayServerWindowsNT *>(GetWindowLongPtrW(p_window, GWLP_USERDATA));
	if (p_message == WM_NCCREATE) {
		const CREATESTRUCTW *create = reinterpret_cast<const CREATESTRUCTW *>(p_lparam);
		display = static_cast<DisplayServerWindowsNT *>(create->lpCreateParams);
		display->_window = p_window;
		SetWindowLongPtrW(p_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(display));
	} else if (p_message == WM_NCDESTROY && display) {
		SetWindowLongPtrW(p_window, GWLP_USERDATA, 0);
		display->_window = nullptr;
		display = nullptr;
	}
	if (!display && p_message == WM_DROPFILES) {
		if (p_wparam) {
			DragFinish(reinterpret_cast<HDROP>(p_wparam));
		}
		return 0;
	}
	return display ? display->_handle_message(p_message, p_wparam, p_lparam) : DefWindowProcW(p_window, p_message, p_wparam, p_lparam);
}

LRESULT DisplayServerWindowsNT::_handle_message(UINT p_message, WPARAM p_wparam, LPARAM p_lparam) {
	switch (p_message) {
		case WM_CLOSE:
			_notify(MainLoop::NOTIFICATION_WM_QUIT_REQUEST);
			return 0;
		case WM_DROPFILES:
			if (_drop_input) {
				DragFinish(reinterpret_cast<HDROP>(p_wparam));
			} else {
				_handle_file_drop(p_wparam);
			}
			return 0;
		case WM_DISPLAYCHANGE:
			_display_change_pending = true;
			_fullscreen_bounds_dirty = _fullscreen;
			break;
		case WM_SYSCOMMAND: {
			enum { SCREENSAVER_SECURE = 0x0001 };
			// The display idle request does not inhibit ordinary screen savers.
			if (_keep_screen_on && (p_wparam & 0xfff0) == SC_SCREENSAVE && !(p_wparam & SCREENSAVER_SECURE)) {
				return 0;
			}
		} break;
		case WM_SIZE:
			if (p_wparam != SIZE_MINIMIZED) {
				RECT client;
				if (GetClientRect(_window, &client)) {
					_client_size = Size2i(client.right, client.bottom);
				}
				_fullscreen_bounds_dirty |= _fullscreen;
			}
			_update_mouse_mode();
			return 0;
		case WM_MOVE:
			_update_mouse_mode();
			break;
		case WM_GETMINMAXINFO: {
			if (_fullscreen) {
				break;
			}
			MINMAXINFO *info = reinterpret_cast<MINMAXINFO *>(p_lparam);
			const Size2i limit = _get_size_limit();
			const Size2i frame = _get_frame_size(Size2i());
			if (limit.x > 0) {
				const LONG width = LONG(limit.x) + frame.x;
				info->ptMaxTrackSize.x = MIN(info->ptMaxTrackSize.x, width);
				info->ptMaxSize.x = MIN(info->ptMaxSize.x, width);
			}
			if (limit.y > 0) {
				const LONG height = LONG(limit.y) + frame.y;
				info->ptMaxTrackSize.y = MIN(info->ptMaxTrackSize.y, height);
				info->ptMaxSize.y = MIN(info->ptMaxSize.y, height);
			}
			info->ptMinTrackSize.x = MIN(info->ptMaxTrackSize.x, MAX(info->ptMinTrackSize.x, LONG(_min_size.x) + frame.x));
			info->ptMinTrackSize.y = MIN(info->ptMaxTrackSize.y, MAX(info->ptMinTrackSize.y, LONG(_min_size.y) + frame.y));
			return 0;
		}
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
			Main::force_redraw();
			break;
		case WM_ACTIVATEAPP:
			if (_ignore_unfocused_joypads && !_drop_input) {
				_joypad.set_input_enabled(*_input, p_wparam != FALSE);
			}
			break;
		case WM_SETFOCUS:
			_mouse_focused = true;
			_mouse_focus_pending = true;
			_mouse_position_valid = false;
			_notify(MainLoop::NOTIFICATION_WM_FOCUS_IN);
			break;
		case WM_KILLFOCUS:
			_mouse_focused = false;
			_cancel_ime();
			_mouse_focus_pending = false;
			_mouse_position_valid = false;
			_update_mouse_mode();
			_high_surrogate = 0;
			if (!_drop_input) {
				_release_keys();
				_release_mouse_buttons();
				_input->flush_buffered_events();
			}
			if (GetCapture() == _window) {
				ReleaseCapture();
			}
			_notify(MainLoop::NOTIFICATION_WM_FOCUS_OUT);
			break;
		case WM_SETCURSOR:
			if (LOWORD(p_lparam) == HTCLIENT) {
				_apply_mouse_cursor();
				return TRUE;
			}
			break;
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			if (_main_loop && !_drop_input && p_wparam != VK_PROCESSKEY && (!_ime_composing || p_message == WM_KEYUP || p_message == WM_SYSKEYUP)) {
				_queue_key(p_message, p_wparam, p_lparam);
			}
			break;
		case WM_CHAR:
		case WM_SYSCHAR:
			if (_main_loop && !_drop_input) {
				_queue_character(p_wparam, p_lparam);
			}
			return 0;
		case WM_IME_SETCONTEXT:
			if (_ime_active) {
				// Godot draws preedit in its text controls. Keep the native candidate UI.
				return DefWindowProcW(_window, p_message, p_wparam, p_lparam & ~LPARAM(ISC_SHOWUICOMPOSITIONWINDOW));
			}
			break;
		case WM_IME_STARTCOMPOSITION:
			if (_main_loop && !_drop_input && _ime_active && _mouse_focused) {
				_ime_composing = true;
				_update_ime_position();
			}
			return 0;
		case WM_IME_COMPOSITION:
			if (_main_loop && !_drop_input && _ime_active && _mouse_focused) {
				_handle_ime_composition(p_lparam);
			}
			return 0;
		case WM_IME_ENDCOMPOSITION:
			_ime_composing = false;
			_set_ime_preedit(String(), 0);
			return 0;
		case WM_IME_CHAR:
			if (_main_loop && !_drop_input && _ime_active && _mouse_focused) {
				_queue_character(p_wparam, 0);
			}
			return 0;
		case WM_MOUSEMOVE:
			if (_main_loop && !_drop_input) {
				_send_mouse_motion(Point2(GET_X_LPARAM(p_lparam), GET_Y_LPARAM(p_lparam)));
			}
			return 0;
		case WM_MOUSELEAVE:
			_mouse_inside = false;
			_mouse_position_valid = false;
			_notify(MainLoop::NOTIFICATION_WM_MOUSE_EXIT);
			return 0;
		case WM_CANCELMODE:
		case WM_CAPTURECHANGED:
			if (_main_loop && !_drop_input) {
				_release_mouse_buttons();
			}
			break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_XBUTTONDBLCLK: {
			if (_drop_input) {
				return p_message >= WM_XBUTTONDOWN ? TRUE : 0;
			}
			if (!_main_loop) {
				break;
			}
			const bool side_button = p_message >= WM_XBUTTONDOWN;
			int button = BUTTON_LEFT;
			if (side_button) {
				const WORD native_button = GET_XBUTTON_WPARAM(p_wparam);
				if (native_button != XBUTTON1 && native_button != XBUTTON2) {
					break;
				}
				button = native_button == XBUTTON1 ? BUTTON_XBUTTON1 : BUTTON_XBUTTON2;
			} else if (p_message >= WM_MBUTTONDOWN) {
				button = BUTTON_MIDDLE;
			} else if (p_message >= WM_RBUTTONDOWN) {
				button = BUTTON_RIGHT;
			}
			const bool pressed = p_message != WM_LBUTTONUP && p_message != WM_RBUTTONUP && p_message != WM_MBUTTONUP && p_message != WM_XBUTTONUP;
			const bool double_click = p_message == WM_LBUTTONDBLCLK || p_message == WM_RBUTTONDBLCLK || p_message == WM_MBUTTONDBLCLK || p_message == WM_XBUTTONDBLCLK;
			_send_mouse_button(button, Point2(GET_X_LPARAM(p_lparam), GET_Y_LPARAM(p_lparam)), pressed, double_click);
			if (pressed && GetCapture() != _window) {
				SetCapture(_window);
			} else if (!_mouse_buttons && GetCapture() == _window) {
				ReleaseCapture();
			}
			return side_button ? TRUE : 0;
		}
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL: {
			if (_drop_input) {
				return 0;
			}
			if (!_main_loop) {
				break;
			}
			const int delta = GET_WHEEL_DELTA_WPARAM(p_wparam);
			if (delta == 0) {
				return 0;
			}
			POINT point = { GET_X_LPARAM(p_lparam), GET_Y_LPARAM(p_lparam) };
			ScreenToClient(_window, &point);
			const Point2 position(point.x, point.y);
			int button;
			if (p_message == WM_MOUSEWHEEL) {
				button = delta > 0 ? BUTTON_WHEEL_UP : BUTTON_WHEEL_DOWN;
			} else {
				button = delta > 0 ? BUTTON_WHEEL_RIGHT : BUTTON_WHEEL_LEFT;
			}
			const float factor = ABS(delta) / float(WHEEL_DELTA);
			_send_mouse_button(button, position, true, false, factor);
			_send_mouse_button(button, position, false, false, factor);
			return 0;
		}
	}
	return DefWindowProcW(_window, p_message, p_wparam, p_lparam);
}

DisplayServerWindowsNT::~DisplayServerWindowsNT() {
	stop_events();
#ifdef GLES_ENABLED
	if (_gl_context) {
		memdelete(_gl_context);
	}
#endif // GLES_ENABLED
	if (_small_icon || _large_icon) {
		_replace_window_icons(nullptr, nullptr);
	}
	if (_window) {
		// The main loop is gone when Windows sends the final focus messages.
		SetWindowLongPtrW(_window, GWLP_USERDATA, 0);
		DestroyWindow(_window);
	}
	for (HCURSOR cursor : _custom_cursors) {
		_destroy_custom_cursor(cursor);
	}
}
