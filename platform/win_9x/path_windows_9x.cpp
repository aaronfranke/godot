#include "path_windows_9x.h"

#include "encoding_windows_9x.h"

#include "core/variant/variant.h"

#include <windows.h>

String PathWindows9x::get_absolute(const String &p_path) {
	if (p_path.is_empty() || p_path.find_char('\0') >= 0) {
		return {};
	}
	const String path = p_path.replace_char('/', '\\');
	if (path.begins_with(R"(\\?\)") || path.begins_with(R"(\\.\)")) {
		return {};
	}
	const CharString native = to_native(path);
	if (native.is_empty()) {
		return {};
	}
	char absolute[MAX_PATH];
	const DWORD length = GetFullPathNameA(native.get_data(), MAX_PATH, absolute, nullptr);
	if (length == 0 || length >= MAX_PATH) {
		return {};
	}
	return Windows9xEncoding::decode(absolute, int(length)).replace_char('\\', '/');
}

CharString PathWindows9x::to_native(const String &p_path) {
	CharString native;
	if (Windows9xEncoding::encode(p_path.replace_char('/', '\\'), native) != OK || native.length() >= MAX_PATH) {
		return {};
	}
	return native;
}

bool PathWindows9x::is_reserved_filename(const String &p_path) {
	const String name = p_path.get_file().get_slicec('.', 0).strip_edges().to_upper();
	if (name == "CON" || name == "PRN" || name == "AUX" || name == "NUL") {
		return true;
	}
	if (name.length() != 4 || (!name.begins_with("COM") && !name.begins_with("LPT"))) {
		return false;
	}
	return name[3] >= '1' && name[3] <= '9';
}

bool PathWindows9x::is_equivalent(const String &p_path_a, const String &p_path_b) {
	const CharString a = to_native(get_absolute(p_path_a));
	const CharString b = to_native(get_absolute(p_path_b));
	if (a.is_empty() || b.is_empty()) {
		return false;
	}
	char short_a[MAX_PATH];
	char short_b[MAX_PATH];
	const DWORD length_a = GetShortPathNameA(a.get_data(), short_a, MAX_PATH);
	const DWORD length_b = GetShortPathNameA(b.get_data(), short_b, MAX_PATH);
	// FAT's long and 8.3 names can refer to the same file. Do not move that file
	// aside as its own replacement backup, even when the two spellings differ.
	if (length_a && length_a < MAX_PATH && length_b && length_b < MAX_PATH) {
		return lstrcmpiA(short_a, short_b) == 0;
	}
	return lstrcmpiA(a.get_data(), b.get_data()) == 0;
}

String PathWindows9x::get_temporary_path(const String &p_destination, const String &p_kind, uint32_t p_sequence) {
	String candidate = p_destination + vformat(".godot-%s-%d-%d.tmp", p_kind, GetCurrentProcessId(), p_sequence);
	if (!to_native(candidate).is_empty()) {
		return candidate;
	}
	// Use the available sibling-name space even when the original path nearly reaches MAX_PATH.
	const String directory = p_destination.get_base_dir();
	const CharString native_directory = to_native(directory);
	if (native_directory.is_empty()) {
		return {};
	}
	const int available = MAX_PATH - 2 - int(native_directory.length());
	if (available <= 0) {
		return {};
	}
	const String suffix = String::num_uint64(p_sequence, 16);
	String name;
	int digits = available;
	if (available > 1) {
		name = "~";
		--digits;
	}
	name += suffix.right(MIN(int(suffix.length()), digits));
	candidate = directory.path_join(name);
	return to_native(candidate).is_empty() ? String() : candidate;
}

Error PathWindows9x::replace_file(const String &p_source, const String &p_destination) {
	const CharString source = to_native(p_source);
	const CharString destination = to_native(p_destination);
	if (source.is_empty() || destination.is_empty()) {
		return ERR_FILE_BAD_PATH;
	}
	// Win9x has no atomic file replacement. Serialize Godot's short replacement transactions
	// across processes so their backup and rollback steps cannot interleave.
	HANDLE mutex = CreateMutexA(nullptr, FALSE, "GodotWindows9xFileReplacement");
	if (!mutex) {
		return ERR_CANT_CREATE;
	}
	const DWORD waited = WaitForSingleObject(mutex, INFINITE);
	if (waited != WAIT_OBJECT_0 && waited != WAIT_ABANDONED) {
		CloseHandle(mutex);
		return ERR_CANT_CREATE;
	}
	Error error = OK;
	const DWORD source_attributes = GetFileAttributesA(source.get_data());
	const DWORD attributes = GetFileAttributesA(destination.get_data());
	String backup;
	if (source_attributes == INVALID_FILE_ATTRIBUTES) {
		error = ERR_FILE_NOT_FOUND;
	} else if (source_attributes & FILE_ATTRIBUTE_DIRECTORY) {
		error = ERR_INVALID_PARAMETER;
	} else if (is_equivalent(p_source, p_destination)) {
		// Preserve native case-only rename behavior without a replacement transaction.
		const bool renamed = MoveFileA(source.get_data(), destination.get_data());
		const DWORD native_error = GetLastError();
		ReleaseMutex(mutex);
		CloseHandle(mutex);
		return renamed || native_error == ERROR_ALREADY_EXISTS || native_error == ERROR_FILE_EXISTS ? OK : ERR_FILE_CANT_WRITE;
	} else if (attributes != INVALID_FILE_ATTRIBUTES) {
		if (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY)) {
			error = ERR_FILE_CANT_WRITE;
		} else {
			static uint32_t sequence = 0;
			bool moved = false;
			for (int attempt = 0; attempt < TEMPORARY_ATTEMPTS; ++attempt) {
				const String candidate = get_temporary_path(p_destination, "backup", ++sequence);
				if (candidate.is_empty()) {
					error = ERR_FILE_BAD_PATH;
					break;
				}
				if (MoveFileA(destination.get_data(), to_native(candidate).get_data())) {
					backup = candidate;
					moved = true;
					break;
				}
				const DWORD native_error = GetLastError();
				if (native_error != ERROR_ALREADY_EXISTS && native_error != ERROR_FILE_EXISTS) {
					error = ERR_FILE_CANT_WRITE;
					break;
				}
			}
			// Near MAX_PATH, the sibling basename may have only a few distinct choices.
			if (!moved && error == OK) {
				error = ERR_CANT_CREATE;
			}
		}
	}
	if (error == OK && !MoveFileA(source.get_data(), destination.get_data())) {
		error = ERR_FILE_CANT_WRITE;
		if (!backup.is_empty() && !MoveFileA(to_native(backup).get_data(), destination.get_data())) {
			ERR_PRINT(vformat("Cannot restore '%s'. Its previous contents remain at '%s'.", p_destination, backup));
		}
	} else if (error == OK && !backup.is_empty()) {
		// A stale backup can be removed later if another application prevents cleanup.
		DeleteFileA(to_native(backup).get_data());
	}
	ReleaseMutex(mutex);
	CloseHandle(mutex);
	return error;
}
