#include "canvas_item_renderer.h"

Color FixedFunctionCanvasItemRenderer::_coverage_edge_color(const Color &p_color, FixedFunctionGraphicsDevice::FFBlendMode p_blend) {
	if (p_blend == FixedFunctionGraphicsDevice::BLEND_MODE_PREMULTIPLIED_ALPHA) {
		return Color(0, 0, 0, 0);
	}
	if (p_blend == FixedFunctionGraphicsDevice::BLEND_MODE_MUL) {
		return Color(1, 1, 1, 0);
	}
	return Color(p_color.r, p_color.g, p_color.b, 0);
}

Error FixedFunctionCanvasItemRenderer::_draw_hairlines(const RasterizerCanvas::Item::CommandPolyLine &p_line, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	const int count = p_line.lines.size();
	const int colors = p_line.line_colors.size();
	if (count < 2 || (p_line.multiline && count % 2) || (colors > 1 && colors != count)) {
		return ERR_INVALID_PARAMETER;
	}
	const uint32_t batch_size = MIN(uint32_t(128), _context.get_device().get_capabilities().max_primitives_per_draw) * 3;
	if (!batch_size) {
		return ERR_UNAVAILABLE;
	}
	Error error = _vertices.resize(batch_size);
	if (error != OK) {
		return error;
	}
	FixedFunctionVertex *batch = _vertices.ptrw();
	uint32_t used = 0;
	Vector2 last_edge;
	const bool antialiased = p_line.antialiased && p_state.blend_mode != FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
	for (int i = 0; i < count - 1; i += p_line.multiline ? 2 : 1) {
		const Vector2 start = _snap_position(p_transform.xform(p_line.lines[i]), _pixel_snap_enabled);
		const Vector2 end = _snap_position(p_transform.xform(p_line.lines[i + 1]), _pixel_snap_enabled);
		const real_t length = (end - start).length();
		if (Math::is_nan(length) || Math::is_inf(length)) {
			return ERR_INVALID_PARAMETER;
		}
		if (!length) {
			continue;
		}
		const Vector2 normal = ((end - start) / length).tangent();
		Vector2 start_edge = normal * real_t(0.5);
		Vector2 end_edge = start_edge;
		if (!p_line.multiline) {
			if (last_edge != Vector2()) {
				start_edge = last_edge;
			}
			last_edge = Vector2();
			if (i < count - 2) {
				const Vector2 next = _snap_position(p_transform.xform(p_line.lines[i + 2]), _pixel_snap_enabled) - end;
				const real_t next_length = next.length();
				if (Math::is_nan(next_length) || Math::is_inf(next_length)) {
					return ERR_INVALID_PARAMETER;
				}
				if (next_length) {
					const Vector2 next_normal = (next / next_length).tangent();
					const real_t denom = real_t(1) + normal.dot(next_normal);
					if (denom > real_t(0.25)) {
						end_edge = (normal + next_normal) * (real_t(0.5) / denom);
						last_edge = end_edge;
					}
				}
			}
		}
		Color tints[2] = { p_modulate, p_modulate };
		if (colors) {
			tints[0] *= p_line.line_colors[colors == 1 ? 0 : i];
			tints[1] *= p_line.line_colors[colors == 1 ? 0 : i + 1];
		}
		Vector2 positions[6];
		Color colors_by_corner[6];
		const int solid[] = { 0, 1, 3, 0, 3, 2 };
		const int coverage[] = { 0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4 };
		const int *order = solid;
		int vertices = 6;
		if (antialiased) {
			positions[0] = start - start_edge * 2;
			positions[1] = start;
			positions[2] = start + start_edge * 2;
			positions[3] = end - end_edge * 2;
			positions[4] = end;
			positions[5] = end + end_edge * 2;
			colors_by_corner[0] = colors_by_corner[2] = _coverage_edge_color(tints[0], p_state.blend_mode);
			colors_by_corner[1] = tints[0];
			colors_by_corner[3] = colors_by_corner[5] = _coverage_edge_color(tints[1], p_state.blend_mode);
			colors_by_corner[4] = tints[1];
			order = coverage;
			vertices = 12;
		} else {
			positions[0] = start - start_edge;
			positions[1] = start + start_edge;
			positions[2] = end - end_edge;
			positions[3] = end + end_edge;
			colors_by_corner[0] = colors_by_corner[1] = tints[0];
			colors_by_corner[2] = colors_by_corner[3] = tints[1];
		}
		for (int j = 0; j < vertices; j++) {
			const int corner = order[j];
			error = _pack_vertex(batch[used++], positions[corner], Vector2(), colors_by_corner[corner]);
			if (error != OK) {
				return error;
			}
			if (used == batch_size) {
				error = _submit(batch, used, RID(), p_state);
				if (error != OK) {
					return error;
				}
				used = 0;
			}
		}
	}
	return used ? _submit(batch, used, RID(), p_state) : OK;
}

