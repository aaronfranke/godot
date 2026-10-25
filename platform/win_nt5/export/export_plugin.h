#pragma once

#include "core/os/process_id.h"
#include "editor/export/editor_export_platform_pc.h"

class EditorExportPlatformWindowsNT5 : public EditorExportPlatformPC {
	GDCLASS(EditorExportPlatformWindowsNT5, EditorExportPlatformPC);

public:
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;

	virtual String get_template_file_name(const String &p_target, const String &p_arch) const override;

	virtual void initialize() override;
};
