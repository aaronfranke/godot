#include "canvas_item_renderer.h"

#include "core/math/math_funcs.h"

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

Error FixedFunctionCanvasItemRenderer::_submit(const FixedFunctionVertex *p_vertices, int p_count, RID p_texture, FixedFunctionGraphicsDevice::FFDrawParameters p_state, const Rect2 *p_glyph_uv, bool p_msdf, uint8_t p_msdf_outline) {
	FixedFunctionGraphicsDevice &device = _context.get_device();
	Error error = _render_targets.begin_draw();
	if (error != OK) {
		return error;
	}
	if (!p_texture.is_valid()) {
		error = device.fill_canvas_triangles(p_vertices, p_count, p_state);
		if (error != ERR_UNAVAILABLE) {
			if (error == OK) {
				_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
				_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += p_count / 3;
			}
			return error;
		}
	}
	FixedFunctionTextureStorage &storage = _context.get_texture_storage();
	RID native;
	RID target;
	if (p_texture.is_valid()) {
		const bool linear = p_state.sampler_flags & FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
		RSE::CanvasItemTextureFilter filter = linear ? RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR : RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST;
		if (p_state.sampler_flags & FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS) {
			filter = linear ? RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS : RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS;
		}
		if (p_state.sampler_flags & FixedFunctionGraphicsDevice::SAMPLER_FILTER_ANISOTROPIC) {
			filter = linear ? RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC : RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC;
		}
		RSE::CanvasItemTextureRepeat repeat = RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED;
		if (p_state.sampler_flags & FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED) {
			repeat = RSE::CANVAS_ITEM_TEXTURE_REPEAT_MIRROR;
		} else if (p_state.sampler_flags & (FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V)) {
			repeat = RSE::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED;
		}
		error = storage.canvas_texture_resolve(p_texture, filter, repeat);
		if (error == OK) {
			error = _set_sampler(p_state, filter, repeat);
		}
		if (error != OK) {
			return p_state.font_glyph ? OK : error;
		}
		if (p_state.font_glyph && !p_texture.is_valid()) {
			return OK;
		}
	}
	if (p_msdf) {
		native = storage.texture_get_msdf_coverage(p_texture, p_msdf_outline);
		if (!native.is_valid()) {
			return OK;
		}
	} else if (p_texture.is_valid()) {
		_redraw_requested |= storage.texture_get_force_redraw_if_visible(p_texture);
		target = storage.texture_get_render_target(p_texture);
		if (target.is_valid()) {
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
			return p_state.font_glyph ? OK : error;
		}
		p_state.sampler_flags |= storage.texture_get_sampler_flags(p_texture);
	}
	if (target.is_valid() && storage.render_target_get_flag(target, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP)) {
		// Godot 4 samples viewport textures top-left; only an explicit flip reverses V.
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
		Projection projection;
		projection.columns[0][0] = real_t(2) / device.get_target_width();
		projection.columns[1][1] = real_t(-2) / device.get_target_height();
		projection.columns[3][0] = -1;
		projection.columns[3][1] = 1;
		error = device.draw_triangles(p_vertices, p_count, projection, native, p_state);
	}
	if (p_state.font_glyph && (error == ERR_UNAVAILABLE || error == ERR_OUT_OF_MEMORY || error == ERR_CANT_CREATE)) {
		return OK;
	}
	if (error == OK) {
		_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
		_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += p_count / 3;
	}
	return error;
}

bool FixedFunctionCanvasItemRenderer::_get_msdf_outline(const RendererCanvasRender::Item::CommandRect &p_rect, uint8_t &r_outline) {
	const float range = p_rect.px_range;
	const float outline = p_rect.outline;
	if (!Math::is_finite(range) || range <= 0 || !Math::is_finite(outline) || outline < 0) {
		return false;
	}
	r_outline = 0;
	if (outline > 0) {
		if (range <= 2) {
			return false;
		}
		const float width = MIN(outline, range * 0.5f - 1.0f) / range;
		r_outline = MAX(1, int(Math::round(width * 255.0f)));
	}
	return true;
}

