#include "os_windows_9x.h"

#include "dir_access_windows_9x.h"
#include "encoding_windows_9x.h"
#include "file_access_windows_9x.h"
#include "ip_windows_9x.h"
#include "path_windows_9x.h"
#ifdef WIN_LEGACY_WINDOWED_ENABLED
#include "platform/win_9x/display/display_server_windows_9x.h"
#include "platform/windows/fixed_function/fixed_function_windows.h"
#endif // WIN_LEGACY_WINDOWED_ENABLED

#include "core/engine.h"
#include "core/os/main_loop.h"
#include "core/project_settings.h"
#include "drivers/dummy/rasterizer_dummy.h"
#include "drivers/unix/net_socket_posix.h"
#include "main/input_default.h"
#include "main/main.h"
#include "scene/main/scene_tree.h"
#include "servers/audio_server.h"
#include "servers/visual/visual_server_raster.h"
#include "servers/visual/visual_server_wrap_mt.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <wincrypt.h>

bool OS_Windows9x::_has_embedded_null(const String &p_text) {
	for (int i = 0; i < p_text.length(); ++i) {
		if (p_text[i] == 0) {
			return true;
		}
	}
	return false;
}

void OS_Windows9x::initialize_core() {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	AudioDriverManager::add_driver(&_audio_driver);
#endif // WIN_LEGACY_WINDOWED_ENABLED
	// Optional DLL probes must return errors instead of opening a loader dialog on 95.
	const UINT error_mode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
	SetErrorMode(error_mode | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
	LARGE_INTEGER frequency;
	LARGE_INTEGER start;
	if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0 && QueryPerformanceCounter(&start)) {
		_clock_frequency = frequency.QuadPart;
		_clock_start = start.QuadPart;
	}
	_clock_msec = GetTickCount();
	FileAccess::make_default<FileAccessWindows9x>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessWindows9x>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessWindows9x>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessWindows9x>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessWindows9x>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessWindows9x>(DirAccess::ACCESS_FILESYSTEM);
	NetSocketPosix::make_default();
	IPWindows9x::make_default();
}

void OS_Windows9x::_initialize_visual_server() {
	_visual_server = memnew(VisualServerRaster);
	if (get_render_thread_mode() != RENDER_THREAD_UNSAFE) {
		_visual_server = memnew(VisualServerWrapMT(_visual_server, false));
	}
	_visual_server->init();
}

Error OS_Windows9x::initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) {
	ERR_FAIL_INDEX_V(p_video_driver, get_video_driver_count(), ERR_INVALID_PARAMETER);
	_video_mode = p_desired;
	_video_driver = p_video_driver;
	_input = memnew(InputDefault);
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	const int windowed_drivers = DisplayServerWindows9x::get_driver_count();
	// Editor imports and exports need real resource RIDs even without a visible
	// window. Keep a hidden renderer there, as the shared NT implementation does.
	if (is_no_window_mode_enabled() && !Engine::get_singleton()->is_editor_hint()) {
		_video_driver = windowed_drivers;
	}
	if (_video_driver < windowed_drivers) {
		if (p_desired.layered || get_render_thread_mode() == RENDER_SEPARATE_THREAD) {
			memdelete(_input);
			_input = nullptr;
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Windows 9x Fixed Function needs an opaque window and main-thread rendering.");
		}
		Error error = ERR_UNAVAILABLE;
		const int requested_driver = _video_driver;
		for (int attempt = -1; attempt < windowed_drivers; ++attempt) {
			const int driver = attempt < 0 ? requested_driver : attempt;
			if (attempt == requested_driver) {
				continue;
			}
			_display = memnew(DisplayServerWindows9x);
			error = _display->initialize(p_desired, driver, *_input);
			if (error == OK) {
				_video_driver = driver;
				_initialize_visual_server();
				error = _display->get_initialization_error();
			}
			if (error == OK) {
				if (driver != requested_driver) {
					WARN_PRINT(vformat("Video driver '%s' is unavailable. Using '%s' for this run.",
							DisplayServerWindows9x::get_driver_name(requested_driver), DisplayServerWindows9x::get_driver_name(driver)));
				}
				break;
			}
			if (_visual_server) {
				_visual_server->finish();
				memdelete(_visual_server);
				_visual_server = nullptr;
			}
			// Retries need a fresh native window because its pixel format is immutable.
			memdelete(_display);
			_display = nullptr;
		}
		if (!_display) {
			memdelete(_input);
			_input = nullptr;
			ERR_FAIL_V_MSG(error, "Could not initialize an available Windows 9x rendering driver.");
		}
	} else
#endif // WIN_LEGACY_WINDOWED_ENABLED
	{
		RasterizerDummy::make_current();
		_initialize_visual_server();
		_dummy_texture_loader.instance();
		ResourceLoader::add_resource_format_loader(_dummy_texture_loader);
		p_audio_driver = AudioDriverManager::get_driver_count() - 1;
	}
	AudioDriverManager::initialize(p_audio_driver);
	return OK;
}

