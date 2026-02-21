/**************************************************************************/
/*  gltf_file_reference.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "gltf_file_reference.h"

#include "../gltf_state.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"

PackedByteArray GLTFFileReference::parse_data_uri(const String &p_data_uri) {
	if (!p_data_uri.contains(";base64,")) {
		ERR_FAIL_V_MSG(PackedByteArray(), "GLTF import: Unsupported data URI format. Only base64-encoded data URIs are supported.");
	}
	int start = p_data_uri.find_char(',');
	ERR_FAIL_COND_V(start == -1, PackedByteArray());
	CharString substr = p_data_uri.substr(start + 1).ascii();
	const int strlen = substr.length();
	PackedByteArray buf;
	buf.resize(strlen / 4 * 3 + 1 + 1);
	size_t len = 0;
	const Error err = CryptoCore::b64_decode(buf.ptrw(), buf.size(), &len, (unsigned char *)substr.get_data(), strlen);
	ERR_FAIL_COND_V(err != OK, PackedByteArray());
	buf.resize(len);
	return buf;
}

PackedByteArray GLTFFileReference::read_file_data(const Ref<GLTFState> &p_gltf_state) const {
	ERR_FAIL_COND_V(!p_gltf_state.is_valid(), PackedByteArray());
	if (!_file_uri.is_empty()) {
		if (_file_uri.begins_with("data:")) {
			// Data URI, decode and return the data.
			return parse_data_uri(_file_uri);
		}
		const String path = p_gltf_state->get_base_path().path_join(_file_uri);
		Ref<DirAccess> dir = DirAccess::open(p_gltf_state->get_base_path());
		if (dir.is_valid() && dir->file_exists(_file_uri)) {
			return FileAccess::get_file_as_bytes(path);
		} else {
			ERR_FAIL_V_MSG(PackedByteArray(), "GLTF import: Failed to load file data from URI '" + _file_uri + "' resolved to '" + path + "'. File does not exist.");
		}
	}
	if (_buffer_view_index >= 0) {
		const Vector<Ref<GLTFBufferView>> &buffer_views = p_gltf_state->get_buffer_views();
		ERR_FAIL_INDEX_V(_buffer_view_index, buffer_views.size(), PackedByteArray());
		Ref<GLTFBufferView> buffer_view = buffer_views[_buffer_view_index];
		ERR_FAIL_COND_V(!buffer_view.is_valid(), PackedByteArray());
		return buffer_view->load_buffer_view_data(p_gltf_state);
	}
	ERR_FAIL_V_MSG(PackedByteArray(), "GLTF import: Failed to load file data, file reference does not have a valid URI or buffer view.");
}

Error GLTFFileReference::create_missing_directories_if_needed(const Ref<GLTFState> &p_gltf_state) const {
	ERR_FAIL_COND_V(p_gltf_state.is_null(), ERR_INVALID_PARAMETER);
	if (_file_uri.is_empty()) {
		return OK; // Nothing to do.
	}
	const String relative_folder = _file_uri.get_base_dir();
	if (!relative_folder.is_empty()) {
		String base_path = p_gltf_state->get_base_path();
		ERR_FAIL_COND_V(base_path.is_empty(), ERR_INVALID_DATA);
		Ref<DirAccess> dir = DirAccess::open(base_path);
		if (dir.is_null()) {
			base_path = ProjectSettings::get_singleton()->globalize_path(base_path);
			DirAccess::make_dir_recursive_absolute(base_path);
			dir = DirAccess::open(base_path);
		}
		ERR_FAIL_COND_V(dir.is_null(), ERR_FILE_CANT_OPEN);
		Error err = dir->make_dir_recursive(relative_folder);
		ERR_FAIL_COND_V(err != OK && err != ERR_ALREADY_EXISTS, err);
	}
	return OK;
}

Error GLTFFileReference::write_file_data(const Ref<GLTFState> &p_gltf_state, const PackedByteArray &p_data, int64_t p_alignment, int p_buffer_index, bool p_deduplicate) {
	ERR_FAIL_COND_V(!p_gltf_state.is_valid(), ERR_INVALID_PARAMETER);
	if (!_file_uri.is_empty()) {
		Error err = create_missing_directories_if_needed(p_gltf_state);
		ERR_FAIL_COND_V(err != OK, err);
		const String base_path = p_gltf_state->get_base_path();
		Ref<FileAccess> file = FileAccess::open(base_path.path_join(_file_uri), FileAccess::WRITE);
		ERR_FAIL_COND_V(file.is_null(), ERR_FILE_CANT_WRITE);
		file->store_buffer(p_data.ptr(), p_data.size());
		return OK;
	}
	// If there is no URI, write to a new buffer view.
	const int buffer_view_index = GLTFBufferView::write_new_buffer_view_into_state(p_gltf_state, p_data, p_alignment, GLTFBufferView::TARGET_NONE, -1, p_buffer_index, p_deduplicate);
	ERR_FAIL_COND_V_MSG(buffer_view_index < 0, ERR_CANT_CREATE, "GLTF export: Failed to write file data into new buffer view.");
	_buffer_view_index = buffer_view_index;
	return OK;
}

void GLTFFileReference::read_file_reference_entries_from_dictionary(const Dictionary &p_dict) {
	if (p_dict.has("bufferView")) {
		_buffer_view_index = p_dict["bufferView"];
	}
	if (p_dict.has("mimeType")) {
		_mime_type = p_dict["mimeType"];
	}
	if (p_dict.has("name")) {
		set_name(p_dict["name"]);
	}
	if (p_dict.has("uri")) {
		_file_uri = p_dict["uri"];
	}
	// Parse the MIME type from the data URI if it wasn't explicitly provided.
	if (_mime_type.is_empty() && _file_uri.begins_with("data:")) {
		const int mime_type_end = _file_uri.find(";");
		if (mime_type_end != -1) {
			const int mime_type_start = _file_uri.find(":") + 1;
			if (mime_type_start != -1 && mime_type_start < mime_type_end) {
				_mime_type = _file_uri.substr(mime_type_start, mime_type_end - mime_type_start);
			}
		}
	}
}

Dictionary GLTFFileReference::write_file_reference_entries_to_dictionary() const {
	Dictionary dict;
	if (_buffer_view_index >= 0) {
		dict["bufferView"] = _buffer_view_index;
	}
	if (!_mime_type.is_empty()) {
		dict["mimeType"] = _mime_type;
	}
	if (!get_name().is_empty()) {
		dict["name"] = get_name();
	}
	if (!_file_uri.is_empty()) {
		dict["uri"] = _file_uri;
	}
	return dict;
}

void GLTFFileReference::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_buffer_view_index"), &GLTFFileReference::get_buffer_view_index);
	ClassDB::bind_method(D_METHOD("set_buffer_view_index", "name"), &GLTFFileReference::set_buffer_view_index);
	ClassDB::bind_method(D_METHOD("get_mime_type"), &GLTFFileReference::get_mime_type);
	ClassDB::bind_method(D_METHOD("set_mime_type", "mime_type"), &GLTFFileReference::set_mime_type);
	ClassDB::bind_method(D_METHOD("get_file_uri"), &GLTFFileReference::get_file_uri);
	ClassDB::bind_method(D_METHOD("set_file_uri", "file_uri"), &GLTFFileReference::set_file_uri);

	ClassDB::bind_method(D_METHOD("read_file_data", "gltf_state"), &GLTFFileReference::read_file_data);
	ClassDB::bind_method(D_METHOD("create_missing_directories_if_needed", "gltf_state"), &GLTFFileReference::create_missing_directories_if_needed);
	ClassDB::bind_method(D_METHOD("write_file_data", "gltf_state", "data", "alignment", "buffer_index", "deduplicate"), &GLTFFileReference::write_file_data, DEFVAL(4), DEFVAL(0), DEFVAL(true));

	ClassDB::bind_method(D_METHOD("read_file_reference_entries_from_dictionary", "dict"), &GLTFFileReference::read_file_reference_entries_from_dictionary);
	ClassDB::bind_method(D_METHOD("write_file_reference_entries_to_dictionary"), &GLTFFileReference::write_file_reference_entries_to_dictionary);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "buffer_view_index"), "set_buffer_view_index", "get_buffer_view_index");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mime_type"), "set_mime_type", "get_mime_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "file_uri"), "set_file_uri", "get_file_uri");
}
