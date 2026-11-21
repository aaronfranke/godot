#include "drivers/d3d3_software/graphics_device_d3d3_software.h"

Error FixedFunctionGraphicsDeviceD3D3Software::_check_canvas_blt(const FFDrawParameters &p_state) const {
	const Error error = _check_thread();
	if (error != OK || !_frame_active) {
		return error == OK ? ERR_UNCONFIGURED : error;
	}
	if (p_state.alpha_cutoff_enabled || p_state.cull_mode != CULL_DISABLED || p_state.depth_test_enabled || p_state.depth_write_enabled ||
			(p_state.blend_mode != BLEND_MODE_DISABLED && p_state.blend_mode != BLEND_MODE_MIX)) {
		return ERR_UNAVAILABLE;
	}
	return validate_draw_state(p_state);
}

Rect2 FixedFunctionGraphicsDeviceD3D3Software::_get_native_canvas_bounds(const Rect2 &p_bounds) const {
	const Vector2 scale = Vector2(_pass->native_size) / Vector2(_pass->size);
	const Point2 start = p_bounds.position * scale;
	return Rect2(start, (p_bounds.position + p_bounds.size) * scale - start);
}

bool FixedFunctionGraphicsDeviceD3D3Software::_get_clipped_blt_rect(const Rect2 &p_bounds, const Rect2i &p_clip, RECT &r_rect) {
	const real_t left = MAX(p_bounds.position.x, p_clip.position.x);
	const real_t top = MAX(p_bounds.position.y, p_clip.position.y);
	const real_t right = MIN(p_bounds.position.x + p_bounds.size.x, p_clip.position.x + p_clip.size.x);
	const real_t bottom = MIN(p_bounds.position.y + p_bounds.size.y, p_clip.position.y + p_clip.size.y);
	if (left >= right || top >= bottom) {
		r_rect = RECT();
		return true;
	}
	// Keep fractional native edges on the triangle path to preserve raster coverage.
	if (left != Math::floor(left) || top != Math::floor(top) || right != Math::floor(right) || bottom != Math::floor(bottom)) {
		return false;
	}
	r_rect = { LONG(left), LONG(top), LONG(right), LONG(bottom) };
	return true;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_fill_canvas_rect(RECT &p_rect, uint32_t p_color) {
	if (p_rect.left == p_rect.right || p_rect.top == p_rect.bottom) {
		return OK;
	}
	DDBLTFX fill = {};
	fill.dwSize = sizeof(fill);
	fill.dwFillColor = p_color & 0x00ffffff;
	const Error blit = _check_result(_pass->color->Blt(&p_rect, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fill));
	return blit == OK || blit == ERR_SKIP ? blit : ERR_UNAVAILABLE;
}

bool FixedFunctionGraphicsDeviceD3D3Software::_get_canvas_rectangle(const FixedFunctionVertex *const *p_triangles, Rect2 &r_bounds) {
	const uint32_t color = p_triangles[0][0].color;
	if ((color >> 24) != 255) {
		return false;
	}
	r_bounds = Rect2(Point2(p_triangles[0][0].x, p_triangles[0][0].y), Size2());
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 3; j++) {
			const FixedFunctionVertex &v = p_triangles[i][j];
			if (v.color != color) {
				return false;
			}
			r_bounds.expand_to(Point2(v.x, v.y));
		}
	}
	const Point2 end = r_bounds.position + r_bounds.size;
	uint8_t corners[2] = {};
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 3; j++) {
			const FixedFunctionVertex &v = p_triangles[i][j];
			if ((v.x != r_bounds.position.x && v.x != end.x) || (v.y != r_bounds.position.y && v.y != end.y)) {
				return false;
			}
			const int corner = int(v.x == end.x) + 2 * int(v.y == end.y);
			corners[i] |= 1 << corner;
		}
	}
	// The two triangles must cover all four corners and share a diagonal.
	const uint8_t diagonal = corners[0] & corners[1];
	return (corners[0] | corners[1]) == 0xf && (diagonal == 0x6 || diagonal == 0x9);
}

