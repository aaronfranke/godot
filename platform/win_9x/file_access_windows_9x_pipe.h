#pragma once

#include "core/io/file_access.h"

#include <windows.h>

// Anonymous child-process streams only. Windows 9x cannot host NT named pipes.
class FileAccessWindows9xPipe : public FileAccess {
	GDSOFTCLASS(FileAccessWindows9xPipe, FileAccess);

	HANDLE _read = nullptr;
	HANDLE _write = nullptr;
	mutable Error _error = ERR_UNCONFIGURED;
	mutable bool _eof = false;

protected:
	Error open_internal(const String &p_path, int p_mode_flags) override { return ERR_UNAVAILABLE; }
	uint64_t _get_modified_time(const String &p_file) override { return 0; }
	uint64_t _get_access_time(const String &p_file) override { return 0; }
	int64_t _get_size(const String &p_file) override { return -1; }

public:
	void open_existing(HANDLE p_read, HANDLE p_write);
	bool is_open() const override { return _read || _write; }
	void close() override;
	void seek(uint64_t p_position) override {}
	void seek_end(int64_t p_position = 0) override {}
	uint64_t get_position() const override { return 0; }
	uint64_t get_length() const override;
	bool eof_reached() const override { return _eof; }
	Error get_error() const override { return _error; }
	uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;
	bool store_buffer(const uint8_t *p_src, uint64_t p_length) override;
	Error resize(int64_t p_length) override { return ERR_UNAVAILABLE; }
	void flush() override {}
	bool file_exists(const String &p_name) override { return false; }
	BitField<UnixPermissionFlags> _get_unix_permissions(const String &p_file) override { return 0; }
	Error _set_unix_permissions(const String &p_file, BitField<UnixPermissionFlags> p_permissions) override { return ERR_UNAVAILABLE; }
	bool _get_hidden_attribute(const String &p_file) override { return false; }
	Error _set_hidden_attribute(const String &p_file, bool p_hidden) override { return ERR_UNAVAILABLE; }
	bool _get_read_only_attribute(const String &p_file) override { return false; }
	Error _set_read_only_attribute(const String &p_file, bool p_read_only) override { return ERR_UNAVAILABLE; }

	~FileAccessWindows9xPipe() override;
};
