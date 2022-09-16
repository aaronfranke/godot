/*************************************************************************/
/*  gltf_document_extension_physics.cpp                                  */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "gltf_document_extension_physics.h"

#include "../../gltf_state.h"
#include "scene/3d/area_3d.h"

Error GLTFDocumentExtensionPhysics::export_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Dictionary &r_json, Node *p_node) {
	Dictionary extra_data = p_gltf_node->get_extra_data();
	Dictionary extensions = r_json["extensions"];
	if (extra_data.has("GLTFCollider")) {
		Ref<GLTFCollider> collider = extra_data["GLTFCollider"];
		if (collider.is_valid()) {
			extensions["OMI_collider"] = collider->to_dictionary();
			p_state->set_extension_as_used("OMI_collider");
		}
	}
	if (extra_data.has("GLTFPhysicsBody")) {
		Ref<GLTFPhysicsBody> physics_body = extra_data["GLTFPhysicsBody"];
		if (physics_body.is_valid()) {
			extensions["OMI_physics_body"] = physics_body->to_dictionary();
			p_state->set_extension_as_used("OMI_physics_body");
		}
	}
	return OK;
}

Node3D *GLTFDocumentExtensionPhysics::_generate_collision(Ref<GLTFNode> p_gltf_node, Node *p_scene_parent, Node *p_scene_owner) {
	Dictionary extra_data = p_gltf_node->get_extra_data();
	Ref<GLTFCollider> collider = extra_data["GLTFCollider"];
	Ref<GLTFPhysicsBody> physics_body = extra_data["GLTFPhysicsBody"];
	print_verbose("glTF: Creating collision for: " + p_gltf_node->get_name());
	bool is_trigger = collider->get_is_trigger();
	if (is_trigger) {
		if (cast_to<Area3D>(p_scene_parent)) {
			return collider->to_node();
		}
	} else {
		if (cast_to<PhysicsBody3D>(p_scene_parent)) {
			return collider->to_node();
		}
	}
	// In this case, the collider does not have a parent physics body or area.
	// This is likely because this GLTF file is using OMI_collider but not
	// OMI_physics_body, or at least this particular node is not using it.
	// Here we create a new physics body and attach the collision shape to it.
	CollisionObject3D *body;
	if (physics_body.is_valid()) {
		body = physics_body->to_node();
	} else if (is_trigger) {
		body = memnew(Area3D);
	} else {
		body = memnew(StaticBody3D);
	}
	CollisionShape3D *child = collider->to_node();
	child->set_name(p_gltf_node->get_name() + "Shape");
	body->add_child(child);
	//child->set_owner(scene_owner);
	return body;
}

Node3D *GLTFDocumentExtensionPhysics::generate_scene_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Node *p_scene_parent, Node *p_scene_owner) {
	Dictionary extra_data = p_gltf_node->get_extra_data();
	if (extra_data.has("GLTFCollider")) {
		Ref<GLTFCollider> collider = extra_data["GLTFCollider"];
		if (collider.is_valid()) {
			return _generate_collision(p_gltf_node, p_scene_parent, p_scene_owner);
		}
	}
	if (extra_data.has("GLTFPhysicsBody")) {
		Ref<GLTFPhysicsBody> physics_body = extra_data["GLTFPhysicsBody"];
		if (physics_body.is_valid()) {
			return physics_body->to_node();
		}
	}
	return nullptr;
}

void GLTFDocumentExtensionPhysics::convert_scene_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Node *p_scene_node) {
	Dictionary extra_data = p_gltf_node->get_extra_data();
	if (cast_to<CollisionShape3D>(p_scene_node)) {
		CollisionShape3D *shape = Object::cast_to<CollisionShape3D>(p_scene_node);
		extra_data["GLTFCollider"] = GLTFCollider::from_node(shape);
	} else if (cast_to<CollisionObject3D>(p_scene_node)) {
		CollisionObject3D *body = Object::cast_to<CollisionObject3D>(p_scene_node);
		extra_data["GLTFPhysicsBody"] = GLTFPhysicsBody::from_node(body);
	}
}

void GLTFDocumentExtensionPhysics::parse_node_extensions(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Dictionary &p_extensions) {
	Dictionary extra_data = p_gltf_node->get_extra_data();
	if (p_extensions.has("OMI_collider")) {
		extra_data["GLTFCollider"] = GLTFCollider::from_dictionary(p_extensions["OMI_collider"]);
	}
	if (p_extensions.has("OMI_physics_body")) {
		extra_data["GLTFPhysicsBody"] = GLTFPhysicsBody::from_dictionary(p_extensions["OMI_physics_body"]);
	}
}
