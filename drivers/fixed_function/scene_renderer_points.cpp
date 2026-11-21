#include "drivers/fixed_function/scene_renderer.h"

Error FixedFunctionSceneRenderer::_submit_points(const FFDrawPacket &p_packet, const Projection &p_projection, RID p_texture, RenderingServerTypes::RenderInfo *r_info) {
	const FixedFunctionGraphicsDevice &device = _context.get_device();
	const Size2i size(device.get_target_width(), device.get_target_height());
	if (size.x <= 0 || size.y <= 0) {
		return ERR_INVALID_PARAMETER;
	}
	const real_t point_size = MAX(p_packet.material.point_size, 1.0f);
	const Vector2 half_size(point_size / size.x, point_size / size.y);
	const bool point_uv = p_packet.material.albedo.uv_set == FixedFunctionSpatialMaterial::UV_POINT_COORD;
	const uint8_t *source = p_packet.surface.source.vertex_data.ptr();
	const uint8_t *indices = p_packet.surface.source.index_data.ptr();
	const uint32_t count = p_packet.surface.source.index_count ? p_packet.surface.source.index_count : p_packet.surface.source.vertex_count;
	FixedFunctionGraphicsDevice::FFDrawParameters state = p_packet.material.state;
	state.cull_mode = FixedFunctionGraphicsDevice::CULL_DISABLED;
	FixedFunctionVertex *batch = _vertices.ptrw();
	uint32_t used = 0;
	bool first_surface = true;
	for (uint32_t i = 0; i < count; i++) {
		const uint32_t index = FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices, i);
		FFSpatialVertex point;
		Error error = _read_vertex(p_packet, source, index, point);
		if (error != OK) {
			return error;
		}
		const Vector4 clip = p_projection.xform(Vector4(point.position.x, point.position.y, point.position.z, 1));
		if (Math::is_nan(clip.w) || Math::is_inf(clip.w)) {
			return ERR_INVALID_PARAMETER;
		}
		bool visible = clip.w > 0;
		for (int axis = 0; axis < 3; axis++) {
			const real_t component = clip[axis];
			if (Math::is_nan(component) || Math::is_inf(component)) {
				return ERR_INVALID_PARAMETER;
			}
			visible &= Math::abs(component) <= clip.w;
		}
		if (!visible) {
			continue;
		}
		if (!(p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED)) {
			const Vector3 normal = _unit_normal(p_packet.normal_from_model.xform(_draw_normal(p_packet, source, index)));
			_shade_vertex(p_packet, normal, point);
		}
		// Clip the expanded rectangle to the viewport while keeping the original
		// point coordinates. Even a very large point only generates four corners.
		const Vector2 center(clip.x / clip.w, clip.y / clip.w);
		const real_t left = MAX(-half_size.x, -1 - center.x);
		const real_t right = MIN(half_size.x, 1 - center.x);
		const real_t top = MIN(half_size.y, 1 - center.y);
		const real_t bottom = MAX(-half_size.y, -1 - center.y);
		const Vector2 offsets[] = { Vector2(left, top), Vector2(right, top), Vector2(right, bottom), Vector2(left, bottom) };
		FixedFunctionVertex quad[4];
		for (int corner = 0; corner < 4; corner++) {
			FFSpatialVertex vertex = point;
			error = _offset_projected_vertex(point.position, clip, p_projection, offsets[corner], vertex.position);
			if (error != OK) {
				return error;
			}
			if (point_uv) {
				const Vector2 uv(0.5 + 0.5 * (offsets[corner].x / half_size.x), 0.5 - 0.5 * (offsets[corner].y / half_size.y));
				vertex.uv = uv * p_packet.material.albedo.uv_scale + p_packet.material.albedo.uv_offset;
				if (p_packet.flip_v) {
					vertex.uv.y = 1 - vertex.uv.y;
				}
			}
			error = _pack_vertex(vertex, quad[corner]);
			if (error != OK) {
				return error;
			}
		}
		const int corners[] = { 0, 1, 2, 0, 2, 3 };
		for (int corner : corners) {
			batch[used++] = quad[corner];
			if (used == uint32_t(_vertices.size())) {
				error = _submit_batch(p_packet, p_projection, p_texture, state, used, first_surface, r_info);
				if (error != OK) {
					return error;
				}
				used = 0;
			}
		}
	}
	return used ? _submit_batch(p_packet, p_projection, p_texture, state, used, first_surface, r_info) : OK;
}
