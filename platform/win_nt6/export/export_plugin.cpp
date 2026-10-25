#include "export_plugin.h"

void EditorExportPlatformWindowsNT6::get_export_options(List<ExportOption> *r_options) {
	EditorExportPlatformPC::get_export_options(r_options);
	for (List<ExportOption>::Element *E = r_options->front(); E; E = E->next()) {
		if (E->get().option.name == "binary_format/64_bits") {
			E->get().default_value = true;
		}
	}
}
