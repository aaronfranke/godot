#include "export.h"

#include "export_plugin.h"

#include "core/object/class_db.h"
#include "editor/export/editor_export.h"

void register_win_nt5_exporter_types() {
	GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformWindowsNT5);
}

void register_win_nt5_exporter() {
	Ref<EditorExportPlatformWindowsNT5> platform;
	platform.instantiate();
	platform->set_name("Windows NT/2000/XP/Vista");
	platform->set_os_name("Windows");

	EditorExport::get_singleton()->add_export_platform(platform);
}
