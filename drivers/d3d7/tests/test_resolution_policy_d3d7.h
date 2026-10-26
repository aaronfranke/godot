#ifndef TEST_D3D7_RESOLUTION_POLICY_H
#define TEST_D3D7_RESOLUTION_POLICY_H

#include "drivers/d3d7/graphics_device_d3d7.h"

#include <vector>

class TestD3D7ResolutionPolicy {
	static std::vector<uint8_t> _fixture(unsigned p_shift = 0);
	static void _branch(std::vector<uint8_t> &r_image, unsigned p_offset, unsigned p_destination);

public:
	static bool test();
	static bool test_device(FixedFunctionGraphicsDeviceD3D7 &p_device, HWND p_window);
	static bool test_mode(HWND p_window, D3D7ResolutionPolicy::D3D7OversizeMode p_mode, bool p_block_override, bool p_force_rgb);
};

#endif // TEST_D3D7_RESOLUTION_POLICY_H
