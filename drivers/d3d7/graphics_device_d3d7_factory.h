#ifndef FIXED_FUNCTION_GRAPHICS_DEVICE_D3D7_FACTORY_H
#define FIXED_FUNCTION_GRAPHICS_DEVICE_D3D7_FACTORY_H

class FixedFunctionGraphicsDevice;

// Factory declarations do not expose version-specific Direct3D SDK headers.
class FixedFunctionGraphicsDeviceD3D7Factory {
public:
	static FixedFunctionGraphicsDevice *create();
};

#endif // FIXED_FUNCTION_GRAPHICS_DEVICE_D3D7_FACTORY_H
