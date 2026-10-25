#include "export.h"

#include "export_plugin.h"

#include "platform/win_9x/logo.gen.h"

void register_win_9x_exporter() {
	Ref<EditorExportPlatformWindows9x> platform;
	platform.instance();

	Ref<Image> img = memnew(Image(_win_9x_logo));
	Ref<ImageTexture> logo;
	logo.instance();
	logo->create_from_image(img);
	platform->set_logo(logo);
	platform->set_name("Windows 95/98/98SE/ME");
	platform->set_extension("exe");
	platform->set_release_32("win_9x_release_x86_32.exe");
	platform->set_debug_32("win_9x_debug_x86_32.exe");
	platform->set_os_name("Windows");

	EditorExport::get_singleton()->add_export_platform(platform);
}
