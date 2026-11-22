#pragma once

#include "core/ustring.h"

class PathWindowsNT {
public:
	static bool has_embedded_null(const String &p_path);
	static String get_absolute(const String &p_path);
	static String to_native(const String &p_path, bool p_force_extended = false);
	static String get_volume_root(const String &p_path);
	static bool is_reserved_filename(const String &p_path);
};
