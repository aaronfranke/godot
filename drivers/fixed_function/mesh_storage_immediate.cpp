#include "mesh_storage.h"

FixedFunctionMeshStorage::FFImmediate *FixedFunctionMeshStorage::_get_immediate(RID p_immediate) const {
	return _immediate_owner.owns(p_immediate) ? _immediate_owner.getornull(p_immediate) : nullptr;
}

FixedFunctionMeshStorage::FFImmediate *FixedFunctionMeshStorage::_get_building_immediate(RID p_immediate) const {
	FFImmediate *immediate = _get_immediate(p_immediate);
	return immediate && immediate->building && !_clearing ? immediate : nullptr;
}

FixedFunctionMeshStorage::FFMesh *FixedFunctionMeshStorage::_get_dependency_mesh(RID p_base) const {
	const FFImmediate *immediate = _get_immediate(p_base);
	return _get_mesh(immediate ? immediate->mesh : p_base);
}

RID FixedFunctionMeshStorage::immediate_create() {
	if (_clearing) {
		return RID();
	}
	const RID mesh = mesh_create();
	if (!mesh.is_valid()) {
		return RID();
	}
	FFImmediate *immediate = memnew(FFImmediate);
	immediate->mesh = mesh;
	immediate->self = _immediate_owner.make_rid(immediate);
	_get_mesh(mesh)->dependency_owner = immediate->self;
	_immediates.push_back(immediate);
	return immediate->self;
}

