#ifndef TEST_FIXED_FUNCTION_STANDALONE_DEVICES_H
#define TEST_FIXED_FUNCTION_STANDALONE_DEVICES_H

#include "drivers/fixed_function/graphics_device.h"
class TestFixedFunctionStandaloneDevices {
public:
	typedef FixedFunctionGraphicsDevice *(*CreateDevice)();
	class FFDeviceFactory {
		const FFDeviceFactory *_next;
		const char *_name;
		CreateDevice _create;

		FFDeviceFactory(const FFDeviceFactory &);
		FFDeviceFactory &operator=(const FFDeviceFactory &);

	public:
		const FFDeviceFactory *get_next() const { return _next; }
		const char *get_name() const { return _name; }
		FixedFunctionGraphicsDevice *create() const { return _create(); }
		FFDeviceFactory(const char *p_name, CreateDevice p_create);
	};

	static const FFDeviceFactory *get_factories() { return _factories; }
	static const FFDeviceFactory *find_factory(const char *p_name);

private:
	static const FFDeviceFactory *_factories;
};

#endif // TEST_FIXED_FUNCTION_STANDALONE_DEVICES_H
