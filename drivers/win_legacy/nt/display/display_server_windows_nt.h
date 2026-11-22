#pragma once

#include "drivers/win_legacy/common/input/joypad_winmm.h"

// Godot includes.
#include "core/map.h"
#include "core/os/os.h"
#ifdef FIXED_FUNCTION_ENABLED
#include "platform/windows/fixed_function/fixed_function_windows.h"
#endif // FIXED_FUNCTION_ENABLED

#ifdef GLES_ENABLED
class GLContextWindowsNT;
#endif // GLES_ENABLED

#include <imm.h>
#include <windows.h>

class InputDefault;

class DisplayServerWindowsNT {
	struct KeyEvent {
		Ref<InputEventKey> event;
		uint16_t scan;
	};

	struct KeyScancodes {
		uint32_t logical;
		uint32_t physical;
	};

#ifdef FIXED_FUNCTION_ENABLED
	FixedFunctionWindows *_renderer = nullptr;
#endif // FIXED_FUNCTION_ENABLED
#ifdef GLES_ENABLED
	GLContextWindowsNT *_gl_context = nullptr;
#endif // GLES_ENABLED
	JoypadWinMM _joypad;
	Vector<KeyEvent> _keys;
	Map<uint16_t, KeyScancodes> _pressed_keys;
	PoolVector2Array _mouse_passthrough_polygon;
	String _ime_text;
	WINDOWPLACEMENT _windowed_placement = {};
	Rect2i _mouse_clip_rect;
	Size2i _client_size;
	Size2i _render_size;
	Size2i _min_size;
	Size2i _max_size;
	Point2 _mouse_position;
	Point2i _mouse_passthrough_offset;
	Point2i _ime_position;
	HCURSOR _custom_cursors[OS::CURSOR_MAX] = {};
	HICON _small_icon = nullptr;
	HICON _large_icon = nullptr;
	HWND _window = nullptr;
	HIMC _ime_context = nullptr;
	InputDefault *_input = nullptr;
	MainLoop *_main_loop = nullptr;
	OS::MouseMode _mouse_mode = OS::MOUSE_MODE_VISIBLE;
	OS::CursorShape _cursor_shape = OS::CURSOR_ARROW;
	DWORD _windowed_style = 0;
	int _mouse_buttons = 0;
	int _ime_cursor = 0;
	uint16_t _high_surrogate = 0;
	bool _keep_screen_on = false;
	bool _ime_active = false;
	bool _ime_composing = false;
	bool _ignore_unfocused_joypads = false;
	bool _mouse_focused = false;
	bool _mouse_focus_pending = false;
	bool _mouse_inside = false;
	bool _mouse_position_valid = false;
	bool _shown = false;
	bool _start_fullscreen = false;
	bool _start_maximized = false;
	bool _fullscreen = false;
	bool _display_change_pending = false;
	bool _fullscreen_bounds_dirty = false;
	bool _quit = false;
	bool _drop_input = false;
	bool _input_reset_pending = false;
	bool _native_prompt_open = false;

	static LRESULT CALLBACK _window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam);
	LRESULT _handle_message(UINT p_message, WPARAM p_wparam, LPARAM p_lparam);
	void _handle_file_drop(WPARAM p_drop);
	void _notify(int p_notification);
	void _pump_messages();
	void _update_window();
	Rect2i _get_client_rect() const;
	Size2i _get_frame_size(const Size2i &p_client_size) const;
	Size2i _get_size_limit() const;
	static DWORD _get_windowed_frame_style(bool p_borderless, bool p_resizable);
	bool _apply_mouse_passthrough_polygon(const PoolVector2Array &p_polygon, bool p_polygon_changed);
	static BOOL CALLBACK _enum_monitor(HMONITOR p_monitor, HDC, LPRECT, LPARAM p_data);
	static Vector<HMONITOR> _get_monitors();
	static HMONITOR _get_window_monitor(HWND p_window, DWORD p_flags);
	static bool _get_monitor_info(HMONITOR p_monitor, MONITORINFO *r_info);
	static Rect2i _get_monitor_rect(HMONITOR p_monitor, bool p_work_area);
	HMONITOR _get_monitor(int p_screen) const;
	bool _set_fullscreen_bounds(HMONITOR p_monitor, UINT p_flags = 0);
	void _enter_fullscreen();
	void _leave_fullscreen();

	void _queue_key(UINT p_message, WPARAM p_key, LPARAM p_flags);
	void _queue_character(uint16_t p_character, LPARAM p_flags);
	void _flush_keys();
	void _release_keys();
	static Vector<HKL> _get_keyboard_layouts();
	static String _get_keyboard_locale_info(HKL p_layout, LCTYPE p_type);
	static String _get_keyboard_layout_value(HKEY p_layout, const WCHAR *p_name);
	static String _get_keyboard_layout_id(HKEY p_root, HKL p_layout);
	Vector<WCHAR> _read_ime_string(DWORD p_type) const;
	void _set_ime_preedit(const String &p_text, int p_cursor);
	void _cancel_ime();
	void _handle_ime_composition(LPARAM p_flags);
	void _update_ime_position();
	bool _mouse_hides_cursor() const;
	static HCURSOR _get_system_cursor(OS::CursorShape p_shape);
	static HICON _create_image_icon(const Ref<Image> &p_image, const Size2i &p_size);
	static HICON _create_ico_frame(Vector<uint8_t> &p_bytes, const Size2i &p_source_size, const Size2i &p_target_size);
	void _replace_window_icons(HICON p_small, HICON p_large);
	static void _destroy_custom_cursor(HCURSOR p_cursor);
	void _apply_mouse_cursor();
	void _update_mouse_cursor();
	void _release_mouse_clip();
	bool _recenter_mouse();
	void _update_mouse_mode(bool p_recenter = false);
	void _release_mouse_buttons();
	void _send_mouse_motion(const Point2 &p_position);
	void _send_mouse_button(int p_button, const Point2 &p_position, bool p_pressed, bool p_double_click = false, float p_factor = 1.0f);
	Error _show_native_prompt(const String &p_title, const String &p_description, const Vector<String> &p_buttons, const String *p_existing_text, Object *p_object, const String &p_callback);

