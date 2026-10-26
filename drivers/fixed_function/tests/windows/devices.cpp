#include "devices.h"

#include <string.h>

// Zero-initialized before backend registration; no Godot core startup is needed.
const TestFixedFunctionStandaloneDevices::FFDeviceFactory *TestFixedFunctionStandaloneDevices::_factories = nullptr;

TestFixedFunctionStandaloneDevices::FFDeviceFactory::FFDeviceFactory(const char *p_name, CreateDevice p_create) :
		_next(_factories), _name(p_name), _create(p_create) {
	_factories = this;
}

const TestFixedFunctionStandaloneDevices::FFDeviceFactory *TestFixedFunctionStandaloneDevices::find_factory(const char *p_name) {
	for (const FFDeviceFactory *factory = _factories; factory; factory = factory->get_next()) {
		if (strcmp(p_name, factory->get_name()) == 0) {
			return factory;
		}
	}
	return nullptr;
}
