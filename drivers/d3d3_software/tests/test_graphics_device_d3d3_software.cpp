#include "test_graphics_device_d3d3_software.h"

#include "core/os/os.h"
#include "drivers/d3d3_software/graphics_device_d3d3_software.h"
#include "drivers/fixed_function/tests/test_blending.h"
#include "drivers/fixed_function/tests/test_canvas_skinning.h"
#include "drivers/fixed_function/tests/test_clipping.h"
#include "drivers/fixed_function/tests/test_projection.h"
#include "drivers/fixed_function/tests/test_render_target_manager.h"
#include "drivers/fixed_function/tests/test_sky_hemispheres.h"
#include "drivers/fixed_function/tests/test_texture_adaptation.h"

#include <stdio.h>

#include <vector>

#define D3D3_SOFTWARE_TEST_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("D3D3 Software FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

class TestD3D3SoftwareWindow {
	static Error _draw_quad(FixedFunctionGraphicsDeviceD3D3Software &p_device, float p_z, uint32_t p_color, RID p_texture, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
		const FixedFunctionVertex vertices[] = {
			{ -0.5f, 0.5f, p_z, p_color, 0, 0 },
			{ 0.5f, 0.5f, p_z, p_color, 1, 0 },
			{ 0.5f, -0.5f, p_z, p_color, 1, 1 },
			{ -0.5f, 0.5f, p_z, p_color, 0, 0 },
			{ 0.5f, -0.5f, p_z, p_color, 1, 1 },
			{ -0.5f, -0.5f, p_z, p_color, 0, 1 },
		};
		return p_device.draw_triangles(vertices, 6, Projection(), p_texture, p_state);
	}

