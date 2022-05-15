#ifndef NODE4D_H
#define NODE4D_H

#ifdef TOOLS_ENABLED
#define CACHE_ROTATION_AND_SCALE
#endif // TOOLS_ENABLED

#include "modules/4d/math/transform_4d.h"
#include "scene/main/node.h"

class Node4D : public Node {
	GDCLASS(Node4D, Node);

	Transform4D _transform;
#ifdef CACHE_ROTATION_AND_SCALE
	Euler4D _euler_cache;
	bool _euler_cache_dirty = true;
	Vector4 _scale_cache;
	bool _scale_cache_dirty = true;
#endif // CACHE_ROTATION_AND_SCALE

	void _update_transform();

protected:
	static void _bind_methods();

public:
	enum RotationEditMode {
		ROTATION_EDIT_MODE_EULER,
		ROTATION_EDIT_MODE_OCTONION,
		ROTATION_EDIT_MODE_BASIS,
	};

#ifdef CACHE_ROTATION_AND_SCALE
	RotationEditMode rotation_edit_mode;
#endif // CACHE_ROTATION_AND_SCALE

	Vector4 get_position() const;
	void set_position(const Vector4 &p_position);

#ifdef CACHE_ROTATION_AND_SCALE
	Vector4 get_scale();
	Euler4D get_rotation();
#else
	Vector4 get_scale() const;
	Euler4D get_rotation() const;
#endif // CACHE_ROTATION_AND_SCALE
	void set_scale(const Vector4 &p_scale);
	void set_rotation(const Euler4D &p_euler);

	Basis4D get_basis() const;
	void set_basis(const Basis4D &p_basis);

	Transform4D get_transform() const;
	void set_transform(const Transform4D &p_transform);

	void translate_local(const Vector4 &p_amount);
	void apply_scale(const Vector4 &p_amount);

	/*
		Vector4 get_global_position() const;
		void set_global_position(const Vector4 &p_global_position);
		Vector4 get_global_scale() const;
		void set_global_scale(const Vector4 &p_global_scale);
	*/
};

VARIANT_ENUM_CAST(Node4D::RotationEditMode)

#endif // NODE4D_H
