#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/marshalls.h"

#include <string.h>

using namespace DisplayServerEnums;

void DisplayServerWindowsNT::_replace_window_icons(HICON p_small, HICON p_large) {
	if (_window) {
		SendMessageW(_window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(p_small));
		SendMessageW(_window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(p_large));
	}
	if (_small_icon) {
		DestroyIcon(_small_icon);
	}
	if (_large_icon) {
		DestroyIcon(_large_icon);
	}
	_small_icon = p_small;
	_large_icon = p_large;
}

HICON DisplayServerWindowsNT::_create_image_icon(const Ref<Image> &p_image, const Size2i &p_size) {
	Ref<Image> image = p_image;
	if (image->is_compressed() || image->get_size() != p_size) {
		image = image->duplicate();
		if (image->is_compressed() && image->decompress() != OK) {
			return nullptr;
		}
		image->resize(p_size.width, p_size.height, Image::INTERPOLATE_BILINEAR);
	}
	return _create_icon_or_cursor(image);
}

void DisplayServerWindowsNT::set_icon(const Ref<Image> &p_icon) {
	window_set_icon(p_icon, MAIN_WINDOW_ID);
}

void DisplayServerWindowsNT::window_set_icon(const Ref<Image> &p_icon, WindowID p_window) {
	if (!_window || p_window != MAIN_WINDOW_ID) {
		return;
	}
	if (p_icon.is_null()) {
		_replace_window_icons(nullptr, nullptr);
		return;
	}
	ERR_FAIL_COND_MSG(p_icon->is_empty(), "The window icon image is empty.");
	HICON small = _create_image_icon(p_icon, Size2i(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)));
	ERR_FAIL_NULL_MSG(small, "Could not create the small window icon.");
	HICON large = _create_image_icon(p_icon, Size2i(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON)));
	if (!large) {
		DestroyIcon(small);
		ERR_FAIL_MSG("Could not create the large window icon.");
	}
	_replace_window_icons(small, large);
}

HICON DisplayServerWindowsNT::_create_ico_frame(Vector<uint8_t> &p_bytes, const Size2i &p_source_size, const Size2i &p_target_size) {
	const uint8_t *data = p_bytes.ptr();
	const uint64_t length = p_bytes.size();
	const uint8_t png_signature[] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
	if (length >= 24 && memcmp(data, png_signature, sizeof(png_signature)) == 0) {
		// Pre-Vista Windows cannot load PNG icon frames through Win32.
		if (BSWAP32(decode_uint32(data + 16)) != uint32_t(p_source_size.width) ||
				BSWAP32(decode_uint32(data + 20)) != uint32_t(p_source_size.height)) {
			return nullptr;
		}
		Ref<Image> image;
		image.instantiate();
		return image->load_png_from_buffer(p_bytes) == OK ? _create_image_icon(image, p_target_size) : nullptr;
	}

	if (length < sizeof(BITMAPCOREHEADER)) {
		return nullptr;
	}
	const uint32_t header_size = decode_uint32(data);
	const bool core_header = header_size == sizeof(BITMAPCOREHEADER);
	if (!core_header && (header_size < sizeof(BITMAPINFOHEADER) || header_size > length)) {
		return nullptr;
	}
	const uint32_t width = core_header ? decode_uint16(data + 4) : decode_uint32(data + 4);
	const uint32_t height = core_header ? decode_uint16(data + 6) : decode_uint32(data + 8);
	const uint16_t planes = decode_uint16(data + (core_header ? 8 : 12));
	const uint16_t bits = decode_uint16(data + (core_header ? 10 : 14));
	if (width != uint32_t(p_source_size.width) || height != uint32_t(p_source_size.height * 2) || planes != 1 ||
			(bits != 1 && bits != 4 && bits != 8 && bits != 16 && bits != 24 && bits != 32)) {
		return nullptr;
	}
	const uint32_t compression = core_header ? BI_RGB : decode_uint32(data + 16);
	if (compression != BI_RGB && (compression != BI_BITFIELDS || (bits != 16 && bits != 32))) {
		return nullptr;
	}
	const uint32_t palette_size = core_header ? 0 : decode_uint32(data + 32);
	const uint32_t default_palette_size = bits <= 8 ? 1U << bits : 0;
	const uint32_t mask_bytes = compression == BI_BITFIELDS && header_size == sizeof(BITMAPINFOHEADER) ? 3 * sizeof(DWORD) : 0;
	const uint32_t color_stride = ((width * bits + 31) / 32) * 4;
	const uint32_t mask_stride = ((width + 31) / 32) * 4;
	const uint64_t required = uint64_t(header_size) + mask_bytes + uint64_t(palette_size ? palette_size : default_palette_size) * (core_header ? 3 : 4) +
			(color_stride + mask_stride) * p_source_size.height;
	if (required > length) {
		return nullptr;
	}
	return CreateIconFromResourceEx(p_bytes.ptrw(), p_bytes.size(), TRUE, 0x00030000, p_target_size.width, p_target_size.height, LR_DEFAULTCOLOR);
}

