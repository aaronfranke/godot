#include "drivers/win_legacy/nt/os/os_windows_nt.h"

// Legacy Windows includes.
#include "drivers/win_legacy/nt/path_windows_nt.h"

#include <limits.h>

struct ProcessOutputWindowsNT {
	HANDLE read = nullptr;
	HANDLE write = nullptr;
	HANDLE null_device = INVALID_HANDLE_VALUE;

	~ProcessOutputWindowsNT() {
		if (read) {
			CloseHandle(read);
		}
		if (write) {
			CloseHandle(write);
		}
		if (null_device != INVALID_HANDLE_VALUE) {
			CloseHandle(null_device);
		}
	}
};

void OS_WindowsNT::_append_process_argument(String &r_command, const String &p_argument) {
	r_command += ' ';
	if (!p_argument.empty() && p_argument.find_char(' ') < 0 && p_argument.find_char('\t') < 0 && p_argument.find_char('"') < 0) {
		r_command += p_argument;
		return;
	}

	r_command += '"';
	int backslashes = 0;
	for (int i = 0; i < p_argument.length(); ++i) {
		const CharType character = p_argument[i];
		if (character == '\\') {
			++backslashes;
			continue;
		}
		// The argument parser treats backslashes before a quote as escape sequences.
		if (character == '"') {
			backslashes = backslashes * 2 + 1;
		}
		for (int j = 0; j < backslashes; ++j) {
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

Error OS_WindowsNT::_append_process_output(String &r_output, Mutex *p_mutex, const char *p_bytes, int p_size) {
	// Match the Windows backend's ANSI output decoding.
	const int size = MultiByteToWideChar(CP_ACP, 0, p_bytes, p_size, nullptr, 0);
	String decoded;
	if (size <= 0) {
		decoded = String::utf8(p_bytes, p_size);
	} else {
		ERR_FAIL_COND_V(size == INT_MAX || decoded.resize(size + 1) != OK, ERR_OUT_OF_MEMORY);
		if (MultiByteToWideChar(CP_ACP, 0, p_bytes, p_size, decoded.ptrw(), size) != size) {
			decoded = String::utf8(p_bytes, p_size);
		} else {
			decoded[size] = 0;
		}
	}
	if (p_mutex) {
		const MutexLock lock(*p_mutex);
		r_output += decoded;
	} else {
		r_output += decoded;
	}
	return OK;
}

Error OS_WindowsNT::_launch_process(const String &p_path, const List<String> &p_arguments, bool p_open_console, STARTUPINFOW &p_startup, PROCESS_INFORMATION &r_process) {
	ERR_FAIL_COND_V(p_path.empty() || p_path.find_char('"') >= 0 || PathWindowsNT::has_embedded_null(p_path), ERR_INVALID_PARAMETER);
	const String path = p_path.replace("/", "\\");
	String command = "\"" + path + "\"";
	for (auto *entry = p_arguments.front(); entry; entry = entry->next()) {
		const String &argument = entry->get();
		ERR_FAIL_COND_V(PathWindowsNT::has_embedded_null(argument), ERR_INVALID_PARAMETER);
		_append_process_argument(command, argument);
	}

	String application;
	if (path.length() >= MAX_PATH) {
		// CreateProcessW caps the executable token at MAX_PATH when locating it.
		const String absolute = PathWindowsNT::get_absolute(path);
		ERR_FAIL_COND_V(absolute.empty(), ERR_INVALID_PARAMETER);
		application = PathWindowsNT::to_native(absolute);
	}
	const bool inherit_handles = p_startup.dwFlags & STARTF_USESTDHANDLES;
	const DWORD flags = p_open_console ? CREATE_NEW_CONSOLE : CREATE_NO_WINDOW;
	String native_command = command;
	BOOL created = CreateProcessW(application.empty() ? nullptr : application.c_str(), native_command.ptrw(), nullptr, nullptr, inherit_handles, flags, nullptr, nullptr, &p_startup, &r_process);
	DWORD error = created ? ERROR_SUCCESS : GetLastError();
	if (error == ERROR_INSUFFICIENT_BUFFER && !application.empty()) {
		// Windows 7 WOW64 can reject even an explicit extended executable path.
		// Retry its existing short alias, preserving the caller's full argv[0].
		const DWORD size = GetShortPathNameW(application.c_str(), nullptr, 0);
		String short_application;
		if (size > 0 && size <= MAX_PATH && short_application.resize(size) == OK) {
			const DWORD length = GetShortPathNameW(application.c_str(), short_application.ptrw(), size);
			if (length > 0 && length < size) {
				// CreateProcessW may modify its command-line buffer, including on failure.
				native_command = command;
				created = CreateProcessW(short_application.c_str(), native_command.ptrw(), nullptr, nullptr, inherit_handles, flags, nullptr, nullptr, &p_startup, &r_process);
				if (!created) {
					error = GetLastError();
				}
			}
		}
	}
	if (!created) {
		ERR_FAIL_V_MSG(ERR_CANT_FORK, vformat("Could not create child process \"%s\" (Windows error %d).", p_path, uint32_t(error)));
	}
	CloseHandle(r_process.hThread);
	return OK;
}

Error OS_WindowsNT::execute(const String &p_path, const List<String> &p_arguments, bool p_blocking, ProcessID *r_child_id, String *r_pipe, int *r_exitcode, bool p_read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
	STARTUPINFOW startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process = {};
	ProcessOutputWindowsNT output;
	if (p_blocking && r_pipe) {
		// CreateProcess inherits every inheritable handle. Serialize the creation of child pipe ends.
		const MutexLock lock(_process_launch_mutex);
		ERR_FAIL_COND_V(!CreatePipe(&output.read, &output.write, nullptr, 0), ERR_CANT_FORK);
		ERR_FAIL_COND_V(!SetHandleInformation(output.write, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT), ERR_CANT_FORK);
		SECURITY_ATTRIBUTES inherit = { sizeof(inherit), nullptr, TRUE };
		output.null_device = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		ERR_FAIL_COND_V(output.null_device == INVALID_HANDLE_VALUE, ERR_CANT_FORK);
		startup.dwFlags = STARTF_USESTDHANDLES;
		startup.hStdInput = output.null_device;
		startup.hStdOutput = output.write;
		startup.hStdError = p_read_stderr ? output.write : output.null_device;
		const Error error = _launch_process(p_path, p_arguments, p_open_console, startup, process);
		CloseHandle(output.write);
		output.write = nullptr;
		CloseHandle(output.null_device);
		output.null_device = INVALID_HANDLE_VALUE;
		if (error != OK) {
			return error;
		}
	} else {
		const Error error = _launch_process(p_path, p_arguments, p_open_console, startup, process);
		if (error != OK) {
			return error;
		}
	}

	if (!p_blocking) {
		const MutexLock lock(_process_mutex);
		if (auto *previous = _child_processes.find(process.dwProcessId)) {
			CloseHandle(previous->get());
		}
		_child_processes.insert(process.dwProcessId, process.hProcess);
		if (r_child_id) {
			*r_child_id = process.dwProcessId;
		}
		return OK;
	}

	Error error = OK;
	if (output.read) {
		const int CHUNK_SIZE = 4096;
		Vector<char> bytes;
		int pending = 0;
		DWORD count = 0;
		for (;;) {
			if (pending > INT_MAX - CHUNK_SIZE || bytes.resize(pending + CHUNK_SIZE) != OK) {
				error = ERR_OUT_OF_MEMORY;
				break;
			}
			if (!ReadFile(output.read, bytes.ptrw() + pending, CHUNK_SIZE, &count, nullptr)) {
				if (GetLastError() != ERROR_BROKEN_PIPE) {
					error = ERR_FILE_CANT_READ;
				}
				break;
			}
			if (!count) {
				break;
			}
			pending += count;
			// Expose completed lines while the child runs, without splitting a
			// multibyte character across pipe reads. Keep only the unfinished line.
			for (int i = pending - 1; i >= pending - int(count); --i) {
				if (bytes[i] == '\n') {
					error = _append_process_output(*r_pipe, p_pipe_mutex, bytes.ptr(), i + 1);
					pending -= i + 1;
					memmove(bytes.ptrw(), bytes.ptr() + i + 1, pending);
					break;
				}
			}
			if (error != OK) {
				break;
			}
		}
		if (pending > 0 && error == OK) {
			error = _append_process_output(*r_pipe, p_pipe_mutex, bytes.ptr(), pending);
		}
		CloseHandle(output.read);
		output.read = nullptr;
	}

	DWORD exit_code = 0;
	if (WaitForSingleObject(process.hProcess, INFINITE) != WAIT_OBJECT_0 || !GetExitCodeProcess(process.hProcess, &exit_code)) {
		error = FAILED;
	} else if (r_exitcode) {
		*r_exitcode = int(exit_code);
	}
	CloseHandle(process.hProcess);
	return error;
}

Error OS_WindowsNT::kill(const ProcessID &p_pid) {
	ERR_FAIL_COND_V(p_pid <= 0 || uint64_t(p_pid) > MAXDWORD, ERR_INVALID_PARAMETER);
	const MutexLock lock(_process_mutex);
	if (auto *child = _child_processes.find(p_pid)) {
		if (!TerminateProcess(child->get(), 0)) {
			return FAILED;
		}
		CloseHandle(child->get());
		_child_processes.erase(child);
		return OK;
	}
	const HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(p_pid));
	if (!process) {
		return FAILED;
	}
	const bool terminated = TerminateProcess(process, 0);
	CloseHandle(process);
	return terminated ? OK : FAILED;
}

bool OS_WindowsNT::is_process_running(const ProcessID &p_pid) const {
	const MutexLock lock(_process_mutex);
	auto *child = _child_processes.find(p_pid);
	if (!child) {
		return false;
	}
	if (WaitForSingleObject(child->get(), 0) == WAIT_TIMEOUT) {
		return true;
	}
	CloseHandle(child->get());
	_child_processes.erase(child);
	return false;
}

void OS_WindowsNT::_close_child_processes() {
	const MutexLock lock(_process_mutex);
	for (auto *child = _child_processes.front(); child; child = child->next()) {
		CloseHandle(child->get());
	}
	_child_processes.clear();
}
