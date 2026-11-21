#include "file_stdio_windows_9x.h"

#include "encoding_windows_9x.h"
#include "path_windows_9x.h"

#include <cerrno>

FILE *windows9x_fopen_utf8(const char *p_filename, const char *p_mode) {
	if (!p_filename || !p_mode) {
		errno = EINVAL;
		return nullptr;
	}
	String filename;
	String mode;
	if (filename.append_utf8(p_filename) != OK || mode.append_utf8(p_mode) != OK || PathWindows9x::is_reserved_filename(filename)) {
		errno = EINVAL;
		return nullptr;
	}
	const CharString native_filename = PathWindows9x::to_native(filename);
	CharString native_mode;
	if (native_filename.is_empty() || Windows9xEncoding::encode(mode, native_mode) != OK || native_mode.is_empty()) {
		errno = EILSEQ;
		return nullptr;
	}
#ifdef _MSC_VER
	FILE *file = nullptr;
	fopen_s(&file, native_filename.get_data(), native_mode.get_data());
	return file;
#else
	return fopen(native_filename.get_data(), native_mode.get_data());
#endif // _MSC_VER
}
