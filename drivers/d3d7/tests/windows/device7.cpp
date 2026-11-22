#include "drivers/d3d7/graphics_device_d3d7.h"
#include "drivers/fixed_function/tests/windows/devices.h"

class TestFixedFunctionStandaloneD3D7 {
	static FixedFunctionGraphicsDevice *_create() { return memnew(FixedFunctionGraphicsDeviceD3D7); }
	static const TestFixedFunctionStandaloneDevices::FFDeviceFactory FACTORY;
};

const TestFixedFunctionStandaloneDevices::FFDeviceFactory TestFixedFunctionStandaloneD3D7::FACTORY("d3d7", _create);
