#include "geometry_data.h"

#include "core/color.h"
#include "core/math/math_funcs.h"

#include <string.h>

void FixedFunctionGeometryData::pack_fixed_lit_vertices(const PoolVector<FixedFunctionLightingVertex> &p_vertices, void *r_output, bool p_ambient_occlusion) {
	const PoolVector<FixedFunctionLightingVertex>::Read vertices = p_vertices.read();
	pack_fixed_lit_vertices(vertices.ptr(), p_vertices.size(), r_output, p_ambient_occlusion);
}

void FixedFunctionGeometryData::pack_fixed_lit_vertices(const FixedFunctionLightingVertex *p_vertices, uint32_t p_count, void *r_output, bool p_ambient_occlusion) {
	if (p_ambient_occlusion) {
		static_assert(sizeof(FixedFunctionLitColorVertex) == 36 && offsetof(FixedFunctionLitColorVertex, color) == 24, "Unexpected lit color vertex layout.");
		FixedFunctionLitColorVertex *output = static_cast<FixedFunctionLitColorVertex *>(r_output);
		for (uint32_t i = 0; i < p_count; i++) {
			const FixedFunctionLightingVertex &vertex = p_vertices[i];
			const uint32_t color = Color(CLAMP(vertex.r, 0.0f, 1.0f), CLAMP(vertex.g, 0.0f, 1.0f), CLAMP(vertex.b, 0.0f, 1.0f)).to_argb32();
			output[i] = { vertex.x, vertex.y, vertex.z, vertex.nx, vertex.ny, vertex.nz, color, vertex.u, vertex.v };
		}
	} else {
		FixedFunctionLitVertex *output = static_cast<FixedFunctionLitVertex *>(r_output);
		for (uint32_t i = 0; i < p_count; i++) {
			memcpy(&output[i], &p_vertices[i], sizeof(FixedFunctionLitVertex));
		}
	}
}

template <class TVertex>
Error FixedFunctionGeometryData::_copy_vertices(const Vector<TVertex> &p_source, PoolVector<TVertex> &r_vertices) {
	const Error error = r_vertices.resize(p_source.size());
	if (error == OK) {
		typename PoolVector<TVertex>::Write output = r_vertices.write();
		memcpy(output.ptr(), p_source.ptr(), p_source.size() * sizeof(TVertex));
	}
	return error;
}

Error FixedFunctionGeometryData::_append_batch(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint16_t> &p_indices, Vector<FFGeometryBatch> &r_batches) {
	FFGeometryBatch batch;
	Error error = _copy_vertices(p_vertices, batch.vertices);
	if (error == OK) {
		error = _copy_vertices(p_indices, batch.indices);
	}
	if (error != OK) {
		return error;
	}
	return r_batches.push_back(batch) ? ERR_OUT_OF_MEMORY : OK;
}

Error FixedFunctionGeometryData::_validate_vertex(const FixedFunctionVertex &p_vertex) {
	const float values[] = { p_vertex.x, p_vertex.y, p_vertex.z, p_vertex.u, p_vertex.v };
	for (unsigned int i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		if (Math::is_nan(values[i]) || Math::is_inf(values[i])) {
			return ERR_INVALID_DATA;
		}
	}
	return OK;
}

Error FixedFunctionGeometryData::_append_batch(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint16_t> &p_indices, Vector<FFGeometryBatch> &r_batches) {
	FFGeometryBatch batch;
	Error error = _copy_vertices(p_vertices, batch.lighting_vertices);
	if (error == OK) {
		error = _copy_vertices(p_indices, batch.indices);
	}
	if (error != OK) {
		return error;
	}
	return r_batches.push_back(batch) ? ERR_OUT_OF_MEMORY : OK;
}

Error FixedFunctionGeometryData::_validate_vertex(const FixedFunctionLightingVertex &p_vertex) {
	const float values[] = { p_vertex.x, p_vertex.y, p_vertex.z, p_vertex.u, p_vertex.v, p_vertex.nx, p_vertex.ny, p_vertex.nz, p_vertex.r, p_vertex.g, p_vertex.b, p_vertex.a };
	for (unsigned int i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		if (Math::is_nan(values[i]) || Math::is_inf(values[i])) {
			return ERR_INVALID_DATA;
		}
	}
	return OK;
}

