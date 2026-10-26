#pragma once

#include "drivers/fixed_function/render_context.h"
#include "drivers/fixed_function/viewport_recovery.h"

#include <vector>

class TestFixedFunctionViewportRecovery {
	struct FFViewportFixture {
		RendererViewport::Viewport *viewport;
		RendererViewport &server;

		FFViewportFixture(RendererViewport &p_server, RID p_target, RSE::ViewportUpdateMode p_mode);
		~FFViewportFixture();
	};
	static bool _render(FixedFunctionRenderContext &p_context, RID p_target, uint32_t p_color);
	static bool _copy_and_check(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, RID p_target, uint32_t p_color, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96,
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};
