#include "export_plugin.h"

#include "platform/win_nt4/run_icon.gen.h"
#include "scene/resources/texture.h"

EditorExportPlatformWindowsNT4::EditorExportPlatformWindowsNT4() {
	Ref<Image> image = memnew(Image(_win_nt4_run_icon));
	Ref<ImageTexture> texture;
	texture.instance();
	texture->create_from_image(image);
	_run_icon = texture;
}

Ref<Texture> EditorExportPlatformWindowsNT4::get_run_icon() const {
	return _run_icon;
}

void EditorExportPlatformWindowsNT4::get_export_options(List<ExportOption> *r_options) {
	EditorExportPlatformWindowsLegacy::get_export_options(r_options);
	_get_resource_export_options(r_options);
	for (List<ExportOption>::Element *E = r_options->front(); E; E = E->next()) {
		if (E->get().option.name == "binary_format/64_bits") {
			E->get().default_value = false;
			E->get().option.usage = PROPERTY_USAGE_STORAGE;
		}
	}
}

bool EditorExportPlatformWindowsNT4::get_option_visibility(const EditorExportPreset *p_preset, const String &p_option, const Map<StringName, Variant> &p_options) const {
	return _get_resource_export_option_visibility(p_preset, p_option, p_options);
}

Error EditorExportPlatformWindowsNT4::modify_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
	return _modify_resources(p_preset, p_path);
}

String EditorExportPlatformWindowsNT4::get_template_file_name(const String &p_target, const String &p_arch) const {
	return "win_nt4_" + p_target + "_" + p_arch + ".exe";
}
