#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/os/thread.h"
#include "scene/resources/texture.h"
#include "servers/display/native_menu.h"

#include <shellapi.h>

using namespace DisplayServerEnums;

HICON DisplayServerWindowsNT::_make_status_indicator_icon(const Ref<Texture2D> &p_texture) {
	if (p_texture.is_null()) {
		return CopyIcon(LoadIconW(nullptr, MAKEINTRESOURCEW(32512)));
	}
	const Ref<Image> image = p_texture->get_image();
	if (image.is_null() || image->is_empty()) {
		return nullptr;
	}
	return _create_image_icon(image, Size2i(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)));
}

bool DisplayServerWindowsNT::_write_status_indicator(IndicatorID p_id, const StatusIndicatorData &p_indicator, DWORD p_command) const {
	NOTIFYICONDATAW data = {};
	data.cbSize = sizeof(data);
	data.hWnd = _window;
	data.uID = p_id;
	if (p_command == NIM_ADD || p_command == NIM_MODIFY) {
		data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
		data.uCallbackMessage = WM_STATUS_INDICATOR;
		data.hIcon = p_indicator.icon;
		const Char16String tip = p_indicator.tooltip.utf16();
		int length = MIN(tip.length(), int(ARRAYSIZE(data.szTip) - 1));
		if (length > 0 && tip[length - 1] >= 0xd800 && tip[length - 1] <= 0xdbff) {
			length--;
		}
		memcpy(data.szTip, tip.get_data(), length * sizeof(WCHAR));
	}
	if (!Shell_NotifyIconW(p_command, &data)) {
		return false;
	}
	if (p_command == NIM_ADD) {
		data.uVersion = NOTIFYICON_VERSION;
		Shell_NotifyIconW(NIM_SETVERSION, &data);
	}
	return true;
}

IndicatorID DisplayServerWindowsNT::create_status_indicator(const Ref<Texture2D> &p_icon, const String &p_tooltip, const Callable &p_callback) {
	ERR_FAIL_COND_V(!Thread::is_main_thread() || !_window || _next_status_indicator > INT32_MAX, INVALID_INDICATOR_ID);
	StatusIndicatorData indicator;
	indicator.icon = _make_status_indicator_icon(p_icon);
	ERR_FAIL_NULL_V(indicator.icon, INVALID_INDICATOR_ID);
	indicator.tooltip = p_tooltip;
	indicator.callback = p_callback;
	const IndicatorID id = _next_status_indicator++;
	if (!_write_status_indicator(id, indicator, NIM_ADD)) {
		DestroyIcon(indicator.icon);
		WARN_PRINT("The Windows notification area could not register the status indicator.");
		return INVALID_INDICATOR_ID;
	}
	_status_indicators.insert(id, indicator);
	return id;
}

void DisplayServerWindowsNT::status_indicator_set_icon(IndicatorID p_id, const Ref<Texture2D> &p_icon) {
	StatusIndicatorData *indicator = _status_indicators.getptr(p_id);
	ERR_FAIL_NULL(indicator);
	const HICON icon = _make_status_indicator_icon(p_icon);
	ERR_FAIL_NULL(icon);
	const HICON previous = indicator->icon;
	indicator->icon = icon;
	_write_status_indicator(p_id, *indicator, NIM_MODIFY);
	DestroyIcon(previous);
}

void DisplayServerWindowsNT::status_indicator_set_tooltip(IndicatorID p_id, const String &p_tooltip) {
	StatusIndicatorData *indicator = _status_indicators.getptr(p_id);
	ERR_FAIL_NULL(indicator);
	indicator->tooltip = p_tooltip;
	_write_status_indicator(p_id, *indicator, NIM_MODIFY);
}

void DisplayServerWindowsNT::status_indicator_set_menu(IndicatorID p_id, const RID &p_menu_rid) {
	if (StatusIndicatorData *indicator = _status_indicators.getptr(p_id)) {
		indicator->menu = p_menu_rid;
	}
}

void DisplayServerWindowsNT::status_indicator_set_callback(IndicatorID p_id, const Callable &p_callback) {
	StatusIndicatorData *indicator = _status_indicators.getptr(p_id);
	ERR_FAIL_NULL(indicator);
	indicator->callback = p_callback;
}

Rect2i DisplayServerWindowsNT::_get_native_status_indicator_rect(IndicatorID p_id) const {
	RECT rect = {};
	const NOTIFYICONIDENTIFIER identifier = { sizeof(NOTIFYICONIDENTIFIER), _window, UINT(p_id), {} };
	if (Shell_NotifyIconGetRect(&identifier, &rect) != S_OK) {
		return Rect2i();
	}
	return Rect2i(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

Rect2 DisplayServerWindowsNT::status_indicator_get_rect(IndicatorID p_id) const {
	ERR_FAIL_COND_V(!_status_indicators.has(p_id), Rect2());
	const Rect2i rect = _get_native_status_indicator_rect(p_id);
	if (!rect.has_area()) {
		return Rect2();
	}
	return Rect2(rect.position - _get_screens_origin(), rect.size);
}

void DisplayServerWindowsNT::_dispatch_status_indicator(IndicatorID p_id, LPARAM p_event) {
	const StatusIndicatorData *indicator = _status_indicators.getptr(p_id);
	if (!indicator || _drop_input) {
		return;
	}
	MouseButton button;
	switch (p_event) {
		case WM_LBUTTONDOWN:
		case NIN_KEYSELECT:
		case NIN_SELECT:
			button = MouseButton::LEFT;
			break;
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			button = MouseButton::RIGHT;
			break;
		case WM_MBUTTONDOWN:
			button = MouseButton::MIDDLE;
			break;
		case WM_XBUTTONDOWN:
			button = MouseButton::MB_XBUTTON1;
			break;
		default:
			return;
	}
	const RID menu = indicator->menu;
	if (button == MouseButton::RIGHT && _native_menu && _native_menu->has_menu(menu)) {
		const Rect2i rect = _get_native_status_indicator_rect(p_id);
		const Point2i anchor = rect.has_area() ? rect.get_center() : mouse_get_position() + _get_screens_origin();
		_native_menu->popup(menu, anchor);
		if (const StatusIndicatorData *remaining = _status_indicators.getptr(p_id)) {
			_write_status_indicator(p_id, *remaining, NIM_SETFOCUS);
		}
		return;
	}
	const Callable callback = indicator->callback;
	if (callback.is_valid()) {
		callback.call(button, mouse_get_position());
	}
}

void DisplayServerWindowsNT::delete_status_indicator(IndicatorID p_id) {
	const StatusIndicatorData *indicator = _status_indicators.getptr(p_id);
	ERR_FAIL_NULL(indicator);
	const HICON icon = indicator->icon;
	_write_status_indicator(p_id, *indicator, NIM_DELETE);
	_status_indicators.erase(p_id);
	DestroyIcon(icon);
}

void DisplayServerWindowsNT::_restore_status_indicators() {
	for (const KeyValue<IndicatorID, StatusIndicatorData> &entry : _status_indicators) {
		if (!_write_status_indicator(entry.key, entry.value, NIM_ADD)) {
			_write_status_indicator(entry.key, entry.value, NIM_MODIFY);
		}
	}
}

void DisplayServerWindowsNT::_clear_status_indicators() {
	const HashMap<IndicatorID, StatusIndicatorData> indicators = std::move(_status_indicators);
	for (const KeyValue<IndicatorID, StatusIndicatorData> &entry : indicators) {
		_write_status_indicator(entry.key, entry.value, NIM_DELETE);
		DestroyIcon(entry.value.icon);
	}
}
