#include "os_windows_9x.h"

#include "dir_access_windows_9x.h"
#include "encoding_windows_9x.h"
#include "file_access_windows_9x.h"
#include "ip_windows_9x.h"
#include "path_windows_9x.h"

#ifdef WIN_LEGACY_WINDOWED_ENABLED
#include "display/display_server_windows_9x.h"
#endif // WIN_LEGACY_WINDOWED_ENABLED

#include "core/os/main_loop.h"
#include "core/profiling/profiling.h"
#include "drivers/windows/net_socket_winsock.h"
#include "main/main.h"
#include "servers/display/display_server.h"

#include <wincrypt.h>

void OS_Windows9x::initialize() {
#ifdef WIN_LEGACY_WINDOWED_ENABLED
	DisplayServerWindows9x::register_driver();
	AudioDriverManager::add_driver(&_audio_driver);
#endif // WIN_LEGACY_WINDOWED_ENABLED
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
	NetSocketWinSock::make_default();
	IPWindows9x::make_default();
}

void OS_Windows9x::set_main_loop(MainLoop *p_main_loop) {
	_main_loop = p_main_loop;
}

void OS_Windows9x::delete_main_loop() {
	if (_main_loop) {
		memdelete(_main_loop);
		_main_loop = nullptr;
	}
}

void OS_Windows9x::finalize() {
	delete_main_loop();
}

void OS_Windows9x::finalize_core() {
	for (const KeyValue<ProcessID, ChildProcess> &entry : _child_processes) {
		if (entry.value.handle) {
			CloseHandle(entry.value.handle);
		}
	}
	_child_processes.clear();
	NetSocketWinSock::cleanup();
}

bool OS_Windows9x::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "pc" || p_feature == "win_9x";
}

MainLoop *OS_Windows9x::get_main_loop() const {
	return _main_loop;
}

void OS_Windows9x::run() {
	ERR_FAIL_NULL(_main_loop);
	_main_loop->initialize();
	do {
		GodotProfileFrameMark;
		DisplayServer::get_singleton()->process_events();
	} while (!Main::iteration());
	_main_loop->finalize();
}

String OS_Windows9x::get_name() const {
	return "Windows";
}

String OS_Windows9x::get_distribution_name() const {
	return get_name();
}

String OS_Windows9x::get_version() const {
	// On NT hosts, bypass the version emulation applied to unmanifested applications.
	typedef LONG(WINAPI * NtVersionFunction)(OSVERSIONINFOW *);
	const HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	const auto nt_version = ntdll ? reinterpret_cast<NtVersionFunction>(reinterpret_cast<void *>(GetProcAddress(ntdll, "RtlGetVersion"))) : nullptr;
	OSVERSIONINFOW nt_info = {};
	nt_info.dwOSVersionInfoSize = sizeof(nt_info);
	if (nt_version && nt_version(&nt_info) == 0) {
		return vformat("%d.%d.%d", nt_info.dwMajorVersion, nt_info.dwMinorVersion, nt_info.dwBuildNumber);
	}
	// GetVersionExA is the native 9x API, deprecated in modern SDK declarations.
	typedef BOOL(WINAPI * LegacyVersionFunction)(OSVERSIONINFOA *);
	const auto legacy_version = reinterpret_cast<LegacyVersionFunction>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetVersionExA")));
	OSVERSIONINFOA version = {};
	version.dwOSVersionInfoSize = sizeof(version);
	if (!legacy_version || !legacy_version(&version)) {
		return {};
	}
	// Win9x stores the major/minor version in the high word of dwBuildNumber.
	return vformat("%d.%d.%d", version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber & 0xFFFF);
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
	return length && length < MAX_PATH ? Windows9xEncoding::decode(path, length).replace_char('\\', '/') : String();
}

String OS_Windows9x::get_cwd() const {
	char path[MAX_PATH];
	const DWORD length = GetCurrentDirectoryA(MAX_PATH, path);
	return length && length < MAX_PATH ? Windows9xEncoding::decode(path, length).replace_char('\\', '/') : String();
}

Error OS_Windows9x::set_cwd(const String &p_path) {
	const CharString path = PathWindows9x::to_native(p_path);
	return !path.is_empty() && SetCurrentDirectoryA(path.get_data()) ? OK : ERR_CANT_OPEN;
}

String OS_Windows9x::get_config_path() const {
	const String app_data = get_environment("APPDATA");
	if (!app_data.is_empty()) {
		return app_data.replace_char('\\', '/');
	}
	char windows[MAX_PATH];
	const UINT length = GetWindowsDirectoryA(windows, MAX_PATH);
	return length && length < MAX_PATH ? Windows9xEncoding::decode(windows, length).replace_char('\\', '/').path_join("Application Data") : get_cwd();
}

String OS_Windows9x::get_data_path() const {
	return get_config_path();
}

String OS_Windows9x::get_cache_path() const {
	return get_config_path();
}

String OS_Windows9x::get_temp_path() const {
	char path[MAX_PATH];
	const DWORD length = GetTempPathA(MAX_PATH, path);
	return length && length < MAX_PATH ? Windows9xEncoding::decode(path, length).replace_char('\\', '/') : get_cwd();
}

