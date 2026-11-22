#pragma once

#include "core/ustring.h"

class Windows9xEncoding {
public:
	static Error encode(const String &p_text, CharString &r_native, bool p_allow_lossy = false);
	static String decode(const char *p_text, int p_length = -1, uint32_t p_code_page = 0);
};
