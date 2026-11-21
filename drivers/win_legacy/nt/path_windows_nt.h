#pragma once

#include "core/string/ustring.h"

class PathWindowsNT {
public:
	static String get_absolute(const String &p_path);
	static Char16String to_native(const String &p_path, bool p_force_extended = false);
	static Char16String get_volume_root(const String &p_path);
	static bool is_reserved_filename(const String &p_path);
};
