#include "drivers/fixed_function/canvas_item_renderer.h"

Error FixedFunctionCanvasItemRenderer::_draw_hairlines(const FFCanvasPolygon &p_polygon, bool p_strip, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	const int count = p_polygon.count;
	if (count < 2 || (!p_strip && (count & 1))) {
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
	Vector2 previous_join;
	const int color_count = p_polygon.colors.size();
	for (int i = 0; i < count - 1; i += p_strip ? 1 : 2) {
		const int a = p_polygon.indices.is_empty() ? i : p_polygon.indices[i];
		const int b = p_polygon.indices.is_empty() ? i + 1 : p_polygon.indices[i + 1];
		const Vector2 start = _snap_position(p_transform.xform(p_polygon.points[a]));
		const Vector2 end = _snap_position(p_transform.xform(p_polygon.points[b]));
		const real_t length = (end - start).length();
		if (!Math::is_finite(length)) {
			return ERR_INVALID_PARAMETER;
		}
		if (length == 0) {
			continue;
		}
		const Vector2 normal = (end - start).orthogonal() / length;
		const Vector2 side = normal * real_t(0.5);
		const Vector2 start_edge = previous_join == Vector2() ? side : previous_join;
		Vector2 end_edge = side;
		previous_join = Vector2();
		if (p_strip && i < count - 2) {
			const int c = p_polygon.indices.is_empty() ? i + 2 : p_polygon.indices[i + 2];
			const Vector2 next = _snap_position(p_transform.xform(p_polygon.points[c])) - end;
			const real_t next_length = next.length();
			if (!Math::is_finite(next_length)) {
				return ERR_INVALID_PARAMETER;
			}
			if (next_length != 0) {
				const Vector2 next_normal = next.orthogonal() / next_length;
				const real_t denominator = 1 + normal.dot(next_normal);
				if (denominator > real_t(0.25)) {
					end_edge = (normal + next_normal) * (real_t(0.5) / denominator);
					previous_join = end_edge;
				}
			}
		}
		Color color_a = p_modulate;
		Color color_b = p_modulate;
		if (color_count) {
			color_a *= p_polygon.colors[color_count == 1 ? 0 : a];
			color_b *= p_polygon.colors[color_count == 1 ? 0 : b];
		}
		const Vector2 uv_a = p_polygon.uvs.is_empty() ? Vector2() : p_polygon.uvs[a];
		const Vector2 uv_b = p_polygon.uvs.is_empty() ? Vector2() : p_polygon.uvs[b];
		const Vector2 points[] = { start - start_edge, start + start_edge, end + end_edge, end - end_edge };
		const int order[] = { 0, 1, 2, 0, 2, 3 };
		for (int corner : order) {
			error = _pack_vertex(batch[used++], points[corner], corner < 2 ? uv_a : uv_b, corner < 2 ? color_a : color_b);
			if (error != OK) {
				return error;
			}
			if (used == batch_size) {
				error = _submit(batch, used, p_texture, p_state);
				if (error != OK) {
					return error;
				}
				used = 0;
			}
		}
	}
	return used ? _submit(batch, used, p_texture, p_state) : OK;
}
