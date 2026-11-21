#pragma once

#include "drivers/fixed_function/canvas_item_renderer.h"

// Construct only with the real renderer: the base owns the canvas singleton.
// Tests exercise the independent command consumer without replacing that singleton.
class RendererCanvasRenderFixedFunction : public RendererCanvasRender {
	FixedFunctionCanvasItemRenderer _items;
	FixedFunctionRenderTargetManager &_render_targets;
	Error _last_error;

	void _report_error(Error p_error, const char *p_operation);
	RendererCanvasRenderFixedFunction(const RendererCanvasRenderFixedFunction &);
	RendererCanvasRenderFixedFunction &operator=(const RendererCanvasRenderFixedFunction &);

public:
	PolygonID request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>(), int p_count = -1) override;
	void free_polygon(PolygonID p_polygon) override;
	void canvas_render_items(RID p_to_render_target, Item *p_item_list, const Color &p_modulate, Light *p_light_list, Light *p_directional_list, const Transform2D &p_canvas_transform, RSE::CanvasItemTextureFilter p_default_filter, RSE::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_vertices_to_pixel, bool &r_sdf_used, RenderingServerTypes::RenderInfo *r_render_info = nullptr) override;
	RID light_create() override;
	void light_set_texture(RID p_rid, RID p_texture) override;
	void light_set_use_shadow(RID p_rid, bool p_enable) override;
	void light_update_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, const Rect2 &p_light_rect) override;
	void light_update_directional_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_cull_distance, const Rect2 &p_clip_rect, LightOccluderInstance *p_occluders) override;
	void render_sdf(RID p_render_target, LightOccluderInstance *p_occluders) override;
	RID occluder_polygon_create() override;
	void occluder_polygon_set_shape(RID p_occluder, const Vector<Vector2> &p_points, bool p_closed) override;
	void occluder_polygon_set_cull_mode(RID p_occluder, RSE::CanvasOccluderPolygonCullMode p_mode) override;
	void set_shadow_texture_size(int p_size) override;
	bool free(RID p_rid) override;
	void update() override;
	void set_debug_redraw(bool p_enabled, double p_time, const Color &p_color) override;
	uint32_t get_pipeline_compilations(RSE::PipelineSource p_source) override;

	Error get_last_error() const { return _last_error; }
	RendererCanvasRenderFixedFunction(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets);
};
