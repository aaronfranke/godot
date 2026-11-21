#include "file_access_windows_9x_pipe.h"

void FileAccessWindows9xPipe::open_existing(HANDLE p_read, HANDLE p_write) {
	close();
	_read = p_read;
	_write = p_write;
	_error = OK;
	_eof = false;
}

void FileAccessWindows9xPipe::close() {
	if (_read) {
		CloseHandle(_read);
		_read = nullptr;
	}
	if (_write) {
		CloseHandle(_write);
		_write = nullptr;
	}
	_error = ERR_UNCONFIGURED;
}

uint64_t FileAccessWindows9xPipe::get_length() const {
	if (!_read) {
		_error = ERR_UNCONFIGURED;
		return 0;
	}
	DWORD available = 0;
	if (!PeekNamedPipe(_read, nullptr, 0, nullptr, &available, nullptr)) {
		_eof = GetLastError() == ERROR_BROKEN_PIPE;
		_error = _eof ? ERR_FILE_EOF : ERR_FILE_CANT_READ;
		return 0;
	}
	_error = OK;
	return available;
}

uint64_t FileAccessWindows9xPipe::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	if (!_read || (!p_dst && p_length)) {
		_error = ERR_INVALID_PARAMETER;
		return 0;
	}
	if (!p_length) {
		_error = OK;
		return 0;
	}
	DWORD count = 0;
	if (!ReadFile(_read, p_dst, DWORD(MIN(p_length, uint64_t(MAXDWORD))), &count, nullptr)) {
		_eof = GetLastError() == ERROR_BROKEN_PIPE;
		_error = _eof ? ERR_FILE_EOF : ERR_FILE_CANT_READ;
	} else {
		_error = count == p_length ? OK : ERR_FILE_CANT_READ;
	}
	return count;
}

bool FileAccessWindows9xPipe::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	if (!_write || (!p_src && p_length)) {
		_error = ERR_FILE_CANT_WRITE;
		return false;
	}
	uint64_t total = 0;
	while (total < p_length) {
		DWORD count = 0;
		if (!WriteFile(_write, p_src + total, DWORD(MIN(p_length - total, uint64_t(MAXDWORD))), &count, nullptr) || !count) {
			_error = ERR_FILE_CANT_WRITE;
			return false;
		}
		total += count;
	}
	_error = OK;
	return true;
}

FileAccessWindows9xPipe::~FileAccessWindows9xPipe() {
	close();
}