void DisplayServerWindowsNT::set_native_icon(const String &p_filename) {
	if (!_window) {
		return;
	}
	enum {
		ICO_HEADER_SIZE = 6,
		ICO_ENTRY_SIZE = 16,
	};
	Ref<FileAccess> file = FileAccess::open(p_filename, FileAccess::READ);
	ERR_FAIL_COND_MSG(file.is_null(), "Could not open window icon '" + p_filename + "'.");
	const uint64_t length = file->get_length();
	ERR_FAIL_COND_MSG(length < ICO_HEADER_SIZE || file->get_16() != 0 || file->get_16() != 1, "Invalid Windows icon file '" + p_filename + "'.");
	const uint16_t count = file->get_16();
	const int directory_size = count * ICO_ENTRY_SIZE;
	const uint64_t first_image = ICO_HEADER_SIZE + directory_size;
	ERR_FAIL_COND_MSG(count == 0 || length < first_image, "Invalid Windows icon directory in '" + p_filename + "'.");
	const Vector<uint8_t> directory = file->get_buffer(directory_size);
	ERR_FAIL_COND_MSG(directory.size() != directory_size, "Could not read the icon directory in '" + p_filename + "'.");

	const Size2i sizes[] = {
		Size2i(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)),
		Size2i(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON)),
	};
	HICON icons[2] = {};
	for (unsigned int i = 0; i < ARRAYSIZE(sizes); i++) {
		int best = INT_MAX;
		for (unsigned int j = 0; j < count; j++) {
			const uint8_t *entry = directory.ptr() + j * ICO_ENTRY_SIZE;
			const Size2i dimensions(entry[0] ? entry[0] : 256, entry[1] ? entry[1] : 256);
			const uint32_t byte_count = decode_uint32(entry + 8);
			const uint32_t offset = decode_uint32(entry + 12);
			if (offset < first_image || uint64_t(offset) + byte_count > length || byte_count > INT_MAX) {
				continue;
			}
			// Prefer the closest native size, using higher color depth to break ties.
			const Size2i difference = (dimensions - sizes[i]).abs();
			const int score = (difference.x + difference.y) * 64 + 32 - MIN(32, decode_uint16(entry + 6));
			if (score >= best) {
				continue;
			}
			file->seek(offset);
			Vector<uint8_t> bytes = file->get_buffer(byte_count);
			if (bytes.size() != int(byte_count)) {
				continue;
			}
			const HICON icon = _create_ico_frame(bytes, dimensions, sizes[i]);
			if (icon) {
				if (icons[i]) {
					DestroyIcon(icons[i]);
				}
				icons[i] = icon;
				best = score;
			}
		}
	}
	if (!icons[0] || !icons[1]) {
		for (HICON icon : icons) {
			if (icon) {
				DestroyIcon(icon);
			}
		}
		ERR_FAIL_MSG("Could not load the window icon from '" + p_filename + "'.");
	}
	_replace_window_icons(icons[0], icons[1]);
}
