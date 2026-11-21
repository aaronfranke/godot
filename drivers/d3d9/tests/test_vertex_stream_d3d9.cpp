#include "drivers/d3d9/tests/test_graphics_device_d3d9.h"
#include "drivers/fixed_function/tests/test_vertex_submission.h"

#include <stdio.h>

#define CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("D3D9 stream FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionGraphicsDeviceD3D9::_test_vertex_stream(HWND p_window) {
	FixedFunctionGraphicsDeviceD3D9 device;
	CHECK(device.initialize(p_window, 64, 64, false) == OK);
	const uint32_t capacity = FixedFunctionGraphicsDeviceD3D9::VERTEX_STREAM_CAPACITY;
	const FixedFunctionVertex outside = { 2, 2, 0, 0xffffffff, 0, 0 };
	std::vector<FixedFunctionVertex> filler(capacity + 3, outside);
	const FixedFunctionVertex left[] = {
		{ -0.9f, -0.8f, 0, 0xffff0000, 0, 0 },
		{ -0.9f, 0.8f, 0, 0xffff0000, 0, 0 },
		{ -0.1f, -0.8f, 0, 0xffff0000, 0, 0 },
	};
	const FixedFunctionVertex right[] = {
		{ 0.1f, -0.8f, 0, 0xff00ff00, 0, 0 },
		{ 0.1f, 0.8f, 0, 0xff00ff00, 0, 0 },
		{ 0.9f, -0.8f, 0, 0xff00ff00, 0, 0 },
	};
	FixedFunctionLightingVertex lit[3] = {};
	for (int i = 0; i < 3; i++) {
		lit[i].x = left[i].x + 0.3f;
		lit[i].y = left[i].y * 0.25f + 0.65f;
		lit[i].nz = 1;
		lit[i].b = lit[i].a = 1;
	}
	FixedFunctionLightingVertex lit_outside = {};
	lit_outside.x = lit_outside.y = 2;
	lit_outside.nz = lit_outside.a = 1;
	std::vector<FixedFunctionLightingVertex> lit_filler(capacity - 3, lit_outside);
	const uint16_t indices[] = { 0, 1, 2, 0, 1, 2 };
	std::vector<uint16_t> filler_indices(capacity - 3);
	for (uint32_t i = 0; i < capacity - 3; i++) {
		filler_indices[i] = uint16_t(i);
	}
	FixedFunctionLightingVertex lit_after[3];
	for (int i = 0; i < 3; i++) {
		lit_after[i] = lit[i];
		lit_after[i].x += 0.8f;
		lit_after[i].g = 1;
	}
	FixedFunctionGraphicsDevice::FFPixelLighting lighting;
	lighting.ambient = Color(1, 1, 1);
	lighting.light_count = 0;
	const bool per_pixel = device.get_max_per_pixel_lights() != 0;
	bool streamed = false;
	bool lit_streamed = false;
	for (int cycle = 0; cycle < 2; cycle++) {
		if (cycle) {
			CHECK(device.resize(96, 64) == OK && device.prepare_resources() == OK);
			CHECK(!device._vertex_stream.native && !device._lighting_vertex_stream.native && !device._stream_indices);
			CHECK(device._vertex_stream.position == 0 && device._lighting_vertex_stream.position == 0);
			CHECK(!device._vertex_stream.unavailable && !device._lighting_vertex_stream.unavailable);
		}
		std::vector<uint32_t> reference;
		for (int immediate = 0; immediate < 3; immediate++) {
			if (immediate == 2) {
				device._release_vertex_streams();
			}
			if (immediate) {
				device._vertex_stream.unavailable = device._lighting_vertex_stream.unavailable = true;
			}
			CHECK(device.begin_frame(0xff000000) == OK);
			CHECK(device.draw_triangles(left, 3, Projection()) == OK);
			CHECK(device.draw_triangles(filler.data(), capacity - 3, Projection()) == OK);
			if (per_pixel) {
				const bool indexed = immediate != 1;
				const FixedFunctionGraphicsDevice::FFDrawParameters state;
				CHECK(device.draw_lit_triangles(lit, 3, Projection(), lighting, RID(), state, indexed ? indices : nullptr, indexed ? 6 : 0) == OK);
				CHECK(device.draw_lit_triangles(lit_filler.data(), capacity - 3, Projection(), lighting, RID(), state, indexed ? filler_indices.data() : nullptr, indexed ? capacity - 3 : 0) == OK);
			}
			// An oversized immediate draw clears stream 0; the next buffered draw must rebind it.
			CHECK(device.draw_triangles(filler.data(), capacity + 3, Projection()) == OK);
			CHECK(device.draw_triangles(right, 3, Projection()) == OK);
			if (per_pixel) {
				const bool indexed = immediate != 1;
				CHECK(device.draw_lit_triangles(lit_after, 3, Projection(), lighting, RID(), FixedFunctionGraphicsDevice::FFDrawParameters(), indexed ? indices : nullptr, indexed ? 6 : 0) == OK);
			}
			CHECK(device.end_frame(false) == OK);
			std::vector<uint32_t> pixels(size_t(device.get_width()) * device.get_height());
			CHECK(device.read_frame(pixels.data(), pixels.size()) == OK);
			CHECK(pixels[48 * device.get_width() + device.get_width() / 5] == 0xffff0000);
			CHECK(pixels[48 * device.get_width() + device.get_width() * 7 / 10] == 0xff00ff00);
			if (per_pixel) {
				CHECK(pixels[12 * device.get_width() + device.get_width() * 3 / 10] == 0xff0000ff);
				CHECK(pixels[12 * device.get_width() + device.get_width() * 7 / 10] == 0xff00ffff);
			}
			if (!immediate) {
				streamed = streamed || device._vertex_stream.native != nullptr;
				lit_streamed = lit_streamed || device._lighting_vertex_stream.native != nullptr;
				reference = pixels;
			} else {
				CHECK(pixels == reference);
			}
		}
	}
	CHECK(device.resize(64, 64) == OK && device.prepare_resources() == OK);
	CHECK(TestFixedFunctionVertexSubmission::benchmark(device, "d3d9_stream"));
	device._vertex_stream.unavailable = device._lighting_vertex_stream.unavailable = true;
	CHECK(TestFixedFunctionVertexSubmission::benchmark(device, "d3d9_immediate"));
	CHECK(device.finalize() == OK);
	CHECK(!device._vertex_stream.native && !device._lighting_vertex_stream.native);
	CHECK(device.initialize(p_window, 64, 64, false) == OK);
	CHECK(device.begin_frame(0xff000000) == OK);
	CHECK(device.draw_triangles(left, 3, Projection()) == OK);
	CHECK(device.end_frame(false) == OK);
	CHECK(device.begin_frame(0xff000000, RID(), false) == OK);
	CHECK(device.draw_triangles(right, 3, Projection()) == OK);
	CHECK(device.end_frame(false) == OK);
	std::vector<uint32_t> preserved(64 * 64);
	CHECK(device.read_frame(preserved.data(), preserved.size()) == OK);
	CHECK(preserved[48 * 64 + 12] == 0xffff0000 && preserved[48 * 64 + 44] == 0xff00ff00);
	CHECK(device.finalize() == OK);
	printf("PASS: D3D9 vertex streams (ordinary=%d, lit=%d), wrap, mixed immediate draws, pixel-equivalent fallback, reset and reinitialization.\n", int(streamed), int(lit_streamed));
	return true;
}

#undef CHECK