Error FixedFunctionCanvasItemRenderer::_draw_polyline_fringe(const RasterizerCanvas::Item::CommandPolyLine &p_line, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	const int count = p_line.lines.size();
	const int colors = p_line.line_colors.size();
	if (count != p_line.triangles.size() || count < 4 || (colors > 1 && colors != count)) {
		return ERR_INVALID_PARAMETER;
	}
	Vector<Vector2> points, normals, outer;
	if (points.resize(count) != OK || normals.resize(count) != OK || outer.resize(count) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	const Vector2 origin = _snap_position(p_transform.xform(p_line.lines[0]), _pixel_snap_enabled);
	double area = 0;
	for (int i = 0; i < count; i++) {
		const Vector2 point = _snap_position(p_transform.xform(p_line.lines[i]), _pixel_snap_enabled);
		const Vector2 next = _snap_position(p_transform.xform(p_line.lines[(i + 1) % count]), _pixel_snap_enabled);
		const Vector2 edge = next - point;
		const real_t length = edge.length();
		if (Math::is_nan(length) || Math::is_inf(length)) {
			return ERR_INVALID_PARAMETER;
		}
		points.write[i] = point;
		normals.write[i] = length ? (edge / length).tangent() : Vector2();
		const Vector2 a = point - origin, b = next - origin;
		area += double(a.x) * b.y - double(a.y) * b.x;
	}
	if (area == 0) {
		return OK;
	}
	if (Math::is_nan(area) || Math::is_inf(area)) {
		return ERR_INVALID_PARAMETER;
	}
	const real_t sign = area > 0 ? 1 : -1;
	for (int i = 0; i < count; i++) {
		const Vector2 previous = normals[(i + count - 1) % count], next = normals[i];
		// Limit the outward miter at reversals and narrow joins.
		const real_t denom = MAX(real_t(0.5), real_t(1) + previous.dot(next));
		outer.write[i] = points[i] + (previous + next) * (sign / denom);
	}
	const uint32_t batch_size = MIN(uint32_t(128), _context.get_device().get_capabilities().max_primitives_per_draw) * 3;
	if (!batch_size) {
		return ERR_UNAVAILABLE;
	}
	Error error = _vertices.resize(batch_size);
	if (error != OK) {
		return error;
	}
	FixedFunctionVertex *batch = _vertices.ptrw();
	uint32_t used = 0;
	for (int i = 0; i < count; i++) {
		const int next = (i + 1) % count;
		if (normals[i] == Vector2()) {
			continue;
		}
		const Vector2 vertices[] = { points[i], outer[i], outer[next], points[next] };
		Color tints[] = { p_modulate, p_modulate, p_modulate, p_modulate };
		if (colors) {
			tints[0] *= p_line.line_colors[colors == 1 ? 0 : i];
			tints[3] *= p_line.line_colors[colors == 1 ? 0 : next];
		}
		tints[1] = _coverage_edge_color(tints[0], p_state.blend_mode);
		tints[2] = _coverage_edge_color(tints[3], p_state.blend_mode);
		const int order[] = { 0, 1, 2, 0, 2, 3 };
		for (int corner : order) {
			error = _pack_vertex(batch[used++], vertices[corner], Vector2(), tints[corner]);
			if (error != OK) {
				return error;
			}
			if (used == batch_size) {
				error = _submit(batch, used, RID(), p_state);
				if (error != OK) {
					return error;
				}
				used = 0;
			}
		}
	}
	return used ? _submit(batch, used, RID(), p_state) : OK;
}
