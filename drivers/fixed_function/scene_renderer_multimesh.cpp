#include "scene_renderer.h"

#include <cmath>
#include <limits>

Error FixedFunctionSceneRenderer::_prepare_multimesh_light_spheres(FFMultimeshLights &r_lights, const Vector3 &p_camera_position) const {
	FixedFunctionLightStorage &lights = _context.get_light_storage();
	for (int i = 0; i < r_lights.source_count; i++) {
		FixedFunctionLightStorage::FFLightInstanceData instance;
		Error error = lights.light_instance_get_data(r_lights.sources[i], instance);
		if (error != OK) {
			return error;
		}
		if (!instance.light.is_valid()) {
			continue;
		}
		FixedFunctionLightStorage::FFLightData light;
		error = lights.light_get_data(instance.light, light);
		if (error != OK) {
			return error;
		}
		const real_t range = light.parameters[VS::LIGHT_PARAM_RANGE];
		if (light.type == VS::LIGHT_DIRECTIONAL || range <= 0) {
			continue;
		}
		if (r_lights.spheres.push_back({ instance.transform.origin - p_camera_position, range, light.cull_mask })) {
			return ERR_OUT_OF_MEMORY;
		}
	}
	return OK;
}

bool FixedFunctionSceneRenderer::_prepare_multimesh_lighting(FFDrawPacket &r_packet, const Transform &p_world, const Transform &p_view, const FFSceneOptions &p_options, bool p_pixel) const {
	FFMultimeshLights &lights = p_options.multimesh_lights;
	if (lights.status == ERR_SKIP) {
		lights.status = _prepare_multimesh_light_spheres(lights, p_options.camera.origin);
	}
	if (lights.status != OK) {
		return false;
	}
	FixedFunctionMeshStorage::FFGeometryBounds bounds;
	bool bounds_ready = false;
	const FFLocalLightSphere *spheres = lights.spheres.ptr();
	for (int i = 0; i < lights.spheres.size(); i++) {
		const FFLocalLightSphere &light = spheres[i];
		if (!(r_packet.layer_mask & light.cull_mask)) {
			continue;
		}
		if (!bounds_ready) {
			// Round each stage outwards, including cancellation from translations.
			if (!_context.get_mesh_storage().multimesh_get_geometry_bounds(r_packet.multimesh, r_packet.surface, bounds) ||
					!bounds.transform(p_world) || !bounds.transform(p_view) || !bounds.transform(Transform(p_options.camera.basis, Vector3()))) {
				return false;
			}
			bounds_ready = true;
		}
		for (int axis = 0; axis < 3; axis++) {
			if (Math::is_nan(light.position[axis]) || Math::is_inf(light.position[axis])) {
				return false;
			}
		}
		if (bounds.range_distance_squared(light.position, light.range) > 1) {
			continue;
		}
		if (!p_pixel) {
			return false;
		}
		// Per-copy shader selection uses the authored box. A region edit can
		// move vertices outside it, where a shared light list would change behavior.
		const AABB &source = r_packet.surface.aabb;
		const Vector3 source_end = source.position + source.size;
		const real_t infinity = std::numeric_limits<real_t>::infinity();
		for (int axis = 0; axis < 3; axis++) {
			const real_t source_min = std::nextafter(source.position[axis], -infinity);
			const real_t source_max = std::nextafter(source_end[axis], infinity);
			if (r_packet.surface.position_min[axis] < source_min || r_packet.surface.position_max[axis] > source_max ||
					bounds.minimum[axis] < -1.0e10 || bounds.maximum[axis] > 1.0e10) {
				return false;
			}
			const real_t minimum = std::nextafter(real_t(bounds.minimum[axis]), -infinity);
			const real_t maximum = std::nextafter(real_t(bounds.maximum[axis]), infinity);
			r_packet.multimesh_light_bounds.position[axis] = minimum;
			r_packet.multimesh_light_bounds.size[axis] = std::nextafter(maximum - minimum, infinity);
		}
		r_packet.multimesh_local_lighting = true;
		break;
	}
	return true;
}

