#include "octonion.h"

#include "core/string/ustring.h"

void Octonion::decompose_into_quaternions(Quaternion &r_rijk, Quaternion &r_lmno) const {
	// Remember that Godot's Quaternion has the real part last.
	r_rijk = Quaternion(i, j, k, r);
	r_lmno = Quaternion(m, n, o, l);
}

bool Octonion::is_equal_approx(const Octonion &p_octonion) const {
	for (int i = 0; i < 8; i++) {
		if (!Math::is_equal_approx(elements[i], p_octonion.elements[i])) {
			return false;
		}
	}
	return true;
}

bool Octonion::operator==(const Octonion &p_v) const {
	return (r == p_v.r && i == p_v.i && j == p_v.j && k == p_v.k && l == p_v.l && m == p_v.m && n == p_v.n && o == p_v.o);
}

bool Octonion::operator!=(const Octonion &p_v) const {
	return (r != p_v.r || i != p_v.i || j != p_v.j || k != p_v.k || l != p_v.l || m != p_v.m || n != p_v.n || o != p_v.o);
}

Quaternion quaternion_conjugate(const Quaternion &p_quat) {
	return Quaternion(-p_quat.x, -p_quat.y, -p_quat.z, p_quat.w);
}

Octonion Octonion::operator*(const Octonion &p_right) const {
	// Cayley-Dickson construction: Octonion multiplication implemented
	// by first decomposing into two Quaternions.
	// https://www.johndcook.com/blog/2018/07/09/octonioin-multiplication/
	Quaternion l_rijk, l_lmno, r_rijk, r_lmno;
	decompose_into_quaternions(l_rijk, l_lmno);
	p_right.decompose_into_quaternions(r_rijk, r_lmno);
	Quaternion rijk = (l_rijk * r_rijk) - (quaternion_conjugate(r_lmno) * l_lmno);
	Quaternion lmno = (r_lmno * l_rijk) + (l_lmno * quaternion_conjugate(r_rijk));
	return Octonion(rijk, lmno);
}

Octonion Octonion::operator-() const {
	return Octonion(-r, -i, -j, -k, -l, -m, -n, -o);
}

Octonion::operator String() const {
	return "(" + String::num_real(r, false) + ", " + String::num_real(i, false) + ", " + String::num_real(j, false) + ", " + String::num_real(k, false) + ", " + String::num_real(l, false) + ", " + String::num_real(m, false) + ", " + String::num_real(n, false) + ", " + String::num_real(o, false) + ")";
}

static_assert(sizeof(Octonion) == 8 * sizeof(real_t));
