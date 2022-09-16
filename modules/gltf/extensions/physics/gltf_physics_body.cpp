/*************************************************************************/
/*  gltf_physics_body.cpp                                                */
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

#include "gltf_physics_body.h"

void GLTFPhysicsBody::_bind_methods() {
	ClassDB::bind_static_method("GLTFPhysicsBody", D_METHOD("from_node", "physics_node"), &GLTFPhysicsBody::from_node);
	ClassDB::bind_method(D_METHOD("to_node"), &GLTFPhysicsBody::to_node);

	ClassDB::bind_static_method("GLTFPhysicsBody", D_METHOD("from_dictionary", "dictionary"), &GLTFPhysicsBody::from_dictionary);
	ClassDB::bind_method(D_METHOD("to_dictionary"), &GLTFPhysicsBody::to_dictionary);

	ClassDB::bind_method(D_METHOD("get_body_type"), &GLTFPhysicsBody::get_body_type);
	ClassDB::bind_method(D_METHOD("set_body_type", "body_type"), &GLTFPhysicsBody::set_body_type);
	ClassDB::bind_method(D_METHOD("get_mass"), &GLTFPhysicsBody::get_mass);
	ClassDB::bind_method(D_METHOD("set_mass", "mass"), &GLTFPhysicsBody::set_mass);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "body_type"), "set_body_type", "get_body_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mass"), "set_mass", "get_mass");
}

String GLTFPhysicsBody::get_body_type() const {
	return body_type;
}

void GLTFPhysicsBody::set_body_type(String p_body_type) {
	body_type = p_body_type;
}

real_t GLTFPhysicsBody::get_mass() const {
	return mass;
}

void GLTFPhysicsBody::set_mass(real_t p_mass) {
	mass = p_mass;
}

Ref<GLTFPhysicsBody> GLTFPhysicsBody::from_node(const CollisionObject3D *p_physics_body) {
	Ref<GLTFPhysicsBody> l;
	l.instantiate();
	ERR_FAIL_COND_V_MSG(!p_physics_body, l, "Tried to create a GLTFPhysicsBody from a CollisionObject3D node, but the given node was null.");
	if (cast_to<CharacterBody3D>(p_physics_body)) {
		l->body_type = "character";
	} else if (cast_to<AnimatableBody3D>(p_physics_body)) {
		l->body_type = "kinematic";
	} else if (cast_to<RigidBody3D>(p_physics_body)) {
		l->body_type = "rigid";
		const RigidBody3D *body = cast_to<const RigidBody3D>(p_physics_body);
		l->mass = body->get_mass();
	} else if (cast_to<StaticBody3D>(p_physics_body)) {
		l->body_type = "static";
	} else if (cast_to<Area3D>(p_physics_body)) {
		l->body_type = "trigger";
	}
	return l;
}

CollisionObject3D *GLTFPhysicsBody::to_node() const {
	if (body_type == "character") {
		CharacterBody3D *body = memnew(CharacterBody3D);
		return body;
	}
	if (body_type == "kinematic") {
		AnimatableBody3D *body = memnew(AnimatableBody3D);
		return body;
	}
	if (body_type == "rigid") {
		RigidBody3D *body = memnew(RigidBody3D);
		body->set_mass(mass);
		return body;
	}
	if (body_type == "static") {
		StaticBody3D *body = memnew(StaticBody3D);
		return body;
	}
	if (body_type == "trigger") {
		Area3D *body = memnew(Area3D);
		return body;
	}
	ERR_FAIL_V_MSG(nullptr, "Error converting GLTFPhysicsBody to a node: Body type '" + body_type + "' is unknown.");
}

Ref<GLTFPhysicsBody> GLTFPhysicsBody::from_dictionary(const Dictionary p_dictionary) {
	Ref<GLTFPhysicsBody> physics_body;
	physics_body.instantiate();
	ERR_FAIL_COND_V_MSG(!p_dictionary.has("type"), physics_body, "Failed to parse GLTF physics body, missing required field 'type'.");
	const String &body_type = p_dictionary["type"];
	physics_body->body_type = body_type;

	if (p_dictionary.has("mass")) {
		physics_body->mass = p_dictionary["mass"];
	}
	if (body_type != "character" && body_type != "kinematic" && body_type != "rigid" && body_type != "static" && body_type != "trigger") {
		ERR_PRINT("Error parsing GLTF physics body: Body type '" + body_type + "' is unknown.");
	}
	return physics_body;
}

Dictionary GLTFPhysicsBody::to_dictionary() const {
	Dictionary d;
	d["type"] = body_type;
	if (body_type == "rigid") {
		d["mass"] = mass;
	}
	return d;
}
