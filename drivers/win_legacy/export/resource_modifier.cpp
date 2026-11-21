#include "resource_modifier.h"

#ifdef WINDOWS_ENABLED

#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/marshalls.h"
#include "core/templates/hash_set.h"
#include "editor/export/editor_export_preset.h"

#include "platform/windows/export/template_modifier.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

class WindowsLegacyIconResource {
	enum {
		DIRECTORY_HEADER_SIZE = 6,
		FILE_ENTRY_SIZE = 16,
		GROUP_ENTRY_SIZE = 14,
		DIB_HEADER_SIZE = 40,
	};

	struct Frame {
		Size2i size;
		Vector<uint8_t> bytes;
		uint16_t depth = 32;
		uint8_t colors = 0;
	};

	Vector<Frame> _frames;

	static bool _valid_bitmap(const Frame &p_frame) {
		const uint8_t *bytes = p_frame.bytes.ptr();
		const uint32_t header_size = decode_uint32(bytes);
		const bool core = header_size == sizeof(BITMAPCOREHEADER);
		const uint32_t length = p_frame.bytes.size();
		if (!core && (header_size < DIB_HEADER_SIZE || header_size > length)) {
			return false;
		}
		const uint32_t width = core ? decode_uint16(bytes + 4) : decode_uint32(bytes + 4);
		const uint32_t height = core ? decode_uint16(bytes + 6) : decode_uint32(bytes + 8);
		const uint16_t planes = decode_uint16(bytes + (core ? 8 : 12));
		const uint16_t depth = decode_uint16(bytes + (core ? 10 : 14));
		if (width != uint32_t(p_frame.size.width) || height != uint32_t(p_frame.size.height * 2) || planes != 1 ||
				(depth != 1 && depth != 4 && depth != 8 && depth != 16 && depth != 24 && depth != 32)) {
			return false;
		}
		const uint32_t compression = core ? BI_RGB : decode_uint32(bytes + 16);
		if (compression != BI_RGB && (compression != BI_BITFIELDS || (depth != 16 && depth != 32))) {
			return false;
		}
		uint32_t palette = core ? 0 : decode_uint32(bytes + 32);
		if (palette == 0 && depth <= 8) {
			palette = 1U << depth;
		}
		const uint32_t bit_masks = compression == BI_BITFIELDS && header_size == DIB_HEADER_SIZE ? 12 : 0;
		const uint32_t stride = ((width * depth + 31) / 32 + (width + 31) / 32) * 4;
		return uint64_t(header_size) + bit_masks + uint64_t(palette) * (core ? 3 : 4) + stride * p_frame.size.height <= length;
	}

	static Vector<uint8_t> _encode_bitmap(const Ref<Image> &p_image, const Size2i &p_size) {
		Ref<Image> image = p_image;
		if (image->get_size() != p_size) {
			image = image->duplicate();
			image->resize(p_size.width, p_size.height, Image::INTERPOLATE_LANCZOS);
		}
		const int color_stride = p_size.width * 4;
		const int mask_stride = ((p_size.width + 31) / 32) * 4;
		const int color_bytes = color_stride * p_size.height;
		Vector<uint8_t> result;
		result.resize_initialized(DIB_HEADER_SIZE + color_bytes + mask_stride * p_size.height);
		uint8_t *bytes = result.ptrw();
		encode_uint32(DIB_HEADER_SIZE, bytes);
		encode_uint32(p_size.width, bytes + 4);
		encode_uint32(p_size.height * 2, bytes + 8);
		encode_uint16(1, bytes + 12);
		encode_uint16(32, bytes + 14);
		encode_uint32(color_bytes, bytes + 20);
		for (int y = 0; y < p_size.height; y++) {
			uint8_t *color = bytes + DIB_HEADER_SIZE + color_stride * y;
			uint8_t *mask = bytes + DIB_HEADER_SIZE + color_bytes + mask_stride * y;
			for (int x = 0; x < p_size.width; x++) {
				const uint32_t pixel = image->get_pixel(x, p_size.height - y - 1).to_argb32();
				// Windows 2000 draws the AND/XOR mask instead of using the alpha channel.
				if ((pixel >> 24) < 128) {
					mask[x / 8] |= 0x80 >> (x % 8);
				} else {
					encode_uint32(pixel, color + x * 4);
				}
			}
		}
		return result;
	}

