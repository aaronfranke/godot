#ifndef TEST_FIXED_FUNCTION_PROJECTION_H
#define TEST_FIXED_FUNCTION_PROJECTION_H

#include "core/math/transform.h"
#include "drivers/fixed_function/graphics_device.h"

#include <stdio.h>

#include <vector>

#define FIXED_FUNCTION_PROJECTION_CHECK(m_condition)                            \
	do {                                                                        \
		if (!(m_condition)) {                                                   \
			printf("Projection FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                       \
		}                                                                       \
	} while (false)

class TestFixedFunctionProjection {
	typedef FixedFunctionGraphicsDevice::FFDrawParameters FFDrawParameters;

	static Error _draw_quad(FixedFunctionGraphicsDevice &p_device, float p_left, float p_top, float p_right, float p_bottom, float p_z, uint32_t p_color, const CameraMatrix &p_projection, const FFDrawParameters &p_state = FFDrawParameters()) {
		const FixedFunctionVertex vertices[] = {
			{ p_left, p_top, p_z, p_color, 0, 0 },
			{ p_right, p_top, p_z, p_color, 1, 0 },
			{ p_right, p_bottom, p_z, p_color, 1, 1 },
			{ p_left, p_top, p_z, p_color, 0, 0 },
			{ p_right, p_bottom, p_z, p_color, 1, 1 },
			{ p_left, p_bottom, p_z, p_color, 0, 1 },
		};
		return p_device.draw_triangles(vertices, 6, p_projection, RID(), p_state);
	}

	static bool _read_pixels(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.end_frame(false) == OK);
		r_pixels.resize(size_t(p_device.get_width()) * p_device.get_height());
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		return true;
	}

	static bool _color_matches(uint32_t p_actual, uint32_t p_expected) {
		for (int shift = 0; shift < 24; shift += 8) {
			const int difference = int((p_actual >> shift) & 255) - int((p_expected >> shift) & 255);
			if (difference < -8 || difference > 8 || (p_actual >> 24) != 255) {
				printf("Projection pixel: actual=0x%08x expected=0x%08x\n", p_actual, p_expected);
				return false;
			}
		}
		return true;
	}

	static bool _check_empty(const std::vector<uint32_t> &p_pixels) {
		for (size_t i = 0; i < p_pixels.size(); i++) {
			FIXED_FUNCTION_PROJECTION_CHECK(_color_matches(p_pixels[i], 0xff182430));
		}
		return true;
	}

	static CameraMatrix _camera_projection(bool p_perspective) {
		CameraMatrix projection;
		if (p_perspective) {
			projection.set_frustum(-1, 3, -2, 2, 2, 10);
		} else {
			// At camera depth 4 this has the same XY mapping as the asymmetric frustum.
			projection.set_orthogonal(-2, 6, -4, 4, 2, 10);
		}
		return projection;
	}

	static CameraMatrix _clip_from_model(const CameraMatrix &p_projection, const Transform &p_relative_model) {
		const Transform camera(Basis(Vector3(0, 1, 0), Math_PI / 2), Vector3(7, -3, 5));
		const Transform model = camera * p_relative_model;
		// Keep Godot's right-handed transforms and real_t composition until device packing.
		return p_projection * CameraMatrix(camera.affine_inverse()) * CameraMatrix(model);
	}

	static bool _check_clip_depth(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
		const float depths[] = { -1.25f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.25f };
		for (size_t i = 0; i < sizeof(depths) / sizeof(depths[0]); i++) {
			FIXED_FUNCTION_PROJECTION_CHECK(p_device.begin_frame(0xff182430) == OK);
			FIXED_FUNCTION_PROJECTION_CHECK(_draw_quad(p_device, -0.5f, 0.5f, 0.5f, -0.5f, depths[i], 0xffffffff, CameraMatrix()) == OK);
			FIXED_FUNCTION_PROJECTION_CHECK(_read_pixels(p_device, r_pixels));
			if (depths[i] < -1 || depths[i] > 1) {
				FIXED_FUNCTION_PROJECTION_CHECK(_check_empty(r_pixels));
			} else {
				const uint32_t width = p_device.get_width();
				FIXED_FUNCTION_PROJECTION_CHECK(r_pixels[size_t(p_device.get_height() / 2) * width + width / 2] == 0xffffffff);
				FIXED_FUNCTION_PROJECTION_CHECK(_color_matches(r_pixels[size_t(2) * width + 2], 0xff182430));
			}
		}
		return true;
	}

	static bool _check_camera_depth(FixedFunctionGraphicsDevice &p_device, bool p_perspective, std::vector<uint32_t> &r_pixels) {
		const struct {
			float depth;
			bool visible;
		} cases[] = { { 1.0f, false }, { 2.25f, true }, { 4.0f, true }, { 9.5f, true }, { 11.0f, false }, { -4.0f, false } };
		const CameraMatrix projection = _camera_projection(p_perspective);
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			const real_t depth = cases[i].depth;
			const real_t scale = p_perspective ? Math::abs(depth) / 4 : real_t(1);
			Basis model_basis;
			model_basis.scale(Vector3(scale, scale, 1));
			const Transform relative_model(model_basis, Vector3(p_perspective ? depth / 2 : real_t(2), 0, -depth));
			FIXED_FUNCTION_PROJECTION_CHECK(p_device.begin_frame(0xff182430) == OK);
			FIXED_FUNCTION_PROJECTION_CHECK(_draw_quad(p_device, -2, 2, 2, -2, 0, 0xffffffff, _clip_from_model(projection, relative_model)) == OK);
			FIXED_FUNCTION_PROJECTION_CHECK(_read_pixels(p_device, r_pixels));
			if (!cases[i].visible) {
				FIXED_FUNCTION_PROJECTION_CHECK(_check_empty(r_pixels));
			} else {
				const uint32_t width = p_device.get_width();
				FIXED_FUNCTION_PROJECTION_CHECK(r_pixels[size_t(p_device.get_height() / 2) * width + width / 2] == 0xffffffff);
				FIXED_FUNCTION_PROJECTION_CHECK(_color_matches(r_pixels[size_t(2) * width + 2], 0xff182430));
			}
		}
		return true;
	}

	static bool _render_camera(FixedFunctionGraphicsDevice &p_device, bool p_perspective, bool p_mirrored, bool p_clipped, std::vector<uint32_t> &r_pixels) {
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.begin_frame(0xff182430) == OK);
		const uint32_t width = p_device.get_width();
		const uint32_t height = p_device.get_height();
		FFDrawParameters state;
		state.clip_enabled = p_clipped;
		state.clip_rect = Rect2i(0, 0, width / 2, height);
		Transform relative_model(Basis(Vector3(0, 0, 1), Math_PI / 2), Vector3(2, 0, -4));
		if (p_mirrored) {
			relative_model.basis.scale_local(Vector3(-1, 1, 1));
		}
		const CameraMatrix projection = _camera_projection(p_perspective);
		const CameraMatrix clip = _clip_from_model(projection, relative_model);
		FIXED_FUNCTION_PROJECTION_CHECK(_draw_quad(p_device, -2, 2, 0, 0, 0, 0xffff0000, clip, state) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(_draw_quad(p_device, 0, 2, 2, 0, 0, 0xff00ff00, clip, state) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(_draw_quad(p_device, -2, 0, 0, -2, 0, 0xff0000ff, clip, state) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(_draw_quad(p_device, 0, 0, 2, -2, 0, 0xffffff00, clip, state) == OK);
		// The same camera-space marker shifts and shrinks with perspective, unlike orthographic.
		const Transform marker(Basis(), Vector3(2, 0, -8));
		FIXED_FUNCTION_PROJECTION_CHECK(_draw_quad(p_device, -0.5f, 0.5f, 0.5f, -0.5f, 0, 0xffffffff, _clip_from_model(projection, marker), state) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(_read_pixels(p_device, r_pixels));
		FIXED_FUNCTION_PROJECTION_CHECK(_color_matches(r_pixels[size_t(height * 3 / 8) * width + width * 3 / 8], p_mirrored ? 0xffff0000 : 0xff00ff00));
		FIXED_FUNCTION_PROJECTION_CHECK(_color_matches(r_pixels[size_t(height * 5 / 8) * width + width * 3 / 8], p_mirrored ? 0xff00ff00 : 0xffff0000));
		uint32_t top_right = 0xff182430, bottom_right = 0xff182430;
		if (!p_clipped) {
			top_right = p_mirrored ? 0xff0000ff : 0xffffff00;
			bottom_right = p_mirrored ? 0xffffff00 : 0xff0000ff;
		}
		FIXED_FUNCTION_PROJECTION_CHECK(_color_matches(r_pixels[size_t(height * 3 / 8) * width + width * 5 / 8], top_right));
		FIXED_FUNCTION_PROJECTION_CHECK(_color_matches(r_pixels[size_t(height * 5 / 8) * width + width * 5 / 8], bottom_right));
		FIXED_FUNCTION_PROJECTION_CHECK(_color_matches(r_pixels[size_t(2) * width + 2], 0xff182430));
		FIXED_FUNCTION_PROJECTION_CHECK(_color_matches(r_pixels[size_t(height / 2) * width + width / 4 - 2], 0xff182430));
		if (!p_clipped) {
			const uint32_t marker_x = p_perspective ? width * 3 / 8 : width / 2;
			const uint32_t other_x = p_perspective ? width / 2 : width * 3 / 8;
			FIXED_FUNCTION_PROJECTION_CHECK(r_pixels[size_t(height / 2) * width + marker_x] == 0xffffffff);
			FIXED_FUNCTION_PROJECTION_CHECK(r_pixels[size_t(height / 2) * width + other_x] != 0xffffffff);
			FIXED_FUNCTION_PROJECTION_CHECK((r_pixels[size_t(height / 2) * width + marker_x + width * 3 / 64] == 0xffffffff) == !p_perspective);
			FIXED_FUNCTION_PROJECTION_CHECK((r_pixels[size_t(height / 2 + height * 3 / 64) * width + marker_x] == 0xffffffff) == !p_perspective);
		}
		return true;
	}

	static bool _check_cameras(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
		FIXED_FUNCTION_PROJECTION_CHECK(_check_clip_depth(p_device, r_pixels));
		for (int perspective = 0; perspective < 2; perspective++) {
			FIXED_FUNCTION_PROJECTION_CHECK(_check_camera_depth(p_device, perspective != 0, r_pixels));
			FIXED_FUNCTION_PROJECTION_CHECK(_render_camera(p_device, perspective != 0, false, false, r_pixels));
			FIXED_FUNCTION_PROJECTION_CHECK(_render_camera(p_device, perspective != 0, true, false, r_pixels));
			FIXED_FUNCTION_PROJECTION_CHECK(_render_camera(p_device, perspective != 0, false, true, r_pixels));
		}
		return true;
	}

	static bool _check_indexed_triangles(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
		const FixedFunctionVertex vertices[] = {
			{ -0.75f, 0.75f, 0, 0xffff0000, 0, 0 },
			{ 0.75f, 0.75f, 0, 0xff00ff00, 1, 0 },
			{ 0.75f, -0.75f, 0, 0xff0000ff, 1, 1 },
			{ -0.75f, -0.75f, 0, 0xffffffff, 0, 1 },
		};
		const uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };
		const uint16_t invalid[] = { 0, 1, 4 };
		FixedFunctionVertex expanded[6];
		for (int i = 0; i < 6; i++) {
			expanded[i] = vertices[indices[i]];
		}
		const CameraMatrix projection;
		FFDrawParameters state;
		state.clip_enabled = true;
		state.clip_rect = Rect2i(0, 0, p_device.get_width() * 3 / 4, p_device.get_height());
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.begin_frame(0xff182430) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.draw_triangles(expanded, 6, projection, RID(), state) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(_read_pixels(p_device, r_pixels));
		const std::vector<uint32_t> reference = r_pixels;
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.begin_frame(0xff182430) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.draw_indexed_triangles(vertices, 4, reinterpret_cast<const uint8_t *>(invalid), 3, projection, RID(), state) == ERR_INVALID_DATA);
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.draw_indexed_triangles(vertices, 4, reinterpret_cast<const uint8_t *>(indices), 6, projection, RID(), state) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(_read_pixels(p_device, r_pixels) && r_pixels == reference);
		return true;
	}

public:
	static bool test(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
		const uint32_t width = p_device.get_width();
		const uint32_t height = p_device.get_height();
		FIXED_FUNCTION_PROJECTION_CHECK(_check_cameras(p_device, r_pixels));
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.resize(width + 63, height + 47) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(_check_cameras(p_device, r_pixels));
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.set_use_vsync(true) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(_check_cameras(p_device, r_pixels));
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.set_use_vsync(false) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.resize(0, 0) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.begin_frame(0) == ERR_SKIP);
		FIXED_FUNCTION_PROJECTION_CHECK(p_device.resize(width, height) == OK);
		FIXED_FUNCTION_PROJECTION_CHECK(_check_cameras(p_device, r_pixels));
		FIXED_FUNCTION_PROJECTION_CHECK(_render_camera(p_device, true, false, false, r_pixels));
		if (p_device.supports_indexed_triangles()) {
			FIXED_FUNCTION_PROJECTION_CHECK(_check_indexed_triangles(p_device, r_pixels));
		}
		printf("PASS: Godot clip depth, right-handed cameras, asymmetric perspective/orthographic transforms, mirrored geometry, clipping, and reset recovery.\n");
		return true;
	}
};

#undef FIXED_FUNCTION_PROJECTION_CHECK

#endif // TEST_FIXED_FUNCTION_PROJECTION_H
