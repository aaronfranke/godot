/**************************************************************************/
/*  gltf_model.h                                                          */
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

#include "gltf_file_reference.h"

class GLTFModel : public GLTFFileReference {
	GDCLASS(GLTFModel, GLTFFileReference);

	// Optional. If provided, use this document for import settings.
	Ref<GLTFDocument> _model_gltf_document;
	// The actual data of the model.
	Ref<GLTFState> _model_gltf_state;

	String _get_inferred_mime_type() const;

protected:
	static void _bind_methods();

public:
	Ref<GLTFDocument> get_model_gltf_document() const;
	void set_model_gltf_document(const Ref<GLTFDocument> &p_model_gltf_document);

	Error import_preload_model_data(const Ref<GLTFState> &p_parent_gltf_state);
	Node *import_instantiate_model(const Ref<GLTFState> &p_parent_gltf_state) const;
	static int export_pack_nodes_into_model(const Variant p_gltf_document, const Ref<GLTFState> &p_parent_gltf_state, Node *p_node, const bool p_deduplicate = true);
	Error export_write_model_data(const Ref<GLTFState> &p_parent_gltf_state, const bool p_deduplicate = true, int p_buffer_index = 0);

	static Ref<GLTFModel> from_dictionary(const Dictionary &p_dict);
};
