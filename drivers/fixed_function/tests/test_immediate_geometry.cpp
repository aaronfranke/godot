#include "test_immediate_geometry.h"

#include <stdio.h>
#include <limits>

#define FF_IMMEDIATE_CHECK(m_condition)                                            \
	do {                                                                           \
		if (!(m_condition)) {                                                      \
			printf("Immediate geometry FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                          \
		}                                                                          \
	} while (false)

TestFixedFunctionImmediateGeometry::FFInstance::FFInstance(FixedFunctionMeshStorage &p_storage, RID p_base) :
		storage(p_storage), other(nullptr), changed(0), removed(0), action(ACTION_NONE), success(true), aabb_changed(false), materials_changed(false) {
	base_type = VS::INSTANCE_IMMEDIATE;
	base = p_base;
	success = storage.instance_add_dependency(base, this) == OK;
}

TestFixedFunctionImmediateGeometry::FFInstance::~FFInstance() {
	if (base.is_valid()) {
		storage.instance_remove_dependency(base, this);
	}
}

void TestFixedFunctionImmediateGeometry::FFInstance::base_changed(bool p_aabb, bool p_materials) {
	changed++;
	aabb_changed = p_aabb;
	materials_changed = p_materials;
	const FFAction request = action;
	action = ACTION_NONE;
	if (request == ACTION_DIRTY) {
		success = success && storage.update_dependencies() == ERR_BUSY && storage.immediate_clear(base) == OK;
	} else if (request == ACTION_FREE) {
		success = success && storage.free(base) == OK;
	} else if (request == ACTION_CLEAR) {
		storage.clear();
	} else if (request == ACTION_REBIND && other) {
		success = success && storage.instance_remove_dependency(other->base, other) == OK;
		other->base = replacement;
		success = success && storage.instance_add_dependency(other->base, other) == OK;
	}
}

void TestFixedFunctionImmediateGeometry::FFInstance::base_removed() {
	removed++;
	success = success && !storage.owns_immediate(base) && storage.instance_remove_dependency(base, this) == OK;
	base = RID();
	if (action == ACTION_CLEAR) {
		action = ACTION_NONE;
		storage.clear();
	}
}

bool TestFixedFunctionImmediateGeometry::_triangle(FixedFunctionMeshStorage &p_meshes, RID p_immediate, const Vector3 &p_origin) {
	FF_IMMEDIATE_CHECK(p_meshes.immediate_begin(p_immediate, VS::PRIMITIVE_TRIANGLES, RID()) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_vertex(p_immediate, p_origin) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_vertex(p_immediate, p_origin + Vector3(1, 0, 0)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_vertex(p_immediate, p_origin + Vector3(0, 1, 0)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_end(p_immediate) == OK);
	return true;
}

bool TestFixedFunctionImmediateGeometry::_data(FixedFunctionMeshStorage &p_meshes) {
	const RID first = p_meshes.immediate_create(), second = p_meshes.immediate_create();
	FF_IMMEDIATE_CHECK(first.is_valid() && p_meshes.owns_immediate(first) && !p_meshes.owns_mesh(first));
	FF_IMMEDIATE_CHECK(p_meshes.immediate_end(first) == ERR_INVALID_PARAMETER);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_begin(first, VS::PRIMITIVE_MAX, RID()) == ERR_INVALID_PARAMETER);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_begin(first, VS::PRIMITIVE_TRIANGLES, second) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_begin(first, VS::PRIMITIVE_LINES, RID()) == ERR_ALREADY_IN_USE);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_clear(first) == ERR_BUSY);
	RID mesh = second;
	FF_IMMEDIATE_CHECK(p_meshes.immediate_get_mesh(first, mesh) == OK && !mesh.is_valid());
	const real_t nan = std::numeric_limits<real_t>::quiet_NaN();
	FF_IMMEDIATE_CHECK(p_meshes.immediate_vertex(first, Vector3(nan, 0, 0)) == ERR_INVALID_PARAMETER);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_normal(first, Vector3(0, nan, 0)) == ERR_INVALID_PARAMETER);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_uv(first, Vector2(0, nan)) == ERR_INVALID_PARAMETER);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_tangent(first, Plane(Vector3(1, 0, 0), nan)) == ERR_INVALID_PARAMETER);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_color(first, Color(1, 1, 1, std::numeric_limits<float>::infinity())) == ERR_INVALID_PARAMETER);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_color(first, Color(1, 0, 0)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_normal(first, Vector3(0, 0, 1)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_tangent(first, Plane(Vector3(1, 0, 0), -1)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_uv(first, Vector2(0.25, 0.5)) == OK && p_meshes.immediate_uv(first, Vector2(0.75, 1), true) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_vertex(first, Vector3(2, 3, 4)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_begin(second, VS::PRIMITIVE_POINTS, RID()) == OK && p_meshes.immediate_color(second, Color(0, 0, 1)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_vertex(second, Vector3()) == OK && p_meshes.immediate_end(second) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_vertex(first, Vector3(5, 3, 4)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_color(first, Color(0, 1, 0)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_vertex(first, Vector3(2, 7, 4)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_end(first) == OK && p_meshes.immediate_get_mesh(first, mesh) == OK);
	FixedFunctionMeshStorage::FFSurfaceData snapshot;
	FF_IMMEDIATE_CHECK(p_meshes.mesh_surface_get_data(mesh, 0, snapshot) == OK && snapshot.array.size() == 0 && snapshot.vertex_count == 3);
	FF_IMMEDIATE_CHECK(snapshot.immediate_texture == second && snapshot.immediate_vertices.size() == 3);
	FF_IMMEDIATE_CHECK(snapshot.immediate_vertices[1].color == Color(1, 0, 0) && snapshot.immediate_vertices[2].color == Color(0, 1, 0));
	FF_IMMEDIATE_CHECK(snapshot.immediate_vertices[1].normal == Vector3(0, 0, 1) && snapshot.immediate_vertices[1].tangent.d == -1);
	FF_IMMEDIATE_CHECK(FixedFunctionMeshStorage::surface_read_position(snapshot, nullptr, 1) == Vector3(5, 3, 4));
	FF_IMMEDIATE_CHECK(FixedFunctionMeshStorage::surface_read_uv(snapshot, nullptr, 2, 1) == Vector2(0.75, 1));
	FF_IMMEDIATE_CHECK(FixedFunctionMeshStorage::surface_read_color(snapshot, nullptr, 2) == Color(0, 1, 0));
	AABB bounds;
	FF_IMMEDIATE_CHECK(p_meshes.immediate_get_aabb(first, bounds) == OK && bounds == AABB(Vector3(2, 3, 4), Vector3(3, 4, 0)));
	FF_IMMEDIATE_CHECK(_triangle(p_meshes, first, Vector3(-3, -4, -5)) && p_meshes.immediate_get_aabb(first, bounds) == OK && bounds.position == Vector3(-3, -4, -5));
	FF_IMMEDIATE_CHECK(p_meshes.immediate_clear(first) == OK && p_meshes.immediate_get_aabb(first, bounds) == OK && bounds == AABB());
	FF_IMMEDIATE_CHECK(snapshot.immediate_vertices[1].position == Vector3(5, 3, 4));
	const real_t limit = std::numeric_limits<real_t>::max();
	FF_IMMEDIATE_CHECK(p_meshes.immediate_begin(first, VS::PRIMITIVE_LINES, RID()) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_vertex(first, Vector3(-limit, 0, 0)) == OK && p_meshes.immediate_vertex(first, Vector3(limit, 0, 0)) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.immediate_end(first) == ERR_INVALID_DATA && p_meshes.mesh_get_surface_count(mesh) == 0);
	FF_IMMEDIATE_CHECK(p_meshes.free(mesh) == ERR_INVALID_PARAMETER); // Borrowed backing RID.
	for (int primitive = 0; primitive < VS::PRIMITIVE_MAX; primitive++) {
		FF_IMMEDIATE_CHECK(p_meshes.immediate_begin(first, VS::PrimitiveType(primitive), RID()) == OK);
		for (int i = 0; i < 4; i++) {
			FF_IMMEDIATE_CHECK(p_meshes.immediate_vertex(first, Vector3(i, i % 2, -1)) == OK);
		}
		FF_IMMEDIATE_CHECK(p_meshes.immediate_end(first) == OK);
	}
	FF_IMMEDIATE_CHECK(p_meshes.mesh_get_surface_count(mesh) == VS::PRIMITIVE_MAX);
	FF_IMMEDIATE_CHECK(p_meshes.mesh_surface_get_data(mesh, VS::PRIMITIVE_TRIANGLES, snapshot) == OK && snapshot.vertex_count == 3 && snapshot.immediate_vertices.size() == 4);
	FF_IMMEDIATE_CHECK(snapshot.aabb.size.x == 2); // The incomplete tail cannot affect culling or the native anchor.
	FF_IMMEDIATE_CHECK(p_meshes.immediate_begin(first, VS::PRIMITIVE_TRIANGLES, RID()) == OK && p_meshes.immediate_end(first) == OK);
	FF_IMMEDIATE_CHECK(p_meshes.mesh_get_surface_count(mesh) == VS::PRIMITIVE_MAX);
	FF_IMMEDIATE_CHECK(p_meshes.free(first) == OK && p_meshes.free(second) == OK && !p_meshes.owns_mesh(mesh));
	return true;
}

bool TestFixedFunctionImmediateGeometry::_dependencies(FixedFunctionMeshStorage &p_meshes, FixedFunctionMaterialStorage &p_materials) {
	const RID immediate = p_meshes.immediate_create();
	FFInstance first(p_meshes, immediate), second(p_meshes, immediate);
	FF_IMMEDIATE_CHECK(first.success && second.success && p_meshes.update_dependencies() == OK);
	first.changed = second.changed = 0;
	FF_IMMEDIATE_CHECK(_triangle(p_meshes, immediate) && _triangle(p_meshes, immediate));
	FF_IMMEDIATE_CHECK(first.changed == 0 && p_meshes.update_dependencies() == OK && first.changed == 1 && second.changed == 1);
	FF_IMMEDIATE_CHECK(first.aabb_changed && first.materials_changed);
	const RID material = p_materials.material_create();
	FF_IMMEDIATE_CHECK(p_meshes.immediate_set_material(immediate, material) == OK && p_meshes.immediate_get_material(immediate) == material);
	FF_IMMEDIATE_CHECK(p_meshes.update_dependencies() == OK && first.changed == 2 && !first.aabb_changed && first.materials_changed);
	FF_IMMEDIATE_CHECK(p_materials.free(material) == OK && !p_meshes.immediate_get_material(immediate).is_valid());
	FF_IMMEDIATE_CHECK(p_meshes.update_dependencies() == OK && first.changed == 3 && first.materials_changed);
	first.action = FFInstance::ACTION_DIRTY;
	FF_IMMEDIATE_CHECK(_triangle(p_meshes, immediate) && p_meshes.update_dependencies() == ERR_SKIP && first.success);
	FF_IMMEDIATE_CHECK(p_meshes.update_dependencies() == OK);
	const RID replacement = p_meshes.immediate_create();
	first.action = FFInstance::ACTION_REBIND;
	first.other = &second;
	first.replacement = replacement;
	FF_IMMEDIATE_CHECK(_triangle(p_meshes, immediate) && p_meshes.update_dependencies() == ERR_SKIP && first.success && second.base == replacement);
	FF_IMMEDIATE_CHECK(p_meshes.update_dependencies() == OK);
	first.action = FFInstance::ACTION_FREE;
	FF_IMMEDIATE_CHECK(_triangle(p_meshes, immediate) && p_meshes.update_dependencies() == OK && first.success && first.removed == 1 && second.removed == 0);
	second.action = FFInstance::ACTION_CLEAR;
	p_meshes.clear();
	FF_IMMEDIATE_CHECK(second.removed == 1 && second.success && p_meshes.get_mesh_count() == 0);
	return true;
}

bool TestFixedFunctionImmediateGeometry::cpu() {
	FixedFunctionMaterialStorage materials;
	FixedFunctionMeshStorage meshes(materials);
	FF_IMMEDIATE_CHECK(_data(meshes) && _dependencies(meshes, materials));
	printf("PASS: immediate typed vertices, per-resource attributes, topology/bounds, COW snapshots, material/dependency ownership, coalescing, reentrant changes/free/rebind and clear.\n");
	return true;
}

#undef FF_IMMEDIATE_CHECK
