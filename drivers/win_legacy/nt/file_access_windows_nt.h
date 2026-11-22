#pragma once

#include "core/os/file_access.h"

#include <windows.h>

class FileAccessWindowsNT : public FileAccess {
	String _path;
	String _absolute_path;
	String _pending_save;
	HANDLE _file = nullptr;
	mutable Error _error = ERR_UNCONFIGURED;
	mutable bool _eof = false;
	bool _write_failed = false;

	bool _seek(int64_t p_offset, uint32_t p_origin) const;
	String _get_native_path(const String &p_path) const;

	static uint64_t _filetime_to_unix(const FILETIME &p_time);
	static bool _query_file_metadata(const String &p_path, WIN32_FILE_ATTRIBUTE_DATA &r_data);
	static bool _set_file_pointer(HANDLE p_file, LARGE_INTEGER p_distance, LARGE_INTEGER *r_position, DWORD p_origin);

protected:
	Error _open(const String &p_path, int p_mode_flags) override;
	uint64_t _get_modified_time(const String &p_file) override;

public:
	bool is_open() const override;
	String get_path() const override;
	String get_path_absolute() const override;
	void close() override;
	void seek(uint64_t p_position) override;
	void seek_end(int64_t p_position = 0) override;
	uint64_t get_position() const override;
	uint64_t get_len() const override;
	bool eof_reached() const override;
	Error get_error() const override;

	uint8_t get_8() const override;
	uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;
	void store_8(uint8_t p_value) override;
	void store_buffer(const uint8_t *p_src, uint64_t p_length) override;
	void flush() override;
	bool file_exists(const String &p_name) override;

	uint32_t _get_unix_permissions(const String &p_file) override;
	Error _set_unix_permissions(const String &p_file, uint32_t p_permissions) override;

	~FileAccessWindowsNT() override;
};