void OS_Windows9x::set_main_loop(MainLoop *p_main_loop) {
	_main_loop = p_main_loop;
	_input->set_main_loop(p_main_loop);
}

void OS_Windows9x::delete_main_loop() {
	if (_main_loop) {
		memdelete(_main_loop);
		_main_loop = nullptr;
	}
}

void OS_Windows9x::finalize() {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		_display->stop_events();
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	delete_main_loop();
	if (_visual_server) {
		_visual_server->finish();
		memdelete(_visual_server);
		_visual_server = nullptr;
	}
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		memdelete(_display);
		_display = nullptr;
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	if (_input) {
		memdelete(_input);
		_input = nullptr;
	}
	if (_dummy_texture_loader.is_valid()) {
		ResourceLoader::remove_resource_format_loader(_dummy_texture_loader);
		_dummy_texture_loader.unref();
	}
}

void OS_Windows9x::finalize_core() {
	for (Map<ProcessID, ChildProcess>::Element *child = _child_processes.front(); child; child = child->next()) {
		if (child->get().handle) {
			CloseHandle(child->get().handle);
		}
	}
	_child_processes.clear();
	NetSocketPosix::cleanup();
}

bool OS_Windows9x::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "pc" || p_feature == "win_9x";
}

MainLoop *OS_Windows9x::get_main_loop() const {
	return _main_loop;
}

void OS_Windows9x::run() {
	ERR_FAIL_NULL(_main_loop);
	_main_loop->init();
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		if (_display->should_render_directly_to_window()) {
			SceneTree *tree = Object::cast_to<SceneTree>(_main_loop);
			if (tree) {
				tree->get_root()->set_use_render_direct_to_screen(true);
			}
		}
		_display->show(*_main_loop);
		while (true) {
			_display->process_events();
			if (Main::iteration()) {
				break;
			}
		}
		_display->stop_events();
	} else
#endif // WIN_LEGACY_WINDOWED_ENABLED
	{
		while (!Main::iteration()) {
		}
	}
	_main_loop->finish();
}

void OS_Windows9x::alert(const String &p_alert, const String &p_title) {
	fprintf(stderr, "%s: %s\n", p_title.utf8().get_data(), p_alert.utf8().get_data());
}

int OS_Windows9x::get_video_driver_count() const {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	return DisplayServerWindows9x::get_driver_count() + 1;
#else
	return 1;
#endif // WIN_LEGACY_WINDOWED_ENABLED
}

const char *OS_Windows9x::get_video_driver_name(int p_driver) const {
	ERR_FAIL_INDEX_V(p_driver, get_video_driver_count(), "");
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (p_driver < DisplayServerWindows9x::get_driver_count()) {
		return DisplayServerWindows9x::get_driver_name(p_driver);
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	return "Dummy";
}

const char *OS_Windows9x::get_video_driver_enum_name(int p_driver) const {
	ERR_FAIL_INDEX_V(p_driver, get_video_driver_count(), nullptr);
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (p_driver < DisplayServerWindows9x::get_driver_count()) {
		return DisplayServerWindows9x::get_driver_enum_name(p_driver);
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	return nullptr;
}

int OS_Windows9x::get_current_video_driver() const {
	return _video_driver;
}

bool OS_Windows9x::can_draw() const {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	return _display && _display->can_any_window_draw();
#else
	return false;
#endif // WIN_LEGACY_WINDOWED_ENABLED
}

Point2 OS_Windows9x::get_mouse_position() const {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		return _display->get_local_mouse_position();
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	return Point2();
}

int OS_Windows9x::get_mouse_button_state() const {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	return _display ? _display->mouse_get_button_state() : 0;
#else
	return 0;
#endif // WIN_LEGACY_WINDOWED_ENABLED
}

void OS_Windows9x::set_window_title(const String &p_title) {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		_display->window_set_title(p_title, 0);
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
}

void OS_Windows9x::set_video_mode(const VideoMode &p_mode, int) {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		set_borderless_window(p_mode.borderless_window);
		set_window_resizable(p_mode.resizable);
		if (!p_mode.fullscreen) {
			set_window_fullscreen(false);
		}
		set_window_size(Size2(p_mode.width, p_mode.height));
		if (p_mode.fullscreen) {
			set_window_fullscreen(true);
		}
		set_window_always_on_top(p_mode.always_on_top);
		_set_use_vsync(p_mode.use_vsync);
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	_video_mode = p_mode;
}

OS::VideoMode OS_Windows9x::get_video_mode(int) const {
	VideoMode mode = _video_mode;
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		const Size2i size = _display->window_get_size(0);
		mode.width = size.x;
		mode.height = size.y;
		mode.fullscreen = is_window_fullscreen();
		mode.non_ex_fs = true;
		mode.maximized = is_window_maximized();
		mode.always_on_top = is_window_always_on_top();
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	return mode;
}

Size2 OS_Windows9x::get_window_size() const {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		return _display->window_get_size(0);
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	return Size2(_video_mode.width, _video_mode.height);
}

String OS_Windows9x::get_name() const {
	return "Windows";
}

String OS_Windows9x::get_locale() const {
	char language[16];
	char country[16];
	if (!GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, language, sizeof(language))) {
		return "en";
	}
	String locale = Windows9xEncoding::decode(language);
	if (GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, sizeof(country))) {
		locale += "_" + Windows9xEncoding::decode(country);
	}
	return locale;
}

