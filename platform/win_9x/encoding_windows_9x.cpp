#include "encoding_windows_9x.h"

#include <windows.h>

Error Windows9xEncoding::encode(const String &p_text, CharString &r_native, bool p_allow_lossy) {
	r_native = CharString();
	if (p_text.is_empty()) {
		return OK;
	}
	if (p_text.find_char('\0') >= 0) {
		return ERR_INVALID_DATA;
	}
	const Char16String wide = p_text.utf16();
	if (wide.length() > INT32_MAX) {
		return ERR_INVALID_DATA;
	}
	const int length = int(wide.length());
	const wchar_t *source = reinterpret_cast<const wchar_t *>(wide.get_data());
	BOOL substituted = FALSE;
	const UINT code_page = GetACP();
	// Windows 10/11 can use UTF-8 as ACP; that conversion rejects a substitution pointer.
	BOOL *used_default = code_page == CP_UTF8 ? nullptr : &substituted;
	const int count = WideCharToMultiByte(code_page, 0, source, length, nullptr, 0, nullptr, used_default);
	if (count <= 0 || (!p_allow_lossy && substituted)) {
		return ERR_INVALID_DATA;
	}
	const Error error = r_native.resize_uninitialized(int64_t(count) + 1);
	if (error != OK) {
		return error;
	}
	const int written = WideCharToMultiByte(code_page, 0, source, length, r_native.ptrw(), count, nullptr, used_default);
	if (written != count || (!p_allow_lossy && substituted)) {
		r_native = CharString();
		return ERR_INVALID_DATA;
	}
	r_native.ptrw()[count] = '\0';
	// Win9x lacks a reliable no-best-fit conversion flag. Check the round trip for native paths.
	if (!p_allow_lossy && decode(r_native.get_data(), count) != p_text) {
		r_native = CharString();
		return ERR_INVALID_DATA;
	}
	return OK;
}

String Windows9xEncoding::decode(const char *p_text, int p_length, uint32_t p_code_page) {
	if (!p_text || p_length == 0 || p_text[0] == '\0') {
		return {};
	}
	const int count = MultiByteToWideChar(p_code_page, 0, p_text, p_length, nullptr, 0);
	if (count <= 0) {
		return {};
	}
	Char16String wide;
	if (wide.resize_uninitialized(int64_t(count) + 1) != OK) {
		return {};
	}
	if (MultiByteToWideChar(p_code_page, 0, p_text, p_length, reinterpret_cast<wchar_t *>(wide.ptrw()), count) != count) {
		return {};
	}
	wide.ptrw()[count] = '\0';
	return String::utf16(wide.get_data(), p_length < 0 ? count - 1 : count);
}