	static bool _test_resized_textures(FixedFunctionGraphicsDeviceD3D3Software &p_device, std::vector<uint32_t> &r_pixels) {
		const FixedFunctionGraphicsDevice::FFCapabilities &caps = p_device.get_capabilities();
		const int max_width = caps.max_texture_width;
		const int small_extent = caps.texture_power_of_two ? 16 : 12;
		const Size2i source_sizes[] = { Size2i(12, 12), Size2i(max_width + 1, 12) };
		const Size2i upload_sizes[] = { Size2i(small_extent, small_extent), Size2i(max_width, caps.texture_square_only ? max_width : small_extent) };
		r_pixels.resize(128 * 96);
		for (int i = 0; i < 2; i++) {
			const Size2i &size = source_sizes[i];
			Ref<Image> image = Image::create_empty(size.x, size.y, false, Image::FORMAT_RGBA8);
			image->fill(Color(0, 1, 0));
			image->fill_rect(Rect2(0, 0, size.x / 2, size.y), Color(1, 0, 0, 0));
			RID texture;
			D3D3_SOFTWARE_TEST_CHECK(p_device.texture_create(image, texture) == OK);
			D3D3_SOFTWARE_TEST_CHECK(image->get_width() == size.x && image->get_height() == size.y);
			FixedFunctionGraphicsDevice::FFTextureInfo info;
			D3D3_SOFTWARE_TEST_CHECK(p_device.texture_get_info(texture, info) == OK && info.size == size);
			const uint32_t texels = upload_sizes[i].x * upload_sizes[i].y;
			D3D3_SOFTWARE_TEST_CHECK(caps.blend_mix ? info.bytes == texels * 8 : (info.bytes == texels * 2 || info.bytes == texels * 4));
			FixedFunctionGraphicsDevice::FFDrawParameters state;
			if (caps.blend_mix) {
				state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
			}
			D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff0000ff) == OK);
			D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0, 0xffffffff, texture, state) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
			const uint32_t left = r_pixels[48 * 128 + 48];
			const uint32_t right = r_pixels[48 * 128 + 80];
			const uint32_t expected_left = caps.blend_mix ? 0xff0000ff : 0xffff0000;
			D3D3_SOFTWARE_TEST_CHECK((left & 0xfff8f8f8) == (expected_left & 0xfff8f8f8) && (right & 0xfff8f8f8) == 0xff00f800);
			D3D3_SOFTWARE_TEST_CHECK(p_device.texture_free(texture) == OK);
		}
		printf("PASS: D3D3 Software resizes NPOT and oversized image uploads while retaining source dimensions and texture coverage.\n");
		return true;
	}

	static bool _test_render_targets(FixedFunctionGraphicsDeviceD3D3Software &p_device, std::vector<uint32_t> &r_pixels) {
		RID child;
		if (!p_device.get_capabilities().render_target_opaque) {
			D3D3_SOFTWARE_TEST_CHECK(p_device.texture_create_render_target(63, 47, child) == ERR_UNAVAILABLE && !child.is_valid());
			return true;
		}
		RID parent;
		D3D3_SOFTWARE_TEST_CHECK(p_device.texture_create_render_target(63, 47, child) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.texture_create_render_target(75, 57, parent) == OK);
		D3D3_SOFTWARE_TEST_CHECK(!p_device.texture_is_render_target_ready(child));
		FixedFunctionGraphicsDevice::FFDrawParameters depth;
		depth.depth_test_enabled = depth.depth_write_enabled = true;
		D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff0000ff, child, true, true) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.get_target_width() == 63 && p_device.get_target_height() == 47);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0, 0xffffffff, child, depth) == ERR_INVALID_PARAMETER);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, -0.5f, 0xff00ff00, RID(), depth) == OK);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0.5f, 0xffff0000, RID(), depth) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.texture_is_render_target_ready(child));
		std::vector<uint32_t> snapshot(63 * 47, 0x12345678);
		D3D3_SOFTWARE_TEST_CHECK(p_device.texture_read_render_target(RID(), snapshot.data(), snapshot.size()) == ERR_INVALID_PARAMETER);
		D3D3_SOFTWARE_TEST_CHECK(p_device.texture_read_render_target(child, snapshot.data(), snapshot.size() - 1) == ERR_INVALID_PARAMETER && snapshot.front() == 0x12345678);
		D3D3_SOFTWARE_TEST_CHECK(p_device.texture_read_render_target(child, snapshot.data(), snapshot.size()) == OK);
		D3D3_SOFTWARE_TEST_CHECK((snapshot[23 * 63 + 31] & 0xffff00ff) == 0xff000000 && ((snapshot[23 * 63 + 31] >> 8) & 0xff) >= 248);

		D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xffff0000, parent) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.texture_read_render_target(child, snapshot.data(), snapshot.size()) == ERR_BUSY);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0, 0xffffffff, child, FixedFunctionGraphicsDevice::FFDrawParameters()) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.texture_is_render_target_ready(parent));
		D3D3_SOFTWARE_TEST_CHECK(p_device.resize(128, 96) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff000000) == OK);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0, 0xffffffff, parent, FixedFunctionGraphicsDevice::FFDrawParameters()) == OK);
		r_pixels.resize(128 * 96);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		const uint32_t center = r_pixels[48 * 128 + 64];
		D3D3_SOFTWARE_TEST_CHECK((center & 0xffff00ff) == 0xff000000 && ((center >> 8) & 0xff) >= 248);
		D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0, child, false, true) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false, true) == OK && !p_device.texture_is_render_target_ready(child));
		D3D3_SOFTWARE_TEST_CHECK(p_device.texture_read_render_target(child, snapshot.data(), snapshot.size()) == ERR_UNCONFIGURED);
		D3D3_SOFTWARE_TEST_CHECK(p_device.texture_free(child) == OK && p_device.texture_free(parent) == OK);
		printf("PASS: nested D3D3 Software render targets with non-power-of-two logical sizes, depth, discard, feedback protection and window resize.\n");
		return true;
	}

	static bool _test_canvas_fill(FixedFunctionGraphicsDeviceD3D3Software &p_device) {
		const uint32_t COLOR = 0xff3ab8e1;
		FixedFunctionVertex vertices[] = {
			{ 11, 17, 0, COLOR, 0, 0 },
			{ 83, 17, 0, COLOR, 0, 0 },
			{ 83, 67, 0, COLOR, 0, 0 },
			{ 11, 17, 0, COLOR, 0, 0 },
			{ 83, 67, 0, COLOR, 0, 0 },
			{ 11, 67, 0, COLOR, 0, 0 },
		};
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		if (p_device.get_capabilities().blend_mix) {
			state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		}
		state.clip_enabled = true;
		state.clip_rect = Rect2i(27, 9, 63, 44);
		D3D3_SOFTWARE_TEST_CHECK(p_device.resize(128, 96) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff000000) == OK);
		const Error fill = p_device.fill_canvas_triangles(vertices, 6, state);
		D3D3_SOFTWARE_TEST_CHECK(fill == OK || fill == ERR_UNAVAILABLE);
		if (fill == ERR_UNAVAILABLE) {
			D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false, true) == OK);
			return true;
		}
		vertices[0].color = 0x803ab8e1;
		D3D3_SOFTWARE_TEST_CHECK(p_device.fill_canvas_triangles(vertices, 6, state) == ERR_UNAVAILABLE);
		vertices[0].color = COLOR;
		std::vector<uint32_t> actual(128 * 96);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(actual.data(), actual.size()) == OK);
		Projection projection;
		projection.columns[0][0] = real_t(2) / 128;
		projection.columns[1][1] = real_t(-2) / 96;
		projection.columns[3][0] = -1;
		projection.columns[3][1] = 1;
		D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff000000) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.draw_triangles(vertices, 6, projection, RID(), state) == OK);
		std::vector<uint32_t> expected(128 * 96);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(expected.data(), expected.size()) == OK);
		D3D3_SOFTWARE_TEST_CHECK(actual == expected);

		const Point2 outer[] = { Point2(11, 17), Point2(83, 17), Point2(83, 67), Point2(11, 67) };
		const Point2 inner[] = { Point2(16, 20), Point2(79, 20), Point2(79, 67), Point2(16, 67) };
		FixedFunctionVertex panel[30];
		int index = 0;
		for (int i = 0; i < 4; i++) {
			const int next = (i + 1) % 4;
			const Point2 strip[] = { inner[i], inner[next], outer[i], outer[i], outer[next], inner[next] };
			for (const Point2 &point : strip) {
				panel[index++] = { float(point.x), float(point.y), 0, 0xffff7f00, 0, 0 };
			}
		}
		const int quad[] = { 0, 2, 1, 0, 3, 2 };
		for (int corner : quad) {
			panel[index++] = { float(inner[corner].x), float(inner[corner].y), 0, COLOR, 0, 0 };
		}
		for (int scale = 1; scale <= 2; scale++) {
			const int requested_width = scale == 1 ? 128 : 4096;
			const int width = MIN(requested_width, int(FixedFunctionGraphicsDeviceD3D3Software::MAX_RENDER_TARGET_LIMIT));
			const int height = 96 * scale;
			if (scale == 2) {
				for (FixedFunctionVertex &vertex : panel) {
					vertex.x *= scale;
					vertex.y *= scale;
				}
			}
			state.clip_rect = Rect2i(13 * scale, 9 * scale, 77 * scale, 51 * scale);
			projection.columns[0][0] = real_t(2) / width;
			projection.columns[1][1] = real_t(-2) / height;
			actual.resize(width * height);
			expected.resize(width * height);
			D3D3_SOFTWARE_TEST_CHECK(p_device.resize(requested_width, height) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff000000) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.fill_canvas_triangles(panel, index, state) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(actual.data(), actual.size()) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff000000) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.draw_triangles(panel, index, projection, RID(), state) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(expected.data(), expected.size()) == OK);
			D3D3_SOFTWARE_TEST_CHECK(actual == expected);
		}
		printf("PASS: DirectDraw solid and bordered fills match clipped D3D3 Software triangles, including capped windows.\n");
		return true;
	}

	static bool _test_canvas_viewport_copy(FixedFunctionGraphicsDeviceD3D3Software &p_device) {
		if (!p_device.get_capabilities().render_target_opaque) {
			return true;
		}
		RID target;
		D3D3_SOFTWARE_TEST_CHECK(p_device.resize(128, 96) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.texture_create_render_target(63, 47, target) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff0000ff, target) == OK);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0, 0xff00ff00, RID(), FixedFunctionGraphicsDevice::FFDrawParameters()) == OK);
		FixedFunctionGraphicsDevice::FFDrawParameters detail;
		detail.clip_enabled = true;
		detail.clip_rect = Rect2i(19, 16, 9, 10);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0, 0xffffff00, RID(), detail) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK);
		FixedFunctionVertex vertices[] = {
			{ 25, 18, 0, 0xffffffff, 0, 0 },
			{ 88, 18, 0, 0xffffffff, 1, 0 },
			{ 88, 65, 0, 0xffffffff, 1, 1 },
			{ 25, 18, 0, 0xffffffff, 0, 0 },
			{ 88, 65, 0, 0xffffffff, 1, 1 },
			{ 25, 65, 0, 0xffffffff, 0, 1 },
		};
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.clip_enabled = true;
		state.clip_rect = Rect2i(35, 22, 55, 39);
		D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xffff0000) == OK);
		vertices[0].color = 0xffeeffff;
		D3D3_SOFTWARE_TEST_CHECK(p_device.copy_canvas_render_target(vertices, 6, target, state) == ERR_UNAVAILABLE);
		vertices[0].color = 0xffffffff;
		vertices[0].v = 0.5f;
		D3D3_SOFTWARE_TEST_CHECK(p_device.copy_canvas_render_target(vertices, 6, target, state) == ERR_UNAVAILABLE);
		vertices[0].v = 0;
		const Error copy = p_device.copy_canvas_render_target(vertices, 6, target, state);
		D3D3_SOFTWARE_TEST_CHECK(copy == OK || copy == ERR_UNAVAILABLE);
		std::vector<uint32_t> pixels(128 * 96);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK);
		if (copy == OK) {
			D3D3_SOFTWARE_TEST_CHECK(pixels[22 * 128 + 34] == 0xffff0000 && pixels[21 * 128 + 35] == 0xffff0000);
			D3D3_SOFTWARE_TEST_CHECK(pixels[22 * 128 + 35] == 0xff0000ff && pixels[60 * 128 + 87] == 0xff0000ff);
			D3D3_SOFTWARE_TEST_CHECK((pixels[40 * 128 + 55] & 0xfff8f8f8) == 0xff00f800 && pixels[60 * 128 + 88] == 0xffff0000);
			printf("PASS: DirectDraw copies a clipped child viewport without sampling its padding.\n");
		}

		Projection projection;
		projection.columns[0][0] = real_t(2) / 128;
		projection.columns[1][1] = real_t(-2) / 96;
		projection.columns[3][0] = -1;
		projection.columns[3][1] = 1;
		std::vector<uint32_t> expected(128 * 96);
		for (int mode = 0; mode < 3; mode++) {
			FixedFunctionVertex quad[6];
			for (int i = 0; i < 6; i++) {
				quad[i] = vertices[i];
				if (mode != 0) {
					quad[i].x = 1 + 2 * (quad[i].x - 25);
					quad[i].y = 1 + 2 * (quad[i].y - 18);
				}
				if (mode != 1) {
					quad[i].u = 1 - quad[i].u;
					quad[i].v = 1 - quad[i].v;
				}
			}
			D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xffff0000) == OK);
			state.clip_enabled = true;
			if (mode != 0) {
				D3D3_SOFTWARE_TEST_CHECK(p_device.copy_canvas_render_target(quad, 6, target, state) == ERR_UNAVAILABLE);
				state.clip_enabled = false;
			}
			const Error stretch = p_device.copy_canvas_render_target(quad, 6, target, state);
			D3D3_SOFTWARE_TEST_CHECK(stretch == OK || stretch == ERR_UNAVAILABLE);
			D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK);
			if (stretch == ERR_UNAVAILABLE) {
				continue;
			}
			D3D3_SOFTWARE_TEST_CHECK(p_device.read_frame(pixels.data(), pixels.size()) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xffff0000) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.draw_triangles(quad, 6, projection, target, state) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(expected.data(), expected.size()) == OK);
			for (size_t i = 0; i < pixels.size(); i++) {
				// The software texture combiner can round a channel down when multiplying by white.
				for (int shift = 0; shift < 24; shift += 8) {
					D3D3_SOFTWARE_TEST_CHECK(Math::abs(int((pixels[i] >> shift) & 255) - int((expected[i] >> shift) & 255)) <= 1);
				}
			}
			printf("PASS: DirectDraw %s preserves point-sampled viewport coverage.\n", mode == 0 ? "clipped mirror" : "integer upscale");
		}
		D3D3_SOFTWARE_TEST_CHECK(p_device.texture_free(target) == OK);
		return true;
	}

	static bool _test_keyed_canvas_sprite(FixedFunctionGraphicsDeviceD3D3Software &p_device, std::vector<uint32_t> &r_pixels) {
		if (!p_device.get_capabilities().blend_mix) {
			return true;
		}
		D3D3_SOFTWARE_TEST_CHECK(p_device.resize(128, 96) == OK);
		r_pixels.resize(128 * 96);
		std::vector<uint32_t> reference(r_pixels.size());
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		state.clip_enabled = true;
		state.clip_rect = Rect2i(9, 11, 18, 16);
		const FixedFunctionVertex quad[] = {
			{ 7, 9, 0, 0xffffffff, 3.0f / 16, 4.0f / 16 },
			{ 16, 9, 0, 0xffffffff, 12.0f / 16, 4.0f / 16 },
			{ 16, 18, 0, 0xffffffff, 12.0f / 16, 13.0f / 16 },
			{ 7, 9, 0, 0xffffffff, 3.0f / 16, 4.0f / 16 },
			{ 16, 18, 0, 0xffffffff, 12.0f / 16, 13.0f / 16 },
			{ 7, 18, 0, 0xffffffff, 3.0f / 16, 13.0f / 16 },
		};
		Projection canvas;
		canvas[0][0] = real_t(2) / 128;
		canvas[1][1] = real_t(-2) / 96;
		canvas[3][0] = -1;
		canvas[3][1] = 1;
		// Changing an unused Z coefficient bypasses the copy while preserving the quad's projection.
		Projection triangle_projection = canvas;
		triangle_projection[2][2] = 0;
		for (int mode = 0; mode < 3; mode++) {
			Ref<Image> image = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
			for (int y = 0; y < 16; y++) {
				for (int x = 0; x < 16; x++) {
					image->set_pixel(x, y, Color(x / 16.0f, 1, y / 16.0f, (x + y) % 2 ? 1.0f : 0.0f));
				}
			}
			if (mode == 1) {
				image->set_pixel(6, 6, Color(1, 0, 1)); // Opaque texel collides with the color key.
			} else if (mode == 2) {
				image->set_pixel(6, 6, Color(1, 0, 0, 1.0f / 255.0f));
			}
			RID texture;
			D3D3_SOFTWARE_TEST_CHECK(p_device.texture_create(image, texture) == OK);
			const uint64_t before = p_device.get_texture_memory_usage();
			D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff182430) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.draw_triangles(quad, 6, triangle_projection, texture, state) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(reference.data(), reference.size()) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff182430) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.draw_triangles(quad, 6, canvas, texture, state) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
			if (mode == 0) {
				for (size_t i = 0; i < r_pixels.size(); i++) {
					// DirectDraw preserves the source color, while the RGB combiner rounds modulation and blending.
					for (int shift = 0; shift < 32; shift += 8) {
						D3D3_SOFTWARE_TEST_CHECK(Math::abs(int((r_pixels[i] >> shift) & 255) - int((reference[i] >> shift) & 255)) <= 3);
					}
				}
			} else {
				D3D3_SOFTWARE_TEST_CHECK(r_pixels == reference && p_device.get_texture_memory_usage() == before);
			}
			D3D3_SOFTWARE_TEST_CHECK(p_device.texture_free(texture) == OK && p_device.get_texture_memory_usage() == 0);
		}
		printf("PASS: clipped binary sprite coverage, color-key collisions, near-binary fallback and texture cleanup.\n");
		return true;
	}

	static bool _test_oversized_targets(FixedFunctionGraphicsDeviceD3D3Software &p_device, std::vector<uint32_t> &r_pixels) {
		enum { WIDTH = 4096,
			HEIGHT = 192 };
		const Size2i limit(FixedFunctionGraphicsDeviceD3D3Software::MAX_RENDER_TARGET_LIMIT, FixedFunctionGraphicsDeviceD3D3Software::MAX_RENDER_TARGET_LIMIT);
		D3D3_SOFTWARE_TEST_CHECK(p_device.resize(WIDTH, HEIGHT) == OK);
		FixedFunctionGraphicsDevice::FFDrawParameters clipped;
		clipped.clip_enabled = true;
		clipped.clip_rect = Rect2i(limit.x / 2, 0, limit.x / 2, HEIGHT);
		D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff0000ff) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.get_width() == uint32_t(limit.x) && p_device.get_height() == HEIGHT);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0, 0xff00ff00, RID(), clipped) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK);
		r_pixels.resize(limit.x * HEIGHT);
		D3D3_SOFTWARE_TEST_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		const size_t middle = HEIGHT / 2 * limit.x + limit.x / 2;
		D3D3_SOFTWARE_TEST_CHECK(r_pixels[middle - 8] == 0xff0000ff && (r_pixels[middle + 8] & 0xfff8f8f8) == 0xff00f800);
		if (p_device.get_capabilities().render_target_scaled) {
			RID target;
			D3D3_SOFTWARE_TEST_CHECK(p_device.texture_create_render_target(WIDTH, HEIGHT, target) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff0000ff, target) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.get_target_width() == WIDTH && p_device.get_target_height() == HEIGHT);
			clipped.clip_rect = Rect2i(WIDTH / 2, 0, WIDTH / 2, HEIGHT);
			D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0, 0xff00ff00, RID(), clipped) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff000000) == OK);
			D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0, 0xffffffff, target, FixedFunctionGraphicsDevice::FFDrawParameters()) == OK);
			D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
			D3D3_SOFTWARE_TEST_CHECK((r_pixels[middle - 32] & 0xfff8f8f8) == 0xff0000f8 && (r_pixels[middle + 32] & 0xfff8f8f8) == 0xff00f800);
			D3D3_SOFTWARE_TEST_CHECK(p_device.texture_free(target) == OK);
		}
		D3D3_SOFTWARE_TEST_CHECK(p_device.resize(128, 96) == OK);
		printf("PASS: D3D3 Software bounds native allocations and readback, preserving large offscreen viewport dimensions and sampling.\n");
		return true;
	}

