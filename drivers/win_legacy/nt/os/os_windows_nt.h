#pragma once

#ifdef WIN_LEGACY_WINDOWED_ENABLED
#include "drivers/win_legacy/common/audio/audio_driver_waveout.h"
#include "drivers/winmidi/midi_driver_winmidi.h"
#endif // WIN_LEGACY_WINDOWED_ENABLED

#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/templates/hash_map.h"

#include <windows.h>

class OS_WindowsNT : public OS {
	class SystemFontCatalog;

	struct ChildProcess {
		HANDLE handle;
		int exit_code = -1;
	};

#ifdef WIN_LEGACY_WINDOWED_ENABLED
	AudioDriverWaveOut _audio_driver;
	MIDIDriverWinMidi _midi_driver;
#endif // WIN_LEGACY_WINDOWED_ENABLED
	MainLoop *_main_loop = nullptr;
	uint64_t _clock_frequency = 0;
	int64_t _clock_start = 0;
	mutable Mutex _clock_mutex;
	mutable uint64_t _ticks_usec = 0;
	mutable uint32_t _clock_msec = 0;
	Mutex _process_launch_mutex;
	mutable Mutex _process_mutex;
	mutable HashMap<ProcessID, ChildProcess> _child_processes;
	HashMap<String, int> _encodings;
	mutable Mutex _system_font_mutex;
	mutable SystemFontCatalog *_system_font_catalog = nullptr;

	static void _append_process_argument(String &r_command, const String &p_argument);
	static void _append_process_output(String &r_output, Mutex *p_mutex, const char *p_bytes, int p_size);
	static Error _launch_process(const String &p_path, const List<String> &p_arguments, bool p_open_console, STARTUPINFOW &p_startup, PROCESS_INFORMATION &r_process);
	static bool _poll_child_process(ChildProcess &r_child);
	void _track_child_process(const PROCESS_INFORMATION &p_process);
	void _close_child_processes();

	static Error _shell_error(DWORD p_error);
	static bool _get_shell_folder_path(int p_folder, wchar_t *r_path);
	static Error _shell_execute(const Char16String &p_target, const Char16String &p_arguments = {});
	static StdHandleType _get_std_handle_type(DWORD p_stream);
	static String _read_hardware_registry_string(HKEY p_key, const wchar_t *p_value);
	const SystemFontCatalog &_get_system_font_catalog() const;
	void _free_system_font_catalog();

	void initialize_joypads() override {}
	void set_main_loop(MainLoop *p_main_loop) override;
	void delete_main_loop() override;
	void finalize() override;
	void finalize_core() override;

protected:
	void initialize() override;
	bool _check_internal_feature_support(const String &p_feature) override;

public:
	void run();
	MainLoop *get_main_loop() const override;

	String get_name() const override;
	void alert(const String &p_alert, const String &p_title = "ALERT!") override;
	String get_distribution_name() const override;
	String get_version() const override;
	String get_locale() const override;
	Vector<String> get_preferred_locales() const override;
	String get_processor_name() const override;
	String get_model_name() const override;
	String get_unique_id() const override;
	Dictionary get_memory_info() const override;
	String get_executable_path() const override;
	String get_cwd() const override;
	Error set_cwd(const String &p_path) override;
	String get_config_path() const override;
	String get_data_path() const override;
	String get_cache_path() const override;
	String get_temp_path() const override;
	String get_godot_dir_name() const override;
	String get_user_data_dir(const String &p_user_dir) const override;
	String expand_path(const String &p_path) const override;

	bool has_environment(const String &p_variable) const override;
	String get_environment(const String &p_variable) const override;
	void set_environment(const String &p_variable, const String &p_value) const override;
	void unset_environment(const String &p_variable) const override;
	String multibyte_to_string(const String &p_encoding, const PackedByteArray &p_array) const override;
	PackedByteArray string_to_multibyte(const String &p_encoding, const String &p_string) const override;

	Error open_dynamic_library(const String &p_path, void *&r_library_handle, GDExtensionData *p_data = nullptr) override;
	Error close_dynamic_library(void *p_library_handle) override;
	Error get_dynamic_library_symbol_handle(void *p_library_handle, const String &p_name, void *&r_symbol_handle, bool p_optional = false) override;

	DateTime get_datetime(bool p_utc) const override;
	TimeZoneInfo get_time_zone_info() const override;
	double get_unix_time() const override;
	void delay_usec(uint32_t p_usec) const override;
	uint64_t get_ticks_usec() const override;
	Error get_entropy(uint8_t *r_buffer, int p_bytes) override;
	String get_system_ca_certificates() override;
	// Legacy Windows root stores can be stale. Use bundled roots for default TLS,
	// while keeping get_system_ca_certificates() available for explicit trust selection.
	String get_system_ca_certificates_for_tls() override { return String(); }
	int get_process_id() const override;
	String get_stdin_string(int64_t p_buffer_size) override;
	PackedByteArray get_stdin_buffer(int64_t p_buffer_size) override;
	StdHandleType get_stdin_type() const override;
	StdHandleType get_stdout_type() const override;
	StdHandleType get_stderr_type() const override;

	Vector<String> get_video_adapter_driver_info() const override { return {}; }
	Vector<String> get_system_fonts() const override;
	String get_system_font_path(const String &p_font_name, int p_weight, int p_stretch, bool p_italic) const override;
	Vector<String> get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale, const String &p_script, int p_weight, int p_stretch, bool p_italic) const override;
	Error execute(const String &p_path, const List<String> &p_arguments, String *r_output, int *r_exit_code, bool p_read_stderr, Mutex *p_pipe_mutex, bool p_open_console) override;
	Dictionary execute_with_pipe(const String &p_path, const List<String> &p_arguments, bool p_blocking) override;
	Error create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) override;
	Error kill(const ProcessID &p_pid) override;
	bool is_process_running(const ProcessID &p_pid) const override;
	int get_process_exit_code(const ProcessID &p_pid) const override;
	Error shell_open(const String &p_uri) override;
	String get_system_dir(SystemDir p_dir, bool p_shared_storage) const override;
	Error shell_show_in_file_manager(String p_path, bool p_open_folder) override;
	Error move_to_trash(const String &p_path) override;
};
