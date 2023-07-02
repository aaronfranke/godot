/**************************************************************************/
/*  gltf_document_extension_physics.cpp                                   */
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

#include "gltf_document_extension_physics.h"

#include "scene/3d/area_3d.h"

// Import process.
Error GLTFDocumentExtensionPhysics::import_preflight(Ref<GLTFState> p_state, Vector<String> p_extensions) {
	if (!p_extensions.has("OMI_collider") && !p_extensions.has("OMI_physics_body") && !p_extensions.has("OMI_physics_shape")) {
		return ERR_SKIP;
	}
	Dictionary state_json = p_state->get_json();
	if (state_json.has("extensions")) {
		Dictionary state_extensions = state_json["extensions"];
		if (state_extensions.has("OMI_physics_shape")) {
			Dictionary omi_physics_shape_ext = state_extensions["OMI_physics_shape"];
			if (omi_physics_shape_ext.has("shapes")) {
				Array state_shape_dicts = omi_physics_shape_ext["shapes"];
				if (state_shape_dicts.size() > 0) {
					Array state_shapes;
					for (int i = 0; i < state_shape_dicts.size(); i++) {
						state_shapes.push_back(GLTFPhysicsShape::from_dictionary(state_shape_dicts[i]));
					}
					p_state->set_additional_data("GLTFPhysicsShapes", state_shapes);
				}
			}
		} else if (state_extensions.has("OMI_collider")) {
			Dictionary omi_collider_ext = state_extensions["OMI_collider"];
			if (omi_collider_ext.has("colliders")) {
				Array state_collider_dicts = omi_collider_ext["colliders"];
				if (state_collider_dicts.size() > 0) {
					Array state_colliders;
					for (int i = 0; i < state_collider_dicts.size(); i++) {
						state_colliders.push_back(GLTFPhysicsShape::from_dictionary(state_collider_dicts[i]));
					}
					p_state->set_additional_data("GLTFPhysicsShapes", state_colliders);
				}
			}
		}
	}
	return OK;
}

Vector<String> GLTFDocumentExtensionPhysics::get_supported_extensions() {
	Vector<String> ret;
	ret.push_back("OMI_collider");
	ret.push_back("OMI_physics_body");
	ret.push_back("OMI_physics_shape");
	return ret;
}

Error GLTFDocumentExtensionPhysics::parse_node_extensions(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Dictionary &p_extensions) {
	if (p_extensions.has("OMI_physics_shape")) {
		Dictionary node_shape_ext = p_extensions["OMI_physics_shape"];
		if (node_shape_ext.has("shape")) {
			// "shape" is the index of the shape in the state shapes array.
			int node_shape_index = node_shape_ext["shape"];
			Array state_shapes = p_state->get_additional_data("GLTFPhysicsShapes");
			ERR_FAIL_INDEX_V_MSG(node_shape_index, state_shapes.size(), Error::ERR_FILE_CORRUPT, "GLTF Physics: On node " + p_gltf_node->get_name() + ", the shape index " + itos(node_shape_index) + " is not in the state shapes (size: " + itos(state_shapes.size()) + ").");
			p_gltf_node->set_additional_data(StringName("GLTFPhysicsShape"), state_shapes[node_shape_index]);
		} else {
			p_gltf_node->set_additional_data(StringName("GLTFPhysicsShape"), GLTFPhysicsShape::from_dictionary(node_shape_ext));
		}
	} else if (p_extensions.has("OMI_collider")) {
		Dictionary node_collider_ext = p_extensions["OMI_collider"];
		if (node_collider_ext.has("collider")) {
			// "collider" is the index of the collider in the state colliders array.
			int node_collider_index = node_collider_ext["collider"];
			Array state_colliders = p_state->get_additional_data("GLTFPhysicsShapes");
			ERR_FAIL_INDEX_V_MSG(node_collider_index, state_colliders.size(), Error::ERR_FILE_CORRUPT, "GLTF Physics: On node " + p_gltf_node->get_name() + ", the collider index " + itos(node_collider_index) + " is not in the state colliders (size: " + itos(state_colliders.size()) + ").");
			p_gltf_node->set_additional_data(StringName("GLTFPhysicsShape"), state_colliders[node_collider_index]);
		} else {
			p_gltf_node->set_additional_data(StringName("GLTFPhysicsShape"), GLTFPhysicsShape::from_dictionary(node_collider_ext));
		}
	}
	if (p_extensions.has("OMI_physics_body")) {
		p_gltf_node->set_additional_data(StringName("GLTFPhysicsBody"), GLTFPhysicsBody::from_dictionary(p_extensions["OMI_physics_body"]));
	}
	return OK;
}

