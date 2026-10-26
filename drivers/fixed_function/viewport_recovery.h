#ifndef FIXED_FUNCTION_VIEWPORT_RECOVERY_H
#define FIXED_FUNCTION_VIEWPORT_RECOVERY_H

#include "texture_storage.h"

#include "servers/visual/visual_server_viewport.h"

// Branch adapter: common storage retains intent without depending on viewport types.
class FixedFunctionViewportRecovery {
public:
	static Error update(FixedFunctionTextureStorage &p_storage, VisualServerViewport &p_viewports);
};

#endif // FIXED_FUNCTION_VIEWPORT_RECOVERY_H
