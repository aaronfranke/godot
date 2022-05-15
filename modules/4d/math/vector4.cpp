#include "vector4.h"

#include "core/string/ustring.h"
#include "vector4i.h"

Vector4 Vector4::clamp(const Vector4 &p_min, const Vector4 &p_max) const {
	return Vector4(
			CLAMP(x, p_min.x, p_max.x),
			CLAMP(y, p_min.y, p_max.y),
			CLAMP(z, p_min.z, p_max.z),
			CLAMP(w, p_min.w, p_max.w));
}

real_t Vector4::dot(const Vector4 &p_with) const {
	return x * p_with.x + y * p_with.y + z * p_with.z + w * p_with.w;
}

Vector4 Vector4::posmod(const real_t p_mod) const {
	return Vector4(Math::fposmod(x, p_mod), Math::fposmod(y, p_mod), Math::fposmod(z, p_mod), Math::fposmod(w, p_mod));
}

Vector4 Vector4::posmodv(const Vector4 &p_modv) const {
	return Vector4(Math::fposmod(x, p_modv.x), Math::fposmod(y, p_modv.y), Math::fposmod(z, p_modv.z), Math::fposmod(w, p_modv.w));
}

Vector4 Vector4::project(const Vector4 &p_to) const {
	return p_to * (dot(p_to) / p_to.length_squared());
}

Vector4::Axis Vector4::min_axis_index() const {
	if (z < y) {
		return (z < x) ? ((z < w) ? Vector4::AXIS_Z : Vector4::AXIS_W) : ((w < x) ? Vector4::AXIS_W : Vector4::AXIS_X);
	} else {
		return (y < x) ? ((y < w) ? Vector4::AXIS_Y : Vector4::AXIS_W) : ((w < x) ? Vector4::AXIS_W : Vector4::AXIS_X);
	}
}

Vector4::Axis Vector4::max_axis_index() const {
	if (x > y) {
		return (x > w) ? ((x > z) ? Vector4::AXIS_X : Vector4::AXIS_Z) : ((z > w) ? Vector4::AXIS_Z : Vector4::AXIS_W);
	} else {
		return (y > w) ? ((y > z) ? Vector4::AXIS_Y : Vector4::AXIS_Z) : ((z > w) ? Vector4::AXIS_Z : Vector4::AXIS_W);
	}
}

Vector4 Vector4::direction_to(const Vector4 &p_to) const {
	Vector4 ret(p_to.x - x, p_to.y - y, p_to.z - z, p_to.w - w);
	ret.normalize();
	return ret;
}

real_t Vector4::distance_to(const Vector4 &p_to) const {
	return (p_to - *this).length();
}

real_t Vector4::distance_squared_to(const Vector4 &p_to) const {
	return (p_to - *this).length_squared();
}

real_t Vector4::length_squared() const {
	return x * (real_t)x + y * (real_t)y + z * (real_t)z + w * (real_t)w;
}

real_t Vector4::length() const {
	return Math::sqrt(length_squared());
}

void Vector4::normalize() {
	real_t lengthsq = length_squared();
	if (lengthsq == 0) {
		x = y = z = w = 0;
	} else {
		real_t length = Math::sqrt(lengthsq);
		x /= length;
		y /= length;
		z /= length;
		w /= length;
	}
}

Vector4 Vector4::normalized() const {
	Vector4 v = *this;
	v.normalize();
	return v;
}

bool Vector4::is_normalized() const {
	// use length_squared() instead of length() to avoid sqrt(), makes it more stringent.
	return Math::is_equal_approx(length_squared(), (real_t)1, (real_t)CMP_EPSILON);
}

bool Vector4::is_equal_approx(const Vector4 &p_v) const {
	return Math::is_equal_approx(x, p_v.x) && Math::is_equal_approx(y, p_v.y) && Math::is_equal_approx(z, p_v.z) && Math::is_equal_approx(w, p_v.w);
}

Vector4 Vector4::inverse() const {
	return Vector4(1.0f / x, 1.0f / y, 1.0f / z, 1.0f / w);
}

Vector4 Vector4::bounce(const Vector4 &p_normal) const {
	return -reflect(p_normal);
}

Vector4 Vector4::reflect(const Vector4 &p_normal) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!p_normal.is_normalized(), Vector4(), "The normal Vector4 must be normalized.");
#endif
	return 2.0f * p_normal * this->dot(p_normal) - *this;
}

// slide returns the component of the vector along the given plane, specified by its normal vector.
Vector4 Vector4::slide(const Vector4 &p_normal) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!p_normal.is_normalized(), Vector4(), "The normal Vector4 must be normalized.");
#endif
	return *this - p_normal * this->dot(p_normal);
}

void Vector4::snap(const Vector4 &p_step) {
	x = Math::snapped(x, p_step.x);
	y = Math::snapped(y, p_step.y);
	z = Math::snapped(z, p_step.z);
	w = Math::snapped(w, p_step.w);
}

Vector4 Vector4::snapped(const Vector4 &p_step) const {
	Vector4 v = *this;
	v.snap(p_step);
	return v;
}

Vector4 Vector4::limit_length(const real_t p_len) const {
	const real_t l = length();
	Vector4 v = *this;
	if (l > 0 && p_len < l) {
		v /= l;
		v *= p_len;
	}

	return v;
}

Vector4 Vector4::lerp(const Vector4 &p_to, const real_t p_weight) const {
	return Vector4(
			x + (p_weight * (p_to.x - x)),
			y + (p_weight * (p_to.y - y)),
			z + (p_weight * (p_to.z - z)),
			w + (p_weight * (p_to.w - w)));
}