Error FixedFunctionMeshStorage::immediate_begin(RID p_immediate, VS::PrimitiveType p_primitive, RID p_texture) {
	FFImmediate *immediate = _get_immediate(p_immediate);
	if (!immediate || _clearing || p_primitive < 0 || p_primitive >= VS::PRIMITIVE_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	if (immediate->building) {
		return ERR_ALREADY_IN_USE;
	}
	immediate->primitive = p_primitive;
	immediate->texture = p_texture;
	immediate->format = VS::ARRAY_FORMAT_VERTEX;
	immediate->current = FFImmediateVertex();
	immediate->building = true;
	return OK;
}

Error FixedFunctionMeshStorage::immediate_vertex(RID p_immediate, const Vector3 &p_vertex) {
	FFImmediate *immediate = _get_building_immediate(p_immediate);
	if (!immediate || Math::is_nan(p_vertex.x) || Math::is_nan(p_vertex.y) || Math::is_nan(p_vertex.z) || Math::is_inf(p_vertex.x) || Math::is_inf(p_vertex.y) || Math::is_inf(p_vertex.z)) {
		return ERR_INVALID_PARAMETER;
	}
	FFImmediateVertex vertex = immediate->current;
	vertex.position = p_vertex;
	return immediate->vertices.push_back(vertex) ? ERR_OUT_OF_MEMORY : OK;
}

Error FixedFunctionMeshStorage::immediate_normal(RID p_immediate, const Vector3 &p_normal) {
	FFImmediate *immediate = _get_building_immediate(p_immediate);
	for (int axis = 0; axis < 3; axis++) {
		if (!immediate || Math::is_nan(p_normal[axis]) || Math::is_inf(p_normal[axis])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	immediate->current.normal = p_normal;
	immediate->format |= VS::ARRAY_FORMAT_NORMAL;
	return OK;
}

Error FixedFunctionMeshStorage::immediate_tangent(RID p_immediate, const Plane &p_tangent) {
	FFImmediate *immediate = _get_building_immediate(p_immediate);
	const real_t values[] = { p_tangent.normal.x, p_tangent.normal.y, p_tangent.normal.z, p_tangent.d };
	for (int i = 0; i < 4; i++) {
		if (!immediate || Math::is_nan(values[i]) || Math::is_inf(values[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	immediate->current.tangent = p_tangent;
	immediate->format |= VS::ARRAY_FORMAT_TANGENT;
	return OK;
}

Error FixedFunctionMeshStorage::immediate_color(RID p_immediate, const Color &p_color) {
	FFImmediate *immediate = _get_building_immediate(p_immediate);
	for (int i = 0; i < 4; i++) {
		if (!immediate || Math::is_nan(p_color[i]) || Math::is_inf(p_color[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	immediate->current.color = p_color;
	immediate->format |= VS::ARRAY_FORMAT_COLOR;
	return OK;
}

Error FixedFunctionMeshStorage::immediate_uv(RID p_immediate, const Vector2 &p_uv, bool p_uv2) {
	FFImmediate *immediate = _get_building_immediate(p_immediate);
	if (!immediate || Math::is_nan(p_uv.x) || Math::is_nan(p_uv.y) || Math::is_inf(p_uv.x) || Math::is_inf(p_uv.y)) {
		return ERR_INVALID_PARAMETER;
	}
	immediate->current.uv[p_uv2 ? 1 : 0] = p_uv;
	immediate->format |= p_uv2 ? VS::ARRAY_FORMAT_TEX_UV2 : VS::ARRAY_FORMAT_TEX_UV;
	return OK;
}

Error FixedFunctionMeshStorage::immediate_end(RID p_immediate) {
	FFImmediate *immediate = _get_building_immediate(p_immediate);
	if (!immediate) {
		return ERR_INVALID_PARAMETER;
	}
	int count = immediate->vertices.size();
	// Native immediate drawing ignores unfinished trailing primitives. Keep the
	// canonical vertices, but publish only complete primitives to the shared draws.
	if (immediate->primitive == VS::PRIMITIVE_TRIANGLES) {
		count -= count % 3;
	} else if (immediate->primitive == VS::PRIMITIVE_LINES) {
		count -= count % 2;
	} else if ((immediate->primitive == VS::PRIMITIVE_TRIANGLE_STRIP || immediate->primitive == VS::PRIMITIVE_TRIANGLE_FAN) && count < 3) {
		count = 0;
	} else if ((immediate->primitive == VS::PRIMITIVE_LINE_STRIP || immediate->primitive == VS::PRIMITIVE_LINE_LOOP) && count < 2) {
		count = 0;
	}
	if (count) {
		if (_next_surface_identity == UINT64_MAX) {
			return ERR_OUT_OF_MEMORY;
		}
		FFMesh *mesh = _get_mesh(immediate->mesh);
		AABB bounds(immediate->vertices[0].position, Vector3());
		for (int i = 1; i < count; i++) {
			bounds.expand_to(immediate->vertices[i].position);
		}
		if (immediate->primitive == VS::PRIMITIVE_POINTS && bounds.has_no_surface()) {
			// A single visible point still needs a cullable bound in VisualServerScene.
			bounds.grow_by(CMP_EPSILON);
		}
		const AABB combined = mesh->surfaces.empty() ? bounds : immediate->bounds.merge(bounds);
		if (!_valid_aabb(bounds) || !_valid_aabb(combined)) {
			// Finite endpoints can still overflow their extent. Reject only this batch.
			immediate->vertices.clear();
			immediate->building = false;
			return ERR_INVALID_DATA;
		}
		FFSurface *surface = memnew(FFSurface);
		surface->data.immediate_vertices = immediate->vertices;
		surface->data.immediate_texture = immediate->texture;
		surface->data.vertex_count = count;
		surface->data.format = immediate->format;
		surface->data.primitive = immediate->primitive;
		surface->data.identity = ++_next_surface_identity;
		surface->data.version = 1;
		surface->data.aabb = bounds;
		if (mesh->surfaces.push_back(surface)) {
			memdelete(surface);
			return ERR_OUT_OF_MEMORY;
		}
		_surfaces[surface->data.identity] = surface;
		immediate->bounds = combined;
		mesh->aabb_dirty = mesh->materials_dirty = true;
	}
	immediate->vertices.clear();
	immediate->building = false;
	return OK;
}

Error FixedFunctionMeshStorage::immediate_clear(RID p_immediate) {
	FFImmediate *immediate = _get_immediate(p_immediate);
	if (!immediate || _clearing) {
		return ERR_INVALID_PARAMETER;
	}
	if (immediate->building) {
		return ERR_BUSY;
	}
	const Error error = mesh_clear(immediate->mesh);
	if (error == OK) {
		immediate->bounds = AABB();
	}
	return error;
}

Error FixedFunctionMeshStorage::immediate_set_material(RID p_immediate, RID p_material) {
	FFImmediate *immediate = _get_immediate(p_immediate);
	if (!immediate || _clearing || (p_material.is_valid() && !_materials.owns_material(p_material))) {
		return ERR_INVALID_PARAMETER;
	}
	if (immediate->material == p_material) {
		return OK;
	}
	_materials.material_remove_geometry_owner(&immediate->material);
	immediate->material = p_material;
	FFMesh *mesh = _get_mesh(immediate->mesh);
	if (p_material.is_valid()) {
		const Error error = _materials.material_add_geometry_owner(&immediate->material, &mesh->materials_dirty);
		if (error != OK) {
			immediate->material = RID();
			mesh->materials_dirty = true;
			return error;
		}
	}
	mesh->materials_dirty = true;
	return OK;
}

RID FixedFunctionMeshStorage::immediate_get_material(RID p_immediate) const {
	const FFImmediate *immediate = _get_immediate(p_immediate);
	return immediate ? immediate->material : RID();
}

Error FixedFunctionMeshStorage::immediate_get_aabb(RID p_immediate, AABB &r_aabb) const {
	const FFImmediate *immediate = _get_immediate(p_immediate);
	if (!immediate) {
		return ERR_INVALID_PARAMETER;
	}
	r_aabb = immediate->bounds;
	return OK;
}

Error FixedFunctionMeshStorage::immediate_get_mesh(RID p_immediate, RID &r_mesh) const {
	const FFImmediate *immediate = _get_immediate(p_immediate);
	if (!immediate) {
		return ERR_INVALID_PARAMETER;
	}
	r_mesh = immediate->building ? RID() : immediate->mesh;
	return OK;
}

bool FixedFunctionMeshStorage::immediate_surface_can_retain(uint64_t p_identity, uint64_t p_frame) {
	const Map<uint64_t, FFSurface *>::Element *entry = _surfaces.find(p_identity);
	if (!entry || !p_frame) {
		return false;
	}
	FFSurface *surface = entry->get();
	if (!surface->first_immediate_frame) {
		surface->first_immediate_frame = p_frame;
	}
	return surface->first_immediate_frame != p_frame;
}

void FixedFunctionMeshStorage::_free_immediate(FFImmediate *p_immediate) {
	_immediates.erase(p_immediate);
	_immediate_owner.free(p_immediate->self);
	_materials.material_remove_geometry_owner(&p_immediate->material);
	_free_mesh(_get_mesh(p_immediate->mesh));
	memdelete(p_immediate);
}
