#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/io/image.h"
#include "core/io/marshalls.h"
#include "drivers/png/png_driver_common.h"

#include <string.h>

class WindowsNTClipboardBitmapChannel {
	uint32_t _mask = 0;
	uint32_t _maximum = 0;
	int _shift = 0;

public:
	bool set_mask(uint32_t p_mask) {
		_mask = p_mask;
		_maximum = p_mask;
		_shift = 0;
		if (p_mask == 0) {
			return true;
		}
		while ((_maximum & 1) == 0) {
			++_shift;
			_maximum >>= 1;
		}
		// Windows bitfields must be contiguous.
		return (_maximum & (_maximum + 1)) == 0;
	}

	uint8_t read(uint32_t p_pixel) const {
		if (_mask == 0) {
			return 255;
		}
		const uint32_t value = (p_pixel & _mask) >> _shift;
		if (_maximum == 255) {
			return uint8_t(value);
		}
		return uint8_t((uint64_t(value) * 255 + _maximum / 2) / _maximum);
	}
};

UINT DisplayServerWindowsNT::_get_clipboard_png_format() {
	static const UINT format = RegisterClipboardFormatW(L"PNG");
	return format;
}

bool DisplayServerWindowsNT::_valid_clipboard_image_size(int64_t p_width, int64_t p_height) {
	return p_width > 0 && p_height > 0 && p_width <= Image::MAX_WIDTH && p_height <= Image::MAX_HEIGHT &&
			p_width * p_height <= Image::MAX_PIXELS;
}

Ref<Image> DisplayServerWindowsNT::_decode_clipboard_png(const uint8_t *p_data, size_t p_size) {
	static const uint8_t PNG_HEADER[] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R' };
	if (p_size < sizeof(PNG_HEADER) + 13 + sizeof(uint32_t) || memcmp(p_data, PNG_HEADER, sizeof(PNG_HEADER)) != 0) {
		return Ref<Image>();
	}
	const uint32_t width = BSWAP32(decode_uint32(p_data + sizeof(PNG_HEADER)));
	const uint32_t height = BSWAP32(decode_uint32(p_data + sizeof(PNG_HEADER) + 4));
	if (!_valid_clipboard_image_size(width, height)) {
		return Ref<Image>();
	}

	Ref<Image> image;
	image.instantiate();
	if (PNGDriverCommon::png_to_image(p_data, p_size, false, image) != OK || image->is_empty()) {
		image.unref();
	}
	return image;
}

Ref<Image> DisplayServerWindowsNT::_decode_clipboard_dib(const uint8_t *p_data, size_t p_size) {
	if (p_size < sizeof(BITMAPCOREHEADER)) {
		return Ref<Image>();
	}
	const uint32_t header_size = decode_uint32(p_data);
	const bool core = header_size == sizeof(BITMAPCOREHEADER);
	if (header_size > p_size || (!core && header_size != sizeof(BITMAPINFOHEADER) && header_size != sizeof(BITMAPV4HEADER) && header_size != sizeof(BITMAPV5HEADER))) {
		return Ref<Image>();
	}

	BITMAPINFOHEADER info = {};
	if (core) {
		BITMAPCOREHEADER header;
		memcpy(&header, p_data, sizeof(header));
		info.biWidth = header.bcWidth;
		info.biHeight = header.bcHeight;
		info.biPlanes = header.bcPlanes;
		info.biBitCount = header.bcBitCount;
	} else {
		memcpy(&info, p_data, sizeof(info));
	}
	const int width = info.biWidth;
	const int64_t height = info.biHeight < 0 ? -int64_t(info.biHeight) : info.biHeight;
	if (info.biPlanes != 1 || !_valid_clipboard_image_size(width, height)) {
		return Ref<Image>();
	}

	const int depth = info.biBitCount;
	if (depth != 1 && depth != 4 && depth != 8 && depth != 16 && depth != 24 && depth != 32) {
		return Ref<Image>();
	}
	const bool bitfields = info.biCompression == BI_BITFIELDS;
	if (info.biCompression != BI_RGB && (!bitfields || (depth != 16 && depth != 32))) {
		return Ref<Image>();
	}

	uint64_t pixel_offset = header_size;
	WindowsNTClipboardBitmapChannel channels[4];
	if (bitfields) {
		const size_t masks_offset = sizeof(BITMAPINFOHEADER);
		const int mask_count = header_size == masks_offset ? 3 : 4;
		if (p_size < masks_offset + mask_count * sizeof(uint32_t)) {
			return Ref<Image>();
		}
		uint32_t assigned = 0;
		for (int i = 0; i < mask_count; ++i) {
			const uint32_t mask = decode_uint32(p_data + masks_offset + i * sizeof(uint32_t));
			if ((i < 3 && mask == 0) || (assigned & mask) != 0 || (depth == 16 && mask > 0xffff) ||
					!channels[i].set_mask(mask)) {
				return Ref<Image>();
			}
			assigned |= mask;
		}
		if (mask_count == 3) {
			pixel_offset += 3 * sizeof(uint32_t);
		}
	} else if (depth == 16) {
		channels[0].set_mask(0x7c00);
		channels[1].set_mask(0x03e0);
		channels[2].set_mask(0x001f);
	}

	const bool indexed = depth <= 8;
	const uint32_t palette_stride = core ? sizeof(RGBTRIPLE) : sizeof(RGBQUAD);
	uint32_t palette_size = info.biClrUsed;
	if (indexed) {
		const uint32_t full_palette_size = 1U << depth;
		if (palette_size > full_palette_size) {
			return Ref<Image>();
		}
		if (palette_size == 0) {
			palette_size = full_palette_size;
		}
	}
	const uint64_t palette_offset = pixel_offset;
	pixel_offset += uint64_t(palette_size) * palette_stride;
	const uint64_t row_bytes = ((uint64_t(width) * depth + 31) / 32) * 4;
	if (pixel_offset + row_bytes * height > p_size) {
		return Ref<Image>();
	}

	Vector<uint8_t> rgba;
	if (rgba.resize(int64_t(width) * height * 4) != OK) {
		return Ref<Image>();
	}
	uint8_t *destination = rgba.ptrw();
	const uint8_t *palette = p_data + size_t(palette_offset);
	const uint8_t *pixels = p_data + size_t(pixel_offset);
	for (int y = 0; y < height; ++y) {
		const int source_y = info.biHeight < 0 ? y : int(height) - 1 - y;
		const uint8_t *row = pixels + size_t(row_bytes * source_y);
		for (int x = 0; x < width; ++x, destination += 4) {
			const uint8_t *bgr;
			if (indexed) {
				const uint32_t bit = x * depth;
				const uint32_t shift = 8 - depth - bit % 8;
				const uint32_t entry = (row[bit / 8] >> shift) & ((1U << depth) - 1);
				if (entry >= palette_size) {
					return Ref<Image>();
				}
				bgr = palette + entry * palette_stride;
			} else if (depth == 16 || bitfields) {
				const uint8_t *source = row + x * (depth / 8);
				const uint32_t pixel = depth == 16 ? decode_uint16(source) : decode_uint32(source);
				for (int i = 0; i < 4; ++i) {
					destination[i] = channels[i].read(pixel);
				}
				continue;
			} else {
				bgr = row + x * (depth / 8);
			}
			destination[0] = bgr[2];
			destination[1] = bgr[1];
			destination[2] = bgr[0];
			// BI_RGB's fourth byte is reserved.
			destination[3] = 255;
		}
	}
	return Image::create_from_data(width, int(height), false, Image::FORMAT_RGBA8, rgba);
}