	Error _load_ico(const String &p_path) {
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
		if (file.is_null()) {
			return ERR_FILE_CANT_OPEN;
		}
		const uint64_t length = file->get_length();
		if (length < DIRECTORY_HEADER_SIZE || file->get_16() != 0 || file->get_16() != 1) {
			return ERR_FILE_CORRUPT;
		}
		const int count = file->get_16();
		const int directory_size = count * FILE_ENTRY_SIZE;
		const uint64_t first_frame = DIRECTORY_HEADER_SIZE + directory_size;
		if (count == 0 || first_frame > length) {
			return ERR_FILE_CORRUPT;
		}
		const Vector<uint8_t> directory = file->get_buffer(directory_size);
		if (directory.size() != directory_size) {
			return ERR_FILE_CANT_READ;
		}
		for (int i = 0; i < count; i++) {
			const uint8_t *entry = directory.ptr() + i * FILE_ENTRY_SIZE;
			const uint32_t byte_count = decode_uint32(entry + 8);
			const uint32_t offset = decode_uint32(entry + 12);
			if (byte_count < sizeof(BITMAPCOREHEADER) || byte_count > INT_MAX || offset < first_frame ||
					uint64_t(offset) + byte_count > length) {
				continue;
			}
			file->seek(offset);
			Frame frame;
			frame.size = Size2i(entry[0] ? entry[0] : 256, entry[1] ? entry[1] : 256);
			frame.bytes = file->get_buffer(byte_count);
			if (frame.bytes.size() != int(byte_count)) {
				continue;
			}
			const uint8_t *bytes = frame.bytes.ptr();
			if (byte_count >= 24 && decode_uint32(bytes) == 0x474e5089 && decode_uint32(bytes + 4) == 0x0a1a0a0d) {
				if (BSWAP32(decode_uint32(bytes + 16)) != uint32_t(frame.size.width) ||
						BSWAP32(decode_uint32(bytes + 20)) != uint32_t(frame.size.height)) {
					continue;
				}
				Ref<Image> image;
				image.instantiate();
				if (image->load_png_from_buffer(frame.bytes) != OK) {
					continue;
				}
				frame.bytes = _encode_bitmap(image, frame.size);
			} else {
				if (!_valid_bitmap(frame)) {
					continue;
				}
				frame.colors = entry[2];
				frame.depth = decode_uint16(entry + 6);
			}
			_frames.push_back(frame);
		}
		return _frames.is_empty() ? ERR_FILE_CORRUPT : OK;
	}

public:
	Error load(const String &p_path) {
		if (p_path.get_extension().nocasecmp_to("ico") == 0) {
			return _load_ico(p_path);
		}
		Ref<Image> image = Image::load_from_file(p_path);
		if (image.is_null() || image->is_empty()) {
			return ERR_FILE_CANT_READ;
		}
		if (image->is_compressed() && image->decompress() != OK) {
			return ERR_UNAVAILABLE;
		}
		for (int side : { 16, 32, 48, 64, 128, 256 }) {
			Frame frame;
			frame.size = Size2i(side, side);
			frame.bytes = _encode_bitmap(image, frame.size);
			_frames.push_back(frame);
		}
		return OK;
	}

	bool update(HANDLE p_update, LPCWSTR p_group_name, const Vector<WORD> &p_languages, const HashSet<WORD> &p_used_ids) {
		Vector<uint8_t> group;
		group.resize_initialized(DIRECTORY_HEADER_SIZE + _frames.size() * GROUP_ENTRY_SIZE);
		encode_uint16(1, group.ptrw() + 2);
		encode_uint16(_frames.size(), group.ptrw() + 4);
		unsigned int next_id = 1;
		for (int i = 0; i < _frames.size(); i++) {
			while (next_id <= UINT16_MAX && p_used_ids.has(next_id)) {
				next_id++;
			}
			if (next_id > UINT16_MAX) {
				SetLastError(ERROR_NO_MORE_ITEMS);
				return false;
			}
			Frame &frame = _frames.write[i];
			uint8_t *entry = group.ptrw() + DIRECTORY_HEADER_SIZE + i * GROUP_ENTRY_SIZE;
			entry[0] = frame.size.width % 256;
			entry[1] = frame.size.height % 256;
			entry[2] = frame.colors;
			encode_uint16(1, entry + 4);
			encode_uint16(frame.depth, entry + 6);
			encode_uint32(frame.bytes.size(), entry + 8);
			encode_uint16(next_id, entry + 12);
			for (WORD language : p_languages) {
				if (!UpdateResourceW(p_update, MAKEINTRESOURCEW(3), MAKEINTRESOURCEW(next_id), language, frame.bytes.ptrw(), frame.bytes.size())) {
					return false;
				}
			}
			next_id++;
		}
		for (WORD language : p_languages) {
			if (!UpdateResourceW(p_update, MAKEINTRESOURCEW(14), p_group_name, language, group.ptrw(), group.size())) {
				return false;
			}
		}
		return true;
	}
};

class WindowsLegacyExistingResources {
	enum {
		ICON = 3,
		GROUP_ICON = 14,
		VERSION = 16,
		ENGLISH_US = 0x0409,
	};

	Char16String _group_name;
	WORD _group_id = 1;
	Vector<WORD> _group_languages;
	Vector<WORD> _version_languages;
	HashSet<WORD> _used_icon_ids;

