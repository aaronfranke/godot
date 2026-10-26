#pragma once

#include "drivers/fixed_function/canvas_item_renderer.h"
#include "drivers/fixed_function/renderer_scene_render_fixed_function.h"

#include <vector>

class TestFixedFunctionStorageIntegration {
	struct FFDependencyState {
		FixedFunctionMaterialStorage *storage;
		RID material;
		int changed;
		int deleted;
		bool free_on_change;
		bool clear_on_change;
		Error error;
		FFDependencyState();
	};
	static void _changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker);
	static void _deleted(const RID &p_rid, DependencyTracker *p_tracker);
	static bool _test_materials(FixedFunctionMaterialStorage &p_storage);
	static bool _test_feature_table(FixedFunctionMaterialStorage &p_storage);
	static bool _test_scene(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, std::vector<uint32_t> &r_pixels);
	static bool _test_unlit_scene(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, RendererSceneRenderFixedFunction &p_scene, const Ref<RenderSceneBuffers> &p_buffers, RID p_target, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 64,
		CAPTURE_HEIGHT = 64
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};
