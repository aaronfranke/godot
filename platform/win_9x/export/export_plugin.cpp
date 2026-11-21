#include "export_plugin.h"

void EditorExportPlatformWindows9x::get_export_options(List<ExportOption> *r_options) {
	EditorExportPlatformWindowsLegacy::get_export_options(r_options);
	for (List<ExportOption>::Element *E = r_options->front(); E; E = E->next()) {
		if (E->get().option.name == "binary_format/64_bits") {
			E->get().default_value = false;
			E->get().option.usage = PROPERTY_USAGE_STORAGE;
		}
	}
}

String EditorExportPlatformWindows9x::get_template_file_name(const String &p_target, const String &p_arch) const {
	return "win_9x_" + p_target + "_" + p_arch + ".exe";
}
