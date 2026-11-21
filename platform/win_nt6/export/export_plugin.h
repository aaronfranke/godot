#pragma once

#include "core/os/process_id.h"
#include "drivers/win_legacy/export/export_platform_windows_legacy.h"

class EditorExportPlatformWindowsNT6 : public EditorExportPlatformWindowsLegacy {
	GDCLASS(EditorExportPlatformWindowsNT6, EditorExportPlatformWindowsLegacy);

public:
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;
	void get_platform_features(List<String> *r_features) const override;

	virtual String get_template_file_name(const String &p_target, const String &p_arch) const override;

	virtual void initialize() override;
};