bool FixedFunctionGraphicsDeviceD3D3Software::_get_canvas_border(const FixedFunctionVertex *const *p_triangles, int p_count, const Rect2 &p_inner, Rect2 &r_outer, uint32_t &r_color) {
	r_color = p_triangles[0][0].color;
	if ((r_color >> 24) != 255) {
		return false;
	}
	r_outer = p_inner;
	for (int i = 0; i < p_count; i++) {
		for (int j = 0; j < 3; j++) {
			const FixedFunctionVertex &v = p_triangles[i][j];
			if (v.color != r_color) {
				return false;
			}
			r_outer.expand_to(Point2(v.x, v.y));
		}
	}
	const Point2 a = r_outer.position;
	const Point2 b = a + r_outer.size;
	const Point2 c = p_inner.position;
	const Point2 d = c + p_inner.size;
	const Point2 outer[] = { a, Point2(b.x, a.y), b, Point2(a.x, b.y) };
	const Point2 inner[] = { c, Point2(d.x, c.y), d, Point2(c.x, d.y) };
	int index = 0;
	// Check the four trapezoids emitted by a sharp StyleBoxFlat in border order.
	// A zero-width side or a shared outer/inner corner can collapse a triangle.
	for (int side = 0; side < 4; side++) {
		const int next = (side + 1) % 4;
		const Point2 expected[2][3] = {
			{ inner[side], inner[next], outer[side] },
			{ outer[side], outer[next], inner[next] },
		};
		for (int i = 0; i < 2; i++) {
			const Point2 *points = expected[i];
			if ((points[1] - points[0]).cross(points[2] - points[0]) == 0) {
				continue;
			}
			if (index == p_count) {
				return false;
			}
			const FixedFunctionVertex *triangle = p_triangles[index++];
			for (int j = 0; j < 3; j++) {
				if (Point2(triangle[j].x, triangle[j].y) != points[j]) {
					return false;
				}
			}
		}
	}
	return index == p_count;
}

Error FixedFunctionGraphicsDeviceD3D3Software::fill_canvas_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const FFDrawParameters &p_state) {
	const Error error = _check_canvas_blt(p_state);
	if (error != OK) {
		return error;
	}
	if (!_pass->rgb888_fill || p_vertex_count < 6) {
		return ERR_UNAVAILABLE;
	}
	if (!p_vertices || p_vertex_count % 3) {
		return ERR_INVALID_PARAMETER;
	}
	enum { MAX_RECT_TRIANGLES = 10 };
	const FixedFunctionVertex *triangles[MAX_RECT_TRIANGLES] = {};
	int triangle_count = 0;
	for (uint32_t i = 0; i < p_vertex_count; i += 3) {
		const FixedFunctionVertex *v = p_vertices + i;
		const Point2 a(v[0].x, v[0].y);
		const Point2 b(v[1].x, v[1].y);
		const Point2 c(v[2].x, v[2].y);
		if (a == b || a == c || b == c || (a.x == b.x && a.x == c.x) || (a.y == b.y && a.y == c.y)) {
			continue;
		}
		if (triangle_count == MAX_RECT_TRIANGLES) {
			return ERR_UNAVAILABLE;
		}
		triangles[triangle_count++] = v;
	}
	Rect2 inner;
	if (triangle_count < 2 || !_get_canvas_rectangle(triangles + triangle_count - 2, inner)) {
		return ERR_UNAVAILABLE;
	}
	const Rect2i clip = _get_native_draw_viewport(p_state);
	RECT center;
	if (!_get_clipped_blt_rect(_get_native_canvas_bounds(inner), clip, center)) {
		return ERR_UNAVAILABLE;
	}
	if (triangle_count > 2) {
		Rect2 outer;
		uint32_t border_color;
		RECT border;
		if (!_get_canvas_border(triangles, triangle_count - 2, inner, outer, border_color) ||
				!_get_clipped_blt_rect(_get_native_canvas_bounds(outer), clip, border)) {
			return ERR_UNAVAILABLE;
		}
		const Error border_error = _fill_canvas_rect(border, border_color);
		if (border_error != OK) {
			return border_error;
		}
		// Both colors are opaque, so the original triangles can replace the outer fill if needed.
	}
	return _fill_canvas_rect(center, triangles[triangle_count - 2][0].color);
}

