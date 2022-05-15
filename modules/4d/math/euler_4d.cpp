#include "euler_4d.h"

#include "core/string/ustring.h"

bool Euler4D::is_equal_approx(const Euler4D &p_euler) const {
	for (int i = 0; i < 6; i++) {
		if (!Math::is_equal_approx(elements[i], p_euler.elements[i], (real_t)0.001)) {
			return false;
		}
	}
	return true;
}

Basis4D Euler4D::to_basis() const {
	Basis4D yz_basis = Basis4D::from_yz(yz);
	Basis4D xz_basis = Basis4D::from_zx(zx);
	Basis4D xy_basis = Basis4D::from_xy(xy);
	// It's unclear if the sin value signs should be flipped for XW, YW, ZW.
	// Meaning, whether they should be XW, YW, ZW, or flipped to WX, WY, WZ.
	// It's probably just a matter of convention. Leaving unflipped for now.
	Basis4D xw_basis = Basis4D::from_xw(xw);
	Basis4D yw_basis = Basis4D::from_yw(yw);
	Basis4D zw_basis = Basis4D::from_zw(zw);

	// Here's what determines the rotation order of Euler angles.
	// For 3D, we have Yaw, Pitch, and Roll, in that order.
	//
	// Yaw acts as horizontal rotation (XZ plane, no Y so it's horizontal).
	// For the 4D equivalent of Yaw, we have three angles that rotate
	// horizontally (no Y): XZ, XW, and ZW. Therefore, these three go first.
	//
	// Next is Pitch. Pitch is vertical rotation that changes the direction
	// an object is pointing by moving its forward vector (-Z) up or down (Y),
	// so pitch only exists in the YZ plane. Therefore, for the 4D equivalent
	// of pitch, there is only one angle that does this: YZ.
	//
	// Lastly is Roll. Roll is also vertical rotation, but it brings the
	// sides of an object higher or lower. For the 4D equivalent of roll,
	// we want every angle that involves Y but not Z, so that gives us
	// two angles: XY and YW. Therefore, these two go last.
	//
	// Outside of this broad ordering, the order is quite arbitary.
	// Ex: Having the last two be XY-YW or YW-XY is just an arbitrary choice.
	// For now we'll hard-code the order to be XZ-ZW-XW-YZ-XY-YW and not
	// change it unless there is a strong reason to use a different order.
	// It's perfectly possible that this convention is very wrong.
	// Remember that matrix multiplication is thought of as right-to-left,
	// but also Euler angles are composed in reverse, so it cancels out.
	return xz_basis * zw_basis * xw_basis * yz_basis * xy_basis * yw_basis;
}

Euler4D Euler4D::from_basis(const Basis4D &p_basis) {
	// This process produces a LOT of floating-point error.
	// It's also likely that this could be optimized a lot.
	// With 32-bit floats, this has a margin of error of 0.1%.

	Basis4D basis = p_basis.orthonormalized();
	// YZ (Pitch) is special and we can get it immediately.
	real_t yz = -Math::asin(basis.z.dot(Vector4(0, 1, 0, 0)));

	// Get the XZ (Yaw 1, yaw-like) rotation and then un-rotate by it.
	real_t zx = Math::atan2(basis.z.x, basis.z.z);
	basis = Basis4D::from_zx(-zx) * basis;

	// Get the ZW (Yaw 2, pitch-like) rotation and then un-rotate by it.
	real_t zw = Math::atan2(basis.z.w, basis.z.z);
	basis = Basis4D::from_zw(-zw) * basis;

	// Get the XW (Yaw 3, roll-like) rotation and then un-rotate by it.
	real_t xw = Math::atan2(basis.x.w, basis.x.x);
	basis = Basis4D::from_xw(-xw) * basis;

	// We already have the YZ (Pitch) rotation so we just un-rotate by it.
	basis = Basis4D::from_yz(-yz) * basis;

	// Get the XY (Roll 1) rotation and then un-rotate by it.
	real_t xy = Math::atan2(basis.x.y, basis.x.x);
	basis = Basis4D::from_xy(-xy) * basis;

	// Get the YW (Roll 2) rotation.
	real_t yw = Math::atan2(basis.y.w, basis.y.y);

	return Euler4D(yz, zx, xy, xw, yw, zw);
}

