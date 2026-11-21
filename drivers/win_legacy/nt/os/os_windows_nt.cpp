#include "os_windows_nt.h"

#include "core/os/main_loop.h"
#include "core/profiling/profiling.h"
#include "drivers/win_legacy/nt/dir_access_windows_nt.h"
#include "drivers/win_legacy/nt/file_access_windows_nt.h"
#include "drivers/win_legacy/nt/ip_windows_nt.h"
#include "drivers/windows/code_page_windows.h"
#include "drivers/windows/net_socket_winsock.h"
#include "main/main.h"
#include "servers/display/display_server.h"

#include "modules/modules_enabled.gen.h"

#include <windows.h>

#include <wincrypt.h>

void OS_WindowsNT::initialize() {
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
	WindowsCodePages::populate(_encodings);

	FileAccess::make_default<FileAccessWindowsNT>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessWindowsNT>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessWindowsNT>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessWindowsNT>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessWindowsNT>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessWindowsNT>(DirAccess::ACCESS_FILESYSTEM);
	NetSocketWinSock::make_default();
	IPWindowsNT::make_default();
}

void OS_WindowsNT::set_main_loop(MainLoop *p_main_loop) {
	_main_loop = p_main_loop;
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
#endif // WIN_LEGACY_WINDOWED_ENABLED
	delete_main_loop();
}

void OS_WindowsNT::finalize_core() {
	_free_system_font_catalog();
	_close_child_processes();
	NetSocketWinSock::cleanup();
}

bool OS_WindowsNT::_check_internal_feature_support(const String &p_feature) {
#ifdef MODULE_FREETYPE_ENABLED
	if (p_feature == "system_fonts") {
		return true;
	}
#endif // MODULE_FREETYPE_ENABLED
	return p_feature == "pc";
}

MainLoop *OS_WindowsNT::get_main_loop() const {
	return _main_loop;
}

void OS_WindowsNT::run() {
	ERR_FAIL_NULL(_main_loop);
	_main_loop->initialize();
	do {
		GodotProfileFrameMark;
		DisplayServer::get_singleton()->process_events();
	} while (!Main::iteration());
	_main_loop->finalize();
}

String OS_WindowsNT::get_name() const {
	return "Windows";
}

void OS_WindowsNT::alert(const String &p_alert, const String &p_title) {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	DisplayServer *display = DisplayServer::get_singleton();
	if (display) {
		const HWND owner = reinterpret_cast<HWND>(intptr_t(display->window_get_native_handle(DisplayServerEnums::WINDOW_HANDLE, DisplayServerEnums::MAIN_WINDOW_ID)));
		if (owner && MessageBoxW(owner, reinterpret_cast<LPCWSTR>(p_alert.utf16().get_data()), reinterpret_cast<LPCWSTR>(p_title.utf16().get_data()), MB_OK | MB_ICONEXCLAMATION)) {
			return;
		}
	}
#endif // WIN_LEGACY_WINDOWED_ENABLED
	OS::alert(p_alert, p_title);
}

String OS_WindowsNT::get_distribution_name() const {
	return get_name();
}

String OS_WindowsNT::get_version() const {
	// RtlGetVersion reports the host version even without a modern Windows manifest.
	using GetVersionFunction = LONG(WINAPI *)(OSVERSIONINFOW *);
	auto get_version = reinterpret_cast<GetVersionFunction>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")));
	OSVERSIONINFOW version = {};
	version.dwOSVersionInfoSize = sizeof(version);
	const bool found = get_version && get_version(&version) == 0;
	if (!found) {
		return String();
	}
	return vformat("%d.%d.%d", version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber);
}

String OS_WindowsNT::get_executable_path() const {
	wchar_t path[MAX_PATH];
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	return length && length < MAX_PATH ? String::utf16(reinterpret_cast<const char16_t *>(path), length).replace_char('\\', '/') : String();
}

String OS_WindowsNT::get_cwd() const {
	wchar_t path[MAX_PATH];
	const DWORD length = GetCurrentDirectoryW(MAX_PATH, path);
	return length && length < MAX_PATH ? String::utf16(reinterpret_cast<const char16_t *>(path), length).replace_char('\\', '/') : String();
}

Error OS_WindowsNT::set_cwd(const String &p_path) {
	return SetCurrentDirectoryW((LPCWSTR)p_path.utf16().get_data()) ? OK : ERR_CANT_OPEN;
}

String OS_WindowsNT::get_config_path() const {
	const String app_data = get_environment("APPDATA");
	return app_data.is_empty() ? get_cwd() : app_data.replace_char('\\', '/');
}

String OS_WindowsNT::get_data_path() const {
	return get_config_path();
}

String OS_WindowsNT::get_cache_path() const {
	const String local_app_data = get_environment("LOCALAPPDATA");
	return local_app_data.is_empty() ? get_config_path() : local_app_data.replace_char('\\', '/');
}

String OS_WindowsNT::get_temp_path() const {
	wchar_t path[MAX_PATH + 1];
	const DWORD length = GetTempPathW(MAX_PATH + 1, path);
	return length && length <= MAX_PATH ? String::utf16(reinterpret_cast<const char16_t *>(path), length).replace_char('\\', '/') : get_cwd();
}

String OS_WindowsNT::get_godot_dir_name() const {
	return "Godot";
}

String OS_WindowsNT::get_user_data_dir(const String &p_user_dir) const {
	return get_data_path().path_join(p_user_dir);
}

bool OS_WindowsNT::has_environment(const String &p_variable) const {
	SetLastError(ERROR_SUCCESS);
	return GetEnvironmentVariableW((LPCWSTR)p_variable.utf16().get_data(), nullptr, 0) != 0 || GetLastError() == ERROR_SUCCESS;
}

