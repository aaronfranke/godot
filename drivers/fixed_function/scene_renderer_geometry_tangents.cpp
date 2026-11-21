#include "drivers/fixed_function/scene_renderer.h"
#include "scene/resources/surface_tool.h"

#include <float.h>

Error FixedFunctionSceneRenderer::_generate_geometry_tangents(Vector<FixedFunctionNormalMapVertex> &r_vertices, Vector<uint32_t> &r_indices) {
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

	const bool indexed = r_indices.size() > 0;
	const int64_t corner_count = indexed ? r_indices.size() : r_vertices.size();
	if (!SurfaceTool::generate_tangents_func || !corner_count || corner_count % 3 ||
			corner_count / 3 > MAX_GENERATED_TANGENT_TRIANGLES || r_vertices.size() > MAX_GENERATED_TANGENT_TRIANGLES * 3) {
		return ERR_UNAVAILABLE;
	}
	const int count = int(corner_count);
	Vector<float> tangents;
	Error error = tangents.resize(count * 4);
	if (error != OK) {
		return error;
	}
	Vector<FFTangentCorner> corners;
	error = corners.resize(count);
	if (error != OK) {
		return error;
	}
	Vector<FixedFunctionNormalMapVertex> vertices;
	Vector<uint32_t> indices;
	{
		const FixedFunctionNormalMapVertex *input = r_vertices.ptr();
		const uint32_t *source_indices = r_indices.ptr();
		FFTangentCorner *frames = corners.ptrw();
		for (int64_t i = 0; i < r_vertices.size(); i++) {
			const FixedFunctionNormalMapVertex &source = input[i];
			const float values[] = { source.lighting.x, source.lighting.y, source.lighting.z, source.normal_u, source.normal_v };
			for (float value : values) {
				if (!(Math::abs(value) <= 1.0e8f)) {
					return ERR_UNAVAILABLE;
				}
			}
		}
		for (int i = 0; i < count; i++) {
			const uint32_t vertex = indexed ? source_indices[i] : uint32_t(i);
			if (vertex >= uint32_t(r_vertices.size())) {
				return ERR_INVALID_DATA;
			}
			const FFTangentCorner corner = { { 0, 0, 0, 0 }, vertex, i };
			frames[i] = corner;
		}
		bool usable_face = false;
		for (int i = 0; i < count && !usable_face; i += 3) {
			const FixedFunctionNormalMapVertex &a = input[frames[i].vertex];
			const FixedFunctionNormalMapVertex &b = input[frames[i + 1].vertex];
			const FixedFunctionNormalMapVertex &c = input[frames[i + 2].vertex];
			const Vector2 uv_b(b.normal_u - a.normal_u, b.normal_v - a.normal_v);
			const Vector2 uv_c(c.normal_u - a.normal_u, c.normal_v - a.normal_v);
			const Vector3 edge_b(b.lighting.x - a.lighting.x, b.lighting.y - a.lighting.y, b.lighting.z - a.lighting.z);
			const Vector3 edge_c(c.lighting.x - a.lighting.x, c.lighting.y - a.lighting.y, c.lighting.z - a.lighting.z);
			usable_face = Math::abs(uv_b.cross(uv_c)) > FLT_MIN && edge_b.cross(edge_c) != Vector3();
		}
		if (!usable_face) {
			return ERR_UNAVAILABLE;
		}
		const size_t stride = sizeof(FixedFunctionNormalMapVertex);
		SurfaceTool::generate_tangents_func(tangents.ptrw(), indexed ? source_indices : nullptr, count,
				&input[0].lighting.x, r_vertices.size(), stride, &input[0].lighting.nx, stride,
				&input[0].normal_u, stride, 0);
		const float *generated = tangents.ptr();
		for (int i = 0; i < count; i++) {
			float *frame = frames[i].frame;
			for (int component = 0; component < 4; component++) {
				frame[component] = generated[i * 4 + component];
			}
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
		FixedFunctionNormalMapVertex *output = vertices.ptrw();
		uint32_t *output_indices = indices.ptrw();
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
