#include <windows.h>

#ifdef _MSC_VER

// Recent static MSVC runtimes import the Windows 8 precise clock for std::chrono.
// Windows 7 provides the same FILETIME representation through the ordinary system clock.
// The assembly import slot references this uniquely named C ABI entry point.
extern "C" void WINAPI windows_nt6_get_system_time(LPFILETIME p_time) {
	typedef void(WINAPI * SystemTimeFunction)(LPFILETIME);
	static const auto precise_time = reinterpret_cast<SystemTimeFunction>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemTimePreciseAsFileTime")));
	if (precise_time) {
		precise_time(p_time);
	} else {
		GetSystemTimeAsFileTime(p_time);
	}
}

#endif // _MSC_VER
