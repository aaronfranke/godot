#pragma once

#include "drivers/win_legacy/export/export_platform_windows_legacy.h"

class EditorExportPlatformWindows9x : public EditorExportPlatformWindowsLegacy {
	GDCLASS(EditorExportPlatformWindows9x, EditorExportPlatformWindowsLegacy);

protected:
	virtual bool _is_32bit_only() const { return true; }

public:
	virtual String get_template_file_name(const String &p_target, const String &p_arch) const;
	virtual void get_export_options(List<ExportOption> *r_options);
};
