#ifndef FIXED_FUNCTION_WINDOWS_H
#define FIXED_FUNCTION_WINDOWS_H

#include "drivers/fixed_function/rasterizer_fixed_function.h"

// Platform-owned device/factory binding. The shared renderer borrows its device.
class FixedFunctionWindows {
	class FFRoot : public RasterizerFixedFunction {
		FixedFunctionWindows &_owner;
		static FixedFunctionWindows *_binding;
		static Rasterizer *_create();
		void _wait_for_frame(uint32_t p_remaining_usec) override;
		FFRoot(FixedFunctionWindows &p_owner);

	public:
		static Error install(FixedFunctionWindows &p_owner);
		static void uninstall(FixedFunctionWindows &p_owner);
		void initialize() override;
		~FFRoot() override;
	};
	struct FFDriver {
		const char *name;
		const char *enum_name;
		FixedFunctionGraphicsDevice *(*create)();
	};
	static const FFDriver DRIVERS[];

	Size2i _size;
	FixedFunctionGraphicsDevice *_device;
	FFRoot *_rasterizer;
	void *_window;
	bool _use_vsync;

	FixedFunctionWindows(const FixedFunctionWindows &);
	FixedFunctionWindows &operator=(const FixedFunctionWindows &);

public:
	static int get_driver_count();
	static const char *get_driver_name(int p_driver);
	static const char *get_driver_enum_name(int p_driver);
	Error install(int p_driver, void *p_window, const Size2i &p_size, bool p_vsync);
	Error resize(const Size2i &p_size);
	Error set_use_vsync(bool p_enabled);
	Error get_initialization_error() const;
	String get_adapter_name() const;
	Size2i get_max_window_size(bool p_include_decorations = false) const;
	Size2i get_window_size(const Size2i &p_window_size) const;
	FixedFunctionWindows();
	~FixedFunctionWindows();
};

#endif // FIXED_FUNCTION_WINDOWS_H
