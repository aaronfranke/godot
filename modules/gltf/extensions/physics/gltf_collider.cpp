/*************************************************************************/
/*  gltf_collider.cpp                                                    */
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

#include "gltf_collider.h"

#include "scene/resources/box_shape_3d.h"
#include "scene/resources/capsule_shape_3d.h"
#include "scene/resources/cylinder_shape_3d.h"
#include "scene/resources/sphere_shape_3d.h"

void GLTFCollider::_bind_methods() {
	ClassDB::bind_static_method("GLTFCollider", D_METHOD("from_node", "collider_node"), &GLTFCollider::from_node);
	ClassDB::bind_method(D_METHOD("to_node"), &GLTFCollider::to_node);

	ClassDB::bind_static_method("GLTFCollider", D_METHOD("from_dictionary", "dictionary"), &GLTFCollider::from_dictionary);
	ClassDB::bind_method(D_METHOD("to_dictionary"), &GLTFCollider::to_dictionary);

	ClassDB::bind_method(D_METHOD("get_shape_type"), &GLTFCollider::get_shape_type);
	ClassDB::bind_method(D_METHOD("set_shape_type", "shape_type"), &GLTFCollider::set_shape_type);
	ClassDB::bind_method(D_METHOD("get_size"), &GLTFCollider::get_size);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &GLTFCollider::set_size);
	ClassDB::bind_method(D_METHOD("get_radius"), &GLTFCollider::get_radius);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &GLTFCollider::set_radius);
	ClassDB::bind_method(D_METHOD("get_height"), &GLTFCollider::get_height);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &GLTFCollider::set_height);
	ClassDB::bind_method(D_METHOD("get_is_trigger"), &GLTFCollider::get_is_trigger);
	ClassDB::bind_method(D_METHOD("set_is_trigger", "is_trigger"), &GLTFCollider::set_is_trigger);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "shape_type"), "set_shape_type", "get_shape_type");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_trigger"), "set_is_trigger", "get_is_trigger");
}

String GLTFCollider::get_shape_type() const {
	return shape_type;
}

void GLTFCollider::set_shape_type(String p_shape_type) {
	shape_type = p_shape_type;
}

Vector3 GLTFCollider::get_size() const {
	return size;
}

void GLTFCollider::set_size(Vector3 p_size) {
	size = p_size;
}

real_t GLTFCollider::get_radius() const {
	return radius;
}

void GLTFCollider::set_radius(real_t p_radius) {
	radius = p_radius;
}

real_t GLTFCollider::get_height() const {
	return height;
}

void GLTFCollider::set_height(real_t p_height) {
	height = p_height;
}

bool GLTFCollider::get_is_trigger() const {
	return is_trigger;
}

void GLTFCollider::set_is_trigger(bool p_is_trigger) {
	is_trigger = p_is_trigger;
}

Ref<GLTFCollider> GLTFCollider::from_node(const CollisionShape3D *p_collider) {
	Ref<GLTFCollider> l;
	l.instantiate();
	ERR_FAIL_COND_V_MSG(!p_collider, l, "Tried to create a GLTFCollider from a CollisionShape3D node, but the given node was null.");
	Ref<Shape3D> shape = p_collider->get_shape();
	ERR_FAIL_COND_V_MSG(shape.is_null(), l, "Tried to create a GLTFCollider from a CollisionShape3D node, but the given node had a null shape.");
	if (cast_to<BoxShape3D>(shape.ptr())) {
		l->shape_type = "box";
		Ref<BoxShape3D> box = shape;
		l->set_size(box->get_size());
	} else if (cast_to<const CapsuleShape3D>(shape.ptr())) {
		l->shape_type = "capsule";
		Ref<CapsuleShape3D> capsule = shape;
		l->set_radius(capsule->get_radius());
		l->set_height(capsule->get_height());
	} else if (cast_to<const CylinderShape3D>(shape.ptr())) {
		l->shape_type = "cylinder";
		Ref<CylinderShape3D> cylinder = shape;
		l->set_radius(cylinder->get_radius());
		l->set_height(cylinder->get_height());
	} else if (cast_to<const SphereShape3D>(shape.ptr())) {
		l->shape_type = "sphere";
		Ref<SphereShape3D> sphere = shape;
		l->set_radius(sphere->get_radius());
	}
	return l;
}

CollisionShape3D *GLTFCollider::to_node() const {
	CollisionShape3D *collider = memnew(CollisionShape3D);
	if (shape_type == "box") {
		BoxShape3D *shape = memnew(BoxShape3D);
		shape->set_size(size);
		collider->set_shape(shape);
	} else if (shape_type == "capsule") {
		CapsuleShape3D *shape = memnew(CapsuleShape3D);
		shape->set_radius(radius);
		shape->set_height(height);
		collider->set_shape(shape);
	} else if (shape_type == "cylinder") {
		CylinderShape3D *shape = memnew(CylinderShape3D);
		shape->set_radius(radius);
		shape->set_height(height);
		collider->set_shape(shape);
	} else if (shape_type == "sphere") {
		SphereShape3D *shape = memnew(SphereShape3D);
		shape->set_radius(radius);
		collider->set_shape(shape);
	} else {
		ERR_PRINT("Error converting GLTFCollider to a node: Shape type '" + shape_type + "' is unknown.");
	}
	return collider;
}

Ref<GLTFCollider> GLTFCollider::from_dictionary(const Dictionary p_dictionary) {
	ERR_FAIL_COND_V_MSG(!p_dictionary.has("type"), Ref<GLTFCollider>(), "Failed to parse GLTF collider, missing required field 'type'.");
	Ref<GLTFCollider> collider;
	collider.instantiate();
	const String &shape_type = p_dictionary["type"];
	collider->shape_type = shape_type;

	if (p_dictionary.has("radius")) {
		collider->set_radius(p_dictionary["radius"]);
	}
	if (p_dictionary.has("height")) {
		collider->set_height(p_dictionary["height"]);
	}
	if (p_dictionary.has("size")) {
		const Array &arr = p_dictionary["size"];
		if (arr.size() == 3) {
			collider->set_size(Vector3(arr[0], arr[1], arr[2]));
		} else {
			ERR_PRINT("Error parsing GLTF collider: The size must have exactly 3 numbers.");
		}
	}
	if (shape_type != "box" && shape_type != "capsule" && shape_type != "cylinder" && shape_type != "sphere") {
		ERR_PRINT("Error parsing GLTF collider: Shape type '" + shape_type + "' is unknown.");
	}
	if (p_dictionary.has("isTrigger")) {
		collider->set_is_trigger(p_dictionary["isTrigger"]);
	}
	return collider;
}

Dictionary GLTFCollider::to_dictionary() const {
	Dictionary d;
	d["type"] = shape_type;
	if (shape_type == "box") {
		Array size_array;
		size_array.resize(3);
		size_array[0] = size.x;
		size_array[1] = size.y;
		size_array[2] = size.z;
		d["size"] = size_array;
	} else if (shape_type == "capsule") {
		d["radius"] = get_radius();
		d["height"] = get_height();
	} else if (shape_type == "cylinder") {
		d["radius"] = get_radius();
		d["height"] = get_height();
	} else if (shape_type == "sphere") {
		d["radius"] = get_radius();
	}
	if (is_trigger) {
		d["isTrigger"] = is_trigger;
	}
	return d;
}