bool FixedFunctionCanvasItemRenderer::_set_msdf_state(FixedFunctionGraphicsDevice::FFDrawParameters &r_state) const {
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

Error FixedFunctionCanvasItemRenderer::_draw_rect(const RendererCanvasRender::Item::CommandRect &p_rect, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	const bool msdf = p_rect.flags & RendererCanvasRender::CANVAS_RECT_MSDF;
	const bool font_glyph = msdf || (p_rect.flags & RendererCanvasRender::CANVAS_RECT_FONT_GLYPH);
	// LCD needs channel-specific blending.
	if (p_rect.flags & RendererCanvasRender::CANVAS_RECT_LCD) {
		return OK;
	}
	uint8_t msdf_outline = 0;
	if (msdf && !_get_msdf_outline(p_rect, msdf_outline)) {
		return OK;
	}
	if (p_rect.flags & ~(RendererCanvasRender::CANVAS_RECT_REGION | RendererCanvasRender::CANVAS_RECT_TILE | RendererCanvasRender::CANVAS_RECT_FLIP_H | RendererCanvasRender::CANVAS_RECT_FLIP_V | RendererCanvasRender::CANVAS_RECT_TRANSPOSE | RendererCanvasRender::CANVAS_RECT_CLIP_UV | RendererCanvasRender::CANVAS_RECT_FONT_GLYPH | RendererCanvasRender::CANVAS_RECT_MSDF)) {
		return font_glyph ? OK : ERR_UNAVAILABLE;
	}
	if (p_rect.rect.size.x < 0 || p_rect.rect.size.y < 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_rect.rect.size.x == 0 || p_rect.rect.size.y == 0) {
		return OK;
	}
	if (font_glyph && !p_rect.texture.is_valid()) {
		return OK;
	}
	Rect2 source(0, 0, 1, 1);
	if (p_rect.texture.is_valid()) {
		FixedFunctionTextureStorage &storage = _context.get_texture_storage();
		if (!storage.owns_texture(p_rect.texture) && !storage.owns_canvas_texture(p_rect.texture)) {
			return font_glyph ? OK : ERR_INVALID_PARAMETER;
		}
		const Size2i size = storage.texture_size_with_proxy(p_rect.texture);
		if (size.x <= 0 || size.y <= 0) {
			if (font_glyph) {
				return OK;
			}
			RID texture = p_rect.texture;
			RSE::CanvasItemTextureFilter filter = RSE::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
			RSE::CanvasItemTextureRepeat repeat = RSE::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;
			const Error error = storage.canvas_texture_resolve(texture, filter, repeat);
			if (error != OK) {
				return error;
			}
			return texture.is_valid() && storage.texture_get_render_target(texture).is_valid() ? ERR_UNCONFIGURED : OK;
		}
		if (p_rect.flags & RendererCanvasRender::CANVAS_RECT_REGION) {
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
	if ((p_rect.flags & RendererCanvasRender::CANVAS_RECT_TILE) && _context.get_device().get_capabilities().sampler_repeat) {
		state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V;
	}
	state.font_glyph = font_glyph;
	if (msdf && !_set_msdf_state(state)) {
		return OK;
	}
	const int glyph_flag = msdf ? RendererCanvasRender::CANVAS_RECT_MSDF : RendererCanvasRender::CANVAS_RECT_FONT_GLYPH;
	const bool glyph = p_rect.flags == (RendererCanvasRender::CANVAS_RECT_REGION | glyph_flag) &&
			p_transform[0] == Vector2(1, 0) && p_transform[1] == Vector2(0, 1);
	return _submit(vertices, 6, p_rect.texture, state, glyph ? &source : nullptr, msdf, msdf_outline);
}

Error FixedFunctionCanvasItemRenderer::_draw_rect_run(const RendererCanvasRender::Item::Command *&r_command, const Transform2D &p_item_transform, Transform2D &r_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, int &r_drawn_rects) {
	const RendererCanvasRender::Item::CommandRect &first = *static_cast<const RendererCanvasRender::Item::CommandRect *>(r_command);
	const bool textured = first.texture.is_valid();
	const uint16_t glyph_mask = RendererCanvasRender::CANVAS_RECT_FONT_GLYPH | RendererCanvasRender::CANVAS_RECT_MSDF;
	const uint16_t glyph_flags = first.flags & glyph_mask;
	const uint16_t flags = textured ? (RECT_BATCH_TEXTURE_FLAGS | glyph_flags) : 0;
	const FixedFunctionGraphicsDevice &device = _context.get_device();
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = device.get_capabilities();
	const int capacity = MIN(caps.max_primitives_per_draw / 2, uint32_t(MAX_RECT_BATCH_QUADS));
	// Tiled draws and native fill and glyph blits use individual submissions.
	if (capacity < 2 || !first.next || (!glyph_flags && first.next->type != RendererCanvasRender::Item::Command::TYPE_RECT) ||
			(!textured && caps.canvas_fill_blit) || (first.flags & ~flags) || (glyph_flags && device.prefers_canvas_glyph_blits())) {
		return _draw_rect(first, r_transform, p_modulate, p_state);
	}
	const bool msdf = glyph_flags & RendererCanvasRender::CANVAS_RECT_MSDF;
	uint8_t msdf_outline = 0;
	if (msdf && !_get_msdf_outline(first, msdf_outline)) {
		return _draw_rect(first, r_transform, p_modulate, p_state);
	}
	const RendererCanvasRender::Item::Command *last = &first;
	int count = 1;
	// A glyph's transform can be packed into its vertices without changing draw state.
	for (const RendererCanvasRender::Item::Command *command = first.next; command && count < capacity; command = command->next) {
		if (glyph_flags && command->type == RendererCanvasRender::Item::Command::TYPE_TRANSFORM) {
			continue;
		}
		if (command->type != RendererCanvasRender::Item::Command::TYPE_RECT) {
			break;
		}
		const RendererCanvasRender::Item::CommandRect &next = *static_cast<const RendererCanvasRender::Item::CommandRect *>(command);
		if (next.texture != first.texture || (next.flags & ~flags) || (next.flags & glyph_mask) != glyph_flags) {
			break;
		}
		uint8_t next_outline = 0;
		if (msdf && (!_get_msdf_outline(next, next_outline) || next_outline != msdf_outline)) {
			break;
		}
		last = &next;
		count++;
	}
	if (count == 1) {
		return _draw_rect(first, r_transform, p_modulate, p_state);
	}
	Size2i size;
	if (!_get_rect_batch_texture_size(first.texture, size)) {
		return _draw_rect(first, r_transform, p_modulate, p_state);
	}
	Error error = _vertices.resize(count * 6);
	if (error != OK) {
		return glyph_flags ? _draw_rect(first, r_transform, p_modulate, p_state) : error;
	}
	FixedFunctionVertex *vertices = _vertices.ptrw();
	int packed = 0;
	Transform2D transform = r_transform;
	for (const RendererCanvasRender::Item::Command *command = &first; command != last->next; command = command->next) {
		if (command->type == RendererCanvasRender::Item::Command::TYPE_TRANSFORM) {
			transform = p_item_transform * static_cast<const RendererCanvasRender::Item::CommandTransform *>(command)->xform;
			continue;
		}
		const RendererCanvasRender::Item::CommandRect &rect = *static_cast<const RendererCanvasRender::Item::CommandRect *>(command);
		error = _pack_rect_batch_command(rect, size, transform, p_modulate, vertices, packed);
		if (error != OK) {
			return error;
		}
	}
	r_command = last;
	r_transform = transform;
	r_drawn_rects = packed;
	FixedFunctionGraphicsDevice::FFDrawParameters state = p_state;
	state.font_glyph = glyph_flags != 0;
	if (!packed || (msdf && !_set_msdf_state(state))) {
		return OK;
	}
	return _submit(vertices, packed * 6, first.texture, state, nullptr, msdf, msdf_outline);
}

const RendererCanvasRender::Item::CommandRect *FixedFunctionCanvasItemRenderer::_get_batchable_item_rect(const RendererCanvasRender::Item &p_item) {
	if (!p_item.commands || p_item.commands->next || p_item.commands->type != RendererCanvasRender::Item::Command::TYPE_RECT) {
		return nullptr;
	}
	const auto *rect = static_cast<const RendererCanvasRender::Item::CommandRect *>(p_item.commands);
	const uint16_t flags = rect->texture.is_valid() ? RECT_BATCH_TEXTURE_FLAGS : 0;
	if ((rect->flags & ~flags) || p_item.material.is_valid() || p_item.material_owner || p_item.final_clip_owner ||
			p_item.copy_back_buffer || p_item.vp_render || p_item.canvas_group_owner || p_item.use_canvas_group || p_item.skeleton.is_valid() ||
			p_item.repeat_source_item || p_item.texture_filter != RSE::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT || p_item.texture_repeat != RSE::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT) {
		return nullptr;
	}
	return rect;
}

Error FixedFunctionCanvasItemRenderer::_draw_rect_items(RendererCanvasRender::Item *&r_item, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, int p_capacity) {
	const RendererCanvasRender::Item::CommandRect *first = _get_batchable_item_rect(*r_item);
	if (!first) {
		return OK;
	}
	RendererCanvasRender::Item *last = r_item;
	int count = 1;
	for (RendererCanvasRender::Item *item = r_item->next; item && count < p_capacity; item = item->next) {
		const RendererCanvasRender::Item::CommandRect *rect = _get_batchable_item_rect(*item);
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
	for (const RendererCanvasRender::Item *item = r_item; item != last->next; item = item->next) {
		const auto &rect = *static_cast<const RendererCanvasRender::Item::CommandRect *>(item->commands);
		error = _pack_rect_batch_command(rect, size, item->final_transform, item->final_modulate * p_modulate, vertices, packed);
		if (error != OK) {
			return error;
		}
	}
	r_item = last;
	if (!packed) {
		return OK;
	}
	auto &info = _render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS];
	const int previous_draws = info[RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME];
	error = _submit(vertices, packed * 6, first->texture, p_state);
	if (info[RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] != previous_draws) {
		info[RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] += packed;
	}
	return error;
}

bool FixedFunctionCanvasItemRenderer::_get_rect_batch_texture_size(RID p_texture, Size2i &r_size) const {
	if (!p_texture.is_valid()) {
		return true;
	}
	const FixedFunctionTextureStorage &storage = _context.get_texture_storage();
	RSE::CanvasItemTextureFilter filter = RSE::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
	RSE::CanvasItemTextureRepeat repeat = RSE::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;
	// Check a CanvasTexture's diffuse source while keeping the wrapper for submission.
	// Viewport textures retain their individual copy and update paths.
	if (storage.canvas_texture_resolve(p_texture, filter, repeat) != OK || !p_texture.is_valid() || storage.texture_get_render_target(p_texture).is_valid()) {
		return false;
	}
	r_size = storage.texture_size_with_proxy(p_texture);
	return r_size.x > 0 && r_size.y > 0;
}

Error FixedFunctionCanvasItemRenderer::_pack_rect_batch_command(const RendererCanvasRender::Item::CommandRect &p_rect, const Size2i &p_texture_size, const Transform2D &p_transform, const Color &p_modulate, FixedFunctionVertex *r_vertices, int &r_packed) const {
	if (p_rect.rect.size.x < 0 || p_rect.rect.size.y < 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (!p_rect.rect.size.x || !p_rect.rect.size.y) {
		return OK;
	}
	Rect2 source(0, 0, 1, 1);
	if (p_rect.flags & RendererCanvasRender::CANVAS_RECT_REGION) {
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

Error FixedFunctionCanvasItemRenderer::_pack_rect(const Rect2 &p_rect, const Rect2 &p_source, uint16_t p_flags, const Transform2D &p_transform, const Color &p_color, FixedFunctionVertex *r_vertices) const {
	Vector2 uv[] = { p_source.position, p_source.position + Vector2(p_source.size.x, 0), p_source.position + p_source.size, p_source.position + Vector2(0, p_source.size.y) };
	if (p_flags & RendererCanvasRender::CANVAS_RECT_TRANSPOSE) {
		SWAP(uv[1], uv[3]);
	}
	if (p_flags & RendererCanvasRender::CANVAS_RECT_FLIP_H) {
		SWAP(uv[0], uv[1]);
		SWAP(uv[2], uv[3]);
	}
	if (p_flags & RendererCanvasRender::CANVAS_RECT_FLIP_V) {
		SWAP(uv[0], uv[3]);
		SWAP(uv[1], uv[2]);
	}
	// Affine interpolation already stays within the source region (CLIP_UV).
	const Vector2 points[] = { p_rect.position, p_rect.position + Vector2(p_rect.size.x, 0), p_rect.position + p_rect.size, p_rect.position + Vector2(0, p_rect.size.y) };
	const int corners[] = { 0, 1, 2, 5 };
	for (int i = 0; i < 4; i++) {
		const Error error = _pack_vertex(r_vertices[corners[i]], _snap_position(p_transform.xform(points[i])), uv[i], p_color);
		if (error != OK) {
			return error;
		}
	}
	r_vertices[3] = r_vertices[0];
	r_vertices[4] = r_vertices[2];
	return OK;
}

Error FixedFunctionCanvasItemRenderer::_draw_polygon(const Vector<Point2> &p_points, const Vector<Point2> &p_uvs, const Vector<Color> &p_colors, const Vector<int> &p_indices, int p_count, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, bool p_snap_vertices) {
	if (p_count < 0 || p_count % 3 || p_count > (p_indices.is_empty() ? p_points.size() : p_indices.size()) || (!p_uvs.is_empty() && p_uvs.size() != p_points.size()) || (!p_colors.is_empty() && p_colors.size() != 1 && p_colors.size() != p_points.size())) {
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
	for (int offset = 0; offset < p_count;) {
		const int count = MIN(p_count - offset, batch_size);
		FixedFunctionVertex *vertices = _vertices.ptrw();
		for (int i = 0; i < count; i++) {
			const int index = p_indices.is_empty() ? offset + i : p_indices[offset + i];
			if (index < 0 || index >= p_points.size()) {
				return ERR_INVALID_PARAMETER;
			}
			Color color = p_modulate;
			if (!p_colors.is_empty()) {
				color = p_colors[p_colors.size() == 1 ? 0 : index] * p_modulate;
			}
			Vector2 position = p_transform.xform(p_points[index]);
			if (p_snap_vertices) {
				position = _snap_position(position);
			}
			error = _pack_vertex(vertices[i], position, p_uvs.is_empty() ? Vector2() : p_uvs[index], color);
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

Error FixedFunctionCanvasItemRenderer::_draw_nine_patch(const RendererCanvasRender::Item::CommandNinePatch &p_patch, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	if (p_patch.axis_x < RSE::NINE_PATCH_STRETCH || p_patch.axis_x > RSE::NINE_PATCH_TILE_FIT || p_patch.axis_y < RSE::NINE_PATCH_STRETCH || p_patch.axis_y > RSE::NINE_PATCH_TILE_FIT) {
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
	// 4.x nine-patch margins remain in destination pixels.
	const real_t screen_scale = 1;
	if (p_patch.axis_x != RSE::NINE_PATCH_STRETCH || p_patch.axis_y != RSE::NINE_PATCH_STRETCH) {
		return _draw_tiled_nine_patch(p_patch, source, size, screen_scale, p_transform, p_modulate, p_state);
	}
	const real_t left = p_patch.margin[SIDE_LEFT];
	const real_t right = p_patch.margin[SIDE_RIGHT];
	const real_t top = p_patch.margin[SIDE_TOP];
	const real_t bottom = p_patch.margin[SIDE_BOTTOM];
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

Error FixedFunctionCanvasItemRenderer::_prepare_nine_patch_axis(FFNinePatchAxis &r_axis, real_t p_destination_size, real_t p_source_size, real_t p_begin, real_t p_end, real_t p_scale, RSE::NinePatchAxisMode p_mode, bool p_middle_needed) {
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
	if (r_axis.tiles[1] && p_mode != RSE::NINE_PATCH_STRETCH) {
		if (source <= 0) {
			return ERR_INVALID_PARAMETER;
		}
		// Widen the count calculation before converting; very small tiles can overflow real_t.
		const double divisor = p_mode == RSE::NINE_PATCH_TILE_FIT ? MAX(double(source), double(real_t(0.0000001))) : double(source);
		const double ratio = double(destination) / divisor;
		const double count = p_mode == RSE::NINE_PATCH_TILE ? MAX(1.0, Math::ceil(ratio)) : MAX(1.0, Math::floor(ratio + 0.5));
		if (count > double(MAX_NINE_PATCH_QUADS)) {
			return ERR_UNAVAILABLE;
		}
		r_axis.tiles[1] = int(count);
		r_axis.tile_size = p_mode == RSE::NINE_PATCH_TILE ? source : destination / r_axis.tiles[1];
	}
	return OK;
}

Error FixedFunctionCanvasItemRenderer::_draw_tiled_nine_patch(const RendererCanvasRender::Item::CommandNinePatch &p_patch, const Rect2 &p_source, const Size2i &p_texture_size, real_t p_scale, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	FFNinePatchAxis horizontal;
	FFNinePatchAxis vertical;
	const bool middle_x = p_patch.draw_center || p_patch.margin[SIDE_TOP] * p_scale > 0 || p_patch.margin[SIDE_BOTTOM] * p_scale > 0;
	const bool middle_y = p_patch.draw_center || p_patch.margin[SIDE_LEFT] * p_scale > 0 || p_patch.margin[SIDE_RIGHT] * p_scale > 0;
	Error error = _prepare_nine_patch_axis(horizontal, p_patch.rect.size.x, p_source.size.x, p_patch.margin[SIDE_LEFT], p_patch.margin[SIDE_RIGHT], p_scale, p_patch.axis_x, middle_x);
	if (error != OK) {
		return error;
	}
	error = _prepare_nine_patch_axis(vertical, p_patch.rect.size.y, p_source.size.y, p_patch.margin[SIDE_TOP], p_patch.margin[SIDE_BOTTOM], p_scale, p_patch.axis_y, middle_y);
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
				const real_t v1 = row == 1 && p_patch.axis_y == RSE::NINE_PATCH_TILE ? vertical.source[row] + (y1 - y0) : vertical.source[row + 1];
				for (int x = 0; x < horizontal.tiles[column]; x++) {
					const real_t x0 = horizontal.destination[column] + step_x * x;
					const real_t x1 = MIN(horizontal.destination[column + 1], horizontal.destination[column] + step_x * (x + 1));
					const real_t u1 = column == 1 && p_patch.axis_x == RSE::NINE_PATCH_TILE ? horizontal.source[column] + (x1 - x0) : horizontal.source[column + 1];
					const Vector2 positions[] = { Vector2(x0, y0), Vector2(x1, y0), Vector2(x1, y1), Vector2(x0, y1) };
					const Vector2 uvs[] = { Vector2(horizontal.source[column], vertical.source[row]), Vector2(u1, vertical.source[row]), Vector2(u1, v1), Vector2(horizontal.source[column], v1) };
					for (int i = 0; i < 6; i++) {
						const int index = indices[i];
						error = _pack_vertex(vertices[count], _snap_position(p_transform.xform(p_patch.rect.position + positions[index])), (p_source.position + uvs[index]) / Size2(p_texture_size.x, p_texture_size.y), color);
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

Error FixedFunctionCanvasItemRenderer::_draw_thin_line(const Vector2 &p_from, const Vector2 &p_to, const Vector2 &p_uv_from, const Vector2 &p_uv_to, const Color &p_color_from, const Color &p_color_to, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	const Vector2 from = _snap_position(p_transform.xform(p_from));
	const Vector2 to = _snap_position(p_transform.xform(p_to));
	const Vector2 direction = to - from;
	const real_t length = direction.length();
	if (Math::is_nan(length) || Math::is_inf(length)) {
		return ERR_INVALID_PARAMETER;
	}
	if (length == 0) {
		return OK;
	}
	// Negative-width server lines remain one logical target pixel after canvas transforms.
	const Vector2 side = direction.orthogonal() * (real_t(0.5) / length);
	Vector<Point2> points = { from - side, from + side, to + side, to - side };
	Vector<Point2> uvs = { p_uv_from, p_uv_from, p_uv_to, p_uv_to };
	Vector<Color> colors = { p_color_from, p_color_from, p_color_to, p_color_to };
	Vector<int> indices = { 0, 1, 2, 0, 2, 3 };
	return _draw_polygon(points, uvs, colors, indices, 6, p_texture, Transform2D(), p_modulate, p_state, false);
}

Error FixedFunctionCanvasItemRenderer::polygon_create(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, int p_count, uint64_t &r_polygon, const Vector<int> &p_bones, const Vector<float> &p_weights) {
	if (p_points.is_empty() || (!p_uvs.is_empty() && p_uvs.size() != p_points.size()) || (!p_colors.is_empty() && p_colors.size() != 1 && p_colors.size() != p_points.size()) || p_count < -1) {
		return ERR_INVALID_PARAMETER;
	}
	int count = p_points.size();
	if (!p_indices.is_empty()) {
		count = p_count < 0 ? p_indices.size() : p_count;
	}
	if (count > (p_indices.is_empty() ? p_points.size() : p_indices.size())) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < count && !p_indices.is_empty(); i++) {
		if (p_indices[i] < 0 || p_indices[i] >= p_points.size()) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (_next_polygon == UINT64_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	FFCanvasPolygon polygon;
	polygon.points = p_points;
	polygon.uvs = p_uvs;
	polygon.colors = p_colors;
	polygon.indices = p_indices;
	polygon.bones = p_bones;
	polygon.weights = p_weights;
	polygon.count = count;
	const uint64_t id = ++_next_polygon;
	_polygons.insert(id, polygon);
	r_polygon = id;
	return OK;
}

Error FixedFunctionCanvasItemRenderer::polygon_free(uint64_t p_polygon) {
	return _polygons.erase(p_polygon) ? OK : ERR_INVALID_PARAMETER;
}

Error FixedFunctionCanvasItemRenderer::_draw_stored_polygon(const RendererCanvasRender::Item::CommandPolygon &p_command, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, const Vector<Transform2D> &p_bones, const Transform2D &p_skeleton_transform) {
	const FFCanvasPolygon *polygon = _polygons.getptr(p_command.polygon.polygon_id);
	if (!polygon) {
		return ERR_INVALID_PARAMETER;
	}
	const Vector<Point2> *points = &polygon->points;
	Transform2D transform = p_transform;
	if (!polygon->weights.is_empty() && !p_bones.is_empty()) {
		const Error error = _skin_polygon(polygon->points, polygon->bones, polygon->weights, p_bones, p_transform, p_skeleton_transform, _skinned_points);
		if (error != OK) {
			return error;
		}
		points = &_skinned_points;
		transform = Transform2D();
	}
	if (p_command.primitive == RSE::PRIMITIVE_TRIANGLES) {
		return _draw_polygon(*points, polygon->uvs, polygon->colors, polygon->indices, polygon->count, p_command.texture, transform, p_modulate, p_state);
	}
	if (p_command.primitive == RSE::PRIMITIVE_LINES || p_command.primitive == RSE::PRIMITIVE_LINE_STRIP) {
		FFCanvasPolygon lines = *polygon;
		lines.points = *points;
		return _draw_hairlines(lines, p_command.primitive == RSE::PRIMITIVE_LINE_STRIP, p_command.texture, transform, p_modulate, p_state);
	}
	if (p_command.primitive != RSE::PRIMITIVE_TRIANGLE_STRIP) {
		return ERR_UNAVAILABLE;
	}
	if (polygon->count < 3 || polygon->count > INT32_MAX / 3 + 2) {
		return ERR_INVALID_PARAMETER;
	}
	Vector<int> indices;
	const Error error = indices.resize((polygon->count - 2) * 3);
	if (error != OK) {
		return error;
	}
	int *write = indices.ptrw();
	for (int i = 0; i < polygon->count - 2; i++) {
		const int triangle[] = { i + (i & 1), i + ((i & 1) ? 0 : 1), i + 2 };
		for (int j = 0; j < 3; j++) {
			write[i * 3 + j] = polygon->indices.is_empty() ? triangle[j] : polygon->indices[triangle[j]];
		}
	}
	return _draw_polygon(*points, polygon->uvs, polygon->colors, indices, indices.size(), p_command.texture, transform, p_modulate, p_state);
}

Error FixedFunctionCanvasItemRenderer::_set_sampler(FixedFunctionGraphicsDevice::FFDrawParameters &r_state, RSE::CanvasItemTextureFilter p_filter, RSE::CanvasItemTextureRepeat p_repeat) const {
	if (p_filter <= RSE::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT || p_filter >= RSE::CANVAS_ITEM_TEXTURE_FILTER_MAX ||
			p_repeat <= RSE::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT || p_repeat >= RSE::CANVAS_ITEM_TEXTURE_REPEAT_MAX) {
		return ERR_UNAVAILABLE;
	}
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = _context.get_device().get_capabilities();
	const bool linear = p_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR || p_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS || p_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC;
	r_state.sampler_flags = 0;
	if (p_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC || p_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC) {
		r_state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_ANISOTROPIC;
	}
	if (p_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS || p_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS ||
			p_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC || p_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC) {
		r_state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
	}
	if (linear && caps.sampler_filter_linear) {
		r_state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
	}
	if (p_repeat == RSE::CANVAS_ITEM_TEXTURE_REPEAT_MIRROR && caps.sampler_repeat_mirrored) {
		r_state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
	} else if (p_repeat == RSE::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED || (p_repeat == RSE::CANVAS_ITEM_TEXTURE_REPEAT_MIRROR && caps.sampler_repeat)) {
		r_state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V;
	}
	return OK;
}

Error FixedFunctionCanvasItemRenderer::_render_item_commands(const RendererCanvasRender::Item &p_item, const Transform2D &p_transform, const Color &p_modulate, FixedFunctionGraphicsDevice::FFDrawParameters p_state, const Vector<Transform2D> &p_bones, const Transform2D &p_skeleton_transform, const Transform2D &p_canvas_transform) {
	const bool clipped = p_state.clip_enabled;
	Transform2D transform = p_transform;
	for (const RendererCanvasRender::Item::Command *command = p_item.commands; command; command = command->next) {
		const int previous_draws = _render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME];
		Error error = OK;
		int drawn_commands = 1;
		switch (command->type) {
			case RendererCanvasRender::Item::Command::TYPE_TRANSFORM:
				transform = p_transform * static_cast<const RendererCanvasRender::Item::CommandTransform *>(command)->xform;
				break;
			case RendererCanvasRender::Item::Command::TYPE_CLIP_IGNORE:
				p_state.clip_enabled = clipped && !static_cast<const RendererCanvasRender::Item::CommandClipIgnore *>(command)->ignore;
				break;
			case RendererCanvasRender::Item::Command::TYPE_RECT:
				error = _draw_rect_run(command, p_transform, transform, p_modulate, p_state, drawn_commands);
				break;
			case RendererCanvasRender::Item::Command::TYPE_NINEPATCH:
				error = _draw_nine_patch(*static_cast<const RendererCanvasRender::Item::CommandNinePatch *>(command), transform, p_modulate, p_state);
				break;
			case RendererCanvasRender::Item::Command::TYPE_PRIMITIVE: {
				const RendererCanvasRender::Item::CommandPrimitive *primitive = static_cast<const RendererCanvasRender::Item::CommandPrimitive *>(command);
				if (primitive->point_count == 2) {
					error = _draw_thin_line(primitive->points[0], primitive->points[1], primitive->uvs[0], primitive->uvs[1], primitive->colors[0], primitive->colors[1], primitive->texture, transform, p_modulate, p_state);
					break;
				}
				if (primitive->point_count < 3 || primitive->point_count > 4) {
					return ERR_UNAVAILABLE;
				}
				Vector<Point2> points;
				Vector<Point2> uvs;
				Vector<Color> colors;
				for (uint32_t i = 0; i < primitive->point_count; i++) {
					points.push_back(primitive->points[i]);
					uvs.push_back(primitive->uvs[i]);
					colors.push_back(primitive->colors[i]);
				}
				Vector<int> indices;
				if (primitive->point_count == 4) {
					const int quad[] = { 0, 1, 2, 0, 2, 3 };
					for (int j = 0; j < 6; j++) {
						indices.push_back(quad[j]);
					}
				}
				error = _draw_polygon(points, uvs, colors, indices, primitive->point_count == 4 ? 6 : 3, primitive->texture, transform, p_modulate, p_state);
			} break;
			case RendererCanvasRender::Item::Command::TYPE_POLYGON:
				error = _draw_stored_polygon(*static_cast<const RendererCanvasRender::Item::CommandPolygon *>(command), transform, p_modulate, p_state, p_bones, p_skeleton_transform);
				break;
			case RendererCanvasRender::Item::Command::TYPE_MESH: {
				const RendererCanvasRender::Item::CommandMesh *mesh = static_cast<const RendererCanvasRender::Item::CommandMesh *>(command);
				error = _draw_mesh(mesh->mesh, RID(), mesh->texture, transform * mesh->transform, p_modulate * mesh->modulate, p_state, mesh->mesh_instance, p_canvas_transform);
			} break;
			case RendererCanvasRender::Item::Command::TYPE_MULTIMESH: {
				const RendererCanvasRender::Item::CommandMultiMesh *multimesh = static_cast<const RendererCanvasRender::Item::CommandMultiMesh *>(command);
				const RID mesh = _context.get_mesh_storage().multimesh_get_mesh(multimesh->multimesh);
				error = _draw_mesh(mesh, multimesh->multimesh, multimesh->texture, transform, p_modulate, p_state);
			} break;
			case RendererCanvasRender::Item::Command::TYPE_PARTICLES:
				// GPU particles are unavailable. Keep the rest of the canvas drawable.
				break;
			default:
				return ERR_UNAVAILABLE;
		}
		// Match 4.x's draw-command object unit, including individual glyph rectangles.
		if (_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] != previous_draws) {
			_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] += drawn_commands;
		}
		if (error != OK) {
			return error;
		}
	}
	return OK;
}

Error FixedFunctionCanvasItemRenderer::_render_items(RendererCanvasRender::Item *p_items, const Color &p_modulate, RSE::CanvasItemTextureFilter p_filter, RSE::CanvasItemTextureRepeat p_repeat, const Transform2D &p_canvas_transform) {
	const Error frame_error = _context.get_frame_status();
	if (frame_error != OK) {
		return frame_error;
	}
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = _context.get_device().get_capabilities();
	const int rect_batch_capacity = MIN(caps.max_primitives_per_draw / 2, uint32_t(MAX_RECT_BATCH_QUADS));
	for (RendererCanvasRender::Item *item = p_items; item; item = item->next) {
		if (item->vp_render || item->canvas_group_owner || item->use_canvas_group) {
			return ERR_UNAVAILABLE;
		}
		if (item->copy_back_buffer) {
			WARN_PRINT_ONCE("Fixed Function skips BackBufferCopy and draws the remaining canvas commands.");
		}
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		const RID material = item->material_owner ? item->material_owner->material : item->material;
		if (material.is_valid()) {
			const Error error = _context.get_material_storage().material_get_canvas_parameters(material, state);
			if (error != OK) {
				state = FixedFunctionGraphicsDevice::FFDrawParameters();
				state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
			}
		}
		const RSE::CanvasItemTextureFilter filter = item->texture_filter == RSE::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT ? p_filter : item->texture_filter;
		const RSE::CanvasItemTextureRepeat repeat = item->texture_repeat == RSE::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT ? p_repeat : item->texture_repeat;
		const Error sampler_error = _set_sampler(state, filter, repeat);
		if (sampler_error != OK) {
			return sampler_error;
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
		if (!caps.canvas_fill_blit && rect_batch_capacity >= 2 && item->next && item->commands && !item->commands->next) {
			RendererCanvasRender::Item *last = item;
			const Error error = _draw_rect_items(last, p_modulate, state, rect_batch_capacity);
			if (error != OK) {
				return error;
			}
			if (last != item) {
				item = last;
				continue;
			}
		}
		Vector<Transform2D> bones;
		Transform2D skeleton_transform;
		if (item->skeleton.is_valid() && _context.get_mesh_storage().skeleton_get_pose_2d(item->skeleton, bones, skeleton_transform) == OK) {
			skeleton_transform = p_canvas_transform * skeleton_transform;
		}
		if (!item->commands) {
			continue;
		}
		const Color modulate = item->final_modulate * p_modulate;
		const RendererCanvasRender::Item *repeat_source = item->repeat_source_item;
		int repeat_x = 0;
		int repeat_y = 0;
		if (repeat_source && item->repeat_times > 0 && item->repeat_size.is_finite()) {
			repeat_x = item->repeat_size.x != 0 ? item->repeat_times : 0;
			repeat_y = item->repeat_size.y != 0 ? item->repeat_times : 0;
			if ((uint64_t(repeat_x) + 1) * (uint64_t(repeat_y) + 1) > MAX_REPEAT_COPIES) {
				WARN_PRINT_ONCE("Fixed Function draws canvas items exceeding its repeat limit once.");
				repeat_x = repeat_y = 0;
			}
		}
		if (!repeat_x && !repeat_y) {
			const Error error = _render_item_commands(*item, item->final_transform, modulate, state, bones, skeleton_transform, p_canvas_transform);
			if (error != OK) {
				return error;
			}
			continue;
		}
		// Match Compatibility's centered grid and transform offsets in the repeat source's basis.
		for (int y = 0; y <= repeat_y; y++) {
			for (int x = 0; x <= repeat_x; x++) {
				const Vector2 offset = repeat_source->final_transform.basis_xform(Vector2((x - repeat_x / 2) * item->repeat_size.x, (y - repeat_y / 2) * item->repeat_size.y));
				// Translate the skinning reference frames too, so every copy uses the same local pose.
				const Error error = _render_item_commands(*item, item->final_transform.translated(offset), modulate, state, bones, skeleton_transform.translated(offset), p_canvas_transform.translated(offset));
				if (error != OK) {
					return error;
				}
			}
		}
	}
	return OK;
}

Error FixedFunctionCanvasItemRenderer::render_items(RendererCanvasRender::Item *p_items, const Color &p_modulate, RSE::CanvasItemTextureFilter p_filter, RSE::CanvasItemTextureRepeat p_repeat, const Transform2D &p_canvas_transform, bool p_snap_vertices) {
	_pixel_snap_enabled = p_snap_vertices;
	_redraw_requested = false;
	_render_info = RenderingServerTypes::RenderInfo();
	const Error error = _render_items(p_items, p_modulate, p_filter, p_repeat, p_canvas_transform);
	return error == OK ? OK : _render_targets.report_draw_error(error);
}

FixedFunctionCanvasItemRenderer::FixedFunctionCanvasItemRenderer(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets) :
		_context(p_context),
		_render_targets(p_render_targets),
		_next_polygon(0),
		_pixel_snap_enabled(false),
		_redraw_requested(false) {
}
