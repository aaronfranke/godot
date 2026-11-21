#include "display_server_windows_9x.h"
#include "encoding_windows_9x.h"

#include <limits.h>
#include <shellapi.h>
#include <shlobj.h>

using namespace DisplayServerEnums;

void DisplayServerWindows9x::window_set_drop_files_callback(const Callable &p_callable, WindowID p_window) {
	if (p_window == MAIN_WINDOW_ID) {
		_drop_files_callback = p_callable;
		if (_shown) {
			DragAcceptFiles(_window, _drop_files_callback.is_valid());
		}
	}
}

void DisplayServerWindows9x::_handle_file_drop(WPARAM p_drop) {
	const HDROP drop = reinterpret_cast<HDROP>(p_drop);
	if (!drop) {
		return;
	}
	const Callable callback = _drop_files_callback;
	Vector<String> paths;
	if (!_drop_input && callback.is_valid()) {
		const SIZE_T bytes = GlobalSize(drop);
		const auto *data = static_cast<const DROPFILES *>(GlobalLock(drop));
		if (data && bytes >= sizeof(DROPFILES) && data->fWide) {
			// 98's DragQueryFileA treats a Unicode payload as separate one-byte
			// names. Read its bounded UTF-16 list directly, preserving CPU Unicode.
			if (data->pFiles >= sizeof(DROPFILES) && data->pFiles <= bytes) {
				const size_t units = (bytes - data->pFiles) / sizeof(char16_t);
				// pFiles is a byte offset; copying also handles an unaligned list.
				Vector<char16_t> names;
				if (units > 0 && names.resize(units) == OK) {
					memcpy(names.ptrw(), reinterpret_cast<const uint8_t *>(data) + data->pFiles, units * sizeof(char16_t));
					size_t start = 0;
					bool terminated = false;
					for (size_t i = 0; i < units; ++i) {
						if (names[i] != 0) {
							continue;
						}
						if (i == start) {
							terminated = true;
							break;
						}
						if (i - start >= INT_MAX) {
							break;
						}
						paths.push_back(String::utf16(names.ptr() + start, int(i - start)));
						start = i + 1;
					}
					if (!terminated) {
						paths.clear();
					}
				}
			}
		} else {
			const UINT count = DragQueryFileA(drop, 0xffffffff, nullptr, 0);
			CharString path;
			for (UINT i = 0; i < count; ++i) {
				const UINT length = DragQueryFileA(drop, i, nullptr, 0);
				if (length == 0 || length >= INT_MAX || path.resize_uninitialized(int64_t(length) + 1) != OK) {
					continue;
				}
				if (DragQueryFileA(drop, i, path.ptrw(), length + 1) == length) {
					paths.push_back(Windows9xEncoding::decode(path.get_data(), int(length)));
				}
			}
		}
		if (data) {
			GlobalUnlock(drop);
		}
	}
	// Queued drops still belong to us after the callback is removed. Release
	// their Shell storage before invoking user code, which may change the callback.
	DragFinish(drop);
	if (!paths.is_empty()) {
		callback.call(paths);
	}
}
