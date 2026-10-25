#include "export.h"

#include "export_plugin.h"

#include "core/object/class_db.h"
#include "editor/export/editor_export.h"

void register_win_nt6_exporter_types() {
	GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformWindowsNT6);
}

void register_win_nt6_exporter() {
	Ref<EditorExportPlatformWindowsNT6> platform;
	platform.instantiate();
	platform->set_name("Windows 7/8/8.1/10");
	platform->set_os_name("Windows");

	EditorExport::get_singleton()->add_export_platform(platform);
}
