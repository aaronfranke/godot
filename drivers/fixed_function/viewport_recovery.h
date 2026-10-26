#pragma once

#include "drivers/fixed_function/texture_storage.h"
#include "servers/rendering/renderer_viewport.h"

// Branch adapter: common storage retains intent without depending on viewport types.
class FixedFunctionViewportRecovery {
public:
	static Error update(FixedFunctionTextureStorage &p_storage, RendererViewport &p_viewports);
};
