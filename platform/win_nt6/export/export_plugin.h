#pragma once

#include "drivers/win_legacy/export/export_platform_windows_legacy.h"

class EditorExportPlatformWindowsNT6 : public EditorExportPlatformWindowsLegacy {
	GDCLASS(EditorExportPlatformWindowsNT6, EditorExportPlatformWindowsLegacy);

public:
	virtual String get_template_file_name(const String &p_target, const String &p_arch) const;
	virtual void get_export_options(List<ExportOption> *r_options);
	virtual bool get_option_visibility(const EditorExportPreset *p_preset, const String &p_option, const Map<StringName, Variant> &p_options) const;
	virtual Error modify_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags);
};
