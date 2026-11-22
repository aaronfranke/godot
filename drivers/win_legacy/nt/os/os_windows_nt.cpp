#include "drivers/win_legacy/nt/os/os_windows_nt.h"

// Legacy Windows includes.
#include "drivers/win_legacy/nt/dir_access_windows_nt.h"
#include "drivers/win_legacy/nt/file_access_windows_nt.h"
#include "drivers/win_legacy/nt/ip_windows_nt.h"
#include "drivers/win_legacy/nt/path_windows_nt.h"

#ifdef WIN_LEGACY_WINDOWED_ENABLED
#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"
#endif // WIN_LEGACY_WINDOWED_ENABLED

// Godot includes.
#include "core/engine.h"
#include "core/os/main_loop.h"
#include "core/os/thread.h"
#include "core/project_settings.h"
#include "drivers/dummy/rasterizer_dummy.h"
#include "drivers/unix/net_socket_posix.h"
#include "main/input_default.h"
#include "main/main.h"
#include "servers/audio_server.h"
#include "servers/visual/visual_server_raster.h"
#include "servers/visual/visual_server_wrap_mt.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <wincrypt.h>

void OS_WindowsNT::initialize_core() {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	AudioDriverManager::add_driver(&_audio_driver);
#endif // WIN_LEGACY_WINDOWED_ENABLED

	LARGE_INTEGER frequency;
	LARGE_INTEGER start;
	if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0 && QueryPerformanceCounter(&start)) {
		_clock_frequency = frequency.QuadPart;
		_clock_start = start.QuadPart;
	}
	_clock_msec = GetTickCount();

	FileAccess::make_default<FileAccessWindowsNT>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessWindowsNT>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessWindowsNT>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessWindowsNT>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessWindowsNT>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessWindowsNT>(DirAccess::ACCESS_FILESYSTEM);
	NetSocketPosix::make_default();
	IPWindowsNT::make_default();
}

void OS_WindowsNT::_initialize_visual_server() {
	_visual_server = memnew(VisualServerRaster);
	if (get_render_thread_mode() != RENDER_THREAD_UNSAFE) {
		_visual_server = memnew(VisualServerWrapMT(_visual_server, get_render_thread_mode() == RENDER_SEPARATE_THREAD));
	}
	_visual_server->init();
}

