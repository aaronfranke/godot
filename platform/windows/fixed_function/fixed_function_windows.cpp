#include "fixed_function_windows.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef D3D3_SOFTWARE_ENABLED
#include "drivers/d3d3_software/graphics_device_d3d3_software_factory.h"
#endif
#ifdef D3D9_ENABLED
#include "drivers/d3d9/graphics_device_d3d9_factory.h"
#endif
#ifdef D3D7_ENABLED
#include "drivers/d3d7/graphics_device_d3d7_factory.h"
#endif
#ifdef OPENGL1_ENABLED
#include "drivers/opengl1/graphics_device_opengl1_factory.h"
#endif

const FixedFunctionWindows::FFDriver FixedFunctionWindows::DRIVERS[] = {
#ifdef D3D9_ENABLED
	{ "D3D9", "VIDEO_DRIVER_D3D9", FixedFunctionGraphicsDeviceD3D9Factory::create },
#endif
#ifdef D3D7_ENABLED
	{ "D3D7", "VIDEO_DRIVER_D3D7", FixedFunctionGraphicsDeviceD3D7Factory::create },
#endif
#ifdef OPENGL1_ENABLED
	{ "OpenGL1", "VIDEO_DRIVER_OPENGL1", FixedFunctionGraphicsDeviceOpenGL1Factory::create },
#endif
#ifdef D3D3_SOFTWARE_ENABLED
	{ "D3D3 Software", "VIDEO_DRIVER_D3D3_SOFTWARE", FixedFunctionGraphicsDeviceD3D3SoftwareFactory::create },
#endif
	{ nullptr, nullptr, nullptr },
};

FixedFunctionWindows *FixedFunctionWindows::FFRoot::_binding = nullptr;

int FixedFunctionWindows::get_driver_count() {
	return sizeof(DRIVERS) / sizeof(DRIVERS[0]) - 1;
}

const char *FixedFunctionWindows::get_driver_name(int p_driver) {
	ERR_FAIL_INDEX_V(p_driver, get_driver_count(), "");
	return DRIVERS[p_driver].name;
}

const char *FixedFunctionWindows::get_driver_enum_name(int p_driver) {
	ERR_FAIL_INDEX_V(p_driver, get_driver_count(), nullptr);
	return DRIVERS[p_driver].enum_name;
}

Rasterizer *FixedFunctionWindows::FFRoot::_create() {
	ERR_FAIL_NULL_V(_binding, nullptr);
	return memnew(FFRoot(*_binding));
}

Error FixedFunctionWindows::FFRoot::install(FixedFunctionWindows &p_owner) {
	if (_binding || _create_func || VisualServer::get_singleton() || RasterizerStorage::base_singleton) {
		return ERR_ALREADY_IN_USE;
	}
	_binding = &p_owner;
	_create_func = _create;
	return OK;
}

void FixedFunctionWindows::FFRoot::uninstall(FixedFunctionWindows &p_owner) {
	if (_binding == &p_owner) {
		if (_create_func == _create) {
			_create_func = nullptr;
		}
		_binding = nullptr;
	}
}

FixedFunctionWindows::FFRoot::FFRoot(FixedFunctionWindows &p_owner) :
		RasterizerFixedFunction(*p_owner._device, p_owner._window, p_owner._size.width, p_owner._size.height, p_owner._use_vsync),
		_owner(p_owner) {
	_owner._rasterizer = this;
}

void FixedFunctionWindows::FFRoot::initialize() {
	RasterizerFixedFunction::initialize();
	// Window callbacks can queue a newer size/vsync while the device initializes.
	if (get_last_error() == OK && is_initialized() && resize(_owner._size.width, _owner._size.height) == OK) {
		set_use_vsync(_owner._use_vsync);
	}
}

