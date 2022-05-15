#ifndef EULER_4D_H
#define EULER_4D_H

#include "basis_4d.h"
#include "core/math/vector3.h"

struct _NO_DISCARD_ Euler4D {
	union {
		struct {
			// YZ, ZX, and XY are analogous to 3D (X, Y, and Z resp), but
			// XW, YW, and ZW are not (they are perpendicular to YZ, ZX, XY).
			// We'll put the 3D-like ones first, on top, in YZ-ZX-ZY order.
			// For UX reasons, we want the 3D-like ones to be first.
			// Yes I'm aware of the irony of trying to have good UX
			// for 4D, which is inherently complex regardless.
			real_t yz; // Pitch
			real_t zx; // Yaw 1, yaw-like
			real_t xy; // Roll 1
			real_t xw; // Yaw 3, roll-like
			real_t yw; // Roll 2
			real_t zw; // Yaw 2, pitch-like
		};

		real_t elements[6] = { 0 };
	};

	_FORCE_INLINE_ const real_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 6);
		return elements[p_axis];
	}

	_FORCE_INLINE_ real_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 6);
		return elements[p_axis];
	}

	bool is_equal_approx(const Euler4D &p_euler) const;
	Basis4D to_basis() const;
	static Euler4D from_basis(const Basis4D &p_basis);
	Euler4D wrapped() const;

	/* Operators */

	bool operator==(const Euler4D &p_v) const;
	bool operator!=(const Euler4D &p_v) const;
	Euler4D operator-() const;

	Euler4D &operator+=(const Euler4D &p_v);
	Euler4D operator+(const Euler4D &p_v) const;
	Euler4D &operator-=(const Euler4D &p_v);
	Euler4D operator-(const Euler4D &p_v) const;

	Euler4D &operator*=(const real_t p_scalar);
	Euler4D operator*(const real_t p_scalar) const;
	Euler4D &operator/=(const real_t p_scalar);
	Euler4D operator/(const real_t p_scalar) const;

	operator String() const;

	Euler4D() {}
	Euler4D(const Vector3 &p_yz_zx_xy, const Vector3 p_xw_yw_zw = Vector3()) {
		yz = p_yz_zx_xy.x;
		zx = p_yz_zx_xy.y;
		xy = p_yz_zx_xy.z;
		xw = p_xw_yw_zw.x;
		yw = p_xw_yw_zw.y;
		zw = p_xw_yw_zw.z;
	}
	Euler4D(const real_t p_yz, const real_t p_zx, const real_t p_xy, const real_t p_xw = 0, const real_t p_yw = 0, const real_t p_zw = 0) {
		yz = p_yz;
		zx = p_zx;
		xy = p_xy;
		xw = p_xw;
		yw = p_yw;
		zw = p_zw;
	}
};

#endif // EULER_4D_H
