#include "drivers/fixed_function/scene_renderer.h"

#include <cmath>
#include <limits>

Error FixedFunctionSceneRenderer::_prepare_multimesh_light_spheres(FFMultimeshLights &r_lights, const Vector3 &p_camera_position) const {
	FixedFunctionLightStorage &lights = _context.get_light_storage();
	for (RID light_instance : r_lights.sources) {
		FixedFunctionLightStorage::FFLightInstanceData instance;
		Error error = lights.light_instance_get_data(light_instance, instance);
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
		const real_t range = light.parameters[RSE::LIGHT_PARAM_RANGE];
		if (light.type == RSE::LIGHT_DIRECTIONAL || range <= 0) {
			continue;
		}
		if (r_lights.spheres.push_back({ instance.transform.origin - p_camera_position, range, light.cull_mask })) {
			return ERR_OUT_OF_MEMORY;
		}
	}
	return OK;
}

bool FixedFunctionSceneRenderer::_prepare_multimesh_lighting(FFDrawPacket &r_packet, FFMultimeshLights &r_lights, const Transform3D &p_world, const Transform3D &p_view, const Transform3D &p_camera, bool p_pixel) const {
	if (r_lights.status == ERR_SKIP) {
		r_lights.status = _prepare_multimesh_light_spheres(r_lights, p_camera.origin);
	}
	if (r_lights.status != OK) {
		return false;
	}
	FixedFunctionMeshStorage::FFGeometryBounds bounds;
	bool bounds_ready = false;
	for (const FFLocalLightSphere &light : r_lights.spheres) {
		if (!(r_packet.layer_mask & light.cull_mask)) {
			continue;
		}
		if (!bounds_ready) {
			// Cover both lighting paths in camera-relative world axes. Transform
			// each stage separately so large translations get outward rounding.
			if (!_context.get_mesh_storage().multimesh_get_geometry_bounds(r_packet.multimesh, r_packet.surface, bounds) ||
					!bounds.transform(p_world) || !bounds.transform(p_view) || !bounds.transform(Transform3D(p_camera.basis, Vector3()))) {
				return false;
			}
			bounds_ready = true;
		}
		if (!light.position.is_finite()) {
			return false;
		}
		if (bounds.range_distance_squared(light.position, light.range) > 1) {
			continue;
		}
		if (!p_pixel) {
			return false;
		}
		// Per-copy shader selection uses the authored box. A region edit can
		// move vertices outside it, where a shared light list would change behavior.
		const AABB &source = r_packet.surface.source.aabb;
		const FixedFunctionMeshStorage::FFPositionBounds &positions = r_packet.surface.position_bounds;
		const Vector3 source_end = source.get_end();
		// The pixel shader clips each light to its range. Preserve the union of
		// the copy bounds for light selection, rounding outward to real_t.
		const real_t infinity = std::numeric_limits<real_t>::infinity();
		for (int axis = 0; axis < 3; axis++) {
			const real_t source_min = std::nextafter(source.position[axis], -infinity);
			const real_t source_max = std::nextafter(source_end[axis], infinity);
			if (positions.minimum[axis] < source_min || positions.maximum[axis] > source_max ||
					bounds.minimum[axis] < -1.0e10 || bounds.maximum[axis] > 1.0e10) {
				return false;
			}
			const real_t minimum = std::nextafter(real_t(bounds.minimum[axis]), -infinity);
			const real_t maximum = std::nextafter(real_t(bounds.maximum[axis]), infinity);
			r_packet.draw_bounds.position[axis] = minimum;
			r_packet.draw_bounds.size[axis] = std::nextafter(maximum - minimum, infinity);
		}
		r_packet.multimesh_local_lighting = true;
		break;
	}
	return true;
}

