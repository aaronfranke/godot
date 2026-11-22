#include <windows.h>

#if defined(__MINGW32__) && !defined(_WIN64)

#include <cerrno>
#include <cstdint>

class Windows9xThreadRuntime {
public:
	typedef unsigned(__stdcall *ThreadStart)(void *);
	typedef uintptr_t(__cdecl *BeginThreadEx)(void *, unsigned, ThreadStart, void *, unsigned, unsigned *);
	typedef BOOL(WINAPI *SetProcessAffinity)(HANDLE, DWORD_PTR);

	static uintptr_t __cdecl begin_thread(void *p_security, unsigned p_stack_size, ThreadStart p_start,
			void *p_argument, unsigned p_flags, unsigned *p_thread_id) {
		const HMODULE crt = GetModuleHandleA("msvcrt.dll");
		const auto original = crt ? reinterpret_cast<BeginThreadEx>(reinterpret_cast<void *>(GetProcAddress(crt, "_beginthreadex"))) : nullptr;
		if (!original) {
			errno = ENOSYS;
			SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
			return 0;
		}
		// Win9x requires this output pointer. The static winpthreads runtime can pass null.
		unsigned thread_id = 0;
		return original(p_security, p_stack_size, p_start, p_argument, p_flags, p_thread_id ? p_thread_id : &thread_id);
	}

	static BOOL WINAPI set_process_affinity(HANDLE p_process, DWORD_PTR p_mask) {
		const HMODULE kernel = GetModuleHandleA("kernel32.dll");
		const auto original = reinterpret_cast<SetProcessAffinity>(reinterpret_cast<void *>(GetProcAddress(kernel, "SetProcessAffinityMask")));
		if (original) {
			return original(p_process, p_mask);
		}
		// Winpthreads references this from its thread unit even when affinity is
		// unused. Windows 95 has no affinity API; report that without blocking startup.
		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		return FALSE;
	}
};

// Resolve only this executable's static compiler-runtime imports. Each wrapper
// forwards to the original DLL export when it is available.
extern "C" {
Windows9xThreadRuntime::BeginThreadEx windows9x_beginthreadex_import asm("__imp___beginthreadex") = Windows9xThreadRuntime::begin_thread;
Windows9xThreadRuntime::SetProcessAffinity windows9x_set_process_affinity_import asm("__imp__SetProcessAffinityMask@8") = Windows9xThreadRuntime::set_process_affinity;
}

#endif // defined(__MINGW32__) && !defined(_WIN64)
