#include "drivers/win_legacy/nt/os/os_windows_nt.h"

// Godot includes.
#include "core/templates/local_vector.h"
#include "drivers/win_legacy/nt/path_windows_nt.h"
#include "drivers/windows/file_access_windows_pipe.h"

#include <utility>

struct ProcessPipeWindowsNT {
	HANDLE read = nullptr;
	HANDLE write = nullptr;

	bool open(bool p_child_reads, bool p_blocking = true) {
		if (!CreatePipe(&read, &write, nullptr, 0)) {
			return false;
		}
		if (!p_blocking) {
			DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
			if (!SetNamedPipeHandleState(p_child_reads ? write : read, &mode, nullptr, nullptr)) {
				return false;
			}
		}
		return SetHandleInformation(p_child_reads ? read : write, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
	}

	ProcessPipeWindowsNT() = default;
	ProcessPipeWindowsNT(const ProcessPipeWindowsNT &) = delete;
	ProcessPipeWindowsNT &operator=(const ProcessPipeWindowsNT &) = delete;

	~ProcessPipeWindowsNT() {
		if (read) {
			CloseHandle(read);
		}
		if (write) {
			CloseHandle(write);
		}
	}
};

void OS_WindowsNT::_append_process_argument(String &r_command, const String &p_argument) {
	r_command += ' ';
	if (!p_argument.is_empty() && p_argument.find_char(' ') < 0 && p_argument.find_char('\t') < 0 && p_argument.find_char('"') < 0) {
		r_command += p_argument;
		return;
	}

	r_command += '"';
	int backslashes = 0;
	for (char32_t character : p_argument.span()) {
		if (character == '\\') {
			++backslashes;
			continue;
		}
		// The argument parser treats backslashes before a quote as escape sequences.
		if (character == '"') {
			backslashes = backslashes * 2 + 1;
		}
		for (int i = 0; i < backslashes; ++i) {
			r_command += '\\';
		}
		backslashes = 0;
		r_command += character;
	}
	for (int i = 0; i < backslashes * 2; ++i) {
		r_command += '\\';
	}
	r_command += '"';
}

void OS_WindowsNT::_append_process_output(String &r_output, Mutex *p_mutex, const char *p_bytes, int p_size) {
	// Match the Windows backend's ANSI output decoding; byte pipes allow other child encodings.
	String output;
	const int wide_length = MultiByteToWideChar(CP_ACP, 0, p_bytes, p_size, nullptr, 0);
	if (wide_length > 0) {
		Char16String wide;
		wide.resize_uninitialized(wide_length);
		if (MultiByteToWideChar(CP_ACP, 0, p_bytes, p_size, reinterpret_cast<wchar_t *>(wide.ptrw()), wide_length) > 0) {
			output = String::utf16(wide.ptr(), wide_length);
		}
	}
	if (output.is_empty()) {
		output = String::utf8(p_bytes, p_size);
	}

	if (p_mutex) {
		const MutexLock lock(*p_mutex);
		r_output += output;
	} else {
		r_output += output;
	}
}

Error OS_WindowsNT::_launch_process(const String &p_path, const List<String> &p_arguments, bool p_open_console, STARTUPINFOW &p_startup, PROCESS_INFORMATION &r_process) {
	ERR_FAIL_COND_V(p_path.is_empty() || p_path.find_char('"') >= 0 || p_path.find_char('\0') >= 0, ERR_INVALID_PARAMETER);
	const String path = p_path.replace_char('/', '\\');
	String command = "\"" + path + "\"";
	for (const String &argument : p_arguments) {
		ERR_FAIL_COND_V(argument.find_char('\0') >= 0, ERR_INVALID_PARAMETER);
		_append_process_argument(command, argument);
	}
	Char16String native_command = command.utf16();
	Char16String application;
	if (path.utf16().length() >= MAX_PATH) {
		// CreateProcessW caps the executable token at MAX_PATH when it locates the executable itself.
		const String absolute = PathWindowsNT::get_absolute(path);
		ERR_FAIL_COND_V(absolute.is_empty(), ERR_INVALID_PARAMETER);
		application = PathWindowsNT::to_native(absolute);
	}
	const bool inherit_handles = p_startup.dwFlags & STARTF_USESTDHANDLES;
	const DWORD flags = p_open_console ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;
	BOOL created = CreateProcessW(reinterpret_cast<const wchar_t *>(application.ptr()), reinterpret_cast<wchar_t *>(native_command.ptrw()), nullptr, nullptr, inherit_handles, flags, nullptr, nullptr, &p_startup, &r_process);
	DWORD error = created ? ERROR_SUCCESS : GetLastError();
	if (error == ERROR_INSUFFICIENT_BUFFER && !application.is_empty()) {
		// Windows 7 WOW64 can reject even an explicit extended executable path.
		// Retry its existing short alias, preserving the caller's full argv[0].
		const DWORD size = GetShortPathNameW(reinterpret_cast<const wchar_t *>(application.ptr()), nullptr, 0);
		if (size > 0 && size <= MAX_PATH) {
			Char16String short_application;
			short_application.resize_uninitialized(size);
			const DWORD length = GetShortPathNameW(reinterpret_cast<const wchar_t *>(application.ptr()), reinterpret_cast<wchar_t *>(short_application.ptrw()), size);
			if (length > 0 && length < size) {
				// CreateProcessW may modify its command-line buffer, including on failure.
				native_command = command.utf16();
				created = CreateProcessW(reinterpret_cast<const wchar_t *>(short_application.ptr()), reinterpret_cast<wchar_t *>(native_command.ptrw()), nullptr, nullptr, inherit_handles, flags, nullptr, nullptr, &p_startup, &r_process);
				if (!created) {
					error = GetLastError();
				}
			}
		}
	}
	if (!created) {
		ERR_FAIL_V_MSG(ERR_CANT_FORK, vformat("Could not create child process \"%s\" (Windows error %d).", p_path, error));
	}
	CloseHandle(r_process.hThread);
	return OK;
}

Error OS_WindowsNT::execute(const String &p_path, const List<String> &p_arguments, String *r_output, int *r_exit_code, bool p_read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
	STARTUPINFOW startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process = {};
	HANDLE reader = nullptr;
	if (r_output) {
		// Keep the child pipe ends private to this launch.
		const MutexLock lock(_process_launch_mutex);
		ProcessPipeWindowsNT output;
		ERR_FAIL_COND_V(!output.open(false), ERR_CANT_FORK);
		SECURITY_ATTRIBUTES inherit = { sizeof(inherit), nullptr, TRUE };
		HANDLE null_device = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		ERR_FAIL_COND_V(null_device == INVALID_HANDLE_VALUE, ERR_CANT_FORK);
		startup.dwFlags = STARTF_USESTDHANDLES;
		startup.hStdInput = null_device;
		startup.hStdOutput = output.write;
		startup.hStdError = p_read_stderr ? output.write : null_device;
		const Error error = _launch_process(p_path, p_arguments, p_open_console, startup, process);
		CloseHandle(null_device);
		if (error != OK) {
			return error;
		}
		reader = std::exchange(output.read, nullptr);
	} else {
		const Error error = _launch_process(p_path, p_arguments, p_open_console, startup, process);
		if (error != OK) {
			return error;
		}
	}

	Error error = OK;
	if (reader) {
		const int CHUNK_SIZE = 4096;
		LocalVector<char> bytes;
		int pending = 0;
		for (;;) {
			bytes.resize(pending + CHUNK_SIZE);
			DWORD count = 0;
			if (!ReadFile(reader, bytes.ptr() + pending, CHUNK_SIZE, &count, nullptr)) {
				if (GetLastError() != ERROR_BROKEN_PIPE) {
					error = ERR_FILE_CANT_READ;
				}
				break;
			}
			if (count == 0) {
				break;
			}
			pending += count;
			// Flush complete lines without splitting a multibyte character across reads.
			for (int i = pending - 1; i >= pending - int(count); --i) {
				if (bytes[i] == '\n') {
					_append_process_output(*r_output, p_pipe_mutex, bytes.ptr(), i + 1);
					pending -= i + 1;
					memmove(bytes.ptr(), bytes.ptr() + i + 1, pending);
					break;
				}
			}
		}
		if (pending > 0) {
			_append_process_output(*r_output, p_pipe_mutex, bytes.ptr(), pending);
		}
		CloseHandle(reader);
	}

	DWORD exit_code = 0;
	if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0 || !GetExitCodeProcess(process.hProcess, &exit_code)) {
		error = FAILED;
	}
	CloseHandle(process.hProcess);
	if (error == OK && r_exit_code) {
		*r_exit_code = int(exit_code);
	}
	return error;
}

