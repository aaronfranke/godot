#ifndef OCTONION_H
#define OCTONION_H

#include "basis_4d.h"
#include "core/math/quaternion.h"

struct _NO_DISCARD_ Octonion {
	union {
		struct {
			real_t r;
			real_t i;
			real_t j;
			real_t k;
			real_t l;
			real_t m;
			real_t n;
			real_t o;
		};

		real_t elements[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
	};

	_FORCE_INLINE_ const real_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 8);
		return elements[p_axis];
	}

	_FORCE_INLINE_ real_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 8);
		return elements[p_axis];
	}

	void decompose_into_quaternions(Quaternion &r_rijk, Quaternion &r_lmno) const;
	bool is_equal_approx(const Octonion &p_octonion) const;

	bool operator==(const Octonion &p_v) const;
	bool operator!=(const Octonion &p_v) const;

	Octonion operator-() const;
	Octonion operator*(const Octonion &p_right) const;

	operator String() const;

	Octonion() {}
	Octonion(const Quaternion &p_rijk, const Quaternion &p_lmno = Quaternion()) {
		r = p_rijk.w;
		i = p_rijk.x;
		j = p_rijk.y;
		k = p_rijk.z;
		l = p_lmno.w;
		m = p_lmno.x;
		n = p_lmno.y;
		o = p_lmno.z;
	}
	Octonion(const real_t p_r, const real_t p_i, const real_t p_j, const real_t p_k, const real_t p_l, const real_t p_m, const real_t p_n, const real_t p_o) {
		r = p_r;
		i = p_i;
		j = p_j;
		k = p_k;
		l = p_l;
		m = p_m;
		n = p_n;
		o = p_o;
	}
};

#endif // OCTONION_H