public:
	static bool test_window_limits(HWND p_window) {
		FixedFunctionGraphicsDeviceD3D3Software device;
		D3D3_SOFTWARE_TEST_CHECK(device.initialize(p_window, 32, 24, false) == OK);
		D3D3_SOFTWARE_TEST_CHECK(device.get_max_window_size() == Size2i(2048, 2048));
		D3D3_SOFTWARE_TEST_CHECK(device.get_window_size(Size2i(4096, 768)) == Size2i(2048, 768));
		D3D3_SOFTWARE_TEST_CHECK(device.finalize() == OK);
		return true;
	}

	static bool test_binary_text(HWND p_window) {
		FixedFunctionGraphicsDeviceD3D3Software device;
		D3D3_SOFTWARE_TEST_CHECK(device.initialize(p_window, 32, 24, false) == OK);
		if (!device.get_capabilities().blend_mix) {
			return true;
		}
		Ref<Image> image = Image::create_empty(8, 8, false, Image::FORMAT_RGBA8);
		image->fill(Color(1, 1, 1, 127 / 255.0f));

		for (int y = 0; y < 8; y++) {
			for (int x = 4; x < 8; x++) {
				image->set_pixel(x, y, Color(1, 1, 1, 128 / 255.0f));
			}
		}

		RID texture;
		D3D3_SOFTWARE_TEST_CHECK(device.texture_create(image, texture) == OK);
		const uint64_t atlas_bytes = device.get_texture_memory_usage();
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		state.clip_enabled = true;
		state.clip_rect = Rect2i(8, 7, 5, 4);
		const Rect2 uv(0, 0, 1, 1);
		const Rect2 bounds(6, 5, 8, 8);
		const uint32_t background = 0xff123456;
		const uint32_t ink = 0xffff00ff;
		std::vector<uint32_t> pixels(32 * 24);
		for (int i = 0; i < 2; i++) {
			D3D3_SOFTWARE_TEST_CHECK(device.begin_frame(background) == OK);
			D3D3_SOFTWARE_TEST_CHECK(device.blit_canvas_glyph(texture, uv, Rect2(6.5f, 5, 8, 8), ink, state) == ERR_UNAVAILABLE);
			D3D3_SOFTWARE_TEST_CHECK(device.blit_canvas_glyph(texture, uv, bounds, 0x80ff00ff, state) == ERR_UNAVAILABLE);
			const Error blit = device.blit_canvas_glyph(texture, uv, bounds, ink, state);
			if (i == 0 && blit == ERR_UNAVAILABLE) {
				printf("SKIP: D3D3 Software binary text requires color-keyed DirectDraw blits.\n");
				D3D3_SOFTWARE_TEST_CHECK(device.end_frame(false) == OK);
				break;
			}
			D3D3_SOFTWARE_TEST_CHECK(blit == OK);
			D3D3_SOFTWARE_TEST_CHECK(device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK);
			D3D3_SOFTWARE_TEST_CHECK(pixels[7 * 32 + 9] == background && pixels[7 * 32 + 10] == ink && pixels[7 * 32 + 12] == ink && pixels[7 * 32 + 13] == background);
			D3D3_SOFTWARE_TEST_CHECK(device.get_texture_memory_usage() == atlas_bytes + 8 * 8 * 4);
		}
		state.clip_enabled = false;
		if (device.get_capabilities().sampler_filter_linear) {
			state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
		}
		state.font_glyph = true;
		D3D3_SOFTWARE_TEST_CHECK(device.begin_frame(background) == OK);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(device, 0, ink, texture, state) == OK);
		D3D3_SOFTWARE_TEST_CHECK(device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK);
		const uint32_t hole = pixels[12 * 32 + 10];
		const uint32_t covered = pixels[12 * 32 + 21];
		printf("D3D3 Software glyph triangle pixels: hole=%08x ink=%08x\n", hole, covered);
		for (int shift = 0; shift < 24; shift += 8) {
			D3D3_SOFTWARE_TEST_CHECK(Math::abs(int((hole >> shift) & 255) - int((background >> shift) & 255)) <= 2);
			D3D3_SOFTWARE_TEST_CHECK(Math::abs(int((covered >> shift) & 255) - int((ink >> shift) & 255)) <= 6);
		}
		state.font_glyph = false;
		D3D3_SOFTWARE_TEST_CHECK(device.begin_frame(background) == OK);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(device, 0, ink, texture, state) == OK);
		D3D3_SOFTWARE_TEST_CHECK(device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK);
		const uint32_t red = (pixels[12 * 32 + 10] >> 16) & 255;
		D3D3_SOFTWARE_TEST_CHECK(red >= 130 && red <= 140);
		D3D3_SOFTWARE_TEST_CHECK(device.texture_free(texture) == OK && device.get_texture_memory_usage() == 0);
		printf("PASS: D3D3 Software binary glyph coverage, filtered triangle fallback, shared atlas alpha and cache release.\n");
		return true;
	}

	static bool _test_sky_hemispheres(FixedFunctionGraphicsDeviceD3D3Software &p_device, std::vector<uint32_t> &r_pixels) {
		D3D3_SOFTWARE_TEST_CHECK(p_device.resize(128, 96) == OK && p_device.begin_frame(0xff000000, RID(), true, true) == OK);
		uint32_t calls = 0;
		D3D3_SOFTWARE_TEST_CHECK(p_device.draw_sky_hemispheres(real_t(0.5), 0xffff0000, 0xff0000ff, calls) == OK && calls == 2);
		FixedFunctionGraphicsDevice::FFDrawParameters depth;
		depth.depth_test_enabled = depth.depth_write_enabled = true;
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, -0.5f, 0xff00ff00, RID(), depth) == OK);
		D3D3_SOFTWARE_TEST_CHECK(_draw_quad(p_device, 0.5f, 0xffffff00, RID(), depth) == OK);
		r_pixels.resize(128 * 96);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		D3D3_SOFTWARE_TEST_CHECK(r_pixels.front() == 0xffff0000 && r_pixels.back() == 0xff0000ff && (r_pixels[48 * 128 + 64] & 0xfffcfcfc) == 0xff00fc00);
		D3D3_SOFTWARE_TEST_CHECK(p_device.begin_frame(0xff000000) == OK);
		D3D3_SOFTWARE_TEST_CHECK(p_device.draw_sky_hemispheres(1, 0xffff0000, 0xff0000ff, calls) == OK && calls == 1);
		D3D3_SOFTWARE_TEST_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		D3D3_SOFTWARE_TEST_CHECK(r_pixels.front() == 0xffff0000 && r_pixels.back() == 0xffff0000);
		printf("PASS: DirectDraw sky fills preserve foreground depth and count one or two fills.\n");
		return true;
	}

	static bool run(HWND p_window) {
		FixedFunctionGraphicsDeviceD3D3Software device;
		const Error initialized = device.initialize(p_window, 128, 96, false);
		printf("D3D3 Software initialize: %d\n", initialized);
		D3D3_SOFTWARE_TEST_CHECK(initialized == OK);
		D3D3_SOFTWARE_TEST_CHECK(device.get_capabilities().depth_buffer);
		std::vector<uint32_t> pixels;
		D3D3_SOFTWARE_TEST_CHECK(_test_keyed_canvas_sprite(device, pixels));
		D3D3_SOFTWARE_TEST_CHECK(_test_resized_textures(device, pixels));
		D3D3_SOFTWARE_TEST_CHECK(_test_sky_hemispheres(device, pixels));
		RID texture;
		Ref<Image> image = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
		image->fill(Color(1, 1, 0));
		D3D3_SOFTWARE_TEST_CHECK(device.texture_create(image, texture) == OK);
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.depth_test_enabled = state.depth_write_enabled = true;
		state.cull_mode = FixedFunctionGraphicsDevice::CULL_BACK;
		for (int i = 0; i < 2; i++) {
			const uint32_t width = i == 0 ? 128 : 144;
			const uint32_t height = i == 0 ? 96 : 104;
			D3D3_SOFTWARE_TEST_CHECK(device.resize(width, height) == OK);
			pixels.resize(size_t(width) * height);
			D3D3_SOFTWARE_TEST_CHECK(device.begin_frame(0xff0000ff, RID(), true, true) == OK);
			D3D3_SOFTWARE_TEST_CHECK(_draw_quad(device, -0.5f, 0xff00ffff, texture, state) == OK);
			D3D3_SOFTWARE_TEST_CHECK(_draw_quad(device, 0.5f, 0xffff0000, RID(), state) == OK);
			D3D3_SOFTWARE_TEST_CHECK(device.end_frame(false) == OK);
			D3D3_SOFTWARE_TEST_CHECK(device.read_frame(pixels.data(), pixels.size()) == OK);
			const uint32_t center = pixels[(height / 2) * width + width / 2];
			printf("D3D3 Software window %ux%u: corner=%08x center=%08x\n", width, height, pixels[0], center);
			D3D3_SOFTWARE_TEST_CHECK(pixels[0] == 0xff0000ff && (center & 0xffff00ff) == 0xff000000 && ((center >> 8) & 0xff) >= 248);
		}
		D3D3_SOFTWARE_TEST_CHECK(device.texture_free(texture) == OK && device.get_texture_memory_usage() == 0);
		D3D3_SOFTWARE_TEST_CHECK(TestFixedFunctionProjection::test(device, pixels));
		D3D3_SOFTWARE_TEST_CHECK(TestFixedFunctionClipping::test(device, pixels));
		D3D3_SOFTWARE_TEST_CHECK(TestFixedFunctionBlending::test(device, pixels));
		const uint32_t vertices_per_batch = device.get_capabilities().max_primitives_per_draw * 3;
		std::vector<FixedFunctionVertex> vertices(vertices_per_batch + 3);
		const FixedFunctionVertex triangle[] = {
			{ -0.5f, 0.5f, 0, 0xffff0000, 0, 0 },
			{ 0.5f, 0.5f, 0, 0xffff0000, 0, 0 },
			{ 0, -0.5f, 0, 0xffff0000, 0, 0 },
		};
		for (uint32_t i = 0; i < vertices_per_batch; i++) {
			vertices[i] = triangle[i % 3];
			vertices[i].color = 0xff0000ff;
		}
		for (uint32_t i = 0; i < 3; i++) {
			vertices[vertices_per_batch + i] = triangle[i];
		}
		D3D3_SOFTWARE_TEST_CHECK(device.begin_frame(0xff000000) == OK);
		D3D3_SOFTWARE_TEST_CHECK(device.draw_triangles(vertices.data(), vertices.size(), Projection()) == OK);
		D3D3_SOFTWARE_TEST_CHECK(device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK);
		D3D3_SOFTWARE_TEST_CHECK(pixels[(device.get_height() / 2) * device.get_width() + device.get_width() / 2] == 0xffff0000);
		D3D3_SOFTWARE_TEST_CHECK(_test_render_targets(device, pixels));
		D3D3_SOFTWARE_TEST_CHECK(_test_canvas_fill(device));
		D3D3_SOFTWARE_TEST_CHECK(_test_canvas_viewport_copy(device));
		D3D3_SOFTWARE_TEST_CHECK(_test_oversized_targets(device, pixels));
		D3D3_SOFTWARE_TEST_CHECK(device.begin_frame(0xff000000) == OK && device.end_frame(true) == OK);
		D3D3_SOFTWARE_TEST_CHECK(device.read_frame(pixels.data(), pixels.size()) == ERR_UNCONFIGURED);
		D3D3_SOFTWARE_TEST_CHECK(device.finalize() == OK);
		D3D3_SOFTWARE_TEST_CHECK(TestFixedFunctionRenderTargetManager::test(device, p_window, pixels));
		D3D3_SOFTWARE_TEST_CHECK(TestFixedFunctionSkyHemispheres::test(device, p_window));
		FixedFunctionRenderContext context(device, p_window, 128, 96, false);
		D3D3_SOFTWARE_TEST_CHECK(context.initialize() == OK);
		D3D3_SOFTWARE_TEST_CHECK(TestFixedFunctionTextureAdaptation::test(context, device, pixels));
		FixedFunctionRenderTargetManager targets(context);
		D3D3_SOFTWARE_TEST_CHECK(TestFixedFunctionCanvasSkinning::test(context, targets, pixels));
		D3D3_SOFTWARE_TEST_CHECK(context.finalize() == OK);
		printf("PASS: Direct3D 3 RGB software device, execute buffers, texture modulation, depth, resize and presentation.\n");
		return true;
	}
};

