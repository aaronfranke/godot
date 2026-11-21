#pragma once

#include "core/os/process_id.h"
#include "drivers/win_legacy/export/export_platform_windows_legacy.h"

class EditorExportPlatformWindowsNT4 : public EditorExportPlatformWindowsLegacy {
	GDCLASS(EditorExportPlatformWindowsNT4, EditorExportPlatformWindowsLegacy);

	Ref<Texture2D> _run_icon;

protected:
	Error modify_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) override;

public:
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;
	bool get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const override;
	bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	void get_platform_features(List<String> *r_features) const override;

	virtual String get_template_file_name(const String &p_target, const String &p_arch) const override;
	Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, bool p_notify) override;

	virtual void initialize() override;
	Ref<Texture2D> get_run_icon() const override;
};
