#pragma once

#include "core/input/input_event.h"
#include "drivers/win_legacy/common/input/joypad_winmm.h"
#include "servers/display/display_server.h"

#include <windows.h>

#include <imm.h>

class FixedFunctionWindows;
class GLContextWindowsNT;
class NativeMenu;

class DisplayServerWindowsNT : public DisplayServer {
	GDSOFTCLASS(DisplayServerWindowsNT, DisplayServer);

	struct KeyEvent {
		Ref<InputEventKey> event;
		uint16_t scan = 0;
	};
	struct FileDialogRequest;
	struct StatusIndicatorData {
		Callable callback;
		String tooltip;
		RID menu;
		HICON icon = nullptr;
	};
	enum { WM_STATUS_INDICATOR = WM_APP + 1 };

	Callable _rect_changed;
	Callable _window_event;
	Callable _input_event;
	Callable _drop_files_callback;
	Vector<KeyEvent> _keys;
	Vector<Vector2> _mouse_passthrough_polygon;
	JoypadWinMM _joypad;
	HashMap<DisplayServerEnums::IndicatorID, StatusIndicatorData> _status_indicators;
	HCURSOR _custom_cursors[DisplayServerEnums::CURSOR_MAX] = {};
	String _ime_text;
	ObjectID _instance_id;
	WINDOWPLACEMENT _windowed_placement = {};
	Rect2i _last_client_rect;
	Rect2i _mouse_clip_rect;
	Size2i _min_size;
	Size2i _max_size;
	Point2i _last_mouse_position;
	Point2i _mouse_passthrough_offset;
	Rect2i _ime_rect;
	Point2i _ime_position;
	HWND _window = nullptr;
	HICON _small_icon = nullptr;
	HICON _large_icon = nullptr;
	HIMC _ime_context = nullptr;
	HKL _keyboard_label_layout = nullptr;
	Key _keyboard_letter_labels['Z' - 'A' + 1] = {};
	FixedFunctionWindows *_renderer = nullptr;
	GLContextWindowsNT *_gl_context = nullptr;
	NativeMenu *_native_menu = nullptr;
	FileDialogRequest *_file_dialog_request = nullptr;
	BitField<MouseButtonMask> _buttons;
	uint32_t _flags = 0;
	uint32_t _next_status_indicator = 1;
	UINT _taskbar_created_message = 0;
	int _ime_cursor = 0;
	uint16_t _high_surrogate = 0;
	DisplayServerEnums::VSyncMode _vsync = DisplayServerEnums::VSYNC_ENABLED;
	DisplayServerEnums::WindowMode _initial_mode = DisplayServerEnums::WINDOW_MODE_WINDOWED;
	DisplayServerEnums::MouseMode _mouse_mode = DisplayServerEnums::MOUSE_MODE_VISIBLE;
	DisplayServerEnums::MouseMode _mouse_mode_base = DisplayServerEnums::MOUSE_MODE_VISIBLE;
	DisplayServerEnums::MouseMode _mouse_mode_override = DisplayServerEnums::MOUSE_MODE_VISIBLE;
	DisplayServerEnums::CursorShape _cursor_shape = DisplayServerEnums::CURSOR_ARROW;
	bool _shown = false;
	bool _fullscreen = false;
	bool _fullscreen_bounds_dirty = false;
	bool _display_change_pending = false;
	bool _mouse_inside = false;
	bool _mouse_focused = false;
	bool _mouse_focus_pending = false;
	bool _mouse_position_valid = false;
	bool _mouse_mode_override_enabled = false;
	bool _drop_input = false;
	bool _keep_screen_on = false;
	bool _native_prompt_open = false;
	bool _ime_active = false;
	bool _ime_composing = false;

