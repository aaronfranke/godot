#include "drivers/win_legacy/nt/os/os_windows_nt.h"

// Godot includes.
#include "modules/modules_enabled.gen.h"

#ifdef MODULE_FREETYPE_ENABLED

#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "core/templates/hash_set.h"
#include "drivers/win_legacy/nt/path_windows_nt.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_TRUETYPE_TABLES_H

#include <limits.h>
#include <shlobj.h>

// Registry-backed font files and FreeType work without DirectWrite. Unregistered
// private fonts and bitmap-only fonts are outside this catalog.
class OS_WindowsNT::SystemFontCatalog {
	struct FontStream {
		FileAccess *file;
		unsigned long start = 0;
		unsigned long count = 0;
		uint8_t bytes[4096];

		unsigned long read_at(unsigned long p_offset, unsigned char *p_bytes, unsigned long p_count) {
			file->seek(p_offset);
			if (file->get_error() != OK) {
				return 0;
			}
			const uint64_t read = file->get_buffer(p_bytes, p_count);
			return read <= p_count ? static_cast<unsigned long>(read) : 0;
		}
	};

	struct GlyphRange {
		uint32_t first;
		uint32_t last;
	};

	struct InstalledFace {
		String path;
		Vector<String> families;
		Vector<String> languages;
		Vector<GlyphRange> coverage;
		int weight = 400;
		int stretch = 100;
		bool italic = false;

		bool covers(char32_t p_char) const {
			int first = 0;
			int last = coverage.size();
			while (first < last) {
				const int middle = first + (last - first) / 2;
				const GlyphRange &range = coverage[middle];
				if (p_char < range.first) {
					last = middle;
				} else if (p_char > range.last) {
					first = middle + 1;
				} else {
					return true;
				}
			}
			return false;
		}

		bool matches_family(const String &p_family) const {
			for (const String &family : families) {
				if (family.nocasecmp_to(p_family) == 0) {
					return true;
				}
			}
			return false;
		}

		int style_distance(int p_weight, int p_stretch, bool p_italic) const {
			return Math::abs(weight - p_weight) + 5 * Math::abs(stretch - p_stretch) + (italic != p_italic ? 1000 : 0);
		}
	};

	struct Match {
		const InstalledFace *face;
		int64_t prefix;
		int style_distance;
		bool family;
		bool language;

		bool operator<(const Match &p_other) const {
			if (prefix != p_other.prefix) {
				return prefix > p_other.prefix;
			}
			if (family != p_other.family) {
				return family;
			}
			if (language != p_other.language) {
				return language;
			}
			if (style_distance != p_other.style_distance) {
				return style_distance < p_other.style_distance;
			}
			return face->path < p_other.face->path;
		}
	};

	Vector<InstalledFace> _faces;
	Vector<String> _families;

	static String _resolve_family(const String &p_family) {
		const String family = p_family.to_lower();
		if (family == "sans-serif") {
			return "Arial";
		}
		if (family == "serif") {
			return "Times New Roman";
		}
		if (family == "monospace") {
			return "Courier New";
		}
		if (family == "cursive") {
			return "Comic Sans MS";
		}
		if (family == "fantasy") {
			return "Impact";
		}
		return p_family;
	}

	static unsigned long _read_font(FT_Stream p_stream, unsigned long p_offset, unsigned char *p_buffer, unsigned long p_count) {
		if (p_offset > p_stream->size) {
			return p_count == 0 ? 1 : 0;
		}
		if (p_count == 0) {
			return 0;
		}
		auto &stream = *static_cast<FontStream *>(p_stream->descriptor.pointer);
		const unsigned long remaining = p_stream->size - p_offset;
		const unsigned long request = MIN(p_count, remaining);
		const unsigned long cache_size = sizeof(stream.bytes);
		if (request >= cache_size) {
			return stream.read_at(p_offset, p_buffer, request);
		}
		// FreeType often reads adjacent SFNT fields only a few bytes at a time.
		unsigned long cached_offset = p_offset - stream.start;
		if (p_offset < stream.start || cached_offset > stream.count || request > stream.count - cached_offset) {
			stream.start = p_offset;
			stream.count = stream.read_at(p_offset, stream.bytes, MIN(cache_size, remaining));
			cached_offset = 0;
		}
		const unsigned long count = MIN(request, stream.count - cached_offset);
		memcpy(p_buffer, stream.bytes + cached_offset, count);
		return count;
	}

