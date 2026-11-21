#include "export_platform_windows_legacy.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/zip_io.h"
#include "drivers/win_legacy/export/resource_modifier.h"

Error EditorExportPlatformWindowsLegacy::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, bool p_notify) {
	if (p_path.get_extension().nocasecmp_to("zip") != 0) {
		return EditorExportPlatformPC::export_project(p_preset, p_debug, p_path, p_flags, p_notify);
	}

	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags, p_notify);
	Error error = OK;
	Ref<DirAccess> temporary = DirAccess::create_temp("godot-legacy-windows-export", false, &error);
	if (temporary.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("ZIP Creation"), TTR("Could not create a temporary directory for the export."));
		return error;
	}

	const String staging = temporary->get_current_dir();
	error = temporary->make_dir("payload");
	if (error != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("ZIP Creation"), TTR("Could not create the export staging directory."));
		return error;
	}
	const String payload = staging.path_join("payload");
	const String executable = payload.path_join(p_path.get_file().get_basename() + ".exe");
	error = EditorExportPlatformPC::export_project(p_preset, p_debug, executable, p_flags, false);
	if (error != OK) {
		return error;
	}

	const String archive_path = staging.path_join("export.zip");
	Ref<FileAccess> archive;
	zlib_filefunc_def io = zipio_create_io(&archive);
	zipFile zip = zipOpen2(archive_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io);
	if (!zip) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("ZIP Creation"), TTR("Could not create the export archive."));
		return ERR_CANT_CREATE;
	}
	const int first_zip_message = get_message_count();
	zip_folder_recursive(zip, payload, String(), String());
	if (zipClose(zip, nullptr) != ZIP_OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("ZIP Creation"), TTR("Could not write the export archive."));
		return ERR_FILE_CANT_WRITE;
	}
	for (int i = first_zip_message; i < get_message_count(); ++i) {
		if (get_message(i).msg_type >= EXPORT_MESSAGE_ERROR) {
			return ERR_FILE_CANT_WRITE;
		}
	}

	Ref<DirAccess> output = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	const String output_dir = p_path.get_base_dir();
	if (!output_dir.is_empty()) {
		error = output->make_dir_recursive(output_dir);
	}
	if (error == OK) {
		error = output->copy(archive_path, p_path);
	}
	if (error != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("ZIP Creation"), vformat(TTR("Could not write the export archive to \"%s\"."), p_path));
	}
	return error;
}

void EditorExportPlatformWindowsLegacy::_get_resource_export_options(List<ExportOption> *r_options) const {
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "application/modify_resources"), false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/icon", PROPERTY_HINT_FILE, "*.ico,*.png,*.webp,*.svg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/file_version", PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use project version"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/product_version", PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use project version"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/company_name"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/product_name"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/file_description"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/copyright"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/trademarks"), ""));
}

bool EditorExportPlatformWindowsLegacy::_get_resource_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	if (p_preset && p_option.begins_with("application/") && p_option != "application/modify_resources") {
		return p_preset->get("application/modify_resources");
	}
	return EditorExportPlatformPC::get_export_option_visibility(p_preset, p_option);
}

Error EditorExportPlatformWindowsLegacy::_modify_resources(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
	if (!p_preset->get("application/modify_resources")) {
		return OK;
	}
	// This tests the editor host: resource editing uses Win32 APIs. Other hosts
	// can still export Windows templates without this optional modification.
#ifdef WINDOWS_ENABLED
	String icon = p_preset->get("application/icon");
	if (icon.is_empty()) {
		icon = get_project_setting(p_preset, "application/config/windows_native_icon");
	}
	if (icon.is_empty()) {
		icon = get_project_setting(p_preset, "application/config/icon");
	}
	const ProjectSettings *settings = ProjectSettings::get_singleton();
	String executable = settings->globalize_path(p_path);
	if (executable.is_relative_path()) {
		executable = DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->get_current_dir().path_join(executable);
	}
	String message;
	const Error error = WindowsLegacyResourceModifier::modify(p_preset, executable, settings->globalize_path(icon), message);
	if (error != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Resources Modification"), message);
	}
	return error;
#else
	add_message(EXPORT_MESSAGE_ERROR, TTR("Resources Modification"), TTR("Modifying legacy Windows executable resources requires a Windows editor host."));
	return ERR_UNAVAILABLE;
#endif
}
