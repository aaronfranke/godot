#include "node_4d.h"

Vector4 Node4D::get_position() const {
	return _transform.get_origin();
}

void Node4D::set_position(const Vector4 &p_position) {
	_transform.set_origin(p_position);
}

#ifdef CACHE_ROTATION_AND_SCALE
Vector4 Node4D::get_scale() {
	if (_scale_cache_dirty) {
		_scale_cache = _transform.get_scale();
		_scale_cache_dirty = false;
	}
	return _scale_cache;
}
#else
Vector4 Node4D::get_scale() const {
	return _transform.get_scale();
}
#endif // CACHE_ROTATION_AND_SCALE

void Node4D::set_scale(const Vector4 &p_scale) {
#ifdef CACHE_ROTATION_AND_SCALE
	_scale_cache = p_scale;
#endif // CACHE_ROTATION_AND_SCALE
	_transform.set_scale(p_scale);
}

#ifdef CACHE_ROTATION_AND_SCALE
Euler4D Node4D::get_rotation() {
	if (_euler_cache_dirty) {
		_euler_cache = Euler4D::from_basis(_transform.basis);
		_euler_cache_dirty = false;
	}
	return _euler_cache;
}
#else
Euler4D Node4D::get_rotation() const {
	return Euler4D::from_basis(_transform.basis);
}
#endif // CACHE_ROTATION_AND_SCALE

void Node4D::set_rotation(const Euler4D &p_euler) {
#ifdef CACHE_ROTATION_AND_SCALE
	_euler_cache = p_euler;
	_transform.basis = p_euler.to_basis().scaled_local(_scale_cache);
#else
	Vector4 scale = _transform.basis.get_scale();
	_transform.basis = p_euler.to_basis().scaled_local(scale);
#endif // CACHE_ROTATION_AND_SCALE
}

Basis4D Node4D::get_basis() const {
	return _transform.basis;
}

void Node4D::set_basis(const Basis4D &p_basis) {
	_transform.basis = p_basis;
#ifdef CACHE_ROTATION_AND_SCALE
	_euler_cache_dirty = true;
	_scale_cache_dirty = true;
#endif // CACHE_ROTATION_AND_SCALE
}

Transform4D Node4D::get_transform() const {
	return _transform;
}

void Node4D::set_transform(const Transform4D &p_transform) {
	_transform = p_transform;
#ifdef CACHE_ROTATION_AND_SCALE
	_euler_cache_dirty = true;
	_scale_cache_dirty = true;
#endif // CACHE_ROTATION_AND_SCALE
}

void Node4D::translate_local(const Vector4 &p_amount) {
	_transform.translate_local(p_amount);
}

void Node4D::apply_scale(const Vector4 &p_amount) {
	_transform.scale(p_amount);
}

/*
real_t Node4D::get_global_position() const {
	Node4D *node_1d_parent = Object::cast_to<Node4D>(get_parent());
	if (node_1d_parent) {
		return node_1d_parent->get_global_position() + _position * node_1d_parent->get_global_scale();
	}
	return _position;
}

real_t Node4D::get_global_scale() const {
	Node4D *node_1d_parent = Object::cast_to<Node4D>(get_parent());
	if (node_1d_parent) {
		return _scale * node_1d_parent->get_global_scale();
	}
	return _scale;
}

void Node4D::set_global_position(const real_t p_global_position) {
	Node4D *node_1d_parent = Object::cast_to<Node4D>(get_parent());
	if (node_1d_parent) {
		set_position((p_global_position - node_1d_parent->get_global_position()) / node_1d_parent->get_global_scale());
	} else {
		set_position(p_global_position);
	}
}

void Node4D::set_global_scale(const real_t p_global_scale) {
	Node4D *node_1d_parent = Object::cast_to<Node4D>(get_parent());
	if (node_1d_parent) {
		set_scale(p_global_scale / node_1d_parent->get_global_scale());
	} else {
		set_scale(p_global_scale);
	}
}*/

void Node4D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_position"), &Node4D::get_position);
	ClassDB::bind_method(D_METHOD("set_position", "position"), &Node4D::set_position);

	ClassDB::bind_method(D_METHOD("get_scale"), &Node4D::get_scale);
	ClassDB::bind_method(D_METHOD("set_scale", "scale"), &Node4D::set_scale);

	ClassDB::bind_method(D_METHOD("get_rotation"), &Node4D::get_rotation);
	ClassDB::bind_method(D_METHOD("set_rotation", "euler"), &Node4D::set_rotation);

	ClassDB::bind_method(D_METHOD("get_basis"), &Node4D::get_basis);
	ClassDB::bind_method(D_METHOD("set_basis", "basis"), &Node4D::set_basis);

	ClassDB::bind_method(D_METHOD("get_transform"), &Node4D::get_transform);
	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &Node4D::set_transform);

	ClassDB::bind_method(D_METHOD("translate_local", "offset"), &Node4D::translate_local);
	ClassDB::bind_method(D_METHOD("apply_scale", "ratio"), &Node4D::apply_scale);

	//ClassDB::bind_method(D_METHOD("get_global_position"), &Node4D::get_global_position);
	//ClassDB::bind_method(D_METHOD("get_global_scale"), &Node4D::get_global_scale);
	//ClassDB::bind_method(D_METHOD("set_global_position", "global_position"), &Node4D::set_global_position);
	//ClassDB::bind_method(D_METHOD("set_global_scale", "global_scale"), &Node4D::set_global_scale);

	//ADD_PROPERTY( PropertyInfo(Variant::TRANSFORM3D,"transform/global",PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR ), "set_global_transform", "get_global_transform") ;
	ADD_GROUP("Transform", "");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM4D, "transform", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_transform", "get_transform");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR4, "position", PROPERTY_HINT_RANGE, "-99999,99999,0.001,or_greater,or_lesser,noslider,suffix:m", PROPERTY_USAGE_EDITOR), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::EULER4D, "rotation", PROPERTY_HINT_RANGE, "-180,180,0.1,or_lesser,or_greater,radians", PROPERTY_USAGE_EDITOR), "set_rotation", "get_rotation");
	//ADD_PROPERTY(PropertyInfo(Variant::OCTONION, "octonion", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_octonion", "get_octonion");
	ADD_PROPERTY(PropertyInfo(Variant::BASIS4D, "basis", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_basis", "get_basis");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR4, "scale", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_scale", "get_scale");
	//ADD_PROPERTY(PropertyInfo(Variant::INT, "rotation_edit_mode", PROPERTY_HINT_ENUM, "Euler4D,Octonion,Basis"), "set_rotation_edit_mode", "get_rotation_edit_mode");

	// Global transform properties.
	//ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM4D, "global_transform", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_global_transform", "get_global_transform");
	//ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "global_position", PROPERTY_HINT_RANGE, "-99999,99999,0.001,or_lesser,or_greater,noslider,suffix:px", 0), "set_global_position", "get_global_position");
	//ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "global_scale", PROPERTY_HINT_NONE, "", 0), "set_global_scale", "get_global_scale");
}
