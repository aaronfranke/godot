/**************************************************************************/
/*  gltf_file_reference.h                                                 */
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

#pragma once

#include "../gltf_defines.h"

#include "core/io/resource.h"

class GLTFFileReference : public Resource {
	GDCLASS(GLTFFileReference, Resource);

	String _mime_type = "";
	String _file_uri = "";
	GLTFBufferViewIndex _buffer_view_index = -1;

protected:
	static void _bind_methods();

public:
	static PackedByteArray parse_data_uri(const String &p_data_uri);

	GLTFBufferViewIndex get_buffer_view_index() const { return _buffer_view_index; }
	void set_buffer_view_index(GLTFBufferViewIndex p_buffer_view_index) { _buffer_view_index = p_buffer_view_index; }

	String get_mime_type() const { return _mime_type; }
	void set_mime_type(const String &p_mime_type) { _mime_type = p_mime_type; }

	String get_file_uri() const { return _file_uri; }
	void set_file_uri(const String &p_uri) { _file_uri = p_uri; }

	PackedByteArray read_file_data(const Ref<GLTFState> &p_gltf_state) const;
	Error create_missing_directories_if_needed(const Ref<GLTFState> &p_gltf_state) const;
	Error write_file_data(const Ref<GLTFState> &p_gltf_state, const PackedByteArray &p_data, int64_t p_alignment = 4, int p_buffer_index = 0, bool p_deduplicate = true);

	void read_file_reference_entries_from_dictionary(const Dictionary &p_dict);
	Dictionary write_file_reference_entries_to_dictionary() const;
};
