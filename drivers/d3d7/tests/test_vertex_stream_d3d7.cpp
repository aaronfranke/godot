#include "drivers/d3d7/graphics_device_d3d7.h"
#include "drivers/d3d7/tests/test_graphics_device_d3d7.h"
#include "drivers/fixed_function/tests/test_vertex_submission.h"

#include <stdio.h>

#define CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("D3D7 stream FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionGraphicsDeviceD3D7::_test_vertex_stream(HWND p_window, bool p_force_rgb) {
	FixedFunctionGraphicsDeviceD3D7 device(D3D7ResolutionPolicy::D3D7_OVERSIZE_RESTRICTED, p_force_rgb);
	CHECK(device.initialize(p_window, 64, 64, false) == OK);
	const uint32_t capacity = FixedFunctionGraphicsDeviceD3D7::VERTEX_STREAM_CAPACITY;
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
	bool streamed = false;
	for (int cycle = 0; cycle < 2; cycle++) {
		if (cycle) {
			CHECK(device.resize(96, 64) == OK && device.prepare_resources() == OK);
			CHECK(!device._vertex_buffer && device._vertex_buffer_position == 0 && !device._vertex_buffer_unavailable);
		}
		std::vector<uint32_t> reference;
		for (int immediate = 0; immediate < 3; immediate++) {
			if (immediate == 2) {
				device._release_vertex_stream();
			}
			if (immediate) {
				device._vertex_buffer_unavailable = true;
			}
			CHECK(device.begin_frame(0xff000000) == OK);
			CHECK(device.draw_triangles(left, 3, Projection()) == OK);
			CHECK(device.draw_triangles(filler.data(), capacity - 3, Projection()) == OK);
			CHECK(device.draw_triangles(filler.data(), capacity + 3, Projection()) == OK);
			CHECK(device.draw_triangles(right, 3, Projection()) == OK);
			CHECK(device.end_frame(false) == OK);
			std::vector<uint32_t> pixels(size_t(device.get_width()) * device.get_height());
			CHECK(device.read_frame(pixels.data(), pixels.size()) == OK);
			CHECK(pixels[48 * device.get_width() + device.get_width() / 5] == 0xffff0000);
			CHECK(pixels[48 * device.get_width() + device.get_width() * 7 / 10] == 0xff00ff00);
			if (!immediate) {
				streamed = streamed || device._vertex_buffer != nullptr;
				reference = pixels;
			} else {
				CHECK(pixels == reference);
			}
		}
	}
	CHECK(device.resize(64, 64) == OK && device.prepare_resources() == OK);
	CHECK(TestFixedFunctionVertexSubmission::benchmark(device, "d3d7_stream"));
	device._vertex_buffer_unavailable = true;
	CHECK(TestFixedFunctionVertexSubmission::benchmark(device, "d3d7_immediate"));
	CHECK(device.finalize() == OK && !device._vertex_buffer);
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
	printf("PASS: D3D7 vertex stream (native=%d), wrap, mixed immediate draws, pixel-equivalent fallback, reset and reinitialization.\n", int(streamed));
	return true;
}

#undef CHECK
