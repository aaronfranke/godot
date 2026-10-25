#include "export.h"

#include "export_plugin.h"

#include "core/object/class_db.h"
#include "editor/export/editor_export.h"

void register_win_9x_exporter_types() {
	GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformWindows9x);
}

void register_win_9x_exporter() {
	Ref<EditorExportPlatformWindows9x> platform;
	platform.instantiate();
	platform->set_name("Windows 95/98/98SE/ME");
	platform->set_os_name("Windows");

	EditorExport::get_singleton()->add_export_platform(platform);
}
