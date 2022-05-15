#include "transform_4d.h"

#include "core/math/math_funcs.h"
#include "core/string/print_string.h"

void Transform4D::affine_invert() {
	basis.invert();
	origin = basis.xform(-origin);
}

Transform4D Transform4D::affine_inverse() const {
	Transform4D ret = *this;
	ret.affine_invert();
	return ret;
}

void Transform4D::invert() {
	basis.transpose();
	origin = basis.xform(-origin);
}

Transform4D Transform4D::inverse() const {
	// FIXME: this function assumes the basis is a rotation matrix, with no scaling.
	// Transform4D::affine_inverse can handle matrices with scaling, so GDScript should eventually use that.
	Transform4D ret = *this;
	ret.invert();
	return ret;
}

Transform4D Transform4D::interpolate_with(const Transform4D &p_transform, real_t p_c) const {
	Transform4D interp;

	interp.basis = basis.lerp(p_transform.basis, p_c);
	interp.origin = origin.lerp(p_transform.origin, p_c);

	return interp;
}

void Transform4D::set_scale(const Vector4 &p_scale) {
	basis.set_scale(p_scale);
}

Vector4 Transform4D::get_scale() const {
	return basis.get_scale();
}

Vector4 Transform4D::get_scale_abs() const {
	return basis.get_scale_abs();
}

void Transform4D::scale(const Vector4 &p_scale) {
	basis.scale(p_scale);
	origin *= p_scale;
}

Transform4D Transform4D::scaled(const Vector4 &p_scale) const {
	Transform4D t = *this;
	t.scale(p_scale);
	return t;
}

void Transform4D::scale_basis(const Vector4 &p_scale) {
	basis.scale(p_scale);
}

void Transform4D::translate_local(const Vector4 &p_translation) {
	origin += basis * p_translation;
}

Transform4D Transform4D::translated_local(const Vector4 &p_translation) const {
	Transform4D t = *this;
	t.translate_local(p_translation);
	return t;
}

void Transform4D::orthonormalize() {
	basis.orthonormalize();
}

Transform4D Transform4D::orthonormalized() const {
	Transform4D t = *this;
	t.orthonormalize();
	return t;
}

void Transform4D::orthogonalize() {
	basis.orthogonalize();
}

Transform4D Transform4D::orthogonalized() const {
	Transform4D _copy = *this;
	_copy.orthogonalize();
	return _copy;
}

bool Transform4D::is_equal_approx(const Transform4D &p_transform) const {
	return basis.is_equal_approx(p_transform.basis) && origin.is_equal_approx(p_transform.origin);
}

bool Transform4D::operator==(const Transform4D &p_transform) const {
	return (basis == p_transform.basis && origin == p_transform.origin);
}

bool Transform4D::operator!=(const Transform4D &p_transform) const {
	return (basis != p_transform.basis || origin != p_transform.origin);
}

void Transform4D::operator*=(const Transform4D &p_transform) {
	origin = xform(p_transform.origin);
	basis *= p_transform.basis;
}

Transform4D Transform4D::operator*(const Transform4D &p_transform) const {
	return Transform4D(
			basis * p_transform.basis,
			xform(p_transform.origin));
}

Basis4D Transform4D::operator*(const Basis4D &p_basis) const {
	return basis * p_basis;
}

Vector4 Transform4D::operator*(const Vector4 &p_vector) const {
	return basis.xform(p_vector) + origin;
}

void Transform4D::operator*=(const real_t p_val) {
	origin *= p_val;
	basis *= p_val;
}

Transform4D Transform4D::operator*(const real_t p_val) const {
	Transform4D ret(*this);
	ret *= p_val;
	return ret;
}

Transform4D::operator String() const {
	return "[X: " + basis.x.operator String() +
			", Y: " + basis.y.operator String() +
			", Z: " + basis.z.operator String() +
			", W: " + basis.w.operator String() +
			", O: " + origin.operator String() + "]";
}

Transform4D::Transform4D(const Basis4D &p_basis, const Vector4 &p_origin) :
		basis(p_basis),
		origin(p_origin) {
}

Transform4D::Transform4D(const Vector4 &p_x, const Vector4 &p_y, const Vector4 &p_z, const Vector4 &p_w, const Vector4 &p_origin) {
	basis = Basis4D(p_x, p_y, p_z, p_w);
	origin = p_origin;
}

Vector4 Transform4D::xform(const Vector4 &p_vector) const {
	return basis.xform(p_vector) + origin;
}

Vector4 Transform4D::xform_transposed(const Vector4 &p_vector) const {
	return basis.xform_transposed(p_vector - origin);
}

Vector<Vector4> Transform4D::xform(const Vector<Vector4> &p_array) const {
	Vector<Vector4> array;
	array.resize(p_array.size());

	const Vector4 *r = p_array.ptr();
	Vector4 *w = array.ptrw();

	for (int i = 0; i < p_array.size(); ++i) {
		w[i] = xform(r[i]);
	}
	return array;
}

Vector<Vector4> Transform4D::xform_transposed(const Vector<Vector4> &p_array) const {
	Vector<Vector4> array;
	array.resize(p_array.size());

	const Vector4 *r = p_array.ptr();
	Vector4 *w = array.ptrw();

	for (int i = 0; i < p_array.size(); ++i) {
		w[i] = xform_transposed(r[i]);
	}
	return array;
}

static_assert(sizeof(Transform4D) == 20 * sizeof(real_t));
