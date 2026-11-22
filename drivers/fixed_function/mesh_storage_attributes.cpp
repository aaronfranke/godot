#include "mesh_storage.h"

#include <string.h>

Vector2 FixedFunctionMeshStorage::surface_read_uv(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, int p_uv_set) {
	Vector2 uv;
	const int attribute = p_uv_set ? VS::ARRAY_TEX_UV2 : VS::ARRAY_TEX_UV;
	if (p_surface.format & (1 << attribute)) {
		if (!p_surface.immediate_vertices.empty()) {
			return p_surface.immediate_vertices[p_index].uv[p_uv_set ? 1 : 0];
		}
		const bool compressed_uv = p_surface.format & (1 << (attribute + VS::ARRAY_COMPRESS_BASE));
		for (int i = 0; i < 2; i++) {
			const uint8_t *source = p_data + p_surface.offsets[attribute] + p_index * p_surface.strides[attribute] + i * (compressed_uv ? 2 : 4);
			float component;
			if (compressed_uv) {
				uint16_t half;
				memcpy(&half, source, 2);
				component = Math::half_to_float(half);
			} else {
				memcpy(&component, source, 4);
			}
			uv[i] = component;
		}
	}
	return uv;
}

Color FixedFunctionMeshStorage::surface_read_color(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index) {
	if (!p_surface.immediate_vertices.empty()) {
		return p_surface.immediate_vertices[p_index].color;
	}
	const uint8_t *source = p_data + p_surface.offsets[VS::ARRAY_COLOR] + p_index * p_surface.strides[VS::ARRAY_COLOR];
	Color color;
	for (int i = 0; i < 4; i++) {
		if (p_surface.format & VS::ARRAY_COMPRESS_COLOR) {
			color[i] = source[i] / 255.0f;
		} else {
			memcpy(&color[i], source + i * 4, 4);
		}
	}
	return color;
}

uint32_t FixedFunctionMeshStorage::surface_get_triangle_count(const FFSurfaceData &p_surface) {
	const uint32_t count = p_surface.index_count ? p_surface.index_count : p_surface.vertex_count;
	switch (p_surface.primitive) {
		case VS::PRIMITIVE_TRIANGLES:
			return count / 3;
		case VS::PRIMITIVE_TRIANGLE_STRIP:
		case VS::PRIMITIVE_TRIANGLE_FAN:
			return count > 2 ? count - 2 : 0;
		default:
			return 0;
	}
}

void FixedFunctionMeshStorage::surface_expand_triangles(const FFSurfaceData &p_surface, const uint8_t *p_indices, uint32_t p_first_triangle, uint32_t p_count, uint32_t *r_indices) {
	if (p_surface.primitive == VS::PRIMITIVE_TRIANGLES) {
		const uint32_t first = p_first_triangle * 3;
		for (uint32_t i = 0; i < p_count * 3; i++) {
			r_indices[i] = surface_read_index(p_surface, p_indices, first + i);
		}
		return;
	}
	const bool strip = p_surface.primitive == VS::PRIMITIVE_TRIANGLE_STRIP;
	const uint32_t fan_center = strip ? 0 : surface_read_index(p_surface, p_indices, 0);
	for (uint32_t i = 0; i < p_count; i++) {
		const uint32_t triangle = p_first_triangle + i;
		if (strip) {
			// Keep the original strip parity across batches and degenerate connectors.
			const uint32_t odd = triangle & 1;
			r_indices[0] = surface_read_index(p_surface, p_indices, triangle + odd);
			r_indices[1] = surface_read_index(p_surface, p_indices, triangle + 1 - odd);
		} else {
			r_indices[0] = fan_center;
			r_indices[1] = surface_read_index(p_surface, p_indices, triangle + 1);
		}
		r_indices[2] = surface_read_index(p_surface, p_indices, triangle + 2);
		r_indices += 3;
	}
}
