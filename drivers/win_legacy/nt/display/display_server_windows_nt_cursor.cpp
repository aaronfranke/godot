#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/image.h"
#include "drivers/win_legacy/common/display/windows_legacy_icon.h"
#include "scene/resources/texture.h"

HCURSOR DisplayServerWindowsNT::_get_system_cursor(OS::CursorShape p_shape) {
	static const LPCTSTR CURSORS[OS::CURSOR_MAX] = {
		IDC_ARROW,
		IDC_IBEAM,
		IDC_HAND,
		IDC_CROSS,
		IDC_WAIT,
		IDC_APPSTARTING,
		IDC_SIZEALL,
		IDC_ARROW,
		IDC_NO,
		IDC_SIZENS,
		IDC_SIZEWE,
		IDC_SIZENESW,
		IDC_SIZENWSE,
		IDC_SIZEALL,
		IDC_SIZENS,
		IDC_SIZEWE,
		IDC_HELP,
	};
	HCURSOR cursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(CURSORS[p_shape]));
	return cursor ? cursor : LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
}

void DisplayServerWindowsNT::_destroy_custom_cursor(HCURSOR p_cursor) {
	if (p_cursor) {
		if (GetCursor() == p_cursor) {
			SetCursor(_get_system_cursor(OS::CURSOR_ARROW));
		}
		DestroyCursor(p_cursor);
	}
}

void DisplayServerWindowsNT::_apply_mouse_cursor() {
	if (_mouse_hides_cursor()) {
		SetCursor(nullptr);
		return;
	}
	const HCURSOR custom = _custom_cursors[_cursor_shape];
	SetCursor(custom ? custom : _get_system_cursor(_cursor_shape));
}

void DisplayServerWindowsNT::_update_mouse_cursor() {
	POINT position;
	if (_window && GetCursorPos(&position) && _get_client_rect().has_point(Point2i(position.x, position.y)) && WindowFromPoint(position) == _window) {
		_apply_mouse_cursor();
	}
}

void DisplayServerWindowsNT::cursor_set_shape(OS::CursorShape p_shape) {
	ERR_FAIL_INDEX(p_shape, OS::CURSOR_MAX);
	if (p_shape != _cursor_shape) {
		_cursor_shape = p_shape;
		_update_mouse_cursor();
	}
}

OS::CursorShape DisplayServerWindowsNT::cursor_get_shape() const {
	return _cursor_shape;
}

void DisplayServerWindowsNT::cursor_set_custom_image(const RES &p_cursor, OS::CursorShape p_shape, const Vector2 &p_hotspot) {
	ERR_FAIL_INDEX(p_shape, OS::CURSOR_MAX);
	HCURSOR replacement = nullptr;
	if (p_cursor.is_valid()) {
		const Ref<Texture> texture = p_cursor;
		Ref<Image> image = p_cursor;
		if (texture.is_valid()) {
			image = texture->get_data();
		}
		ERR_FAIL_COND_MSG(image.is_null() || image->empty(), "Cursor resource has no image data.");
		ERR_FAIL_COND_MSG(image->get_width() > 256 || image->get_height() > 256, "Cursor image exceeds 256×256 pixels.");
		ERR_FAIL_COND_MSG(!(p_hotspot.x >= 0 && p_hotspot.y >= 0 && p_hotspot.x < image->get_width() && p_hotspot.y < image->get_height()), "Hotspot outside cursor image.");
		if (image->is_compressed()) {
			image = image->duplicate();
			ERR_FAIL_COND_MSG(image->decompress() != OK, "Could not decompress the cursor image.");
		}
		const Point2i hotspot(p_hotspot);
		replacement = WindowsLegacyIcon::create(image, &hotspot);
		ERR_FAIL_NULL_MSG(replacement, "Could not create a native cursor.");
	}
	const HCURSOR previous = _custom_cursors[p_shape];
	_custom_cursors[p_shape] = replacement;
	if (p_shape == _cursor_shape) {
		_update_mouse_cursor();
	}
	_destroy_custom_cursor(previous);
}