	static void _read_names(FT_Face p_face, InstalledFace &r_face) {
		for (FT_UInt i = 0; i < FT_Get_Sfnt_Name_Count(p_face); i++) {
			FT_SfntName record;
			if (FT_Get_Sfnt_Name(p_face, i, &record) ||
					(record.name_id != TT_NAME_ID_FONT_FAMILY && record.name_id != TT_NAME_ID_TYPOGRAPHIC_FAMILY) ||
					(record.platform_id != TT_PLATFORM_MICROSOFT && record.platform_id != TT_PLATFORM_APPLE_UNICODE) ||
					record.string_len < 2 || (record.string_len & 1)) {
				continue;
			}
			if (record.platform_id == TT_PLATFORM_MICROSOFT && record.encoding_id != TT_MS_ID_SYMBOL_CS &&
					record.encoding_id != TT_MS_ID_UNICODE_CS && record.encoding_id != TT_MS_ID_UCS_4) {
				continue;
			}
			Char16String name;
			const unsigned int length = record.string_len / 2;
			if (name.resize_uninitialized(length) != OK) {
				continue;
			}
			char16_t *units = name.ptrw();
			for (unsigned int unit = 0; unit < length; unit++) {
				units[unit] = (uint16_t(record.string[unit * 2]) << 8) | record.string[unit * 2 + 1];
			}
			const String family = String::utf16(units, length).strip_edges();
			if (!family.is_empty() && !r_face.families.has(family)) {
				r_face.families.push_back(family);
			}
			if (record.platform_id == TT_PLATFORM_MICROSOFT) {
				wchar_t language[9];
				if (GetLocaleInfoW(MAKELCID(record.language_id, SORT_DEFAULT), LOCALE_SISO639LANGNAME, language, ARRAYSIZE(language)) > 1) {
					const String code = String::utf16(reinterpret_cast<const char16_t *>(language));
					if (!r_face.languages.has(code)) {
						r_face.languages.push_back(code);
					}
				}
			}
		}
		if (r_face.families.is_empty() && p_face->family_name) {
			r_face.families.push_back(String::utf8(p_face->family_name));
		}
	}