Euler4D Euler4D::wrapped() const {
	return Euler4D(
			Math::wrapf(yz, (real_t)-Math_PI, (real_t)Math_PI),
			Math::wrapf(zx, (real_t)-Math_PI, (real_t)Math_PI),
			Math::wrapf(xy, (real_t)-Math_PI, (real_t)Math_PI),
			Math::wrapf(xw, (real_t)-Math_PI, (real_t)Math_PI),
			Math::wrapf(yw, (real_t)-Math_PI, (real_t)Math_PI),
			Math::wrapf(zw, (real_t)-Math_PI, (real_t)Math_PI));
}

/* Operators */

Euler4D &Euler4D::operator+=(const Euler4D &p_v) {
	yz += p_v.yz;
	zx += p_v.zx;
	xy += p_v.xy;
	xw += p_v.xw;
	yw += p_v.yw;
	zw += p_v.zw;
	return *this;
}

Euler4D Euler4D::operator+(const Euler4D &p_v) const {
	return Euler4D(yz + p_v.yz, zx + p_v.zx, xy + p_v.xy, xw + p_v.xw, yw + p_v.yw, zw + p_v.zw);
}

Euler4D &Euler4D::operator-=(const Euler4D &p_v) {
	yz -= p_v.yz;
	zx -= p_v.zx;
	xy -= p_v.xy;
	xw -= p_v.xw;
	yw -= p_v.yw;
	zw -= p_v.zw;
	return *this;
}

Euler4D Euler4D::operator-(const Euler4D &p_v) const {
	return Euler4D(yz - p_v.yz, zx - p_v.zx, xy - p_v.xy, xw - p_v.xw, yw - p_v.yw, zw - p_v.zw);
}

Euler4D &Euler4D::operator*=(const real_t p_scalar) {
	yz *= p_scalar;
	zx *= p_scalar;
	xy *= p_scalar;
	xw *= p_scalar;
	yw *= p_scalar;
	zw *= p_scalar;
	return *this;
}

Euler4D Euler4D::operator*(const real_t p_scalar) const {
	return Euler4D(yz * p_scalar, zx * p_scalar, xy * p_scalar, xw * p_scalar, yw * p_scalar, zw * p_scalar);
}

Euler4D &Euler4D::operator/=(const real_t p_scalar) {
	yz /= p_scalar;
	zx /= p_scalar;
	xy /= p_scalar;
	xw /= p_scalar;
	yw /= p_scalar;
	zw /= p_scalar;
	return *this;
}

Euler4D Euler4D::operator/(const real_t p_scalar) const {
	return Euler4D(yz / p_scalar, zx / p_scalar, xy / p_scalar, xw / p_scalar, yw / p_scalar, zw / p_scalar);
}

Euler4D Euler4D::operator-() const {
	return Euler4D(-yz, -zx, -xy, -xw, -yw, -zw);
}

bool Euler4D::operator==(const Euler4D &p_v) const {
	return (xy == p_v.xy && zx == p_v.zx && xw == p_v.xw && yz == p_v.yz && yw == p_v.yw && zw == p_v.zw);
}

bool Euler4D::operator!=(const Euler4D &p_v) const {
	return (yz != p_v.yz || zx != p_v.zx || xy != p_v.xy || xw != p_v.xw || yw != p_v.yw || zw != p_v.zw);
}

Euler4D::operator String() const {
	return "(YZ: " + String::num_real(yz, false) + ", ZX: " + String::num_real(zx, false) + ", XY: " + String::num_real(xy, false) + ", XW: " + String::num_real(xw, false) + ", YW: " + String::num_real(yw, false) + ", ZW: " + String::num_real(zw, false) + ")";
}

static_assert(sizeof(Euler4D) == 6 * sizeof(real_t));
