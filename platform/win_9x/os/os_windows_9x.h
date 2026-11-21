#pragma once

#ifdef WIN_LEGACY_WINDOWED_ENABLED
#include "drivers/win_legacy/common/audio/audio_driver_waveout.h"
#endif // WIN_LEGACY_WINDOWED_ENABLED

#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/templates/hash_map.h"

#include <windows.h>

class OS_Windows9x : public OS {
	struct ChildProcess {
		HANDLE handle;
		int exit_code = -1;
	};

#ifdef WIN_LEGACY_WINDOWED_ENABLED
	AudioDriverWaveOut _audio_driver;
#endif // WIN_LEGACY_WINDOWED_ENABLED

	uint64_t _clock_frequency = 0;
	int64_t _clock_start = 0;
	mutable uint64_t _ticks_usec = 0;
	MainLoop *_main_loop = nullptr;
	mutable Mutex _clock_mutex;
	Mutex _process_launch_mutex;
	mutable Mutex _process_mutex;
	mutable HashMap<ProcessID, ChildProcess> _child_processes;
	mutable uint32_t _clock_msec = 0;

	static void _append_process_argument(String &r_command, const String &p_argument);
	static Error _launch_process(const String &p_path, const List<String> &p_arguments, bool p_open_console, STARTUPINFOA &p_startup, PROCESS_INFORMATION &r_process);
	static bool _poll_child_process(ChildProcess &r_child);
	static DWORD _get_native_process_id(const ProcessID &p_pid);
	static StdHandleType _get_std_handle_type(DWORD p_stream);
	static String _get_shell_folder_path(int p_folder);
	static Error _shell_error(DWORD p_error);
	static Error _shell_execute(const CharString &p_target, const CharString &p_arguments = {});
	void _track_child_process(const PROCESS_INFORMATION &p_process);

	void initialize() override;
	void initialize_joypads() override {}
	void set_main_loop(MainLoop *p_main_loop) override;
	void delete_main_loop() override;
	void finalize() override;
	void finalize_core() override;
	bool _check_internal_feature_support(const String &p_feature) override;

public:
	void run();
	MainLoop *get_main_loop() const override;
	String get_name() const override;
	String get_distribution_name() const override;
	String get_version() const override;
	String get_locale() const override;
	String get_executable_path() const override;
	String get_cwd() const override;
	Error set_cwd(const String &p_path) override;
	String get_config_path() const override;
	String get_data_path() const override;
	String get_cache_path() const override;
	String get_temp_path() const override;
	String get_godot_dir_name() const override;
	String get_user_data_dir(const String &p_user_dir) const override;
	String get_system_dir(SystemDir p_dir, bool p_shared_storage = true) const override;
	String expand_path(const String &p_path) const override;
	bool has_environment(const String &p_variable) const override;
	String get_environment(const String &p_variable) const override;
	void set_environment(const String &p_variable, const String &p_value) const override;
	void unset_environment(const String &p_variable) const override;
	DateTime get_datetime(bool p_utc) const override;
	TimeZoneInfo get_time_zone_info() const override;
	double get_unix_time() const override;
	void delay_usec(uint32_t p_usec) const override;
	uint64_t get_ticks_usec() const override;
	Error get_entropy(uint8_t *r_buffer, int p_bytes) override;
	Error open_dynamic_library(const String &p_path, void *&r_library_handle, GDExtensionData *p_data = nullptr) override;
	Error close_dynamic_library(void *p_library_handle) override;
	Error get_dynamic_library_symbol_handle(void *p_library_handle, const String &p_name, void *&r_symbol_handle, bool p_optional = false) override;
	int get_process_id() const override;
	String get_stdin_string(int64_t p_buffer_size) override;
	PackedByteArray get_stdin_buffer(int64_t p_buffer_size) override;
	StdHandleType get_stdin_type() const override;
	StdHandleType get_stdout_type() const override;
	StdHandleType get_stderr_type() const override;
	Vector<String> get_video_adapter_driver_info() const override { return {}; }
	Error execute(const String &p_path, const List<String> &p_arguments, String *r_output, int *r_exit_code, bool p_read_stderr, Mutex *p_pipe_mutex, bool p_open_console) override;
	Dictionary execute_with_pipe(const String &p_path, const List<String> &p_arguments, bool p_blocking) override;
	Error create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) override;
	Error kill(const ProcessID &p_pid) override;
	bool is_process_running(const ProcessID &p_pid) const override;
	int get_process_exit_code(const ProcessID &p_pid) const override;
	Error shell_open(const String &p_uri) override;
	Error shell_show_in_file_manager(String p_path, bool p_open_folder) override;
};
