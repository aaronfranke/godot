#include "export_plugin.h"

#include "logo_svg.gen.h"
#include "run_icon_svg.gen.h"

#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/image_texture.h"

#include "modules/svg/image_loader_svg.h"

List<String> EditorExportPlatformWindowsNT5::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> extensions;
	extensions.push_back("exe");
	extensions.push_back("zip");
	return extensions;
}

void EditorExportPlatformWindowsNT5::get_export_options(List<ExportOption> *r_options) const {
	EditorExportPlatformPC::get_export_options(r_options);
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "binary_format/architecture", PROPERTY_HINT_ENUM, "x86_32,x86_64"), "x86_32"));
	_get_resource_export_options(r_options);
}

bool EditorExportPlatformWindowsNT5::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	return _get_resource_export_option_visibility(p_preset, p_option);
}

void EditorExportPlatformWindowsNT5::get_platform_features(List<String> *r_features) const {
	EditorExportPlatformPC::get_platform_features(r_features);
	r_features->push_back("windows");
	r_features->push_back("win_nt5");
}

String EditorExportPlatformWindowsNT5::get_template_file_name(const String &p_target, const String &p_arch) const {
	return "win_nt5_" + p_target + "_" + p_arch + ".exe";
}

Error EditorExportPlatformWindowsNT5::modify_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	return _modify_resources(p_preset, p_path);
}

Error EditorExportPlatformWindowsNT5::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, bool p_notify) {
	return EditorExportPlatformWindowsLegacy::export_project(p_preset, p_debug, p_path, p_flags, p_notify);
}

void EditorExportPlatformWindowsNT5::initialize() {
	if (EditorNode::get_singleton()) {
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);

		ImageLoaderSVG::create_image_from_string(img, _win_nt5_logo_svg, EDSCALE, upsample, HashMap<Color, Color>());
		set_logo(ImageTexture::create_from_image(img));
	}
}
