#pragma once

#include "core/templates/vector.h"
#include "drivers/fixed_function/fixed_function_defines.h"

// Native upload copies, not the authoritative Godot mesh. Backends choose their
// limits; local 16-bit batches never limit source mesh size or index precision.
class FixedFunctionGeometryData {
public:
	enum { MAX_BATCH_VERTICES = 65535 };
	struct FFIndexRange {
		uint32_t first;
		uint32_t count;
	};
	// Each offset begins an authored LOD. Level zero starts at index zero.
	static FFIndexRange get_index_range(const Vector<uint32_t> &p_lod_offsets, uint32_t p_index_count, uint32_t p_lod);
	static void pack_fixed_lit_vertices(const Vector<FixedFunctionLightingVertex> &p_vertices, void *r_output, bool p_ambient_occlusion);
	static void pack_fixed_lit_vertices(const FixedFunctionLightingVertex *p_vertices, uint32_t p_count, void *r_output, bool p_ambient_occlusion);
	struct FFGeometryBatch {
		// Exactly one vertex array is populated; all layouts share the index partitioner.
		Vector<FixedFunctionVertex> vertices;
		Vector<FixedFunctionLightingVertex> lighting_vertices;
		Vector<FixedFunctionNormalMapVertex> normal_vertices;
		Vector<uint16_t> indices;
		Vector<uint32_t> lod_offsets;
		FFIndexRange get_index_range(uint32_t p_lod) const {
			return FixedFunctionGeometryData::get_index_range(lod_offsets, uint32_t(indices.size()), p_lod);
		}
		uint32_t get_vertex_count() const {
			if (normal_vertices.size()) {
				return normal_vertices.size();
			}
			if (lighting_vertices.size()) {
				return lighting_vertices.size();
			}
			return vertices.size();
		}
		uint32_t get_vertex_stride() const {
			if (normal_vertices.size()) {
				return sizeof(FixedFunctionNormalMapVertex);
			}
			if (lighting_vertices.size()) {
				return sizeof(FixedFunctionLightingVertex);
			}
			return sizeof(FixedFunctionVertex);
		}
	};

private:
	template <class TVertex>
	static Error _partition(const Vector<TVertex> &p_vertices, const Vector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches);
	template <class TVertex>
	static Error _copy_vertices(const Vector<TVertex> &p_source, Vector<TVertex> &r_vertices);
	static Error _validate_vertex(const FixedFunctionVertex &p_vertex);
	static Error _validate_vertex(const FixedFunctionLightingVertex &p_vertex);
	static Error _validate_vertex(const FixedFunctionNormalMapVertex &p_vertex);
	static Error _append_batch(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint16_t> &p_indices, Vector<FFGeometryBatch> &r_batches);
	static Error _append_batch(const Vector<FixedFunctionNormalMapVertex> &p_vertices, const Vector<uint16_t> &p_indices, Vector<FFGeometryBatch> &r_batches);
	static Error _append_batch(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint16_t> &p_indices, Vector<FFGeometryBatch> &r_batches);

public:
	static Error validate_lod_batch(const FFGeometryBatch &p_batch, uint32_t p_max_vertices, uint32_t p_max_primitives);
	// Empty indices mean a nonindexed triangle list. Preserve triangle order and
	// attributes exactly, including degenerate triangles. Failure retains r_batches.
	static Error partition(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches);
	static Error partition(const Vector<FixedFunctionNormalMapVertex> &p_vertices, const Vector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches);
	static Error partition(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches);
};