Ref<Image> DisplayServerWindowsNT::_read_clipboard_bitmap(HBITMAP p_bitmap) {
	BITMAP bitmap = {};
	if (!GetObjectW(p_bitmap, sizeof(bitmap), &bitmap) || !_valid_clipboard_image_size(bitmap.bmWidth, bitmap.bmHeight)) {
		return Ref<Image>();
	}

	Vector<uint8_t> rgba;
	if (rgba.resize(int64_t(bitmap.bmWidth) * bitmap.bmHeight * 4) != OK) {
		return Ref<Image>();
	}
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = bitmap.bmWidth;
	info.bmiHeader.biHeight = -bitmap.bmHeight;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	HDC dc = CreateCompatibleDC(nullptr);
	if (!dc) {
		return Ref<Image>();
	}
	uint8_t *data = rgba.ptrw();
	const int rows = GetDIBits(dc, p_bitmap, 0, bitmap.bmHeight, data, &info, DIB_RGB_COLORS);
	DeleteDC(dc);
	if (rows != bitmap.bmHeight) {
		return Ref<Image>();
	}
	for (int64_t i = 0; i < rgba.size(); i += 4) {
		SWAP(data[i], data[i + 2]);
		data[i + 3] = 255;
	}
	return Image::create_from_data(bitmap.bmWidth, bitmap.bmHeight, false, Image::FORMAT_RGBA8, rgba);
}

Ref<Image> DisplayServerWindowsNT::clipboard_get_image() const {
	if (!_window || !OpenClipboard(_window)) {
		return Ref<Image>();
	}

	Ref<Image> image;
	const UINT png = _get_clipboard_png_format();
	UINT bitmap = 0;
	// Windows lists the original formats before synthesized ones. Keep the
	// publisher's layout, and read CF_BITMAP as a device-dependent handle.
	for (UINT format = EnumClipboardFormats(0); format != 0; format = EnumClipboardFormats(format)) {
		if (format == CF_DIBV5 || format == CF_DIB || format == CF_BITMAP) {
			bitmap = format;
			break;
		}
	}
	const UINT formats[] = { png, bitmap, bitmap == CF_BITMAP ? CF_DIB : 0U };
	for (UINT format : formats) {
		if (!format || !IsClipboardFormatAvailable(format)) {
			continue;
		}
		HANDLE data = GetClipboardData(format);
		if (!data) {
			continue;
		}
		if (format == CF_BITMAP) {
			image = _read_clipboard_bitmap(static_cast<HBITMAP>(data));
		} else {
			const auto *source = static_cast<const uint8_t *>(GlobalLock(data));
			if (source) {
				const SIZE_T size = GlobalSize(data);
				image = format == png ? _decode_clipboard_png(source, size) : _decode_clipboard_dib(source, size);
				GlobalUnlock(data);
			}
		}
		if (image.is_valid()) {
			break;
		}
	}
	CloseClipboard();
	return image;
}

bool DisplayServerWindowsNT::clipboard_has_image() const {
	const UINT png = _get_clipboard_png_format();
	return (png && IsClipboardFormatAvailable(png)) ||
			IsClipboardFormatAvailable(CF_DIBV5) || IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_BITMAP);
}