	static BOOL CALLBACK _get_first_group(HMODULE p_module, LPCWSTR p_type, LPWSTR p_name, LONG_PTR p_data) {
		WindowsLegacyExistingResources &resources = *reinterpret_cast<WindowsLegacyExistingResources *>(p_data);
		if (IS_INTRESOURCE(p_name)) {
			resources._group_id = WORD(reinterpret_cast<ULONG_PTR>(p_name));
		} else {
			resources._group_name = String::utf16(reinterpret_cast<const char16_t *>(p_name)).utf16();
		}
		return FALSE;
	}

	static BOOL CALLBACK _get_language(HMODULE p_module, LPCWSTR p_type, LPCWSTR p_name, WORD p_language, LONG_PTR p_data) {
		reinterpret_cast<Vector<WORD> *>(p_data)->push_back(p_language);
		return TRUE;
	}

	static BOOL CALLBACK _get_icon_id(HMODULE p_module, LPCWSTR p_type, LPWSTR p_name, LONG_PTR p_data) {
		if (IS_INTRESOURCE(p_name)) {
			reinterpret_cast<HashSet<WORD> *>(p_data)->insert(WORD(reinterpret_cast<ULONG_PTR>(p_name)));
		}
		return TRUE;
	}

	LPCWSTR _group_identifier() const {
		return _group_name.is_empty() ? MAKEINTRESOURCEW(_group_id) : reinterpret_cast<LPCWSTR>(_group_name.get_data());
	}

public:
	void read(HMODULE p_module, bool p_with_icon) {
		EnumResourceLanguagesW(p_module, MAKEINTRESOURCEW(VERSION), MAKEINTRESOURCEW(1), _get_language, reinterpret_cast<LONG_PTR>(&_version_languages));
		if (_version_languages.is_empty()) {
			_version_languages.push_back(ENGLISH_US);
		}
		if (p_with_icon) {
			EnumResourceNamesW(p_module, MAKEINTRESOURCEW(GROUP_ICON), _get_first_group, reinterpret_cast<LONG_PTR>(this));
			EnumResourceNamesW(p_module, MAKEINTRESOURCEW(ICON), _get_icon_id, reinterpret_cast<LONG_PTR>(&_used_icon_ids));
			EnumResourceLanguagesW(p_module, MAKEINTRESOURCEW(GROUP_ICON), _group_identifier(), _get_language, reinterpret_cast<LONG_PTR>(&_group_languages));
			if (_group_languages.is_empty()) {
				_group_languages.push_back(ENGLISH_US);
			}
		}
	}

	bool update(HANDLE p_update, Vector<uint8_t> &p_version, WindowsLegacyIconResource *p_icon) const {
		for (WORD language : _version_languages) {
			if (!UpdateResourceW(p_update, MAKEINTRESOURCEW(VERSION), MAKEINTRESOURCEW(1), language, p_version.ptrw(), p_version.size())) {
				return false;
			}
		}
		return !p_icon || p_icon->update(p_update, _group_identifier(), _group_languages, _used_icon_ids);
	}
};

} // namespace

Error WindowsLegacyResourceModifier::modify(const Ref<EditorExportPreset> &p_preset, const String &p_executable, const String &p_icon, String &r_message) {
	WindowsLegacyIconResource icon;
	if (!p_icon.is_empty()) {
		const Error error = icon.load(p_icon);
		if (error != OK) {
			r_message = vformat(TTR("Could not read the executable icon \"%s\"."), p_icon);
			return error;
		}
	}
	Vector<uint8_t> version = TemplateModifier::create_version_resource(p_preset);
	if (version.size() > UINT16_MAX) {
		r_message = TTR("The version information is too large for a Windows version resource.");
		return ERR_INVALID_DATA;
	}

	const Char16String filename = p_executable.utf16();
	const LPCWSTR path = reinterpret_cast<LPCWSTR>(filename.get_data());
	HMODULE module = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_AS_DATAFILE);
	if (!module) {
		r_message = vformat(TTR("Could not inspect the executable resources (Windows error %d)."), GetLastError());
		return ERR_FILE_CANT_OPEN;
	}
	WindowsLegacyExistingResources existing;
	existing.read(module, !p_icon.is_empty());
	FreeLibrary(module);

	HANDLE update = BeginUpdateResourceW(path, FALSE);
	if (!update) {
		r_message = vformat(TTR("Could not open the executable for resource editing (Windows error %d)."), GetLastError());
		return ERR_FILE_CANT_WRITE;
	}
	if (!existing.update(update, version, p_icon.is_empty() ? nullptr : &icon)) {
		const DWORD error = GetLastError();
		EndUpdateResourceW(update, TRUE);
		r_message = vformat(TTR("Could not update the executable resources (Windows error %d)."), error);
		return ERR_FILE_CANT_WRITE;
	}
	if (!EndUpdateResourceW(update, FALSE)) {
		r_message = vformat(TTR("Could not save the executable resources (Windows error %d)."), GetLastError());
		return ERR_FILE_CANT_WRITE;
	}
	return OK;
}

#endif // WINDOWS_ENABLED
