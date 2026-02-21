/**************************************************************************/
/*  gltf_model.cpp                                                        */
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

#include "gltf_model.h"

#include "../extensions/gltf_document_extension_convert_importer_mesh.h"
#include "../gltf_document.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "scene/resources/packed_scene.h"

Ref<GLTFDocument> GLTFModel::get_model_gltf_document() const {
	return _model_gltf_document;
}

void GLTFModel::set_model_gltf_document(const Ref<GLTFDocument> &p_model_gltf_document) {
	_model_gltf_document = p_model_gltf_document;
}

String GLTFModel::_get_inferred_mime_type() const {
	const String declared_mime_type = get_mime_type();
	if (!declared_mime_type.is_empty()) {
		return declared_mime_type;
	}
	// If MIME type is not set, try to guess based on file extension.
	const String extension = get_file_uri().get_extension().to_lower();
	if (extension == "glb") {
		return "model/gltf-binary";
	} else if (extension == "gltf") {
		return "model/gltf+json";
	} else if (extension == "fbx") {
		return "model/fbx";
	} else if (extension == "obj") {
		return "model/obj";
	}
	return String();
}

Error GLTFModel::import_preload_model_data(const Ref<GLTFState> &p_parent_gltf_state) {
	ERR_FAIL_COND_V(p_parent_gltf_state.is_null(), ERR_INVALID_PARAMETER);
	const String mime_type = get_mime_type();
	ERR_FAIL_COND_V(mime_type.is_empty(), ERR_INVALID_DATA);
	const String main_base_path = p_parent_gltf_state->get_base_path();
	const String file_uri_only = get_file_uri();
	Error err = OK;
	// Handle based on declared or inferred MIME type.
	if (mime_type == "model/gltf-binary" || mime_type == "model/gltf+json") {
		Ref<GLTFDocument> model_gltf_document = _model_gltf_document;
		if (model_gltf_document.is_null()) {
			model_gltf_document.instantiate();
			_model_gltf_document = model_gltf_document;
		}
		_model_gltf_state.instantiate();
		if (file_uri_only.is_empty() || file_uri_only.begins_with("data:")) {
			const PackedByteArray file_data = read_file_data(p_parent_gltf_state);
			ERR_FAIL_COND_V(file_data.is_empty(), ERR_CANT_OPEN);
			err = _model_gltf_document->append_from_buffer(file_data, main_base_path, _model_gltf_state);
		} else {
			const String file_uri_path = main_base_path.path_join(file_uri_only);
			err = _model_gltf_document->append_from_file(file_uri_path, _model_gltf_state);
		}
		ERR_FAIL_COND_V_MSG(err != OK, err, "GLTF import: GLTFModel: Failed to import glTF model.");
	} else {
		ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "GLTF import: GLTFModel: Unsupported model MIME type: " + mime_type);
	}
	return err;
}

Node *GLTFModel::import_instantiate_model(const Ref<GLTFState> &p_parent_gltf_state) const {
	Node *ret = nullptr;
	ERR_FAIL_COND_V(p_parent_gltf_state.is_null(), ret);
	const String mime_type = get_mime_type();
	ERR_FAIL_COND_V(mime_type.is_empty(), ret);
	Ref<GLTFDocument> model_gltf_document = _model_gltf_document;
	// Handle based on MIME type.
	if (mime_type == "model/gltf-binary" || mime_type == "model/gltf+json") {
		// TODO: Check dimension before importing to try and pick the best importer. For now assume .
		ERR_FAIL_COND_V_MSG(model_gltf_document.is_null() || _model_gltf_state.is_null(), ret, "GLTF import: GLTFModel: Failed to instantiate GLTF model without preloaded data.");
		ret = model_gltf_document->generate_scene(_model_gltf_state);
		if (ret->get_name().is_empty()) {
			if (!get_name().is_empty()) {
				ret->set_name(get_name());
			} else if (!get_file_uri().is_empty()) {
				ret->set_name(get_file_uri().get_file().get_basename());
			} else {
				ret->set_name("ModelRoot");
			}
		}
	} else {
		ERR_FAIL_V_MSG(ret, "GLTF import: GLTFModel: Unsupported model MIME type: " + mime_type);
	}
	ERR_FAIL_NULL_V_MSG(ret, ret, "GLTF import: GLTFModel: Failed to instantiate model.");
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		// If this is the editor, try to set the scene file path for saving purposes.
		// For non-editor situations, it's good enough to just have the nodes in memory,
		// also the owner property will be set in case the user wants to save the scene themselves.
		String file_uri_path = p_parent_gltf_state->get_base_path();
		if (!file_uri_path.is_empty()) {
			const String file_uri_only = get_file_uri();
			if (file_uri_only.is_empty() || file_uri_only.begins_with("data:")) {
				// If the file URI is empty, it means the model data came from a buffer view, or this is a data URI.
				// In both cases, we can't point to an original file on disk, so we need to save the
				// scene data into the imported files directory as a Godot `.scn` file, and point to that.
				const String imported_files_path = ProjectSettings::get_singleton()->get_imported_files_path();
				const String scn_prefix = p_parent_gltf_state->get_base_path().trim_prefix("res://").replace_chars("/", '_');
				const String scn_suffix = p_parent_gltf_state->get_filename() + "_" + String(ret->get_name()) + ".scn";
				file_uri_path = imported_files_path.path_join(scn_prefix + "_" + scn_suffix).to_lower();
				ret = GLTFDocumentExtensionConvertImporterMesh::convert_importer_meshes_recursively(ret);
				Ref<PackedScene> packed_scene;
				packed_scene.instantiate();
				packed_scene->pack(ret);
				Error err = ResourceSaver::save(packed_scene, file_uri_path);
				if (err != OK) {
					WARN_PRINT(vformat("GLTFModel: Failed to save scene file for model instance. Path: '%s', Error code: %d", file_uri_path, err));
					file_uri_path = String();
				}
			} else {
				file_uri_path = file_uri_path.path_join(file_uri_only);
			}
			if (file_uri_path.begins_with("res://")) {
				ret->set_scene_file_path(file_uri_path);
			}
		}
	}
