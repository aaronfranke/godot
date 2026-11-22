#ifndef FIXED_FUNCTION_CANVAS_ITEM_RENDERER_H
#define FIXED_FUNCTION_CANVAS_ITEM_RENDERER_H

#include "render_target_manager.h"

#include "servers/visual/rasterizer.h"

// The 3.x canvas pass consumes server commands directly, including weighted polygons.
// Lighting and screen-reading effects require separate implementations.
class FixedFunctionCanvasItemRenderer {
	friend class TestFixedFunctionCanvasSkinning;
	enum { MAX_NINE_PATCH_QUADS = 65536 };
	enum { MAX_RECT_BATCH_QUADS = 512 };
	enum { RECT_BATCH_TEXTURE_FLAGS = RasterizerCanvas::CANVAS_RECT_REGION | RasterizerCanvas::CANVAS_RECT_FLIP_H | RasterizerCanvas::CANVAS_RECT_FLIP_V | RasterizerCanvas::CANVAS_RECT_TRANSPOSE | RasterizerCanvas::CANVAS_RECT_CLIP_UV };
	struct FFNinePatchAxis {
		real_t destination[4];
		real_t source[4];
		real_t tile_size;
		int tiles[3];
	};

	Vector<FixedFunctionVertex> _vertices;
	Vector<FixedFunctionVertex> _flipped_vertices;
	Vector<Point2> _skinned_points;
	FixedFunctionRenderContext &_context;
	FixedFunctionRenderTargetManager &_render_targets;
	bool _pixel_snap_enabled;
	bool _redraw_requested;

	Error _submit(const FixedFunctionVertex *p_vertices, int p_count, RID p_texture, FixedFunctionGraphicsDevice::FFDrawParameters p_state, const Rect2 *p_glyph_uv = nullptr, bool p_distance_field = false);
	Error _draw_rect(const RasterizerCanvas::Item::CommandRect &p_rect, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_distance_field_item);
	Error _draw_rect_run(const Vector<RasterizerCanvas::Item::Command *> &p_commands, int &r_index, const Transform2D &p_item_transform, Transform2D &r_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_distance_field_item);
	static const RasterizerCanvas::Item::CommandRect *_get_batchable_item_rect(const RasterizerCanvas::Item &p_item);
	Error _draw_rect_items(RasterizerCanvas::Item *&r_item, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, int p_capacity);
	bool _get_rect_batch_texture_size(RID p_texture, Size2i &r_size) const;
	Error _pack_rect_batch_command(const RasterizerCanvas::Item::CommandRect &p_rect, const Size2i &p_texture_size, const Transform2D &p_transform, const Color &p_modulate, FixedFunctionVertex *r_vertices, int &r_packed) const;
	bool _set_distance_field_state(FixedFunctionGraphicsDevice::FFDrawParameters &r_state) const;
	Error _draw_multirect(const RasterizerCanvas::Item::CommandMultiRect &p_rects, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_distance_field_item);
	Error _pack_rect(const Rect2 &p_rect, const Rect2 &p_source, uint32_t p_flags, const Transform2D &p_transform, const Color &p_color, FixedFunctionVertex *r_vertices) const;
	Error _draw_nine_patch(const RasterizerCanvas::Item::CommandNinePatch &p_patch, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	static Error _prepare_nine_patch_axis(FFNinePatchAxis &r_axis, real_t p_destination_size, real_t p_source_size, real_t p_begin, real_t p_end, real_t p_scale, VS::NinePatchAxisMode p_mode, bool p_middle_needed);
	Error _draw_tiled_nine_patch(const RasterizerCanvas::Item::CommandNinePatch &p_patch, const Rect2 &p_source, const Size2i &p_texture_size, real_t p_scale, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	Error _draw_circle(const RasterizerCanvas::Item::CommandCircle &p_circle, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	Error _draw_line(const RasterizerCanvas::Item::CommandLine &p_line, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	Error _draw_antialiased_quad(const Vector<Point2> &p_points, const Transform2D &p_transform, const Color &p_color, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_snap_vertices);
	Error _draw_polyline(const RasterizerCanvas::Item::CommandPolyLine &p_line, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	Error _draw_hairlines(const RasterizerCanvas::Item::CommandPolyLine &p_line, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	Error _draw_polyline_fringe(const RasterizerCanvas::Item::CommandPolyLine &p_line, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	static Color _coverage_edge_color(const Color &p_color, FixedFunctionGraphicsDevice::FFBlendMode p_blend);
	Error _draw_polygon(const Vector<Point2> &p_points, const Vector<Point2> &p_uvs, const Vector<Color> &p_colors, const Vector<int> &p_indices, int p_count, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_snap_vertices = true);
	static Error _skin_polygon(const Vector<Point2> &p_points, const Vector<int> &p_bone_indices, const Vector<float> &p_bone_weights, const Vector<Transform2D> &p_bones, const Transform2D &p_transform, const Transform2D &p_skeleton_transform, Vector<Point2> &r_points, bool p_snap_vertices = false);
	Error _draw_mesh(RID p_mesh, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, const FixedFunctionMeshStorage::FFMultiMeshInstance *p_copies = nullptr, int p_count = 1, bool p_2d = false);
	Error _draw_multimesh(const RasterizerCanvas::Item::CommandMultiMesh &p_command, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	Error _render_items(RasterizerCanvas::Item *p_items, const Color &p_modulate, const Transform2D &p_base_transform);
	static Vector2 _snap_position(const Vector2 &p_position, bool p_enabled) { return p_enabled ? (p_position + Vector2(0.5, 0.5)).floor() : p_position; }
	static Error _pack_vertex(FixedFunctionVertex &r_vertex, const Vector2 &p_position, const Vector2 &p_uv, const Color &p_color);
	static Error _get_clip(const Rect2 &p_rect, Rect2i &r_clip);

	FixedFunctionCanvasItemRenderer(const FixedFunctionCanvasItemRenderer &);
	FixedFunctionCanvasItemRenderer &operator=(const FixedFunctionCanvasItemRenderer &);

public:
	// Low-priority scheduling intent from the most recent render_items call.
	bool is_redraw_requested() const { return _redraw_requested; }
	// Ordered MIX blending, target-pixel clipping and per-texture sampling.
	Error render_items(RasterizerCanvas::Item *p_items, const Color &p_modulate, const Transform2D &p_base_transform = Transform2D());

	FixedFunctionCanvasItemRenderer(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets);
};

#endif // FIXED_FUNCTION_CANVAS_ITEM_RENDERER_H
