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
	ERR_FAIL_COND(!Thread::is_main_thread());
	const Vector<HKL> layouts = _get_keyboard_layouts();
	ERR_FAIL_INDEX(p_index, layouts.size());
	ERR_FAIL_COND_MSG(!ActivateKeyboardLayout(layouts[p_index], KLF_SETFORPROCESS), "Could not activate the keyboard layout.");
	_update_keyboard_labels();
}

String DisplayServerWindowsNT::_get_keyboard_locale_info(HKL p_layout, LCTYPE p_type) {
	WCHAR text[256];
	const int size = GetLocaleInfoW(MAKELCID(LOWORD(p_layout), SORT_DEFAULT), p_type, text, ARRAYSIZE(text));
	return size > 1 ? String::utf16(reinterpret_cast<const char16_t *>(text), size - 1) : String();
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
	return String::utf16(reinterpret_cast<const char16_t *>(text), bytes / sizeof(WCHAR));
}

String DisplayServerWindowsNT::_get_keyboard_layout_id(HKEY p_root, HKL p_layout) {
	if (p_layout == GetKeyboardLayout(0)) {
		WCHAR id[KL_NAMELENGTH];
		if (GetKeyboardLayoutNameW(id)) {
			return String::utf16(reinterpret_cast<const char16_t *>(id));
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
		if (!id.is_empty() && id.hex_to_int() == (device & 0x0fff)) {
			return String::utf16(reinterpret_cast<const char16_t *>(name), length);
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
		const Char16String id = _get_keyboard_layout_id(root, layout).utf16();
		HKEY key;
		if (!id.is_empty() && RegOpenKeyExW(root, reinterpret_cast<const WCHAR *>(id.get_data()), 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
			name = _get_keyboard_layout_value(key, L"Layout Text");
			RegCloseKey(key);
		}
		RegCloseKey(root);
	}
	return name.is_empty() ? _get_keyboard_locale_info(layout, LOCALE_SLANGUAGE) : name;
}

void DisplayServerWindowsNT::_update_keyboard_labels() {
	const HKL layout = GetKeyboardLayout(0);
	if (layout == _keyboard_label_layout) {
		return;
	}
	_keyboard_label_layout = layout;
	for (Key &key : _keyboard_letter_labels) {
		key = Key::NONE;
	}
	// MapVirtualKeyExW always reports Latin letters for A-Z. Reverse the
	// unmodified layout mappings without consuming the pending dead-key state.
	const UINT code_page = _get_keyboard_locale_info(layout, LOCALE_IDEFAULTANSICODEPAGE).to_int();
	for (unsigned int unit = 32; unit <= 255; unit++) {
		WCHAR character = 0;
		const char encoded = char(unit);
		if (unit < 128) {
			character = unit;
		} else if (!code_page || MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, &encoded, 1, &character, 1) != 1) {
			continue;
		}
		const SHORT mapping = VkKeyScanExW(character, layout);
		if (mapping >= 'A' && mapping <= 'Z') {
			_keyboard_letter_labels[mapping - 'A'] = fix_key_label(character, Key(mapping));
		}
	}
}

Key DisplayServerWindowsNT::keyboard_get_keycode_from_physical(Key p_keycode) const {
	const Key key = Key(p_keycode & KeyModifierMask::CODE_MASK);
	const Key modifiers = p_keycode & KeyModifierMask::MODIFIER_MASK;
	if (key < Key::SPACE || key > Key::ASCIITILDE || (key >= Key::KEY_0 && key <= Key::KEY_9)) {
		return p_keycode;
	}
	const unsigned int scan = KeyMappingWindows::get_scancode(key);
	const HKL layout = GetKeyboardLayout(0);
	const UINT vk = MapVirtualKeyExW(scan, MAPVK_VSC_TO_VK, layout);
	const Key logical = KeyMappingWindows::get_keysym(vk);
	return (logical == Key::NONE ? key : logical) | modifiers;
}

Key DisplayServerWindowsNT::keyboard_get_label_from_physical(Key p_keycode) const {
	const Key key = Key(p_keycode & KeyModifierMask::CODE_MASK);
	const Key modifiers = p_keycode & KeyModifierMask::MODIFIER_MASK;
	if (key < Key::SPACE || key > Key::ASCIITILDE) {
		return p_keycode;
	}
	const unsigned int scan = KeyMappingWindows::get_scancode(key);
	const HKL layout = GetKeyboardLayout(0);
	const UINT vk = MapVirtualKeyExW(scan, MAPVK_VSC_TO_VK, layout);
	if (Thread::is_main_thread() && layout == _keyboard_label_layout && vk >= 'A' && vk <= 'Z') {
		const Key label = _keyboard_letter_labels[vk - 'A'];
		if (label != Key::NONE) {
			return label | modifiers;
		}
	}
	const char32_t character = MapVirtualKeyExW(vk, MAPVK_VK_TO_CHAR, layout) & 0xffff;
	return fix_key_label(character, key) | modifiers;
}
