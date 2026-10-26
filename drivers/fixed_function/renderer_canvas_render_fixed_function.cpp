#include "renderer_canvas_render_fixed_function.h"

#include "servers/rendering/rendering_server_default.h"

void RendererCanvasRenderFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP) {
		_render_targets.report_draw_error(p_error);
		ERR_PRINT(String("Fixed Function canvas ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

RendererCanvasRender::PolygonID RendererCanvasRenderFixedFunction::request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, const Vector<int> &p_bones, const Vector<float> &p_weights, int p_count) {
	PolygonID polygon = 0;
	_report_error(_items.polygon_create(p_indices, p_points, p_colors, p_uvs, p_count, polygon, p_bones, p_weights), "request_polygon");
	return polygon;
}

void RendererCanvasRenderFixedFunction::free_polygon(PolygonID p_polygon) {
	_report_error(_items.polygon_free(p_polygon), "free_polygon");
}

void RendererCanvasRenderFixedFunction::canvas_render_items(RID p_to_render_target, Item *p_item_list, const Color &p_modulate, Light *p_light_list, Light *p_directional_list, const Transform2D &p_canvas_transform, RSE::CanvasItemTextureFilter p_default_filter, RSE::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_vertices_to_pixel, bool &r_sdf_used, RenderingServerTypes::RenderInfo *r_render_info) {
	r_sdf_used = false;
	Error error = _render_targets.select_render_target(p_to_render_target);
	if (error == OK) {
		// final_transform already includes the canvas frame; pass it separately only
		// to recover the skeleton/item frames used by CPU skinning.
		error = _items.render_items(p_item_list, p_modulate, p_default_filter, p_default_repeat, p_canvas_transform, p_snap_2d_vertices_to_pixel);
		if (r_render_info) {
			for (int i = 0; i < RSE::VIEWPORT_RENDER_INFO_MAX; i++) {
				r_render_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][i] += _items.get_render_info().info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][i];
			}
		}
		if (_items.is_redraw_requested()) {
			RenderingServerDefault::redraw_request();
		}
	}
	_report_error(error, "render_items");
}

RID RendererCanvasRenderFixedFunction::light_create() {
	// RendererCanvasCull owns the light. The unlit pass needs no native resource.
	WARN_PRINT_ONCE("Fixed Function draws canvas items without 2D lighting.");
	return RID();
}

void RendererCanvasRenderFixedFunction::light_set_texture(RID p_rid, RID p_texture) {
	ERR_FAIL_COND(p_rid.is_valid());
}

void RendererCanvasRenderFixedFunction::light_set_use_shadow(RID p_rid, bool p_enable) {
	ERR_FAIL_COND(p_rid.is_valid());
}

void RendererCanvasRenderFixedFunction::light_update_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, const Rect2 &p_light_rect) {
	ERR_FAIL_COND(p_rid.is_valid());
}

void RendererCanvasRenderFixedFunction::light_update_directional_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_cull_distance, const Rect2 &p_clip_rect, LightOccluderInstance *p_occluders) {
	ERR_FAIL_COND(p_rid.is_valid());
}

void RendererCanvasRenderFixedFunction::render_sdf(RID p_render_target, LightOccluderInstance *p_occluders) {
	_report_error(ERR_UNAVAILABLE, "render_sdf");
}

RID RendererCanvasRenderFixedFunction::occluder_polygon_create() {
	// The canvas server owns the polygon and its bounds. The unlit pass uses neither.
	return RID();
}

void RendererCanvasRenderFixedFunction::occluder_polygon_set_shape(RID p_occluder, const Vector<Vector2> &p_points, bool p_closed) {
	ERR_FAIL_COND(p_occluder.is_valid());
}

void RendererCanvasRenderFixedFunction::occluder_polygon_set_cull_mode(RID p_occluder, RSE::CanvasOccluderPolygonCullMode p_mode) {
	ERR_FAIL_COND(p_occluder.is_valid());
}

void RendererCanvasRenderFixedFunction::set_shadow_texture_size(int p_size) {
	// The server supplies this global quality hint even without canvas shadow resources.
	_report_error(p_size > 0 ? OK : ERR_INVALID_PARAMETER, "set_shadow_texture_size");
}

bool RendererCanvasRenderFixedFunction::free(RID p_rid) {
	// This profile owns CPU polygon IDs, but creates no lighting/occluder RIDs.
	return false;
}

void RendererCanvasRenderFixedFunction::update() {
	// Polygon snapshots are immediately available on the owning render thread.
}

void RendererCanvasRenderFixedFunction::set_debug_redraw(bool p_enabled, double p_time, const Color &p_color) {
	if (p_enabled) {
		_report_error(ERR_UNAVAILABLE, "set_debug_redraw");
	}
}

uint32_t RendererCanvasRenderFixedFunction::get_pipeline_compilations(RSE::PipelineSource p_source) {
	return 0;
}

RendererCanvasRenderFixedFunction::RendererCanvasRenderFixedFunction(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets) :
		_items(p_context, p_render_targets),
		_render_targets(p_render_targets),
		_last_error(OK) {
}