#endif // TOOLS_ENABLED
	return ret;
}

int GLTFModel::export_pack_nodes_into_model(const Variant p_gltf_document, const Ref<GLTFState> &p_parent_gltf_state, Node *p_node, const bool p_deduplicate) {
	ERR_FAIL_COND_V(p_parent_gltf_state.is_null(), ERR_INVALID_PARAMETER);
	Ref<GLTFDocument> model_gltf_document = p_gltf_document;
	if (model_gltf_document.is_null()) {
		model_gltf_document.instantiate();
	}
	ERR_FAIL_NULL_V(p_node, -1);
	const String scene_base_name = p_node->get_scene_file_path().get_file().get_basename();
	const String target_file_without_extension = (scene_base_name.is_empty() ? String(p_node->get_name()) : scene_base_name).to_snake_case();
	const String target_uri_without_extension = "models/" + target_file_without_extension;
	Vector<Ref<GLTFModel>> gltf_models = p_parent_gltf_state->get_models();
	if (p_deduplicate && !target_uri_without_extension.is_empty()) {
		for (int i = 0; i < gltf_models.size(); i++) {
			Ref<GLTFModel> gltf_model = gltf_models[i];
			if (gltf_model.is_valid() && gltf_model->get_file_uri() == target_uri_without_extension) {
				return i;
			}
		}
	}
	// Create a new GLTFModel to hold a reference to the model data.
	Ref<GLTFModel> gltf_model;
	gltf_model.instantiate();
	gltf_model->set_model_gltf_document(model_gltf_document);
	gltf_model->set_file_uri(target_uri_without_extension);
	// Fill the GLTFModel with the model data.
	{
		Ref<GLTFState> model_gltf_state;
		model_gltf_state.instantiate();
		Error err = model_gltf_document->append_from_scene(p_node, model_gltf_state);
		ERR_FAIL_COND_V(err != OK, -1);
		// Ensure that the root node is untransformed and visible.
		// If invisible, the *instance* should be the one to be invisible, not the model itself.
		{
			const Vector<Ref<GLTFNode>> &nodes = model_gltf_state->get_nodes();
			ERR_FAIL_COND_V(nodes.is_empty(), -1);
			if (model_gltf_document->get_root_node_mode() != GLTFDocument::ROOT_NODE_MODE_MULTI_ROOT) {
				// Note: glTF does not require that the root node is 0, but during Godot export, it is.
				Ref<GLTFNode> root_node = nodes[0];
				ERR_FAIL_COND_V(root_node.is_null(), -1);
				root_node->set_xform(Transform3D());
				root_node->set_visible(true);
				if (root_node->get_name().is_empty()) {
					root_node->set_name(target_file_without_extension);
				}
			}
		}
		gltf_model->_model_gltf_state = model_gltf_state;
	}
	// Append the model to the state.
	const int model_index = gltf_models.size();
	gltf_models.append(gltf_model);
	p_parent_gltf_state->set_models(gltf_models);
	return model_index;
}

