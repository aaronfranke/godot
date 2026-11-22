#pragma once

#include "core/map.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "drivers/dummy/texture_loader_dummy.h"
#ifdef WIN_LEGACY_WINDOWED_ENABLED
#include "drivers/win_legacy/common/audio/audio_driver_waveout.h"
#endif // WIN_LEGACY_WINDOWED_ENABLED

#include <windows.h>

class InputDefault;
class VisualServer;
class DisplayServerWindows9x;

class OS_Windows9x : public OS {
	struct ChildProcess {
		HANDLE handle = nullptr;
		int exit_code = -1;
	};

	MainLoop *_main_loop = nullptr;
	InputDefault *_input = nullptr;
	VisualServer *_visual_server = nullptr;
	Ref<ResourceFormatDummyTexture> _dummy_texture_loader;
	VideoMode _video_mode;
	int _video_driver = 0;
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	DisplayServerWindows9x *_display = nullptr;
	AudioDriverWaveOut _audio_driver;
#endif // WIN_LEGACY_WINDOWED_ENABLED
	uint64_t _clock_frequency = 0;
	int64_t _clock_start = 0;
	mutable Mutex _clock_mutex;
	mutable uint64_t _ticks_usec = 0;
	mutable uint32_t _clock_msec = 0;
	Mutex _stdin_mutex;
	HANDLE _stdin_handle = nullptr;
	char _stdin_buffer[4096];
	DWORD _stdin_size = 0;
	DWORD _stdin_offset = 0;
	Mutex _process_launch_mutex;
	mutable Mutex _process_mutex;
	mutable Map<ProcessID, ChildProcess> _child_processes;

	static bool _has_embedded_null(const String &p_text);
	static void _append_process_argument(String &r_command, const String &p_argument);
	static Error _launch_process(const String &p_path, const List<String> &p_arguments, bool p_open_console, STARTUPINFOA &p_startup, PROCESS_INFORMATION &r_process);
	static bool _poll_child_process(ChildProcess &r_child);
	static DWORD _get_native_process_id(const ProcessID &p_pid);
	static String _get_shell_folder_path(int p_folder);
	static Error _shell_error(DWORD p_error);
	static Error _shell_execute(const CharString &p_target, const CharString &p_arguments = CharString());
	void _track_child_process(const PROCESS_INFORMATION &p_process);
	void _initialize_visual_server();

protected:
	void initialize_core() override;
	Error initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) override;
	void set_main_loop(MainLoop *p_main_loop) override;
	void delete_main_loop() override;
	void finalize() override;
	void finalize_core() override;
	bool _check_internal_feature_support(const String &p_feature) override;

public:
	void run();
	MainLoop *get_main_loop() const override;
	String get_name() const override;
	void alert(const String &p_alert, const String &p_title = "ALERT!") override;
	String get_stdin_string() override;
	int get_video_driver_count() const override;
	const char *get_video_driver_name(int p_driver) const override;
	const char *get_video_driver_enum_name(int p_driver) const override;
	int get_current_video_driver() const override;
	bool can_draw() const override;
	Point2 get_mouse_position() const override;
	int get_mouse_button_state() const override;
	void set_window_title(const String &p_title) override;
	void set_video_mode(const VideoMode &p_mode, int p_screen = 0) override;
	VideoMode get_video_mode(int p_screen = 0) const override;
	void get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen = 0) const override {}
	Size2 get_window_size() const override;
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	void set_clipboard(const String &p_text) override;
	String get_clipboard() const override;
	Size2 get_real_window_size() const override;
	void set_window_size(const Size2 p_size) override;
	Size2 get_min_window_size() const override;
	void set_min_window_size(const Size2 p_size) override;
	Size2 get_max_window_size() const override;
	void set_max_window_size(const Size2 p_size) override;
	Point2 get_window_position() const override;
	void set_window_position(const Point2 &p_position) override;
	void set_window_minimized(bool p_enabled) override;
	bool is_window_minimized() const override;
	void set_window_maximized(bool p_enabled) override;
	bool is_window_maximized() const override;
	bool is_window_focused() const override;
	void request_attention() override;
	void move_window_to_foreground() override;
	void set_borderless_window(bool p_enabled) override;
	bool get_borderless_window() override;
	void set_window_resizable(bool p_enabled) override;
	bool is_window_resizable() const override;
	void set_mouse_mode(MouseMode p_mode) override;
	MouseMode get_mouse_mode() const override;
	void set_cursor_shape(CursorShape p_shape) override;
	CursorShape get_cursor_shape() const override;
	void set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) override;
	void warp_mouse_position(const Point2 &p_position) override;
	void set_window_fullscreen(bool p_enabled) override;
	bool is_window_fullscreen() const override;
	bool is_window_use_nonexclusive_fullscreen() const override;
	void set_window_always_on_top(bool p_enabled) override;
	bool is_window_always_on_top() const override;
	int get_screen_count() const override;
	int get_current_screen() const override;
	void set_current_screen(int p_screen) override;
	Point2 get_screen_position(int p_screen = -1) const override;
	Size2 get_screen_size(int p_screen = -1) const override;
	int get_screen_dpi(int p_screen = -1) const override;
	float get_screen_refresh_rate(int p_screen = -1) const override;
	void *get_native_handle(int p_handle_type) override;
	void force_process_input() override;
	void process_and_drop_events() override;
	void _set_use_vsync(bool p_enable) override;
#endif // WIN_LEGACY_WINDOWED_ENABLED
	String get_executable_path() const override;
	Error set_cwd(const String &p_path) override;
	String get_config_path() const override;
	String get_data_path() const override;
	String get_cache_path() const override;
	String get_godot_dir_name() const override;
	String get_user_data_dir() const override;
	String get_locale() const override;
	bool has_environment(const String &p_variable) const override;
	String get_environment(const String &p_variable) const override;
	bool set_environment(const String &p_variable, const String &p_value) const override;
	Date get_date(bool p_local) const override;
	Time get_time(bool p_local) const override;
	TimeZoneInfo get_time_zone_info() const override;
	uint64_t get_unix_time() const override;
	double get_subsecond_unix_time() const override;
	void delay_usec(uint32_t p_usec) const override;
	uint64_t get_ticks_usec() const override;
	Error get_entropy(uint8_t *r_buffer, int p_bytes) override;
	int get_process_id() const override;
	Error open_dynamic_library(const String p_path, void *&r_library_handle, bool p_also_set_library_path = false) override;
	Error close_dynamic_library(void *p_library_handle) override;
	Error get_dynamic_library_symbol_handle(void *p_library_handle, const String p_name, void *&r_symbol_handle, bool p_optional = false) override;
	Error execute(const String &p_path, const List<String> &p_arguments, bool p_blocking, ProcessID *r_child_id, String *r_pipe, int *r_exitcode, bool p_read_stderr, Mutex *p_pipe_mutex, bool p_open_console) override;
	Error kill(const ProcessID &p_pid) override;
	bool is_process_running(const ProcessID &p_pid) const override;
	Error shell_open(String p_uri) override;
	String expand_path(const String &p_path) const override;
	String get_system_dir(SystemDir p_dir, bool p_shared_storage = true) const override;
};