void _setup_shape_mesh_resource_from_index_if_needed(Ref<GLTFState> p_state, Ref<GLTFPhysicsShape> p_gltf_shape) {
	GLTFMeshIndex shape_mesh_index = p_gltf_shape->get_mesh_index();
	if (shape_mesh_index == -1) {
		return; // No mesh for this shape.
	}
	Ref<ImporterMesh> importer_mesh = p_gltf_shape->get_importer_mesh();
	if (importer_mesh.is_valid()) {
		return; // The mesh resource is already set up.
	}
	TypedArray<GLTFMesh> state_meshes = p_state->get_meshes();
	ERR_FAIL_INDEX_MSG(shape_mesh_index, state_meshes.size(), "GLTF Physics: When importing '" + p_state->get_scene_name() + "', the shape mesh index " + itos(shape_mesh_index) + " is not in the state meshes (size: " + itos(state_meshes.size()) + ").");
	Ref<GLTFMesh> gltf_mesh = state_meshes[shape_mesh_index];
	ERR_FAIL_COND(gltf_mesh.is_null());
	importer_mesh = gltf_mesh->get_mesh();
	ERR_FAIL_COND(importer_mesh.is_null());
	p_gltf_shape->set_importer_mesh(importer_mesh);
}

CollisionObject3D *_generate_shape_with_body(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Ref<GLTFPhysicsShape> p_physics_shape, Ref<GLTFPhysicsBody> p_physics_body) {
	print_verbose("glTF: Creating shape with body for: " + p_gltf_node->get_name());
	bool is_trigger = p_physics_shape->get_is_trigger();
	// This method is used for the case where we must generate a parent body.
	// This is can happen for multiple reasons. One possibility is that this
	// GLTF file is using OMI_collider but not OMI_physics_body, or at least
	// this particular node is not using it. Another possibility is that the
	// physics body information is set up on the same GLTF node, not a parent.
	CollisionObject3D *body;
	if (p_physics_body.is_valid()) {
		// This code is run when the physics body is on the same GLTF node.
		body = p_physics_body->to_node();
		if (is_trigger && (p_physics_body->get_body_type() != "trigger")) {
			// Edge case: If the body's trigger and the collider's trigger
			// are in disagreement, we need to create another new body.
			CollisionObject3D *child = _generate_shape_with_body(p_state, p_gltf_node, p_physics_shape, nullptr);
			child->set_name(p_gltf_node->get_name() + (is_trigger ? String("Trigger") : String("Solid")));
			body->add_child(child);
			return body;
		}
	} else if (is_trigger) {
		body = memnew(Area3D);
	} else {
		body = memnew(StaticBody3D);
	}
	CollisionShape3D *shape = p_physics_shape->to_node();
	shape->set_name(p_gltf_node->get_name() + "Shape");
	body->add_child(shape);
	return body;
}

CollisionObject3D *_get_ancestor_collision_object(Node *p_scene_parent) {
	if (p_scene_parent) {
		CollisionObject3D *co = Object::cast_to<CollisionObject3D>(p_scene_parent);
		if (likely(co)) {
			return co;
		}
	}
	return nullptr;
}

Node3D *GLTFDocumentExtensionPhysics::generate_scene_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Node *p_scene_parent) {
	Ref<GLTFPhysicsBody> gltf_physics_body = p_gltf_node->get_additional_data(StringName("GLTFPhysicsBody"));
	Ref<GLTFPhysicsShape> gltf_physics_shape = p_gltf_node->get_additional_data(StringName("GLTFPhysicsShape"));
	if (gltf_physics_shape.is_valid()) {
		_setup_shape_mesh_resource_from_index_if_needed(p_state, gltf_physics_shape);
		// If this GLTF node specifies both a shape and a body, generate both.
		if (gltf_physics_body.is_valid()) {
			return _generate_shape_with_body(p_state, p_gltf_node, gltf_physics_shape, gltf_physics_body);
		}
		CollisionObject3D *ancestor_col_obj = _get_ancestor_collision_object(p_scene_parent);
		if (gltf_physics_shape->get_is_trigger()) {
			// If the shape wants to be a trigger and it already has a
			// trigger parent, we only need to make the shape node.
			if (Object::cast_to<Area3D>(ancestor_col_obj)) {
				return gltf_physics_shape->to_node(true);
			}
		} else if (ancestor_col_obj != nullptr) {
			// If the shape has a valid parent, only make the shape node.
			return gltf_physics_shape->to_node(true);
		}
		// Otherwise, we need to create a new body.
		return _generate_shape_with_body(p_state, p_gltf_node, gltf_physics_shape, nullptr);
	}
	if (gltf_physics_body.is_valid()) {
		return gltf_physics_body->to_node();
	}
	return nullptr;
}

// Export process.
bool _are_all_faces_equal(const Vector<Face3> &p_a, const Vector<Face3> &p_b) {
	if (p_a.size() != p_b.size()) {
		return false;
	}
	for (int i = 0; i < p_a.size(); i++) {
		const Vector3 *a_vertices = p_a[i].vertex;
		const Vector3 *b_vertices = p_b[i].vertex;
		for (int j = 0; j < 3; j++) {
			if (!a_vertices[j].is_equal_approx(b_vertices[j])) {
				return false;
			}
		}
	}
	return true;
}

