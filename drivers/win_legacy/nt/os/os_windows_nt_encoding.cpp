#include "drivers/win_legacy/nt/os/os_windows_nt.h"

// System includes.
#include <limits.h>

String OS_WindowsNT::multibyte_to_string(const String &p_encoding, const PackedByteArray &p_array) const {
	if (p_array.is_empty()) {
		return String();
	}
	const int *code_page = _encodings.getptr(p_encoding.to_upper());
	ERR_FAIL_NULL_V_MSG(code_page, String(), vformat("Unknown Windows encoding: %s.", p_encoding));
	ERR_FAIL_COND_V(p_array.size() > INT_MAX, String());
	const char *bytes = reinterpret_cast<const char *>(p_array.ptr());
	const int byte_count = int(p_array.size());
	const int wide_length = MultiByteToWideChar(*code_page, 0, bytes, byte_count, nullptr, 0);
	ERR_FAIL_COND_V_MSG(wide_length <= 0, String(), vformat("Could not decode Windows code page %d (error %d).", *code_page, GetLastError()));

	Char16String wide;
	ERR_FAIL_COND_V(wide.resize_uninitialized(wide_length) != OK, String());
	const int written = MultiByteToWideChar(*code_page, 0, bytes, byte_count, reinterpret_cast<wchar_t *>(wide.ptrw()), wide_length);
	ERR_FAIL_COND_V(written != wide_length, String());
	return String::utf16(wide.ptr(), written);
}

PackedByteArray OS_WindowsNT::string_to_multibyte(const String &p_encoding, const String &p_string) const {
	if (p_string.is_empty()) {
		return PackedByteArray();
	}
	const int *code_page = _encodings.getptr(p_encoding.to_upper());
	ERR_FAIL_NULL_V_MSG(code_page, PackedByteArray(), vformat("Unknown Windows encoding: %s.", p_encoding));
	const Char16String wide = p_string.utf16();
	ERR_FAIL_COND_V(wide.size() > INT_MAX, PackedByteArray());
	const wchar_t *text = reinterpret_cast<const wchar_t *>(wide.ptr());
	// Pass the UTF-16 terminator through the encoder to match the Windows backend.
	const int wide_length = int(wide.size());
	const int byte_count = WideCharToMultiByte(*code_page, 0, text, wide_length, nullptr, 0, nullptr, nullptr);
	ERR_FAIL_COND_V_MSG(byte_count <= 0, PackedByteArray(), vformat("Could not encode Windows code page %d (error %d).", *code_page, GetLastError()));

	PackedByteArray bytes;
	ERR_FAIL_COND_V(bytes.resize_uninitialized(byte_count) != OK, PackedByteArray());
	const int written = WideCharToMultiByte(*code_page, 0, text, wide_length, reinterpret_cast<char *>(bytes.ptrw()), byte_count, nullptr, nullptr);
	ERR_FAIL_COND_V(written != byte_count, PackedByteArray());
	return bytes;
}
