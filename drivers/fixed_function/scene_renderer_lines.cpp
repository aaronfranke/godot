#include "drivers/fixed_function/scene_renderer.h"

#include <float.h>

Error FixedFunctionSceneRenderer::_offset_projected_vertex(const Vector3 &p_position, const Vector4 &p_clip, const Projection &p_projection, const Vector2 &p_offset, Vector3 &r_position) {
	const Vector2 ndc = Vector2(p_clip.x, p_clip.y) / p_clip.w + p_offset;
	// Solve the view-plane displacement directly. Unlike a full projection inverse,
	// this also handles very small orthographic scales without an absolute determinant cutoff.
	const real_t a = p_projection.columns[0][0] - ndc.x * p_projection.columns[0][3];
	const real_t b = p_projection.columns[1][0] - ndc.x * p_projection.columns[1][3];
	const real_t c = p_projection.columns[0][1] - ndc.y * p_projection.columns[0][3];
	const real_t d = p_projection.columns[1][1] - ndc.y * p_projection.columns[1][3];
	const real_t determinant = a * d - b * c;
	if (determinant == 0 || Math::is_nan(determinant) || Math::is_inf(determinant)) {
		return ERR_INVALID_PARAMETER;
	}
	const real_t x = ndc.x * p_clip.w - p_clip.x;
	const real_t y = ndc.y * p_clip.w - p_clip.y;
	r_position = p_position + Vector3((x * d - b * y) / determinant, (a * y - x * c) / determinant, 0);
	return OK;
}

