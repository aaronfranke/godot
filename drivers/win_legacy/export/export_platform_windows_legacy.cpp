#include "export_platform_windows_legacy.h"

#include "resource_modifier.h"

#include "core/io/zip_io.h"
#include "core/os/dir_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"

// WINDOWS_ENABLED describes the editor host. Native PE resource editing needs
// Win32 APIs; editors on other hosts can still package unmodified templates.
#ifdef WINDOWS_ENABLED
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // WINDOWS_ENABLED

void EditorExportPlatformWindowsLegacy::get_export_options(List<ExportOption> *r_options) {
	EditorExportPlatformPC::get_export_options(r_options);

	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "debug/export_console_wrapper", PROPERTY_HINT_ENUM, "No,Debug Only,Debug and Release"), 1));
}

void EditorExportPlatformWindowsLegacy::_get_resource_export_options(List<ExportOption> *r_options) const {
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "application/modify_resources"), false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/icon", PROPERTY_HINT_FILE, "*.ico,*.png,*.webp,*.svg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/file_version", PROPERTY_HINT_PLACEHOLDER_TEXT, "1.0.0.0"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/product_version", PROPERTY_HINT_PLACEHOLDER_TEXT, "1.0.0.0"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/company_name"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/product_name"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/file_description"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/copyright"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/trademarks"), ""));
}

bool EditorExportPlatformWindowsLegacy::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const {
	const bool valid = EditorExportPlatformPC::has_valid_export_configuration(p_preset, r_error, r_missing_templates);
	if (_is_32bit_only() && bool(p_preset->get("binary_format/64_bits"))) {
		r_error += vformat(TTR("%s requires an x86_32 export template."), get_name()) + "\n";
		return false;
	}
	return valid;
}

Error EditorExportPlatformWindowsLegacy::prepare_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
	Error error = EditorExportPlatformPC::prepare_template(p_preset, p_debug, p_path, p_flags);
	if (error != OK) {
		return error;
	}
	const int mode = p_preset->get("debug/export_console_wrapper");
	if (mode != 2 && (mode != 1 || !p_debug)) {
		return OK;
	}
	String source = String(p_preset->get(p_debug ? "custom_template/debug" : "custom_template/release")).strip_edges();
	if (source.empty()) {
		const String architecture = bool(p_preset->get("binary_format/64_bits")) ? "x86_64" : "x86_32";
		source = find_export_template(get_template_file_name(p_debug ? "debug" : "release", architecture));
	}
	// Match the wrapper suffixes accepted by Godot 4's PC exporter. Custom
	// headless templates can omit a wrapper because they already have a console.
	const char *const suffixes[] = { ".console.exe", "_console.exe", " console.exe", "console.exe" };
	for (unsigned int i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
		const String wrapper = source.get_basename() + suffixes[i];
		if (!FileAccess::exists(wrapper)) {
			continue;
		}
		DirAccessRef dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		error = dir->copy(wrapper, p_path.get_basename() + ".console.exe", get_chmod_flags());
		if (error != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Template"), TTR("Failed to copy the console wrapper."));
		}
		return error;
	}
	return OK;
}

bool EditorExportPlatformWindowsLegacy::_get_resource_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option, const Map<StringName, Variant> &p_options) const {
	if (p_preset && p_option.begins_with("application/") && p_option != "application/modify_resources") {
		return p_preset->get("application/modify_resources");
	}
	return EditorExportPlatformPC::get_option_visibility(p_preset, p_option, p_options);
}

List<String> EditorExportPlatformWindowsLegacy::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> extensions = EditorExportPlatformPC::get_binary_extensions(p_preset);
	extensions.push_back("zip");
	return extensions;
}

