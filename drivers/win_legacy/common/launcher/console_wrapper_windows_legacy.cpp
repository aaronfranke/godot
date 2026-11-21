// Shared by the legacy platforms: use ANSI on 9x and Unicode on NT.
#ifndef WIN_9X_ENABLED
#define UNICODE
#define _UNICODE
#endif // WIN_9X_ENABLED

#include <windows.h>

#include <tchar.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

class WindowsLegacyConsoleWrapper {
	static HANDLE _process;
	static HANDLE _input;
	static HANDLE _input_writer;

	static bool _open_pipe(HANDLE &r_parent, HANDLE &r_child, bool p_child_reads);
	static bool _forward_output(HANDLE p_reader, HANDLE &r_destination);
	static DWORD WINAPI _forward_input(void *);
	static BOOL WINAPI _console_control(DWORD p_event);

public:
	static DWORD run();
};

HANDLE WindowsLegacyConsoleWrapper::_process = nullptr;
HANDLE WindowsLegacyConsoleWrapper::_input = nullptr;
HANDLE WindowsLegacyConsoleWrapper::_input_writer = nullptr;

bool WindowsLegacyConsoleWrapper::_open_pipe(HANDLE &r_parent, HANDLE &r_child, bool p_child_reads) {
	HANDLE reader = nullptr;
	HANDLE writer = nullptr;
	if (!CreatePipe(&reader, &writer, nullptr, 0)) {
		return false;
	}
	// 9x ignores CreatePipe's security attributes and lacks SetHandleInformation.
	// Duplicate just the child's end so it cannot inherit the parent's pipe end.
	HANDLE child_end = p_child_reads ? reader : writer;
	const BOOL duplicated = DuplicateHandle(GetCurrentProcess(), child_end, GetCurrentProcess(), &r_child, 0, TRUE, DUPLICATE_SAME_ACCESS);
	CloseHandle(child_end);
	r_parent = p_child_reads ? writer : reader;
	return duplicated;
}

bool WindowsLegacyConsoleWrapper::_forward_output(HANDLE p_reader, HANDLE &r_destination) {
	// Bound each drain so a busy stdout cannot starve stderr or the exit check.
	bool forwarded = false;
	for (int i = 0; i < 16; ++i) {
		DWORD available = 0;
		if (!PeekNamedPipe(p_reader, nullptr, 0, nullptr, &available, nullptr) || !available) {
			break;
		}
		char buffer[4096];
		DWORD count = 0;
		const DWORD size = available < sizeof(buffer) ? available : sizeof(buffer);
		if (!ReadFile(p_reader, buffer, size, &count, nullptr) || !count) {
			break;
		}
		forwarded = true;
		DWORD offset = 0;
		while (r_destination && offset < count) {
			DWORD written = 0;
			if (!WriteFile(r_destination, buffer + offset, count - offset, &written, nullptr) || !written) {
				// Keep draining if the caller closed its output, so the game cannot deadlock.
				r_destination = nullptr;
				break;
			}
			offset += written;
		}
	}
	return forwarded;
}

DWORD WINAPI WindowsLegacyConsoleWrapper::_forward_input(void *) {
	char buffer[4096];
	DWORD count = 0;
	while (ReadFile(_input, buffer, sizeof(buffer), &count, nullptr) && count) {
		DWORD offset = 0;
		while (offset < count) {
			DWORD written = 0;
			if (!WriteFile(_input_writer, buffer + offset, count - offset, &written, nullptr) || !written) {
				CloseHandle(_input_writer);
				return 0;
			}
			offset += written;
		}
	}
	CloseHandle(_input_writer);
	return 0;
}

BOOL WINAPI WindowsLegacyConsoleWrapper::_console_control(DWORD p_event) {
	switch (p_event) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			// The GUI child has no console and cannot receive console control events.
			if (_process) {
				TerminateProcess(_process, 0xC000013A); // STATUS_CONTROL_C_EXIT.
			}
			return TRUE;
	}
	return FALSE;
}

