#ifndef BASIS_4D_H
#define BASIS_4D_H

#include "vector4.h"

struct _NO_DISCARD_ Basis4D {
	// IMPORTANT NOTE: Godot stores 2D basis vectors with columns,
	// but stores 3D basis vectors with rows. Columns are much more
	// intuitive work with, the only reason Godot stores rows is
	// for low-level optimization reasons. For Basis4D, we store
	// columns, NOT rows, for improved code readability.
	Vector4 x = Vector4(1, 0, 0, 0);
	Vector4 y = Vector4(0, 1, 0, 0);
	Vector4 z = Vector4(0, 0, 1, 0);
	Vector4 w = Vector4(0, 0, 0, 1);

	_FORCE_INLINE_ const Vector4 &operator[](unsigned int p_index) const {
		DEV_ASSERT((unsigned int)p_index < 4);
		if (p_index == 0) {
			return x;
		} else if (p_index == 1) {
			return y;
		} else if (p_index == 2) {
			return z;
		}
		return w;
	}

	_FORCE_INLINE_ Vector4 &operator[](unsigned int p_index) {
		DEV_ASSERT((unsigned int)p_index < 4);
		if (p_index == 0) {
			return x;
		} else if (p_index == 1) {
			return y;
		} else if (p_index == 2) {
			return z;
		}
		return w;
	}

	void invert();
	void transpose();

	Basis4D inverse() const;
	Basis4D transposed() const;

	real_t determinant() const;

	void scale(const Vector4 &p_scale);
	Basis4D scaled(const Vector4 &p_scale) const;

	void scale_local(const Vector4 &p_scale);
	Basis4D scaled_local(const Vector4 &p_scale) const;

	void set_scale(const Vector4 &p_scale);
	Vector4 get_scale() const;
	Vector4 get_scale_abs() const;

	bool is_equal_approx(const Basis4D &p_basis) const;

	bool operator==(const Basis4D &p_matrix) const;
	bool operator!=(const Basis4D &p_matrix) const;

	Vector4 xform(const Vector4 &p_vector) const;
	Vector4 xform_transposed(const Vector4 &p_vector) const;
	void operator*=(const Basis4D &p_matrix);
	Basis4D operator*(const Basis4D &p_matrix) const;
	Vector4 operator*(const Vector4 &p_vector) const;
	void operator+=(const Basis4D &p_matrix);
	Basis4D operator+(const Basis4D &p_matrix) const;
	void operator-=(const Basis4D &p_matrix);
	Basis4D operator-(const Basis4D &p_matrix) const;
	void operator*=(const real_t p_val);
	Basis4D operator*(const real_t p_val) const;

	bool is_orthogonal() const;
	bool is_diagonal() const;
	bool is_rotation() const;

	Basis4D lerp(const Basis4D &p_to, const real_t &p_weight) const;

	operator String() const;

	/* create / set */

	_FORCE_INLINE_ Vector4 get_column(int p_index) const {
		return this->operator[](p_index);
	}

	_FORCE_INLINE_ void set_column(int p_index, const Vector4 &p_value) {
		this->operator[](p_index) = p_value;
	}

	_FORCE_INLINE_ Vector4 get_row(int i) const {
		return Vector4(x[i], y[i], z[i], w[i]);
	}

	_FORCE_INLINE_ void set_row(int i, const Vector4 &p_row) {
		x[i] = p_row.x;
		y[i] = p_row.y;
		z[i] = p_row.z;
		w[i] = p_row.w;
	}

	_FORCE_INLINE_ Vector4 get_main_diagonal() const {
		return Vector4(x.x, y.y, z.z, w.w);
	}

	void orthonormalize();
	Basis4D orthonormalized() const;

	void orthogonalize();
	Basis4D orthogonalized() const;

	static Basis4D from_scale(const Vector4 &p_scale);
	static Basis4D from_yz(const real_t p_yz);
	static Basis4D from_zx(const real_t p_zx);
	static Basis4D from_xy(const real_t p_xy);
	static Basis4D from_xw(const real_t p_xw);
	static Basis4D from_yw(const real_t p_yw);
	static Basis4D from_zw(const real_t p_zw);

	_FORCE_INLINE_ Basis4D() {}
	_FORCE_INLINE_ Basis4D(const Vector4 &p_x, const Vector4 &p_y, const Vector4 &p_z, const Vector4 &p_w) {
		x = p_x;
		y = p_y;
		z = p_z;
		w = p_w;
	}
};

#endif // BASIS_4D_H
