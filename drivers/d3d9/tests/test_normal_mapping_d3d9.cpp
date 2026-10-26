#include "drivers/d3d9/tests/test_graphics_device_d3d9.h"

#include <stdio.h>

#define FF_NATIVE_NORMAL_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Normal-map native FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionGraphicsDeviceD3D9::_test_normal_geometry(HWND p_window) {
	FixedFunctionGraphicsDeviceD3D9 device;
	FF_NATIVE_NORMAL_CHECK(device.initialize(p_window, 64, 64, false) == OK);
	if (!device.supports_normal_mapping()) {
		return device.finalize() == OK;
	}
	Vector<FixedFunctionNormalMapVertex> vertices;
	Vector<uint32_t> indices;
	FF_NATIVE_NORMAL_CHECK(vertices.resize(65538) == OK && indices.resize(6) == OK);
	FixedFunctionLightingVertex reference_vertices[6];
	{
		FixedFunctionNormalMapVertex *output = vertices.ptrw();
		const FixedFunctionLightingVertex outside = { 2, 2, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1 };
		for (int i = 0; i < vertices.size(); i++) {
			output[i] = { outside, 1, 0, 0, 1, 0, 0 };
		}
		const float x[] = { -0.9f, -0.9f, -0.1f, 0.1f, 0.1f, 0.9f };
		for (int i = 0; i < 6; i++) {
			FixedFunctionLightingVertex v = { x[i], i % 3 == 1 ? 0.8f : -0.8f, 0, 0, 0, 1, 0, 0, i < 3 ? 1.0f : 0.0f, i < 3 ? 0.0f : 1.0f, 0, 1 };
			reference_vertices[i] = v;
			output[65532 + i] = { v, 1, 0, 0, 1, 0, 0 };
			indices.write[i] = 65532 + i;
		}
	}
	const uint32_t limit = device._capabilities.max_primitives_per_draw;
	device._capabilities.max_primitives_per_draw = 1;
	RID geometry;
	FF_NATIVE_NORMAL_CHECK(!device._normal_map_vertex_shader && !device._normal_map_pixel_shader);
	device._normal_map_unavailable = true;
	FF_NATIVE_NORMAL_CHECK(device.geometry_create_normal_mapped(vertices, indices, geometry) == ERR_UNAVAILABLE && !geometry.is_valid() && device.supports_lit_geometry());
	device._normal_map_unavailable = false;
	FF_NATIVE_NORMAL_CHECK(device.geometry_create_normal_mapped(vertices, indices, geometry) == OK);
	device._capabilities.max_primitives_per_draw = limit;
	FixedFunctionGraphicsDeviceD3D9::D3D9Geometry *record = device._geometry_owner.get_or_null(geometry);
	FF_NATIVE_NORMAL_CHECK(record->batches.size() == 2 && device.get_geometry_memory_usage() == 0);
	RID unchanged = geometry;
	FF_NATIVE_NORMAL_CHECK(device.geometry_create_normal_mapped(Vector<FixedFunctionNormalMapVertex>(), indices, unchanged) == ERR_INVALID_PARAMETER && unchanged == geometry);
	indices.write[5] = 65538;
	FF_NATIVE_NORMAL_CHECK(device.geometry_create_normal_mapped(vertices, indices, unchanged) == ERR_INVALID_DATA && unchanged == geometry);
	vertices.write[65532].lighting.r = 0; // The accepted snapshot remains red.
	FixedFunctionGraphicsDevice::FFPixelLighting lighting;
	lighting.light_count = 0;
	lighting.ambient = Color(1, 1, 1);
	FixedFunctionGraphicsDevice::FFLitGeometryParameters parameters;
	FF_NATIVE_NORMAL_CHECK(parameters.color == Color(1, 1, 1, 1));
	FixedFunctionGraphicsDevice::FFNormalMapParameters normal;
	Ref<Image> image;
	image = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	image->fill(Color(0.5, 0.5, 1));
	FF_NATIVE_NORMAL_CHECK(device.texture_create(image, normal.texture) == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	std::vector<uint32_t> reference(64 * 64), pixels(64 * 64);
	FF_NATIVE_NORMAL_CHECK(device.begin_frame(0xff000000) == OK && device.draw_lit_triangles(reference_vertices, 6, Projection(), lighting, RID(), state) == OK && device.end_frame(false) == OK && device.read_frame(reference.data(), reference.size()) == OK);
	for (int path = 0; path < 3; path++) {
		if (path) {
			device._release_geometry_batch(record->batches[path == 1 ? 1 : 0]);
			record->batches[path == 1 ? 1 : 0]->unavailable = true;
		}
		FF_NATIVE_NORMAL_CHECK(device.begin_frame(0xff000000) == OK);
		FF_NATIVE_NORMAL_CHECK(device.geometry_free(geometry) == ERR_BUSY && device.geometry_create_normal_mapped(vertices, indices, unchanged) == ERR_BUSY);
		FF_NATIVE_NORMAL_CHECK(device.draw_geometry(geometry, Projection()) == ERR_INVALID_PARAMETER && device.draw_lit_geometry(geometry, Projection(), parameters, lighting, RID(), state) == ERR_INVALID_PARAMETER);
		FF_NATIVE_NORMAL_CHECK(device.draw_normal_mapped_geometry(geometry, Projection(), parameters, lighting, RID(), state, normal) == OK && device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK);
		FF_NATIVE_NORMAL_CHECK(pixels == reference);
		if (!path) {
			FF_NATIVE_NORMAL_CHECK(record->batches[0]->vertices && record->batches[1]->vertices && device.get_geometry_memory_usage() == 444);
		}
	}
	FF_NATIVE_NORMAL_CHECK(device.get_geometry_memory_usage() == 0);
	FF_NATIVE_NORMAL_CHECK(device.resize(80, 64) == OK && device.prepare_resources() == OK && device.resize(64, 64) == OK && device.prepare_resources() == OK);
	FF_NATIVE_NORMAL_CHECK(!record->batches[0]->unavailable && !record->batches[1]->unavailable);
	FF_NATIVE_NORMAL_CHECK(device.begin_frame(0xff000000) == OK && device.draw_normal_mapped_geometry(geometry, Projection(), parameters, lighting, RID(), state, normal) == OK && device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK && pixels == reference);
	FixedFunctionGraphicsDevice::FFOcclusionMapParameters occlusion;
	occlusion.uv_scale = Vector2(1, 1);
	occlusion.weights = Color(0, 1, 0, 0);
	occlusion.light_affect = 0;
	occlusion.sampler_flags = 0;
	occlusion.uses_secondary_uv = false;
	image->fill(Color(1, 64.0f / 255, 0));
	FF_NATIVE_NORMAL_CHECK(device.texture_create(image, occlusion.texture) == OK && !device._normal_occlusion_shaders.vertex);
	device._normal_occlusion_shaders.unavailable = true;
	FF_NATIVE_NORMAL_CHECK(device.begin_frame(0xff000000) == OK);
	FF_NATIVE_NORMAL_CHECK(device.draw_normal_mapped_geometry(geometry, Projection(), parameters, lighting, RID(), state, normal, &occlusion) == ERR_UNAVAILABLE);
	device._normal_occlusion_shaders.unavailable = false;
	FF_NATIVE_NORMAL_CHECK(device.draw_normal_mapped_geometry(geometry, Projection(), parameters, lighting, RID(), state, normal, &occlusion) == OK);
	IDirect3DBaseTexture9 *bound = nullptr;
	FF_NATIVE_NORMAL_CHECK(SUCCEEDED(device._device->GetTexture(2, &bound)) && !bound);
	FF_NATIVE_NORMAL_CHECK(device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK);
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_NATIVE_NORMAL_CHECK(pixels[i] == (0xff000000 | (reference[i] & 0x00404040)));
	}
	FF_NATIVE_NORMAL_CHECK(device.texture_free(occlusion.texture) == OK);
	FF_NATIVE_NORMAL_CHECK(device.geometry_free(geometry) == OK && device.texture_free(normal.texture) == OK && device.get_geometry_memory_usage() == 0 && device.finalize() == OK);
	printf("PASS: D3D9 normal-map indexed buffers, large source indices, lazy capability fallback, snapshots, mixed/immediate equivalence, busy ownership, memory and reset.\n");
	return true;
}

#undef FF_NATIVE_NORMAL_CHECK
