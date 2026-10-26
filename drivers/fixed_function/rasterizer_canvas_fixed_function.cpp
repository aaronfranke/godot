#include "rasterizer_canvas_fixed_function.h"

#include "servers/visual/visual_server_raster.h"

Error RasterizerCanvasFixedFunction::_render_items(Item *p_items, const Color &p_modulate, const Transform2D &p_base_transform) {
	const Error error = _items.render_items(p_items, p_modulate, p_base_transform);
	if (_items.is_redraw_requested()) {
		VisualServerRaster::redraw_request(false);
	}
	return error;
}

void RasterizerCanvasFixedFunction::reset_frame() {
	_canvas_open = false;
	_last_error = OK;
}

void RasterizerCanvasFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP) {
		_render_targets.report_draw_error(p_error);
		ERR_PRINT(String("Fixed Function canvas ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

RID RasterizerCanvasFixedFunction::light_internal_create() {
	// VisualServerCanvas owns the light. The unlit pass needs no native resource.
	WARN_PRINT_ONCE("Fixed Function draws canvas items without Light2D lighting.");
	return RID();
}

void RasterizerCanvasFixedFunction::light_internal_update(RID p_rid, Light *p_light) {
	ERR_FAIL_COND(p_rid.is_valid());
}

void RasterizerCanvasFixedFunction::light_internal_free(RID p_rid) {
	ERR_FAIL_COND(p_rid.is_valid());
}

void RasterizerCanvasFixedFunction::canvas_begin() {
	const Error error = _context.get_frame_status();
	_report_error(_canvas_open && error != ERR_BUG ? ERR_BUSY : error, "canvas_begin");
	// The server still brackets its canvas calls during a suspended logical frame.
	if (_last_error == OK || _last_error == ERR_SKIP) {
		_canvas_open = true;
	}
}

void RasterizerCanvasFixedFunction::canvas_end() {
	if (!_canvas_open) {
		_report_error(ERR_UNCONFIGURED, "canvas_end");
		return;
	}
	_canvas_open = false;
	// Target switching, native scene closure and presentation belong to the target manager.
}

void RasterizerCanvasFixedFunction::canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {
	if (!_canvas_open) {
		_report_error(ERR_UNCONFIGURED, "render_items");
		return;
	}
	const Error error = _context.get_frame_status();
	if (error != OK) {
		_report_error(error, "render_items");
	} else {
		// Item placement is already baked in. Skinning also needs the skeleton's canvas frame.
		_report_error(_render_items(p_item_list, p_modulate, p_base_transform), "render_items");
	}
}

void RasterizerCanvasFixedFunction::canvas_debug_viewport_shadows(Light *p_lights_with_shadow) {
	if (p_lights_with_shadow) {
		_report_error(ERR_UNAVAILABLE, "debug_viewport_shadows");
	}
}

void RasterizerCanvasFixedFunction::canvas_light_shadow_buffer_update(RID p_buffer, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, CameraMatrix *p_xform_cache) {
	_report_error(ERR_UNAVAILABLE, "shadow_buffer_update");
}

void RasterizerCanvasFixedFunction::reset_canvas() {
	// All native draws bind their own state, so restoration only needs an active target.
	_report_error(_render_targets.begin_draw(), "reset_canvas");
}

Error RasterizerCanvasFixedFunction::_draw_margins(int *p_margins, RID *p_textures) {
	if (!p_margins || !p_textures || _canvas_open) {
		return ERR_INVALID_PARAMETER;
	}
	Error error = _render_targets.select_render_target(RID());
	if (error != OK) {
		return error;
	}
	error = _render_targets.begin_draw();
	if (error != OK) {
		return error;
	}
	const int width = _context.get_device().get_width();
	const int height = _context.get_device().get_height();
	for (int i = 0; i < 4; i++) {
		if (p_margins[i] < 0) {
			return ERR_INVALID_PARAMETER;
		}
	}
	const int left = MIN(p_margins[MARGIN_LEFT], width);
	const int right = MIN(p_margins[MARGIN_RIGHT], width);
	const int top = MIN(p_margins[MARGIN_TOP], height);
	const int bottom = MIN(p_margins[MARGIN_BOTTOM], height);
	const Rect2 rects[] = { Rect2(0, 0, left, height), Rect2(0, 0, width, top), Rect2(width - right, 0, right, height), Rect2(0, height - bottom, width, bottom) };
	// Preserve the existing corner overlap order: left, right, top, bottom.
	const int order[] = { MARGIN_LEFT, MARGIN_RIGHT, MARGIN_TOP, MARGIN_BOTTOM };
	Item item;
	for (int i = 0; i < 4; i++) {
		const int side = order[i];
		if (rects[side].size.x == 0 || rects[side].size.y == 0) {
			continue;
		}
		Item::CommandRect *rect = memnew(Item::CommandRect);
		rect->rect = rects[side];
		rect->texture = p_textures[side];
		rect->modulate = rect->texture.is_valid() ? Color(1, 1, 1) : Color(0, 0, 0);
		if (rect->texture.is_valid()) {
			// Existing margin images use pixel-sized UV regions and their texture's sampler.
			rect->source = Rect2(Vector2(), rect->rect.size);
			rect->flags = CANVAS_RECT_REGION;
		}
		item.commands.push_back(rect);
	}
	return _render_items(&item, Color(1, 1, 1), Transform2D());
}

void RasterizerCanvasFixedFunction::draw_window_margins(int *p_margins, RID *p_margin_textures) {
	_report_error(_draw_margins(p_margins, p_margin_textures), "draw_window_margins");
}

RasterizerCanvasFixedFunction::RasterizerCanvasFixedFunction(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets) :
		_items(p_context, p_render_targets),
		_context(p_context),
		_render_targets(p_render_targets),
		_last_error(OK),
		_canvas_open(false) {
}
