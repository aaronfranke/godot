#pragma once

#include "editor/editor_export.h"

class EditorExportPlatformWindowsLegacy : public EditorExportPlatformPC {
	GDCLASS(EditorExportPlatformWindowsLegacy, EditorExportPlatformPC);
	// Use 3.x's virtual declaration style: GDCLASS omits override specifiers,
	// and mixing styles triggers Clang's inconsistent-missing-override warning.

	static Error _get_export_files(const String &p_root, const String &p_relative, Vector<String> &r_files);
	static Error _write_zip(const String &p_payload, const String &p_archive);

protected:
	virtual bool _is_32bit_only() const { return false; }
	void _get_resource_export_options(List<ExportOption> *r_options) const;
	bool _get_resource_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option, const Map<StringName, Variant> &p_options) const;
	Error _modify_resources(const Ref<EditorExportPreset> &p_preset, const String &p_path);

public:
	virtual void get_export_options(List<ExportOption> *r_options);
	virtual String get_template_file_name(const String &p_target, const String &p_arch) const = 0;
	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const;
	virtual Error prepare_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags);
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags = 0);
};
