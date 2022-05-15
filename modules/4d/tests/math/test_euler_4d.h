#ifndef TEST_EULER_4D_H
#define TEST_EULER_4D_H

#include "core/math/random_number_generator.h"
#include "modules/4d/math/euler_4d.h"

#include "tests/test_macros.h"

namespace TestEuler4D {
TEST_CASE("[Euler4D] Basis4D to Euler4D round-trip conversions") {
	RandomNumberGenerator rng;

	// Identity basis should have identity rotation.
	Basis4D basis1 = Basis4D();
	Euler4D euler = Euler4D::from_basis(basis1);
	Basis4D basis2 = euler.to_basis();
	CHECK_MESSAGE(
			basis1.is_equal_approx(basis2),
			"Basis4D to Euler4D round-trip conversion should give the same output as the input.");
	CHECK_MESSAGE(
			euler.is_equal_approx(Euler4D()),
			"Basis4D to Euler4D conversion should give the expected angles.");

	// Rotation of 90 degrees on the XY plane.
	basis1 = Basis4D();
	basis1.x.x = basis1.y.y = 0;
	basis1.x.y = 1;
	basis1.y.x = -1;
	euler = Euler4D::from_basis(basis1);
	basis2 = euler.to_basis();
	CHECK_MESSAGE(
			basis1.is_equal_approx(basis2),
			"Basis4D to Euler4D round-trip conversion should give the same output as the input.");
	CHECK_MESSAGE(
			euler.is_equal_approx(Euler4D(0, 0, Math_TAU / 4)),
			"Basis4D to Euler4D conversion should give the expected angles.");

	// Rotation of 90 degrees on the XZ plane.
	basis1 = Basis4D();
	basis1.x.x = basis1.z.z = 0;
	basis1.x.z = -1;
	basis1.z.x = 1;
	euler = Euler4D::from_basis(basis1);
	basis2 = euler.to_basis();
	CHECK_MESSAGE(
			basis1.is_equal_approx(basis2),
			"Basis4D to Euler4D round-trip conversion should give the same output as the input.");
	CHECK_MESSAGE(
			euler.is_equal_approx(Euler4D(0, Math_TAU / 4, 0)),
			"Basis4D to Euler4D conversion should give the expected angles.");

	// Rotation of 90 degrees on the YZ plane.
	basis1 = Basis4D();
	basis1.y.y = basis1.z.z = 0;
	basis1.y.z = 1;
	basis1.z.y = -1;
	euler = Euler4D::from_basis(basis1);
	basis2 = euler.to_basis();
	CHECK_MESSAGE(
			basis1.is_equal_approx(basis2),
			"Basis4D to Euler4D round-trip conversion should give the same output as the input.");
	CHECK_MESSAGE(
			euler.is_equal_approx(Euler4D(Math_TAU / 4, 0, 0)),
			"Basis4D to Euler4D conversion should give the expected angles.");

	// Rotation of 90 degrees on the XW plane.
	basis1 = Basis4D();
	basis1.x.x = basis1.w.w = 0;
	basis1.x.w = 1;
	basis1.w.x = -1;
	euler = Euler4D::from_basis(basis1);
	basis2 = euler.to_basis();
	CHECK_MESSAGE(
			basis1.is_equal_approx(basis2),
			"Basis4D to Euler4D round-trip conversion should give the same output as the input.");
	CHECK_MESSAGE(
			euler.is_equal_approx(Euler4D(0, 0, 0, Math_TAU / 4, 0, 0)),
			"Basis4D to Euler4D conversion should give the expected angles.");

	// Rotation of 90 degrees on the YW plane.
	basis1 = Basis4D();
	basis1.y.y = basis1.w.w = 0;
	basis1.y.w = 1;
	basis1.w.y = -1;
	euler = Euler4D::from_basis(basis1);
	basis2 = euler.to_basis();
	CHECK_MESSAGE(
			basis1.is_equal_approx(basis2),
			"Basis4D to Euler4D round-trip conversion should give the same output as the input.");
	CHECK_MESSAGE(
			euler.is_equal_approx(Euler4D(0, 0, 0, 0, Math_TAU / 4, 0)),
			"Basis4D to Euler4D conversion should give the expected angles.");

	// Rotation of 90 degrees on the ZW plane.
	basis1 = Basis4D();
	basis1.z.z = basis1.w.w = 0;
	basis1.z.w = 1;
	basis1.w.z = -1;
	euler = Euler4D::from_basis(basis1);
	basis2 = euler.to_basis();
	CHECK_MESSAGE(
			basis1.is_equal_approx(basis2),
			"Basis4D to Euler4D round-trip conversion should give the same output as the input.");
	CHECK_MESSAGE(
			euler.is_equal_approx(Euler4D(0, 0, 0, 0, 0, Math_TAU / 4)),
			"Basis4D to Euler4D conversion should give the expected angles.");
}

TEST_CASE("[Euler4D] Euler4D to Basis4D round-trip conversions") {
	RandomNumberGenerator rng;
	for (int i = 0; i < 100; i++) {
		Euler4D euler = Euler4D(
				// We use 3.14 and 1.57 instead of PI or TAU because we want to avoid the edge cases.
				rng.randf_range(-1.57, 1.57), // YZ, Pitch
				rng.randf_range(-3.14, 3.14), // XZ, Yaw 1, yaw-like
				rng.randf_range(-1.57, 1.57), // XY, Roll 1
				rng.randf_range(-3.14, 3.14), // XW, Yaw 3, roll-like
				rng.randf_range(-1.57, 1.57), // YW, Roll 2
				rng.randf_range(-1.57, 1.57)); // ZW, Yaw 2, pitch-like
		Basis4D basis = euler.to_basis();
		Euler4D euler2 = Euler4D::from_basis(basis);
		CHECK_MESSAGE(
				euler.is_equal_approx(euler2),
				"Euler4D to Basis4D round-trip conversion should work as expected.");
	}
}
} // namespace TestEuler4D

#endif // TEST_EULER_4D_H
