#include "vector4i.h"

#include "core/string/ustring.h"
#include "vector4.h"

Vector4i Vector4i::clamp(const Vector4i &p_min, const Vector4i &p_max) const {
	return Vector4i(
			CLAMP(x, p_min.x, p_max.x),
			CLAMP(y, p_min.y, p_max.y),
			CLAMP(z, p_min.z, p_max.z),
			CLAMP(w, p_min.w, p_max.w));
}

int64_t Vector4i::dot(const Vector4i &p_with) const {
	return x * p_with.x + y * p_with.y + z * p_with.z + w * p_with.w;
}

Vector4i Vector4i::posmod(const int32_t p_mod) const {
	return Vector4i(Math::posmod(x, p_mod), Math::posmod(y, p_mod), Math::posmod(z, p_mod), Math::posmod(w, p_mod));
}

Vector4i Vector4i::posmodv(const Vector4i &p_modv) const {
	return Vector4i(Math::posmod(x, p_modv.x), Math::posmod(y, p_modv.y), Math::posmod(z, p_modv.z), Math::posmod(w, p_modv.w));
}

Vector4i::Axis Vector4i::min_axis_index() const {
	if (z < y) {
		return (z < x) ? ((z < w) ? Vector4i::AXIS_Z : Vector4i::AXIS_W) : ((w < x) ? Vector4i::AXIS_W : Vector4i::AXIS_X);
	} else {
		return (y < x) ? ((y < w) ? Vector4i::AXIS_Y : Vector4i::AXIS_W) : ((w < x) ? Vector4i::AXIS_W : Vector4i::AXIS_X);
	}
}

Vector4i::Axis Vector4i::max_axis_index() const {
	if (x > y) {
		return (x > w) ? ((x > z) ? Vector4i::AXIS_X : Vector4i::AXIS_Z) : ((z > w) ? Vector4i::AXIS_Z : Vector4i::AXIS_W);
	} else {
		return (y > w) ? ((y > z) ? Vector4i::AXIS_Y : Vector4i::AXIS_Z) : ((z > w) ? Vector4i::AXIS_Z : Vector4i::AXIS_W);
	}
}

double Vector4i::distance_to(const Vector4i &p_to) const {
	return (p_to - *this).length();
}

int64_t Vector4i::distance_squared_to(const Vector4i &p_to) const {
	return (p_to - *this).length_squared();
}

int64_t Vector4i::length_squared() const {
	return x * (int64_t)x + y * (int64_t)y + z * (int64_t)z + w * (int64_t)w;
}

double Vector4i::length() const {
	return Math::sqrt((double)length_squared());
}

Vector4i Vector4i::abs() const {
	return Vector4i(Math::abs(x), Math::abs(y), Math::abs(z), Math::abs(w));
}

Vector4i Vector4i::sign() const {
	return Vector4i(SIGN(x), SIGN(y), SIGN(z), SIGN(w));
}

/* Operators */

Vector4i &Vector4i::operator+=(const Vector4i &p_v) {
	x += p_v.x;
	y += p_v.y;
	z += p_v.z;
	w += p_v.w;
	return *this;
}

Vector4i Vector4i::operator+(const Vector4i &p_v) const {
	return Vector4i(x + p_v.x, y + p_v.y, z + p_v.z, w + p_v.w);
}

Vector4i &Vector4i::operator-=(const Vector4i &p_v) {
	x -= p_v.x;
	y -= p_v.y;
	z -= p_v.z;
	w -= p_v.w;
	return *this;
}

Vector4i Vector4i::operator-(const Vector4i &p_v) const {
	return Vector4i(x - p_v.x, y - p_v.y, z - p_v.z, w - p_v.w);
}

Vector4i &Vector4i::operator*=(const Vector4i &p_v) {
	x *= p_v.x;
	y *= p_v.y;
	z *= p_v.z;
	w *= p_v.w;
	return *this;
}

Vector4i Vector4i::operator*(const Vector4i &p_v) const {
	return Vector4i(x * p_v.x, y * p_v.y, z * p_v.z, w * p_v.w);
}

