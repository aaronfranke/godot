#pragma once

#include "drivers/fixed_function/renderer_utilities_fixed_function.h"

class TestFixedFunctionPresetScope {
	FixedFunctionRenderContext _context;
	FixedFunctionRenderTargetManager _targets;
	RendererTextureStorageFixedFunction _textures;
	RendererMaterialStorageFixedFunction _facade;
	RendererSceneRenderFixedFunction _scene;
	RendererLightStorageFixedFunction _lights;
	RendererUtilitiesFixedFunction _utility_facade;
	RendererMaterialStorage *_materials;
	RendererUtilities *_utilities;

public:
	TestFixedFunctionPresetScope(FixedFunctionGraphicsDevice &p_device, void *p_window);
	~TestFixedFunctionPresetScope();
};