bool FixedFunctionSceneRenderer::_request_multimesh_geometry(FFDrawPacket &r_packet, const Transform &p_world, const Transform &p_view, const FFSceneOptions &p_options) {
	const FixedFunctionSpatialMaterial::FFMaterialParameters &material = r_packet.material;
	const bool lit = !(material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED);
	if (r_packet.late || !material.state.depth_write_enabled || material.is_fallback || material.transform_flags ||
			material.albedo_uses_vertex_color || material.albedo_uses_vertex_alpha || material.albedo.uv_set < 0 || material.albedo.uv_set > 1 ||
			r_packet.occlusion || r_packet.normal_mapping || material.emission.energy > 0 ||
			r_packet.surface.primitive != VS::PRIMITIVE_TRIANGLES) {
		return false;
	}
	if (lit) {
		const bool pixel = _per_pixel_lighting && !(material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_VERTEX) &&
				_context.get_device().supports_lit_geometry() && _context.get_device().get_max_per_pixel_lights();
		if (!(r_packet.surface.format & VS::ARRAY_FORMAT_NORMAL) || (!pixel && !_uses_vertex_lit_geometry(r_packet)) ||
				!_prepare_multimesh_lighting(r_packet, p_world, p_view, p_options, pixel)) {
			return false;
		}
	} else if (r_packet.flip_v || material.albedo.uv_scale != Vector2(1, 1) || material.albedo.uv_offset != Vector2()) {
		return false;
	}

	FixedFunctionGeometryCache &cache = _context.get_geometry_cache();
	RID geometry;
	FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
	Vector3 anchor, extent;
	const Error cached = cache.get(r_packet.surface.identity, r_packet.surface.version, material.albedo.uv_set, geometry, info, anchor, extent, lit, -1, false, nullptr, r_packet.multimesh);
	if (cached != ERR_DOES_NOT_EXIST) {
		return cached == OK;
	}
	PoolVector<FixedFunctionVertex> vertices;
	PoolVector<FixedFunctionLightingVertex> lit_vertices;
	PoolVector<uint32_t> indices;
	const Error built = _build_multimesh_geometry(r_packet, vertices, lit_vertices, indices, anchor, extent);
	if (lit) {
		cache.request(r_packet.surface, material.albedo.uv_set, lit_vertices, indices, anchor, extent, built, false, nullptr, r_packet.multimesh);
	} else {
		cache.request(r_packet.surface, material.albedo.uv_set, vertices, indices, anchor, extent, built, r_packet.multimesh);
	}
	return false; // Draw the individual copies until the idle upload completes.
}

