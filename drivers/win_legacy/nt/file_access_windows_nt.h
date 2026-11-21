#pragma once

#include "core/io/file_access.h"

#include <windows.h>

class FileAccessWindowsNT : public FileAccess {
	GDSOFTCLASS(FileAccessWindowsNT, FileAccess);

	String _path;
	String _absolute_path;
	Char16String _pending_save;
	void *_file = nullptr;
	mutable Error _error = ERR_UNCONFIGURED;
	mutable bool _eof = false;
	bool _write_failed = false;

	bool _seek(int64_t p_offset, uint32_t p_origin) const;
	Char16String _get_native_path(const String &p_path) const;
	Char16String _get_stream_path(const String &p_file, const String &p_name);

	static uint64_t _filetime_to_unix(const FILETIME &p_time);
	static bool _query_file_metadata(const Char16String &p_path, WIN32_FILE_ATTRIBUTE_DATA &r_data);
	static bool _set_file_pointer(HANDLE p_file, LARGE_INTEGER p_distance, LARGE_INTEGER *r_position, DWORD p_origin);
	static Error _set_file_attribute(const Char16String &p_path, DWORD p_attribute, bool p_enabled);

protected:
	Error open_internal(const String &p_path, int p_mode_flags) override;
	uint64_t _get_modified_time(const String &p_file) override;
	uint64_t _get_access_time(const String &p_file) override;
	int64_t _get_size(const String &p_file) override;

public:
	bool is_open() const override;
	String get_path() const override;
	String get_path_absolute() const override;
	void close() override;
	void seek(uint64_t p_position) override;
	void seek_end(int64_t p_position = 0) override;
	uint64_t get_position() const override;
	uint64_t get_length() const override;
	bool eof_reached() const override;
	Error get_error() const override;

	uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;
	bool store_buffer(const uint8_t *p_src, uint64_t p_length) override;
	Error resize(int64_t p_length) override;
	void flush() override;
	bool file_exists(const String &p_name) override;

	BitField<UnixPermissionFlags> _get_unix_permissions(const String &p_file) override;
	Error _set_unix_permissions(const String &p_file, BitField<UnixPermissionFlags> p_permissions) override;
	bool _get_hidden_attribute(const String &p_file) override;
	Error _set_hidden_attribute(const String &p_file, bool p_hidden) override;
	bool _get_read_only_attribute(const String &p_file) override;
	Error _set_read_only_attribute(const String &p_file, bool p_read_only) override;
	PackedByteArray _get_extended_attribute(const String &p_file, const String &p_attribute_name) override;
	Error _set_extended_attribute(const String &p_file, const String &p_attribute_name, const PackedByteArray &p_data) override;
	Error _remove_extended_attribute(const String &p_file, const String &p_attribute_name) override;
	PackedStringArray _get_extended_attributes_list(const String &p_file) override;

	~FileAccessWindowsNT() override;
};
