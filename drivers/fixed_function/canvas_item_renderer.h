#pragma once

#include "core/templates/hash_map.h"
#include "drivers/fixed_function/render_target_manager.h"
#include "servers/rendering/renderer_canvas_render.h"

// The base canvas pass consumes branch-native commands and CPU polygon snapshots.
// Lighting, custom shader execution and screen-reading effects remain unavailable.
class FixedFunctionCanvasItemRenderer {
	friend class TestFixedFunctionCanvasSkinning;
	enum { MAX_NINE_PATCH_QUADS = 65536 };
	enum { MAX_RECT_BATCH_QUADS = 512 };
	enum { MAX_REPEAT_COPIES = 4096 };
	enum { RECT_BATCH_TEXTURE_FLAGS = RendererCanvasRender::CANVAS_RECT_REGION | RendererCanvasRender::CANVAS_RECT_FLIP_H | RendererCanvasRender::CANVAS_RECT_FLIP_V | RendererCanvasRender::CANVAS_RECT_TRANSPOSE | RendererCanvasRender::CANVAS_RECT_CLIP_UV };
	struct FFNinePatchAxis {
		real_t destination[4];
		real_t source[4];
		real_t tile_size;
		int tiles[3];
	};
	struct FFCanvasPolygon {
		Vector<Point2> points;
		Vector<Point2> uvs;
		Vector<Color> colors;
		Vector<int> indices;
		Vector<int> bones;
		Vector<float> weights;
		int count;
	};

	HashMap<uint64_t, FFCanvasPolygon> _polygons;
	Vector<Point2> _skinned_points;
	Vector<FixedFunctionVertex> _vertices;
	Vector<FixedFunctionVertex> _flipped_vertices;
	FixedFunctionRenderContext &_context;
	FixedFunctionRenderTargetManager &_render_targets;
	uint64_t _next_polygon;
	RenderingServerTypes::RenderInfo _render_info;
	bool _pixel_snap_enabled;
	bool _redraw_requested;

	Error _submit(const FixedFunctionVertex *p_vertices, int p_count, RID p_texture, FixedFunctionGraphicsDevice::FFDrawParameters p_state, const Rect2 *p_glyph_uv = nullptr, bool p_msdf = false, uint8_t p_msdf_outline = 0);
	Error _draw_rect(const RendererCanvasRender::Item::CommandRect &p_rect, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	Error _draw_rect_run(const RendererCanvasRender::Item::Command *&r_command, const Transform2D &p_item_transform, Transform2D &r_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, int &r_drawn_rects);
	static const RendererCanvasRender::Item::CommandRect *_get_batchable_item_rect(const RendererCanvasRender::Item &p_item);
	Error _draw_rect_items(RendererCanvasRender::Item *&r_item, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, int p_capacity);
	bool _get_rect_batch_texture_size(RID p_texture, Size2i &r_size) const;
	Error _pack_rect_batch_command(const RendererCanvasRender::Item::CommandRect &p_rect, const Size2i &p_texture_size, const Transform2D &p_transform, const Color &p_modulate, FixedFunctionVertex *r_vertices, int &r_packed) const;
	static bool _get_msdf_outline(const RendererCanvasRender::Item::CommandRect &p_rect, uint8_t &r_outline);
	bool _set_msdf_state(FixedFunctionGraphicsDevice::FFDrawParameters &r_state) const;
	Error _pack_rect(const Rect2 &p_rect, const Rect2 &p_source, uint16_t p_flags, const Transform2D &p_transform, const Color &p_color, FixedFunctionVertex *r_vertices) const;
	Error _draw_nine_patch(const RendererCanvasRender::Item::CommandNinePatch &p_patch, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	static Error _prepare_nine_patch_axis(FFNinePatchAxis &r_axis, real_t p_destination_size, real_t p_source_size, real_t p_begin, real_t p_end, real_t p_scale, RSE::NinePatchAxisMode p_mode, bool p_middle_needed);
	Error _draw_tiled_nine_patch(const RendererCanvasRender::Item::CommandNinePatch &p_patch, const Rect2 &p_source, const Size2i &p_texture_size, real_t p_scale, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	Error _draw_thin_line(const Vector2 &p_from, const Vector2 &p_to, const Vector2 &p_uv_from, const Vector2 &p_uv_to, const Color &p_color_from, const Color &p_color_to, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	Error _draw_polygon(const Vector<Point2> &p_points, const Vector<Point2> &p_uvs, const Vector<Color> &p_colors, const Vector<int> &p_indices, int p_count, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_snap_vertices = true);
	Error _draw_stored_polygon(const RendererCanvasRender::Item::CommandPolygon &p_command, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, const Vector<Transform2D> &p_bones, const Transform2D &p_skeleton_transform);
	Error _draw_hairlines(const FFCanvasPolygon &p_polygon, bool p_strip, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state);
	Error _draw_mesh(RID p_mesh, RID p_multimesh, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, RID p_mesh_instance = RID(), const Transform2D &p_canvas_transform = Transform2D());
	Error _render_item_commands(const RendererCanvasRender::Item &p_item, const Transform2D &p_transform, const Color &p_modulate, FixedFunctionGraphicsDevice::FFDrawParameters p_state, const Vector<Transform2D> &p_bones, const Transform2D &p_skeleton_transform, const Transform2D &p_canvas_transform);
	Error _render_items(RendererCanvasRender::Item *p_items, const Color &p_modulate, RSE::CanvasItemTextureFilter p_filter, RSE::CanvasItemTextureRepeat p_repeat, const Transform2D &p_canvas_transform);
	static Error _skin_polygon(const Vector<Point2> &p_points, const Vector<int> &p_bone_indices, const Vector<float> &p_bone_weights, const Vector<Transform2D> &p_bones, const Transform2D &p_transform, const Transform2D &p_skeleton_transform, Vector<Point2> &r_points);
	Vector2 _snap_position(const Vector2 &p_position) const { return _pixel_snap_enabled ? (p_position + Vector2(0.5, 0.5)).floor() : p_position; }
	static Error _pack_vertex(FixedFunctionVertex &r_vertex, const Vector2 &p_position, const Vector2 &p_uv, const Color &p_color);
	static Error _get_clip(const Rect2 &p_rect, Rect2i &r_clip);
	Error _set_sampler(FixedFunctionGraphicsDevice::FFDrawParameters &r_state, RSE::CanvasItemTextureFilter p_filter, RSE::CanvasItemTextureRepeat p_repeat) const;

	FixedFunctionCanvasItemRenderer(const FixedFunctionCanvasItemRenderer &);
	FixedFunctionCanvasItemRenderer &operator=(const FixedFunctionCanvasItemRenderer &);

public:
	// Ordered blending, target-pixel clipping and per-texture sampling.
	Error render_items(RendererCanvasRender::Item *p_items, const Color &p_modulate, RSE::CanvasItemTextureFilter p_filter = RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RSE::CanvasItemTextureRepeat p_repeat = RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED, const Transform2D &p_canvas_transform = Transform2D(), bool p_snap_vertices = false);
	// Nonzero IDs are never reused; snapshots remain valid until explicit release/destruction.
	Error polygon_create(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, int p_count, uint64_t &r_polygon, const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>());
	Error polygon_free(uint64_t p_polygon);
	int get_polygon_count() const { return _polygons.size(); }
	const RenderingServerTypes::RenderInfo &get_render_info() const { return _render_info; }
	bool is_redraw_requested() const { return _redraw_requested; }

	FixedFunctionCanvasItemRenderer(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets);
};