String OS_Windows9x::get_executable_path() const {
	char path[MAX_PATH];
	const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
	return length && length < MAX_PATH ? Windows9xEncoding::decode(path, length).replace("\\", "/") : String();
}

Error OS_Windows9x::set_cwd(const String &p_path) {
	const CharString path = PathWindows9x::to_native(p_path);
	return path.length() > 0 && SetCurrentDirectoryA(path.get_data()) ? OK : ERR_CANT_OPEN;
}

String OS_Windows9x::get_config_path() const {
	const String app_data = get_environment("APPDATA");
	if (!app_data.empty()) {
		return app_data.replace("\\", "/");
	}
	char windows[MAX_PATH];
	const UINT length = GetWindowsDirectoryA(windows, MAX_PATH);
	return length && length < MAX_PATH ? Windows9xEncoding::decode(windows, length).replace("\\", "/").plus_file("Application Data") : PathWindows9x::get_absolute(".");
}

String OS_Windows9x::get_data_path() const {
	return get_config_path();
}

String OS_Windows9x::get_cache_path() const {
	return get_config_path();
}

String OS_Windows9x::get_godot_dir_name() const {
	return "Godot";
}

bool OS_Windows9x::has_environment(const String &p_variable) const {
	CharString variable;
	if (Windows9xEncoding::encode(p_variable, variable) != OK || variable.length() == 0) {
		return false;
	}
	SetLastError(NO_ERROR);
	return GetEnvironmentVariableA(variable.get_data(), nullptr, 0) != 0 || GetLastError() == NO_ERROR;
}

String OS_Windows9x::get_environment(const String &p_variable) const {
	CharString variable;
	if (Windows9xEncoding::encode(p_variable, variable) != OK || variable.length() == 0) {
		return {};
	}
	const DWORD capacity = GetEnvironmentVariableA(variable.get_data(), nullptr, 0);
	if (!capacity) {
		return {};
	}
	CharString value;
	if (value.resize(capacity) != OK) {
		return {};
	}
	const DWORD length = GetEnvironmentVariableA(variable.get_data(), value.ptrw(), capacity);
	return length < capacity ? Windows9xEncoding::decode(value.get_data(), length) : String();
}

void OS_Windows9x::delay_usec(uint32_t p_usec) const {
	Sleep(uint32_t((uint64_t(p_usec) + 999) / 1000));
}

uint64_t OS_Windows9x::get_ticks_usec() const {
	const MutexLock lock(_clock_mutex);
	const uint32_t now_msec = GetTickCount();
	LARGE_INTEGER now;
	if (_clock_frequency && QueryPerformanceCounter(&now) && now.QuadPart >= _clock_start) {
		const uint64_t ticks = now.QuadPart - _clock_start;
		const uint64_t usec = ticks / _clock_frequency * 1000000 + (ticks % _clock_frequency) * 1000000 / _clock_frequency;
		_ticks_usec = MAX(_ticks_usec, usec);
	} else {
		// Unsigned subtraction handles the 32-bit millisecond counter wrapping.
		_ticks_usec += uint64_t(now_msec - _clock_msec) * 1000;
	}
	_clock_msec = now_msec;
	return _ticks_usec;
}

Error OS_Windows9x::get_entropy(uint8_t *r_buffer, int p_bytes) {
	ERR_FAIL_COND_V(p_bytes < 0 || (!r_buffer && p_bytes), ERR_INVALID_PARAMETER);
	HCRYPTPROV provider;
	// The 9x RSA provider rejects CRYPT_SILENT. Verification-only contexts do not access private keys.
	if (!CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
		return ERR_UNAVAILABLE;
	}
	const bool generated = CryptGenRandom(provider, p_bytes, r_buffer);
	CryptReleaseContext(provider, 0);
	return generated ? OK : FAILED;
}

