#include "drivers/win_legacy/nt/display/display_server_windows_nt.h"

// Godot includes.
#include "core/os/thread.h"
#include "platform/windows/key_mapping_windows.h"

Vector<HKL> DisplayServerWindowsNT::_get_keyboard_layouts() {
	Vector<HKL> layouts;
	const int count = GetKeyboardLayoutList(0, nullptr);
	if (count > 0 && layouts.resize(count) == OK) {
		layouts.resize(GetKeyboardLayoutList(count, layouts.ptrw()));
	}
	return layouts;
}

int DisplayServerWindowsNT::keyboard_get_layout_count() const {
	return GetKeyboardLayoutList(0, nullptr);
}

int DisplayServerWindowsNT::keyboard_get_current_layout() const {
	return _get_keyboard_layouts().find(GetKeyboardLayout(0));
}

void DisplayServerWindowsNT::keyboard_set_current_layout(int p_index) {
	ERR_FAIL_COND(Thread::get_caller_id() != Thread::get_main_id());
	const Vector<HKL> layouts = _get_keyboard_layouts();
	ERR_FAIL_INDEX(p_index, layouts.size());
	ERR_FAIL_COND_MSG(!ActivateKeyboardLayout(layouts[p_index], KLF_SETFORPROCESS), "Could not activate the keyboard layout.");
}

String DisplayServerWindowsNT::_get_keyboard_locale_info(HKL p_layout, LCTYPE p_type) {
	WCHAR text[256];
	const int size = GetLocaleInfoW(MAKELCID(LOWORD(p_layout), SORT_DEFAULT), p_type, text, ARRAYSIZE(text));
	return size > 1 ? String(text, size - 1) : String();
}

String DisplayServerWindowsNT::keyboard_get_layout_language(int p_index) const {
	const Vector<HKL> layouts = _get_keyboard_layouts();
	ERR_FAIL_INDEX_V(p_index, layouts.size(), String());
	return _get_keyboard_locale_info(layouts[p_index], LOCALE_SISO639LANGNAME);
}

String DisplayServerWindowsNT::_get_keyboard_layout_value(HKEY p_layout, const WCHAR *p_name) {
	WCHAR text[256] = {};
	DWORD bytes = sizeof(text) - sizeof(WCHAR);
	DWORD type = 0;
	if (RegQueryValueExW(p_layout, p_name, nullptr, &type, reinterpret_cast<BYTE *>(text), &bytes) != ERROR_SUCCESS || type != REG_SZ) {
		return String();
	}
	return String(text, bytes / sizeof(WCHAR));
}

String DisplayServerWindowsNT::_get_keyboard_layout_id(HKEY p_root, HKL p_layout) {
	if (p_layout == GetKeyboardLayout(0)) {
		WCHAR id[KL_NAMELENGTH];
		if (GetKeyboardLayoutNameW(id)) {
			return String(id);
		}
	}
	const WORD device = HIWORD(p_layout);
	if ((device & 0xf000) != 0xf000) {
		return String::num_uint64(device ? device : LOWORD(p_layout), 16).lpad(8, "0");
	}

	// Variant handles encode the registry Layout Id in the low 12 bits of the device word.
	for (DWORD i = 0;; i++) {
		WCHAR name[256];
		DWORD length = ARRAYSIZE(name);
		if (RegEnumKeyExW(p_root, i, name, &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
			break;
		}
		HKEY key;
		if (RegOpenKeyExW(p_root, name, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
			continue;
		}
		const String id = _get_keyboard_layout_value(key, L"Layout Id");
		RegCloseKey(key);
		if (!id.empty() && id.hex_to_int(false) == (device & 0x0fff)) {
			return String(name, length);
		}
	}
	return String();
}

String DisplayServerWindowsNT::keyboard_get_layout_name(int p_index) const {
	const Vector<HKL> layouts = _get_keyboard_layouts();
	ERR_FAIL_INDEX_V(p_index, layouts.size(), String());
	const HKL layout = layouts[p_index];
	HKEY root;
	String name;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts", 0, KEY_READ, &root) == ERROR_SUCCESS) {
		const String id = _get_keyboard_layout_id(root, layout);
		HKEY key;
		if (!id.empty() && RegOpenKeyExW(root, id.c_str(), 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
			name = _get_keyboard_layout_value(key, L"Layout Text");
			RegCloseKey(key);
		}
		RegCloseKey(root);
	}
	return name.empty() ? _get_keyboard_locale_info(layout, LOCALE_SLANGUAGE) : name;
}

OS::LatinKeyboardVariant DisplayServerWindowsNT::get_latin_keyboard_variant() const {
	WCHAR id[KL_NAMELENGTH];
	if (!GetKeyboardLayoutNameW(id)) {
		return OS::LATIN_KEYBOARD_QWERTY;
	}

	// Use the same layout IDs as the Godot 3.x Windows backend.
	switch (String(id).hex_to_int(false)) {
		case 0x00020401: // Arabic (102) AZERTY
		case 0x0001080c: // Belgian (Comma)
		case 0x0000080c: // Belgian French
		case 0x0000040c: // French
			return OS::LATIN_KEYBOARD_AZERTY;
		case 0x0000041a: // Croatian
		case 0x00000405: // Czech
		case 0x00000407: // German
		case 0x00010407: // German (IBM)
		case 0x0000040e: // Hungarian
		case 0x0000046e: // Luxembourgish
		case 0x00010415: // Polish (214)
		case 0x00000418: // Romanian (Legacy)
		case 0x0000081a: // Serbian (Latin)
		case 0x0000041b: // Slovak
		case 0x00000424: // Slovenian
		case 0x0001042e: // Sorbian Extended
		case 0x0002042e: // Sorbian Standard
		case 0x0000042e: // Sorbian Standard (Legacy)
		case 0x0000100c: // Swiss French
		case 0x00000807: // Swiss German
			return OS::LATIN_KEYBOARD_QWERTZ;
		case 0x00010409: // US-Dvorak
		case 0x00030409: // US-Dvorak for left hand
		case 0x00040409: // US-Dvorak for right hand
			return OS::LATIN_KEYBOARD_DVORAK;
		default:
			return OS::LATIN_KEYBOARD_QWERTY;
	}
}

uint32_t DisplayServerWindowsNT::keyboard_get_scancode_from_physical(uint32_t p_scancode) const {
	const uint32_t key = p_scancode & KEY_CODE_MASK;
	// Keep number-row shortcuts, keypad keys, and other special keys independent of the layout.
	if (key < KEY_SPACE || key > KEY_ASCIITILDE || (key >= KEY_0 && key <= KEY_9)) {
		return p_scancode;
	}

	const unsigned int scan = KeyMappingWindows::get_scancode(key);
	const UINT vk = MapVirtualKeyExW(scan, MAPVK_VSC_TO_VK, GetKeyboardLayout(0));
	// Use the same virtual-key mapping as the delivered InputEventKey.
	const uint32_t logical = KeyMappingWindows::get_keysym(vk);
	return logical == KEY_UNKNOWN ? p_scancode : logical | (p_scancode & KEY_MODIFIER_MASK);
}
