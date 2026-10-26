#include "drivers/d3d9/graphics_device_d3d9.h"
#include "drivers/d3d9/tests/test_graphics_device_d3d9.h"
#include "drivers/fixed_function/tests/test_geometry.h"

#include <stdio.h>

#define CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("D3D9 geometry FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionGraphicsDeviceD3D9::_test_geometry(HWND p_window) {
	CHECK(TestFixedFunctionGeometry::partition());
	FixedFunctionGraphicsDeviceD3D9 device;
	CHECK(device.initialize(p_window, 64, 64, false) == OK);
	CHECK(device.get_geometry_memory_usage() == 0);
	// Force two independent native batches without requiring an oversized draw.
	const uint32_t max_primitives = device._capabilities.max_primitives_per_draw;
	device._capabilities.max_primitives_per_draw = 1;
	RID rid;
	CHECK(TestFixedFunctionGeometry::create(device, rid));
	device._capabilities.max_primitives_per_draw = max_primitives;
	FixedFunctionGraphicsDeviceD3D9::D3D9Geometry *geometry = device._geometry_owner.get_or_null(rid);
	CHECK(geometry->batches.size() == 2 && device.get_geometry_memory_usage() == 0);
	Ref<Image> image;
	image = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	image->set_pixel(0, 0, Color(1, 0, 0, 0));
	image->set_pixel(1, 0, Color(1, 1, 1, 0.5));
	image->set_pixel(0, 1, Color(0.5, 1, 1, 1));
	image->set_pixel(1, 1, Color(1, 0.5, 1, 1));
	RID texture;
	CHECK(device.texture_create(image, texture) == OK);
	bool native = false;
	for (int cycle = 0; cycle < 2; cycle++) {
		if (cycle) {
			CHECK(device.resize(96, 64) == OK && device.prepare_resources() == OK);
			CHECK(device.get_geometry_memory_usage() == 0);
			for (int i = 0; i < geometry->batches.size(); i++) {
				CHECK(!geometry->batches[i]->vertices && !geometry->batches[i]->unavailable);
				CHECK(!geometry->batches[i]->indices);
			}
		}
		std::vector<uint32_t> reference, pixels;
		CHECK(TestFixedFunctionGeometry::draw(device, rid, true, reference));
		CHECK(TestFixedFunctionGeometry::draw(device, rid, false, pixels) && pixels == reference);
		IDirect3DVertexBuffer9 *retained = geometry->batches[0]->vertices;
		native = native || retained != nullptr;
		const uint64_t bytes = device.get_geometry_memory_usage();
		if (geometry->batches[0]->vertices && geometry->batches[1]->vertices) {
			CHECK(bytes == 156);
		}
		CHECK(TestFixedFunctionGeometry::draw(device, rid, false, pixels) && pixels == reference);
		CHECK(geometry->batches[0]->vertices == retained && device.get_geometry_memory_usage() == bytes);
		CHECK(TestFixedFunctionGeometry::draw(device, rid, true, reference, texture));
		CHECK(TestFixedFunctionGeometry::draw(device, rid, false, pixels, texture) && pixels == reference);
		CHECK(TestFixedFunctionGeometry::draw(device, rid, true, reference));
		// Mix native and immediate batches, then reject both optional uploads.
		device._release_geometry_batch(geometry->batches[1]);
		geometry->batches[1]->unavailable = true;
		CHECK(TestFixedFunctionGeometry::draw(device, rid, false, pixels) && pixels == reference);
		device._release_geometry_batch(geometry->batches[0]);
		geometry->batches[0]->unavailable = true;
		CHECK(TestFixedFunctionGeometry::draw(device, rid, false, pixels) && pixels == reference);
		CHECK(device.get_geometry_memory_usage() == 0);
		CHECK(TestFixedFunctionGeometry::draw(device, rid, true, reference, texture));
		CHECK(TestFixedFunctionGeometry::draw(device, rid, false, pixels, texture) && pixels == reference);
	}
	CHECK(device.geometry_free(rid) == OK && device._geometries.is_empty());
	CHECK(device.texture_free(texture) == OK);
	// Exercise the actual local-index boundary on the device, not just the packer.
	CHECK(TestFixedFunctionGeometry::create(device, rid, true));
	std::vector<uint32_t> dense_pixels;
	CHECK(TestFixedFunctionGeometry::draw(device, rid, false, dense_pixels));
	CHECK(device.geometry_free(rid) == OK);
	CHECK(device.geometry_free(RID()) == ERR_INVALID_PARAMETER);
	CHECK(device.resize(0, 0) == OK);
	// Logical creation remains possible while drawing is suspended; no upload/reset.
	CHECK(TestFixedFunctionGeometry::create(device, rid));
	CHECK(device.get_geometry_memory_usage() == 0);
	CHECK(device.resize(64, 64) == OK && device.prepare_resources() == OK);
	std::vector<uint32_t> pixels;
	CHECK(TestFixedFunctionGeometry::draw(device, rid, false, pixels));
	// Finalize owns cleanup too; never query this discarded RID afterward.
	CHECK(device.finalize() == OK && device._geometries.is_empty() && device.get_geometry_memory_usage() == 0);
	CHECK(device.initialize(p_window, 64, 64, false) == OK);
	CHECK(TestFixedFunctionGeometry::create_nonindexed(device, rid));
	CHECK(TestFixedFunctionGeometry::draw(device, rid, false, pixels));
	CHECK(device.geometry_free(rid) == OK && device.get_geometry_memory_usage() == 0);
	CHECK(TestFixedFunctionGeometry::benchmark(device, "d3d9"));
	CHECK(TestFixedFunctionGeometry::test_lods(device));
	CHECK(device.finalize() == OK);
	printf("PASS: D3D9 persistent indexed geometry (native=%d), source snapshots, batching, immediate equivalence, recovery and ownership.\n", int(native));
	return true;
}

#undef CHECK
