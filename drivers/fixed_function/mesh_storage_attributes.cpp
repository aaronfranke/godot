#include "drivers/fixed_function/mesh_storage.h"

#include <string.h>

Vector2 FixedFunctionMeshStorage::surface_read_uv(const FFSurfaceData &surface, uint32_t p_index, int p_uv_set) {
	Vector2 uv;
	const int attribute = p_uv_set == 0 ? RSE::ARRAY_TEX_UV : RSE::ARRAY_TEX_UV2;
	if (surface.source.format & (uint64_t(1) << attribute)) {
		const bool compressed_uv = surface.source.format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES;
		for (int i = 0; i < 2; i++) {
			const uint8_t *source = surface.source.attribute_data.ptr() + surface.offsets[attribute] + p_index * surface.attribute_stride + i * (compressed_uv ? 2 : 4);
			float component;
			if (compressed_uv) {
				uint16_t half;
				memcpy(&half, source, 2);
				component = float(half) / 65535.0f;
			} else {
				memcpy(&component, source, 4);
			}
			uv[i] = component;
		}
	}
	if ((surface.source.format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) && !surface.source.uv_scale.is_zero_approx()) {
		const Vector4 &scale = surface.source.uv_scale;
		uv = (uv - Vector2(0.5, 0.5)) * (attribute == RSE::ARRAY_TEX_UV ? Vector2(scale.x, scale.y) : Vector2(scale.z, scale.w));
	}
	return uv;
}

uint32_t FixedFunctionMeshStorage::surface_get_triangle_count(const FFSurfaceData &p_surface) {
	const uint32_t count = p_surface.source.index_count ? p_surface.source.index_count : p_surface.source.vertex_count;
	if (p_surface.source.primitive == RSE::PRIMITIVE_TRIANGLES) {
		return count / 3;
	}
	if (p_surface.source.primitive == RSE::PRIMITIVE_TRIANGLE_STRIP && count > 2) {
		return count - 2;
	}
	return 0;
}

void FixedFunctionMeshStorage::surface_expand_triangle_strip(const FFSurfaceData &p_surface, const uint8_t *p_indices, uint32_t p_first_triangle, uint32_t p_count, uint32_t *r_indices) {
	for (uint32_t i = 0; i < p_count; i++) {
		const uint32_t triangle = p_first_triangle + i;
		// Keep the original winding across batches and degenerate connectors.
		const uint32_t odd = triangle & 1;
		r_indices[0] = surface_read_index(p_surface, p_indices, triangle + odd);
		r_indices[1] = surface_read_index(p_surface, p_indices, triangle + 1 - odd);
		r_indices[2] = surface_read_index(p_surface, p_indices, triangle + 2);
		r_indices += 3;
	}
}

Color FixedFunctionMeshStorage::surface_read_color(const FFSurfaceData &p_surface, uint32_t p_index) {
	if (!(p_surface.source.format & RSE::ARRAY_FORMAT_COLOR)) {
		return Color(1, 1, 1);
	}
	const uint8_t *color = p_surface.source.attribute_data.ptr() + p_surface.offsets[RSE::ARRAY_COLOR] + p_index * p_surface.attribute_stride;
	return Color(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, color[3] / 255.0f);
}