	static LRESULT CALLBACK _window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam);
	static void _dispatch_input(const Ref<InputEvent> &p_event);
	static DWORD _style(uint32_t p_flags);
	static BOOL CALLBACK _enum_monitor(HMONITOR p_monitor, HDC p_dc, LPRECT p_rect, LPARAM p_data);
	static Vector<HMONITOR> _get_monitors();
	static HMONITOR _get_primary_monitor();
	static HMONITOR _get_window_monitor(HWND p_window, DWORD p_flags);
	static HMONITOR _get_point_monitor(const POINT &p_position, DWORD p_flags);
	static bool _get_monitor_info(HMONITOR p_monitor, MONITORINFO *r_info);
	static Point2i _get_screens_origin();
	static Rect2i _get_monitor_rect(HMONITOR p_monitor, bool p_work_area = false);

	Error _initialize(const String &p_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Size2i &p_size, int p_screen);
	LRESULT _handle_message(UINT p_message, WPARAM p_wparam, LPARAM p_lparam);
	HMONITOR _get_monitor(int p_screen) const;
	Rect2i _get_client_rect() const;
	Size2i _get_frame_size(const Size2i &p_client_size) const;
	Size2i _get_size_limit() const;
	bool _set_fullscreen_bounds(HMONITOR p_monitor, UINT p_flags = 0);
	void _enter_fullscreen();
	void _leave_fullscreen(int p_show_command);
	bool _apply_mouse_passthrough_polygon(const Vector<Vector2> &p_polygon, const Point2i &p_client_origin, bool p_polygon_changed);
	void _update_client_rect(bool p_force_callback = false);
	void _send_window_event(DisplayServerEnums::WindowEvent p_event);
	void _handle_file_drop(WPARAM p_drop);
	Error _show_native_prompt(const String &p_title, const String &p_description, const Vector<String> &p_buttons, const String *p_existing_text, const Callable &p_callback);
	void _process_file_dialog();
	void _stop_file_dialog();
	void _pump_messages();
	void _queue_key(UINT p_message, WPARAM p_key, LPARAM p_flags);
	void _queue_character(char16_t p_unit, LPARAM p_flags);
	void _flush_keys();
	static Vector<HKL> _get_keyboard_layouts();
	static String _get_keyboard_locale_info(HKL p_layout, LCTYPE p_type);
	static String _get_keyboard_layout_value(HKEY p_layout, const WCHAR *p_name);
	static String _get_keyboard_layout_id(HKEY p_root, HKL p_layout);
	void _update_keyboard_labels();
	void _update_ime_position();
	void _cancel_ime();
	void _set_ime_preedit(const String &p_text, int p_cursor);
	void _handle_ime_composition(LPARAM p_flags);
	Vector<char16_t> _read_ime_string(DWORD p_type) const;
	bool _mouse_hides_cursor() const;
	void _update_mouse_cursor();
	void _apply_mouse_cursor();
	static HCURSOR _get_system_cursor(DisplayServerEnums::CursorShape p_shape);
	static HICON _create_icon_or_cursor(const Ref<Image> &p_image, const Point2i *p_hotspot = nullptr);
	static void _destroy_custom_cursor(HCURSOR p_cursor);
	static HICON _create_image_icon(const Ref<Image> &p_image, const Size2i &p_size);
	static HICON _create_ico_frame(Vector<uint8_t> &p_bytes, const Size2i &p_source_size, const Size2i &p_target_size);
	void _replace_window_icons(HICON p_small, HICON p_large);
	static HICON _make_status_indicator_icon(const Ref<Texture2D> &p_texture);
	bool _write_status_indicator(DisplayServerEnums::IndicatorID p_id, const StatusIndicatorData &p_indicator, DWORD p_command) const;
	Rect2i _get_native_status_indicator_rect(DisplayServerEnums::IndicatorID p_id) const;
	void _dispatch_status_indicator(DisplayServerEnums::IndicatorID p_id, LPARAM p_event);
	void _restore_status_indicators();
	void _clear_status_indicators();
	static UINT _get_clipboard_png_format();
	static bool _valid_clipboard_image_size(int64_t p_width, int64_t p_height);
	static Ref<Image> _decode_clipboard_png(const uint8_t *p_data, size_t p_size);
	static Ref<Image> _decode_clipboard_dib(const uint8_t *p_data, size_t p_size);
	static Ref<Image> _read_clipboard_bitmap(HBITMAP p_bitmap);
	void _update_effective_mouse_mode();
	void _update_mouse_mode(bool p_recenter = false);
	void _release_mouse_clip();
	bool _recenter_mouse();
	void _send_mouse_motion(const Point2i &p_position);
	void _send_mouse_button(MouseButton p_button, const Point2i &p_position, bool p_pressed, bool p_double_click, float p_factor = 1.0f);
	void _release_mouse_buttons();

