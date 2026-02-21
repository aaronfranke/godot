/**************************************************************************/
/*  gltf_model_instance.cpp                                               */
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

#include "gltf_model_instance.h"

#include "../gltf_state.h"

#include "core/object/class_db.h"

Node *GLTFModelInstance::_find_node_by_name(Node *p_node, const String &p_node_name) const {
	if (p_node->get_name() == p_node_name) {
		return p_node;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		Node *result = _find_node_by_name(child, p_node_name);
		if (result != nullptr) {
			return result;
		}
	}
	return nullptr;
}

Node *GLTFModelInstance::import_generate_godot_node(const Ref<GLTFState> &p_parent_gltf_state) const {
	ERR_FAIL_COND_V(p_parent_gltf_state.is_null(), nullptr);
	// Get the model and create an instance of it.
	const Vector<Ref<GLTFModel>> &models = p_parent_gltf_state->get_models();
	ERR_FAIL_INDEX_V(_model_index, models.size(), nullptr);
	Ref<GLTFModel> model = models[_model_index];
	ERR_FAIL_COND_V(model.is_null(), nullptr);
	Node *ret = model->import_instantiate_model(p_parent_gltf_state);
	ERR_FAIL_NULL_V(ret, nullptr);
	// Apply any node overrides to the instance.
	for (const String node_name : _node_overrides.keys()) {
		Node *node = _find_node_by_name(ret, node_name);
		ERR_CONTINUE_MSG(node == nullptr, "GLTF import: Tried to override node with name '" + node_name + "', but no node with that name was found.");
		ret->set_editable_instance(node, true);
		const Dictionary node_override = _node_overrides[node_name];
		// TODO: Node overrides.
	}
	for (const String node_name : _node_additional_children.keys()) {
		Node *node = _find_node_by_name(ret, node_name);
		ERR_CONTINUE_MSG(node == nullptr, "GLTF import: Tried to add children to node with name '" + node_name + "', but no node with that name was found.");
		ret->set_editable_instance(node, true);
		const PackedInt32Array node_add_children = _node_additional_children[node_name];
		const Vector<Ref<GLTFNode>> &parent_gltf_nodes = p_parent_gltf_state->get_nodes();
		for (const int32_t &child_index : node_add_children) {
			ERR_FAIL_INDEX_V(child_index, parent_gltf_nodes.size(), ret);
			const Ref<GLTFNode> &child_gltf_node = parent_gltf_nodes[child_index];
			ERR_FAIL_COND_V(child_gltf_node.is_null(), ret);
			Node *child_node = child_gltf_node->import_generate_godot_node(p_parent_gltf_state, child_index);
			ERR_FAIL_NULL_V(child_node, ret);
			node->add_child(child_node);
		}
	}
	return ret;
}

Ref<GLTFModelInstance> GLTFModelInstance::from_dictionary(const Dictionary &p_dict) {
	Ref<GLTFModelInstance> instance;
	instance.instantiate();
	if (p_dict.has("model")) {
		instance->set_model_index(p_dict["model"]);
	}
	if (p_dict.has("name")) {
		instance->set_name(p_dict["name"]);
	}
	if (p_dict.has("nodeAdditionalChildren")) {
		instance->_node_additional_children = Dictionary(p_dict["nodeAdditionalChildren"]);
	}
	if (p_dict.has("nodeOverrides")) {
		instance->_node_overrides = Dictionary(p_dict["nodeOverrides"]);
	}
	return instance;
}

Dictionary GLTFModelInstance::to_dictionary() const {
	Dictionary dict;
	dict["model"] = _model_index;
	if (!get_name().is_empty()) {
		dict["name"] = get_name();
	}
	if (!_node_additional_children.is_empty()) {
		dict["nodeAdditionalChildren"] = _node_additional_children;
	}
	if (!_node_overrides.is_empty()) {
		dict["nodeOverrides"] = _node_overrides;
	}
	return dict;
}

void GLTFModelInstance::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_model_index"), &GLTFModelInstance::get_model_index);
	ClassDB::bind_method(D_METHOD("set_model_index", "name"), &GLTFModelInstance::set_model_index);

	ClassDB::bind_static_method("GLTFModelInstance", D_METHOD("from_dictionary", "dict"), &GLTFModelInstance::from_dictionary);
	ClassDB::bind_method(D_METHOD("to_dictionary"), &GLTFModelInstance::to_dictionary);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "model_index"), "set_model_index", "get_model_index");
}