template <class TVertex>
Error FixedFunctionGeometryData::_partition(const PoolVector<TVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches) {
	const uint32_t count = p_indices.size() ? p_indices.size() : p_vertices.size();
	if (!p_vertices.size() || !count || count % 3 || p_max_vertices < 3 || p_max_vertices > MAX_BATCH_VERTICES || !p_max_primitives) {
		return ERR_INVALID_PARAMETER;
	}
	p_max_primitives = MIN(p_max_primitives, uint32_t(INT32_MAX / (3 * sizeof(uint16_t))));
	const typename PoolVector<TVertex>::Read vertices = p_vertices.read();
	const PoolVector<uint32_t>::Read indices = p_indices.read();
	for (int i = 0; i < p_vertices.size(); i++) {
		const Error error = _validate_vertex(vertices[i]);
		if (error != OK) {
			return error;
		}
	}
	// Four temporary bytes per source vertex avoid a heap node for every remap.
	// Clear only touched entries on flush, rather than scanning the whole source.
	Vector<int32_t> remap;
	Error error = remap.resize(p_vertices.size());
	if (error != OK) {
		return error;
	}
	memset(remap.ptrw(), 0xff, remap.size() * sizeof(int32_t));
	Vector<uint32_t> touched;
	Vector<TVertex> batch_vertices;
	Vector<uint16_t> batch_indices;
	Vector<FFGeometryBatch> batches;
	for (uint32_t offset = 0; offset < count; offset += 3) {
		uint32_t triangle[3];
		uint32_t added = 0;
		for (int corner = 0; corner < 3; corner++) {
			triangle[corner] = p_indices.size() ? indices[offset + corner] : offset + corner;
			if (triangle[corner] >= uint32_t(p_vertices.size())) {
				return ERR_INVALID_DATA;
			}
			bool unique = remap[triangle[corner]] < 0;
			for (int prior = 0; prior < corner; prior++) {
				unique = unique && triangle[prior] != triangle[corner];
			}
			added += unique;
		}
		if (uint32_t(batch_vertices.size()) + added > p_max_vertices || uint32_t(batch_indices.size()) / 3 == p_max_primitives) {
			error = _append_batch(batch_vertices, batch_indices, batches);
			if (error != OK) {
				return error;
			}
			for (int i = 0; i < touched.size(); i++) {
				remap.write[touched[i]] = -1;
			}
			touched.clear();
			batch_vertices.clear();
			batch_indices.clear();
		}
		for (int corner = 0; corner < 3; corner++) {
			const uint32_t source = triangle[corner];
			if (remap[source] < 0) {
				remap.write[source] = batch_vertices.size();
				if (batch_vertices.push_back(vertices[source]) || touched.push_back(source)) {
					return ERR_OUT_OF_MEMORY;
				}
			}
			if (batch_indices.push_back(uint16_t(remap[source]))) {
				return ERR_OUT_OF_MEMORY;
			}
		}
	}
	error = _append_batch(batch_vertices, batch_indices, batches);
	if (error == OK) {
		r_batches = batches;
	}
	return error;
}

Error FixedFunctionGeometryData::partition(const PoolVector<FixedFunctionVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches) {
	return _partition(p_vertices, p_indices, p_max_vertices, p_max_primitives, r_batches);
}

Error FixedFunctionGeometryData::partition(const PoolVector<FixedFunctionLightingVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches) {
	return _partition(p_vertices, p_indices, p_max_vertices, p_max_primitives, r_batches);
}

Error FixedFunctionGeometryData::_append_batch(const Vector<FixedFunctionNormalMapVertex> &p_vertices, const Vector<uint16_t> &p_indices, Vector<FFGeometryBatch> &r_batches) {
	FFGeometryBatch batch;
	Error error = _copy_vertices(p_vertices, batch.normal_vertices);
	if (error == OK) {
		error = _copy_vertices(p_indices, batch.indices);
	}
	if (error != OK) {
		return error;
	}
	return r_batches.push_back(batch) ? ERR_OUT_OF_MEMORY : OK;
}

Error FixedFunctionGeometryData::_validate_vertex(const FixedFunctionNormalMapVertex &p_vertex) {
	if (_validate_vertex(p_vertex.lighting) != OK) {
		return ERR_INVALID_DATA;
	}
	const float values[] = { p_vertex.tx, p_vertex.ty, p_vertex.tz, p_vertex.tw, p_vertex.normal_u, p_vertex.normal_v };
	for (int i = 0; i < 6; i++) {
		if (Math::is_nan(values[i]) || Math::is_inf(values[i])) {
			return ERR_INVALID_DATA;
		}
	}
	return OK;
}

Error FixedFunctionGeometryData::partition(const PoolVector<FixedFunctionNormalMapVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, uint32_t p_max_vertices, uint32_t p_max_primitives, Vector<FFGeometryBatch> &r_batches) {
	return _partition(p_vertices, p_indices, p_max_vertices, p_max_primitives, r_batches);
}
