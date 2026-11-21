#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/io/image.h"
#include "drivers/win_legacy/common/display/windows_legacy_icon.h"

using namespace DisplayServerEnums;

HCURSOR DisplayServerWindowsNT::_get_system_cursor(CursorShape p_shape) {
	static const LPCTSTR CURSORS[CURSOR_MAX] = {
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

HICON DisplayServerWindowsNT::_create_icon_or_cursor(const Ref<Image> &p_image, const Point2i *p_hotspot) {
	return WindowsLegacyIcon::create(p_image, p_hotspot);
}

void DisplayServerWindowsNT::_destroy_custom_cursor(HCURSOR p_cursor) {
	if (p_cursor) {
		if (GetCursor() == p_cursor) {
			SetCursor(_get_system_cursor(CURSOR_ARROW));
		}
		DestroyCursor(p_cursor);
	}
}

void DisplayServerWindowsNT::_apply_mouse_cursor() {
	if (_mouse_hides_cursor()) {
		SetCursor(nullptr);
		return;
	}
	HCURSOR custom = _custom_cursors[_cursor_shape];
	SetCursor(custom ? custom : _get_system_cursor(_cursor_shape));
}

void DisplayServerWindowsNT::_update_mouse_cursor() {
	POINT native;
	if (_window && GetCursorPos(&native) && _get_client_rect().has_point(Point2i(native.x, native.y)) && WindowFromPoint(native) == _window) {
		_apply_mouse_cursor();
	}
}

void DisplayServerWindowsNT::cursor_set_shape(CursorShape p_shape) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);
	if (p_shape != _cursor_shape) {
		_cursor_shape = p_shape;
		_update_mouse_cursor();
	}
}

CursorShape DisplayServerWindowsNT::cursor_get_shape() const {
	return _cursor_shape;
}

void DisplayServerWindowsNT::cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);
	HCURSOR replacement = nullptr;
	if (p_cursor.is_valid()) {
		ERR_FAIL_COND_MSG(!p_hotspot.is_finite(), "Cursor hotspot must be finite.");
		Ref<Image> image = _get_cursor_image_from_resource(p_cursor, p_hotspot);
		if (image.is_null()) {
			return;
		}
		ERR_FAIL_COND_MSG(image->is_empty(), "Cursor image is empty.");
		ERR_FAIL_COND_MSG(p_hotspot.x >= image->get_width() || p_hotspot.y >= image->get_height(), "Hotspot outside cursor image.");
		const Point2i hotspot(p_hotspot);
		replacement = _create_icon_or_cursor(image, &hotspot);
		ERR_FAIL_NULL_MSG(replacement, "Could not create a native cursor.");
	}
	const HCURSOR previous = _custom_cursors[p_shape];
	_custom_cursors[p_shape] = replacement;
	if (p_shape == _cursor_shape) {
		_update_mouse_cursor();
	}
	_destroy_custom_cursor(previous);
}