bool FixedFunctionSceneRenderer::_request_multimesh_geometry(FFDrawPacket &r_packet, FFMultimeshLights &r_lights, const Transform3D &p_world, const Transform3D &p_view, const Transform3D &p_camera) {
	const FixedFunctionSpatialMaterial::FFMaterialParameters &material = r_packet.material;
	const bool lit = !(material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED);
	if (r_packet.late || !material.state.depth_write_enabled || material.is_fallback || material.transform_flags ||
			material.albedo_uses_vertex_color || material.albedo_uses_vertex_alpha || material.albedo.uv_set < 0 || material.albedo.uv_set > 1 ||
			r_packet.occlusion || r_packet.normal_mapping || r_packet.emission ||
			r_packet.surface.source.primitive != RSE::PRIMITIVE_TRIANGLES) {
		return false;
	}
	if (lit) {
		const bool pixel = _per_pixel_lighting && !(material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_VERTEX) &&
				_context.get_device().supports_lit_geometry() && _context.get_device().get_max_per_pixel_lights();
		if (!(r_packet.surface.source.format & RSE::ARRAY_FORMAT_NORMAL) || (!pixel && !_uses_vertex_lit_geometry(r_packet)) ||
				!_prepare_multimesh_lighting(r_packet, r_lights, p_world, p_view, p_camera, pixel)) {
			return false;
		}
	} else if (r_packet.flip_v || material.albedo.uv_scale != Vector2(1, 1) || material.albedo.uv_offset != Vector2()) {
		return false;
	}

	FixedFunctionGeometryCache &cache = _context.get_geometry_cache();
	RID geometry;
	FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
	Vector3 anchor, extent;
	const Error cached = cache.get(r_packet.surface.identity, r_packet.surface.version, material.albedo.uv_set, geometry, info, anchor, extent, lit, -1, false, nullptr, r_packet.multimesh, r_packet.lod);
	if (cached != ERR_DOES_NOT_EXIST) {
		return cached == OK;
	}
	Vector<FixedFunctionVertex> vertices;
	Vector<FixedFunctionLightingVertex> lit_vertices;
	Vector<uint32_t> indices;
	const Error built = _build_multimesh_geometry(r_packet, vertices, lit_vertices, indices, anchor, extent);
	if (lit) {
		cache.request(r_packet.surface, material.albedo.uv_set, lit_vertices, indices, anchor, extent, built, false, false, nullptr, r_packet.multimesh, r_packet.lod);
	} else {
		cache.request(r_packet.surface, material.albedo.uv_set, vertices, indices, anchor, extent, built, r_packet.multimesh, r_packet.lod);
	}
	return false; // Draw the individual copies until the idle upload completes.
}

