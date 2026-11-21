#include "export.h"

#include "export_plugin.h"

#include "platform/win_nt4/logo.gen.h"

void register_win_nt4_exporter() {
	Ref<EditorExportPlatformWindowsNT4> platform;
	platform.instance();

	Ref<Image> img = memnew(Image(_win_nt4_logo));
	Ref<ImageTexture> logo;
	logo.instance();
	logo->create_from_image(img);
	platform->set_logo(logo);
	platform->set_name("Windows NT 4");
	platform->set_extension("exe");
	platform->set_release_32("win_nt4_release_x86_32.exe");
	platform->set_debug_32("win_nt4_debug_x86_32.exe");
	platform->set_os_name("Windows");
	platform->add_platform_feature("win_nt4");

	EditorExport::get_singleton()->add_export_platform(platform);
}