public:
	static int get_driver_count();
	static int get_driver_index(int p_driver);
	static bool is_gl_driver(int p_driver);
	static const char *get_driver_name(int p_driver);
	static const char *get_driver_enum_name(int p_driver);

	Error initialize(const OS::VideoMode &p_mode, int p_driver, InputDefault &p_input);
	Error get_initialization_error() const;
	void show(MainLoop &p_main_loop);
	void process_events();
	void process_and_drop_events();
	void stop_events();
	bool is_quit_requested() const;
	bool can_draw() const;

	void clipboard_set(const String &p_text);
	String clipboard_get() const;
	Error dialog_show(String p_title, String p_description, Vector<String> p_buttons, Object *p_object, const String &p_callback);
	Error dialog_input_text(String p_title, String p_description, String p_partial, Object *p_object, const String &p_callback);
	void ime_set_active(bool p_active);
	void ime_set_position(const Point2i &p_position);
	String ime_get_text() const;
	Point2i ime_get_selection() const;
	OS::LatinKeyboardVariant get_latin_keyboard_variant() const;
	int keyboard_get_layout_count() const;
	int keyboard_get_current_layout() const;
	void keyboard_set_current_layout(int p_index);
	String keyboard_get_layout_language(int p_index) const;
	String keyboard_get_layout_name(int p_index) const;
	uint32_t keyboard_get_scancode_from_physical(uint32_t p_scancode) const;

	HWND get_window() const;
	bool should_render_directly_to_window() const;
	void make_rendering_thread();
	void release_rendering_thread();
	void swap_buffers();
	void set_title(const String &p_title);
	void set_icon(const Ref<Image> &p_icon);
	void set_native_icon(const String &p_filename);
	void set_position(const Point2 &p_position);
	void set_mouse_passthrough(const PoolVector2Array &p_polygon);
	Point2 get_position() const;
	void set_size(const Size2i &p_size);
	Size2i get_size() const;
	Size2i get_frame_size() const;
	void set_min_size(const Size2i &p_size);
	Size2i get_min_size() const;
	void set_max_size(const Size2i &p_size);
	Size2i get_max_size() const;
	void set_fullscreen(bool p_enabled);
	bool is_fullscreen() const;
	bool set_windowed_frame(bool p_borderless, bool p_resizable);
	void set_always_on_top(bool p_enabled);
	bool is_always_on_top() const;
	int get_screen_count() const;
	int get_current_screen() const;
	void set_current_screen(int p_screen);
	Point2i get_screen_position(int p_screen) const;
	Size2i get_screen_size(int p_screen) const;
	int get_screen_dpi() const;
	float get_screen_refresh_rate(int p_screen, float p_fallback) const;
	bool set_keep_screen_on(bool p_enabled);
	void set_minimized(bool p_enabled);
	bool is_minimized() const;
	void set_maximized(bool p_enabled);
	bool is_maximized() const;
	bool is_focused() const;
	void request_attention();
	void move_to_foreground();
	void set_use_vsync(bool p_enabled);
	void set_mouse_mode(OS::MouseMode p_mode);
	OS::MouseMode get_mouse_mode() const;
	void cursor_set_shape(OS::CursorShape p_shape);
	OS::CursorShape cursor_get_shape() const;
	void cursor_set_custom_image(const RES &p_cursor, OS::CursorShape p_shape, const Vector2 &p_hotspot);
	void warp_mouse_position(const Point2 &p_position);
	Point2 get_mouse_position() const;
	int get_mouse_button_state() const;

	virtual ~DisplayServerWindowsNT();
};
