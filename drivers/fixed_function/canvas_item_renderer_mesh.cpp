#include "canvas_item_renderer.h"

Error FixedFunctionCanvasItemRenderer::_draw_mesh(RID p_mesh, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, const FixedFunctionMeshStorage::FFMultiMeshInstance *p_copies, int p_count, bool p_2d) {
	const FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	if (!p_count || !meshes.owns_mesh(p_mesh)) {
		return OK;
	}
	const uint32_t batch_triangles = MIN(uint32_t(128), _context.get_device().get_capabilities().max_primitives_per_draw);
	if (!batch_triangles) {
		return ERR_UNAVAILABLE;
	}
	Error error = _vertices.resize(batch_triangles * 3);
	if (error != OK) {
		return error;
	}
	FixedFunctionVertex *batch = _vertices.ptrw();
	uint32_t used = 0;
	for (int i = 0; i < meshes.mesh_get_surface_count(p_mesh); i++) {
		FixedFunctionMeshStorage::FFSurfaceData surface;
		error = meshes.mesh_surface_get_data(p_mesh, i, surface);
		if (error != OK) {
			return error;
		}
		const uint32_t triangles = FixedFunctionMeshStorage::surface_get_triangle_count(surface);
		const int count = surface.index_count ? surface.index_count : surface.vertex_count;
		if (surface.primitive == VS::PRIMITIVE_TRIANGLES && count % 3) {
			return ERR_INVALID_PARAMETER;
		}
		if (!triangles) {
			continue;
		}
		const PoolVector<uint8_t>::Read source = surface.array.read();
		const PoolVector<uint8_t>::Read indices = surface.index_array.read();
		const bool has_color = surface.format & VS::ARRAY_FORMAT_COLOR;
		for (int copy = 0; copy < p_count; copy++) {
			const FixedFunctionMeshStorage::FFMultiMeshInstance *instance = p_copies ? &p_copies[copy] : nullptr;
			if (instance && FixedFunctionMeshStorage::multimesh_is_collapsed(instance->transform, p_2d)) {
				continue;
			}
			const Color tint = instance ? p_modulate * instance->color : p_modulate;
			for (uint32_t triangle = 0; triangle < triangles; triangle++) {
				uint32_t corners[3];
				FixedFunctionMeshStorage::surface_expand_triangles(surface, indices.ptr(), triangle, 1, corners);
				for (int corner = 0; corner < 3; corner++) {
					const uint32_t index = corners[corner];
					Vector3 position = FixedFunctionMeshStorage::surface_read_position(surface, source.ptr(), index);
					if (instance) {
						position = instance->transform.xform(position);
					}
					const Vector2 uv = FixedFunctionMeshStorage::surface_read_uv(surface, source.ptr(), index, 0);
					const Color color = has_color ? tint * FixedFunctionMeshStorage::surface_read_color(surface, source.ptr(), index) : tint;
					const Vector2 canvas_position = _snap_position(p_transform.xform(Vector2(position.x, position.y)), _pixel_snap_enabled);
					error = _pack_vertex(batch[used++], canvas_position, uv, color);
					if (error != OK) {
						return error;
					}
				}
				if (used == batch_triangles * 3) {
					error = _submit(batch, used, p_texture, p_state);
					if (error != OK) {
						return error;
					}
					used = 0;
				}
			}
		}
	}
	return used ? _submit(batch, used, p_texture, p_state) : OK;
}

Error FixedFunctionCanvasItemRenderer::_draw_multimesh(const RasterizerCanvas::Item::CommandMultiMesh &p_command, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state) {
	const FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	if (!meshes.owns_multimesh(p_command.multimesh)) {
		return OK;
	}
	RID mesh;
	Vector<FixedFunctionMeshStorage::FFMultiMeshInstance> copies;
	int visible;
	const Error error = meshes.multimesh_get_draw_data(p_command.multimesh, mesh, copies, visible);
	if (error != OK) {
		return error;
	}
	const bool is_2d = meshes.multimesh_get_transform_format(p_command.multimesh) == VS::MULTIMESH_TRANSFORM_2D;
	return _draw_mesh(mesh, p_command.texture, p_transform, p_modulate, p_state, copies.ptr(), visible, is_2d);
}
