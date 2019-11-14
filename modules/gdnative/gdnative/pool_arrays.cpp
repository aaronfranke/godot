/*************************************************************************/
/*  pool_arrays.cpp                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "gdnative/pool_arrays.h"

#include "core/array.h"
#include "core/pool_vector.h"
#include "core/variant.h"

#include "core/color.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"

#ifdef __cplusplus
extern "C" {
#endif

#define memnew_placement_custom(m_placement, m_class, m_constr) _post_initialize(new (m_placement, sizeof(m_class), "") m_constr)

// byte

void GDAPI godot_pool_byte_array_new(godot_pool_byte_array *r_dest) {
	PoolByteArray *dest = (PoolByteArray *)r_dest;
	memnew_placement(dest, PoolByteArray);
}

void GDAPI godot_pool_byte_array_new_copy(godot_pool_byte_array *r_dest, const godot_pool_byte_array *p_src) {
	PoolByteArray *dest = (PoolByteArray *)r_dest;
	const PoolByteArray *src = (const PoolByteArray *)p_src;
	memnew_placement(dest, PoolByteArray(*src));
}

void GDAPI godot_pool_byte_array_new_with_array(godot_pool_byte_array *r_dest, const godot_array *p_a) {
	PoolByteArray *dest = (PoolByteArray *)r_dest;
	Array *a = (Array *)p_a;
	memnew_placement(dest, PoolByteArray);

	dest->resize(a->size());
	for (int i = 0; i < a->size(); i++) {
		dest->set(i, (*a)[i]);
	}
}

void GDAPI godot_pool_byte_array_append(godot_pool_byte_array *p_self, const uint8_t p_data) {
	PoolByteArray *self = (PoolByteArray *)p_self;
	self->append(p_data);
}

void GDAPI godot_pool_byte_array_append_array(godot_pool_byte_array *p_self, const godot_pool_byte_array *p_array) {
	PoolByteArray *self = (PoolByteArray *)p_self;
	PoolByteArray *array = (PoolByteArray *)p_array;
	self->append_array(*array);
}

godot_error GDAPI godot_pool_byte_array_insert(godot_pool_byte_array *p_self, const godot_int p_idx, const uint8_t p_data) {
	PoolByteArray *self = (PoolByteArray *)p_self;
	return (godot_error)self->insert(p_idx, p_data);
}

void GDAPI godot_pool_byte_array_invert(godot_pool_byte_array *p_self) {
	PoolByteArray *self = (PoolByteArray *)p_self;
	self->invert();
}

void GDAPI godot_pool_byte_array_push_back(godot_pool_byte_array *p_self, const uint8_t p_data) {
	PoolByteArray *self = (PoolByteArray *)p_self;
	self->push_back(p_data);
}

void GDAPI godot_pool_byte_array_remove(godot_pool_byte_array *p_self, const godot_int p_idx) {
	PoolByteArray *self = (PoolByteArray *)p_self;
	self->remove(p_idx);
}

void GDAPI godot_pool_byte_array_resize(godot_pool_byte_array *p_self, const godot_int p_size) {
	PoolByteArray *self = (PoolByteArray *)p_self;
	self->resize(p_size);
}

godot_pool_byte_array_read_access GDAPI *godot_pool_byte_array_read(const godot_pool_byte_array *p_self) {
	const PoolByteArray *self = (const PoolByteArray *)p_self;
	return (godot_pool_byte_array_read_access *)memnew(PoolByteArray::Read(self->read()));
}

godot_pool_byte_array_write_access GDAPI *godot_pool_byte_array_write(godot_pool_byte_array *p_self) {
	PoolByteArray *self = (PoolByteArray *)p_self;
	return (godot_pool_byte_array_write_access *)memnew(PoolByteArray::Write(self->write()));
}

void GDAPI godot_pool_byte_array_set(godot_pool_byte_array *p_self, const godot_int p_idx, const uint8_t p_data) {
	PoolByteArray *self = (PoolByteArray *)p_self;
	self->set(p_idx, p_data);
}

uint8_t GDAPI godot_pool_byte_array_get(const godot_pool_byte_array *p_self, const godot_int p_idx) {
	const PoolByteArray *self = (const PoolByteArray *)p_self;
	return self->get(p_idx);
}

godot_int GDAPI godot_pool_byte_array_size(const godot_pool_byte_array *p_self) {
	const PoolByteArray *self = (const PoolByteArray *)p_self;
	return self->size();
}

godot_bool GDAPI godot_pool_byte_array_empty(const godot_pool_byte_array *p_self) {
	const PoolByteArray *self = (const PoolByteArray *)p_self;
	return self->empty();
}

void GDAPI godot_pool_byte_array_destroy(godot_pool_byte_array *p_self) {
	((PoolByteArray *)p_self)->~PoolVector();
}

// int

void GDAPI godot_pool_int_array_new(godot_pool_int_array *r_dest) {
	PoolVector<godot_int> *dest = (PoolVector<godot_int> *)r_dest;
	memnew_placement(dest, PoolVector<godot_int>);
}

void GDAPI godot_pool_int_array_new_copy(godot_pool_int_array *r_dest, const godot_pool_int_array *p_src) {
	PoolVector<godot_int> *dest = (PoolVector<godot_int> *)r_dest;
	const PoolVector<godot_int> *src = (const PoolVector<godot_int> *)p_src;
	memnew_placement(dest, PoolVector<godot_int>(*src));
}

void GDAPI godot_pool_int_array_new_with_array(godot_pool_int_array *r_dest, const godot_array *p_a) {
	PoolVector<godot_int> *dest = (PoolVector<godot_int> *)r_dest;
	Array *a = (Array *)p_a;
	memnew_placement(dest, PoolVector<godot_int>);

	dest->resize(a->size());
	for (int i = 0; i < a->size(); i++) {
		dest->set(i, (*a)[i]);
	}
}

void GDAPI godot_pool_int_array_append(godot_pool_int_array *p_self, const godot_int p_data) {
	PoolVector<godot_int> *self = (PoolVector<godot_int> *)p_self;
	self->append(p_data);
}

void GDAPI godot_pool_int_array_append_array(godot_pool_int_array *p_self, const godot_pool_int_array *p_array) {
	PoolVector<godot_int> *self = (PoolVector<godot_int> *)p_self;
	PoolVector<godot_int> *array = (PoolVector<godot_int> *)p_array;
	self->append_array(*array);
}

godot_error GDAPI godot_pool_int_array_insert(godot_pool_int_array *p_self, const godot_int p_idx, const godot_int p_data) {
	PoolVector<godot_int> *self = (PoolVector<godot_int> *)p_self;
	return (godot_error)self->insert(p_idx, p_data);
}

void GDAPI godot_pool_int_array_invert(godot_pool_int_array *p_self) {
	PoolVector<godot_int> *self = (PoolVector<godot_int> *)p_self;
	self->invert();
}

void GDAPI godot_pool_int_array_push_back(godot_pool_int_array *p_self, const godot_int p_data) {
	PoolVector<godot_int> *self = (PoolVector<godot_int> *)p_self;
	self->push_back(p_data);
}

void GDAPI godot_pool_int_array_remove(godot_pool_int_array *p_self, const godot_int p_idx) {
	PoolVector<godot_int> *self = (PoolVector<godot_int> *)p_self;
	self->remove(p_idx);
}

void GDAPI godot_pool_int_array_resize(godot_pool_int_array *p_self, const godot_int p_size) {
	PoolVector<godot_int> *self = (PoolVector<godot_int> *)p_self;
	self->resize(p_size);
}

godot_pool_int_array_read_access GDAPI *godot_pool_int_array_read(const godot_pool_int_array *p_self) {
	const PoolVector<godot_int> *self = (const PoolVector<godot_int> *)p_self;
	return (godot_pool_int_array_read_access *)memnew(PoolVector<godot_int>::Read(self->read()));
}

godot_pool_int_array_write_access GDAPI *godot_pool_int_array_write(godot_pool_int_array *p_self) {
	PoolVector<godot_int> *self = (PoolVector<godot_int> *)p_self;
	return (godot_pool_int_array_write_access *)memnew(PoolVector<godot_int>::Write(self->write()));
}

void GDAPI godot_pool_int_array_set(godot_pool_int_array *p_self, const godot_int p_idx, const godot_int p_data) {
	PoolVector<godot_int> *self = (PoolVector<godot_int> *)p_self;
	self->set(p_idx, p_data);
}

godot_int GDAPI godot_pool_int_array_get(const godot_pool_int_array *p_self, const godot_int p_idx) {
	const PoolVector<godot_int> *self = (const PoolVector<godot_int> *)p_self;
	return self->get(p_idx);
}

godot_int GDAPI godot_pool_int_array_size(const godot_pool_int_array *p_self) {
	const PoolVector<godot_int> *self = (const PoolVector<godot_int> *)p_self;
	return self->size();
}

godot_bool GDAPI godot_pool_int_array_empty(const godot_pool_int_array *p_self) {
	const PoolVector<godot_int> *self = (const PoolVector<godot_int> *)p_self;
	return self->empty();
}

void GDAPI godot_pool_int_array_destroy(godot_pool_int_array *p_self) {
	((PoolVector<godot_int> *)p_self)->~PoolVector();
}

// real

void GDAPI godot_pool_real_array_new(godot_pool_real_array *r_dest) {
	PoolVector<godot_real> *dest = (PoolVector<godot_real> *)r_dest;
	memnew_placement(dest, PoolVector<godot_real>);
}

void GDAPI godot_pool_real_array_new_copy(godot_pool_real_array *r_dest, const godot_pool_real_array *p_src) {
	PoolVector<godot_real> *dest = (PoolVector<godot_real> *)r_dest;
	const PoolVector<godot_real> *src = (const PoolVector<godot_real> *)p_src;
	memnew_placement(dest, PoolVector<godot_real>(*src));
}

void GDAPI godot_pool_real_array_new_with_array(godot_pool_real_array *r_dest, const godot_array *p_a) {
	PoolVector<godot_real> *dest = (PoolVector<godot_real> *)r_dest;
	Array *a = (Array *)p_a;
	memnew_placement(dest, PoolVector<godot_real>);

	dest->resize(a->size());
	for (int i = 0; i < a->size(); i++) {
		dest->set(i, (*a)[i]);
	}
}

void GDAPI godot_pool_real_array_append(godot_pool_real_array *p_self, const godot_real p_data) {
	PoolVector<godot_real> *self = (PoolVector<godot_real> *)p_self;
	self->append(p_data);
}

void GDAPI godot_pool_real_array_append_array(godot_pool_real_array *p_self, const godot_pool_real_array *p_array) {
	PoolVector<godot_real> *self = (PoolVector<godot_real> *)p_self;
	PoolVector<godot_real> *array = (PoolVector<godot_real> *)p_array;
	self->append_array(*array);
}

godot_error GDAPI godot_pool_real_array_insert(godot_pool_real_array *p_self, const godot_int p_idx, const godot_real p_data) {
	PoolVector<godot_real> *self = (PoolVector<godot_real> *)p_self;
	return (godot_error)self->insert(p_idx, p_data);
}

void GDAPI godot_pool_real_array_invert(godot_pool_real_array *p_self) {
	PoolVector<godot_real> *self = (PoolVector<godot_real> *)p_self;
	self->invert();
}

void GDAPI godot_pool_real_array_push_back(godot_pool_real_array *p_self, const godot_real p_data) {
	PoolVector<godot_real> *self = (PoolVector<godot_real> *)p_self;
	self->push_back(p_data);
}

void GDAPI godot_pool_real_array_remove(godot_pool_real_array *p_self, const godot_int p_idx) {
	PoolVector<godot_real> *self = (PoolVector<godot_real> *)p_self;
	self->remove(p_idx);
}

void GDAPI godot_pool_real_array_resize(godot_pool_real_array *p_self, const godot_int p_size) {
	PoolVector<godot_real> *self = (PoolVector<godot_real> *)p_self;
	self->resize(p_size);
}

godot_pool_real_array_read_access GDAPI *godot_pool_real_array_read(const godot_pool_real_array *p_self) {
	const PoolVector<godot_real> *self = (const PoolVector<godot_real> *)p_self;
	return (godot_pool_real_array_read_access *)memnew(PoolVector<godot_real>::Read(self->read()));
}

godot_pool_int_array_write_access GDAPI *godot_pool_real_array_write(godot_pool_real_array *p_self) {
	PoolVector<godot_real> *self = (PoolVector<godot_real> *)p_self;
	return (godot_pool_real_array_write_access *)memnew(PoolVector<godot_real>::Write(self->write()));
}

void GDAPI godot_pool_real_array_set(godot_pool_real_array *p_self, const godot_int p_idx, const godot_real p_data) {
	PoolVector<godot_real> *self = (PoolVector<godot_real> *)p_self;
	self->set(p_idx, p_data);
}

godot_real GDAPI godot_pool_real_array_get(const godot_pool_real_array *p_self, const godot_int p_idx) {
	const PoolVector<godot_real> *self = (const PoolVector<godot_real> *)p_self;
	return self->get(p_idx);
}

godot_int GDAPI godot_pool_real_array_size(const godot_pool_real_array *p_self) {
	const PoolVector<godot_real> *self = (const PoolVector<godot_real> *)p_self;
	return self->size();
}

godot_bool GDAPI godot_pool_real_array_empty(const godot_pool_real_array *p_self) {
	const PoolVector<godot_real> *self = (const PoolVector<godot_real> *)p_self;
	return self->empty();
}

void GDAPI godot_pool_real_array_destroy(godot_pool_real_array *p_self) {
	((PoolVector<godot_real> *)p_self)->~PoolVector();
}

// string

void GDAPI godot_pool_string_array_new(godot_pool_string_array *r_dest) {
	PoolStringArray *dest = (PoolStringArray *)r_dest;
	memnew_placement(dest, PoolStringArray);
}

void GDAPI godot_pool_string_array_new_copy(godot_pool_string_array *r_dest, const godot_pool_string_array *p_src) {
	PoolStringArray *dest = (PoolStringArray *)r_dest;
	const PoolStringArray *src = (const PoolStringArray *)p_src;
	memnew_placement(dest, PoolStringArray(*src));
}

void GDAPI godot_pool_string_array_new_with_array(godot_pool_string_array *r_dest, const godot_array *p_a) {
	PoolStringArray *dest = (PoolStringArray *)r_dest;
	Array *a = (Array *)p_a;
	memnew_placement(dest, PoolStringArray);

	dest->resize(a->size());
	for (int i = 0; i < a->size(); i++) {
		dest->set(i, (*a)[i]);
	}
}

void GDAPI godot_pool_string_array_append(godot_pool_string_array *p_self, const godot_string *p_data) {
	PoolStringArray *self = (PoolStringArray *)p_self;
	String &s = *(String *)p_data;
	self->append(s);
}

void GDAPI godot_pool_string_array_append_array(godot_pool_string_array *p_self, const godot_pool_string_array *p_array) {
	PoolStringArray *self = (PoolStringArray *)p_self;
	PoolStringArray *array = (PoolStringArray *)p_array;
	self->append_array(*array);
}

godot_error GDAPI godot_pool_string_array_insert(godot_pool_string_array *p_self, const godot_int p_idx, const godot_string *p_data) {
	PoolStringArray *self = (PoolStringArray *)p_self;
	String &s = *(String *)p_data;
	return (godot_error)self->insert(p_idx, s);
}

void GDAPI godot_pool_string_array_invert(godot_pool_string_array *p_self) {
	PoolStringArray *self = (PoolStringArray *)p_self;
	self->invert();
}

void GDAPI godot_pool_string_array_push_back(godot_pool_string_array *p_self, const godot_string *p_data) {
	PoolStringArray *self = (PoolStringArray *)p_self;
	String &s = *(String *)p_data;
	self->push_back(s);
}

void GDAPI godot_pool_string_array_remove(godot_pool_string_array *p_self, const godot_int p_idx) {
	PoolStringArray *self = (PoolStringArray *)p_self;
	self->remove(p_idx);
}

void GDAPI godot_pool_string_array_resize(godot_pool_string_array *p_self, const godot_int p_size) {
	PoolStringArray *self = (PoolStringArray *)p_self;
	self->resize(p_size);
}

godot_pool_string_array_read_access GDAPI *godot_pool_string_array_read(const godot_pool_string_array *p_self) {
	const PoolStringArray *self = (const PoolStringArray *)p_self;
	return (godot_pool_string_array_read_access *)memnew(PoolStringArray::Read(self->read()));
}

godot_pool_string_array_write_access GDAPI *godot_pool_string_array_write(godot_pool_string_array *p_self) {
	PoolStringArray *self = (PoolStringArray *)p_self;
	return (godot_pool_string_array_write_access *)memnew(PoolStringArray::Write(self->write()));
}

void GDAPI godot_pool_string_array_set(godot_pool_string_array *p_self, const godot_int p_idx, const godot_string *p_data) {
	PoolStringArray *self = (PoolStringArray *)p_self;
	String &s = *(String *)p_data;
	self->set(p_idx, s);
}

godot_string GDAPI godot_pool_string_array_get(const godot_pool_string_array *p_self, const godot_int p_idx) {
	const PoolStringArray *self = (const PoolStringArray *)p_self;
	godot_string str;
	String *s = (String *)&str;
	memnew_placement(s, String);
	*s = self->get(p_idx);
	return str;
}

godot_int GDAPI godot_pool_string_array_size(const godot_pool_string_array *p_self) {
	const PoolStringArray *self = (const PoolStringArray *)p_self;
	return self->size();
}

godot_bool GDAPI godot_pool_string_array_empty(const godot_pool_string_array *p_self) {
	const PoolStringArray *self = (const PoolStringArray *)p_self;
	return self->empty();
}

void GDAPI godot_pool_string_array_destroy(godot_pool_string_array *p_self) {
	((PoolStringArray *)p_self)->~PoolVector();
}

// vector2

void GDAPI godot_pool_vector2_array_new(godot_pool_vector2_array *r_dest) {
	PoolVector2Array *dest = (PoolVector2Array *)r_dest;
	memnew_placement(dest, PoolVector2Array);
}

void GDAPI godot_pool_vector2_array_new_copy(godot_pool_vector2_array *r_dest, const godot_pool_vector2_array *p_src) {
	PoolVector2Array *dest = (PoolVector2Array *)r_dest;
	const PoolVector2Array *src = (const PoolVector2Array *)p_src;
	memnew_placement(dest, PoolVector2Array(*src));
}

void GDAPI godot_pool_vector2_array_new_with_array(godot_pool_vector2_array *r_dest, const godot_array *p_a) {
	PoolVector2Array *dest = (PoolVector2Array *)r_dest;
	Array *a = (Array *)p_a;
	memnew_placement(dest, PoolVector2Array);

	dest->resize(a->size());
	for (int i = 0; i < a->size(); i++) {
		dest->set(i, (*a)[i]);
	}
}

void GDAPI godot_pool_vector2_array_append(godot_pool_vector2_array *p_self, const godot_vector2 *p_data) {
	PoolVector2Array *self = (PoolVector2Array *)p_self;
	Vector2 &s = *(Vector2 *)p_data;
	self->append(s);
}

void GDAPI godot_pool_vector2_array_append_array(godot_pool_vector2_array *p_self, const godot_pool_vector2_array *p_array) {
	PoolVector2Array *self = (PoolVector2Array *)p_self;
	PoolVector2Array *array = (PoolVector2Array *)p_array;
	self->append_array(*array);
}

godot_error GDAPI godot_pool_vector2_array_insert(godot_pool_vector2_array *p_self, const godot_int p_idx, const godot_vector2 *p_data) {
	PoolVector2Array *self = (PoolVector2Array *)p_self;
	Vector2 &s = *(Vector2 *)p_data;
	return (godot_error)self->insert(p_idx, s);
}

void GDAPI godot_pool_vector2_array_invert(godot_pool_vector2_array *p_self) {
	PoolVector2Array *self = (PoolVector2Array *)p_self;
	self->invert();
}

void GDAPI godot_pool_vector2_array_push_back(godot_pool_vector2_array *p_self, const godot_vector2 *p_data) {
	PoolVector2Array *self = (PoolVector2Array *)p_self;
	Vector2 &s = *(Vector2 *)p_data;
	self->push_back(s);
}

void GDAPI godot_pool_vector2_array_remove(godot_pool_vector2_array *p_self, const godot_int p_idx) {
	PoolVector2Array *self = (PoolVector2Array *)p_self;
	self->remove(p_idx);
}

void GDAPI godot_pool_vector2_array_resize(godot_pool_vector2_array *p_self, const godot_int p_size) {
	PoolVector2Array *self = (PoolVector2Array *)p_self;
	self->resize(p_size);
}

godot_pool_vector2_array_read_access GDAPI *godot_pool_vector2_array_read(const godot_pool_vector2_array *p_self) {
	const PoolVector2Array *self = (const PoolVector2Array *)p_self;
	return (godot_pool_vector2_array_read_access *)memnew(PoolVector2Array::Read(self->read()));
}

godot_pool_vector2_array_write_access GDAPI *godot_pool_vector2_array_write(godot_pool_vector2_array *p_self) {
	PoolVector2Array *self = (PoolVector2Array *)p_self;
	return (godot_pool_vector2_array_write_access *)memnew(PoolVector2Array::Write(self->write()));
}

void GDAPI godot_pool_vector2_array_set(godot_pool_vector2_array *p_self, const godot_int p_idx, const godot_vector2 *p_data) {
	PoolVector2Array *self = (PoolVector2Array *)p_self;
	Vector2 &s = *(Vector2 *)p_data;
	self->set(p_idx, s);
}

godot_vector2 GDAPI godot_pool_vector2_array_get(const godot_pool_vector2_array *p_self, const godot_int p_idx) {
	const PoolVector2Array *self = (const PoolVector2Array *)p_self;
	godot_vector2 v;
	Vector2 *s = (Vector2 *)&v;
	*s = self->get(p_idx);
	return v;
}

godot_int GDAPI godot_pool_vector2_array_size(const godot_pool_vector2_array *p_self) {
	const PoolVector2Array *self = (const PoolVector2Array *)p_self;
	return self->size();
}

godot_bool GDAPI godot_pool_vector2_array_empty(const godot_pool_vector2_array *p_self) {
	const PoolVector2Array *self = (const PoolVector2Array *)p_self;
	return self->empty();
}

void GDAPI godot_pool_vector2_array_destroy(godot_pool_vector2_array *p_self) {
	((PoolVector2Array *)p_self)->~PoolVector();
}

// vector3

void GDAPI godot_pool_vector3_array_new(godot_pool_vector3_array *r_dest) {
	PoolVector3Array *dest = (PoolVector3Array *)r_dest;
	memnew_placement(dest, PoolVector3Array);
}

void GDAPI godot_pool_vector3_array_new_copy(godot_pool_vector3_array *r_dest, const godot_pool_vector3_array *p_src) {
	PoolVector3Array *dest = (PoolVector3Array *)r_dest;
	const PoolVector3Array *src = (const PoolVector3Array *)p_src;
	memnew_placement(dest, PoolVector3Array(*src));
}

void GDAPI godot_pool_vector3_array_new_with_array(godot_pool_vector3_array *r_dest, const godot_array *p_a) {
	PoolVector3Array *dest = (PoolVector3Array *)r_dest;
	Array *a = (Array *)p_a;
	memnew_placement(dest, PoolVector3Array);

	dest->resize(a->size());
	for (int i = 0; i < a->size(); i++) {
		dest->set(i, (*a)[i]);
	}
}

void GDAPI godot_pool_vector3_array_append(godot_pool_vector3_array *p_self, const godot_vector3 *p_data) {
	PoolVector3Array *self = (PoolVector3Array *)p_self;
	Vector3 &s = *(Vector3 *)p_data;
	self->append(s);
}

void GDAPI godot_pool_vector3_array_append_array(godot_pool_vector3_array *p_self, const godot_pool_vector3_array *p_array) {
	PoolVector3Array *self = (PoolVector3Array *)p_self;
	PoolVector3Array *array = (PoolVector3Array *)p_array;
	self->append_array(*array);
}

godot_error GDAPI godot_pool_vector3_array_insert(godot_pool_vector3_array *p_self, const godot_int p_idx, const godot_vector3 *p_data) {
	PoolVector3Array *self = (PoolVector3Array *)p_self;
	Vector3 &s = *(Vector3 *)p_data;
	return (godot_error)self->insert(p_idx, s);
}

void GDAPI godot_pool_vector3_array_invert(godot_pool_vector3_array *p_self) {
	PoolVector3Array *self = (PoolVector3Array *)p_self;
	self->invert();
}

void GDAPI godot_pool_vector3_array_push_back(godot_pool_vector3_array *p_self, const godot_vector3 *p_data) {
	PoolVector3Array *self = (PoolVector3Array *)p_self;
	Vector3 &s = *(Vector3 *)p_data;
	self->push_back(s);
}

void GDAPI godot_pool_vector3_array_remove(godot_pool_vector3_array *p_self, const godot_int p_idx) {
	PoolVector3Array *self = (PoolVector3Array *)p_self;
	self->remove(p_idx);
}

void GDAPI godot_pool_vector3_array_resize(godot_pool_vector3_array *p_self, const godot_int p_size) {
	PoolVector3Array *self = (PoolVector3Array *)p_self;
	self->resize(p_size);
}

godot_pool_vector3_array_read_access GDAPI *godot_pool_vector3_array_read(const godot_pool_vector3_array *p_self) {
	const PoolVector3Array *self = (const PoolVector3Array *)p_self;
	return (godot_pool_vector3_array_read_access *)memnew(PoolVector3Array::Read(self->read()));
}

godot_pool_vector3_array_write_access GDAPI *godot_pool_vector3_array_write(godot_pool_vector3_array *p_self) {
	PoolVector3Array *self = (PoolVector3Array *)p_self;
	return (godot_pool_vector3_array_write_access *)memnew(PoolVector3Array::Write(self->write()));
}

void GDAPI godot_pool_vector3_array_set(godot_pool_vector3_array *p_self, const godot_int p_idx, const godot_vector3 *p_data) {
	PoolVector3Array *self = (PoolVector3Array *)p_self;
	Vector3 &s = *(Vector3 *)p_data;
	self->set(p_idx, s);
}

godot_vector3 GDAPI godot_pool_vector3_array_get(const godot_pool_vector3_array *p_self, const godot_int p_idx) {
	const PoolVector3Array *self = (const PoolVector3Array *)p_self;
	godot_vector3 v;
	Vector3 *s = (Vector3 *)&v;
	*s = self->get(p_idx);
	return v;
}

godot_int GDAPI godot_pool_vector3_array_size(const godot_pool_vector3_array *p_self) {
	const PoolVector3Array *self = (const PoolVector3Array *)p_self;
	return self->size();
}

godot_bool GDAPI godot_pool_vector3_array_empty(const godot_pool_vector3_array *p_self) {
	const PoolVector3Array *self = (const PoolVector3Array *)p_self;
	return self->empty();
}

void GDAPI godot_pool_vector3_array_destroy(godot_pool_vector3_array *p_self) {
	((PoolVector3Array *)p_self)->~PoolVector();
}

// color

void GDAPI godot_pool_color_array_new(godot_pool_color_array *r_dest) {
	PoolColorArray *dest = (PoolColorArray *)r_dest;
	memnew_placement(dest, PoolColorArray);
}

void GDAPI godot_pool_color_array_new_copy(godot_pool_color_array *r_dest, const godot_pool_color_array *p_src) {
	PoolColorArray *dest = (PoolColorArray *)r_dest;
	const PoolColorArray *src = (const PoolColorArray *)p_src;
	memnew_placement(dest, PoolColorArray(*src));
}

void GDAPI godot_pool_color_array_new_with_array(godot_pool_color_array *r_dest, const godot_array *p_a) {
	PoolColorArray *dest = (PoolColorArray *)r_dest;
	Array *a = (Array *)p_a;
	memnew_placement(dest, PoolColorArray);

	dest->resize(a->size());
	for (int i = 0; i < a->size(); i++) {
		dest->set(i, (*a)[i]);
	}
}

void GDAPI godot_pool_color_array_append(godot_pool_color_array *p_self, const godot_color *p_data) {
	PoolColorArray *self = (PoolColorArray *)p_self;
	Color &s = *(Color *)p_data;
	self->append(s);
}

void GDAPI godot_pool_color_array_append_array(godot_pool_color_array *p_self, const godot_pool_color_array *p_array) {
	PoolColorArray *self = (PoolColorArray *)p_self;
	PoolColorArray *array = (PoolColorArray *)p_array;
	self->append_array(*array);
}

godot_error GDAPI godot_pool_color_array_insert(godot_pool_color_array *p_self, const godot_int p_idx, const godot_color *p_data) {
	PoolColorArray *self = (PoolColorArray *)p_self;
	Color &s = *(Color *)p_data;
	return (godot_error)self->insert(p_idx, s);
}

void GDAPI godot_pool_color_array_invert(godot_pool_color_array *p_self) {
	PoolColorArray *self = (PoolColorArray *)p_self;
	self->invert();
}

void GDAPI godot_pool_color_array_push_back(godot_pool_color_array *p_self, const godot_color *p_data) {
	PoolColorArray *self = (PoolColorArray *)p_self;
	Color &s = *(Color *)p_data;
	self->push_back(s);
}

void GDAPI godot_pool_color_array_remove(godot_pool_color_array *p_self, const godot_int p_idx) {
	PoolColorArray *self = (PoolColorArray *)p_self;
	self->remove(p_idx);
}

void GDAPI godot_pool_color_array_resize(godot_pool_color_array *p_self, const godot_int p_size) {
	PoolColorArray *self = (PoolColorArray *)p_self;
	self->resize(p_size);
}

godot_pool_color_array_read_access GDAPI *godot_pool_color_array_read(const godot_pool_color_array *p_self) {
	const PoolColorArray *self = (const PoolColorArray *)p_self;
	return (godot_pool_color_array_read_access *)memnew(PoolColorArray::Read(self->read()));
}

godot_pool_color_array_write_access GDAPI *godot_pool_color_array_write(godot_pool_color_array *p_self) {
	PoolColorArray *self = (PoolColorArray *)p_self;
	return (godot_pool_color_array_write_access *)memnew(PoolColorArray::Write(self->write()));
}

void GDAPI godot_pool_color_array_set(godot_pool_color_array *p_self, const godot_int p_idx, const godot_color *p_data) {
	PoolColorArray *self = (PoolColorArray *)p_self;
	Color &s = *(Color *)p_data;
	self->set(p_idx, s);
}

godot_color GDAPI godot_pool_color_array_get(const godot_pool_color_array *p_self, const godot_int p_idx) {
	const PoolColorArray *self = (const PoolColorArray *)p_self;
	godot_color v;
	Color *s = (Color *)&v;
	*s = self->get(p_idx);
	return v;
}

godot_int GDAPI godot_pool_color_array_size(const godot_pool_color_array *p_self) {
	const PoolColorArray *self = (const PoolColorArray *)p_self;
	return self->size();
}

godot_bool GDAPI godot_pool_color_array_empty(const godot_pool_color_array *p_self) {
	const PoolColorArray *self = (const PoolColorArray *)p_self;
	return self->empty();
}

void GDAPI godot_pool_color_array_destroy(godot_pool_color_array *p_self) {
	((PoolColorArray *)p_self)->~PoolVector();
}

//
// read accessor functions
//

godot_pool_byte_array_read_access GDAPI *godot_pool_byte_array_read_access_copy(const godot_pool_byte_array_read_access *p_other) {
	PoolByteArray::Read *other = (PoolByteArray::Read *)p_other;
	return (godot_pool_byte_array_read_access *)memnew(PoolByteArray::Read(*other));
}
const uint8_t GDAPI *godot_pool_byte_array_read_access_ptr(const godot_pool_byte_array_read_access *p_read) {
	const PoolByteArray::Read *read = (const PoolByteArray::Read *)p_read;
	return read->ptr();
}
void GDAPI godot_pool_byte_array_read_access_operator_assign(godot_pool_byte_array_read_access *p_read, godot_pool_byte_array_read_access *p_other) {
	PoolByteArray::Read *read = (PoolByteArray::Read *)p_read;
	PoolByteArray::Read *other = (PoolByteArray::Read *)p_other;
	read->operator=(*other);
}
void GDAPI godot_pool_byte_array_read_access_destroy(godot_pool_byte_array_read_access *p_read) {
	memdelete((PoolByteArray::Read *)p_read);
}

godot_pool_int_array_read_access GDAPI *godot_pool_int_array_read_access_copy(const godot_pool_int_array_read_access *p_other) {
	PoolVector<godot_int>::Read *other = (PoolVector<godot_int>::Read *)p_other;
	return (godot_pool_int_array_read_access *)memnew(PoolVector<godot_int>::Read(*other));
}
const godot_int GDAPI *godot_pool_int_array_read_access_ptr(const godot_pool_int_array_read_access *p_read) {
	const PoolVector<godot_int>::Read *read = (const PoolVector<godot_int>::Read *)p_read;
	return read->ptr();
}
void GDAPI godot_pool_int_array_read_access_operator_assign(godot_pool_int_array_read_access *p_read, godot_pool_int_array_read_access *p_other) {
	PoolVector<godot_int>::Read *read = (PoolVector<godot_int>::Read *)p_read;
	PoolVector<godot_int>::Read *other = (PoolVector<godot_int>::Read *)p_other;
	read->operator=(*other);
}
void GDAPI godot_pool_int_array_read_access_destroy(godot_pool_int_array_read_access *p_read) {
	memdelete((PoolVector<godot_int>::Read *)p_read);
}

godot_pool_real_array_read_access GDAPI *godot_pool_real_array_read_access_copy(const godot_pool_real_array_read_access *p_other) {
	PoolVector<godot_real>::Read *other = (PoolVector<godot_real>::Read *)p_other;
	return (godot_pool_real_array_read_access *)memnew(PoolVector<godot_real>::Read(*other));
}
const godot_real GDAPI *godot_pool_real_array_read_access_ptr(const godot_pool_real_array_read_access *p_read) {
	const PoolVector<godot_real>::Read *read = (const PoolVector<godot_real>::Read *)p_read;
	return read->ptr();
}
void GDAPI godot_pool_real_array_read_access_operator_assign(godot_pool_real_array_read_access *p_read, godot_pool_real_array_read_access *p_other) {
	PoolVector<godot_real>::Read *read = (PoolVector<godot_real>::Read *)p_read;
	PoolVector<godot_real>::Read *other = (PoolVector<godot_real>::Read *)p_other;
	read->operator=(*other);
}
void GDAPI godot_pool_real_array_read_access_destroy(godot_pool_real_array_read_access *p_read) {
	memdelete((PoolVector<godot_real>::Read *)p_read);
}

godot_pool_string_array_read_access GDAPI *godot_pool_string_array_read_access_copy(const godot_pool_string_array_read_access *p_other) {
	PoolStringArray::Read *other = (PoolStringArray::Read *)p_other;
	return (godot_pool_string_array_read_access *)memnew(PoolStringArray::Read(*other));
}
const godot_string GDAPI *godot_pool_string_array_read_access_ptr(const godot_pool_string_array_read_access *p_read) {
	const PoolStringArray::Read *read = (const PoolStringArray::Read *)p_read;
	return (const godot_string *)read->ptr();
}
void GDAPI godot_pool_string_array_read_access_operator_assign(godot_pool_string_array_read_access *p_read, godot_pool_string_array_read_access *p_other) {
	PoolStringArray::Read *read = (PoolStringArray::Read *)p_read;
	PoolStringArray::Read *other = (PoolStringArray::Read *)p_other;
	read->operator=(*other);
}
void GDAPI godot_pool_string_array_read_access_destroy(godot_pool_string_array_read_access *p_read) {
	memdelete((PoolStringArray::Read *)p_read);
}

godot_pool_vector2_array_read_access GDAPI *godot_pool_vector2_array_read_access_copy(const godot_pool_vector2_array_read_access *p_other) {
	PoolVector2Array::Read *other = (PoolVector2Array::Read *)p_other;
	return (godot_pool_vector2_array_read_access *)memnew(PoolVector2Array::Read(*other));
}
const godot_vector2 GDAPI *godot_pool_vector2_array_read_access_ptr(const godot_pool_vector2_array_read_access *p_read) {
	const PoolVector2Array::Read *read = (const PoolVector2Array::Read *)p_read;
	return (const godot_vector2 *)read->ptr();
}
void GDAPI godot_pool_vector2_array_read_access_operator_assign(godot_pool_vector2_array_read_access *p_read, godot_pool_vector2_array_read_access *p_other) {
	PoolVector2Array::Read *read = (PoolVector2Array::Read *)p_read;
	PoolVector2Array::Read *other = (PoolVector2Array::Read *)p_other;
	read->operator=(*other);
}
void GDAPI godot_pool_vector2_array_read_access_destroy(godot_pool_vector2_array_read_access *p_read) {
	memdelete((PoolVector2Array::Read *)p_read);
}

godot_pool_vector3_array_read_access GDAPI *godot_pool_vector3_array_read_access_copy(const godot_pool_vector3_array_read_access *p_other) {
	PoolVector3Array::Read *other = (PoolVector3Array::Read *)p_other;
	return (godot_pool_vector3_array_read_access *)memnew(PoolVector3Array::Read(*other));
}
const godot_vector3 GDAPI *godot_pool_vector3_array_read_access_ptr(const godot_pool_vector3_array_read_access *p_read) {
	const PoolVector3Array::Read *read = (const PoolVector3Array::Read *)p_read;
	return (const godot_vector3 *)read->ptr();
}
void GDAPI godot_pool_vector3_array_read_access_operator_assign(godot_pool_vector3_array_read_access *p_read, godot_pool_vector3_array_read_access *p_other) {
	PoolVector3Array::Read *read = (PoolVector3Array::Read *)p_read;
	PoolVector3Array::Read *other = (PoolVector3Array::Read *)p_other;
	read->operator=(*other);
}
void GDAPI godot_pool_vector3_array_read_access_destroy(godot_pool_vector3_array_read_access *p_read) {
	memdelete((PoolVector2Array::Read *)p_read);
}

godot_pool_color_array_read_access GDAPI *godot_pool_color_array_read_access_copy(const godot_pool_color_array_read_access *p_other) {
	PoolColorArray::Read *other = (PoolColorArray::Read *)p_other;
	return (godot_pool_color_array_read_access *)memnew(PoolColorArray::Read(*other));
}
const godot_color GDAPI *godot_pool_color_array_read_access_ptr(const godot_pool_color_array_read_access *p_read) {
	const PoolColorArray::Read *read = (const PoolColorArray::Read *)p_read;
	return (const godot_color *)read->ptr();
}
void GDAPI godot_pool_color_array_read_access_operator_assign(godot_pool_color_array_read_access *p_read, godot_pool_color_array_read_access *p_other) {
	PoolColorArray::Read *read = (PoolColorArray::Read *)p_read;
	PoolColorArray::Read *other = (PoolColorArray::Read *)p_other;
	read->operator=(*other);
}
void GDAPI godot_pool_color_array_read_access_destroy(godot_pool_color_array_read_access *p_read) {
	memdelete((PoolColorArray::Read *)p_read);
}

//
// write accessor functions
//

godot_pool_byte_array_write_access GDAPI *godot_pool_byte_array_write_access_copy(const godot_pool_byte_array_write_access *p_other) {
	PoolByteArray::Write *other = (PoolByteArray::Write *)p_other;
	return (godot_pool_byte_array_write_access *)memnew(PoolByteArray::Write(*other));
}
uint8_t GDAPI *godot_pool_byte_array_write_access_ptr(const godot_pool_byte_array_write_access *p_write) {
	PoolByteArray::Write *write = (PoolByteArray::Write *)p_write;
	return write->ptr();
}
void GDAPI godot_pool_byte_array_write_access_operator_assign(godot_pool_byte_array_write_access *p_write, godot_pool_byte_array_write_access *p_other) {
	PoolByteArray::Write *write = (PoolByteArray::Write *)p_write;
	PoolByteArray::Write *other = (PoolByteArray::Write *)p_other;
	write->operator=(*other);
}
void GDAPI godot_pool_byte_array_write_access_destroy(godot_pool_byte_array_write_access *p_write) {
	memdelete((PoolByteArray::Write *)p_write);
}

godot_pool_int_array_write_access GDAPI *godot_pool_int_array_write_access_copy(const godot_pool_int_array_write_access *p_other) {
	PoolVector<godot_int>::Write *other = (PoolVector<godot_int>::Write *)p_other;
	return (godot_pool_int_array_write_access *)memnew(PoolVector<godot_int>::Write(*other));
}
godot_int GDAPI *godot_pool_int_array_write_access_ptr(const godot_pool_int_array_write_access *p_write) {
	PoolVector<godot_int>::Write *write = (PoolVector<godot_int>::Write *)p_write;
	return write->ptr();
}
void GDAPI godot_pool_int_array_write_access_operator_assign(godot_pool_int_array_write_access *p_write, godot_pool_int_array_write_access *p_other) {
	PoolVector<godot_int>::Write *write = (PoolVector<godot_int>::Write *)p_write;
	PoolVector<godot_int>::Write *other = (PoolVector<godot_int>::Write *)p_other;
	write->operator=(*other);
}
void GDAPI godot_pool_int_array_write_access_destroy(godot_pool_int_array_write_access *p_write) {
	memdelete((PoolVector<godot_int>::Write *)p_write);
}

godot_pool_real_array_write_access GDAPI *godot_pool_real_array_write_access_copy(const godot_pool_real_array_write_access *p_other) {
	PoolVector<godot_real>::Write *other = (PoolVector<godot_real>::Write *)p_other;
	return (godot_pool_real_array_write_access *)memnew(PoolVector<godot_real>::Write(*other));
}
godot_real GDAPI *godot_pool_real_array_write_access_ptr(const godot_pool_real_array_write_access *p_write) {
	PoolVector<godot_real>::Write *write = (PoolVector<godot_real>::Write *)p_write;
	return write->ptr();
}
void GDAPI godot_pool_real_array_write_access_operator_assign(godot_pool_real_array_write_access *p_write, godot_pool_real_array_write_access *p_other) {
	PoolVector<godot_real>::Write *write = (PoolVector<godot_real>::Write *)p_write;
	PoolVector<godot_real>::Write *other = (PoolVector<godot_real>::Write *)p_other;
	write->operator=(*other);
}
void GDAPI godot_pool_real_array_write_access_destroy(godot_pool_real_array_write_access *p_write) {
	memdelete((PoolVector<godot_real>::Write *)p_write);
}

godot_pool_string_array_write_access GDAPI *godot_pool_string_array_write_access_copy(const godot_pool_string_array_write_access *p_other) {
	PoolStringArray::Write *other = (PoolStringArray::Write *)p_other;
	return (godot_pool_string_array_write_access *)memnew(PoolStringArray::Write(*other));
}
godot_string GDAPI *godot_pool_string_array_write_access_ptr(const godot_pool_string_array_write_access *p_write) {
	PoolStringArray::Write *write = (PoolStringArray::Write *)p_write;
	return (godot_string *)write->ptr();
}
void GDAPI godot_pool_string_array_write_access_operator_assign(godot_pool_string_array_write_access *p_write, godot_pool_string_array_write_access *p_other) {
	PoolStringArray::Write *write = (PoolStringArray::Write *)p_write;
	PoolStringArray::Write *other = (PoolStringArray::Write *)p_other;
	write->operator=(*other);
}
void GDAPI godot_pool_string_array_write_access_destroy(godot_pool_string_array_write_access *p_write) {
	memdelete((PoolStringArray::Write *)p_write);
}

godot_pool_vector2_array_write_access GDAPI *godot_pool_vector2_array_write_access_copy(const godot_pool_vector2_array_write_access *p_other) {
	PoolVector2Array::Write *other = (PoolVector2Array::Write *)p_other;
	return (godot_pool_vector2_array_write_access *)memnew(PoolVector2Array::Write(*other));
}
godot_vector2 GDAPI *godot_pool_vector2_array_write_access_ptr(const godot_pool_vector2_array_write_access *p_write) {
	PoolVector2Array::Write *write = (PoolVector2Array::Write *)p_write;
	return (godot_vector2 *)write->ptr();
}
void GDAPI godot_pool_vector2_array_write_access_operator_assign(godot_pool_vector2_array_write_access *p_write, godot_pool_vector2_array_write_access *p_other) {
	PoolVector2Array::Write *write = (PoolVector2Array::Write *)p_write;
	PoolVector2Array::Write *other = (PoolVector2Array::Write *)p_other;
	write->operator=(*other);
}
void GDAPI godot_pool_vector2_array_write_access_destroy(godot_pool_vector2_array_write_access *p_write) {
	memdelete((PoolVector2Array::Write *)p_write);
}

godot_pool_vector3_array_write_access GDAPI *godot_pool_vector3_array_write_access_copy(const godot_pool_vector3_array_write_access *p_other) {
	PoolVector3Array::Write *other = (PoolVector3Array::Write *)p_other;
	return (godot_pool_vector3_array_write_access *)memnew(PoolVector3Array::Write(*other));
}
godot_vector3 GDAPI *godot_pool_vector3_array_write_access_ptr(const godot_pool_vector3_array_write_access *p_write) {
	PoolVector3Array::Write *write = (PoolVector3Array::Write *)p_write;
	return (godot_vector3 *)write->ptr();
}
void GDAPI godot_pool_vector3_array_write_access_operator_assign(godot_pool_vector3_array_write_access *p_write, godot_pool_vector3_array_write_access *p_other) {
	PoolVector3Array::Write *write = (PoolVector3Array::Write *)p_write;
	PoolVector3Array::Write *other = (PoolVector3Array::Write *)p_other;
	write->operator=(*other);
}
void GDAPI godot_pool_vector3_array_write_access_destroy(godot_pool_vector3_array_write_access *p_write) {
	memdelete((PoolVector3Array::Write *)p_write);
}

godot_pool_color_array_write_access GDAPI *godot_pool_color_array_write_access_copy(const godot_pool_color_array_write_access *p_other) {
	PoolColorArray::Write *other = (PoolColorArray::Write *)p_other;
	return (godot_pool_color_array_write_access *)memnew(PoolColorArray::Write(*other));
}
godot_color GDAPI *godot_pool_color_array_write_access_ptr(const godot_pool_color_array_write_access *p_write) {
	PoolColorArray::Write *write = (PoolColorArray::Write *)p_write;
	return (godot_color *)write->ptr();
}
void GDAPI godot_pool_color_array_write_access_operator_assign(godot_pool_color_array_write_access *p_write, godot_pool_color_array_write_access *p_other) {
	PoolColorArray::Write *write = (PoolColorArray::Write *)p_write;
	PoolColorArray::Write *other = (PoolColorArray::Write *)p_other;
	write->operator=(*other);
}
void GDAPI godot_pool_color_array_write_access_destroy(godot_pool_color_array_write_access *p_write) {
	memdelete((PoolColorArray::Write *)p_write);
}

#ifdef __cplusplus
}
#endif