GLTFMeshIndex _get_or_insert_mesh_in_state(Ref<GLTFState> p_state, Ref<ImporterMesh> p_mesh) {
	ERR_FAIL_COND_V(p_mesh.is_null(), -1);
	TypedArray<GLTFMesh> state_meshes = p_state->get_meshes();
	Vector<Face3> mesh_faces = p_mesh->get_faces();
	// De-duplication: If the state already has the mesh we need, use that one.
	for (GLTFMeshIndex i = 0; i < state_meshes.size(); i++) {
		Ref<GLTFMesh> state_gltf_mesh = state_meshes[i];
		ERR_CONTINUE(state_gltf_mesh.is_null());
		Ref<ImporterMesh> state_importer_mesh = state_gltf_mesh->get_mesh();
		ERR_CONTINUE(state_importer_mesh.is_null());
		if (state_importer_mesh == p_mesh) {
			return i;
		}
		if (_are_all_faces_equal(state_importer_mesh->get_faces(), mesh_faces)) {
			return i;
		}
	}
	// After the loop, we have checked that the mesh is not equal to any of the
	// meshes in the state. So we insert a new mesh into the state mesh array.
	Ref<GLTFMesh> gltf_mesh;
	gltf_mesh.instantiate();
	gltf_mesh->set_mesh(p_mesh);
	GLTFMeshIndex mesh_index = state_meshes.size();
	state_meshes.push_back(gltf_mesh);
	p_state->set_meshes(state_meshes);
	return mesh_index;
}

void GLTFDocumentExtensionPhysics::convert_scene_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Node *p_scene_node) {
	if (cast_to<CollisionShape3D>(p_scene_node)) {
		CollisionShape3D *shape = Object::cast_to<CollisionShape3D>(p_scene_node);
		Ref<GLTFPhysicsShape> collider = GLTFPhysicsShape::from_node(shape);
		{
			Ref<ImporterMesh> importer_mesh = collider->get_importer_mesh();
			if (importer_mesh.is_valid()) {
				collider->set_mesh_index(_get_or_insert_mesh_in_state(p_state, importer_mesh));
			}
		}
		p_gltf_node->set_additional_data(StringName("GLTFPhysicsShape"), collider);
	} else if (cast_to<CollisionObject3D>(p_scene_node)) {
		CollisionObject3D *body = Object::cast_to<CollisionObject3D>(p_scene_node);
		p_gltf_node->set_additional_data(StringName("GLTFPhysicsBody"), GLTFPhysicsBody::from_node(body));
	}
}

Array _get_or_create_state_colliders_in_state(Ref<GLTFState> p_state) {
	Dictionary state_json = p_state->get_json();
	Dictionary state_extensions;
	if (state_json.has("extensions")) {
		state_extensions = state_json["extensions"];
	} else {
		state_json["extensions"] = state_extensions;
	}
	Dictionary omi_collider_ext;
	if (state_extensions.has("OMI_collider")) {
		omi_collider_ext = state_extensions["OMI_collider"];
	} else {
		state_extensions["OMI_collider"] = omi_collider_ext;
		p_state->add_used_extension("OMI_collider");
	}
	Array state_colliders;
	if (omi_collider_ext.has("colliders")) {
		state_colliders = omi_collider_ext["colliders"];
	} else {
		omi_collider_ext["colliders"] = state_colliders;
	}
	return state_colliders;
}

Error GLTFDocumentExtensionPhysics::export_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Dictionary &r_node_json, Node *p_node) {
	Dictionary node_extensions = r_node_json["extensions"];
	Ref<GLTFPhysicsBody> physics_body = p_gltf_node->get_additional_data(StringName("GLTFPhysicsBody"));
	if (physics_body.is_valid()) {
		node_extensions["OMI_physics_body"] = physics_body->to_dictionary();
		p_state->add_used_extension("OMI_physics_body");
	}
	Ref<GLTFPhysicsShape> collider = p_gltf_node->get_additional_data(StringName("GLTFPhysicsShape"));
	if (collider.is_valid()) {
		Array state_colliders = _get_or_create_state_colliders_in_state(p_state);
		int size = state_colliders.size();
		Dictionary omi_collider_ext;
		node_extensions["OMI_collider"] = omi_collider_ext;
		Dictionary collider_dict = collider->to_dictionary();
		for (int i = 0; i < size; i++) {
			Dictionary other = state_colliders[i];
			if (other == collider_dict) {
				// De-duplication: If we already have an identical collider,
				// set the collider index to the existing one and return.
				omi_collider_ext["collider"] = i;
				return OK;
			}
		}
		// If we don't have an identical collider, add it to the array.
		state_colliders.push_back(collider_dict);
		omi_collider_ext["collider"] = size;
	}
	return OK;
}
