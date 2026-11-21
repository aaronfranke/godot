#include "display_server_windows_9x.h"

#include "platform/win_9x/encoding_windows_9x.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

void DisplayServerWindows9x::clipboard_set(const String &p_text) {
	if (!_window) {
		return;
	}
	ERR_FAIL_COND_MSG(p_text.find_char('\0') >= 0, "Clipboard text cannot contain an embedded NUL.");
	const String text = p_text.replace("\r\n", "\n").replace("\n", "\r\n");
	const Char16String wide = text.utf16();
	CharString native;
	// Win9x accepts CF_UNICODETEXT but does not synthesize ANSI/Unicode conversions.
	// Publish both to preserve Unicode between aware applications and serve native ANSI clients.
	// On modern UTF-8-ACP hosts, CF_LOCALE cannot describe that encoding: let Windows convert Unicode.
	const int format_count = GetACP() == CP_UTF8 ? 1 : 3;
	if (format_count > 1) {
		ERR_FAIL_COND_MSG(Windows9xEncoding::encode(text, native, true) != OK, "Could not encode clipboard text.");
	}
	const LCID locale = GetSystemDefaultLCID();
	const UINT formats[] = { CF_UNICODETEXT, CF_TEXT, CF_LOCALE };
	const void *sources[] = { wide.get_data(), native.get_data(), &locale };
	const SIZE_T sizes[] = { (SIZE_T(wide.length()) + 1) * sizeof(char16_t), SIZE_T(native.length()) + 1, sizeof(locale) };
	HGLOBAL data[3] = {};
	bool prepared = true;
	for (int i = 0; i < format_count; i++) {
		data[i] = GlobalAlloc(GMEM_MOVEABLE, sizes[i]);
		void *destination = data[i] ? GlobalLock(data[i]) : nullptr;
		if (!destination) {
			prepared = false;
			break;
		}
		memcpy(destination, sources[i], sizes[i]);
		GlobalUnlock(data[i]);
	}
	// Allocate every representation before clearing the previous clipboard contents.
	bool transferred = false;
	if (prepared && OpenClipboard(_window)) {
		transferred = EmptyClipboard();
		if (transferred) {
			for (int i = 0; i < format_count; i++) {
				if (SetClipboardData(formats[i], data[i])) {
					data[i] = nullptr; // The system owns successfully published handles.
				} else {
					transferred = false;
				}
			}
		}
		CloseClipboard();
	}
	for (int i = 0; i < format_count; i++) {
		if (data[i]) {
			GlobalFree(data[i]);
		}
	}
	ERR_FAIL_COND_MSG(!transferred, "Could not set clipboard text.");
}

String DisplayServerWindows9x::clipboard_get() const {
	if (!_window || !OpenClipboard(_window)) {
		return {};
	}
	String text;
	bool decoded = false;
	HGLOBAL data = GetClipboardData(CF_UNICODETEXT);
	if (data) {
		const auto *source = static_cast<const char16_t *>(GlobalLock(data));
		if (source) {
			const SIZE_T units = MIN(GlobalSize(data) / sizeof(char16_t), SIZE_T(INT_MAX));
			SIZE_T length = 0;
			while (length < units && source[length] != 0) {
				length++;
			}
			if (length < units) {
				text = String::utf16(source, int(length));
				decoded = true;
			}
			GlobalUnlock(data);
		}
	}
	if (!decoded) {
		const bool ansi = IsClipboardFormatAvailable(CF_TEXT);
		UINT code_page = ansi ? GetACP() : GetOEMCP();
		// Text can come from an application using a different locale than the system ACP.
		HGLOBAL locale_data = GetClipboardData(CF_LOCALE);
		if (locale_data && GlobalSize(locale_data) >= sizeof(LCID)) {
			const auto *source = static_cast<const LCID *>(GlobalLock(locale_data));
			if (source) {
				const LCID locale = *source;
				GlobalUnlock(locale_data);
				char name[16] = {};
				if (GetLocaleInfoA(locale, ansi ? LOCALE_IDEFAULTANSICODEPAGE : LOCALE_IDEFAULTCODEPAGE, name, sizeof(name))) {
					const UINT candidate = UINT(strtoul(name, nullptr, 10));
					if (candidate && IsValidCodePage(candidate)) {
						code_page = candidate;
					}
				}
			}
		}
		data = GetClipboardData(ansi ? CF_TEXT : CF_OEMTEXT);
		if (data) {
			const auto *source = static_cast<const char *>(GlobalLock(data));
			if (source) {
				const SIZE_T bytes = MIN(GlobalSize(data), SIZE_T(INT_MAX));
				SIZE_T length = 0;
				while (length < bytes && source[length] != '\0') {
					length++;
				}
				if (length < bytes) {
					text = Windows9xEncoding::decode(source, int(length), code_page);
				}
				GlobalUnlock(data);
			}
		}
	}
	CloseClipboard();
	return text;
}

bool DisplayServerWindows9x::clipboard_has() const {
	return IsClipboardFormatAvailable(CF_UNICODETEXT) || IsClipboardFormatAvailable(CF_TEXT) || IsClipboardFormatAvailable(CF_OEMTEXT);
}
