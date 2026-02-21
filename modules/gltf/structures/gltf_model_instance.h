/**************************************************************************/
/*  gltf_model_instance.h                                                 */
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
#include "core/variant/typed_dictionary.h"

class GLTFModelInstance : public Resource {
	GDCLASS(GLTFModelInstance, Resource);

	TypedDictionary<String, PackedInt32Array> _node_additional_children;
	TypedDictionary<String, Dictionary> _node_overrides;
	GLTFModelIndex _model_index = -1;

	Node *_find_node_by_name(Node *p_node, const String &p_node_name) const;

protected:
	static void _bind_methods();

public:
	GLTFModelIndex get_model_index() const { return _model_index; }
	void set_model_index(GLTFModelIndex p_model_index) { _model_index = p_model_index; }

	TypedDictionary<String, PackedInt32Array> get_node_additional_children() const { return _node_additional_children; }
	void set_node_additional_children(const TypedDictionary<String, PackedInt32Array> &p_node_additional_children) { _node_additional_children = p_node_additional_children; }

	TypedDictionary<String, Dictionary> get_node_overrides() const { return _node_overrides; }
	void set_node_overrides(const TypedDictionary<String, Dictionary> &p_node_overrides) { _node_overrides = p_node_overrides; }

	Node *import_generate_godot_node(const Ref<GLTFState> &p_parent_gltf_state) const;

	static Ref<GLTFModelInstance> from_dictionary(const Dictionary &p_dict);
	Dictionary to_dictionary() const;
};
