#pragma once

#include "core/os/dir_access.h"

class DirAccessWindowsNT : public DirAccess {
	struct Search;
	String _current_dir;
	Search *_search = nullptr;
	bool _current_is_dir = false;
	bool _current_is_hidden = false;

	static bool _is_under_root(const String &p_path, const String &p_root);

	String _get_absolute_path(const String &p_path) const;

public:
	Error list_dir_begin() override;
	String get_next() override;
	bool current_is_dir() const override;
	bool current_is_hidden() const override;
	void list_dir_end() override;

	int get_drive_count() override;
	String get_drive(int p_drive) override;
	Error change_dir(String p_dir) override;
	String get_current_dir() override;
	Error make_dir(String p_dir) override;
	bool file_exists(String p_file) override;
	bool dir_exists(String p_dir) override;
	Error rename(String p_from, String p_to) override;
	Error remove(String p_name) override;

	bool is_link(String p_file) override;
	String read_link(String p_file) override;
	Error create_link(String p_source, String p_target) override;
	uint64_t get_space_left() override;
	String get_filesystem_type() const override;

	DirAccessWindowsNT();
	~DirAccessWindowsNT() override;
};