void FixedFunctionWindows::FFRoot::_wait_for_frame(uint32_t p_remaining_usec) {
	// Godot's Windows sleep rounds sub-millisecond delays up to 1 ms. Yield and recheck the deadline.
	if (p_remaining_usec < 1000) {
		Sleep(0);
		return;
	}
	RasterizerFixedFunction::_wait_for_frame(p_remaining_usec);
}

FixedFunctionWindows::FFRoot::~FFRoot() {
	_owner._rasterizer = nullptr;
}

Error FixedFunctionWindows::install(int p_driver, void *p_window, const Size2i &p_size, bool p_vsync) {
	ERR_FAIL_INDEX_V(p_driver, get_driver_count(), ERR_INVALID_PARAMETER);
	if (_device) {
		return ERR_ALREADY_IN_USE;
	}
	if (!p_window || p_size.width <= 0 || p_size.height <= 0) {
		return ERR_INVALID_PARAMETER;
	}
	_size = p_size;
	_window = p_window;
	_use_vsync = p_vsync;
	_device = DRIVERS[p_driver].create();
	if (!_device) {
		return ERR_CANT_CREATE;
	}
	const Error error = FFRoot::install(*this);
	if (error != OK) {
		memdelete(_device);
		_device = nullptr;
	}
	return error;
}

Error FixedFunctionWindows::resize(const Size2i &p_size) {
	if (p_size.width < 0 || p_size.height < 0) {
		return ERR_INVALID_PARAMETER;
	}
	const Error error = _rasterizer && _rasterizer->is_initialized() ? _rasterizer->resize(p_size.width, p_size.height) : OK;
	if (error == OK) {
		_size = p_size;
	}
	return error;
}

Error FixedFunctionWindows::set_use_vsync(bool p_enabled) {
	const Error error = _rasterizer && _rasterizer->is_initialized() ? _rasterizer->set_use_vsync(p_enabled) : OK;
	if (error == OK) {
		_use_vsync = p_enabled;
	}
	return error;
}

Error FixedFunctionWindows::get_initialization_error() const {
	return _rasterizer ? _rasterizer->get_last_error() : ERR_UNCONFIGURED;
}

String FixedFunctionWindows::get_adapter_name() const {
	return _device ? _device->get_adapter_name() : String();
}

Size2i FixedFunctionWindows::get_max_window_size(bool p_include_decorations) const {
	const Size2i size = _device ? _device->get_max_window_size() : Size2i();
	if (!p_include_decorations || size == Size2i()) {
		return size;
	}
	HWND window = static_cast<HWND>(_window);
	// Prefer the effective frame: a runtime/subclass can alter the non-client area.
	// Minimize/restore can report an icon-sized outer rectangle and an empty client
	// even after IsIconic clears. Such rectangles cannot measure the window frame.
	RECT outer, client;
	if (!IsIconic(window) && GetWindowRect(window, &outer) && GetClientRect(window, &client) && client.right > 0 && client.bottom > 0) {
		return size + Size2i(MAX(0L, outer.right - outer.left - client.right), MAX(0L, outer.bottom - outer.top - client.bottom));
	}
	RECT rect = { 0, 0, size.x, size.y };
	if (AdjustWindowRectEx(&rect, GetWindowLongPtr(window, GWL_STYLE), GetMenu(window) != nullptr, GetWindowLongPtr(window, GWL_EXSTYLE))) {
		return Size2i(rect.right - rect.left, rect.bottom - rect.top);
	}
	return size;
}

Size2i FixedFunctionWindows::get_window_size(const Size2i &p_window_size) const {
	return _device ? _device->get_window_size(p_window_size) : p_window_size;
}

FixedFunctionWindows::FixedFunctionWindows() :
		_device(nullptr), _rasterizer(nullptr), _window(nullptr), _use_vsync(false) {}

FixedFunctionWindows::~FixedFunctionWindows() {
	// The VisualServer owns the root and must be destroyed before this binding.
	ERR_FAIL_COND(_rasterizer);
	FFRoot::uninstall(*this);
	if (_device) {
		memdelete(_device);
	}
}