Vector4i &Vector4i::operator/=(const Vector4i &p_v) {
	x /= p_v.x;
	y /= p_v.y;
	z /= p_v.z;
	w /= p_v.w;
	return *this;
}

Vector4i Vector4i::operator/(const Vector4i &p_v) const {
	return Vector4i(x / p_v.x, y / p_v.y, z / p_v.z, w / p_v.w);
}

Vector4i &Vector4i::operator%=(const Vector4i &p_v) {
	x %= p_v.x;
	y %= p_v.y;
	z %= p_v.z;
	w %= p_v.w;
	return *this;
}

Vector4i Vector4i::operator%(const Vector4i &p_v) const {
	return Vector4i(x % p_v.x, y % p_v.y, z % p_v.z, w % p_v.w);
}

Vector4 Vector4i::operator+(const Vector4 &p_v) const {
	return Vector4(x + p_v.x, y + p_v.y, z + p_v.z, w + p_v.w);
}

Vector4 Vector4i::operator-(const Vector4 &p_v) const {
	return Vector4(x - p_v.x, y - p_v.y, z - p_v.z, w - p_v.w);
}

Vector4 Vector4i::operator*(const Vector4 &p_v) const {
	return Vector4(x * p_v.x, y * p_v.y, z * p_v.z, w * p_v.w);
}

Vector4 Vector4i::operator/(const Vector4 &p_v) const {
	return Vector4(x / p_v.x, y / p_v.y, z / p_v.z, w / p_v.w);
}

Vector4i &Vector4i::operator*=(const int32_t p_scalar) {
	x *= p_scalar;
	y *= p_scalar;
	z *= p_scalar;
	w *= p_scalar;
	return *this;
}

Vector4i Vector4i::operator*(const int32_t p_scalar) const {
	return Vector4i(x * p_scalar, y * p_scalar, z * p_scalar, w * p_scalar);
}

Vector4i &Vector4i::operator/=(const int32_t p_scalar) {
	x /= p_scalar;
	y /= p_scalar;
	z /= p_scalar;
	w /= p_scalar;
	return *this;
}

Vector4i Vector4i::operator/(const int32_t p_scalar) const {
	return Vector4i(x / p_scalar, y / p_scalar, z / p_scalar, w / p_scalar);
}

Vector4i &Vector4i::operator%=(const int32_t p_scalar) {
	x %= p_scalar;
	y %= p_scalar;
	z %= p_scalar;
	w %= p_scalar;
	return *this;
}

Vector4i Vector4i::operator%(const int32_t p_scalar) const {
	return Vector4i(x % p_scalar, y % p_scalar, z % p_scalar, w % p_scalar);
}

Vector4i Vector4i::operator-() const {
	return Vector4i(-x, -y, -z, -w);
}

bool Vector4i::operator==(const Vector4i &p_v) const {
	return x == p_v.x && y == p_v.y && z == p_v.z && w == p_v.w;
}

bool Vector4i::operator!=(const Vector4i &p_v) const {
	return x != p_v.x || y != p_v.y || z != p_v.z || w != p_v.w;
}

bool Vector4i::operator<(const Vector4i &p_v) const {
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

bool Vector4i::operator>(const Vector4i &p_v) const {
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

bool Vector4i::operator<=(const Vector4i &p_v) const {
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

bool Vector4i::operator>=(const Vector4i &p_v) const {
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

Vector4i::Vector4i(const Vector4 &p_vec) {
	x = (int32_t)p_vec.x;
	y = (int32_t)p_vec.y;
	z = (int32_t)p_vec.z;
	w = (int32_t)p_vec.w;
}

Vector4i::operator String() const {
	return "(" + String::num_real(x, false) + ", " + String::num_real(y, false) + ", " + String::num_real(z, false) + ", " + String::num_real(w, false) + ")";
}

Vector4i::operator Vector4() const {
	return Vector4(x, y, z, w);
}

static_assert(sizeof(Vector4i) == 4 * sizeof(int32_t));