	static void _read_face(FT_Face p_face, InstalledFace &r_face) {
		_read_names(p_face, r_face);
		r_face.italic = (p_face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
		if (p_face->style_flags & FT_STYLE_FLAG_BOLD) {
			r_face.weight = 700;
		}
		const auto *style = static_cast<const TT_OS2 *>(FT_Get_Sfnt_Table(p_face, ft_sfnt_os2));
		if (style) {
			static constexpr int WIDTHS[] = { 50, 62, 75, 87, 100, 112, 125, 150, 200 };
			if (style->usWeightClass > 0 && style->usWeightClass <= 1000) {
				r_face.weight = style->usWeightClass;
			}
			if (style->usWidthClass > 0 && style->usWidthClass <= 9) {
				r_face.stretch = WIDTHS[style->usWidthClass - 1];
			}
		}

		if (FT_Select_Charmap(p_face, FT_ENCODING_UNICODE) != 0) {
			return;
		}
		FT_UInt glyph = 0;
		for (FT_ULong scalar = FT_Get_First_Char(p_face, &glyph); glyph && scalar <= 0x10FFFF; scalar = FT_Get_Next_Char(p_face, scalar, &glyph)) {
			const uint32_t code = scalar;
			if (!r_face.coverage.is_empty() && r_face.coverage[r_face.coverage.size() - 1].last + 1 == code) {
				r_face.coverage.write[r_face.coverage.size() - 1].last = code;
			} else {
				r_face.coverage.push_back({ code, code });
			}
		}
	}

	void _add_file(FT_Library p_library, const String &p_path, HashSet<String> &r_families) {
		const Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
		if (file.is_null()) {
			return;
		}
		const uint64_t size = file->get_length();
		if (!size || size > ULONG_MAX) {
			return;
		}
		FontStream reader;
		reader.file = file.ptr();
		FT_Long count = 1;
		for (FT_Long index = 0; index < count; index++) {
			FT_StreamRec stream = {};
			stream.size = static_cast<unsigned long>(size);
			stream.descriptor.pointer = &reader;
			stream.read = _read_font;
			FT_Open_Args args = {};
			args.flags = FT_OPEN_STREAM;
			args.stream = &stream;
			FT_Face face;
			if (FT_Open_Face(p_library, &args, index, &face) != 0) {
				continue;
			}
			count = face->num_faces;
			if (FT_IS_SCALABLE(face)) {
				InstalledFace entry;
				entry.path = p_path;
				_read_face(face, entry);
				if (!entry.families.is_empty()) {
					for (const String &family : entry.families) {
						const String key = family.to_lower();
						if (!r_families.has(key)) {
							r_families.insert(key);
							_families.push_back(family);
						}
					}
					_faces.push_back(entry);
				}
			}
			FT_Done_Face(face);
		}
	}

	void _read_registry(FT_Library p_library, HKEY p_root, const String &p_font_dir, HashSet<String> &r_paths, HashSet<String> &r_families) {
		HKEY key;
		if (RegOpenKeyExW(p_root, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
			return;
		}
		DWORD count, name_length, data_length;
		Char16String name, data;
		if (RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &count, &name_length, &data_length, nullptr, nullptr) != ERROR_SUCCESS ||
				name.resize_uninitialized(int64_t(name_length) + 1) != OK ||
				data.resize_uninitialized(int64_t(data_length) / 2 + 1) != OK) {
			RegCloseKey(key);
			return;
		}
		for (DWORD index = 0; index < count; index++) {
			DWORD name_size = name.size();
			DWORD data_size = data_length;
			DWORD type = 0;
			if (RegEnumValueW(key, index, reinterpret_cast<LPWSTR>(name.ptrw()), &name_size, nullptr, &type, reinterpret_cast<BYTE *>(data.ptrw()), &data_size) != ERROR_SUCCESS ||
					(type != REG_SZ && type != REG_EXPAND_SZ) || data_size < 2 || (data_size & 1)) {
				continue;
			}
			String path = String::utf16(data.ptr(), data_size / 2).replace_char('\\', '/');
			if (type == REG_EXPAND_SZ) {
				path = OS::get_singleton()->expand_path(path);
			}
			const String extension = path.get_extension().to_lower();
			if (extension != "ttf" && extension != "ttc" && extension != "otf" && extension != "otc") {
				continue;
			}
			if (!path.is_absolute_path()) {
				path = p_font_dir.path_join(path);
			}
			path = PathWindowsNT::get_absolute(path);
			const String path_key = path.to_lower();
			if (!path.is_empty() && !r_paths.has(path_key)) {
				r_paths.insert(path_key);
				_add_file(p_library, path, r_families);
			}
		}
		RegCloseKey(key);
	}

public:
	SystemFontCatalog() {
		wchar_t directory[MAX_PATH];
		if (!_get_shell_folder_path(CSIDL_FONTS, directory)) {
			return;
		}
		FT_Library library;
		if (FT_Init_FreeType(&library) != 0) {
			return;
		}
		const String path = String::utf16(reinterpret_cast<const char16_t *>(directory)).replace_char('\\', '/');
		HashSet<String> paths, families;
		_read_registry(library, HKEY_LOCAL_MACHINE, path, paths, families);
		_read_registry(library, HKEY_CURRENT_USER, path, paths, families);
		FT_Done_FreeType(library);
		_families.sort();
	}

	Vector<String> get_families() const {
		return _families;
	}

	String get_path(const String &p_family, int p_weight, int p_stretch, bool p_italic) const {
		const String family = _resolve_family(p_family);
		const int weight = CLAMP(p_weight, 1, 1000);
		const int stretch = CLAMP(p_stretch, 50, 200);
		const InstalledFace *best = nullptr;
		int best_distance = INT_MAX;
		for (const InstalledFace &face : _faces) {
			if (!face.matches_family(family)) {
				continue;
			}
			const int distance = face.style_distance(weight, stretch, p_italic);
			if (distance < best_distance) {
				best_distance = distance;
				best = &face;
			}
		}
		return best ? best->path : String();
	}

	Vector<String> get_fallback(const String &p_family, const String &p_text, const String &p_locale, int p_weight, int p_stretch, bool p_italic) const {
		const String family = _resolve_family(p_family);
		const String language = p_locale.replace_char('-', '_').get_slice("_", 0).to_lower();
		const int weight = CLAMP(p_weight, 1, 1000);
		const int stretch = CLAMP(p_stretch, 50, 200);
		Vector<Match> matches;
		for (const InstalledFace &face : _faces) {
			int64_t prefix = 0;
			for (int64_t i = 0; i < p_text.length(); i++) {
				const char32_t scalar = p_text[i];
				if (scalar == 0x200C || scalar == 0x200D || (scalar >= 0xFE00 && scalar <= 0xFE0F) || (scalar >= 0xE0100 && scalar <= 0xE01EF)) {
					// Joiners and variation selectors can be consumed by shaping.
					continue;
				}
				if (!face.covers(scalar)) {
					break;
				}
				prefix++;
			}
			if (prefix > 0) {
				matches.push_back({ &face, prefix, face.style_distance(weight, stretch, p_italic), face.matches_family(family), face.languages.has(language) });
			}
		}
		matches.sort();
		Vector<String> paths;
		HashSet<String> seen;
		for (const Match &match : matches) {
			if (!seen.has(match.face->path)) {
				seen.insert(match.face->path);
				paths.push_back(match.face->path);
			}
		}
		return paths;
	}
};

const OS_WindowsNT::SystemFontCatalog &OS_WindowsNT::_get_system_font_catalog() const {
	const MutexLock lock(_system_font_mutex);
	if (!_system_font_catalog) {
		_system_font_catalog = memnew(SystemFontCatalog);
	}
	return *_system_font_catalog;
}

#endif // MODULE_FREETYPE_ENABLED

void OS_WindowsNT::_free_system_font_catalog() {
#ifdef MODULE_FREETYPE_ENABLED
	if (_system_font_catalog) {
		memdelete(_system_font_catalog);
		_system_font_catalog = nullptr;
	}
#endif // MODULE_FREETYPE_ENABLED
}

Vector<String> OS_WindowsNT::get_system_fonts() const {
#ifdef MODULE_FREETYPE_ENABLED
	return _get_system_font_catalog().get_families();
#else
	return Vector<String>();
#endif // MODULE_FREETYPE_ENABLED
}

String OS_WindowsNT::get_system_font_path(const String &p_font_name, int p_weight, int p_stretch, bool p_italic) const {
#ifdef MODULE_FREETYPE_ENABLED
	return _get_system_font_catalog().get_path(p_font_name, p_weight, p_stretch, p_italic);
#else
	return String();
#endif // MODULE_FREETYPE_ENABLED
}

Vector<String> OS_WindowsNT::get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale, const String &, int p_weight, int p_stretch, bool p_italic) const {
#ifdef MODULE_FREETYPE_ENABLED
	return _get_system_font_catalog().get_fallback(p_font_name, p_text, p_locale, p_weight, p_stretch, p_italic);
#else
	return Vector<String>();
#endif // MODULE_FREETYPE_ENABLED
}