protected:
	typedef DisplayServerWindowsNT *(*CreateInstanceFunction)();

	static Vector<String> _get_rendering_drivers();
	static DisplayServer *_create_display_server(const String &p_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_size, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error, CreateInstanceFunction p_create_instance);

public:
	~DisplayServerWindowsNT() override;

	String get_name() const override;
	bool has_feature(DisplayServerEnums::Feature p_feature) const override;
	void process_events() override;
	void force_process_and_drop_events() override;
	void release_rendering_thread() override;
	void swap_buffers() override;
	void gl_window_make_current(DisplayServerEnums::WindowID p_window) override;

	int get_screen_count() const override;
	int get_primary_screen() const override;
	Point2i screen_get_position(int p_screen) const override;
	Size2i screen_get_size(int p_screen) const override;
	Rect2i screen_get_usable_rect(int p_screen) const override;
	int screen_get_dpi(int p_screen) const override;
	float screen_get_refresh_rate(int p_screen) const override;
	void screen_set_keep_on(bool p_enable) override;
	bool screen_is_kept_on() const override;

	BitField<MouseButtonMask> mouse_get_button_state() const override;
	Point2i mouse_get_position() const override;
	void mouse_set_mode(DisplayServerEnums::MouseMode p_mode) override;
	DisplayServerEnums::MouseMode mouse_get_mode() const override;
	void mouse_set_mode_override(DisplayServerEnums::MouseMode p_mode) override;
	DisplayServerEnums::MouseMode mouse_get_mode_override() const override;
	void mouse_set_mode_override_enabled(bool p_enabled) override;
	bool mouse_is_mode_override_enabled() const override;
	void warp_mouse(const Point2i &p_position) override;
	void cursor_set_shape(DisplayServerEnums::CursorShape p_shape) override;
	DisplayServerEnums::CursorShape cursor_get_shape() const override;
	void cursor_set_custom_image(const Ref<Resource> &p_cursor, DisplayServerEnums::CursorShape p_shape, const Vector2 &p_hotspot) override;

	int keyboard_get_layout_count() const override;
	int keyboard_get_current_layout() const override;
	void keyboard_set_current_layout(int p_index) override;
	String keyboard_get_layout_language(int p_index) const override;
	String keyboard_get_layout_name(int p_index) const override;
	Key keyboard_get_keycode_from_physical(Key p_keycode) const override;
	Key keyboard_get_label_from_physical(Key p_keycode) const override;

	void clipboard_set(const String &p_text) override;
	String clipboard_get() const override;
	bool clipboard_has() const override;
	Ref<Image> clipboard_get_image() const override;
	bool clipboard_has_image() const override;
	void set_native_icon(const String &p_filename) override;
	void set_icon(const Ref<Image> &p_icon) override;
	DisplayServerEnums::IndicatorID create_status_indicator(const Ref<Texture2D> &p_icon, const String &p_tooltip, const Callable &p_callback) override;
	void status_indicator_set_icon(DisplayServerEnums::IndicatorID p_id, const Ref<Texture2D> &p_icon) override;
	void status_indicator_set_tooltip(DisplayServerEnums::IndicatorID p_id, const String &p_tooltip) override;
	void status_indicator_set_menu(DisplayServerEnums::IndicatorID p_id, const RID &p_menu_rid) override;
	void status_indicator_set_callback(DisplayServerEnums::IndicatorID p_id, const Callable &p_callback) override;
	Rect2 status_indicator_get_rect(DisplayServerEnums::IndicatorID p_id) const override;
	void delete_status_indicator(DisplayServerEnums::IndicatorID p_id) override;
	Error dialog_show(String p_title, String p_description, Vector<String> p_buttons, const Callable &p_callback) override;
	Error dialog_input_text(String p_title, String p_description, String p_partial, const Callable &p_callback) override;
	Error file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, DisplayServerEnums::FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback, DisplayServerEnums::WindowID p_window_id) override;

	String ime_get_text() const override;
	Point2i ime_get_selection() const override;
	void window_set_ime_active(bool p_active, DisplayServerEnums::WindowID p_window) override;
	void window_set_ime_position(const Point2i &p_position, DisplayServerEnums::WindowID p_window) override;
	void window_set_ime_rect(const Rect2i &p_rect, DisplayServerEnums::WindowID p_window) override;

	Vector<DisplayServerEnums::WindowID> get_window_list() const override;
	DisplayServerEnums::WindowID get_window_at_screen_position(const Point2i &p_position) const override;
	void show_window(DisplayServerEnums::WindowID p_window) override;
	int64_t window_get_native_handle(DisplayServerEnums::HandleType p_type, DisplayServerEnums::WindowID p_window) const override;
	void window_attach_instance_id(ObjectID p_instance, DisplayServerEnums::WindowID p_window) override;
	ObjectID window_get_attached_instance_id(DisplayServerEnums::WindowID p_window) const override;
	void window_set_rect_changed_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) override;
	void window_set_window_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) override;
	void window_set_input_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) override;
	void window_set_input_text_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) override {}
	void window_set_drop_files_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) override;
	void window_set_mouse_passthrough(const Vector<Vector2> &p_region, DisplayServerEnums::WindowID p_window) override;
	void window_set_title(const String &p_title, DisplayServerEnums::WindowID p_window) override;
	void window_set_icon(const Ref<Image> &p_icon, DisplayServerEnums::WindowID p_window) override;
	int window_get_current_screen(DisplayServerEnums::WindowID p_window) const override;
	void window_set_current_screen(int p_screen, DisplayServerEnums::WindowID p_window) override;
	Point2i window_get_position(DisplayServerEnums::WindowID p_window) const override;
	Point2i window_get_position_with_decorations(DisplayServerEnums::WindowID p_window) const override;
	void window_set_position(const Point2i &p_position, DisplayServerEnums::WindowID p_window) override;
	void window_set_transient(DisplayServerEnums::WindowID p_window, DisplayServerEnums::WindowID p_parent) override {}
	void window_set_max_size(Size2i p_size, DisplayServerEnums::WindowID p_window) override;
	Size2i window_get_max_size(DisplayServerEnums::WindowID p_window) const override;
	void window_set_min_size(Size2i p_size, DisplayServerEnums::WindowID p_window) override;
	Size2i window_get_min_size(DisplayServerEnums::WindowID p_window) const override;
	void window_set_size(Size2i p_size, DisplayServerEnums::WindowID p_window) override;
	Size2i window_get_size(DisplayServerEnums::WindowID p_window) const override;
	Size2i window_get_size_with_decorations(DisplayServerEnums::WindowID p_window) const override;
	void window_set_mode(DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::WindowID p_window) override;
	DisplayServerEnums::WindowMode window_get_mode(DisplayServerEnums::WindowID p_window) const override;
	void window_set_vsync_mode(DisplayServerEnums::VSyncMode p_mode, DisplayServerEnums::WindowID p_window) override;
	DisplayServerEnums::VSyncMode window_get_vsync_mode(DisplayServerEnums::WindowID p_window) const override;
	bool window_is_maximize_allowed(DisplayServerEnums::WindowID p_window) const override;
	void window_set_flag(DisplayServerEnums::WindowFlags p_flag, bool p_enabled, DisplayServerEnums::WindowID p_window) override;
	bool window_get_flag(DisplayServerEnums::WindowFlags p_flag, DisplayServerEnums::WindowID p_window) const override;
	void window_request_attention(DisplayServerEnums::WindowID p_window) override;
	void window_move_to_foreground(DisplayServerEnums::WindowID p_window) override;
	bool window_is_focused(DisplayServerEnums::WindowID p_window) const override;
	bool window_can_draw(DisplayServerEnums::WindowID p_window) const override;
	bool can_any_window_draw() const override;
};