Error FixedFunctionSceneRenderer::_compact_multimesh_lod(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, Vector<uint32_t> &r_source_vertices, Vector<uint32_t> &r_indices) {
	const uint32_t count = p_surface.source.index_count;
	if (r_source_vertices.resize(count) != OK || r_indices.resize(count) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	uint32_t *vertices = r_source_vertices.ptrw();
	uint32_t *indices = r_indices.ptrw();
	const uint8_t *source_indices = p_surface.source.index_data.ptr();
	for (uint32_t i = 0; i < count; i++) {
		const uint32_t source = FixedFunctionMeshStorage::surface_read_index(p_surface, source_indices, i);
		vertices[i] = source;
		indices[i] = source;
	}
	// The temporary map scales with the selected index list, even for a sparse
	// LOD of a large source mesh. Only the lookup is sorted; draw order stays intact.
	r_source_vertices.sort();
	vertices = r_source_vertices.ptrw();
	uint32_t unique = 0;
	for (uint32_t i = 0; i < count; i++) {
		if (!unique || vertices[i] != vertices[unique - 1]) {
			vertices[unique++] = vertices[i];
		}
	}
	if (r_source_vertices.resize(unique) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	for (uint32_t i = 0; i < count; i++) {
		indices[i] = uint32_t(r_source_vertices.bsearch(indices[i], true));
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_build_multimesh_geometry(const FFDrawPacket &p_packet, Vector<FixedFunctionVertex> &r_vertices, Vector<FixedFunctionLightingVertex> &r_lit_vertices, Vector<uint32_t> &r_indices, Vector3 &r_anchor, Vector3 &r_extent) {
	// Bound the retained expansion. The device further partitions it by its native index and draw limits.
	enum { MAX_VERTICES = 262144,
		MAX_INDICES = 393216 };
	const FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	const FixedFunctionMeshStorage::FFSurfaceData &surface = p_packet.surface;
	uint32_t vertices = surface.source.vertex_count;
	const uint32_t indices = surface.source.index_count ? surface.source.index_count : vertices;
	const int visible = meshes.multimesh_get_visible_count(p_packet.multimesh);
	if (!vertices || !indices || !visible || uint64_t(indices) * visible > MAX_INDICES) {
		return ERR_UNAVAILABLE;
	}
	Vector<uint32_t> source_vertex_indices, compact_index_data;
	if (p_packet.lod) {
		const Error error = _compact_multimesh_lod(surface, source_vertex_indices, compact_index_data);
		if (error != OK) {
			return error;
		}
		vertices = source_vertex_indices.size();
	}
	if (uint64_t(vertices) * visible > MAX_VERTICES) {
		return ERR_UNAVAILABLE;
	}
	const bool lit = !(p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED);
	Vector<Transform3D> copies;
	if (copies.resize(visible) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	AABB bounds;
	int count = 0;
	for (int i = 0; i < visible; i++) {
		const Transform3D copy = meshes.multimesh_get_transform(p_packet.multimesh, i);
		if (copy.basis == Basis(0, 0, 0, 0, 0, 0, 0, 0, 0)) {
			continue;
		}
		// Keep lit reflections on the per-copy path, where native lighting sees their model transform.
		if (!_finite_transform(copy) || (lit && (copy.basis.determinant() <= 0 || !_uniform_basis_scale_squared(copy.basis)))) {
			return ERR_UNAVAILABLE;
		}
		const AABB copy_bounds = copy.xform(surface.source.aabb);
		bounds = count ? bounds.merge(copy_bounds) : copy_bounds;
		copies.write[count++] = copy;
	}
	if (!count || !bounds.is_finite()) {
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
	upload.view_from_model = Transform3D();
	upload.material.albedo.factor = Color(1, 1, 1, 1);
	upload.material.albedo.uv_scale = Vector2(1, 1);
	upload.material.albedo.uv_offset = Vector2();
	upload.flip_v = false;
	Vector<FFSpatialVertex> source_vertices;
	Vector<Vector3> source_normals;
	if (source_vertices.resize(vertices) != OK || (lit && source_normals.resize(vertices) != OK)) {
		return ERR_OUT_OF_MEMORY;
	}
	for (uint32_t i = 0; i < vertices; i++) {
		const uint32_t source = p_packet.lod ? source_vertex_indices[i] : i;
		const Error error = _read_vertex(upload, surface.source.vertex_data.ptr(), source, source_vertices.write[i]);
		if (error != OK) {
			return error;
		}
		if (lit) {
			const Vector3 normal = _unit_normal(FixedFunctionMeshStorage::surface_get_normal(surface, surface.source.vertex_data.ptr(), source));
			if (normal == Vector3()) {
				return ERR_UNAVAILABLE;
			}
			source_normals.write[i] = normal;
		}
	}
	FixedFunctionVertex *out = r_vertices.ptrw();
	FixedFunctionLightingVertex *lit_out = r_lit_vertices.ptrw();
	uint32_t *index_out = r_indices.ptrw();
	const uint8_t *source_indices = surface.source.index_data.ptr();
	const uint32_t *compact_indices = compact_index_data.ptr();
	for (int i = 0; i < count; i++) {
		Transform3D copy = copies[i];
		copy.origin -= r_anchor;
		const real_t normal_scale = lit ? real_t(1) / Math::sqrt(copy.basis.get_column(0).length_squared()) : real_t(1);
		for (uint32_t v = 0; v < vertices; v++) {
			FFSpatialVertex vertex = source_vertices[v];
			vertex.position = copy.xform(vertex.position);
			if (!vertex.position.is_finite()) {
				return ERR_UNAVAILABLE;
			}
			r_extent = r_extent.max(vertex.position.abs());
			const Error error = lit ? _pack_lighting_vertex(vertex, copy.basis.xform(source_normals[v]) * normal_scale, *lit_out++) : _pack_vertex(vertex, *out++);
			if (error != OK) {
				return error;
			}
		}
		for (uint32_t index = 0; index < indices; index++) {
			uint32_t source = index;
			if (compact_indices) {
				source = compact_indices[index];
			} else if (source_indices) {
				source = FixedFunctionMeshStorage::surface_read_index(surface, source_indices, index);
			}
			*index_out++ = i * vertices + source;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_submit_multimesh_copies(const FFDrawPacket &p_packet, const Projection &p_projection, RID p_texture, uint32_t p_batch_primitives, RenderingServerTypes::RenderInfo *r_info) {
	// A cached batch can still fail a view-dependent precision or device check.
	// These packets have one opaque pass, so they can reuse the prepared material.
	const FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	const int visible = meshes.multimesh_get_visible_count(p_packet.multimesh);
	FFDrawPacket copy = p_packet;
	copy.multimesh = RID();
	copy.multimesh_local_lighting = false;
	copy.draw_bounds = p_packet.surface.source.aabb;
	for (int i = 0; i < visible; i++) {
		const Transform3D transform = meshes.multimesh_get_transform(p_packet.multimesh, i);
		if (transform.basis == Basis(0, 0, 0, 0, 0, 0, 0, 0, 0)) {
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
		Error error = _request_geometry(copy) ? _submit_geometry(copy, p_projection, p_texture, r_info) : ERR_UNAVAILABLE;
		if (error == ERR_SKIP) {
			_redraw_requested = true;
			return _render_targets.defer_render_target();
		}
		if (error == ERR_UNAVAILABLE) {
			error = _submit_triangles(copy, p_projection, p_texture, p_batch_primitives, true, r_info);
		}
		if (error != OK) {
			return error;
		}
	}
	return OK;
}