int OS_Windows9x::get_process_id() const {
	return GetCurrentProcessId();
}

String OS_Windows9x::get_stdin_string() {
	const MutexLock lock(_stdin_mutex);
	const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
	if (input != _stdin_handle) {
		_stdin_handle = input;
		_stdin_size = 0;
		_stdin_offset = 0;
	}
	if (!input || input == INVALID_HANDLE_VALUE) {
		return String();
	}

	Vector<char> line;
	bool ended_with_newline = false;
	for (;;) {
		if (_stdin_offset == _stdin_size) {
			_stdin_offset = 0;
			_stdin_size = 0;
			if (!ReadFile(input, _stdin_buffer, sizeof(_stdin_buffer), &_stdin_size, nullptr) && GetLastError() != ERROR_MORE_DATA) {
				_stdin_size = 0;
			}
			if (_stdin_size == 0) {
				break;
			}
		}

		// Retain any bytes after the newline for the next call.
		const char *start = _stdin_buffer + _stdin_offset;
		const DWORD available = _stdin_size - _stdin_offset;
		const char *newline = static_cast<const char *>(memchr(start, '\n', available));
		const int count = newline ? int(newline - start) : int(available);
		if (count > 0) {
			const int size = line.size();
			ERR_FAIL_COND_V(count > INT_MAX - size || line.resize(size + count) != OK, String());
			memcpy(line.ptrw() + size, start, count);
		}
		_stdin_offset += count;
		if (newline) {
			++_stdin_offset;
			ended_with_newline = true;
			break;
		}
	}

	int length = line.size();
	if (ended_with_newline && length > 0 && line[length - 1] == '\r') {
		--length;
	}
	return length > 0 ? String::utf8(line.ptr(), length) : String();
}

String OS_Windows9x::get_user_data_dir() const {
	const String app_name = get_safe_dir_name(GLOBAL_GET("application/config/name"));
	if (app_name.empty()) {
		return get_data_path().plus_file(get_godot_dir_name()).plus_file("app_userdata").plus_file("[unnamed project]");
	}
	if (GLOBAL_GET("application/config/use_custom_user_dir")) {
		const String custom = get_safe_dir_name(GLOBAL_GET("application/config/custom_user_dir_name"), true);
		return get_data_path().plus_file(custom.empty() ? app_name : custom);
	}
	return get_data_path().plus_file(get_godot_dir_name()).plus_file("app_userdata").plus_file(app_name);
}

OS::Date OS_Windows9x::get_date(bool p_local) const {
	SYSTEMTIME time;
	TIME_ZONE_INFORMATION zone;
	if (p_local) {
		GetLocalTime(&time);
	} else {
		GetSystemTime(&time);
	}
	return { time.wYear, Month(time.wMonth), time.wDay, Weekday(time.wDayOfWeek), p_local && GetTimeZoneInformation(&zone) == TIME_ZONE_ID_DAYLIGHT };
}

OS::Time OS_Windows9x::get_time(bool p_local) const {
	SYSTEMTIME time;
	if (p_local) {
		GetLocalTime(&time);
	} else {
		GetSystemTime(&time);
	}
	return { time.wHour, time.wMinute, time.wSecond };
}

OS::TimeZoneInfo OS_Windows9x::get_time_zone_info() const {
	TIME_ZONE_INFORMATION zone;
	const DWORD state = GetTimeZoneInformation(&zone);
	if (state == TIME_ZONE_ID_INVALID) {
		return {};
	}
	const bool daylight = state == TIME_ZONE_ID_DAYLIGHT;
	return { int(-(zone.Bias + (daylight ? zone.DaylightBias : zone.StandardBias))), String(daylight ? zone.DaylightName : zone.StandardName) };
}

uint64_t OS_Windows9x::get_unix_time() const {
	return uint64_t(get_subsecond_unix_time());
}

double OS_Windows9x::get_subsecond_unix_time() const {
	// Convert UTC calendar time to FILETIME for the Unix-epoch calculation.
	SYSTEMTIME system_time;
	FILETIME time;
	GetSystemTime(&system_time);
	if (!SystemTimeToFileTime(&system_time, &time)) {
		return 0;
	}
	const uint64_t ticks = (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
	return (double(ticks) - 116444736000000000.0) / 10000000.0;
}

bool OS_Windows9x::set_environment(const String &p_variable, const String &p_value) const {
	CharString variable;
	CharString value;
	ERR_FAIL_COND_V(Windows9xEncoding::encode(p_variable, variable) != OK || variable.length() == 0 || Windows9xEncoding::encode(p_value, value) != OK, false);
	return SetEnvironmentVariableA(variable.get_data(), value.get_data()) != FALSE;
}
