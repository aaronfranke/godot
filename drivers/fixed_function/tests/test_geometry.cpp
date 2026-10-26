#include "test_geometry.h"

#include "drivers/fixed_function/geometry_data.h"

#include "core/os/os.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <limits>

#define FF_GEOMETRY_CHECK(m_condition)                                   \
	do {                                                                 \
		if (!(m_condition)) {                                            \
			printf("Geometry FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                \
		}                                                                \
	} while (false)

void TestFixedFunctionGeometry::_triangles(FixedFunctionVertex *r_vertices) {
	const FixedFunctionVertex vertices[] = {
		{ -0.9f, -0.8f, 0, 0xffff0000, 0, 1 }, { -0.9f, 0.8f, 0, 0xffff0000, 0, 0 }, { -0.1f, -0.8f, 0, 0xffff0000, 1, 1 }, { 0.1f, -0.8f, 0, 0xff00ff00, 0, 1 }, { 0.1f, 0.8f, 0, 0xff00ff00, 0, 0 }, { 0.9f, -0.8f, 0, 0xff00ff00, 1, 1 }
	};
	memcpy(r_vertices, vertices, sizeof(vertices));
}

bool TestFixedFunctionGeometry::partition() {
	PoolVector<FixedFunctionVertex> vertices;
	PoolVector<uint32_t> indices;
	FF_GEOMETRY_CHECK(vertices.resize(65538) == OK && indices.resize(65538) == OK);
	{
		PoolVector<FixedFunctionVertex>::Write vertex = vertices.write();
		PoolVector<uint32_t>::Write index = indices.write();
		for (int i = 0; i < vertices.size(); i++) {
			const FixedFunctionVertex value = { float(i), 0, 0, uint32_t(i), float(i), -float(i) };
			vertex[i] = value;
			index[i] = i;
		}
	}
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	FF_GEOMETRY_CHECK(FixedFunctionGeometryData::partition(vertices, indices, 65535, UINT32_MAX, batches) == OK);
	FF_GEOMETRY_CHECK(batches.size() == 2 && batches[0].vertices.size() == 65535 && batches[1].vertices.size() == 3);
	uint32_t source = 0;
	for (int batch = 0; batch < batches.size(); batch++) {
		const PoolVector<FixedFunctionVertex>::Read vertex = batches[batch].vertices.read();
		const PoolVector<uint16_t>::Read index = batches[batch].indices.read();
		for (int i = 0; i < batches[batch].indices.size(); i++) {
			FF_GEOMETRY_CHECK(vertex[index[i]].color == source && vertex[index[i]].x == float(source) && vertex[index[i]].v == -float(source));
			source++;
		}
	}
	FF_GEOMETRY_CHECK(source == 65538);
	// Reuse, repeated corners and a primitive limit independent of vertex count.
	FF_GEOMETRY_CHECK(indices.resize(9) == OK);
	{
		PoolVector<uint32_t>::Write index = indices.write();
		const uint32_t order[] = { 65537, 1, 65537, 1, 2, 65537, 2, 65537, 2 };
		memcpy(index.ptr(), order, sizeof(order));
	}
	FF_GEOMETRY_CHECK(FixedFunctionGeometryData::partition(vertices, indices, 3, 1, batches) == OK && batches.size() == 3);
	FF_GEOMETRY_CHECK(batches[0].vertices.size() == 2 && batches[1].vertices.size() == 3 && batches[2].vertices.size() == 2);
	PoolVector<uint32_t>::Read input = indices.read();
	for (int batch = 0; batch < batches.size(); batch++) {
		const PoolVector<FixedFunctionVertex>::Read vertex = batches[batch].vertices.read();
		const PoolVector<uint16_t>::Read index = batches[batch].indices.read();
		for (int i = 0; i < 3; i++) {
			FF_GEOMETRY_CHECK(vertex[index[i]].color == input[batch * 3 + i]);
		}
	}
	input.release();
	const Vector<FixedFunctionGeometryData::FFGeometryBatch> previous = batches;
	FF_GEOMETRY_CHECK(FixedFunctionGeometryData::partition(vertices, indices, 2, 1, batches) == ERR_INVALID_PARAMETER && batches.size() == previous.size());
	{
		PoolVector<uint32_t>::Write write = indices.write();
		write[8] = 65538;
	}
	FF_GEOMETRY_CHECK(FixedFunctionGeometryData::partition(vertices, indices, 3, 1, batches) == ERR_INVALID_DATA && batches.size() == previous.size());
	FF_GEOMETRY_CHECK(vertices.resize(6) == OK);
	FF_GEOMETRY_CHECK(FixedFunctionGeometryData::partition(vertices, PoolVector<uint32_t>(), 3, 2, batches) == OK && batches.size() == 2);
	return true;
}

bool TestFixedFunctionGeometry::create(FixedFunctionGraphicsDevice &p_device, RID &r_geometry, bool p_dense) {
	PoolVector<FixedFunctionVertex> vertices;
	PoolVector<uint32_t> indices;
	FF_GEOMETRY_CHECK(vertices.resize(65538) == OK && indices.resize(p_dense ? 65538 : 6) == OK);
	{
		PoolVector<FixedFunctionVertex>::Write vertex = vertices.write();
		const FixedFunctionVertex outside = { 2, 2, 0, 0xffffffff, 0, 0 };
		for (int i = 0; i < vertices.size(); i++) {
			vertex[i] = outside;
		}
		_triangles(vertex.ptr() + 65532);
		PoolVector<uint32_t>::Write index = indices.write();
		for (int i = 0; i < indices.size(); i++) {
			index[i] = p_dense ? i : 65532 + i;
		}
	}
	FF_GEOMETRY_CHECK(p_device.geometry_create(vertices, indices, r_geometry) == OK && r_geometry.is_valid());
	RID retained = r_geometry;
	FF_GEOMETRY_CHECK(p_device.geometry_create(PoolVector<FixedFunctionVertex>(), indices, retained) == ERR_INVALID_PARAMETER && retained == r_geometry);
	{
		PoolVector<uint32_t>::Write index = indices.write();
		index[5] = 65538;
	}
	FF_GEOMETRY_CHECK(p_device.geometry_create(vertices, indices, retained) == ERR_INVALID_DATA && retained == r_geometry);
	{
		PoolVector<FixedFunctionVertex>::Write vertex = vertices.write();
		vertex[0].x = std::numeric_limits<float>::quiet_NaN();
		vertex[65532].color = 0xff0000ff;
		vertex[65537].x = -2;
	}
	FF_GEOMETRY_CHECK(p_device.geometry_create(vertices, PoolVector<uint32_t>(), retained) == ERR_INVALID_DATA && retained == r_geometry);
	// All caller arrays now differ from the accepted resource, then go out of scope.
	FF_GEOMETRY_CHECK(p_device.draw_geometry(r_geometry, CameraMatrix()) == ERR_UNCONFIGURED);
	return true;
}

bool TestFixedFunctionGeometry::create_nonindexed(FixedFunctionGraphicsDevice &p_device, RID &r_geometry) {
	PoolVector<FixedFunctionVertex> vertices;
	FF_GEOMETRY_CHECK(vertices.resize(6) == OK);
	{
		PoolVector<FixedFunctionVertex>::Write write = vertices.write();
		_triangles(write.ptr());
	}
	FF_GEOMETRY_CHECK(p_device.geometry_create(vertices, PoolVector<uint32_t>(), r_geometry) == OK);
	return true;
}

bool TestFixedFunctionGeometry::draw(FixedFunctionGraphicsDevice &p_device, RID p_geometry, bool p_reference, std::vector<uint32_t> &r_pixels, RID p_texture) {
	FF_GEOMETRY_CHECK(p_device.begin_frame(0xff000000, RID(), true, true) == OK);
	FF_GEOMETRY_CHECK(p_device.geometry_free(p_geometry) == ERR_BUSY);
	RID retained = p_geometry;
	FF_GEOMETRY_CHECK(p_device.geometry_create(PoolVector<FixedFunctionVertex>(), PoolVector<uint32_t>(), retained) == ERR_BUSY && retained == p_geometry);
	CameraMatrix projection;
	projection.matrix[3][0] = real_t(0.025);
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.depth_test_enabled = state.depth_write_enabled = true;
	if (p_texture.is_valid()) {
		state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		state.alpha_cutoff_enabled = p_device.get_capabilities().alpha_cutoff;
		state.alpha_cutoff = 0.5f;
	}
	FixedFunctionVertex vertices[6];
	_triangles(vertices);
	if (p_device.get_max_per_pixel_lights()) {
		// Leave a shader pipeline selected, without changing the expected pixels.
		FixedFunctionLightingVertex outside[3] = {};
		for (int i = 0; i < 3; i++) {
			outside[i].x = outside[i].y = 2;
			outside[i].nz = outside[i].a = 1;
		}
		FixedFunctionGraphicsDevice::FFPixelLighting lighting;
		lighting.ambient = Color(1, 1, 1);
		lighting.light_count = 0;
		FF_GEOMETRY_CHECK(p_device.draw_lit_triangles(outside, 3, projection, lighting, RID(), state) == OK);
	}
	if (p_reference) {
		FF_GEOMETRY_CHECK(p_device.draw_triangles(vertices, 6, projection, p_texture, state) == OK);
	} else {
		CameraMatrix invalid;
		invalid.matrix[0][0] = std::numeric_limits<real_t>::quiet_NaN();
		FF_GEOMETRY_CHECK(p_device.draw_geometry(p_geometry, invalid, p_texture, state) == ERR_UNAVAILABLE);
		FF_GEOMETRY_CHECK(p_device.draw_geometry(p_geometry, projection, p_texture, state) == OK);
	}
	// Mix the transient path and another clipped retained draw. D3D9 must rebind
	// both streams/indices and restore the normal pipeline after shader draws.
	const FixedFunctionVertex outside[] = { { 2, 2, 0, 0xffffffff, 0, 0 }, { 2, 2, 0, 0xffffffff, 0, 0 }, { 2, 2, 0, 0xffffffff, 0, 0 } };
	FF_GEOMETRY_CHECK(p_device.draw_triangles(outside, 3, CameraMatrix()) == OK);
	state.clip_enabled = true;
	state.clip_rect = Rect2i(p_device.get_width() / 2, 0, p_device.get_width() / 2, p_device.get_height());
	if (p_reference) {
		FF_GEOMETRY_CHECK(p_device.draw_triangles(vertices, 6, projection, p_texture, state) == OK);
	} else {
		FF_GEOMETRY_CHECK(p_device.draw_geometry(p_geometry, projection, p_texture, state) == OK);
	}
	FF_GEOMETRY_CHECK(p_device.end_frame(false) == OK);
	r_pixels.resize(size_t(p_device.get_width()) * p_device.get_height());
	FF_GEOMETRY_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	if (!p_texture.is_valid()) {
		FF_GEOMETRY_CHECK(r_pixels[48 * p_device.get_width() + p_device.get_width() / 5] == 0xffff0000);
		FF_GEOMETRY_CHECK(r_pixels[48 * p_device.get_width() + p_device.get_width() * 7 / 10] == 0xff00ff00);
	}
	return true;
}

bool TestFixedFunctionGeometry::benchmark(FixedFunctionGraphicsDevice &p_device, const char *p_path) {
	if (!OS::get_singleton()->get_cmdline_args().find("--geometry-benchmark")) {
		return true;
	}
	FF_GEOMETRY_CHECK(p_device.resize(64, 64) == OK && p_device.prepare_resources() == OK);
	PoolVector<FixedFunctionVertex> vertices;
	PoolVector<uint32_t> indices;
	FF_GEOMETRY_CHECK(vertices.resize(17 * 9) == OK && indices.resize(256 * 3) == OK);
	std::vector<FixedFunctionVertex> expanded(256 * 3);
	{
		PoolVector<FixedFunctionVertex>::Write vertex = vertices.write();
		for (int y = 0; y < 9; y++) {
			for (int x = 0; x < 17; x++) {
				const FixedFunctionVertex value = { x / 8.0f - 1.0f, y / 4.0f - 1.0f, 0, 0xffffffff, 0, 0 };
				vertex[y * 17 + x] = value;
			}
		}
		PoolVector<uint32_t>::Write index = indices.write();
		int cursor = 0;
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 16; x++) {
				const uint32_t corners[] = { uint32_t(y * 17 + x), uint32_t((y + 1) * 17 + x), uint32_t(y * 17 + x + 1), uint32_t(y * 17 + x + 1), uint32_t((y + 1) * 17 + x), uint32_t((y + 1) * 17 + x + 1) };
				for (int corner = 0; corner < 6; corner++) {
					index[cursor] = corners[corner];
					expanded[cursor++] = vertex[corners[corner]];
				}
			}
		}
	}
	RID geometry;
	FF_GEOMETRY_CHECK(p_device.geometry_create(vertices, indices, geometry) == OK);
	std::vector<uint32_t> pixels(64 * 64), reference;
	for (int profile = 0; profile < 2; profile++) {
		std::vector<uint64_t> submission, completion;
		for (int sample = 0; sample < 10; sample++) {
			const uint64_t start = OS::get_singleton()->get_ticks_usec();
			FF_GEOMETRY_CHECK(p_device.begin_frame(0xff000000) == OK);
			for (int draw = 0; draw < 16; draw++) {
				const Error error = profile ? p_device.draw_triangles(expanded.data(), 768, CameraMatrix()) : p_device.draw_geometry(geometry, CameraMatrix());
				FF_GEOMETRY_CHECK(error == OK);
			}
			FF_GEOMETRY_CHECK(p_device.end_frame(false) == OK);
			const uint64_t submitted = OS::get_singleton()->get_ticks_usec() - start;
			// Read every sample to bound queued work; these are wall times, not GPU timestamps.
			FF_GEOMETRY_CHECK(p_device.read_frame(pixels.data(), pixels.size()) == OK);
			const uint64_t completed = OS::get_singleton()->get_ticks_usec() - start;
			if (!profile) {
				reference = pixels;
			} else {
				FF_GEOMETRY_CHECK(pixels == reference);
			}
			if (sample) {
				submission.push_back(submitted);
				completion.push_back(completed);
			}
		}
		std::sort(submission.begin(), submission.end());
		std::sort(completion.begin(), completion.end());
		printf("PERF3D_GEOMETRY path=%s indexed=%d triangles=4096 draws=16 unique_per_mesh=153 samples=9 submit_median_us=%llu complete_median_us=%llu complete_max_us=%llu\n", p_path, int(!profile), static_cast<unsigned long long>(submission[4]), static_cast<unsigned long long>(completion[4]), static_cast<unsigned long long>(completion[8]));
	}
	FF_GEOMETRY_CHECK(p_device.geometry_free(geometry) == OK);
	return true;
}

#undef FF_GEOMETRY_CHECK
