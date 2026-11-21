#include "windows_legacy_icon.h"

#include "core/io/image.h"
#include "core/os/os.h"

HICON WindowsLegacyIcon::create(const Ref<Image> &p_image, const Point2i *p_hotspot) {
	const Size2i size = p_image->get_size();
	BITMAPINFO color_info = {};
	color_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	color_info.bmiHeader.biWidth = size.width;
	color_info.bmiHeader.biHeight = -size.height;
	color_info.bmiHeader.biPlanes = 1;
	color_info.bmiHeader.biBitCount = 32;

	struct {
		BITMAPINFOHEADER header;
		RGBQUAD palette[2];
	} mask_info = { color_info.bmiHeader, { { 0, 0, 0, 0 }, { 255, 255, 255, 0 } } };
	mask_info.header.biBitCount = 1;

	void *color_data = nullptr;
	HBITMAP color = CreateDIBSection(nullptr, &color_info, DIB_RGB_COLORS, &color_data, nullptr, 0);
	if (!color) {
		return nullptr;
	}
	void *mask_data = nullptr;
	HBITMAP mask = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO *>(&mask_info), DIB_RGB_COLORS, &mask_data, nullptr, 0);
	if (!mask) {
		DeleteObject(color);
		return nullptr;
	}

	// Windows 2000 uses the AND mask without alpha blending. Preserve the
	// visible RGB values and threshold transparency on that system.
	const String version = OS::get_singleton()->get_version();
	const bool blend_alpha = !version.is_empty() && !version.begins_with("5.0.");
	const uint32_t min_alpha = blend_alpha ? 1 : 128;
	const int mask_pitch = ((size.width + 31) / 32) * 4;
	auto *pixels = static_cast<uint32_t *>(color_data);
	auto *mask_bits = static_cast<uint8_t *>(mask_data);
	ZeroMemory(mask_bits, mask_pitch * size.height);
	for (int y = 0; y < size.height; y++) {
		for (int x = 0; x < size.width; x++) {
			const uint32_t pixel = p_image->get_pixel(x, y).to_argb32();
			if ((pixel >> 24) < min_alpha) {
				mask_bits[y * mask_pitch + x / 8] |= 0x80 >> (x % 8);
				pixels[y * size.width + x] = 0;
			} else {
				pixels[y * size.width + x] = blend_alpha ? pixel : (pixel | 0xff000000);
			}
		}
	}

	ICONINFO info = { p_hotspot == nullptr, p_hotspot ? DWORD(p_hotspot->x) : 0, p_hotspot ? DWORD(p_hotspot->y) : 0, mask, color };
	HICON icon = CreateIconIndirect(&info);
	DeleteObject(mask);
	DeleteObject(color);
	return icon;
}
