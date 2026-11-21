#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined(__MINGW32__) && !defined(_WIN64)

#include <cerrno>
#include <cstdint>

#ifdef TOOLS_ENABLED
#include <shlwapi.h>
#endif // TOOLS_ENABLED

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

class Windows9xOptionalRuntime {
public:
	typedef BOOL(WINAPI *DebuggerPresent)();

	static BOOL WINAPI is_debugger_present() {
		// Doctest imports this API, which Windows 95 lacks. Preserve native debugger
		// detection on later Windows without making it a loader requirement.
		const DWORD previous_error = GetLastError();
		const HMODULE kernel = GetModuleHandleA("kernel32.dll");
		const auto original = kernel ? reinterpret_cast<DebuggerPresent>(reinterpret_cast<void *>(GetProcAddress(kernel, "IsDebuggerPresent"))) : nullptr;
		SetLastError(previous_error);
		return original ? original() : FALSE;
	}

#ifdef TOOLS_ENABLED
	typedef HRESULT(WINAPI *QueryAssociation)(ASSOCF, ASSOCSTR, LPCSTR, LPCSTR, LPSTR, DWORD *);

	static HRESULT WINAPI query_association(ASSOCF p_flags, ASSOCSTR p_string, LPCSTR p_association, LPCSTR p_extra, LPSTR p_output, DWORD *p_output_size) {
		// Older Shells omit this optional Blender-autodetection API. Its caller
		// already handles a failed query and retains the ordinary path candidates.
		static const HMODULE shell = LoadLibraryA("shlwapi.dll");
		static const auto original = shell ? reinterpret_cast<QueryAssociation>(reinterpret_cast<void *>(GetProcAddress(shell, "AssocQueryStringA"))) : nullptr;
		return original ? original(p_flags, p_string, p_association, p_extra, p_output, p_output_size) : E_NOTIMPL;
	}
#endif // TOOLS_ENABLED
};

// Resolve only this executable's static imports. Each wrapper
// forwards to the original DLL export when it is available.
extern "C" {
Windows9xThreadRuntime::BeginThreadEx windows9x_beginthreadex_import asm("__imp___beginthreadex") = Windows9xThreadRuntime::begin_thread;
Windows9xThreadRuntime::SetProcessAffinity windows9x_set_process_affinity_import asm("__imp__SetProcessAffinityMask@8") = Windows9xThreadRuntime::set_process_affinity;
Windows9xOptionalRuntime::DebuggerPresent windows9x_is_debugger_present_import asm("__imp__IsDebuggerPresent@0") = Windows9xOptionalRuntime::is_debugger_present;
#ifdef TOOLS_ENABLED
Windows9xOptionalRuntime::QueryAssociation windows9x_query_association_import asm("__imp__AssocQueryStringA@24") = Windows9xOptionalRuntime::query_association;
#endif // TOOLS_ENABLED
}

#endif // defined(__MINGW32__) && !defined(_WIN64)
