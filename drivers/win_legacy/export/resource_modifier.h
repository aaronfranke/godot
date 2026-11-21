#pragma once

#ifdef WINDOWS_ENABLED

#include "core/error_list.h"
#include "core/reference.h"
#include "core/ustring.h"

class EditorExportPreset;

class WindowsLegacyResourceModifier {
public:
	static Error modify(const Ref<EditorExportPreset> &p_preset, const String &p_executable, const String &p_icon, String &r_message);
};

#endif // WINDOWS_ENABLED
