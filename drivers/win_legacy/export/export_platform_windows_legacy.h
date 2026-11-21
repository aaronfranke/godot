#pragma once

#include "editor/export/editor_export_platform_pc.h"

// Host-side packaging shared by the legacy Windows targets. Keep target names,
// template selection and version-specific options in each platform's exporter.
class EditorExportPlatformWindowsLegacy : public EditorExportPlatformPC {
	GDSOFTCLASS(EditorExportPlatformWindowsLegacy, EditorExportPlatformPC);

protected:
	void _get_resource_export_options(List<ExportOption> *r_options) const;
	bool _get_resource_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const;
	Error _modify_resources(const Ref<EditorExportPreset> &p_preset, const String &p_path);

public:
	Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0, bool p_notify = true) override;
};
