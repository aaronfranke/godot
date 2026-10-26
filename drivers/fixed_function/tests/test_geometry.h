#ifndef TEST_FIXED_FUNCTION_GEOMETRY_H
#define TEST_FIXED_FUNCTION_GEOMETRY_H

#include "drivers/fixed_function/graphics_device.h"

#include <vector>

class TestFixedFunctionGeometry {
	static void _triangles(FixedFunctionVertex *r_vertices);

public:
	static bool partition();
	static bool create(FixedFunctionGraphicsDevice &p_device, RID &r_geometry, bool p_dense = false);
	static bool create_nonindexed(FixedFunctionGraphicsDevice &p_device, RID &r_geometry);
	static bool draw(FixedFunctionGraphicsDevice &p_device, RID p_geometry, bool p_reference, std::vector<uint32_t> &r_pixels, RID p_texture = RID());
	static bool benchmark(FixedFunctionGraphicsDevice &p_device, const char *p_path);
};

#endif // TEST_FIXED_FUNCTION_GEOMETRY_H
