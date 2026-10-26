#ifndef TEST_FIXED_FUNCTION_RESTRICTED_ROOT_H
#define TEST_FIXED_FUNCTION_RESTRICTED_ROOT_H

#include "drivers/fixed_function/rasterizer_fixed_function.h"

#include <vector>

// Dedicated-process entry: never call while another VisualServer/storage is alive.
class TestFixedFunctionRestrictedRoot {
	class FFReleaseProbe : public Reference {
		bool *_storage_alive;

	public:
		FFReleaseProbe(bool *p_storage_alive);
		~FFReleaseProbe();
	};
	class FFRoot : public RasterizerFixedFunction {
		static FixedFunctionGraphicsDevice *_device;
		static void *_window;
		static Rasterizer *_create();
		FFRoot();

	public:
		static bool install(FixedFunctionGraphicsDevice &p_device, void *p_window);
		static void uninstall();
		FixedFunctionRenderContext &get_context() { return get_render_context(); }
	};
	class FFExpectedErrors {
		ErrorHandlerList _handler;
		int _count;
		bool _printing;
		static void _error(void *p_userdata, const char *p_function, const char *p_file, int p_line, const char *p_error, const char *p_message, ErrorHandlerType p_type);
		FFExpectedErrors(const FFExpectedErrors &);
		FFExpectedErrors &operator=(const FFExpectedErrors &);

	public:
		int get_count() const { return _count; }
		FFExpectedErrors();
		~FFExpectedErrors();
	};
	static bool _test_unavailable(RasterizerStorageFixedFunction &p_storage, RID p_texture);
	static bool _test_meshes(VisualServer &p_server, FixedFunctionRenderContext &p_context);
	static bool _test_unlit_scene(VisualServer &p_server, FFRoot &p_root, FixedFunctionGraphicsDevice &p_device);
	static bool _test_teardown(FixedFunctionGraphicsDevice &p_device, void *p_window);
	static bool _test_gui(VisualServer &p_server, FFRoot &p_root, FixedFunctionGraphicsDevice &p_device, RID p_canvas, std::vector<uint32_t> &r_pixels);
	static bool _test_sub_viewports(VisualServer &p_server, FFRoot &p_root, FixedFunctionGraphicsDevice &p_device, RID p_parent_viewport, RID p_parent_canvas, RID p_scenario, std::vector<uint32_t> &r_pixels);
	static bool _draw(VisualServer &p_server, FFRoot &p_root, FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels);
	static bool _run(VisualServer &p_server, FFRoot &p_root, FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_RESTRICTED_ROOT_H
