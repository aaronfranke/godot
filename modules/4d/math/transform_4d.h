#ifndef TRANSFORM_4D_H
#define TRANSFORM_4D_H

#include "basis_4d.h"
#include "core/templates/vector.h"

struct _NO_DISCARD_ Transform4D {
	Basis4D basis;
	Vector4 origin;

	_FORCE_INLINE_ const Vector4 &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 5);
		if (p_axis == 4) {
			return origin;
		}
		return basis[p_axis];
	}

	_FORCE_INLINE_ Vector4 &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 5);
		if (p_axis == 4) {
			return origin;
		}
		return basis[p_axis];
	}

	void invert();
	Transform4D inverse() const;

	void affine_invert();
	Transform4D affine_inverse() const;

	void set_scale(const Vector4 &p_scale);
	Vector4 get_scale() const;
	Vector4 get_scale_abs() const;

	void scale(const Vector4 &p_scale);
	Transform4D scaled(const Vector4 &p_scale) const;
	void scale_basis(const Vector4 &p_scale);
	void translate_local(const Vector4 &p_translation);
	Transform4D translated_local(const Vector4 &p_translation) const;

	const Basis4D &get_basis() const { return basis; }
	void set_basis(const Basis4D &p_basis) { basis = p_basis; }

	const Vector4 &get_origin() const { return origin; }
	void set_origin(const Vector4 &p_origin) { origin = p_origin; }

	void orthonormalize();
	Transform4D orthonormalized() const;
	void orthogonalize();
	Transform4D orthogonalized() const;
	bool is_equal_approx(const Transform4D &p_transform) const;

	bool operator==(const Transform4D &p_transform) const;
	bool operator!=(const Transform4D &p_transform) const;

	Vector4 xform(const Vector4 &p_vector) const;
	Vector<Vector4> xform(const Vector<Vector4> &p_array) const;

	// NOTE: These are UNSAFE with non-uniform scaling, and will produce incorrect results.
	// They use the transpose.
	// For safe inverse transforms, xform by the affine_inverse.
	Vector4 xform_transposed(const Vector4 &p_vector) const;
	Vector<Vector4> xform_transposed(const Vector<Vector4> &p_array) const;

	void operator*=(const Transform4D &p_transform);
	Transform4D operator*(const Transform4D &p_transform) const;
	Basis4D operator*(const Basis4D &p_basis) const;
	Vector4 operator*(const Vector4 &p_vector) const;
	void operator*=(const real_t p_val);
	Transform4D operator*(const real_t p_val) const;

	Transform4D interpolate_with(const Transform4D &p_transform, real_t p_c) const;

	operator String() const;

	Transform4D() {}
	Transform4D(const Basis4D &p_basis, const Vector4 &p_origin = Vector4());
	Transform4D(const Vector4 &p_x, const Vector4 &p_y, const Vector4 &p_z, const Vector4 &p_w, const Vector4 &p_origin = Vector4());
};

#endif // TRANSFORM_4D_H
