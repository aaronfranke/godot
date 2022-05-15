#ifndef VECTOR4I_H
#define VECTOR4I_H

#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"

class String;
struct Vector4;

struct _NO_DISCARD_ Vector4i {
	enum Axis {
		AXIS_X,
		AXIS_Y,
		AXIS_Z,
		AXIS_W,
	};

	union {
		struct {
			int32_t x;
			int32_t y;
			int32_t z;
			int32_t w;
		};

		int32_t elements[4] = { 0 };
	};

	_FORCE_INLINE_ const int32_t &operator[](const int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 4);
		return elements[p_axis];
	}

	_FORCE_INLINE_ int32_t &operator[](const int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 4);
		return elements[p_axis];
	}

	Vector4i::Axis min_axis_index() const;
	Vector4i::Axis max_axis_index() const;

	double length() const;
	int64_t length_squared() const;

	int64_t dot(const Vector4i &p_with) const;

	Vector4i abs() const;
	Vector4i sign() const;
	Vector4i clamp(const Vector4i &p_min, const Vector4i &p_max) const;

	double distance_to(const Vector4i &p_to) const;
	int64_t distance_squared_to(const Vector4i &p_to) const;

	Vector4i posmod(const int32_t p_mod) const;
	Vector4i posmodv(const Vector4i &p_modv) const;

	real_t angle_to(const Vector4i &p_to) const;

	/* Operators */

	Vector4i &operator+=(const Vector4i &p_v);
	Vector4i operator+(const Vector4i &p_v) const;
	Vector4i &operator-=(const Vector4i &p_v);
	Vector4i operator-(const Vector4i &p_v) const;
	Vector4i &operator*=(const Vector4i &p_v);
	Vector4i operator*(const Vector4i &p_v) const;
	Vector4i &operator/=(const Vector4i &p_v);
	Vector4i operator/(const Vector4i &p_v) const;
	Vector4i &operator%=(const Vector4i &p_v);
	Vector4i operator%(const Vector4i &p_v) const;

	Vector4 operator+(const Vector4 &p_v) const;
	Vector4 operator-(const Vector4 &p_v) const;
	Vector4 operator*(const Vector4 &p_v) const;
	Vector4 operator/(const Vector4 &p_v) const;

	Vector4i &operator*=(const int32_t p_scalar);
	Vector4i operator*(const int32_t p_scalar) const;
	Vector4i &operator/=(const int32_t p_scalar);
	Vector4i operator/(const int32_t p_scalar) const;
	Vector4i &operator%=(const int32_t p_scalar);
	Vector4i operator%(const int32_t p_scalar) const;

	Vector4i operator-() const;

	bool operator==(const Vector4i &p_v) const;
	bool operator!=(const Vector4i &p_v) const;
	bool operator<(const Vector4i &p_v) const;
	bool operator<=(const Vector4i &p_v) const;
	bool operator>(const Vector4i &p_v) const;
	bool operator>=(const Vector4i &p_v) const;

	operator String() const;
	operator Vector4() const;

	Vector4i() {}
	Vector4i(const Vector4 &p_vec);
	Vector4i(const int32_t p_x, const int32_t p_y, const int32_t p_z, const int32_t p_w) {
		x = p_x;
		y = p_y;
		z = p_z;
		w = p_w;
	}
};

_FORCE_INLINE_ Vector4i operator*(const float p_scalar, const Vector4i &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector4i operator*(const double p_scalar, const Vector4i &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector4i operator*(const int32_t p_scalar, const Vector4i &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector4i operator*(const int64_t p_scalar, const Vector4i &p_vec) {
	return p_vec * p_scalar;
}

#endif // VECTOR4I_H
