#include "drivers/fixed_function/canvas_item_renderer.h"

Error FixedFunctionCanvasItemRenderer::_draw_mesh(RID p_mesh, RID p_multimesh, RID p_texture, const Transform2D &p_transform, const Color &p_modulate, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, RID p_mesh_instance, const Transform2D &p_canvas_transform) {
	const FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	const int copies = p_multimesh.is_valid() ? meshes.multimesh_get_visible_count(p_multimesh) : 1;
	if (!copies || !meshes.owns_mesh(p_mesh)) {
		return OK;
	}
	const uint32_t batch_size = MIN(uint32_t(128), _context.get_device().get_capabilities().max_primitives_per_draw) * 3;
	if (!batch_size) {
		return ERR_UNAVAILABLE;
	}
	Error error = _vertices.resize(batch_size);
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
		Vector<FixedFunctionMeshStorage::FFDeformedVertex> deformed;
		if (p_mesh_instance.is_valid() && p_canvas_transform.determinant() != 0) {
			const Transform2D item_transform = p_canvas_transform.affine_inverse() * p_transform;
			AABB bounds;
			error = meshes.mesh_instance_deform_2d(p_mesh_instance, surface, deformed, bounds, &item_transform);
			if (error != OK) {
				return error;
			}
		}
		const uint32_t triangles = FixedFunctionMeshStorage::surface_get_triangle_count(surface);
		const uint32_t corners = surface.source.index_count ? surface.source.index_count : surface.source.vertex_count;
		if (surface.source.primitive == RSE::PRIMITIVE_TRIANGLES && corners % 3) {
			return ERR_INVALID_PARAMETER;
		}
		const bool strip = surface.source.primitive == RSE::PRIMITIVE_TRIANGLE_STRIP;
		const uint8_t *vertices = surface.source.vertex_data.ptr();
		const uint8_t *indices = surface.source.index_data.ptr();
		for (int copy = 0; copy < copies && triangles; copy++) {
			Transform3D instance;
			Color tint = p_modulate;
			if (p_multimesh.is_valid()) {
				instance = meshes.multimesh_get_transform(p_multimesh, copy);
				if (instance.basis == Basis(0, 0, 0, 0, 0, 0, 0, 0, 0)) {
					continue;
				}
				tint *= meshes.multimesh_get_channel(p_multimesh, copy, false);
			}
			for (uint32_t triangle = 0; triangle < triangles; triangle++) {
				uint32_t corners_in_triangle[3];
				if (strip) {
					FixedFunctionMeshStorage::surface_expand_triangle_strip(surface, indices, triangle, 1, corners_in_triangle);
				} else {
					for (int corner = 0; corner < 3; corner++) {
						corners_in_triangle[corner] = FixedFunctionMeshStorage::surface_read_index(surface, indices, triangle * 3 + corner);
					}
				}
				for (uint32_t index : corners_in_triangle) {
					const Vector3 position = instance.xform(deformed.is_empty() ? FixedFunctionMeshStorage::surface_get_position(surface, vertices, index) : deformed[index].position);
					const Vector2 uv = FixedFunctionMeshStorage::surface_read_uv(surface, index, 0);
					const Color color = tint * FixedFunctionMeshStorage::surface_read_color(surface, index);
					error = _pack_vertex(batch[used++], _snap_position(p_transform.xform(Vector2(position.x, position.y))), uv, color);
					if (error != OK) {
						return error;
					}
				}
				if (used == batch_size) {
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