Error FixedFunctionSceneRenderer::_line_quad(const FFSpatialVertex *p_endpoints, const Projection &p_projection, const Size2i &p_size, float p_width, FixedFunctionVertex *r_vertices, int &r_vertex_count, bool p_antialiased) {
	r_vertex_count = 0;
	if (p_size.x <= 0 || p_size.y <= 0 || Math::is_nan(p_width) || Math::is_inf(p_width)) {
		return ERR_INVALID_PARAMETER;
	}
	Vector4 clip[2];
	real_t scale = 1;
	for (int end = 0; end < 2; end++) {
		clip[end] = p_projection.xform(Vector4(p_endpoints[end].position.x, p_endpoints[end].position.y, p_endpoints[end].position.z, 1));
		for (int component = 0; component < 4; component++) {
			const real_t value = clip[end][component];
			if (Math::is_nan(value) || Math::is_inf(value)) {
				return ERR_INVALID_PARAMETER;
			}
			scale = MAX(scale, Math::abs(value));
		}
	}
	real_t first = 0;
	real_t last = 1;
	// Clip the centerline before dividing by w or widening it. This handles eye/near
	// crossings and enormous editor axes without generating unbounded screen quads.
	for (int plane = 0; plane < 6; plane++) {
		const int axis = plane / 2;
		const real_t sign = plane & 1 ? -1 : 1;
		const real_t from = clip[0].w / scale + sign * (clip[0][axis] / scale);
		const real_t to = clip[1].w / scale + sign * (clip[1][axis] / scale);
		if (from < 0 && to < 0) {
			return OK;
		}
		if ((from < 0) != (to < 0)) {
			const real_t distance_scale = MAX(Math::abs(from), Math::abs(to));
			const real_t t = (from / distance_scale) / (from / distance_scale - to / distance_scale);
			if (from < 0) {
				first = MAX(first, t);
			} else {
				last = MIN(last, t);
			}
			if (first >= last) {
				return OK;
			}
		}
	}
	FFSpatialVertex endpoints[2];
	for (int end = 0; end < 2; end++) {
		const real_t t = end ? last : first;
		endpoints[end].position = p_endpoints[0].position * (1 - t) + p_endpoints[1].position * t;
		endpoints[end].uv = p_endpoints[0].uv * (1 - t) + p_endpoints[1].uv * t;
		endpoints[end].color = p_endpoints[0].color.lerp(p_endpoints[1].color, float(t));
		clip[end] = p_projection.xform(Vector4(endpoints[end].position.x, endpoints[end].position.y, endpoints[end].position.z, 1));
		if (clip[end].w <= 0) {
			return OK; // A degenerate endpoint at the homogeneous origin has no drawable stroke.
		}
	}
	Vector2 direction = Vector2(clip[1].x / clip[1].w - clip[0].x / clip[0].w, clip[1].y / clip[1].w - clip[0].y / clip[0].w);
	direction *= Vector2(p_size.x, p_size.y);
	const real_t length = direction.length();
	if (length == 0) {
		return OK;
	}
	if (Math::is_nan(length) || Math::is_inf(length)) {
		return ERR_INVALID_PARAMETER;
	}
	// Beyond twice the viewport diagonal, more width cannot change visible coverage.
	// Bound only this draw copy; the requested material width remains CPU metadata.
	const real_t width = MIN(real_t(MAX(p_width, 1.0f)), Vector2(p_size.x, p_size.y).length() * 2);
	const Vector2 offset = Vector2(-direction.y, direction.x) / length * width / Vector2(p_size.x, p_size.y);
	const bool hairline = p_antialiased && width == 1;
	FixedFunctionVertex packed[8];
	for (int corner = 0; corner < (p_antialiased ? 8 : 4); corner++) {
		if (hairline && corner < 4 && (corner & 1)) {
			packed[corner] = packed[corner - 1];
			continue;
		}
		const int end = (corner % 4) / 2;
		FFSpatialVertex vertex = endpoints[end];
		Vector2 displacement = corner & 1 ? offset : -offset;
		if (p_antialiased) {
			// Match the canvas coverage model: inset the solid body by half a pixel,
			// with an alpha-zero outer ring half a pixel beyond the nominal stroke.
			const bool outside = corner >= 4;
			const Vector2 tangent = direction / length;
			const Vector2 normal(-tangent.y, tangent.x);
			const real_t radius = outside ? width + 1 : width - 1;
			const real_t cap = outside ? real_t(1) : -MIN(real_t(1), length * real_t(0.5));
			displacement = (normal * (corner & 1 ? radius : -radius) + tangent * (end ? cap : -cap)) / Vector2(p_size.x, p_size.y);
			vertex.color.a *= outside ? 0 : float(MIN(real_t(1), length * real_t(0.5)));
		}
		Error error = _offset_projected_vertex(endpoints[end].position, clip[end], p_projection, displacement, vertex.position);
		if (error != OK) {
			return error;
		}
		error = _pack_vertex(vertex, packed[corner]);
		if (error != OK) {
			return error;
		}
	}
	if (hairline) {
		// Each pair of inner corners coincides, so the solid core and half of each cap have zero area.
		const int indices[] = { 0, 4, 5, 1, 5, 7, 1, 7, 3, 3, 7, 6, 2, 6, 4, 2, 4, 0 };
		for (int vertex : indices) {
			r_vertices[r_vertex_count++] = packed[vertex];
		}
		return OK;
	}
	const int indices[] = { 0, 1, 2, 2, 1, 3 };
	for (int vertex : indices) {
		r_vertices[r_vertex_count++] = packed[vertex];
	}
	if (p_antialiased) {
		const int perimeter[] = { 0, 1, 3, 2 };
		for (int side = 0; side < 4; side++) {
			const int from = perimeter[side];
			const int to = perimeter[(side + 1) % 4];
			const int edge[] = { from, from + 4, to + 4, from, to + 4, to };
			for (int vertex : edge) {
				r_vertices[r_vertex_count++] = packed[vertex];
			}
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_submit_lines(const FFDrawPacket &p_packet, const Projection &p_projection, RID p_texture, RenderingServerTypes::RenderInfo *r_info) {
	FixedFunctionGraphicsDevice &device = _context.get_device();
	const Size2i size(device.get_target_width(), device.get_target_height());
	const uint8_t *source = p_packet.surface.source.vertex_data.ptr();
	const uint8_t *indices = p_packet.surface.source.index_data.ptr();
	const uint32_t count = p_packet.surface.source.index_count ? p_packet.surface.source.index_count : p_packet.surface.source.vertex_count;
	if (count < 2) {
		return OK;
	}
	const uint32_t segments = p_packet.surface.source.primitive == RSE::PRIMITIVE_LINES ? count / 2 : count - 1;
	FixedFunctionGraphicsDevice::FFDrawParameters state = p_packet.material.state;
	state.cull_mode = FixedFunctionGraphicsDevice::CULL_DISABLED; // Lines have no front/back faces.
	uint32_t used = 0;
	FixedFunctionVertex *batch = _vertices.ptrw();
	bool first_surface = true;
	for (uint32_t segment = 0; segment < segments; segment++) {
		FFSpatialVertex endpoints[2];
		for (int end = 0; end < 2; end++) {
			uint32_t index = p_packet.surface.source.primitive == RSE::PRIMITIVE_LINES ? segment * 2 + end : segment + end;
			if (index == count) {
				index = 0;
			}
			const uint32_t vertex_index = FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices, index);
			const Error error = _read_vertex(p_packet, source, vertex_index, endpoints[end]);
			if (error != OK) {
				return error;
			}
			if (!(p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED)) {
				const Vector3 normal = _unit_normal(p_packet.normal_from_model.xform(_draw_normal(p_packet, source, vertex_index)));
				_shade_vertex(p_packet, normal, endpoints[end]);
			}
		}
		FixedFunctionVertex quad[MAX_LINE_VERTICES];
		int vertex_count;
		Error error = _line_quad(endpoints, p_projection, size, p_packet.line_width, quad, vertex_count, p_packet.antialiased);
		if (error != OK) {
			return error;
		}
		for (int vertex = 0; vertex < vertex_count; vertex++) {
			batch[used++] = quad[vertex];
			if (used == uint32_t(_vertices.size())) {
				error = _submit_batch(p_packet, p_projection, p_texture, state, used, first_surface, r_info);
				if (error != OK) {
					return error;
				}
				used = 0;
			}
		}
	}
	return used ? _submit_batch(p_packet, p_projection, p_texture, state, used, first_surface, r_info) : OK;
}