Error EditorExportPlatformWindowsLegacy::_modify_resources(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
	if (!p_preset->get("application/modify_resources")) {
		return OK;
	}
#ifdef WINDOWS_ENABLED
	String icon = p_preset->get("application/icon");
	if (icon.empty()) {
		icon = GLOBAL_GET("application/config/windows_native_icon");
	}
	if (icon.empty()) {
		icon = GLOBAL_GET("application/config/icon");
	}

	const ProjectSettings *settings = ProjectSettings::get_singleton();
	String executable = settings->globalize_path(p_path);
	if (executable.is_rel_path()) {
		DirAccessRef dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		executable = dir->get_current_dir().plus_file(executable);
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
#endif // WINDOWS_ENABLED
}

Error EditorExportPlatformWindowsLegacy::_get_export_files(const String &p_root, const String &p_relative, Vector<String> &r_files) {
	DirAccessRef dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	Error error = dir->change_dir(p_root.plus_file(p_relative));
	if (error != OK) {
		return error;
	}
	error = dir->list_dir_begin();
	if (error != OK) {
		return error;
	}
	for (String name = dir->get_next(); !name.empty() && error == OK; name = dir->get_next()) {
		if (name == "." || name == "..") {
			continue;
		}
		const String relative = p_relative.empty() ? name : p_relative.plus_file(name);
		if (dir->current_is_dir()) {
			error = _get_export_files(p_root, relative, r_files);
		} else {
			r_files.push_back(relative);
		}
	}
	dir->list_dir_end();
	return error;
}

Error EditorExportPlatformWindowsLegacy::_write_zip(const String &p_payload, const String &p_archive) {
	Vector<String> files;
	Error error = _get_export_files(p_payload, String(), files);
	if (error != OK) {
		return error;
	}
	FileAccess *target = nullptr;
	zlib_filefunc64_def io = {};
	io.opaque = &target;
	io.zopen64_file = [](voidpf p_context, const void *p_path, int p_mode) {
		return zipio_open(p_context, static_cast<const char *>(p_path), p_mode);
	};
	io.zread_file = zipio_read;
	io.zwrite_file = [](voidpf p_context, voidpf p_stream, const void *p_data, uLong p_size) -> uLong {
		FileAccess *file = *static_cast<FileAccess **>(p_context);
		const uint64_t start = file->get_position();
		zipio_write(p_context, p_stream, p_data, p_size);
		return file->get_position() == start + p_size ? p_size : 0;
	};
	io.ztell64_file = [](voidpf p_context, voidpf) -> ZPOS64_T {
		return (*static_cast<FileAccess **>(p_context))->get_position();
	};
	io.zseek64_file = [](voidpf p_context, voidpf, ZPOS64_T p_offset, int p_origin) -> long {
		FileAccess *file = *static_cast<FileAccess **>(p_context);
		if (p_origin == ZLIB_FILEFUNC_SEEK_CUR) {
			p_offset += file->get_position();
		} else if (p_origin == ZLIB_FILEFUNC_SEEK_END) {
			p_offset += file->get_len();
		}
		file->seek(p_offset);
		return file->get_position() == p_offset ? 0 : -1;
	};
	io.zclose_file = zipio_close;
	io.zerror_file = zipio_testerror;
	io.alloc_mem = zipio_alloc;
	io.free_mem = zipio_free;
	zipFile archive = zipOpen2_64(p_archive.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io);
	if (!archive) {
		return ERR_CANT_CREATE;
	}

	uint8_t buffer[65536];
	for (int i = 0; i < files.size() && error == OK; ++i) {
		const String &name = files[i];
		FileAccessRef source = FileAccess::open(p_payload.plus_file(name), FileAccess::READ);
		if (!source) {
			error = ERR_CANT_OPEN;
			break;
		}
		uint64_t remaining = source->get_len();
		const unsigned int utf8_name_flag = 1 << 11;
		if (zipOpenNewFileInZip4_64(archive, name.utf8().get_data(), nullptr, nullptr, 0, nullptr, 0, nullptr,
					Z_DEFLATED, Z_DEFAULT_COMPRESSION, 0, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, nullptr,
					0, 0, utf8_name_flag, remaining >= UINT32_MAX) != ZIP_OK) {
			error = ERR_FILE_CANT_WRITE;
			break;
		}
		while (remaining > 0) {
			const unsigned int size = MIN(remaining, uint64_t(sizeof(buffer)));
			if (source->get_buffer(buffer, size) != size) {
				error = ERR_FILE_CANT_READ;
				break;
			}
			if (zipWriteInFileInZip(archive, buffer, size) != ZIP_OK) {
				error = ERR_FILE_CANT_WRITE;
				break;
			}
			remaining -= size;
		}
		if (zipCloseFileInZip(archive) != ZIP_OK && error == OK) {
			error = ERR_FILE_CANT_WRITE;
		}
	}
	if (zipClose(archive, nullptr) != ZIP_OK && error == OK) {
		error = ERR_FILE_CANT_WRITE;
	}
	return error;
}

Error EditorExportPlatformWindowsLegacy::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int p_flags) {
	if (_is_32bit_only() && bool(p_preset->get("binary_format/64_bits"))) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Architecture"), vformat(TTR("%s requires an x86_32 export template."), get_name()));
		return ERR_INVALID_PARAMETER;
	}
	if (p_path.get_extension().nocasecmp_to("zip") != 0) {
		return EditorExportPlatformPC::export_project(p_preset, p_debug, p_path, p_flags);
	}

	DirAccessRef output = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	const String destination = p_path.is_abs_path() ? p_path : output->get_current_dir().plus_file(p_path);
	if (!output->dir_exists(destination.get_base_dir())) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("ZIP Creation"), TTR("The given export path doesn't exist."));
		return ERR_FILE_BAD_PATH;
	}
	// Stage alongside the archive for native replacement. A leading dot keeps the project export scan out.
	OS *os = OS::get_singleton();
	const String staging = destination.get_base_dir().plus_file(vformat(".godot-legacy-windows-export-%d-%d", os->get_process_id(), os->get_ticks_usec()));
	Error error = output->make_dir(staging);
	if (error != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("ZIP Creation"), TTR("Could not create the export staging directory."));
		return error;
	}

	const String payload = staging.plus_file("payload");
	const String archive = staging.plus_file("export.zip");
	error = output->make_dir(payload);
	if (error == OK) {
		ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);
		const String executable = payload.plus_file(p_path.get_file().get_basename() + ".exe");
		error = prepare_template(p_preset, p_debug, executable, p_flags);
		if (error == OK) {
			error = modify_template(p_preset, p_debug, executable, p_flags);
		}
		if (error == OK) {
			error = export_project_data(p_preset, p_debug, executable, p_flags);
		}
		if (error == OK) {
			error = _write_zip(payload, archive);
		}
	}
	if (error == OK) {
#if defined(WINDOWS_ENABLED) && !defined(WIN_9X_ENABLED)
		if (!MoveFileExW(archive.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING)) {
			error = ERR_FILE_CANT_WRITE;
		}
#else
		// Win9x uses its ANSI backup/rollback replacement; MoveFileExW is a stub.
		error = output->rename(archive, destination);
#endif // defined(WINDOWS_ENABLED) && !defined(WIN_9X_ENABLED)
	}
	if (error != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("ZIP Creation"), vformat(TTR("Could not write the export archive to \"%s\"."), p_path));
	}
	if (output->change_dir(staging) == OK) {
		output->erase_contents_recursive();
		output->change_dir("..");
		output->remove(staging);
	}
	return error;
}
