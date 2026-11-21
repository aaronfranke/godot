#pragma once

#include "core/string/ustring.h"

class PathWindows9x {
public:
	enum { TEMPORARY_ATTEMPTS = 256 };

	static String get_absolute(const String &p_path);
	static CharString to_native(const String &p_path);
	static bool is_reserved_filename(const String &p_path);
	static bool is_equivalent(const String &p_path_a, const String &p_path_b);
	static String get_temporary_path(const String &p_destination, const String &p_kind, uint32_t p_sequence);
	static Error replace_file(const String &p_source, const String &p_destination);
};