Dictionary OS_WindowsNT::execute_with_pipe(const String &p_path, const List<String> &p_arguments, bool p_blocking) {
	Ref<FileAccessWindowsPipe> stdio;
	stdio.instantiate();
	Ref<FileAccessWindowsPipe> stderr_pipe;
	stderr_pipe.instantiate();

	PROCESS_INFORMATION process = {};
	{
		const MutexLock lock(_process_launch_mutex);
		ProcessPipeWindowsNT input;
		ProcessPipeWindowsNT output;
		ProcessPipeWindowsNT error_output;
		ERR_FAIL_COND_V(!input.open(true, p_blocking) || !output.open(false, p_blocking) || !error_output.open(false, p_blocking), Dictionary());

		STARTUPINFOW startup = {};
		startup.cb = sizeof(startup);
		startup.dwFlags = STARTF_USESTDHANDLES;
		startup.hStdInput = input.read;
		startup.hStdOutput = output.write;
		startup.hStdError = error_output.write;
		if (_launch_process(p_path, p_arguments, false, startup, process) != OK) {
			return Dictionary();
		}

		// The parent ends already have the requested wait mode.
		stdio->open_existing(std::exchange(output.read, nullptr), std::exchange(input.write, nullptr), true);
		stderr_pipe->open_existing(std::exchange(error_output.read, nullptr), nullptr, true);
	}
	_track_child_process(process);

	Dictionary result;
	result["stdio"] = stdio;
	result["stderr"] = stderr_pipe;
	result["pid"] = ProcessID(process.dwProcessId);
	return result;
}

