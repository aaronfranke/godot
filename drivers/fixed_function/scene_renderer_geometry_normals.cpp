#include "scene_renderer.h"

#include <string.h>

bool FixedFunctionSceneRenderer::_geometry_upload_fits(uint64_t p_count, size_t p_stride) {
	// Leave room for power-of-two allocation growth on both vector implementations.
	// Rejection only disables this optional upload copy, never the logical mesh.
	return p_stride && p_count <= INT32_MAX && p_count <= SIZE_MAX / p_stride / 2;
}

Error FixedFunctionSceneRenderer::_generate_geometry_normals(const FFDrawPacket &p_packet, PoolVector<FixedFunctionLightingVertex> &r_vertices, PoolVector<uint32_t> &r_indices) {
	const int count = r_indices.size() ? r_indices.size() : r_vertices.size();
	if (r_vertices.size() != p_packet.surface.vertex_count || !count || count % 3 || !p_packet.surface.array.size()) {
		return ERR_INVALID_PARAMETER;
	}
	if (!_geometry_upload_fits(r_vertices.size(), sizeof(int32_t)) || !_geometry_upload_fits(count, sizeof(uint32_t))) {
		return ERR_OUT_OF_MEMORY;
	}
	Vector<int32_t> shared;
	Vector<FixedFunctionLightingVertex> vertices;
	Vector<uint32_t> indices;
	Error error = shared.resize(r_vertices.size());
	if (error == OK) {
		error = indices.resize(count);
	}
	if (error != OK) {
		return error;
	}
	memset(shared.ptrw(), 0xff, shared.size() * sizeof(int32_t));
	{
		const PoolVector<FixedFunctionLightingVertex>::Read source = r_vertices.read();
		const PoolVector<uint32_t>::Read source_indices = r_indices.read();
		const PoolVector<uint8_t>::Read data = p_packet.surface.array.read();
		uint32_t *output_indices = indices.ptrw();
		for (int offset = 0; offset < count; offset += 3) {
			uint32_t triangle[3];
			bool missing[3];
			for (int corner = 0; corner < 3; corner++) {
				triangle[corner] = r_indices.size() ? source_indices[offset + corner] : offset + corner;
				if (triangle[corner] >= uint32_t(r_vertices.size())) {
					return ERR_INVALID_DATA;
				}
				const FixedFunctionLightingVertex &vertex = source[triangle[corner]];
				missing[corner] = vertex.nx == 0 && vertex.ny == 0 && vertex.nz == 0;
			}
			Vector3 face;
			if (missing[0] || missing[1] || missing[2]) {
				Vector3 positions[3];
				for (int corner = 0; corner < 3; corner++) {
					FFSpatialVertex unused;
					error = _read_vertex(p_packet, data.ptr(), triangle[corner], unused, &positions[corner]);
					if (error != OK) {
						return error;
					}
				}
				// Derive clockwise face directions from canonical real_t positions,
				// before anchoring/float packing. Degenerate faces retain zero normals.
				const Vector3 edge1 = _unit_normal(positions[1] - positions[0]);
				const Vector3 edge2 = _unit_normal(positions[2] - positions[0]);
				face = _unit_normal(edge2.cross(edge1));
			}
			for (int corner = 0; corner < 3; corner++) {
				const uint32_t original = triangle[corner];
				if (!missing[corner] && shared[original] >= 0) {
					output_indices[offset + corner] = shared[original];
					continue;
				}
				if (!_geometry_upload_fits(uint64_t(vertices.size()) + 1, sizeof(FixedFunctionLightingVertex))) {
					return ERR_OUT_OF_MEMORY;
				}
				FixedFunctionLightingVertex vertex = source[original];
				if (missing[corner]) {
					// A source vertex may border different faces. Split only its missing
					// corners; do not invent smoothing or weld distinct source identities.
					vertex.nx = float(face.x);
					vertex.ny = float(face.y);
					vertex.nz = float(face.z);
				} else {
					shared.write[original] = vertices.size();
				}
				output_indices[offset + corner] = vertices.size();
				if (vertices.push_back(vertex)) {
					return ERR_OUT_OF_MEMORY;
				}
			}
		}
	}
	PoolVector<FixedFunctionLightingVertex> generated_vertices;
	PoolVector<uint32_t> generated_indices;
	error = generated_vertices.resize(vertices.size());
	if (error == OK) {
		error = generated_indices.resize(indices.size());
	}
	if (error != OK) {
		return error;
	}
	{
		PoolVector<FixedFunctionLightingVertex>::Write output = generated_vertices.write();
		PoolVector<uint32_t>::Write output_indices = generated_indices.write();
		memcpy(output.ptr(), vertices.ptr(), vertices.size() * sizeof(FixedFunctionLightingVertex));
		memcpy(output_indices.ptr(), indices.ptr(), indices.size() * sizeof(uint32_t));
	}
	r_vertices = generated_vertices;
	r_indices = generated_indices;
	return OK;
}