Error OS_WindowsNT::initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) {
	ERR_FAIL_INDEX_V(p_video_driver, get_video_driver_count(), ERR_INVALID_PARAMETER);
	_video_mode = p_desired;
	_video_driver = p_video_driver;
	_input = memnew(InputDefault);

#ifdef WIN_LEGACY_WINDOWED_ENABLED
	const int windowed_drivers = DisplayServerWindowsNT::get_driver_count();
	// Godot 3's editor needs real resource RIDs even during command-line exports.
	// Match its desktop platforms by retaining a hidden rendering window there.
	if (is_no_window_mode_enabled() && !Engine::get_singleton()->is_editor_hint()) {
		_video_driver = windowed_drivers;
	}
	if (_video_driver < windowed_drivers) {
		if (p_desired.layered) {
			memdelete(_input);
			_input = nullptr;
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Legacy Windows requires an opaque window.");
		}
		const int requested_driver = _video_driver;
		Vector<int> candidates;
		candidates.push_back(requested_driver);
		for (int driver = 0; driver < windowed_drivers; ++driver) {
			if (driver == requested_driver) {
				continue;
			}
			if (DisplayServerWindowsNT::is_gl_driver(driver)) {
				// Keep 3.x's GLES3-to-GLES2 fallback policy.
				const bool allow_gles2 = bool(GLOBAL_GET("rendering/quality/driver/fallback_to_gles2")) || Engine::get_singleton()->is_editor_hint();
				if (requested_driver != 0 || driver != 1 || !allow_gles2) {
					continue;
				}
			}
			candidates.push_back(driver);
		}
		Error error = ERR_UNAVAILABLE;
		for (int i = 0; i < candidates.size(); ++i) {
			const int driver = candidates[i];
			_display = _create_display();
			error = _display->initialize(p_desired, driver, *_input);
			if (error == OK) {
				_video_driver = driver;
				_initialize_visual_server();
			}
			if (error == OK) {
				if (driver != requested_driver) {
					WARN_PRINT(vformat("Video driver '%s' is unavailable. Using '%s' for this run.", DisplayServerWindowsNT::get_driver_name(requested_driver), DisplayServerWindowsNT::get_driver_name(driver)));
				}
				break;
			}
			if (_visual_server) {
				_visual_server->finish();
				memdelete(_visual_server);
				_visual_server = nullptr;
			}
			// Pixel formats are immutable. Retry with a fresh native window/context.
			memdelete(_display);
			_display = nullptr;
		}
		if (!_display) {
			memdelete(_input);
			_input = nullptr;
			ERR_FAIL_V_MSG(error, "Could not initialize an available Windows NT rendering driver.");
		}
		if (!is_no_window_mode_enabled() && !_display->set_keep_screen_on(is_keep_screen_on())) {
			OS::set_keep_screen_on(false);
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

void OS_WindowsNT::set_main_loop(MainLoop *p_main_loop) {
	_main_loop = p_main_loop;
	_input->set_main_loop(p_main_loop);
}

void OS_WindowsNT::delete_main_loop() {
	if (_main_loop) {
		memdelete(_main_loop);
		_main_loop = nullptr;
	}
}

void OS_WindowsNT::finalize() {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	_midi_driver.close();
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

void OS_WindowsNT::finalize_core() {
	_close_child_processes();
	NetSocketPosix::cleanup();
}

bool OS_WindowsNT::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "pc";
}

#ifdef WIN_LEGACY_WINDOWED_ENABLED
DisplayServerWindowsNT *OS_WindowsNT::_create_display() {
	return memnew(DisplayServerWindowsNT);
}
#endif // WIN_LEGACY_WINDOWED_ENABLED

MainLoop *OS_WindowsNT::get_main_loop() const {
	return _main_loop;
}

void OS_WindowsNT::run() {
	ERR_FAIL_NULL(_main_loop);
	_main_loop->init();
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		_display->show(*_main_loop);
		while (!_display->is_quit_requested()) {
			_display->process_events();
			if (_display->is_quit_requested() || Main::iteration()) {
				break;
			}
		}
		_display->stop_events();
		_main_loop->finish();
		return;
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	while (true) {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
		// A window-capable template can open MIDI inputs when started with --no-window.
		_midi_driver.process();
#endif // WIN_LEGACY_WINDOWED_ENABLED
		if (Main::iteration()) {
			break;
		}
	}
	_main_loop->finish();
}

String OS_WindowsNT::get_name() const {
	return "Windows";
}

void OS_WindowsNT::alert(const String &p_alert, const String &p_title) {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	// Showing a modal box from a worker can block while it disables the owner.
	if (Thread::get_caller_id() == Thread::get_main_id() && _display) {
		const HWND owner = _display->get_window();
		if (owner && MessageBoxW(owner, p_alert.c_str(), p_title.c_str(), MB_OK | MB_ICONEXCLAMATION)) {
			return;
		}
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	fprintf(stderr, "%s: %s\n", p_title.utf8().get_data(), p_alert.utf8().get_data());
}

String OS_WindowsNT::get_stdin_string() {
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

const char *OS_WindowsNT::get_video_driver_name(int p_driver) const {
	ERR_FAIL_INDEX_V(p_driver, get_video_driver_count(), "");
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (p_driver < DisplayServerWindowsNT::get_driver_count()) {
		return DisplayServerWindowsNT::get_driver_name(p_driver);
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	return "Dummy";
}

int OS_WindowsNT::get_video_driver_count() const {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	return DisplayServerWindowsNT::get_driver_count() + 1;
#else
	return 1;
#endif // WIN_LEGACY_WINDOWED_ENABLED
}

int OS_WindowsNT::get_current_video_driver() const {
	return _video_driver;
}

bool OS_WindowsNT::can_draw() const {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	return _display && _display->can_draw();
#else
	return false;
#endif // WIN_LEGACY_WINDOWED_ENABLED
}

Point2 OS_WindowsNT::get_mouse_position() const {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		return _display->get_mouse_position();
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	return Point2();
}

int OS_WindowsNT::get_mouse_button_state() const {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	return _display ? _display->get_mouse_button_state() : 0;
#else
	return 0;
#endif // WIN_LEGACY_WINDOWED_ENABLED
}

void OS_WindowsNT::set_window_title(const String &p_title) {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		_display->set_title(p_title);
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
}

void OS_WindowsNT::set_video_mode(const VideoMode &p_video_mode, int) {
	VideoMode mode = p_video_mode;
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		if (!_display->set_windowed_frame(p_video_mode.borderless_window, p_video_mode.resizable)) {
			mode.borderless_window = _video_mode.borderless_window;
			mode.resizable = _video_mode.resizable;
		}
		if (!p_video_mode.fullscreen) {
			_display->set_fullscreen(false);
		}
		_display->set_size(Size2i(p_video_mode.width, p_video_mode.height));
		if (p_video_mode.fullscreen) {
			_display->set_fullscreen(true);
		}
		_display->set_always_on_top(p_video_mode.always_on_top);
		_display->set_use_vsync(p_video_mode.use_vsync);
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	_video_mode = mode;
}

OS::VideoMode OS_WindowsNT::get_video_mode(int) const {
	VideoMode mode = _video_mode;
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		const Size2i size = _display->get_size();
		mode.width = size.x;
		mode.height = size.y;
		mode.fullscreen = _display->is_fullscreen();
		mode.non_ex_fs = true;
		mode.maximized = _display->is_maximized();
		mode.always_on_top = _display->is_always_on_top();
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	return mode;
}

Size2 OS_WindowsNT::get_window_size() const {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	if (_display) {
		return _display->get_size();
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	return Size2(_video_mode.width, _video_mode.height);
}

String OS_WindowsNT::get_executable_path() const {
	wchar_t path[MAX_PATH];
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	return length && length < MAX_PATH ? String(path, length).replace("\\", "/") : String();
}

Error OS_WindowsNT::set_cwd(const String &p_cwd) {
	return SetCurrentDirectoryW(p_cwd.c_str()) ? OK : ERR_CANT_OPEN;
}

String OS_WindowsNT::get_config_path() const {
	const String app_data = get_environment("APPDATA");
	return app_data.empty() ? PathWindowsNT::get_absolute(".") : app_data.replace("\\", "/");
}

String OS_WindowsNT::get_data_path() const {
	return get_config_path();
}

String OS_WindowsNT::get_cache_path() const {
	const String local_app_data = get_environment("LOCALAPPDATA");
	return local_app_data.empty() ? get_config_path() : local_app_data.replace("\\", "/");
}

String OS_WindowsNT::get_godot_dir_name() const {
	return "Godot";
}

String OS_WindowsNT::get_user_data_dir() const {
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

String OS_WindowsNT::get_locale() const {
	constexpr DWORD MUI_LANGUAGE_NAME_FLAG = 0x8;
	ULONG count = 0;
	ULONG length = 0;
	if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME_FLAG, &count, nullptr, &length) && length > 0 && length <= INT_MAX) {
		String names;
		if (names.resize(length) == OK && GetUserPreferredUILanguages(MUI_LANGUAGE_NAME_FLAG, &count, names.ptrw(), &length) && count > 0) {
			const String primary = String(names.c_str());
			if (!primary.empty()) {
				return primary.replace("-", "_");
			}
		}
	}

	const LANGID language_id = GetUserDefaultUILanguage();
	const LCID locale_id = MAKELCID(language_id, SORT_DEFAULT);
	wchar_t language[9];
	if (GetLocaleInfoW(locale_id, LOCALE_SISO639LANGNAME, language, ARRAYSIZE(language)) <= 1) {
		return OS::get_locale();
	}
	String locale = String(language);
	// These legacy locale fields omit the script for these language variants.
	switch (language_id) {
		case MAKELANGID(LANG_AZERI, SUBLANG_AZERI_LATIN):
		case MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_LATIN):
		case MAKELANGID(LANG_UZBEK, SUBLANG_UZBEK_LATIN):
			locale += "_Latn";
			break;
		case MAKELANGID(LANG_AZERI, SUBLANG_AZERI_CYRILLIC):
		case MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_CYRILLIC):
		case MAKELANGID(LANG_UZBEK, SUBLANG_UZBEK_CYRILLIC):
			locale += "_Cyrl";
			break;
	}
	wchar_t country[9];
	if (GetLocaleInfoW(locale_id, LOCALE_SISO3166CTRYNAME, country, ARRAYSIZE(country)) > 1) {
		locale += "_" + String(country);
	}
	return locale;
}

bool OS_WindowsNT::has_environment(const String &p_variable) const {
	SetLastError(ERROR_SUCCESS);
	return GetEnvironmentVariableW(p_variable.c_str(), nullptr, 0) != 0 || GetLastError() == ERROR_SUCCESS;
}

String OS_WindowsNT::get_environment(const String &p_variable) const {
	const DWORD capacity = GetEnvironmentVariableW(p_variable.c_str(), nullptr, 0);
	String value;
	if (!capacity || value.resize(capacity) != OK) {
		return String();
	}
	const DWORD length = GetEnvironmentVariableW(p_variable.c_str(), value.ptrw(), capacity);
	return length < capacity ? String(value.c_str(), length) : String();
}

bool OS_WindowsNT::set_environment(const String &p_variable, const String &p_value) const {
	return SetEnvironmentVariableW(p_variable.c_str(), p_value.c_str());
}

OS::Date OS_WindowsNT::get_date(bool p_local) const {
	SYSTEMTIME time;
	TIME_ZONE_INFORMATION zone;
	if (p_local) {
		GetLocalTime(&time);
	} else {
		GetSystemTime(&time);
	}
	return { time.wYear, Month(time.wMonth), time.wDay, Weekday(time.wDayOfWeek), p_local && GetTimeZoneInformation(&zone) == TIME_ZONE_ID_DAYLIGHT };
}

OS::Time OS_WindowsNT::get_time(bool p_local) const {
	SYSTEMTIME time;
	if (p_local) {
		GetLocalTime(&time);
	} else {
		GetSystemTime(&time);
	}
	return { time.wHour, time.wMinute, time.wSecond };
}

OS::TimeZoneInfo OS_WindowsNT::get_time_zone_info() const {
	TIME_ZONE_INFORMATION zone;
	const DWORD state = GetTimeZoneInformation(&zone);
	if (state == TIME_ZONE_ID_INVALID) {
		return {};
	}
	const bool daylight = state == TIME_ZONE_ID_DAYLIGHT;
	return { int(-(zone.Bias + (daylight ? zone.DaylightBias : zone.StandardBias))), String(daylight ? zone.DaylightName : zone.StandardName) };
}

uint64_t OS_WindowsNT::get_unix_time() const {
	return uint64_t(get_subsecond_unix_time());
}

double OS_WindowsNT::get_subsecond_unix_time() const {
	FILETIME time;
	GetSystemTimeAsFileTime(&time);
	const uint64_t ticks = (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
	return (double(ticks) - 116444736000000000.0) / 10000000.0;
}

void OS_WindowsNT::delay_usec(uint32_t p_usec) const {
	Sleep(DWORD((uint64_t(p_usec) + 999) / 1000));
}

uint64_t OS_WindowsNT::get_ticks_usec() const {
	const MutexLock lock(_clock_mutex);
	const uint32_t now_msec = GetTickCount();
	LARGE_INTEGER now;
	if (_clock_frequency && QueryPerformanceCounter(&now) && now.QuadPart >= _clock_start) {
		const uint64_t ticks = now.QuadPart - _clock_start;
		const uint64_t usec = ticks / _clock_frequency * 1000000 + ticks % _clock_frequency * 1000000 / _clock_frequency;
		_ticks_usec = MAX(_ticks_usec, usec);
	} else {
		// Unsigned subtraction handles the 32-bit GetTickCount wrap.
		_ticks_usec += uint64_t(now_msec - _clock_msec) * 1000;
	}
	_clock_msec = now_msec;
	return _ticks_usec;
}

Error OS_WindowsNT::get_entropy(uint8_t *r_buffer, int p_bytes) {
	ERR_FAIL_COND_V(p_bytes < 0, ERR_INVALID_PARAMETER);
	HCRYPTPROV provider;
	const DWORD flags = CRYPT_VERIFYCONTEXT | CRYPT_SILENT;
	if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_FULL, flags)) {
		return ERR_UNAVAILABLE;
	}
	const bool generated = CryptGenRandom(provider, p_bytes, r_buffer);
	CryptReleaseContext(provider, 0);
	return generated ? OK : FAILED;
}

int OS_WindowsNT::get_process_id() const {
	return GetCurrentProcessId();
}