Error GLTFModel::export_write_model_data(const Ref<GLTFState> &p_parent_gltf_state, const bool p_deduplicate, int p_buffer_index) {
	ERR_FAIL_COND_V(p_parent_gltf_state.is_null(), ERR_INVALID_PARAMETER);
	const Ref<GLTFDocument> model_gltf_document = _model_gltf_document;
	ERR_FAIL_COND_V(model_gltf_document.is_null(), ERR_INVALID_DATA);
	const bool is_text_file = p_parent_gltf_state->is_text_file();
	constexpr bool should_separate_model_files = true;
	// Determine the file extension and MIME type to use based on what data is available.
	String file_extension;
	String mime_type;
	if (_model_gltf_state.is_valid()) {
		file_extension = is_text_file ? ".gltf" : ".glb";
		mime_type = is_text_file ? "model/gltf+json" : "model/gltf-binary";
	} else {
		ERR_FAIL_V_MSG(ERR_INVALID_DATA, "GLTF export: GLTFModel: No model data to write.");
	}
	// Use the MIME type determined above as-is, and use the file extension to determine the path and name.
	set_mime_type(mime_type);
	const String target_uri = get_file_uri() + file_extension;
	const String file_path = p_parent_gltf_state->get_base_path().path_join(target_uri);
	if (target_uri.contains("/")) {
		set_name(target_uri.get_file());
	}
	// If we are writing to a separate model file, set the URI and write to that location.
	if (should_separate_model_files) {
		set_file_uri(target_uri);
		create_missing_directories_if_needed(p_parent_gltf_state);
		if (_model_gltf_state.is_valid()) {
			return _model_gltf_document->write_to_filesystem(_model_gltf_state, file_path);
		} else {
			CRASH_NOW_MSG("GLTF export: GLTFModel: No model data to write, but should've exited earlier.");
		}
	}
	// Embed the model data into a buffer view. The URI must be empty in this case.
	set_file_uri("");
	PackedByteArray model_bytes;
	if (_model_gltf_state.is_valid()) {
		model_bytes = _model_gltf_document->generate_buffer(_model_gltf_state);
	} else {
		CRASH_NOW_MSG("GLTF export: GLTFModel: No model data to write, but should've exited earlier.");
	}
	// Now that we have the bytes, write them into a new buffer view in the state.
	ERR_FAIL_COND_V_MSG(model_bytes.is_empty(), ERR_CANT_CREATE, "GLTF export: GLTFModel: The exported model data is an empty byte array, export failed.");
	const int buffer_view_index = GLTFBufferView::write_new_buffer_view_into_state(p_parent_gltf_state, model_bytes, 16, GLTFBufferView::TARGET_NONE, -1, p_buffer_index, p_deduplicate);
	ERR_FAIL_COND_V(buffer_view_index < 0, ERR_CANT_CREATE);
	set_buffer_view_index(buffer_view_index);
	return OK;
}

Ref<GLTFModel> GLTFModel::from_dictionary(const Dictionary &p_dict) {
	Ref<GLTFModel> model;
	model.instantiate();
	model->read_file_reference_entries_from_dictionary(p_dict);
	return model;
}

void GLTFModel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_model_gltf_document"), &GLTFModel::get_model_gltf_document);
	ClassDB::bind_method(D_METHOD("set_model_gltf_document", "model_gltf_document"), &GLTFModel::set_model_gltf_document);

	ClassDB::bind_method(D_METHOD("import_preload_model_data", "parent_gltf_state"), &GLTFModel::import_preload_model_data);
	ClassDB::bind_method(D_METHOD("import_instantiate_model", "parent_gltf_state"), &GLTFModel::import_instantiate_model);
	ClassDB::bind_static_method("GLTFModel", D_METHOD("export_pack_nodes_into_model", "gltf_document", "parent_gltf_state", "node", "deduplicate"), &GLTFModel::export_pack_nodes_into_model, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("export_write_model_data", "parent_gltf_state", "deduplicate", "buffer_index"), &GLTFModel::export_write_model_data, DEFVAL(true), DEFVAL(0));

	ClassDB::bind_static_method("GLTFModel", D_METHOD("from_dictionary", "dict"), &GLTFModel::from_dictionary);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "model_gltf_document", PROPERTY_HINT_RESOURCE_TYPE, "GLTFDocument"), "set_model_gltf_document", "get_model_gltf_document");
}