Error FixedFunctionGraphicsDeviceD3D3Software::copy_canvas_render_target(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, RID p_texture, const FFDrawParameters &p_state) {
	const Error error = _check_canvas_blt(p_state);
	if (error != OK) {
		return error;
	}
	if (p_vertex_count != 6 || !p_state.texture_rgb_enabled || p_state.sampler_flags) {
		return ERR_UNAVAILABLE;
	}
	if (!p_vertices || !_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D3SoftwareRenderTarget *source = _texture_owner.get_or_null(p_texture)->target;
	if (!source || source == _pass || !source->readable || source->is_lost()) {
		return ERR_UNAVAILABLE;
	}
	const Point2 first(p_vertices[0].x, p_vertices[0].y);
	const Point2 last(p_vertices[2].x, p_vertices[2].y);
	const bool flip_x = p_vertices[0].u == 1;
	const bool flip_y = p_vertices[0].v == 1;
	const Point2 uv[] = { Point2(0, 0), Point2(1, 0), Point2(1, 1), Point2(0, 0), Point2(1, 1), Point2(0, 1) };
	for (int i = 0; i < 6; i++) {
		const FixedFunctionVertex &v = p_vertices[i];
		const real_t expected_u = flip_x ? 1 - uv[i].x : uv[i].x;
		const real_t expected_v = flip_y ? 1 - uv[i].y : uv[i].y;
		if (v.color != 0xffffffff || v.u != expected_u || v.v != expected_v ||
				v.x != (uv[i].x ? last.x : first.x) || v.y != (uv[i].y ? last.y : first.y)) {
			return ERR_UNAVAILABLE;
		}
	}
	const Rect2 dest = _get_native_canvas_bounds(Rect2(first, last - first));
	const Size2i &source_size = source->native_size;
	if (dest.position != dest.position.floor() || dest.size.x < source_size.x || dest.size.y < source_size.y) {
		return ERR_UNAVAILABLE;
	}
	RECT dst;
	if (!_get_clipped_blt_rect(dest, _get_native_draw_viewport(p_state), dst)) {
		return ERR_UNAVAILABLE;
	}
	if (dst.left == dst.right || dst.top == dst.bottom) {
		return OK;
	}
	// Use only the rendered part of the source, excluding texture padding.
	RECT src = { 0, 0, source_size.x, source_size.y };
	if (dest.size == Vector2(source_size)) {
		const LONG x = LONG(dest.position.x);
		const LONG y = LONG(dest.position.y);
		src = { dst.left - x, dst.top - y, dst.right - x, dst.bottom - y };
		// Mirror the source crop as well as the copy so clipping keeps the same texels.
		if (flip_x) {
			const LONG left = src.left;
			src.left = source_size.x - src.right;
			src.right = source_size.x - left;
		}
		if (flip_y) {
			const LONG top = src.top;
			src.top = source_size.y - src.bottom;
			src.bottom = source_size.y - top;
		}
	} else {
		// Whole-texel magnification preserves point sampling. A cropped stretch can shift
		// the sampling phase, so leave it to the textured draw.
		const LONG width = dst.right - dst.left;
		const LONG height = dst.bottom - dst.top;
		if (width != dest.size.x || height != dest.size.y || width % source_size.x || height % source_size.y) {
			return ERR_UNAVAILABLE;
		}
	}
	HRESULT result;
	if (flip_x || flip_y) {
		DDBLTFX effects = {};
		effects.dwSize = sizeof(effects);
		effects.dwDDFX = (flip_x ? DDBLTFX_MIRRORLEFTRIGHT : 0) | (flip_y ? DDBLTFX_MIRRORUPDOWN : 0);
		result = _pass->color->Blt(&dst, source->color, &src, DDBLT_WAIT | DDBLT_DDFX, &effects);
	} else {
		result = _pass->color->Blt(&dst, source->color, &src, DDBLT_WAIT, nullptr);
	}
	const Error blit = _check_result(result);
	return blit == OK || blit == ERR_SKIP ? blit : ERR_UNAVAILABLE;
}
