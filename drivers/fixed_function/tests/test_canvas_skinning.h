#pragma once

#include "drivers/fixed_function/canvas_item_renderer.h"

#include <vector>

class TestFixedFunctionCanvasSkinning {
	static bool _storage();
	static void _changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker);
	static bool _positions();
	static bool _mesh_positions();
	static bool _pixel(const std::vector<uint32_t> &p_pixels, int p_x, int p_y, uint32_t p_color);

public:
	static bool cpu();
	static bool test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, std::vector<uint32_t> &r_pixels);
};
