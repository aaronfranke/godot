#pragma once

#include "core/input/input_event.h"
#include "drivers/win_legacy/common/input/joypad_winmm.h"
#include "servers/display/display_server.h"

#include <windows.h>

class FixedFunctionWindows;

class DisplayServerWindows9x : public DisplayServer {
	GDSOFTCLASS(DisplayServerWindows9x, DisplayServer);

	struct KeyEvent {
		Ref<InputEventKey> event;
		uint16_t scan;
	};

	JoypadWinMM _joypad;
	Callable _rect_changed;
	Callable _window_event;
	Callable _input_event;
	Callable _drop_files_callback;
	Vector<KeyEvent> _keys;
	ObjectID _instance_id;
	WINDOWPLACEMENT _windowed_placement = {};
	Rect2i _last_client_rect;
	Rect2i _mouse_clip_rect;
	Size2i _min_size;
	Size2i _max_size;
	Point2i _last_mouse_position;
	HWND _window = nullptr;
	HCURSOR _custom_cursors[DisplayServerEnums::CURSOR_MAX] = {};
	FixedFunctionWindows *_renderer = nullptr;
	BitField<MouseButtonMask> _buttons;
	uint32_t _flags = 0;
	DisplayServerEnums::WindowMode _initial_mode = DisplayServerEnums::WINDOW_MODE_WINDOWED;
	DisplayServerEnums::VSyncMode _vsync = DisplayServerEnums::VSYNC_ENABLED;
	DisplayServerEnums::CursorShape _cursor_shape = DisplayServerEnums::CURSOR_ARROW;
	DisplayServerEnums::MouseMode _mouse_mode = DisplayServerEnums::MOUSE_MODE_VISIBLE;
	DisplayServerEnums::MouseMode _mouse_mode_base = DisplayServerEnums::MOUSE_MODE_VISIBLE;
	DisplayServerEnums::MouseMode _mouse_mode_override = DisplayServerEnums::MOUSE_MODE_VISIBLE;
	bool _shown = false;
	bool _fullscreen = false;
	bool _drop_input = false;
	bool _mouse_inside = false;
	bool _mouse_position_valid = false;
	bool _display_changed = false;
	bool _mouse_mode_override_enabled = false;
	bool _mouse_focus_pending = false;
	uint8_t _character_length = 0;
	uint8_t _character_expected = 0;
	char _character_bytes[4] = {};

	static Vector<String> _get_rendering_drivers();
	static DisplayServer *_create(const String &p_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_size, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error);
	static LRESULT CALLBACK _window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam);
	static void _dispatch_input(const Ref<InputEvent> &p_event);
	static DWORD _style(uint32_t p_flags);
	static void _set_modifiers(const Ref<InputEventWithModifiers> &p_event);
	Error _initialize(const String &p_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync, uint32_t p_flags, const Vector2i *p_position, const Size2i &p_size);
	Rect2i _get_client_rect() const;
	Size2i _get_frame_size(const Size2i &p_size) const;
	Size2i _get_size_limit() const;
	void _update_client_rect();
	void _send_window_event(DisplayServerEnums::WindowEvent p_event);
	void _handle_file_drop(WPARAM p_drop);
	void _queue_key(UINT p_message, WPARAM p_key, LPARAM p_flags);
	void _queue_character(uint8_t p_byte, LPARAM p_flags);
	void _flush_keys();
	void _send_mouse_motion(const Point2i &p_position);
	void _send_mouse_button(MouseButton p_button, const Point2i &p_position, bool p_pressed, bool p_double_click = false, float p_factor = 1.0f);
	void _release_mouse_buttons();
	void _update_effective_mouse_mode();
	void _update_mouse_mode(bool p_recenter);
	void _release_mouse_clip();
	void _apply_mouse_cursor();
	static HCURSOR _get_system_cursor(DisplayServerEnums::CursorShape p_shape);
	static void _destroy_custom_cursor(HCURSOR p_cursor);
	bool _recenter_mouse();
	void _pump_messages();
	LRESULT _handle_message(UINT p_message, WPARAM p_wparam, LPARAM p_lparam);

public:
	static void register_driver();
	~DisplayServerWindows9x() override;
	String get_name() const override;
	bool has_feature(DisplayServerEnums::Feature p_feature) const override;
	void clipboard_set(const String &p_text) override;
	String clipboard_get() const override;
	bool clipboard_has() const override;
	Ref<Image> clipboard_get_image() const override { return {}; }
	bool clipboard_has_image() const override { return false; }
	void process_events() override;
	void force_process_and_drop_events() override;
	int get_screen_count() const override;
	int get_primary_screen() const override;
	Point2i screen_get_position(int p_screen) const override;
	Size2i screen_get_size(int p_screen) const override;
	Rect2i screen_get_usable_rect(int p_screen) const override;
	int screen_get_dpi(int p_screen) const override;
	float screen_get_refresh_rate(int p_screen) const override;
	Point2i mouse_get_position() const override;
	void mouse_set_mode(DisplayServerEnums::MouseMode p_mode) override;
	DisplayServerEnums::MouseMode mouse_get_mode() const override;
	void mouse_set_mode_override(DisplayServerEnums::MouseMode p_mode) override;
	DisplayServerEnums::MouseMode mouse_get_mode_override() const override;
	void mouse_set_mode_override_enabled(bool p_enabled) override;
	bool mouse_is_mode_override_enabled() const override;
	BitField<MouseButtonMask> mouse_get_button_state() const override;
	void warp_mouse(const Point2i &p_position) override;
	void cursor_set_shape(DisplayServerEnums::CursorShape p_shape) override;
	DisplayServerEnums::CursorShape cursor_get_shape() const override;
	void cursor_set_custom_image(const Ref<Resource> &p_cursor, DisplayServerEnums::CursorShape p_shape, const Vector2 &p_hotspot) override;
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
	void window_set_title(const String &p_title, DisplayServerEnums::WindowID p_window) override;
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
