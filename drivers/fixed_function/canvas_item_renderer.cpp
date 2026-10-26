#include "canvas_item_renderer.h"

#include "core/math/math_funcs.h"
#include "core/project_settings.h"

#include <float.h>

Error FixedFunctionCanvasItemRenderer::_pack_vertex(FixedFunctionVertex &r_vertex, const Vector2 &p_position, const Vector2 &p_uv, const Color &p_color) {
	const real_t values[] = { p_position.x, p_position.y, p_uv.x, p_uv.y };
	for (int i = 0; i < 4; i++) {
		if (Math::is_nan(values[i]) || Math::is_inf(values[i]) || values[i] < -FLT_MAX || values[i] > FLT_MAX || Math::is_nan(p_color[i]) || Math::is_inf(p_color[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	r_vertex.x = static_cast<float>(p_position.x);
	r_vertex.y = static_cast<float>(p_position.y);
	r_vertex.z = 0;
	r_vertex.u = static_cast<float>(p_uv.x);
	r_vertex.v = static_cast<float>(p_uv.y);
	const Color color(CLAMP(p_color.r, 0.0f, 1.0f), CLAMP(p_color.g, 0.0f, 1.0f), CLAMP(p_color.b, 0.0f, 1.0f), CLAMP(p_color.a, 0.0f, 1.0f));
	r_vertex.color = color.to_argb32();
	return OK;
}

Error FixedFunctionCanvasItemRenderer::_get_clip(const Rect2 &p_rect, Rect2i &r_clip) {
	const real_t values[] = { p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y };
	for (int i = 0; i < 4; i++) {
		if (Math::is_nan(values[i]) || Math::is_inf(values[i]) || double(values[i]) < INT32_MIN || double(values[i]) > INT32_MAX) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (p_rect.size.x < 0 || p_rect.size.y < 0) {
		return ERR_INVALID_PARAMETER;
	}
	// The canvas server rounds clip rectangles; match GLES's integer conversion.
	r_clip = Rect2i(int(p_rect.position.x), int(p_rect.position.y), int(p_rect.size.x), int(p_rect.size.y));
	return OK;
}

Error FixedFunctionCanvasItemRenderer::_submit(const FixedFunctionVertex *p_vertices, int p_count, RID p_texture, FixedFunctionGraphicsDevice::FFDrawParameters p_state, const Rect2 *p_glyph_uv, bool p_distance_field) {
	FixedFunctionGraphicsDevice &device = _context.get_device();
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = device.get_capabilities();
	Error error = _render_targets.begin_draw();
	if (error != OK) {
		return error;
	}
	if (!p_texture.is_valid()) {
		error = device.fill_canvas_triangles(p_vertices, p_count, p_state);
		if (error != ERR_UNAVAILABLE) {
			if (error == OK) {
				_context.get_storage_queries().note_canvas_draw();
			}
			return error;
		}
	}
	FixedFunctionTextureStorage &storage = _context.get_texture_storage();
	RID native;
	RID target;
	if (p_distance_field) {
		native = storage.texture_get_distance_field(p_texture);
		if (!native.is_valid()) {
			return OK;
		}
	} else if (p_texture.is_valid()) {
		target = storage.texture_get_render_target(p_texture);
		if (target.is_valid()) {
			if (!caps.render_target_opaque) {
				return OK; // There is no texture to sample on a window-only device.
			}
			if (target == _render_targets.get_current_render_target()) {
				return ERR_INVALID_PARAMETER;
			}
			// Visibility drives WHEN_VISIBLE updates, even before the first native image exists.
			error = storage.render_target_mark_used(target);
			if (error != OK) {
				return error;
			}
			if (storage.render_target_has_failed(target)) {
				return OK;
			}
		}
		error = storage.texture_get_or_upload_device_texture(p_texture, native);
		if (target.is_valid() && (error == ERR_UNCONFIGURED || (error == OK && !storage.render_target_is_ready(target)))) {
			_redraw_requested = true;
			return _render_targets.defer_render_target();
		}
		if (error == ERR_UNCONFIGURED && !target.is_valid()) {
			return OK; // An ordinary texture without backing only omits this command.
		}
		if (error != OK) {
			return error;
		}
	}
	if (native.is_valid()) {
		p_state.sampler_flags |= storage.texture_get_sampler_flags(p_texture);
	}
	if (target.is_valid() && !storage.render_target_get_flag(target, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP)) {
		// Godot 3's viewport textures have a bottom-left origin; the device API stays top-left.
		// Keep the source intact: polygon batches can already reside in _vertices.
		error = _flipped_vertices.resize(p_count);
		if (error != OK) {
			return error;
		}
		FixedFunctionVertex *vertices = _flipped_vertices.ptrw();
		for (int i = 0; i < p_count; i++) {
			vertices[i] = p_vertices[i];
			vertices[i].v = 1.0f - vertices[i].v;
		}
		p_vertices = vertices;
	}
	error = ERR_UNAVAILABLE;
	if (target.is_valid()) {
		error = device.copy_canvas_render_target(p_vertices, p_count, native, p_state);
	} else if (p_glyph_uv && native.is_valid()) {
		const Point2 start(p_vertices[0].x, p_vertices[0].y);
		const Point2 end(p_vertices[2].x, p_vertices[2].y);
		error = device.blit_canvas_glyph(native, *p_glyph_uv, Rect2(start, end - start), p_vertices[0].color, p_state);
	}
	if (error == ERR_UNAVAILABLE) {
		CameraMatrix projection;
		projection.matrix[0][0] = real_t(2) / device.get_target_width();
		projection.matrix[1][1] = real_t(-2) / device.get_target_height();
		projection.matrix[3][0] = -1;
		projection.matrix[3][1] = 1;
		error = device.draw_triangles(p_vertices, p_count, projection, native, p_state);
	}
	if (p_distance_field && (error == ERR_UNAVAILABLE || error == ERR_OUT_OF_MEMORY || error == ERR_CANT_CREATE)) {
		return OK; // Optional glyph backing must not discard other canvas commands.
	}
	if (error == OK) {
		_context.get_storage_queries().note_canvas_draw();
	}
	if (error == OK && p_texture.is_valid() && storage.texture_get_force_redraw_if_visible(p_texture)) {
		_redraw_requested = true;
	}
	return error;
}

Error FixedFunctionCanvasItemRenderer::_draw_rect(const RasterizerCanvas::Item::CommandRect &p_rect, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_distance_field_item) {
	if (p_rect.rect.size.x < 0 || p_rect.rect.size.y < 0 || p_rect.flags & ~(RasterizerCanvas::CANVAS_RECT_REGION | RasterizerCanvas::CANVAS_RECT_TILE | RasterizerCanvas::CANVAS_RECT_FLIP_H | RasterizerCanvas::CANVAS_RECT_FLIP_V | RasterizerCanvas::CANVAS_RECT_TRANSPOSE | RasterizerCanvas::CANVAS_RECT_CLIP_UV | RasterizerCanvas::CANVAS_RECT_FONT_GLYPH)) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_rect.rect.size.x == 0 || p_rect.rect.size.y == 0) {
		return OK;
	}
	const bool distance_field = p_distance_field_item && (p_rect.flags & RasterizerCanvas::CANVAS_RECT_FONT_GLYPH);
	if (distance_field && !p_rect.texture.is_valid()) {
		return OK;
	}
	Rect2 source(0, 0, 1, 1);
	if (p_rect.texture.is_valid()) {
		FixedFunctionTextureStorage &storage = _context.get_texture_storage();
		if (!storage.owns_texture(p_rect.texture)) {
			return distance_field ? OK : ERR_INVALID_PARAMETER;
		}
		const Size2i size = storage.texture_size_with_proxy(p_rect.texture);
		if (size.x <= 0 || size.y <= 0) {
			return distance_field || !storage.texture_get_render_target(p_rect.texture).is_valid() ? OK : ERR_UNCONFIGURED;
		}
		if (p_rect.flags & RasterizerCanvas::CANVAS_RECT_REGION) {
			if (p_rect.source.size.x < 0 || p_rect.source.size.y < 0) {
				return ERR_INVALID_PARAMETER;
			}
			source = Rect2(p_rect.source.position / Size2(size.x, size.y), p_rect.source.size / Size2(size.x, size.y));
		}
	}
	FixedFunctionVertex vertices[6];
	const Error error = _pack_rect(p_rect.rect, source, p_rect.flags, p_transform, p_rect.modulate * p_modulate, vertices);
	if (error != OK) {
		return error;
	}
	FixedFunctionGraphicsDevice::FFDrawParameters state = p_state;
	if ((p_rect.flags & RasterizerCanvas::CANVAS_RECT_TILE) && _context.get_device().get_capabilities().sampler_repeat) {
		state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V;
	}
	state.font_glyph = (p_rect.flags & RasterizerCanvas::CANVAS_RECT_FONT_GLYPH) != 0;
	if (distance_field && !_set_distance_field_state(state)) {
		return OK;
	}
	const bool glyph = p_rect.flags == (RasterizerCanvas::CANVAS_RECT_REGION | RasterizerCanvas::CANVAS_RECT_FONT_GLYPH) &&
			p_transform[0] == Vector2(1, 0) && p_transform[1] == Vector2(0, 1);
	return _submit(vertices, 6, p_rect.texture, state, glyph ? &source : nullptr, distance_field);
}

bool FixedFunctionCanvasItemRenderer::_set_distance_field_state(FixedFunctionGraphicsDevice::FFDrawParameters &r_state) const {
	if (r_state.blend_mode != FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED) {
		return true;
	}
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = _context.get_device().get_capabilities();
	if (caps.alpha_cutoff) {
		r_state.alpha_cutoff_enabled = true;
		r_state.alpha_cutoff = 0.5f;
	} else if (caps.blend_mix) {
		r_state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
	} else {
		return false;
	}
	return true;
}

Error FixedFunctionCanvasItemRenderer::_draw_rect_run(const Vector<RasterizerCanvas::Item::Command *> &p_commands, int &r_index, const Transform2D &p_item_transform, Transform2D &r_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_distance_field_item) {
	const RasterizerCanvas::Item::CommandRect &first = *static_cast<const RasterizerCanvas::Item::CommandRect *>(p_commands[r_index]);
	const bool textured = first.texture.is_valid();
	const uint32_t glyph_flag = first.flags & RasterizerCanvas::CANVAS_RECT_FONT_GLYPH;
	const uint32_t flags = textured ? RECT_BATCH_TEXTURE_FLAGS | glyph_flag : 0;
	const FixedFunctionGraphicsDevice &device = _context.get_device();
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = device.get_capabilities();
	const int capacity = MIN(caps.max_primitives_per_draw / 2, uint32_t(MAX_RECT_BATCH_QUADS));
	// Keep tiled draws and the native fill and glyph blits on their individual paths.
	if (capacity < 2 || r_index + 1 == p_commands.size() || (!textured && caps.canvas_fill_blit) ||
			(first.flags & ~flags) || (glyph_flag && device.prefers_canvas_glyph_blits())) {
		return _draw_rect(first, r_transform, p_modulate, p_state, p_distance_field_item);
	}
	int last = r_index;
	int count = 1;
	for (int i = r_index + 1; i < p_commands.size() && count < capacity; i++) {
		const RasterizerCanvas::Item::Command *command = p_commands[i];
		if (!command) {
			break;
		}
		if (glyph_flag && command->type == RasterizerCanvas::Item::Command::TYPE_TRANSFORM) {
			continue;
		}
		if (command->type != RasterizerCanvas::Item::Command::TYPE_RECT) {
			break;
		}
		const auto &next = *static_cast<const RasterizerCanvas::Item::CommandRect *>(command);
		if (next.texture != first.texture || (next.flags & ~flags) || (uint32_t(next.flags) & RasterizerCanvas::CANVAS_RECT_FONT_GLYPH) != glyph_flag) {
			break;
		}
		last = i;
		count++;
	}
	Size2i size;
	if (count == 1 || !_get_rect_batch_texture_size(first.texture, size)) {
		return _draw_rect(first, r_transform, p_modulate, p_state, p_distance_field_item);
	}
	const bool distance_field = p_distance_field_item && glyph_flag;
	Error error = _vertices.resize(count * 6);
	if (error != OK) {
		return distance_field ? _draw_rect(first, r_transform, p_modulate, p_state, p_distance_field_item) : error;
	}
	FixedFunctionVertex *vertices = _vertices.ptrw();
	Transform2D transform = r_transform;
	int packed = 0;
	for (int i = r_index; i <= last; i++) {
		const RasterizerCanvas::Item::Command *command = p_commands[i];
		if (command->type == RasterizerCanvas::Item::Command::TYPE_TRANSFORM) {
			transform = p_item_transform * static_cast<const RasterizerCanvas::Item::CommandTransform *>(command)->xform;
			continue;
		}
		const auto &rect = *static_cast<const RasterizerCanvas::Item::CommandRect *>(command);
		error = _pack_rect_batch_command(rect, size, transform, p_modulate, vertices, packed);
		if (error != OK) {
			return error;
		}
	}
	r_index = last;
	r_transform = transform;
	FixedFunctionGraphicsDevice::FFDrawParameters state = p_state;
	state.font_glyph = glyph_flag != 0;
	if (!packed || (distance_field && !_set_distance_field_state(state))) {
		return OK;
	}
	return _submit(vertices, packed * 6, first.texture, state, nullptr, distance_field);
}

const RasterizerCanvas::Item::CommandRect *FixedFunctionCanvasItemRenderer::_get_batchable_item_rect(const RasterizerCanvas::Item &p_item) {
	if (p_item.commands.size() != 1 || !p_item.commands[0] || p_item.commands[0]->type != RasterizerCanvas::Item::Command::TYPE_RECT) {
		return nullptr;
	}
	const auto *rect = static_cast<const RasterizerCanvas::Item::CommandRect *>(p_item.commands[0]);
	const uint32_t flags = rect->texture.is_valid() ? RECT_BATCH_TEXTURE_FLAGS : 0;
	if ((rect->flags & ~flags) || p_item.material.is_valid() || p_item.material_owner || p_item.final_clip_owner ||
			p_item.copy_back_buffer || p_item.vp_render || p_item.skeleton.is_valid()) {
		return nullptr;
	}
	return rect;
}

Error FixedFunctionCanvasItemRenderer::_draw_rect_items(RasterizerCanvas::Item *&r_item, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, int p_capacity) {
	const auto *first = _get_batchable_item_rect(*r_item);
	if (!first) {
		return OK;
	}
	RasterizerCanvas::Item *last = r_item;
	int count = 1;
	for (RasterizerCanvas::Item *item = r_item->next; item && count < p_capacity; item = item->next) {
		const auto *rect = _get_batchable_item_rect(*item);
		if (!rect || rect->texture != first->texture) {
			break;
		}
		last = item;
		count++;
	}
	Size2i size;
	if (count == 1 || !_get_rect_batch_texture_size(first->texture, size)) {
		return OK;
	}
	Error error = _vertices.resize(count * 6);
	if (error != OK) {
		return error;
	}
	FixedFunctionVertex *vertices = _vertices.ptrw();
	int packed = 0;
	for (const RasterizerCanvas::Item *item = r_item; item != last->next; item = item->next) {
		const auto &rect = *static_cast<const RasterizerCanvas::Item::CommandRect *>(item->commands[0]);
		error = _pack_rect_batch_command(rect, size, item->final_transform, item->final_modulate * p_modulate, vertices, packed);
		if (error != OK) {
			return error;
		}
		_context.get_storage_queries().note_canvas_item();
	}
	r_item = last;
	return packed ? _submit(vertices, packed * 6, first->texture, p_state) : OK;
}

bool FixedFunctionCanvasItemRenderer::_get_rect_batch_texture_size(RID p_texture, Size2i &r_size) const {
	if (!p_texture.is_valid()) {
		return true;
	}
	const FixedFunctionTextureStorage &storage = _context.get_texture_storage();
	// Viewport textures keep their individual copy and update paths.
	if (!storage.owns_texture(p_texture) || storage.texture_get_render_target(p_texture).is_valid()) {
		return false;
	}
	r_size = storage.texture_size_with_proxy(p_texture);
	return r_size.x > 0 && r_size.y > 0;
}

Error FixedFunctionCanvasItemRenderer::_pack_rect_batch_command(const RasterizerCanvas::Item::CommandRect &p_rect, const Size2i &p_texture_size, const Transform2D &p_transform, const Color &p_modulate, FixedFunctionVertex *r_vertices, int &r_packed) const {
	if (p_rect.rect.size.x < 0 || p_rect.rect.size.y < 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (!p_rect.rect.size.x || !p_rect.rect.size.y) {
		return OK;
	}
	Rect2 source(0, 0, 1, 1);
	if (p_rect.flags & RasterizerCanvas::CANVAS_RECT_REGION) {
		if (p_rect.source.size.x < 0 || p_rect.source.size.y < 0) {
			return ERR_INVALID_PARAMETER;
		}
		source = Rect2(p_rect.source.position / Size2(p_texture_size), p_rect.source.size / Size2(p_texture_size));
	}
	const Error error = _pack_rect(p_rect.rect, source, p_rect.flags, p_transform, p_rect.modulate * p_modulate, r_vertices + r_packed * 6);
	if (error == OK) {
		r_packed++;
	}
	return error;
}

Error FixedFunctionCanvasItemRenderer::_pack_rect(const Rect2 &p_rect, const Rect2 &p_source, uint32_t p_flags, const Transform2D &p_transform, const Color &p_color, FixedFunctionVertex *r_vertices) const {
	Vector2 uv[] = { p_source.position, p_source.position + Vector2(p_source.size.x, 0), p_source.position + p_source.size, p_source.position + Vector2(0, p_source.size.y) };
	if (p_flags & RasterizerCanvas::CANVAS_RECT_TRANSPOSE) {
		SWAP(uv[1], uv[3]);
	}
	if (p_flags & RasterizerCanvas::CANVAS_RECT_FLIP_H) {
		SWAP(uv[0], uv[1]);
		SWAP(uv[2], uv[3]);
	}
	if (p_flags & RasterizerCanvas::CANVAS_RECT_FLIP_V) {
		SWAP(uv[0], uv[3]);
		SWAP(uv[1], uv[2]);
	}
	// Affine interpolation already stays within the source region (CLIP_UV).
	const Vector2 points[] = { p_rect.position, p_rect.position + Vector2(p_rect.size.x, 0), p_rect.position + p_rect.size, p_rect.position + Vector2(0, p_rect.size.y) };
	const int corners[] = { 0, 1, 2, 5 };
	for (int i = 0; i < 4; i++) {
		const Vector2 position = _snap_position(p_transform.xform(points[i]), _pixel_snap_enabled);
		const Error error = _pack_vertex(r_vertices[corners[i]], position, uv[i], p_color);
		if (error != OK) {
			return error;
		}
	}
	r_vertices[3] = r_vertices[0];
	r_vertices[4] = r_vertices[2];
	return OK;
}

Error FixedFunctionCanvasItemRenderer::_draw_multirect(const RasterizerCanvas::Item::CommandMultiRect &p_rects, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_distance_field_item) {
	const int count = p_rects.rects.size();
	if (count != p_rects.sources.size()) {
		return ERR_INVALID_PARAMETER;
	}
	const uint32_t batch_flags = RasterizerCanvas::CANVAS_RECT_REGION | RasterizerCanvas::CANVAS_RECT_FLIP_H | RasterizerCanvas::CANVAS_RECT_FLIP_V | RasterizerCanvas::CANVAS_RECT_TRANSPOSE | RasterizerCanvas::CANVAS_RECT_CLIP_UV;
	const int capacity = MIN(_context.get_device().get_capabilities().max_primitives_per_draw / 2, uint32_t(MAX_RECT_BATCH_QUADS));
	const FixedFunctionTextureStorage &storage = _context.get_texture_storage();
	// Fonts arrive here as one command per run. Batch them unless the device blits glyphs natively.
	const uint32_t glyph_flag = p_rects.flags & RasterizerCanvas::CANVAS_RECT_FONT_GLYPH;
	// Keep individual glyph and viewport draws eligible for their native blits.
	if (count < 2 || capacity < 2 || !(p_rects.flags & RasterizerCanvas::CANVAS_RECT_REGION) || (p_rects.flags & ~(batch_flags | glyph_flag)) ||
			(glyph_flag && _context.get_device().prefers_canvas_glyph_blits()) ||
			!storage.owns_texture(p_rects.texture) || storage.texture_get_render_target(p_rects.texture).is_valid()) {
		RasterizerCanvas::Item::CommandRect rect;
		rect.texture = p_rects.texture;
		rect.modulate = p_rects.modulate;
		rect.flags = p_rects.flags;
		for (int i = 0; i < count; i++) {
			rect.rect = p_rects.rects[i];
			rect.source = p_rects.sources[i];
			const Error error = _draw_rect(rect, p_transform, p_modulate, p_state, p_distance_field_item);
			if (error != OK) {
				return error;
			}
		}
		return OK;
	}

	const Size2i size = storage.texture_size_with_proxy(p_rects.texture);
	if (size.x <= 0 || size.y <= 0) {
		return OK;
	}
	const Color color = p_rects.modulate * p_modulate;
	FixedFunctionGraphicsDevice::FFDrawParameters state = p_state;
	state.font_glyph = glyph_flag != 0;
	const bool distance_field = p_distance_field_item && glyph_flag;
	if (distance_field && !_set_distance_field_state(state)) {
		return OK;
	}
	Error error = _vertices.resize(MIN(count, capacity) * 6);
	if (error != OK) {
		return error;
	}
	FixedFunctionVertex *vertices = _vertices.ptrw();
	int packed = 0;
	for (int i = 0; i < count; i++) {
		const Rect2 &rect = p_rects.rects[i];
		if (rect.size.x < 0 || rect.size.y < 0) {
			return ERR_INVALID_PARAMETER;
		}
		if (!rect.size.x || !rect.size.y) {
			continue;
		}
		const Rect2 &source = p_rects.sources[i];
		if (source.size.x < 0 || source.size.y < 0) {
			return ERR_INVALID_PARAMETER;
		}
		const Rect2 uv(source.position / Size2(size), source.size / Size2(size));
		error = _pack_rect(rect, uv, p_rects.flags, p_transform, color, vertices + packed * 6);
		if (error != OK) {
			return error;
		}
		packed++;
		if (packed == capacity) {
			error = _submit(vertices, packed * 6, p_rects.texture, state, nullptr, distance_field);
			if (error != OK) {
				return error;
			}
			packed = 0;
		}
	}
	return packed ? _submit(vertices, packed * 6, p_rects.texture, state, nullptr, distance_field) : OK;
}

Error FixedFunctionCanvasItemRenderer::_draw_polygon(const Vector<Point2> &p_points, const Vector<Point2> &p_uvs, const Vector<Color> &p_colors, const Vector<int> &p_indices, int p_count, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_snap_vertices) {
	if (p_count < 0 || p_count % 3 || p_count > (p_indices.empty() ? p_points.size() : p_indices.size()) || (!p_uvs.empty() && p_uvs.size() != p_points.size()) || (!p_colors.empty() && p_colors.size() != 1 && p_colors.size() != p_points.size())) {
		return ERR_INVALID_PARAMETER;
	}
	if (!p_count) {
		return OK;
	}
	// Bounded expansion also accommodates devices with small primitive-count limits.
	const uint32_t batch_triangles = MIN(uint32_t(128), _context.get_device().get_capabilities().max_primitives_per_draw);
	if (!batch_triangles) {
		return ERR_UNAVAILABLE;
	}
	const int batch_size = int(batch_triangles * 3);
	Error error = _vertices.resize(MIN(p_count, batch_size));
	if (error != OK) {
		return error;
	}
	const bool snap = _pixel_snap_enabled && p_snap_vertices;
	for (int offset = 0; offset < p_count;) {
		const int count = MIN(p_count - offset, batch_size);
		FixedFunctionVertex *vertices = _vertices.ptrw();
		for (int i = 0; i < count; i++) {
			const int index = p_indices.empty() ? offset + i : p_indices[offset + i];
			if (index < 0 || index >= p_points.size()) {
				return ERR_INVALID_PARAMETER;
			}
			Color color = p_modulate;
			if (!p_colors.empty()) {
				color = p_colors[p_colors.size() == 1 ? 0 : index] * p_modulate;
			}
			const Vector2 position = _snap_position(p_transform.xform(p_points[index]), snap);
			error = _pack_vertex(vertices[i], position, p_uvs.empty() ? Vector2() : p_uvs[index], color);
			if (error != OK) {
				return error;
			}
		}
		error = _submit(vertices, count, p_texture, p_state);
		if (error != OK) {
			return error;
		}
		offset += count;
	}
	return OK;
}

Error FixedFunctionCanvasItemRenderer::_draw_nine_patch(const RasterizerCanvas::Item::CommandNinePatch &p_patch, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	if (p_patch.axis_x < VS::NINE_PATCH_STRETCH || p_patch.axis_x > VS::NINE_PATCH_TILE_FIT || p_patch.axis_y < VS::NINE_PATCH_STRETCH || p_patch.axis_y > VS::NINE_PATCH_TILE_FIT) {
		return ERR_INVALID_PARAMETER;
	}
	const real_t values[] = { p_patch.rect.position.x, p_patch.rect.position.y, p_patch.rect.size.x, p_patch.rect.size.y, p_patch.source.position.x, p_patch.source.position.y, p_patch.source.size.x, p_patch.source.size.y };
	for (int i = 0; i < 8; i++) {
		if (Math::is_nan(values[i]) || Math::is_inf(values[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	FixedFunctionTextureStorage &storage = _context.get_texture_storage();
	if (!storage.owns_texture(p_patch.texture) || p_patch.rect.size.x < 0 || p_patch.rect.size.y < 0) {
		return ERR_INVALID_PARAMETER;
	}
	const Size2i size = storage.texture_size_with_proxy(p_patch.texture);
	if (size.x <= 0 || size.y <= 0) {
		return storage.texture_get_render_target(p_patch.texture).is_valid() ? ERR_UNCONFIGURED : OK;
	}
	Rect2 source = p_patch.source;
	if (source.size == Size2()) {
		source.size = Size2(size.x, size.y);
	}
	if (source.size.x <= 0 || source.size.y <= 0) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < 4; i++) {
		if (Math::is_nan(p_patch.margin[i]) || Math::is_inf(p_patch.margin[i]) || p_patch.margin[i] < 0) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (p_patch.rect.size.x == 0 || p_patch.rect.size.y == 0) {
		return OK;
	}
	real_t screen_scale = 1;
	// Match 3.x's Fixed/Scaling nine-patch setting, including its default Scaling mode.
	ProjectSettings *settings = ProjectSettings::get_singleton();
	if (!settings->has_setting("rendering/2d/options/ninepatch_mode") || int(settings->get("rendering/2d/options/ninepatch_mode")) == 1) {
		screen_scale = MIN(real_t(1), MIN(p_patch.rect.size.x / source.size.x, p_patch.rect.size.y / source.size.y));
	}
	if (p_patch.axis_x != VS::NINE_PATCH_STRETCH || p_patch.axis_y != VS::NINE_PATCH_STRETCH) {
		return _draw_tiled_nine_patch(p_patch, source, size, screen_scale, p_transform, p_modulate, p_state);
	}
	const real_t left = p_patch.margin[MARGIN_LEFT];
	const real_t right = p_patch.margin[MARGIN_RIGHT];
	const real_t top = p_patch.margin[MARGIN_TOP];
	const real_t bottom = p_patch.margin[MARGIN_BOTTOM];
	const real_t x[] = { 0, left * screen_scale, p_patch.rect.size.x - right * screen_scale, p_patch.rect.size.x };
	const real_t y[] = { 0, top * screen_scale, p_patch.rect.size.y - bottom * screen_scale, p_patch.rect.size.y };
	const real_t u[] = { 0, left, source.size.x - right, source.size.x };
	const real_t v[] = { 0, top, source.size.y - bottom, source.size.y };
	Vector<Point2> points;
	Vector<Point2> uvs;
	Vector<int> indices;
	Vector<Color> colors;
	colors.push_back(p_patch.color);
	for (int row = 0; row < 4; row++) {
		for (int column = 0; column < 4; column++) {
			points.push_back(p_patch.rect.position + Vector2(x[column], y[row]));
			uvs.push_back((source.position + Vector2(u[column], v[row])) / Size2(size.x, size.y));
		}
	}
	for (int row = 0; row < 3; row++) {
		for (int column = 0; column < 3; column++) {
			if (!p_patch.draw_center && row == 1 && column == 1) {
				continue;
			}
			const int base = row * 4 + column;
			const int quad[] = { base, base + 1, base + 5, base, base + 5, base + 4 };
			for (int i = 0; i < 6; i++) {
				indices.push_back(quad[i]);
			}
		}
	}
	return _draw_polygon(points, uvs, colors, indices, indices.size(), p_patch.texture, p_transform, p_modulate, p_state);
}

Error FixedFunctionCanvasItemRenderer::_prepare_nine_patch_axis(FFNinePatchAxis &r_axis, real_t p_destination_size, real_t p_source_size, real_t p_begin, real_t p_end, real_t p_scale, VS::NinePatchAxisMode p_mode, bool p_middle_needed) {
	r_axis.destination[0] = 0;
	r_axis.destination[1] = p_begin * p_scale;
	r_axis.destination[2] = p_destination_size - p_end * p_scale;
	r_axis.destination[3] = p_destination_size;
	r_axis.source[0] = 0;
	r_axis.source[1] = p_begin;
	r_axis.source[2] = p_source_size - p_end;
	r_axis.source[3] = p_source_size;
	if (r_axis.destination[1] > r_axis.destination[2] || r_axis.source[1] > r_axis.source[2]) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < 3; i++) {
		r_axis.tiles[i] = r_axis.destination[i + 1] > r_axis.destination[i] ? 1 : 0;
	}
	if (!p_middle_needed) {
		r_axis.tiles[1] = 0;
	}
	const real_t destination = r_axis.destination[2] - r_axis.destination[1];
	const real_t source = r_axis.source[2] - r_axis.source[1];
	r_axis.tile_size = destination;
	if (r_axis.tiles[1] && p_mode != VS::NINE_PATCH_STRETCH) {
		if (source <= 0) {
			return ERR_INVALID_PARAMETER;
		}
		// Widen the count calculation before converting; very small tiles can overflow real_t.
		const double divisor = p_mode == VS::NINE_PATCH_TILE_FIT ? MAX(double(source), double(real_t(0.0000001))) : double(source);
		const double ratio = double(destination) / divisor;
		const double count = p_mode == VS::NINE_PATCH_TILE ? MAX(1.0, Math::ceil(ratio)) : MAX(1.0, Math::floor(ratio + 0.5));
		if (count > double(MAX_NINE_PATCH_QUADS)) {
			return ERR_UNAVAILABLE;
		}
		r_axis.tiles[1] = int(count);
		r_axis.tile_size = p_mode == VS::NINE_PATCH_TILE ? source : destination / r_axis.tiles[1];
	}
	return OK;
}

Error FixedFunctionCanvasItemRenderer::_draw_tiled_nine_patch(const RasterizerCanvas::Item::CommandNinePatch &p_patch, const Rect2 &p_source, const Size2i &p_texture_size, real_t p_scale, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	FFNinePatchAxis horizontal;
	FFNinePatchAxis vertical;
	const bool middle_x = p_patch.draw_center || p_patch.margin[MARGIN_TOP] * p_scale > 0 || p_patch.margin[MARGIN_BOTTOM] * p_scale > 0;
	const bool middle_y = p_patch.draw_center || p_patch.margin[MARGIN_LEFT] * p_scale > 0 || p_patch.margin[MARGIN_RIGHT] * p_scale > 0;
	Error error = _prepare_nine_patch_axis(horizontal, p_patch.rect.size.x, p_source.size.x, p_patch.margin[MARGIN_LEFT], p_patch.margin[MARGIN_RIGHT], p_scale, p_patch.axis_x, middle_x);
	if (error != OK) {
		return error;
	}
	error = _prepare_nine_patch_axis(vertical, p_patch.rect.size.y, p_source.size.y, p_patch.margin[MARGIN_TOP], p_patch.margin[MARGIN_BOTTOM], p_scale, p_patch.axis_y, middle_y);
	if (error != OK) {
		return error;
	}
	uint64_t quads = 0;
	for (int row = 0; row < 3; row++) {
		for (int column = 0; column < 3; column++) {
			if (p_patch.draw_center || row != 1 || column != 1) {
				quads += uint64_t(horizontal.tiles[column]) * vertical.tiles[row];
			}
		}
	}
	// Bound total CPU work before any native draw; batches separately bound memory and draw size.
	if (quads > MAX_NINE_PATCH_QUADS) {
		return ERR_UNAVAILABLE;
	}
	if (!quads) {
		return OK;
	}
	const int batch_size = int(MIN(uint32_t(128), _context.get_device().get_capabilities().max_primitives_per_draw) * 3);
	if (!batch_size) {
		return ERR_UNAVAILABLE;
	}
	error = _vertices.resize(MIN(int(quads * 6), batch_size));
	if (error != OK) {
		return error;
	}
	FixedFunctionVertex *vertices = _vertices.ptrw();
	const Color color = p_patch.color * p_modulate;
	const int indices[] = { 0, 1, 2, 0, 2, 3 };
	int count = 0;
	for (int row = 0; row < 3; row++) {
		for (int column = 0; column < 3; column++) {
			if (!p_patch.draw_center && row == 1 && column == 1) {
				continue;
			}
			const real_t step_x = column == 1 ? horizontal.tile_size : horizontal.destination[column + 1] - horizontal.destination[column];
			const real_t step_y = row == 1 ? vertical.tile_size : vertical.destination[row + 1] - vertical.destination[row];
			for (int y = 0; y < vertical.tiles[row]; y++) {
				const real_t y0 = vertical.destination[row] + step_y * y;
				const real_t y1 = MIN(vertical.destination[row + 1], vertical.destination[row] + step_y * (y + 1));
				const real_t v1 = row == 1 && p_patch.axis_y == VS::NINE_PATCH_TILE ? vertical.source[row] + (y1 - y0) : vertical.source[row + 1];
				for (int x = 0; x < horizontal.tiles[column]; x++) {
					const real_t x0 = horizontal.destination[column] + step_x * x;
					const real_t x1 = MIN(horizontal.destination[column + 1], horizontal.destination[column] + step_x * (x + 1));
					const real_t u1 = column == 1 && p_patch.axis_x == VS::NINE_PATCH_TILE ? horizontal.source[column] + (x1 - x0) : horizontal.source[column + 1];
					const Vector2 positions[] = { Vector2(x0, y0), Vector2(x1, y0), Vector2(x1, y1), Vector2(x0, y1) };
					const Vector2 uvs[] = { Vector2(horizontal.source[column], vertical.source[row]), Vector2(u1, vertical.source[row]), Vector2(u1, v1), Vector2(horizontal.source[column], v1) };
					for (int i = 0; i < 6; i++) {
						const int index = indices[i];
						const Vector2 position = _snap_position(p_transform.xform(p_patch.rect.position + positions[index]), _pixel_snap_enabled);
						error = _pack_vertex(vertices[count], position, (p_source.position + uvs[index]) / Size2(p_texture_size.x, p_texture_size.y), color);
						if (error != OK) {
							return error;
						}
						if (++count == _vertices.size()) {
							error = _submit(vertices, count, p_patch.texture, p_state);
							if (error != OK) {
								return error;
							}
							count = 0;
						}
					}
				}
			}
		}
	}
	return count ? _submit(vertices, count, p_patch.texture, p_state) : OK;
}

Error FixedFunctionCanvasItemRenderer::_draw_circle(const RasterizerCanvas::Item::CommandCircle &p_circle, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	if (Math::is_nan(p_circle.radius) || Math::is_inf(p_circle.radius) || p_circle.radius < 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_circle.radius == 0) {
		return OK;
	}
	// Match GLES2's 32-segment, untextured circle rather than requiring native wide lines.
	const int segments = 32;
	Vector<Point2> points;
	Vector<int> indices;
	Vector<Color> colors;
	colors.push_back(p_circle.color);
	for (int i = 0; i < segments; i++) {
		const real_t angle = real_t(i) * real_t(Math_PI * 2) / segments;
		points.push_back(p_circle.pos + Vector2(Math::sin(angle), Math::cos(angle)) * p_circle.radius);
		indices.push_back(i);
		indices.push_back((i + 1) % segments);
		indices.push_back(segments);
	}
	points.push_back(p_circle.pos);
	return _draw_polygon(points, Vector<Point2>(), colors, indices, indices.size(), RID(), p_transform, p_modulate, p_state);
}

Error FixedFunctionCanvasItemRenderer::_draw_antialiased_quad(const Vector<Point2> &p_points, const Transform2D &p_transform, const Color &p_color, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_snap_vertices) {
	if (p_state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED) {
		return ERR_UNAVAILABLE;
	}
	Vector2 points[4];
	Vector2 normals[4];
	Vector2 center;
	for (int i = 0; i < 4; i++) {
		points[i] = _snap_position(p_transform.xform(p_points[i]), _pixel_snap_enabled && p_snap_vertices);
		FixedFunctionVertex vertex;
		const Error error = _pack_vertex(vertex, points[i], Vector2(), p_color);
		if (error != OK) {
			return error;
		}
		center += points[i] * real_t(0.25);
	}
	for (int i = 0; i < 4; i++) {
		const Vector2 edge = points[(i + 1) % 4] - points[i];
		const real_t length = edge.length();
		if (Math::is_nan(length) || Math::is_inf(length)) {
			return ERR_INVALID_PARAMETER;
		}
		if (length == 0) {
			return OK;
		}
		normals[i] = (edge / length).tangent();
		if (normals[i].dot(center - points[i]) > 0) {
			normals[i] = -normals[i];
		}
	}
	const real_t extent = MIN(Math::abs(normals[0].dot(points[2] - points[0])), Math::abs(normals[1].dot(points[3] - points[1])));
	if (extent == 0) {
		return OK;
	}
	// Approximate coverage with a one-pixel fringe, independently of canvas scale/shear.
	const real_t inset = MIN(real_t(0.5), extent * real_t(0.5));
	const float coverage = static_cast<float>(MIN(real_t(1), extent));
	Color inside = p_color;
	const Color outside = _coverage_edge_color(p_color, p_state.blend_mode);
	if (p_state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_PREMULTIPLIED_ALPHA) {
		inside *= coverage;
	} else if (p_state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MUL) {
		inside = Color(1, 1, 1, 0).linear_interpolate(inside, coverage);
	} else {
		inside.a *= coverage;
	}
	Vector<Point2> vertices;
	Vector<Color> colors;
	vertices.resize(8);
	colors.resize(8);
	for (int i = 0; i < 4; i++) {
		const Vector2 previous = normals[(i + 3) % 4];
		const real_t denominator = real_t(1) + previous.dot(normals[i]);
		if (denominator <= CMP_EPSILON) {
			return ERR_UNAVAILABLE;
		}
		const Vector2 miter = (previous + normals[i]) / denominator;
		vertices.write[i] = points[i] - miter * inset;
		vertices.write[i + 4] = points[i] + miter * real_t(0.5);
		colors.write[i] = inside;
		colors.write[i + 4] = outside;
	}
	Vector<int> indices;
	const int body[] = { 0, 1, 2, 0, 2, 3 };
	for (int i = 0; i < 6; i++) {
		indices.push_back(body[i]);
	}
	for (int i = 0; i < 4; i++) {
		const int next = (i + 1) % 4;
		const int edge[] = { i, i + 4, next + 4, i, next + 4, next };
		for (int j = 0; j < 6; j++) {
			indices.push_back(edge[j]);
		}
	}
	// The coverage vertices retain their subpixel offsets from the snapped shape.
	return _draw_polygon(vertices, Vector<Point2>(), colors, indices, indices.size(), RID(), Transform2D(), Color(1, 1, 1), p_state, false);
}

Error FixedFunctionCanvasItemRenderer::_draw_line(const RasterizerCanvas::Item::CommandLine &p_line, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	if (Math::is_nan(p_line.width) || Math::is_inf(p_line.width)) {
		return ERR_INVALID_PARAMETER;
	}
	Vector2 from = p_line.from;
	Vector2 to = p_line.to;
	Transform2D transform = p_transform;
	real_t width = p_line.width;
	const bool thin = p_line.width <= 1;
	if (thin) {
		// GLES's native thin lines stay one target pixel wide after the canvas transform.
		from = _snap_position(p_transform.xform(from), _pixel_snap_enabled);
		to = _snap_position(p_transform.xform(to), _pixel_snap_enabled);
		const Vector2 direction = to - from;
		const real_t length = direction.length();
		if (Math::is_nan(length) || Math::is_inf(length)) {
			return ERR_INVALID_PARAMETER;
		}
		if (length == 0) {
			return OK;
		}
		width = 1;
		transform = Transform2D();
	}
	// Match GLES2's wide-line quad in local coordinates, including transform scaling.
	const Vector2 tangent = (from - to).normalized().tangent() * width * real_t(0.5);
	Vector<Point2> points;
	points.push_back(from - tangent);
	points.push_back(from + tangent);
	points.push_back(to + tangent);
	points.push_back(to - tangent);
	if (p_line.antialiased && (p_state.blend_mode != FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED || _context.get_device().get_capabilities().blend_mix)) {
		return _draw_antialiased_quad(points, transform, p_line.color * p_modulate, p_state, !thin);
	}
	Vector<Color> colors;
	colors.push_back(p_line.color);
	Vector<int> indices;
	const int quad[] = { 0, 1, 2, 0, 2, 3 };
	for (int i = 0; i < 6; i++) {
		indices.push_back(quad[i]);
	}
	return _draw_polygon(points, Vector<Point2>(), colors, indices, indices.size(), RID(), transform, p_modulate, p_state, !thin);
}

Error FixedFunctionCanvasItemRenderer::_draw_polyline(const RasterizerCanvas::Item::CommandPolyLine &p_line, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	if (p_line.triangles.empty()) {
		return _draw_hairlines(p_line, p_transform, p_modulate, p_state);
	}
	if (p_line.triangles.size() < 3 || p_line.triangles.size() > INT32_MAX / 3 + 2) {
		return ERR_INVALID_PARAMETER;
	}
	// The server supplies a triangle strip with its existing join geometry/colors.
	Vector<int> indices;
	const Error error = indices.resize((p_line.triangles.size() - 2) * 3);
	if (error != OK) {
		return error;
	}
	int *write = indices.ptrw();
	for (int i = 0; i < p_line.triangles.size() - 2; i++) {
		write[i * 3] = i + (i & 1);
		write[i * 3 + 1] = i + ((i & 1) ? 0 : 1);
		write[i * 3 + 2] = i + 2;
	}
	const Error draw_error = _draw_polygon(p_line.triangles, Vector<Point2>(), p_line.triangle_colors, indices, indices.size(), RID(), p_transform, p_modulate, p_state);
	if (draw_error != OK || !p_line.antialiased || p_state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED) {
		return draw_error;
	}
	return _draw_polyline_fringe(p_line, p_transform, p_modulate, p_state);
}

Error FixedFunctionCanvasItemRenderer::_render_items(RasterizerCanvas::Item *p_items, const Color &p_modulate, const Transform2D &p_base_transform) {
	const Error frame_error = _context.get_frame_status();
	if (frame_error != OK) {
		return frame_error;
	}
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = _context.get_device().get_capabilities();
	const int rect_batch_capacity = MIN(caps.max_primitives_per_draw / 2, uint32_t(MAX_RECT_BATCH_QUADS));
	for (RasterizerCanvas::Item *item = p_items; item; item = item->next) {
		if (item->vp_render) {
			return ERR_UNAVAILABLE;
		}
		if (item->copy_back_buffer) {
			WARN_PRINT_ONCE("Fixed Function skips BackBufferCopy and draws the remaining canvas commands.");
		}
		// Mask lights and normal maps affect only the optional lighting pass.
		Vector<Transform2D> bones;
		Transform2D skeleton_transform;
		bool skin = false;
		if (item->skeleton.is_valid()) {
			// Missing or 3D skeletons retain unskinned commands, as in GLES.
			skin = _context.get_mesh_storage().skeleton_get_pose_2d(item->skeleton, bones, skeleton_transform) == OK;
			skeleton_transform = p_base_transform * skeleton_transform;
		}
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		const RID material = item->material_owner ? item->material_owner->material : item->material;
		if (material.is_valid()) {
			const Error error = _context.get_material_storage().material_get_canvas_parameters(material, state);
			if (error != OK && !_context.get_material_storage().owns_material(material)) {
				return error;
			}
			// Unsupported canvas shaders use the ordinary textured/modulated draw instead.
		}
		if ((state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD && !caps.blend_add) ||
				(state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MUL && !caps.blend_mul) ||
				(state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_PREMULTIPLIED_ALPHA && !caps.blend_premultiplied_alpha)) {
			// Preserve sprites and UI whose requested blend is unavailable.
			state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		}
		if (state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MIX && !caps.blend_mix) {
			state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
		}
		const bool clipped = item->final_clip_owner != nullptr;
		if (clipped) {
			const Error error = _get_clip(item->final_clip_owner->final_clip_rect, state.clip_rect);
			if (error != OK) {
				return error;
			}
			state.clip_enabled = true;
		}
		if (!caps.canvas_fill_blit && rect_batch_capacity >= 2 && item->next && item->commands.size() == 1) {
			const RasterizerCanvas::Item *first = item;
			const Error error = _draw_rect_items(item, p_modulate, state, rect_batch_capacity);
			if (error != OK) {
				return error;
			}
			if (item != first) {
				continue;
			}
		}
		_context.get_storage_queries().note_canvas_item();
		const Color modulate = item->final_modulate * p_modulate;
		Transform2D transform = item->final_transform;
		for (int i = 0; i < item->commands.size(); i++) {
			const RasterizerCanvas::Item::Command *command = item->commands[i];
			if (!command) {
				return ERR_INVALID_PARAMETER;
			}
			Error error = OK;
			switch (command->type) {
				case RasterizerCanvas::Item::Command::TYPE_TRANSFORM:
					transform = item->final_transform * static_cast<const RasterizerCanvas::Item::CommandTransform *>(command)->xform;
					break;
				case RasterizerCanvas::Item::Command::TYPE_CLIP_IGNORE:
					state.clip_enabled = clipped && !static_cast<const RasterizerCanvas::Item::CommandClipIgnore *>(command)->ignore;
					break;
				case RasterizerCanvas::Item::Command::TYPE_RECT:
					error = _draw_rect_run(item->commands, i, item->final_transform, transform, modulate, state, item->distance_field);
					break;
				case RasterizerCanvas::Item::Command::TYPE_NINEPATCH:
					error = _draw_nine_patch(*static_cast<const RasterizerCanvas::Item::CommandNinePatch *>(command), transform, modulate, state);
					break;
				case RasterizerCanvas::Item::Command::TYPE_CIRCLE:
					error = _draw_circle(*static_cast<const RasterizerCanvas::Item::CommandCircle *>(command), transform, modulate, state);
					break;
				case RasterizerCanvas::Item::Command::TYPE_LINE:
					error = _draw_line(*static_cast<const RasterizerCanvas::Item::CommandLine *>(command), transform, modulate, state);
					break;
				case RasterizerCanvas::Item::Command::TYPE_POLYLINE:
					error = _draw_polyline(*static_cast<const RasterizerCanvas::Item::CommandPolyLine *>(command), transform, modulate, state);
					break;
				case RasterizerCanvas::Item::Command::TYPE_MESH: {
					const RasterizerCanvas::Item::CommandMesh *mesh = static_cast<const RasterizerCanvas::Item::CommandMesh *>(command);
					error = _draw_mesh(mesh->mesh, mesh->texture, transform * mesh->transform, modulate * mesh->modulate, state);
				} break;
				case RasterizerCanvas::Item::Command::TYPE_MULTIMESH:
					error = _draw_multimesh(*static_cast<const RasterizerCanvas::Item::CommandMultiMesh *>(command), transform, modulate, state);
					break;
				case RasterizerCanvas::Item::Command::TYPE_MULTIRECT:
					error = _draw_multirect(*static_cast<const RasterizerCanvas::Item::CommandMultiRect *>(command), transform, modulate, state, item->distance_field);
					break;
				case RasterizerCanvas::Item::Command::TYPE_PRIMITIVE: {
					const RasterizerCanvas::Item::CommandPrimitive *primitive = static_cast<const RasterizerCanvas::Item::CommandPrimitive *>(command);
					if (primitive->points.size() < 3 || primitive->points.size() > 4) {
						return ERR_UNAVAILABLE;
					}
					Vector<int> indices;
					if (primitive->points.size() == 4) {
						const int quad[] = { 0, 1, 2, 0, 2, 3 };
						for (int j = 0; j < 6; j++) {
							indices.push_back(quad[j]);
						}
					}
					error = _draw_polygon(primitive->points, primitive->uvs, primitive->colors, indices, primitive->points.size() == 4 ? 6 : 3, primitive->texture, transform, modulate, state);
				} break;
				case RasterizerCanvas::Item::Command::TYPE_POLYGON: {
					const RasterizerCanvas::Item::CommandPolygon *polygon = static_cast<const RasterizerCanvas::Item::CommandPolygon *>(command);
					// The indexed fill remains drawable with aliased edges when antialiasing is requested.
					if (skin && !polygon->weights.empty()) {
						error = _skin_polygon(polygon->points, polygon->bones, polygon->weights, bones, transform, skeleton_transform, _skinned_points, _pixel_snap_enabled);
						if (error == OK) {
							error = _draw_polygon(_skinned_points, polygon->uvs, polygon->colors, polygon->indices, polygon->count, polygon->texture, Transform2D(), modulate, state, false);
						}
					} else {
						error = _draw_polygon(polygon->points, polygon->uvs, polygon->colors, polygon->indices, polygon->count, polygon->texture, transform, modulate, state);
					}
				} break;
				case RasterizerCanvas::Item::Command::TYPE_PARTICLES:
					// GPU particles are unavailable. Keep the rest of the canvas drawable.
					break;
				default:
					return ERR_UNAVAILABLE;
			}
			if (error != OK) {
				return error;
			}
		}
	}
	return OK;
}

Error FixedFunctionCanvasItemRenderer::render_items(RasterizerCanvas::Item *p_items, const Color &p_modulate, const Transform2D &p_base_transform) {
	_redraw_requested = false;
	const Error error = _render_items(p_items, p_modulate, p_base_transform);
	return error == OK ? OK : _render_targets.report_draw_error(error);
}

FixedFunctionCanvasItemRenderer::FixedFunctionCanvasItemRenderer(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets) :
		_context(p_context),
		_render_targets(p_render_targets),
		_pixel_snap_enabled(GLOBAL_DEF("rendering/2d/snapping/use_gpu_pixel_snap", false)),
		_redraw_requested(false) {
}