Error FixedFunctionSceneRenderer::_build_multimesh_geometry(const FFDrawPacket &p_packet, PoolVector<FixedFunctionVertex> &r_vertices, PoolVector<FixedFunctionLightingVertex> &r_lit_vertices, PoolVector<uint32_t> &r_indices, Vector3 &r_anchor, Vector3 &r_extent) {
	// Bound the retained expansion. The device further partitions it by its native index and draw limits.
	enum {
		MAX_VERTICES = 262144,
		MAX_INDICES = 393216,
	};
	const FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	const FixedFunctionMeshStorage::FFSurfaceData &surface = p_packet.surface;
	const uint32_t vertices = surface.vertex_count;
	const uint32_t indices = surface.index_count ? surface.index_count : vertices;
	RID mesh;
	Vector<FixedFunctionMeshStorage::FFMultiMeshInstance> copies;
	int visible;
	if (meshes.multimesh_get_draw_data(p_packet.multimesh, mesh, copies, visible) != OK || !mesh.is_valid() || !vertices || !indices || !visible ||
			uint64_t(vertices) * visible > MAX_VERTICES || uint64_t(indices) * visible > MAX_INDICES) {
		return ERR_UNAVAILABLE;
	}
	const bool lit = !(p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED);
	AABB bounds;
	int count = 0;
	for (int i = 0; i < visible; i++) {
		const Transform &copy = copies[i].transform;
		if (FixedFunctionMeshStorage::multimesh_is_collapsed(copy, false)) {
			continue;
		}
		// Keep lit reflections on the per-copy path, where native lighting sees their model transform.
		if (!_finite_transform(copy) || (lit && (copy.basis.determinant() <= 0 || !_uniform_basis_scale_squared(copy.basis)))) {
			return ERR_UNAVAILABLE;
		}
		const AABB copy_bounds = copy.xform(surface.aabb);
		bounds = count ? bounds.merge(copy_bounds) : copy_bounds;
		count++;
	}
	if (!count) {
		return ERR_UNAVAILABLE;
	}
	r_anchor = bounds.get_center();
	r_extent = Vector3();
	const Error allocation = lit ? r_lit_vertices.resize(count * vertices) : r_vertices.resize(count * vertices);
	if (allocation != OK || r_indices.resize(count * indices) != OK) {
		return ERR_OUT_OF_MEMORY;
	}

	// Decode the shared surface once. Copy positions into MultiMesh-local coordinates
	// around the anchor so moving the camera or the MultiMesh node does not rebuild it.
	FFDrawPacket upload = p_packet;
	upload.view_from_model = Transform();
	upload.material.albedo.factor = Color(1, 1, 1, 1);
	upload.material.albedo.uv_scale = Vector2(1, 1);
	upload.material.albedo.uv_offset = Vector2();
	upload.flip_v = false;
	Vector<FFSpatialVertex> source_vertices;
	Vector<Vector3> source_normals;
	if (source_vertices.resize(vertices) != OK || (lit && source_normals.resize(vertices) != OK)) {
		return ERR_OUT_OF_MEMORY;
	}
	const PoolVector<uint8_t>::Read source = surface.array.read();
	for (uint32_t i = 0; i < vertices; i++) {
		const Error error = _read_vertex(upload, source.ptr(), i, source_vertices.write[i]);
		if (error != OK) {
			return error;
		}
		if (lit) {
			const Vector3 normal = _read_normal(surface, source.ptr(), i);
			if (normal == Vector3()) {
				return ERR_UNAVAILABLE;
			}
			source_normals.write[i] = normal;
		}
	}
	PoolVector<FixedFunctionVertex>::Write vertex_write = r_vertices.write();
	PoolVector<FixedFunctionLightingVertex>::Write lit_vertex_write = r_lit_vertices.write();
	PoolVector<uint32_t>::Write index_write = r_indices.write();
	const PoolVector<uint8_t>::Read source_indices = surface.index_array.read();
	FixedFunctionVertex *out = vertex_write.ptr();
	FixedFunctionLightingVertex *lit_out = lit_vertex_write.ptr();
	uint32_t *index_out = index_write.ptr();
	uint32_t vertex_offset = 0;
	for (int i = 0; i < visible; i++) {
		Transform copy = copies[i].transform;
		if (FixedFunctionMeshStorage::multimesh_is_collapsed(copy, false)) {
			continue;
		}
		copy.origin -= r_anchor;
		const real_t normal_scale = lit ? real_t(1) / Math::sqrt(copy.basis.get_axis(0).length_squared()) : real_t(1);
		for (uint32_t v = 0; v < vertices; v++) {
			FFSpatialVertex vertex = source_vertices[v];
			vertex.position = copy.xform(vertex.position);
			for (int axis = 0; axis < 3; axis++) {
				const real_t value = Math::abs(vertex.position[axis]);
				if (Math::is_nan(value) || Math::is_inf(value)) {
					return ERR_UNAVAILABLE;
				}
				r_extent[axis] = MAX(r_extent[axis], value);
			}
			const Error error = lit ? _pack_lighting_vertex(vertex, copy.basis.xform(source_normals[v]) * normal_scale, *lit_out++) : _pack_vertex(vertex, *out++);
			if (error != OK) {
				return error;
			}
		}
		for (uint32_t index = 0; index < indices; index++) {
			const uint32_t source_index = surface.index_count ? FixedFunctionMeshStorage::surface_read_index(surface, source_indices.ptr(), index) : index;
			*index_out++ = vertex_offset + source_index;
		}
		vertex_offset += vertices;
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_submit_multimesh_copies(const FFDrawPacket &p_packet, const CameraMatrix &p_projection, RID p_texture, uint32_t p_batch_primitives) {
	// A cached batch can still fail a view-dependent precision or device check.
	// These packets have one opaque pass, so they can reuse the prepared material.
	const FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	RID mesh;
	Vector<FixedFunctionMeshStorage::FFMultiMeshInstance> copies;
	int visible;
	if (meshes.multimesh_get_draw_data(p_packet.multimesh, mesh, copies, visible) != OK || !mesh.is_valid()) {
		return OK;
	}
	FFDrawPacket copy = p_packet;
	copy.multimesh = 0;
	copy.multimesh_local_lighting = false;
	for (int i = 0; i < visible; i++) {
		const Transform &transform = copies[i].transform;
		if (FixedFunctionMeshStorage::multimesh_is_collapsed(transform, false)) {
			continue;
		}
		copy.view_from_model = p_packet.view_from_model * transform;
		if (!_finite_transform(copy.view_from_model)) {
			return ERR_INVALID_PARAMETER;
		}
		copy.normal_from_model = _normal_matrix(_camera_basis * copy.view_from_model.basis);
		if (!(copy.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED)) {
			_filter_local_lights(copy);
		}
		Error error = _request_geometry(copy) ? _submit_geometry(copy, p_projection, p_texture) : ERR_UNAVAILABLE;
		if (error == ERR_SKIP) {
			_redraw_requested = true;
			return _render_targets.defer_render_target();
		}
		if (error == ERR_UNAVAILABLE) {
			error = _submit_triangles(copy, p_projection, p_texture, p_batch_primitives, true);
		}
		if (error != OK) {
			return error;
		}
	}
	return OK;
}
