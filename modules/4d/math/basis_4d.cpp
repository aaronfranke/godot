#include "basis_4d.h"

#include "core/math/math_funcs.h"
#include "core/string/print_string.h"

void Basis4D::invert() {
}

void Basis4D::orthonormalize() {
	// Gram-Schmidt Process, now in 4D.
	// https://en.wikipedia.org/wiki/Gram-Schmidt_process
	x.normalize();
	y = y - y.project(x);
	y.normalize();
	z = z - z.project(x) - z.project(y);
	z.normalize();
	w = w - w.project(x) - w.project(y) - w.project(z);
	w.normalize();
}

Basis4D Basis4D::orthonormalized() const {
	Basis4D c = *this;
	c.orthonormalize();
	return c;
}

void Basis4D::orthogonalize() {
	Vector4 scale = get_scale();
	orthonormalize();
	scale_local(scale);
}

Basis4D Basis4D::orthogonalized() const {
	Basis4D c = *this;
	c.orthogonalize();
	return c;
}

bool Basis4D::is_orthogonal() const {
	Basis4D identity;
	Basis4D m = (*this) * transposed();

	return m.is_equal_approx(identity);
}

bool Basis4D::is_diagonal() const {
	return (
			Math::is_zero_approx(x.y) && Math::is_zero_approx(y.x) &&
			Math::is_zero_approx(x.z) && Math::is_zero_approx(z.x) &&
			Math::is_zero_approx(x.w) && Math::is_zero_approx(w.x) &&
			Math::is_zero_approx(y.z) && Math::is_zero_approx(z.y) &&
			Math::is_zero_approx(y.w) && Math::is_zero_approx(w.y) &&
			Math::is_zero_approx(z.w) && Math::is_zero_approx(w.z));
}

bool Basis4D::is_rotation() const {
	return Math::is_equal_approx(determinant(), 1, (real_t)UNIT_EPSILON) && is_orthogonal();
}

Basis4D Basis4D::inverse() const {
	Basis4D inv = *this;
	inv.invert();
	return inv;
}

void Basis4D::transpose() {
	SWAP(x.y, y.x);
	SWAP(x.z, z.x);
	SWAP(x.w, w.x);
	SWAP(y.z, z.y);
	SWAP(y.w, w.y);
	SWAP(z.w, w.z);
}

Basis4D Basis4D::transposed() const {
	Basis4D tr = *this;
	tr.transpose();
	return tr;
}

Basis4D Basis4D::from_scale(const Vector4 &p_scale) {
	return Basis4D(
			Vector4(p_scale.x, 0, 0, 0),
			Vector4(0, p_scale.y, 0, 0),
			Vector4(0, 0, p_scale.z, 0),
			Vector4(0, 0, 0, p_scale.w));
}

