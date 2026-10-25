#include "export_plugin.h"

#include "logo_svg.gen.h"
#include "run_icon_svg.gen.h"

#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/image_texture.h"

#include "modules/svg/image_loader_svg.h"

List<String> EditorExportPlatformWindows9x::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> extensions;
	extensions.push_back("exe");
	extensions.push_back("zip");
	return extensions;
}

void EditorExportPlatformWindows9x::get_export_options(List<ExportOption> *r_options) const {
	EditorExportPlatformPC::get_export_options(r_options);
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "binary_format/architecture", PROPERTY_HINT_ENUM, "x86_32"), "x86_32"));
}

String EditorExportPlatformWindows9x::get_template_file_name(const String &p_target, const String &p_arch) const {
	return "win_9x_" + p_target + "_" + p_arch + ".exe";
}

void EditorExportPlatformWindows9x::initialize() {
	if (EditorNode::get_singleton()) {
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);

		ImageLoaderSVG::create_image_from_string(img, _win_9x_logo_svg, EDSCALE, upsample, HashMap<Color, Color>());
		set_logo(ImageTexture::create_from_image(img));
	}
}
