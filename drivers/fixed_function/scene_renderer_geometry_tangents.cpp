#include "scene_renderer.h"

#include "thirdparty/misc/mikktspace.h"

#include <float.h>

Error FixedFunctionSceneRenderer::_generate_geometry_tangents(PoolVector<FixedFunctionNormalMapVertex> &r_vertices, PoolVector<uint32_t> &r_indices) {
	struct FFTangentCorner {
		float frame[4];
		uint32_t vertex;
		int index;
		bool operator<(const FFTangentCorner &p_other) const {
			if (vertex != p_other.vertex) {
				return vertex < p_other.vertex;
			}
			for (int i = 0; i < 4; i++) {
				if (frame[i] != p_other.frame[i]) {
					return frame[i] < p_other.frame[i];
				}
			}
			return false;
		}
	};
	struct FFMikkMesh {
		const FixedFunctionNormalMapVertex *vertices;
		FFTangentCorner *corners;
		int triangle_count;

		static const FFMikkMesh &get_mesh(const SMikkTSpaceContext *p_context) { return *static_cast<const FFMikkMesh *>(p_context->m_pUserData); }
		const FixedFunctionNormalMapVertex &get_vertex(int p_corner) const { return vertices[corners[p_corner].vertex]; }
		static int get_face_count(const SMikkTSpaceContext *p_context) { return get_mesh(p_context).triangle_count; }
		static int get_face_size(const SMikkTSpaceContext *p_context, int p_face) { return 3; }
		static void get_position(const SMikkTSpaceContext *p_context, float p_output[], int p_face, int p_corner) {
			const FixedFunctionLightingVertex &vertex = get_mesh(p_context).get_vertex(p_face * 3 + p_corner).lighting;
			p_output[0] = vertex.x;
			p_output[1] = vertex.y;
			p_output[2] = vertex.z;
		}
		static void get_normal(const SMikkTSpaceContext *p_context, float p_output[], int p_face, int p_corner) {
			const FixedFunctionLightingVertex &vertex = get_mesh(p_context).get_vertex(p_face * 3 + p_corner).lighting;
			p_output[0] = vertex.nx;
			p_output[1] = vertex.ny;
			p_output[2] = vertex.nz;
		}
		static void get_uv(const SMikkTSpaceContext *p_context, float p_output[], int p_face, int p_corner) {
			const FixedFunctionNormalMapVertex &vertex = get_mesh(p_context).get_vertex(p_face * 3 + p_corner);
			p_output[0] = vertex.normal_u;
			p_output[1] = vertex.normal_v;
		}
		static void set_frame(const SMikkTSpaceContext *p_context, const float p_tangent[], const float p_bitangent[], float p_scale_s, float p_scale_t, tbool p_orientation, int p_face, int p_corner) {
			const FFMikkMesh &mesh = get_mesh(p_context);
			const int corner = p_face * 3 + p_corner;
			const FixedFunctionLightingVertex &vertex = mesh.get_vertex(corner).lighting;
			const Vector3 normal(vertex.nx, vertex.ny, vertex.nz);
			const Vector3 tangent(p_tangent[0], p_tangent[1], p_tangent[2]);
			// Match SurfaceTool's packed tangent sign, including its bitangent reversal.
			const Vector3 bitangent(-p_bitangent[0], -p_bitangent[1], -p_bitangent[2]);
			float *frame = mesh.corners[corner].frame;
			frame[0] = p_tangent[0];
			frame[1] = p_tangent[1];
			frame[2] = p_tangent[2];
			frame[3] = float(normal.cross(tangent).dot(bitangent));
		}
	};

	const bool indexed = r_indices.size() > 0;
	const int count = indexed ? r_indices.size() : r_vertices.size();
	if (!count || count % 3 || count / 3 > MAX_GENERATED_TANGENT_TRIANGLES) {
		return ERR_UNAVAILABLE;
	}
	Vector<FFTangentCorner> corners;
	Error error = corners.resize(count);
	if (error != OK) {
		return error;
	}
	PoolVector<FixedFunctionNormalMapVertex> vertices;
	PoolVector<uint32_t> indices;
	{
		const PoolVector<FixedFunctionNormalMapVertex>::Read input = r_vertices.read();
		const PoolVector<uint32_t>::Read source_indices = r_indices.read();
		FFTangentCorner *frames = corners.ptrw();
		for (int i = 0; i < count; i++) {
			const uint32_t vertex = indexed ? source_indices[i] : uint32_t(i);
			if (vertex >= uint32_t(r_vertices.size())) {
				return ERR_INVALID_DATA;
			}
			const FixedFunctionNormalMapVertex &source = input[vertex];
			const float values[] = { source.lighting.x, source.lighting.y, source.lighting.z, source.normal_u, source.normal_v };
			for (int component = 0; component < 5; component++) {
				// MikkTSpace uses float products and squared lengths internally.
				if (!(Math::abs(values[component]) <= 1.0e8f)) {
					return ERR_UNAVAILABLE;
				}
			}
			const FFTangentCorner corner = { { 0, 0, 0, 0 }, vertex, i };
			frames[i] = corner;
		}
		FFMikkMesh mesh = { input.ptr(), frames, count / 3 };
		bool usable_face = false;
		for (int i = 0; i < count && !usable_face; i += 3) {
			const FixedFunctionNormalMapVertex &a = mesh.get_vertex(i);
			const FixedFunctionNormalMapVertex &b = mesh.get_vertex(i + 1);
			const FixedFunctionNormalMapVertex &c = mesh.get_vertex(i + 2);
			const Vector2 uv_b(b.normal_u - a.normal_u, b.normal_v - a.normal_v);
			const Vector2 uv_c(c.normal_u - a.normal_u, c.normal_v - a.normal_v);
			const Vector3 edge_b(b.lighting.x - a.lighting.x, b.lighting.y - a.lighting.y, b.lighting.z - a.lighting.z);
			const Vector3 edge_c(c.lighting.x - a.lighting.x, c.lighting.y - a.lighting.y, c.lighting.z - a.lighting.z);
			usable_face = Math::abs(uv_b.cross(uv_c)) > FLT_MIN && edge_b.cross(edge_c) != Vector3();
		}
		SMikkTSpaceInterface callbacks = { FFMikkMesh::get_face_count, FFMikkMesh::get_face_size, FFMikkMesh::get_position, FFMikkMesh::get_normal, FFMikkMesh::get_uv, nullptr, FFMikkMesh::set_frame };
		SMikkTSpaceContext context = { &callbacks, &mesh };
		if (!usable_face || !genTangSpaceDefault(&context)) {
			return ERR_UNAVAILABLE;
		}
		for (int i = 0; i < count; i++) {
			float *frame = frames[i].frame;
			const FixedFunctionLightingVertex &vertex = input[frames[i].vertex].lighting;
			const Vector3 normal(vertex.nx, vertex.ny, vertex.nz);
			const Vector3 tangent = _unit_normal(Vector3(frame[0], frame[1], frame[2]));
			if (Math::is_nan(frame[3]) || Math::is_inf(frame[3]) || frame[3] == 0 || normal.cross(tangent).length_squared() < real_t(0.000001)) {
				return ERR_UNAVAILABLE;
			}
			frame[0] = float(tangent.x);
			frame[1] = float(tangent.y);
			frame[2] = float(tangent.z);
			frame[3] = frame[3] < 0 ? -1 : 1;
		}
		if (indexed) {
			// Reuse a source vertex only where its complete tangent frame agrees.
			// Remap by original corner so triangle order and winding stay intact.
			corners.sort();
			frames = corners.ptrw();
		}
		int vertex_count = 1;
		for (int i = 1; i < count; i++) {
			vertex_count += frames[i - 1] < frames[i];
		}
		error = vertices.resize(vertex_count);
		if (error == OK && indexed) {
			error = indices.resize(count);
		}
		if (error != OK) {
			return error;
		}
		PoolVector<FixedFunctionNormalMapVertex>::Write output = vertices.write();
		PoolVector<uint32_t>::Write output_indices = indices.write();
		int packed_index = -1;
		for (int i = 0; i < count; i++) {
			const FFTangentCorner &corner = frames[i];
			if (i == 0 || frames[i - 1] < corner) {
				FixedFunctionNormalMapVertex &vertex = output[++packed_index];
				vertex = input[corner.vertex];
				vertex.tx = corner.frame[0];
				vertex.ty = corner.frame[1];
				vertex.tz = corner.frame[2];
				vertex.tw = corner.frame[3];
			}
			if (indexed) {
				output_indices[corner.index] = packed_index;
			}
		}
	}
	r_vertices = vertices;
	r_indices = indices;
	return OK;
}
