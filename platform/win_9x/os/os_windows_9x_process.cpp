#include "encoding_windows_9x.h"
#include "file_access_windows_9x_pipe.h"
#include "os_windows_9x.h"

struct ProcessPipeWindows9x {
	HANDLE read = nullptr;
	HANDLE write = nullptr;

	bool open(bool p_child_reads) {
		if (!CreatePipe(&read, &write, nullptr, 0)) {
			return false;
		}
		// 9x ignores CreatePipe's security attributes and does not implement
		// SetHandleInformation. Duplicate only the child's end as inheritable.
		HANDLE &child_end = p_child_reads ? read : write;
		HANDLE inherited = nullptr;
		if (!DuplicateHandle(GetCurrentProcess(), child_end, GetCurrentProcess(), &inherited, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
			return false;
		}
		CloseHandle(child_end);
		child_end = inherited;
		return true;
	}

	ProcessPipeWindows9x() = default;
	ProcessPipeWindows9x(const ProcessPipeWindows9x &) = delete;
	ProcessPipeWindows9x &operator=(const ProcessPipeWindows9x &) = delete;

	~ProcessPipeWindows9x() {
		if (read) {
			CloseHandle(read);
		}
		if (write) {
			CloseHandle(write);
		}
	}
};

void OS_Windows9x::_append_process_argument(String &r_command, const String &p_argument) {
	r_command += " \"";
	int backslashes = 0;
	for (char32_t character : p_argument.span()) {
		if (character == '\\') {
			++backslashes;
			continue;
		}
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

Error OS_Windows9x::_launch_process(const String &p_path, const List<String> &p_arguments, bool p_open_console, STARTUPINFOA &p_startup, PROCESS_INFORMATION &r_process) {
	if (p_path.is_empty() || p_path.find_char('"') >= 0 || p_path.find_char('\0') >= 0) {
		return ERR_INVALID_PARAMETER;
	}
	String command = "\"" + p_path.replace_char('/', '\\') + "\"";
	for (const String &argument : p_arguments) {
		if (argument.find_char('\0') >= 0) {
			return ERR_INVALID_PARAMETER;
		}
		_append_process_argument(command, argument);
	}
	CharString native_command;
	if (Windows9xEncoding::encode(command, native_command) != OK) {
		return ERR_INVALID_PARAMETER;
	}
	const DWORD flags = p_open_console ? CREATE_NEW_CONSOLE : DETACHED_PROCESS;
	const bool inherit = (p_startup.dwFlags & STARTF_USESTDHANDLES) != 0;
	if (!CreateProcessA(nullptr, native_command.ptrw(), nullptr, nullptr, inherit, flags, nullptr, nullptr, &p_startup, &r_process)) {
		return ERR_CANT_FORK;
	}
	CloseHandle(r_process.hThread);
	return OK;
}

Error OS_Windows9x::execute(const String &p_path, const List<String> &p_arguments, String *r_output, int *r_exit_code, bool p_read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
	STARTUPINFOA startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process = {};
	HANDLE reader = nullptr;
	Error error;
	{
		const MutexLock lock(_process_launch_mutex);
		HANDLE writer = nullptr;
		HANDLE child_writer = nullptr;
		HANDLE null_device = INVALID_HANDLE_VALUE;
		if (r_output) {
			if (!CreatePipe(&reader, &writer, nullptr, 0)) {
				return ERR_CANT_FORK;
			}
			// 9x ignores CreatePipe's security attributes. Duplicate only the child's
			// write end as inheritable instead of using NT's SetHandleInformation.
			const bool duplicated = DuplicateHandle(GetCurrentProcess(), writer, GetCurrentProcess(), &child_writer, 0, TRUE, DUPLICATE_SAME_ACCESS);
			CloseHandle(writer);
			SECURITY_ATTRIBUTES inherit = { sizeof(inherit), nullptr, TRUE };
			null_device = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (!duplicated || null_device == INVALID_HANDLE_VALUE) {
				CloseHandle(reader);
				if (child_writer) {
					CloseHandle(child_writer);
				}
				if (null_device != INVALID_HANDLE_VALUE) {
					CloseHandle(null_device);
				}
				return ERR_CANT_FORK;
			}
			startup.dwFlags = STARTF_USESTDHANDLES;
			startup.hStdInput = null_device;
			startup.hStdOutput = child_writer;
			startup.hStdError = p_read_stderr ? child_writer : null_device;
		}
		error = _launch_process(p_path, p_arguments, p_open_console, startup, process);
		if (child_writer) {
			CloseHandle(child_writer);
		}
		if (null_device != INVALID_HANDLE_VALUE) {
			CloseHandle(null_device);
		}
	}
	if (error != OK) {
		if (reader) {
			CloseHandle(reader);
		}
		return error;
	}
	PackedByteArray output;
	if (reader) {
		for (;;) {
			DWORD available = 0;
			if (!PeekNamedPipe(reader, nullptr, 0, nullptr, &available, nullptr)) {
				if (GetLastError() != ERROR_BROKEN_PIPE) {
					error = ERR_FILE_CANT_READ;
				}
				break;
			}
			if (!available) {
				const DWORD waited = WaitForSingleObject(process.hProcess, 1);
				if (waited == WAIT_OBJECT_0) {
					// The child may have written between the first peek and its exit.
					if (!PeekNamedPipe(reader, nullptr, 0, nullptr, &available, nullptr) || !available) {
						break;
					}
				} else if (waited != WAIT_TIMEOUT) {
					error = FAILED;
					break;
				}
				if (!available) {
					continue;
				}
			}
			const int64_t previous = output.size();
			const DWORD chunk = MIN(available, DWORD(4096));
			if (output.resize(previous + chunk) != OK) {
				error = ERR_OUT_OF_MEMORY;
				break;
			}
			DWORD count = 0;
			if (!ReadFile(reader, output.ptrw() + previous, chunk, &count, nullptr)) {
				error = ERR_FILE_CANT_READ;
				output.resize(previous);
				break;
			}
			output.resize(previous + count);
		}
		CloseHandle(reader);
		if (!output.is_empty()) {
			const String text = Windows9xEncoding::decode(reinterpret_cast<const char *>(output.ptr()), output.size());
			if (p_pipe_mutex) {
				const MutexLock lock(*p_pipe_mutex);
				*r_output += text;
			} else {
				*r_output += text;
			}
		}
	}
	DWORD exit_code = 0;
	if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0 || !GetExitCodeProcess(process.hProcess, &exit_code)) {
		error = FAILED;
	}
	CloseHandle(process.hProcess);
	if (r_exit_code && error == OK) {
		*r_exit_code = int(exit_code);
	}
	return error;
}

Dictionary OS_Windows9x::execute_with_pipe(const String &p_path, const List<String> &p_arguments, bool p_blocking) {
	if (!p_blocking) {
		// Native 9x anonymous pipes reject PIPE_NOWAIT. Do not silently return
		// blocking streams when the caller explicitly requests immediate I/O.
		WARN_PRINT_ONCE("Windows 9x does not support nonblocking process pipes. Use blocking=true.");
		return {};
	}
	Ref<FileAccessWindows9xPipe> stdio;
	stdio.instantiate();
	Ref<FileAccessWindows9xPipe> error_stream;
	error_stream.instantiate();
	PROCESS_INFORMATION process = {};
	{
		const MutexLock lock(_process_launch_mutex);
		ProcessPipeWindows9x input;
		ProcessPipeWindows9x output;
		ProcessPipeWindows9x error_output;
		if (!input.open(true) || !output.open(false) || !error_output.open(false)) {
			return {};
		}
		STARTUPINFOA startup = {};
		startup.cb = sizeof(startup);
		startup.dwFlags = STARTF_USESTDHANDLES;
		startup.hStdInput = input.read;
		startup.hStdOutput = output.write;
		startup.hStdError = error_output.write;
		if (_launch_process(p_path, p_arguments, false, startup, process) != OK) {
			return {};
		}
		stdio->open_existing(output.read, input.write);
		output.read = nullptr;
		input.write = nullptr;
		error_stream->open_existing(error_output.read, nullptr);
		error_output.read = nullptr;
	}
	_track_child_process(process);
	Dictionary result;
	result["stdio"] = stdio;
	result["stderr"] = error_stream;
	result["pid"] = ProcessID(process.dwProcessId);
	return result;
}

void OS_Windows9x::_track_child_process(const PROCESS_INFORMATION &p_process) {
	const MutexLock lock(_process_mutex);
	ChildProcess *previous = _child_processes.getptr(p_process.dwProcessId);
	if (previous && previous->handle) {
		CloseHandle(previous->handle);
	}
	_child_processes.insert(p_process.dwProcessId, ChildProcess{ p_process.hProcess });
}

Error OS_Windows9x::create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) {
	const MutexLock lock(_process_launch_mutex);
	STARTUPINFOA startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process = {};
	const Error error = _launch_process(p_path, p_arguments, p_open_console, startup, process);
	if (error == OK) {
		_track_child_process(process);
		if (r_child_id) {
			*r_child_id = process.dwProcessId;
		}
	}
	return error;
}

bool OS_Windows9x::_poll_child_process(ChildProcess &r_child) {
	if (!r_child.handle) {
		return false;
	}
	const DWORD result = WaitForSingleObject(r_child.handle, 0);
	if (result == WAIT_TIMEOUT) {
		return true;
	}
	DWORD exit_code;
	if (result == WAIT_OBJECT_0 && GetExitCodeProcess(r_child.handle, &exit_code)) {
		r_child.exit_code = int(exit_code);
		CloseHandle(r_child.handle);
		r_child.handle = nullptr;
	}
	return false;
}

DWORD OS_Windows9x::_get_native_process_id(const ProcessID &p_pid) {
	// 9x process IDs can set bit 31, while Godot's script process APIs use int.
	// Accept their signed representation as well as the native full DWORD.
	return p_pid < INT32_MIN || p_pid > MAXDWORD ? 0 : DWORD(p_pid);
}

bool OS_Windows9x::is_process_running(const ProcessID &p_pid) const {
	const DWORD native_pid = _get_native_process_id(p_pid);
	if (!native_pid) {
		return false;
	}
	const MutexLock lock(_process_mutex);
	ChildProcess *child = _child_processes.getptr(native_pid);
	if (child) {
		return _poll_child_process(*child);
	}
	const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, native_pid);
	if (!process) {
		return false;
	}
	const bool running = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
	CloseHandle(process);
	return running;
}

int OS_Windows9x::get_process_exit_code(const ProcessID &p_pid) const {
	const MutexLock lock(_process_mutex);
	ChildProcess *child = _child_processes.getptr(_get_native_process_id(p_pid));
	if (!child || _poll_child_process(*child)) {
		return -1;
	}
	return child->exit_code;
}

Error OS_Windows9x::kill(const ProcessID &p_pid) {
	const DWORD native_pid = _get_native_process_id(p_pid);
	ERR_FAIL_COND_V(!native_pid, ERR_INVALID_PARAMETER);
	const MutexLock lock(_process_mutex);
	ChildProcess *child = _child_processes.getptr(native_pid);
	// A retained child handle prevents a recycled numeric ID from selecting another process.
	const HANDLE process = child ? child->handle : OpenProcess(PROCESS_TERMINATE, FALSE, native_pid);
	if (!process) {
		return FAILED;
	}
	const bool killed = TerminateProcess(process, 0);
	if (!child) {
		CloseHandle(process);
	}
	return killed ? OK : FAILED;
}
