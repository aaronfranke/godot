#pragma once

#include "core/io/dir_access.h"

class DirAccessWindows9x : public DirAccess {
	GDSOFTCLASS(DirAccessWindows9x, DirAccess);

	struct Search;
	String _current_dir;
	Search *_search = nullptr;
	bool _current_is_dir = false;
	bool _current_is_hidden = false;

	static bool _is_under_root(const String &p_path, const String &p_root);
	String _get_volume_root() const;

protected:
	String fix_path(const String &p_path) const override;

public:
	Error list_dir_begin() override;
	String get_next() override;
	bool current_is_dir() const override;
	bool current_is_hidden() const override;
	void list_dir_end() override;
	int get_drive_count() override;
	String get_drive(int p_drive) override;
	String get_drive_label(int p_drive) override;
	Error change_dir(String p_dir) override;
	String get_current_dir(bool p_include_drive = true) const override;
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
	bool is_case_sensitive(const String &p_path) const override;
	bool is_equivalent(const String &p_path_a, const String &p_path_b) const override;

	DirAccessWindows9x();
	~DirAccessWindows9x() override;
};
