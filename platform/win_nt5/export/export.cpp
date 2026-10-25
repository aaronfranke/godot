#include "export.h"

#include "export_plugin.h"

#include "platform/win_nt5/logo.gen.h"

void register_win_nt5_exporter() {
	Ref<EditorExportPlatformWindowsNT5> platform;
	platform.instance();

	Ref<Image> img = memnew(Image(_win_nt5_logo));
	Ref<ImageTexture> logo;
	logo.instance();
	logo->create_from_image(img);
	platform->set_logo(logo);
	platform->set_name("Windows NT/2000/XP/Vista");
	platform->set_extension("exe");
	platform->set_release_32("win_nt5_release_x86_32.exe");
	platform->set_debug_32("win_nt5_debug_x86_32.exe");
	platform->set_release_64("win_nt5_release_x86_64.exe");
	platform->set_debug_64("win_nt5_debug_x86_64.exe");
	platform->set_os_name("Windows");

	EditorExport::get_singleton()->add_export_platform(platform);
}
