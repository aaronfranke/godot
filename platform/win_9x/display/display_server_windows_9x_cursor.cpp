#include "display_server_windows_9x.h"

#include "core/image.h"
#include "core/os/os.h"
#include "drivers/win_legacy/common/display/windows_legacy_icon.h"
#include "scene/resources/texture.h"

HCURSOR DisplayServerWindows9x::_get_system_cursor(OS::CursorShape p_shape) {
	// Standard IDC_* resource IDs; SDKs hide some names at the Windows 95 floor.
	static const int CURSORS[OS::CURSOR_MAX] = {
		32512, // ARROW.
		32513, // IBEAM.
		32649, // POINTING_HAND.
		32515, // CROSS.
		32514, // WAIT.
		32650, // BUSY.
		32646, // DRAG.
		32512, // CAN_DROP.
		32648, // FORBIDDEN.
		32645, // VSIZE.
		32644, // HSIZE.
		32643, // BDIAGSIZE.
		32642, // FDIAGSIZE.
		32646, // MOVE.
		32645, // VSPLIT.
		32644, // HSPLIT.
		32651, // HELP.
	};
	HCURSOR cursor = LoadCursorA(nullptr, MAKEINTRESOURCEA(CURSORS[p_shape]));
	return cursor ? cursor : LoadCursorA(nullptr, MAKEINTRESOURCEA(CURSORS[OS::CURSOR_ARROW]));
}

void DisplayServerWindows9x::_destroy_custom_cursor(HCURSOR p_cursor) {
	if (p_cursor) {
		if (GetCursor() == p_cursor) {
			SetCursor(_get_system_cursor(OS::CURSOR_ARROW));
		}
		DestroyCursor(p_cursor);
	}
}

void DisplayServerWindows9x::cursor_set_custom_image(const Ref<Resource> &p_cursor, OS::CursorShape p_shape, const Vector2 &p_hotspot) {
	ERR_FAIL_INDEX(p_shape, OS::CURSOR_MAX);
	HCURSOR replacement = nullptr;
	if (p_cursor.is_valid()) {
		const Ref<Texture> texture = p_cursor;
		Ref<Image> image = p_cursor;
		if (texture.is_valid()) {
			image = texture->get_data();
		}
		ERR_FAIL_COND_MSG(!(p_hotspot.x >= 0 && p_hotspot.y >= 0), "Cursor hotspot must be finite and nonnegative.");
		if (image.is_null()) {
			return;
		}
		ERR_FAIL_COND_MSG(image->empty(), "Cursor image is empty.");
		if (image->is_compressed()) {
			image = image->duplicate();
			ERR_FAIL_COND_MSG(image->decompress() != OK, "Could not decompress the cursor image.");
		}
		ERR_FAIL_COND_MSG(p_hotspot.x >= image->get_width() || p_hotspot.y >= image->get_height(), "Hotspot outside cursor image.");
		Point2i hotspot(p_hotspot);
		OSVERSIONINFOA version = {};
		version.dwOSVersionInfoSize = sizeof(version);
		typedef BOOL(WINAPI * GetVersionFunction)(LPOSVERSIONINFOA);
		const GetVersionFunction get_version = reinterpret_cast<GetVersionFunction>(reinterpret_cast<void *>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetVersionExA")));
		if (get_version && get_version(&version) && version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
			// 9x can create and draw other sizes but only selects system-sized
			// cursor bitmaps. Pad or scale a copy without modifying the source image.
			const Size2i native_size(GetSystemMetrics(SM_CXCURSOR), GetSystemMetrics(SM_CYCURSOR));
			ERR_FAIL_COND_MSG(native_size.x <= 0 || native_size.y <= 0, "Could not query the native cursor size.");
			if (Size2i(image->get_width(), image->get_height()) != native_size) {
				const Size2i source_size = Size2i(image->get_width(), image->get_height());
				image = image->duplicate();
				const real_t scale = MIN(real_t(1), MIN(real_t(native_size.x) / source_size.x, real_t(native_size.y) / source_size.y));
				if (scale < 1) {
					const Size2i scaled_size(MAX(1, int(source_size.x * scale)), MAX(1, int(source_size.y * scale)));
					image->resize(scaled_size.x, scaled_size.y, Image::INTERPOLATE_BILINEAR);
					hotspot = Point2i(p_hotspot * Vector2(scaled_size) / Vector2(source_size));
				}
				image->convert(Image::FORMAT_RGBA8);
				Ref<Image> padded;
				padded.instance();
				padded->create(native_size.x, native_size.y, false, Image::FORMAT_RGBA8);
				padded->fill(Color(0, 0, 0, 0));
				ERR_FAIL_COND_MSG(padded.is_null() || padded->empty(), "Could not allocate the native cursor image.");
				padded->blit_rect(image, Rect2i(Point2i(), Size2i(image->get_width(), image->get_height())), Point2i());
				image = padded;
			}
		}
		replacement = WindowsLegacyIcon::create(image, &hotspot);
		ERR_FAIL_NULL_MSG(replacement, "Could not create a native cursor.");
	}
	const HCURSOR previous = _custom_cursors[p_shape];
	_custom_cursors[p_shape] = replacement;
	if (p_shape == _cursor_shape) {
		_apply_mouse_cursor();
	}
	_destroy_custom_cursor(previous);
}
