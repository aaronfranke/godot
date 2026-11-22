#pragma once

#include "core/os/os.h"
#include "drivers/win_legacy/common/input/joypad_winmm.h"
#include <windows.h>

class FixedFunctionWindows;
class InputDefault;

// A private display adapter for OS_Windows9x; Godot 3 keeps its public window API on OS.
class DisplayServerWindows9x {
public:
	typedef int WindowID;
	enum {
		MAIN_WINDOW_ID = 0,
		INVALID_WINDOW_ID = -1,
	};
	enum WindowMode {
		WINDOW_MODE_WINDOWED,
		WINDOW_MODE_MINIMIZED,
		WINDOW_MODE_MAXIMIZED,
		WINDOW_MODE_FULLSCREEN,
		WINDOW_MODE_EXCLUSIVE_FULLSCREEN,
	};
	enum WindowFlags {
		WINDOW_FLAG_RESIZE_DISABLED,
		WINDOW_FLAG_BORDERLESS,
		WINDOW_FLAG_ALWAYS_ON_TOP,
		WINDOW_FLAG_MINIMIZE_DISABLED,
		WINDOW_FLAG_MAXIMIZE_DISABLED,
		WINDOW_FLAG_MAX,
	};
	enum WindowFlagsBit {
		WINDOW_FLAG_RESIZE_DISABLED_BIT = 1 << WINDOW_FLAG_RESIZE_DISABLED,
		WINDOW_FLAG_BORDERLESS_BIT = 1 << WINDOW_FLAG_BORDERLESS,
		WINDOW_FLAG_ALWAYS_ON_TOP_BIT = 1 << WINDOW_FLAG_ALWAYS_ON_TOP,
		WINDOW_FLAG_MINIMIZE_DISABLED_BIT = 1 << WINDOW_FLAG_MINIMIZE_DISABLED,
		WINDOW_FLAG_MAXIMIZE_DISABLED_BIT = 1 << WINDOW_FLAG_MAXIMIZE_DISABLED
	};
	enum VSyncMode {
		VSYNC_DISABLED,
		VSYNC_ENABLED,
	};
	enum WindowEvent {
		WINDOW_EVENT_MOUSE_ENTER,
		WINDOW_EVENT_MOUSE_EXIT,
		WINDOW_EVENT_FOCUS_IN,
		WINDOW_EVENT_FOCUS_OUT,
		WINDOW_EVENT_CLOSE_REQUEST,
	};

private:
	struct KeyEvent {
		Ref<InputEventKey> event;
		uint16_t scan;
	};
	JoypadWinMM _joypad;
	Vector<KeyEvent> _keys;
	WINDOWPLACEMENT _windowed_placement = {};
	Rect2i _last_client_rect;
	Rect2i _mouse_clip_rect;
	Size2i _min_size;
	Size2i _max_size;
	Point2i _last_mouse_position;
	HWND _window = nullptr;
	HCURSOR _custom_cursors[OS::CURSOR_MAX] = {};
	FixedFunctionWindows *_renderer = nullptr;
	InputDefault *_input = nullptr;
	MainLoop *_main_loop = nullptr;
	uint32_t _buttons = 0;
	uint32_t _flags = 0;
	WindowMode _initial_mode = WINDOW_MODE_WINDOWED;
	VSyncMode _vsync = VSYNC_ENABLED;
	OS::CursorShape _cursor_shape = OS::CURSOR_ARROW;
	OS::MouseMode _mouse_mode = OS::MOUSE_MODE_VISIBLE;
	bool _shown = false;
	bool _fullscreen = false;
	bool _drop_input = false;
	bool _mouse_inside = false;
	bool _mouse_position_valid = false;
	bool _display_changed = false;
	bool _mouse_focus_pending = false;
	bool _ignore_unfocused_joypads = false;
	uint8_t _character_length = 0;
	uint8_t _character_expected = 0;
	char _character_bytes[4] = {};

