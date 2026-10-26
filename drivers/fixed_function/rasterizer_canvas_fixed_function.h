#ifndef RASTERIZER_CANVAS_FIXED_FUNCTION_H
#define RASTERIZER_CANVAS_FIXED_FUNCTION_H

#include "canvas_item_renderer.h"

// Branch-native canvas facade for the unlit color/texture pass.
// The context/manager outlive this facade; all calls use their owner thread.
class RasterizerCanvasFixedFunction : public RasterizerCanvas {
	FixedFunctionCanvasItemRenderer _items;
	FixedFunctionRenderContext &_context;
	FixedFunctionRenderTargetManager &_render_targets;
	Error _last_error;
	bool _canvas_open;

	void _report_error(Error p_error, const char *p_operation);
	Error _render_items(Item *p_items, const Color &p_modulate, const Transform2D &p_base_transform);
	Error _draw_margins(int *p_margins, RID *p_textures);
	RasterizerCanvasFixedFunction(const RasterizerCanvasFixedFunction &);
	RasterizerCanvasFixedFunction &operator=(const RasterizerCanvasFixedFunction &);

public:
	// Reset the canvas bracket when its owner successfully opens a new logical frame.
	void reset_frame();
	RID light_internal_create() override;
	void light_internal_update(RID p_rid, Light *p_light) override;
	void light_internal_free(RID p_rid) override;
	void canvas_begin() override;
	void canvas_end() override;
	void canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) override;
	void canvas_debug_viewport_shadows(Light *p_lights_with_shadow) override;
	void canvas_light_shadow_buffer_update(RID p_buffer, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, CameraMatrix *p_xform_cache) override;
	void reset_canvas() override;
	void draw_window_margins(int *p_margins, RID *p_margin_textures) override;

	Error get_last_error() const { return _last_error; }
	bool is_canvas_open() const { return _canvas_open; }

	RasterizerCanvasFixedFunction(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets);
};

#endif // RASTERIZER_CANVAS_FIXED_FUNCTION_H
