#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// System includes.
#include <limits.h>
#include <string.h>

void DisplayServerWindowsNT::clipboard_set(const String &p_text) {
	if (!_window) {
		return;
	}

	// The Windows text formats use CRLF. Preserve existing CRLF pairs.
	const String text = p_text.replace("\r\n", "\n").replace("\n", "\r\n");
	const SIZE_T bytes = (SIZE_T(text.length()) + 1) * sizeof(WCHAR);
	HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
	ERR_FAIL_NULL_MSG(data, "Could not allocate clipboard text.");

	void *destination = GlobalLock(data);
	if (!destination) {
		GlobalFree(data);
		ERR_FAIL_MSG("Could not lock clipboard text.");
	}
	memcpy(destination, text.c_str(), bytes);
	GlobalUnlock(data);

	if (!OpenClipboard(_window)) {
		GlobalFree(data);
		ERR_FAIL_MSG("Could not open the clipboard.");
	}
	const bool transferred = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data);
	CloseClipboard();
	if (!transferred) {
		GlobalFree(data);
		ERR_FAIL_MSG("Could not set clipboard text.");
	}
}

String DisplayServerWindowsNT::clipboard_get() const {
	if (!_window || !OpenClipboard(_window)) {
		return String();
	}

	String text;
	// Windows synthesizes Unicode text from CF_TEXT and CF_OEMTEXT.
	HGLOBAL data = GetClipboardData(CF_UNICODETEXT);
	if (data) {
		const WCHAR *source = static_cast<const WCHAR *>(GlobalLock(data));
		if (source) {
			const SIZE_T units = MIN(GlobalSize(data) / sizeof(WCHAR), SIZE_T(INT_MAX - 1));
			text = String(source, int(units));
			GlobalUnlock(data);
		}
	}
	CloseClipboard();
	return text;
}