bool TestFixedFunctionGraphicsDeviceD3D3Software::test() {
	WNDCLASSA window_class = {};
	window_class.lpfnWndProc = DefWindowProcA;
	window_class.hInstance = GetModuleHandleA(nullptr);
	window_class.lpszClassName = "GodotD3D3SoftwareProbe";
	if (!RegisterClassA(&window_class)) {
		return false;
	}
	HWND window = CreateWindowA(window_class.lpszClassName, "Godot Direct3D 3 probe", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 320, 240, nullptr, nullptr, window_class.hInstance, nullptr);
	bool passed = false;
	if (window) {
		ShowWindow(window, SW_SHOWNOACTIVATE);
		passed = TestD3D3SoftwareWindow::test_window_limits(window) && TestD3D3SoftwareWindow::run(window) && TestD3D3SoftwareWindow::test_binary_text(window);
		DestroyWindow(window);
	}
	UnregisterClassA(window_class.lpszClassName, window_class.hInstance);
	return passed;
}

#undef D3D3_SOFTWARE_TEST_CHECK

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_graphics_device_d3d3_software)

TEST_CASE("[FixedFunction][D3D3 Software] Native window device probe" * doctest::skip()) {
	CHECK(TestFixedFunctionGraphicsDeviceD3D3Software::test());
}