DWORD WindowsLegacyConsoleWrapper::run() {
	enum { COMMAND_CAPACITY = 32768 };
	TCHAR executable[COMMAND_CAPACITY];
	const DWORD length = GetModuleFileName(nullptr, executable, COMMAND_CAPACITY);
	if (!length || length >= COMMAND_CAPACITY) {
		fprintf(stderr, "Could not find the console wrapper's executable path.\n");
		return EXIT_FAILURE;
	}
	// Match the suffixes accepted by EditorExportPlatformPC, at the end of the name.
	const TCHAR *const suffixes[] = { _T(".console.exe"), _T("_console.exe"), _T(" console.exe"), _T("console.exe") };
	bool renamed = false;
	for (unsigned int i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
		const size_t suffix_length = _tcslen(suffixes[i]);
		if (length >= suffix_length && _tcsicmp(executable + length - suffix_length, suffixes[i]) == 0) {
			memcpy(executable + length - suffix_length, _T(".exe"), 5 * sizeof(TCHAR));
			renamed = true;
			break;
		}
	}
	if (!renamed) {
		fprintf(stderr, "Invalid console wrapper name; use <game>.console.exe.\n");
		return EXIT_FAILURE;
	}

	// Replace only argv[0]. Preserve the raw argument tail, including empty arguments
	// and escaped quotes; CharNext also respects multibyte ANSI characters on 9x.
	const TCHAR *arguments = GetCommandLine();
	bool quoted = false;
	while (*arguments) {
		if (*arguments == _T('"')) {
			quoted = !quoted;
		} else if (!quoted && (*arguments == _T(' ') || *arguments == _T('\t'))) {
			break;
		}
		arguments = CharNext(arguments);
	}
	const size_t executable_length = _tcslen(executable);
	const size_t arguments_length = _tcslen(arguments);
	if (executable_length + arguments_length + 3 > COMMAND_CAPACITY) {
		fprintf(stderr, "Console wrapper command line is too long.\n");
		return EXIT_FAILURE;
	}
	TCHAR command[COMMAND_CAPACITY];
	command[0] = _T('"');
	memcpy(command + 1, executable, executable_length * sizeof(TCHAR));
	command[executable_length + 1] = _T('"');
	memcpy(command + executable_length + 2, arguments, (arguments_length + 1) * sizeof(TCHAR));

	HANDLE output_reader = nullptr;
	HANDLE error_reader = nullptr;
	STARTUPINFO startup = {};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	if (!_open_pipe(output_reader, startup.hStdOutput, false) || !_open_pipe(error_reader, startup.hStdError, false)) {
		fprintf(stderr, "Could not create console output pipes (Windows error %lu).\n", GetLastError());
		return EXIT_FAILURE;
	}
	_input = GetStdHandle(STD_INPUT_HANDLE);
	DWORD input_mode = 0;
	const bool console_input = GetConsoleMode(_input, &input_mode) != FALSE;
	if (console_input) {
		if (!_open_pipe(_input_writer, startup.hStdInput, true)) {
			fprintf(stderr, "Could not create the console input pipe (Windows error %lu).\n", GetLastError());
			return EXIT_FAILURE;
		}
	} else if (!DuplicateHandle(GetCurrentProcess(), _input, GetCurrentProcess(), &startup.hStdInput, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
		SECURITY_ATTRIBUTES inherit = { sizeof(inherit), nullptr, TRUE };
		startup.hStdInput = CreateFile(_T("NUL"), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);
		if (startup.hStdInput == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "Could not open console input (Windows error %lu).\n", GetLastError());
			return EXIT_FAILURE;
		}
	}

	PROCESS_INFORMATION process = {};
	if (!CreateProcess(executable, command, nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr, nullptr, &startup, &process)) {
		fprintf(stderr, "Could not start the main executable (Windows error %lu).\n", GetLastError());
		return EXIT_FAILURE;
	}
	_process = process.hProcess;
	if (!SetConsoleCtrlHandler(_console_control, TRUE)) {
		const DWORD error = GetLastError();
		TerminateProcess(_process, EXIT_FAILURE);
		fprintf(stderr, "Could not register console control handling (Windows error %lu).\n", error);
		return EXIT_FAILURE;
	}
	if (console_input) {
		DWORD thread_id = 0; // Windows 9x requires a non-null thread ID pointer.
		HANDLE input_thread = CreateThread(nullptr, 0, _forward_input, nullptr, 0, &thread_id);
		if (!input_thread) {
			const DWORD error = GetLastError();
			TerminateProcess(_process, EXIT_FAILURE);
			fprintf(stderr, "Could not forward console input (Windows error %lu).\n", error);
			return EXIT_FAILURE;
		}
		CloseHandle(input_thread);
	}
	CloseHandle(startup.hStdInput);
	CloseHandle(startup.hStdOutput);
	CloseHandle(startup.hStdError);
	if (ResumeThread(process.hThread) == DWORD(-1)) {
		const DWORD error = GetLastError();
		TerminateProcess(_process, EXIT_FAILURE);
		fprintf(stderr, "Could not resume the main executable (Windows error %lu).\n", error);
		return EXIT_FAILURE;
	}
	CloseHandle(process.hThread);

	// Forward the GUI program's output through pipes, preserving separate stdout and stderr.
	HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE error_output = GetStdHandle(STD_ERROR_HANDLE);
	DWORD waited;
	do {
		const bool output_read = _forward_output(output_reader, output);
		const bool error_read = _forward_output(error_reader, error_output);
		waited = WaitForSingleObject(_process, output_read || error_read ? 0 : 10);
	} while (waited == WAIT_TIMEOUT);
	// Drain the remaining pipe buffers without waiting for handles inherited by
	// grandchildren. This wrapper follows the main process, including its exit code.
	_forward_output(output_reader, output);
	_forward_output(error_reader, error_output);
	DWORD exit_code = EXIT_FAILURE;
	if (waited != WAIT_OBJECT_0 || !GetExitCodeProcess(_process, &exit_code)) {
		fprintf(stderr, "Could not wait for the main executable (Windows error %lu).\n", GetLastError());
		return EXIT_FAILURE;
	}
	SetConsoleCtrlHandler(_console_control, FALSE);
	return exit_code;
}

int main() {
	const DWORD exit_code = WindowsLegacyConsoleWrapper::run();
	// ExitProcess ends the input thread if it is still waiting for a line;
	// it uses no CRT locks or engine data.
	ExitProcess(exit_code);
}