String OS_Windows9x::get_godot_dir_name() const {
	return "Godot";
}

String OS_Windows9x::get_user_data_dir(const String &p_user_dir) const {
	return get_data_path().path_join(p_user_dir);
}

bool OS_Windows9x::has_environment(const String &p_variable) const {
	CharString variable;
	if (Windows9xEncoding::encode(p_variable, variable) != OK || variable.is_empty()) {
		return false;
	}
	SetLastError(NO_ERROR);
	return GetEnvironmentVariableA(variable.get_data(), nullptr, 0) != 0 || GetLastError() == NO_ERROR;
}

String OS_Windows9x::get_environment(const String &p_variable) const {
	CharString variable;
	if (Windows9xEncoding::encode(p_variable, variable) != OK || variable.is_empty()) {
		return {};
	}
	const DWORD capacity = GetEnvironmentVariableA(variable.get_data(), nullptr, 0);
	if (!capacity) {
		return {};
	}
	CharString value;
	if (value.resize_uninitialized(capacity) != OK) {
		return {};
	}
	const DWORD length = GetEnvironmentVariableA(variable.get_data(), value.ptrw(), capacity);
	return length < capacity ? Windows9xEncoding::decode(value.get_data(), length) : String();
}

void OS_Windows9x::set_environment(const String &p_variable, const String &p_value) const {
	CharString variable;
	CharString value;
	ERR_FAIL_COND(Windows9xEncoding::encode(p_variable, variable) != OK || variable.is_empty() || Windows9xEncoding::encode(p_value, value) != OK);
	SetEnvironmentVariableA(variable.get_data(), value.get_data());
}

void OS_Windows9x::unset_environment(const String &p_variable) const {
	CharString variable;
	ERR_FAIL_COND(Windows9xEncoding::encode(p_variable, variable) != OK || variable.is_empty());
	SetEnvironmentVariableA(variable.get_data(), nullptr);
}

OS::DateTime OS_Windows9x::get_datetime(bool p_utc) const {
	SYSTEMTIME time;
	if (p_utc) {
		GetSystemTime(&time);
	} else {
		GetLocalTime(&time);
	}
	TIME_ZONE_INFORMATION zone;
	return { time.wYear, Month(time.wMonth), uint8_t(time.wDay), Weekday(time.wDayOfWeek), uint8_t(time.wHour), uint8_t(time.wMinute), uint8_t(time.wSecond), !p_utc && GetTimeZoneInformation(&zone) == TIME_ZONE_ID_DAYLIGHT };
}

OS::TimeZoneInfo OS_Windows9x::get_time_zone_info() const {
	TIME_ZONE_INFORMATION zone;
	const DWORD state = GetTimeZoneInformation(&zone);
	if (state == TIME_ZONE_ID_INVALID) {
		return {};
	}
	const bool is_daylight = state == TIME_ZONE_ID_DAYLIGHT;
	const wchar_t *name = is_daylight ? zone.DaylightName : zone.StandardName;
	return { -(zone.Bias + (is_daylight ? zone.DaylightBias : zone.StandardBias)), String::utf16(reinterpret_cast<const char16_t *>(name)) };
}

double OS_Windows9x::get_unix_time() const {
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

PackedByteArray OS_Windows9x::get_stdin_buffer(int64_t p_buffer_size) {
	ERR_FAIL_COND_V(p_buffer_size < 0 || p_buffer_size > INT32_MAX, PackedByteArray());
	PackedByteArray data;
	const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
	if (!input || input == INVALID_HANDLE_VALUE || p_buffer_size == 0 || data.resize(p_buffer_size) != OK) {
		return {};
	}
	DWORD count = 0;
	if (!ReadFile(input, data.ptrw(), p_buffer_size, &count, nullptr)) {
		return {};
	}
	data.resize(count);
	return data;
}

String OS_Windows9x::get_stdin_string(int64_t p_buffer_size) {
	const PackedByteArray data = get_stdin_buffer(p_buffer_size);
	return data.is_empty() ? String() : Windows9xEncoding::decode(reinterpret_cast<const char *>(data.ptr()), data.size()).replace("\r\n", "\n").rstrip("\n");
}

OS::StdHandleType OS_Windows9x::_get_std_handle_type(DWORD p_stream) {
	const HANDLE stream = GetStdHandle(p_stream);
	if (!stream || stream == INVALID_HANDLE_VALUE) {
		return STD_HANDLE_INVALID;
	}
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
	}
	return STD_HANDLE_UNKNOWN;
}

OS::StdHandleType OS_Windows9x::get_stdin_type() const {
	return _get_std_handle_type(STD_INPUT_HANDLE);
}

OS::StdHandleType OS_Windows9x::get_stdout_type() const {
	return _get_std_handle_type(STD_OUTPUT_HANDLE);
}

OS::StdHandleType OS_Windows9x::get_stderr_type() const {
	return _get_std_handle_type(STD_ERROR_HANDLE);
}
