/*************************************************************************/
/*  gltf_collider.h                                                      */
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

#ifndef GLTF_COLLIDER_H
#define GLTF_COLLIDER_H

#include "scene/3d/collision_shape_3d.h"

class GLTFCollider : public Resource {
	GDCLASS(GLTFCollider, Resource)
	friend class GLTFDocument;

protected:
	static void _bind_methods();

private:
	String shape_type;
	Vector3 size = Vector3(1.0, 1.0, 1.0);
	real_t radius = 0.5;
	real_t height = 2.0;
	bool is_trigger = false;

public:
	String get_shape_type() const;
	void set_shape_type(String p_shape_type);

	Vector3 get_size() const;
	void set_size(Vector3 p_size);

	real_t get_radius() const;
	void set_radius(real_t p_radius);

	real_t get_height() const;
	void set_height(real_t p_height);

	bool get_is_trigger() const;
	void set_is_trigger(bool p_is_trigger);

	static Ref<GLTFCollider> from_node(const CollisionShape3D *p_collider);
	CollisionShape3D *to_node() const;

	static Ref<GLTFCollider> from_dictionary(const Dictionary p_dictionary);
	Dictionary to_dictionary() const;
};

#endif // GLTF_COLLIDER_H
