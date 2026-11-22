#include "test_graphics_device_d3d9.h"

#include <stdio.h>

#define FF_NATIVE_LIT_CHECK(m_condition)                                          \
	do {                                                                          \
		if (!(m_condition)) {                                                     \
			printf("D3D9 lit geometry FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                         \
		}                                                                         \
	} while (false)

bool TestFixedFunctionGraphicsDeviceD3D9::_test_lit_geometry(HWND p_window) {
	FixedFunctionGraphicsDeviceD3D9 device;
	FF_NATIVE_LIT_CHECK(device.initialize(p_window, 64, 64, false) == OK);
	if (!device.supports_lit_geometry()) {
		return device.finalize() == OK;
	}
	PoolVector<FixedFunctionLightingVertex> vertices;
	PoolVector<uint32_t> indices;
	FF_NATIVE_LIT_CHECK(vertices.resize(65538) == OK && indices.resize(6) == OK);
	FixedFunctionLightingVertex reference_vertices[6];
	{
		PoolVector<FixedFunctionLightingVertex>::Write output = vertices.write();
		const FixedFunctionLightingVertex outside = { 2, 2, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1 };
		for (int i = 0; i < vertices.size(); i++) {
			output[i] = outside;
		}
		const float x[] = { -0.9f, -0.9f, -0.1f, 0.1f, 0.1f, 0.9f };
		for (int i = 0; i < 6; i++) {
			FixedFunctionLightingVertex v = { x[i], i % 3 == 1 ? 0.8f : -0.8f, 0, 0, 0, 1, 0, 0, i < 3 ? 1.0f : 0.0f, i < 3 ? 0.0f : 1.0f, 0, 1 };
			v.u = i == 2 ? 1 : 0;
			output[65532 + i] = reference_vertices[i] = v;
			indices.write()[i] = 65532 + i;
		}
	}
	const uint32_t limit = device._capabilities.max_primitives_per_draw;
	device._capabilities.max_primitives_per_draw = 1;
	RID geometry;
	FF_NATIVE_LIT_CHECK(device.geometry_create_lit(vertices, indices, geometry) == OK);
	device._capabilities.max_primitives_per_draw = limit;
	FixedFunctionGraphicsDeviceD3D9::D3D9Geometry *record = device._geometry_owner.getptr(geometry);
	FF_NATIVE_LIT_CHECK(record->batches.size() == 2 && device.get_geometry_memory_usage() == 0);
	RID unchanged = geometry;
	FF_NATIVE_LIT_CHECK(device.geometry_create_lit(PoolVector<FixedFunctionLightingVertex>(), indices, unchanged) == ERR_INVALID_PARAMETER && unchanged == geometry);
	indices.write()[5] = 65538;
	FF_NATIVE_LIT_CHECK(device.geometry_create_lit(vertices, indices, unchanged) == ERR_INVALID_DATA && unchanged == geometry);
	vertices.write()[65532].r = 0; // The accepted snapshot remains red.
	FixedFunctionGraphicsDevice::FFPixelLighting lighting;
	lighting.light_count = 0;
	lighting.ambient = Color(1, 1, 1);
	FixedFunctionGraphicsDevice::FFLitGeometryParameters parameters;
	FF_NATIVE_LIT_CHECK(parameters.color == Color(1, 1, 1, 1));
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	std::vector<uint32_t> reference(64 * 64), pixels(64 * 64);
	FF_NATIVE_LIT_CHECK(device.begin_frame(0xff000000) == OK && device.draw_lit_triangles(reference_vertices, 6, CameraMatrix(), lighting, RID(), state) == OK && device.end_frame(false) == OK && device.read_frame(reference.data(), reference.size()) == OK);
	for (int path = 0; path < 3; path++) {
		if (path) {
			device._release_geometry_batch(record->batches[path == 1 ? 1 : 0]);
			record->batches[path == 1 ? 1 : 0]->unavailable = true;
		}
		FF_NATIVE_LIT_CHECK(device.begin_frame(0xff000000) == OK);
		FF_NATIVE_LIT_CHECK(device.geometry_free(geometry) == ERR_BUSY && device.geometry_create_lit(vertices, indices, unchanged) == ERR_BUSY);
		FF_NATIVE_LIT_CHECK(device.draw_geometry(geometry, CameraMatrix()) == ERR_INVALID_PARAMETER);
		FF_NATIVE_LIT_CHECK(device.draw_lit_geometry(geometry, CameraMatrix(), parameters, lighting, RID(), state) == OK && device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK);
		FF_NATIVE_LIT_CHECK(pixels == reference);
		if (!path) {
			FF_NATIVE_LIT_CHECK(record->batches[0]->vertices && record->batches[1]->vertices && device.get_geometry_memory_usage() == 300);
		}
	}
	FF_NATIVE_LIT_CHECK(device.get_geometry_memory_usage() == 0);
	FF_NATIVE_LIT_CHECK(device.resize(80, 64) == OK && device.prepare_resources() == OK && device.resize(64, 64) == OK && device.prepare_resources() == OK);
	FF_NATIVE_LIT_CHECK(!record->batches[0]->unavailable && !record->batches[1]->unavailable);
	FF_NATIVE_LIT_CHECK(device.begin_frame(0xff000000) == OK && device.draw_lit_geometry(geometry, CameraMatrix(), parameters, lighting, RID(), state) == OK && device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK && pixels == reference);

	// Same-UV AO uses the regular lit layout and keeps the authored vertex colors.
	FixedFunctionGraphicsDevice::FFOcclusionMapParameters occlusion;
	occlusion.uv_scale = Vector2(1, 1);
	occlusion.weights = Color(1, 0, 0, 0);
	occlusion.light_affect = 0;
	occlusion.sampler_flags = 0;
	occlusion.uses_secondary_uv = false;
	// Both UV endpoints sample white. Only per-pixel sampling reaches the dark interior texel.
	Ref<Image> image = memnew(Image(4, 1, false, Image::FORMAT_RGBA8));
	image->fill(Color(1, 1, 1));
	image->lock();
	image->set_pixel(1, 0, Color(0.5f, 1, 1));
	image->unlock();
	FF_NATIVE_LIT_CHECK(device.texture_create(image, occlusion.texture) == OK && !device._occlusion_shaders.vertex);
	device._occlusion_shaders.unavailable = true;
	FF_NATIVE_LIT_CHECK(device.begin_frame(0xff000000) == OK);
	FF_NATIVE_LIT_CHECK(device.draw_lit_geometry(geometry, CameraMatrix(), parameters, lighting, RID(), state, &occlusion) == ERR_UNAVAILABLE);
	device._occlusion_shaders.unavailable = false;
	FF_NATIVE_LIT_CHECK(device.draw_lit_geometry(geometry, CameraMatrix(), parameters, lighting, RID(), state, &occlusion) == OK);
	FF_NATIVE_LIT_CHECK(device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK);
	const size_t occluded = 32 * 64 + 12;
	FF_NATIVE_LIT_CHECK(reference[occluded] == 0xffff0000 && (pixels[occluded] == 0xff7f0000 || pixels[occluded] == 0xff800000));
	FF_NATIVE_LIT_CHECK(pixels[32 * 64 + 5] == 0xffff0000 && pixels[32 * 64 + 40] == 0xff00ff00);
	FF_NATIVE_LIT_CHECK(device.texture_free(occlusion.texture) == OK);

	FF_NATIVE_LIT_CHECK(device.geometry_free(geometry) == OK && device.get_geometry_memory_usage() == 0 && device.finalize() == OK);
	printf("PASS: D3D9 lit indexed buffers, large source indices, snapshots, mixed/immediate equivalence, busy ownership, memory and reset.\n");
	return true;
}

#undef FF_NATIVE_LIT_CHECK