String OS_WindowsNT::get_environment(const String &p_variable) const {
	const Char16String variable = p_variable.utf16();
	const DWORD capacity = GetEnvironmentVariableW((LPCWSTR)variable.get_data(), nullptr, 0);
	if (!capacity) {
		return String();
	}
	Char16String value;
	value.resize_uninitialized(capacity);
	const DWORD length = GetEnvironmentVariableW((LPCWSTR)variable.get_data(), (LPWSTR)value.ptrw(), capacity);
	return length < capacity ? String::utf16(value.get_data(), length) : String();
}

void OS_WindowsNT::set_environment(const String &p_variable, const String &p_value) const {
	SetEnvironmentVariableW((LPCWSTR)p_variable.utf16().get_data(), (LPCWSTR)p_value.utf16().get_data());
}

void OS_WindowsNT::unset_environment(const String &p_variable) const {
	SetEnvironmentVariableW((LPCWSTR)p_variable.utf16().get_data(), nullptr);
}

OS::DateTime OS_WindowsNT::get_datetime(bool p_utc) const {
	SYSTEMTIME time;
	if (p_utc) {
		GetSystemTime(&time);
	} else {
		GetLocalTime(&time);
	}
	TIME_ZONE_INFORMATION zone;
	return { time.wYear, Month(time.wMonth), uint8_t(time.wDay), Weekday(time.wDayOfWeek), uint8_t(time.wHour), uint8_t(time.wMinute), uint8_t(time.wSecond), !p_utc && GetTimeZoneInformation(&zone) == TIME_ZONE_ID_DAYLIGHT };
}

OS::TimeZoneInfo OS_WindowsNT::get_time_zone_info() const {
	TIME_ZONE_INFORMATION zone;
	const DWORD state = GetTimeZoneInformation(&zone);
	if (state == TIME_ZONE_ID_INVALID) {
		return {};
	}
	const bool is_daylight = state == TIME_ZONE_ID_DAYLIGHT;
	const wchar_t *name = is_daylight ? zone.DaylightName : zone.StandardName;
	return { -(zone.Bias + (is_daylight ? zone.DaylightBias : zone.StandardBias)), String::utf16(reinterpret_cast<const char16_t *>(name)) };
}

double OS_WindowsNT::get_unix_time() const {
	FILETIME time;
	GetSystemTimeAsFileTime(&time);
	const uint64_t ticks = (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
	// The number of 100-nanosecond intervals between Windows epoch (1601) to Unix epoch (1970),
	// divided by 10000000 to get seconds. This will break in the year 30,828.
	return (double(ticks) - 116444736000000000.0) / 10000000.0;
}

void OS_WindowsNT::delay_usec(uint32_t p_usec) const {
	Sleep(uint32_t((uint64_t(p_usec) + 999) / 1000));
}

uint64_t OS_WindowsNT::get_ticks_usec() const {
	MutexLock lock(_clock_mutex);
	const uint32_t now_msec = GetTickCount();
	LARGE_INTEGER now;
	if (_clock_frequency && QueryPerformanceCounter(&now) && now.QuadPart >= _clock_start) {
		const uint64_t ticks = now.QuadPart - _clock_start;
		const uint64_t usec = ticks / _clock_frequency * 1000000 + (ticks % _clock_frequency) * 1000000 / _clock_frequency;
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

String OS_WindowsNT::get_stdin_string(int64_t p_buffer_size) {
	const PackedByteArray data = get_stdin_buffer(p_buffer_size);
	if (data.is_empty()) {
		return String();
	}
	return String::utf8(reinterpret_cast<const char *>(data.ptr()), data.size()).replace("\r\n", "\n").rstrip("\n");
}

PackedByteArray OS_WindowsNT::get_stdin_buffer(int64_t p_buffer_size) {
	PackedByteArray data;
	ERR_FAIL_COND_V(p_buffer_size < 0 || p_buffer_size > INT32_MAX, data);
	HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
	if (!input || input == INVALID_HANDLE_VALUE || p_buffer_size == 0 || data.resize(p_buffer_size) != OK) {
		return {};
	}
	DWORD size = 0;
	if (!ReadFile(input, data.ptrw(), p_buffer_size, &size, nullptr)) {
		return {};
	}
	data.resize(size);
	return data;
}

OS::StdHandleType OS_WindowsNT::_get_std_handle_type(DWORD p_stream) {
	const HANDLE stream = GetStdHandle(p_stream);
	if (!stream || stream == INVALID_HANDLE_VALUE) {
		return STD_HANDLE_INVALID;
	}

	SetLastError(ERROR_SUCCESS);
	switch (GetFileType(stream) & ~DWORD(FILE_TYPE_REMOTE)) {
		case FILE_TYPE_DISK:
			return STD_HANDLE_FILE;
		case FILE_TYPE_PIPE:
			return STD_HANDLE_PIPE;
		case FILE_TYPE_CHAR: {
			DWORD mode;
			if (GetConsoleMode(stream, &mode)) {
				return STD_HANDLE_CONSOLE;
			}
		} break;
		case FILE_TYPE_UNKNOWN:
			if (GetLastError() == ERROR_INVALID_HANDLE) {
				return STD_HANDLE_INVALID;
			}
			break;
	}
	return STD_HANDLE_UNKNOWN;
}

OS::StdHandleType OS_WindowsNT::get_stdin_type() const {
	return _get_std_handle_type(STD_INPUT_HANDLE);
}

OS::StdHandleType OS_WindowsNT::get_stdout_type() const {
	return _get_std_handle_type(STD_OUTPUT_HANDLE);
}

OS::StdHandleType OS_WindowsNT::get_stderr_type() const {
	return _get_std_handle_type(STD_ERROR_HANDLE);
}