Vector4 Vector4::cubic_interpolate(const Vector4 &p_b, const Vector4 &p_pre_a, const Vector4 &p_post_b, const real_t p_weight) const {
	Vector4 res = *this;
	res.x = Math::cubic_interpolate(res.x, p_b.x, p_pre_a.x, p_post_b.x, p_weight);
	res.y = Math::cubic_interpolate(res.y, p_b.y, p_pre_a.y, p_post_b.y, p_weight);
	res.z = Math::cubic_interpolate(res.z, p_b.z, p_pre_a.z, p_post_b.z, p_weight);
	res.w = Math::cubic_interpolate(res.w, p_b.w, p_pre_a.w, p_post_b.w, p_weight);
	return res;
}

Vector4 Vector4::move_toward(const Vector4 &p_to, const real_t p_delta) const {
	Vector4 v = *this;
	Vector4 vd = p_to - v;
	real_t len = vd.length();
	return len <= p_delta || len < (real_t)CMP_EPSILON ? p_to : v + vd / len * p_delta;
}

Vector4 Vector4::abs() const {
	return Vector4(Math::abs(x), Math::abs(y), Math::abs(z), Math::abs(w));
}

Vector4 Vector4::sign() const {
	return Vector4(SIGN(x), SIGN(y), SIGN(z), SIGN(w));
}

Vector4 Vector4::floor() const {
	return Vector4(Math::floor(x), Math::floor(y), Math::floor(z), Math::floor(w));
}

Vector4 Vector4::ceil() const {
	return Vector4(Math::ceil(x), Math::ceil(y), Math::ceil(z), Math::ceil(w));
}

Vector4 Vector4::round() const {
	return Vector4(Math::round(x), Math::round(y), Math::round(z), Math::round(w));
}

/* Operators */

Vector4 &Vector4::operator+=(const Vector4 &p_v) {
	x += p_v.x;
	y += p_v.y;
	z += p_v.z;
	w += p_v.w;
	return *this;
}

Vector4 Vector4::operator+(const Vector4 &p_v) const {
	return Vector4(x + p_v.x, y + p_v.y, z + p_v.z, w + p_v.w);
}

Vector4 &Vector4::operator-=(const Vector4 &p_v) {
	x -= p_v.x;
	y -= p_v.y;
	z -= p_v.z;
	w -= p_v.w;
	return *this;
}

Vector4 Vector4::operator-(const Vector4 &p_v) const {
	return Vector4(x - p_v.x, y - p_v.y, z - p_v.z, w - p_v.w);
}

Vector4 &Vector4::operator*=(const Vector4 &p_v) {
	x *= p_v.x;
	y *= p_v.y;
	z *= p_v.z;
	w *= p_v.w;
	return *this;
}

Vector4 Vector4::operator*(const Vector4 &p_v) const {
	return Vector4(x * p_v.x, y * p_v.y, z * p_v.z, w * p_v.w);
}

Vector4 &Vector4::operator/=(const Vector4 &p_v) {
	x /= p_v.x;
	y /= p_v.y;
	z /= p_v.z;
	w /= p_v.w;
	return *this;
}

Vector4 Vector4::operator/(const Vector4 &p_v) const {
	return Vector4(x / p_v.x, y / p_v.y, z / p_v.z, w / p_v.w);
}

Vector4 &Vector4::operator*=(const real_t p_scalar) {
	x *= p_scalar;
	y *= p_scalar;
	z *= p_scalar;
	w *= p_scalar;
	return *this;
}

Vector4 Vector4::operator*(const real_t p_scalar) const {
	return Vector4(x * p_scalar, y * p_scalar, z * p_scalar, w * p_scalar);
}

Vector4 &Vector4::operator/=(const real_t p_scalar) {
	x /= p_scalar;
	y /= p_scalar;
	z /= p_scalar;
	w /= p_scalar;
	return *this;
}

Vector4 Vector4::operator/(const real_t p_scalar) const {
	return Vector4(x / p_scalar, y / p_scalar, z / p_scalar, w / p_scalar);
}

Vector4 Vector4::operator-() const {
	return Vector4(-x, -y, -z, -w);
}

bool Vector4::operator==(const Vector4 &p_v) const {
	return x == p_v.x && y == p_v.y && z == p_v.z && w == p_v.w;
}

bool Vector4::operator!=(const Vector4 &p_v) const {
	return x != p_v.x || y != p_v.y || z != p_v.z || w != p_v.w;
}

bool Vector4::operator<(const Vector4 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			if (z == p_v.z) {
				return w < p_v.w;
			}
			return z < p_v.z;
		}
		return y < p_v.y;
	}
	return x < p_v.x;
}

bool Vector4::operator>(const Vector4 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			if (z == p_v.z) {
				return w > p_v.w;
			}
			return z > p_v.z;
		}
		return y > p_v.y;
	}
	return x > p_v.x;
}

bool Vector4::operator<=(const Vector4 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			if (z == p_v.z) {
				return w <= p_v.w;
			}
			return z < p_v.z;
		}
		return y < p_v.y;
	}
	return x < p_v.x;
}

bool Vector4::operator>=(const Vector4 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			if (z == p_v.z) {
				return w >= p_v.w;
			}
			return z > p_v.z;
		}
		return y > p_v.y;
	}
	return x > p_v.x;
}

Vector4::operator String() const {
	return "(" + String::num_real(x, false) + ", " + String::num_real(y, false) + ", " + String::num_real(z, false) + ", " + String::num_real(w, false) + ")";
}

Vector4::operator Vector4i() const {
	return Vector4i(x, y, z, w);
}

static_assert(sizeof(Vector4) == 4 * sizeof(real_t));
