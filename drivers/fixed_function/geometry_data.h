#ifndef FIXED_FUNCTION_GEOMETRY_DATA_H
#define FIXED_FUNCTION_GEOMETRY_DATA_H

#include "fixed_function_defines.h"

#include "core/pool_vector.h"
#include "core/vector.h"

// Native upload copies, not the authoritative Godot mesh. Backends choose their
// limits; local 16-bit batches never limit source mesh size or index precision.
class FixedFunctionGeometryData {
public:
	enum { MAX_BATCH_VERTICES = 65535 };
	static void pack_fixed_lit_vertices(const PoolVector<FixedFunctionLightingVertex> &p_vertices, void *r_output, bool p_ambient_occlusion);
	static void pack_fixed_lit_vertices(const FixedFunctionLightingVertex *p_vertices, uint32_t p_count, void *r_output, bool p_ambient_occlusion);
	struct FFGeometryBatch {
		// Exactly one vertex array is populated; all layouts share the index partitioner.
		PoolVector<FixedFunctionVertex> vertices;
		PoolVector<FixedFunctionLightingVertex> lighting_vertices;
		PoolVector<FixedFunctionNormalMapVertex> normal_vertices;
		PoolVector<uint16_t> indices;
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
	static Error _partition(const PoolVector<TVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches);
	template <class TVertex>
	static Error _copy_vertices(const Vector<TVertex> &p_source, PoolVector<TVertex> &r_vertices);
	static Error _validate_vertex(const FixedFunctionVertex &p_vertex);
	static Error _validate_vertex(const FixedFunctionLightingVertex &p_vertex);
	static Error _validate_vertex(const FixedFunctionNormalMapVertex &p_vertex);
	static Error _append_batch(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint16_t> &p_indices, Vector<FFGeometryBatch> &r_batches);
	static Error _append_batch(const Vector<FixedFunctionNormalMapVertex> &p_vertices, const Vector<uint16_t> &p_indices, Vector<FFGeometryBatch> &r_batches);
	static Error _append_batch(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint16_t> &p_indices, Vector<FFGeometryBatch> &r_batches);

public:
	// Empty indices mean a nonindexed triangle list. Preserve triangle order and
	// attributes exactly, including degenerate triangles. Failure retains r_batches.
	static Error partition(const PoolVector<FixedFunctionLightingVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches);
	static Error partition(const PoolVector<FixedFunctionNormalMapVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches);
	static Error partition(const PoolVector<FixedFunctionVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches);
};

#endif // FIXED_FUNCTION_GEOMETRY_DATA_H