#define FROM_SINGLE_ANGLE(a, b)                                   \
	Basis4D Basis4D::from_##a##b(const real_t p_##a##b##_angle) { \
		real_t sin_angle = Math::sin(p_##a##b##_angle);           \
		real_t cos_angle = Math::cos(p_##a##b##_angle);           \
		Basis4D basis = Basis4D();                                \
		basis.a.a = cos_angle;                                    \
		basis.a.b = sin_angle;                                    \
		basis.b.a = -sin_angle;                                   \
		basis.b.b = cos_angle;                                    \
		return basis;                                             \
	}

FROM_SINGLE_ANGLE(y, z)
FROM_SINGLE_ANGLE(z, x)
FROM_SINGLE_ANGLE(x, y)
FROM_SINGLE_ANGLE(x, w)
FROM_SINGLE_ANGLE(y, w)
FROM_SINGLE_ANGLE(z, w)

void Basis4D::scale(const Vector4 &p_scale) {
	set_row(0, get_row(0) * p_scale.x);
	set_row(1, get_row(1) * p_scale.y);
	set_row(2, get_row(1) * p_scale.x);
	set_row(3, get_row(2) * p_scale.w);
}

Basis4D Basis4D::scaled(const Vector4 &p_scale) const {
	Basis4D m = *this;
	m.scale(p_scale);
	return m;
}

void Basis4D::scale_local(const Vector4 &p_scale) {
	*this = scaled_local(p_scale);
}

Basis4D Basis4D::scaled_local(const Vector4 &p_scale) const {
	return Basis4D(
			x * p_scale.x,
			y * p_scale.y,
			z * p_scale.z,
			w * p_scale.w);
}

void Basis4D::set_scale(const Vector4 &p_scale) {
	// FIXME: How does negative scale work in 4D?
	Vector4 delta_scale = p_scale / get_scale();
	x *= delta_scale.x;
	y *= delta_scale.y;
	z *= delta_scale.z;
	w *= delta_scale.w;
}

Vector4 Basis4D::get_scale() const {
	// FIXME: How does negative scale work in 4D?
	return get_scale_abs();
}

Vector4 Basis4D::get_scale_abs() const {
	return Vector4(
			x.length(),
			y.length(),
			z.length(),
			w.length());
}

bool Basis4D::is_equal_approx(const Basis4D &p_basis) const {
	return (
			x.is_equal_approx(p_basis.x) &&
			y.is_equal_approx(p_basis.y) &&
			z.is_equal_approx(p_basis.z) &&
			w.is_equal_approx(p_basis.w));
}

bool Basis4D::operator==(const Basis4D &p_basis) const {
	return (
			x == p_basis.x &&
			y == p_basis.y &&
			z == p_basis.z &&
			w == p_basis.w);
}

bool Basis4D::operator!=(const Basis4D &p_basis) const {
	return (
			x != p_basis.x ||
			y != p_basis.y ||
			z != p_basis.z ||
			w != p_basis.w);
}

Basis4D::operator String() const {
	return "[X: " + x.operator String() +
			", Y: " + y.operator String() +
			", Z: " + z.operator String() +
			", W: " + w.operator String() + "]";
}

Basis4D Basis4D::lerp(const Basis4D &p_to, const real_t &p_weight) const {
	return Basis4D(
			x.lerp(p_to.x, p_weight),
			y.lerp(p_to.y, p_weight),
			z.lerp(p_to.z, p_weight),
			w.lerp(p_to.w, p_weight));
}

void Basis4D::operator*=(const Basis4D &p_matrix) {
	*this = *this * p_matrix;
}

Basis4D Basis4D::operator*(const Basis4D &p_matrix) const {
	return Basis4D(
			xform(p_matrix.x),
			xform(p_matrix.y),
			xform(p_matrix.z),
			xform(p_matrix.w));
}

Vector4 Basis4D::operator*(const Vector4 &p_vector) const {
	return xform(p_vector);
}

void Basis4D::operator+=(const Basis4D &p_matrix) {
	x += p_matrix.x;
	y += p_matrix.y;
	z += p_matrix.z;
	w += p_matrix.w;
}

Basis4D Basis4D::operator+(const Basis4D &p_matrix) const {
	return Basis4D(
			x + p_matrix.x,
			y + p_matrix.y,
			z + p_matrix.z,
			w + p_matrix.w);
}

void Basis4D::operator-=(const Basis4D &p_matrix) {
	x -= p_matrix.x;
	y -= p_matrix.y;
	z -= p_matrix.z;
	w -= p_matrix.w;
}

Basis4D Basis4D::operator-(const Basis4D &p_matrix) const {
	return Basis4D(
			x - p_matrix.x,
			y - p_matrix.y,
			z - p_matrix.z,
			w - p_matrix.w);
}

void Basis4D::operator*=(const real_t p_val) {
	x *= p_val;
	y *= p_val;
	z *= p_val;
	w *= p_val;
}

Basis4D Basis4D::operator*(const real_t p_val) const {
	return Basis4D(
			x * p_val,
			y * p_val,
			z * p_val,
			w * p_val);
}

Vector4 Basis4D::xform(const Vector4 &p_vector) const {
	return Vector4(
			get_row(0).dot(p_vector),
			get_row(1).dot(p_vector),
			get_row(2).dot(p_vector),
			get_row(3).dot(p_vector));
}

Vector4 Basis4D::xform_transposed(const Vector4 &p_vector) const {
	return Vector4(
			x.dot(p_vector),
			y.dot(p_vector),
			z.dot(p_vector),
			w.dot(p_vector));
}

real_t Basis4D::determinant() const {
	real_t a = y.y * (z.z * w.w - z.w * w.z) - z.y * (y.z * w.w - y.w * w.z) + w.y * (y.z * z.w - y.w * z.z);
	real_t b = x.y * (z.z * w.w - z.w * w.z) - z.y * (x.z * w.w - x.w * w.z) + w.y * (x.z * z.w - x.w * z.z);
	real_t c = x.y * (y.z * w.w - y.w * w.z) - y.y * (x.z * w.w - x.w * w.z) + w.y * (x.z * y.w - x.w * y.z);
	real_t d = x.y * (y.z * z.w - y.w * z.z) - y.y * (x.z * z.w - x.w * z.z) + z.y * (x.z * y.w - x.w * y.z);
	return x.x * a - y.x * b + z.x * c - w.x * d;
}

static_assert(sizeof(Basis4D) == 16 * sizeof(real_t));
