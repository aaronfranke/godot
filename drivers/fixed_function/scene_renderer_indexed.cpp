#include "scene_renderer.h"

#include <string.h>

uint32_t FixedFunctionSceneRenderer::_remap_batch(const FFDrawPacket &p_packet, const uint8_t *p_indices, const uint32_t *p_expanded, uint32_t p_first_index, uint32_t p_count) {
	if (p_count < 96 || !p_packet.surface.index_count) {
		return 0;
	}
	if ((_batch_source_indices.size() < int(p_count) && _batch_source_indices.resize(p_count) != OK) ||
			(_batch_draw_indices.size() < int(p_count) && _batch_draw_indices.resize(p_count) != OK) ||
			(_batch_index_table.empty() && _batch_index_table.resize(BATCH_INDEX_TABLE_SIZE) != OK)) {
		return 0;
	}
	static_assert(MAX_BATCH_PRIMITIVES * 6 < BATCH_INDEX_TABLE_SIZE, "Keep the remap table below half full.");
	uint32_t *table = _batch_index_table.ptrw();
	if (++_batch_index_generation <= 1) {
		memset(table, 0, BATCH_INDEX_TABLE_SIZE * sizeof(uint32_t));
		_batch_index_generation = 1;
	}
	const uint32_t generation = uint32_t(_batch_index_generation) << 16;
	uint32_t *sources = _batch_source_indices.ptrw();
	uint16_t *indices = _batch_draw_indices.ptrw();
	uint32_t unique_count = 0;
	for (uint32_t i = 0; i < p_count; i++) {
		const uint32_t source = p_expanded ? p_expanded[i] : FixedFunctionMeshStorage::surface_read_index(p_packet.surface, p_indices, p_first_index + i);
		uint32_t slot = (source ^ (source >> 15)) & (BATCH_INDEX_TABLE_SIZE - 1);
		while ((table[slot] & 0xffff0000) == generation && sources[uint16_t(table[slot])] != source) {
			slot = (slot + 1) & (BATCH_INDEX_TABLE_SIZE - 1);
		}
		if ((table[slot] & 0xffff0000) != generation) {
			sources[unique_count] = source;
			table[slot] = generation | unique_count;
			unique_count++;
		}
		indices[i] = uint16_t(table[slot]);
	}
	return unique_count == p_count ? 0 : unique_count;
}

Error FixedFunctionSceneRenderer::_pack_indexed_batch(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_vertex_count, const FFOcclusionSample *p_occlusion) {
	FixedFunctionVertex *output = _vertices.ptrw();
	const uint32_t *sources = _batch_source_indices.ptr();
	if (!_has_direct_lights(p_packet)) {
		return _pack_ambient_indexed_vertices(p_packet, p_data, sources, p_vertex_count, output, p_occlusion);
	}
	for (uint32_t i = 0; i < p_vertex_count; i++) {
		const uint32_t index = sources[i];
		const Vector3 normal = _unit_normal(p_packet.normal_from_model.xform(_vertex_normal(p_packet, p_data, index)));
		FFSpatialVertex vertex;
		if (normal == Vector3()) {
			// Synthesized face normals require the original triangle corners.
			return ERR_UNAVAILABLE;
		}
		const Error read_error = _read_vertex(p_packet, p_data, index, vertex);
		if (read_error != OK) {
			return read_error;
		}
		const Error pack_error = _shade_and_pack_vertex(p_packet, p_data, index, normal, vertex, output[i], p_occlusion);
		if (pack_error != OK) {
			return pack_error;
		}
	}
	return OK;
}
