#ifndef TEST_FIXED_FUNCTION_SCENE_BOOTSTRAP_H
#define TEST_FIXED_FUNCTION_SCENE_BOOTSTRAP_H

#include "drivers/fixed_function/rasterizer_scene_fixed_function.h"

#include <vector>

class TestFixedFunctionSceneBootstrap {
	struct FFInstance : RasterizerScene::InstanceBase {
		enum FFAction {
			NONE,
			CHANGE,
			REMOVE_OTHER,
			REBIND_OTHER,
			DELETE_OTHER,
			FREE_MATERIAL,
			CLEAR
		};
		FixedFunctionMaterialStorage &storage;
		FFInstance *other;
		FFInstance **survivor;
		Error nested;
		int changes;
		int removals;
		FFAction action;
		bool valid_flags;
		FFInstance(FixedFunctionMaterialStorage &p_storage, RID p_material);
		~FFInstance();
		void detach();
		void bind(RID p_material);
		void base_removed() override;
		void base_changed(bool p_aabb, bool p_materials) override;
	};
	static bool _test_features(FixedFunctionMaterialStorage &p_storage);
	static bool _test_dependencies(FixedFunctionMaterialStorage &p_storage);
	static bool _test_owners(FixedFunctionSceneStorage &p_storage);
	static bool _test_sky(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RasterizerSceneFixedFunction &p_scene, RID p_environment);
	static bool _test_panorama_averages(FixedFunctionTextureStorage &p_textures);
	static bool _test_sky_hemispheres(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RasterizerSceneFixedFunction &p_scene, RID p_environment);
	static bool _render(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RasterizerSceneFixedFunction &p_scene, RID p_env, RID p_shadow, RID p_reflection, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_SCENE_BOOTSTRAP_H
