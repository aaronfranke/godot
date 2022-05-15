#ifndef VECTOR4_H
#define VECTOR4_H

#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"

class String;
struct Vector4i;

struct _NO_DISCARD_ Vector4 {
	enum Axis {
		AXIS_X,
		AXIS_Y,
		AXIS_Z,
		AXIS_W,
	};

	union {
		struct {
			real_t x;
			real_t y;
			real_t z;
			real_t w;
		};

		real_t elements[4] = { 0 };
	};

	_FORCE_INLINE_ const real_t &operator[](const int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 4);
		return elements[p_axis];
	}

	_FORCE_INLINE_ real_t &operator[](const int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 4);
		return elements[p_axis];
	}

	Vector4::Axis min_axis_index() const;
	Vector4::Axis max_axis_index() const;

	real_t length() const;
	real_t length_squared() const;

	void normalize();
	Vector4 normalized() const;
	bool is_normalized() const;
	Vector4 inverse() const;
	Vector4 limit_length(const real_t p_len = 1.0) const;

	void snap(const Vector4 &p_val);
	Vector4 snapped(const Vector4 &p_val) const;

	/* Static Methods between 2 Vector4s */

	Vector4 lerp(const Vector4 &p_to, const real_t p_weight) const;
	Vector4 cubic_interpolate(const Vector4 &p_b, const Vector4 &p_pre_a, const Vector4 &p_post_b, const real_t p_weight) const;
	Vector4 move_toward(const Vector4 &p_to, const real_t p_delta) const;

	real_t dot(const Vector4 &p_with) const;

	Vector4 abs() const;
	Vector4 floor() const;
	Vector4 sign() const;
	Vector4 ceil() const;
	Vector4 round() const;
	Vector4 clamp(const Vector4 &p_min, const Vector4 &p_max) const;

	real_t distance_to(const Vector4 &p_to) const;
	real_t distance_squared_to(const Vector4 &p_to) const;

	Vector4 posmod(const real_t p_mod) const;
	Vector4 posmodv(const Vector4 &p_modv) const;
	Vector4 project(const Vector4 &p_to) const;

	real_t angle_to(const Vector4 &p_to) const;
	Vector4 direction_to(const Vector4 &p_to) const;

	Vector4 slide(const Vector4 &p_normal) const;
	Vector4 bounce(const Vector4 &p_normal) const;
	Vector4 reflect(const Vector4 &p_normal) const;

	bool is_equal_approx(const Vector4 &p_v) const;

	/* Operators */

	Vector4 &operator+=(const Vector4 &p_v);
	Vector4 operator+(const Vector4 &p_v) const;
	Vector4 &operator-=(const Vector4 &p_v);
	Vector4 operator-(const Vector4 &p_v) const;
	Vector4 &operator*=(const Vector4 &p_v);
	Vector4 operator*(const Vector4 &p_v) const;
	Vector4 &operator/=(const Vector4 &p_v);
	Vector4 operator/(const Vector4 &p_v) const;

	Vector4 &operator*=(const real_t p_scalar);
	Vector4 operator*(const real_t p_scalar) const;
	Vector4 &operator/=(const real_t p_scalar);
	Vector4 operator/(const real_t p_scalar) const;

	Vector4 operator-() const;

	bool operator==(const Vector4 &p_v) const;
	bool operator!=(const Vector4 &p_v) const;
	bool operator<(const Vector4 &p_v) const;
	bool operator<=(const Vector4 &p_v) const;
	bool operator>(const Vector4 &p_v) const;
	bool operator>=(const Vector4 &p_v) const;

	operator String() const;
	operator Vector4i() const;

	Vector4() {}
	Vector4(const real_t p_x, const real_t p_y, const real_t p_z, const real_t p_w) {
		x = p_x;
		y = p_y;
		z = p_z;
		w = p_w;
	}
};

_FORCE_INLINE_ Vector4 operator*(const float p_scalar, const Vector4 &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector4 operator*(const double p_scalar, const Vector4 &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector4 operator*(const int32_t p_scalar, const Vector4 &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector4 operator*(const int64_t p_scalar, const Vector4 &p_vec) {
	return p_vec * p_scalar;
}

#endif // VECTOR4_H
