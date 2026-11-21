#pragma once

class FixedFunctionGraphicsDevice;

// Factory declarations do not expose version-specific Direct3D SDK headers.
class FixedFunctionGraphicsDeviceD3D9Factory {
public:
	static FixedFunctionGraphicsDevice *create();
};
