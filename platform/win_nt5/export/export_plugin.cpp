#include "export_plugin.h"

void EditorExportPlatformWindowsNT5::get_export_options(List<ExportOption> *r_options) {
	EditorExportPlatformWindowsLegacy::get_export_options(r_options);
	_get_resource_export_options(r_options);
	for (List<ExportOption>::Element *E = r_options->front(); E; E = E->next()) {
		if (E->get().option.name == "binary_format/64_bits") {
			E->get().default_value = false;
		}
	}
}

bool EditorExportPlatformWindowsNT5::get_option_visibility(const EditorExportPreset *p_preset, const String &p_option, const Map<StringName, Variant> &p_options) const {
	return _get_resource_export_option_visibility(p_preset, p_option, p_options);
}

Error EditorExportPlatformWindowsNT5::modify_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
	return _modify_resources(p_preset, p_path);
}

String EditorExportPlatformWindowsNT5::get_template_file_name(const String &p_target, const String &p_arch) const {
	return "win_nt5_" + p_target + "_" + p_arch + ".exe";
}
