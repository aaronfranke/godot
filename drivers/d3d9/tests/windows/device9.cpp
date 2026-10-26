#include "drivers/d3d9/graphics_device_d3d9.h"
#include "drivers/fixed_function/tests/windows/devices.h"

class TestFixedFunctionStandaloneD3D9 {
	static FixedFunctionGraphicsDevice *_create() { return memnew(FixedFunctionGraphicsDeviceD3D9); }
	static const TestFixedFunctionStandaloneDevices::FFDeviceFactory FACTORY;
};

const TestFixedFunctionStandaloneDevices::FFDeviceFactory TestFixedFunctionStandaloneD3D9::FACTORY("d3d9", _create);
