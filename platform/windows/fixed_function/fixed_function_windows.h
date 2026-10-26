#pragma once

#include "drivers/fixed_function/renderer_compositor_fixed_function.h"

// Platform-owned device/factory binding. The shared renderer borrows its device.
class FixedFunctionWindows {
	class FFRoot : public RendererCompositorFixedFunction {
		FixedFunctionWindows &_owner;
		static FixedFunctionWindows *_binding;
		static RendererCompositor *_create();
		void _wait_for_frame(uint32_t p_remaining_usec) override;

	public:
		FFRoot(FixedFunctionWindows &p_owner);
		static Error install(FixedFunctionWindows &p_owner);
		static void uninstall(FixedFunctionWindows &p_owner);
		void initialize() override;
		~FFRoot() override;
	};
	struct FFDriver {
		const char *name;
		FixedFunctionGraphicsDevice *(*create)();
	};
	static const FFDriver DRIVERS[];

	Size2i _size;
	FixedFunctionGraphicsDevice *_device;
	FFRoot *_rasterizer;
	void *_window;
	bool _use_vsync;
	bool _transferred;

	FixedFunctionWindows(const FixedFunctionWindows &);
	FixedFunctionWindows &operator=(const FixedFunctionWindows &);

public:
	static int get_driver_count();
	static const char *get_driver_name(int p_driver);
	Error install(int p_driver, void *p_window, const Size2i &p_size, bool p_vsync);
	Error resize(const Size2i &p_size);
	Error set_use_vsync(bool p_enabled);
	Error get_initialization_error() const;
	String get_adapter_name() const;
	Size2i get_max_window_size(bool p_include_decorations = false) const;
	FixedFunctionWindows();
	~FixedFunctionWindows();
};