	static DWORD _style(uint32_t p_flags);
	static int _get_driver_index(int p_driver);
	Error _initialize(const String &p_driver, WindowMode p_mode, VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Size2i &p_size);
	Rect2i _get_client_rect() const;
	Size2i _get_frame_size(const Size2i &p_size) const;
	Size2i _get_size_limit() const;
	void _update_client_rect();
	static LRESULT CALLBACK _window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam);
	static HCURSOR _get_system_cursor(OS::CursorShape p_shape);
	static void _destroy_custom_cursor(HCURSOR p_cursor);
	void _handle_file_drop(WPARAM p_drop);
	static void _set_modifiers(Ref<InputEventWithModifiers> p_event);
	void _queue_key(UINT p_message, WPARAM p_key, LPARAM p_flags);
	void _queue_character(uint8_t p_byte, LPARAM p_flags);
	void _flush_keys();
	void _send_window_event(WindowEvent p_event);
	void _release_mouse_clip();
	bool _recenter_mouse();
	void _update_mouse_mode(bool p_recenter);
	void _apply_mouse_cursor();
	void _send_mouse_motion(const Point2i &p_position);
	void _send_mouse_button(int p_button, const Point2i &p_position, bool p_pressed, bool p_double_click = false, float p_factor = 1.0f);
	void _release_mouse_buttons();
	LRESULT _handle_message(UINT p_message, WPARAM p_wparam, LPARAM p_lparam);
	void _pump_messages();

public:
	~DisplayServerWindows9x();
	int get_screen_count() const;
	int get_primary_screen() const;
	Point2i screen_get_position(int p_screen) const;
	Size2i screen_get_size(int p_screen) const;
	Rect2i screen_get_usable_rect(int p_screen) const;
	int screen_get_dpi(int p_screen) const;
	float screen_get_refresh_rate(int p_screen) const;
	void show(MainLoop &p_loop);
	void window_set_title(const String &p_title, WindowID p_window);
	int window_get_current_screen(WindowID p_window) const;
	void window_set_current_screen(int p_screen, WindowID p_window);
	Point2i window_get_position(WindowID p_window) const;
	Point2i window_get_position_with_decorations(WindowID p_window) const;
	void window_set_position(const Point2i &p_position, WindowID p_window);
	void window_set_max_size(Size2i p_size, WindowID p_window);
	Size2i window_get_max_size(WindowID p_window) const;
	void window_set_min_size(Size2i p_size, WindowID p_window);
	Size2i window_get_min_size(WindowID p_window) const;
	void window_set_size(Size2i p_size, WindowID p_window);
	Size2i window_get_size(WindowID p_window) const;
	Size2i window_get_size_with_decorations(WindowID p_window) const;
	void window_set_mode(WindowMode p_mode, WindowID p_window);
	WindowMode window_get_mode(WindowID p_window) const;
	void window_set_vsync_mode(VSyncMode p_mode, WindowID p_window);
	VSyncMode window_get_vsync_mode(WindowID p_window) const;
	bool window_is_maximize_allowed(WindowID p_window) const;
	void window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window);
	bool window_get_flag(WindowFlags p_flag, WindowID p_window) const;
	void window_request_attention(WindowID p_window);
	void window_move_to_foreground(WindowID p_window);
	bool window_is_focused(WindowID p_window) const;
	bool window_can_draw(WindowID p_window) const;
	bool can_any_window_draw() const;
	static int get_driver_count();
	static const char *get_driver_name(int p_driver);
	static const char *get_driver_enum_name(int p_driver);
	Error initialize(const OS::VideoMode &p_mode, int p_driver, InputDefault &p_input);
	Error get_initialization_error() const;
	bool should_render_directly_to_window() const;
	void stop_events();
	Point2i get_local_mouse_position() const { return _last_mouse_position; }
	HWND get_window() const { return _window; }
	void clipboard_set(const String &p_text);
	String clipboard_get() const;
	bool clipboard_has() const;
	void cursor_set_custom_image(const Ref<Resource> &p_cursor, OS::CursorShape p_shape, const Vector2 &p_hotspot);
	Point2i mouse_get_position() const;
	void mouse_set_mode(OS::MouseMode p_mode);
	OS::MouseMode mouse_get_mode() const;
	uint32_t mouse_get_button_state() const;
	void warp_mouse(const Point2i &p_position);
	void cursor_set_shape(OS::CursorShape p_shape);
	OS::CursorShape cursor_get_shape() const;
	void process_events();
	void force_process_and_drop_events();
};