void OS_WindowsNT::_track_child_process(const PROCESS_INFORMATION &p_process) {
	const MutexLock lock(_process_mutex);
	ChildProcess *previous = _child_processes.getptr(p_process.dwProcessId);
	if (previous && previous->handle) {
		CloseHandle(previous->handle);
	}
	_child_processes.insert(p_process.dwProcessId, ChildProcess{ p_process.hProcess });
}

Error OS_WindowsNT::create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) {
	STARTUPINFOW startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process = {};
	const Error error = _launch_process(p_path, p_arguments, p_open_console, startup, process);
	if (error != OK) {
		return error;
	}
	_track_child_process(process);
	if (r_child_id) {
		*r_child_id = process.dwProcessId;
	}
	return OK;
}

Error OS_WindowsNT::kill(const ProcessID &p_pid) {
	ERR_FAIL_COND_V(p_pid <= 0 || p_pid > MAXDWORD, ERR_INVALID_PARAMETER);
	const MutexLock lock(_process_mutex);
	ChildProcess *child = _child_processes.getptr(p_pid);
	if (child) {
		if (!child->handle || !TerminateProcess(child->handle, 0)) {
			return FAILED;
		}
		CloseHandle(child->handle);
		_child_processes.erase(p_pid);
		return OK;
	}

	HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(p_pid));
	if (!process) {
		return FAILED;
	}
	const bool terminated = TerminateProcess(process, 0);
	CloseHandle(process);
	return terminated ? OK : FAILED;
}

bool OS_WindowsNT::_poll_child_process(ChildProcess &r_child) {
	if (!r_child.handle) {
		return false;
	}
	// STILL_ACTIVE (259) is also a valid exit code for a signaled process.
	const DWORD wait_result = WaitForSingleObject(r_child.handle, 0);
	if (wait_result == WAIT_TIMEOUT) {
		return true;
	}
	DWORD exit_code = 0;
	if (wait_result == WAIT_OBJECT_0 && GetExitCodeProcess(r_child.handle, &exit_code)) {
		r_child.exit_code = int(exit_code);
		CloseHandle(r_child.handle);
		r_child.handle = nullptr;
	}
	return false;
}

bool OS_WindowsNT::is_process_running(const ProcessID &p_pid) const {
	const MutexLock lock(_process_mutex);
	ChildProcess *child = _child_processes.getptr(p_pid);
	return child && _poll_child_process(*child);
}

int OS_WindowsNT::get_process_exit_code(const ProcessID &p_pid) const {
	const MutexLock lock(_process_mutex);
	ChildProcess *child = _child_processes.getptr(p_pid);
	if (!child || _poll_child_process(*child)) {
		return -1;
	}
	return child->exit_code;
}

void OS_WindowsNT::_close_child_processes() {
	const MutexLock lock(_process_mutex);
	for (const KeyValue<ProcessID, ChildProcess> &entry : _child_processes) {
		if (entry.value.handle) {
			CloseHandle(entry.value.handle);
		}
	}
	_child_processes.clear();
}
