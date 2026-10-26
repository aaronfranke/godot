#include "test_mesh_storage.h"

#include "core/project_settings.h"

#include <stdio.h>
#include <string.h>
#include <limits>

#define FF_MESH_CHECK(m_condition)                                   \
	do {                                                             \
		if (!(m_condition)) {                                        \
			printf("Mesh FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                            \
		}                                                            \
	} while (false)

TestFixedFunctionMeshStorage::FFInstance::FFInstance(FixedFunctionMeshStorage &p_storage, RID p_mesh) :
		storage(p_storage), other(nullptr), changed(0), removed(0), action(ACTION_NONE), aabb(false), materials(false), success(true) {
	base_type = VS::INSTANCE_MESH;
	base = p_mesh;
	success = storage.instance_add_dependency(base, this) == OK;
}

TestFixedFunctionMeshStorage::FFInstance::~FFInstance() {
	if (base.is_valid()) {
		storage.instance_remove_dependency(base, this);
	}
}

void TestFixedFunctionMeshStorage::FFInstance::base_changed(bool p_aabb, bool p_materials) {
	changed++;
	aabb = p_aabb;
	materials = p_materials;
	const FFAction current_action = action;
	action = ACTION_NONE;
	if (current_action == ACTION_DIRTY) {
		success = success && storage.update_dependencies() == ERR_BUSY && storage.mesh_set_custom_aabb(base, AABB(Vector3(), Vector3(2, 3, 4))) == OK;
	} else if (current_action == ACTION_FREE) {
		success = success && storage.free(base) == OK;
	} else if (current_action == ACTION_CLEAR) {
		storage.clear();
	} else if (current_action == ACTION_REBIND && other) {
		success = success && storage.instance_remove_dependency(other->base, other) == OK;
		other->base = replacement;
		success = success && storage.instance_add_dependency(other->base, other) == OK;
	}
}

void TestFixedFunctionMeshStorage::FFInstance::base_removed() {
	removed++;
	success = success && !storage.owns_mesh(base) && storage.instance_remove_dependency(base, this) == OK;
	base = RID();
	if (action == ACTION_CLEAR) {
		action = ACTION_NONE;
		storage.clear();
	}
}

PoolVector<uint8_t> TestFixedFunctionMeshStorage::_vertices(int p_count) {
	PoolVector<uint8_t> bytes;
	bytes.resize(p_count * 12);
	PoolVector<uint8_t>::Write write = bytes.write();
	for (int i = 0; i < p_count; i++) {
		const float position[] = { float(i % 2), float(i % 3), -1.0f };
		memcpy(write.ptr() + i * 12, position, sizeof(position));
	}
	return bytes;
}

Error TestFixedFunctionMeshStorage::_add(FixedFunctionMeshStorage &p_storage, RID p_mesh, const AABB &p_aabb) {
	return p_storage.mesh_add_surface(p_mesh, VS::ARRAY_FORMAT_VERTEX, VS::PRIMITIVE_TRIANGLES, _vertices(), 3, PoolVector<uint8_t>(), 0, p_aabb, Vector<PoolVector<uint8_t>>(), Vector<AABB>());
}

bool TestFixedFunctionMeshStorage::_test_data(FixedFunctionMeshStorage &p_storage) {
	const RID mesh = p_storage.mesh_create();
	FF_MESH_CHECK(mesh.is_valid() && p_storage.owns_mesh(mesh) && p_storage.get_mesh_count() == 1);
	FF_MESH_CHECK(p_storage.mesh_get_surface_count(mesh) == 0 && p_storage.mesh_get_blend_shape_count(mesh) == 0 && p_storage.mesh_get_blend_shape_mode(mesh) == VS::BLEND_SHAPE_MODE_NORMALIZED);
	const AABB first(Vector3(-1, -1, -1), Vector3(2, 2, 2));
	const AABB second(Vector3(2, 2, 2), Vector3(2, 3, 4));
	FF_MESH_CHECK(_add(p_storage, mesh, first) == OK && _add(p_storage, mesh, second) == OK);
	AABB bounds;
	FF_MESH_CHECK(p_storage.mesh_get_aabb(mesh, RID(), bounds) == OK && bounds == first.merge(second));
	FixedFunctionMeshStorage::FFSurfaceData snapshot;
	FF_MESH_CHECK(p_storage.mesh_surface_get_data(mesh, 0, snapshot) == OK && snapshot.array.size() == 36 && snapshot.strides[VS::ARRAY_VERTEX] == 12);
	FF_MESH_CHECK(snapshot.identity && snapshot.version == 1 && p_storage.surface_is_current(snapshot.identity, snapshot.version));
	FixedFunctionMeshStorage::FFSurfaceData second_snapshot, updated;
	FF_MESH_CHECK(p_storage.mesh_surface_get_data(mesh, 1, second_snapshot) == OK && second_snapshot.identity != snapshot.identity);
	PoolVector<uint8_t> patch;
	patch.resize(4);
	{
		PoolVector<uint8_t>::Write write = patch.write();
		const float value = 7.0f;
		memcpy(write.ptr(), &value, 4);
	}
	FF_MESH_CHECK(p_storage.mesh_surface_update_region(mesh, 0, 0, patch) == OK);
	FF_MESH_CHECK(p_storage.mesh_surface_get_data(mesh, 0, updated) == OK && updated.identity == snapshot.identity && updated.version == snapshot.version + 1);
	FF_MESH_CHECK(!p_storage.surface_is_current(snapshot.identity, snapshot.version) && p_storage.surface_is_current(updated.identity, updated.version));
	PoolVector<Vector3> positions;
	FF_MESH_CHECK(p_storage.mesh_surface_get_positions(mesh, 0, positions) == OK && positions.read()[0] == Vector3(7, 0, -1));
	FF_MESH_CHECK(snapshot.array.read()[2] == 0); // Original x=0 survives the COW write.
	FF_MESH_CHECK(p_storage.mesh_get_aabb(mesh, RID(), bounds) == OK && bounds == first.merge(second));
	{
		PoolVector<uint8_t>::Write write = patch.write();
		const float value = std::numeric_limits<float>::quiet_NaN();
		memcpy(write.ptr(), &value, 4);
	}
	FF_MESH_CHECK(p_storage.mesh_surface_update_region(mesh, 0, 0, patch) == ERR_INVALID_DATA);
	FF_MESH_CHECK(p_storage.surface_is_current(updated.identity, updated.version));
	FF_MESH_CHECK(p_storage.mesh_surface_update_region(mesh, 0, 0, PoolVector<uint8_t>()) == OK && p_storage.surface_is_current(updated.identity, updated.version));
	FF_MESH_CHECK(p_storage.mesh_surface_get_positions(mesh, 0, positions) == OK && positions.read()[0].x == 7);
	FF_MESH_CHECK(p_storage.mesh_surface_update_region(mesh, 0, -1, patch) == ERR_INVALID_PARAMETER && p_storage.mesh_surface_update_region(mesh, 0, INT32_MAX, patch) == ERR_INVALID_PARAMETER);
	FF_MESH_CHECK(p_storage.mesh_set_custom_aabb(mesh, second) == OK && p_storage.mesh_get_aabb(mesh, RID(), bounds) == OK && bounds == second);
	FF_MESH_CHECK(p_storage.mesh_set_custom_aabb(mesh, AABB()) == OK && p_storage.mesh_remove_surface(mesh, 0) == OK);
	FF_MESH_CHECK(!p_storage.surface_is_current(updated.identity, updated.version));
	FF_MESH_CHECK(p_storage.mesh_surface_get_data(mesh, 0, updated) == OK && updated.identity == second_snapshot.identity && updated.version == second_snapshot.version);
	FF_MESH_CHECK(p_storage.mesh_get_aabb(mesh, RID(), bounds) == OK && bounds == second);
	FF_MESH_CHECK(p_storage.mesh_set_custom_aabb(mesh, AABB(Vector3(), Vector3(-1, 1, 1))) == ERR_INVALID_PARAMETER);
	FF_MESH_CHECK(p_storage.mesh_set_blend_shape_count(mesh, 1) == ERR_INVALID_PARAMETER);
	FF_MESH_CHECK(p_storage.mesh_clear(mesh) == OK && p_storage.mesh_get_surface_count(mesh) == 0);
	FF_MESH_CHECK(!p_storage.surface_is_current(second_snapshot.identity, second_snapshot.version));
	FF_MESH_CHECK(p_storage.mesh_surface_get_data(mesh, 0, snapshot) == ERR_INVALID_PARAMETER && snapshot.array.size() == 36);
	FF_MESH_CHECK(p_storage.mesh_set_blend_shape_count(mesh, 1) == OK && p_storage.mesh_set_blend_shape_mode(mesh, VS::BLEND_SHAPE_MODE_RELATIVE) == OK);
	Vector<PoolVector<uint8_t>> blend_shapes;
	blend_shapes.push_back(_vertices());
	FF_MESH_CHECK(p_storage.mesh_add_surface(mesh, VS::ARRAY_FORMAT_VERTEX, VS::PRIMITIVE_TRIANGLES, _vertices(), 3, PoolVector<uint8_t>(), 0, first, blend_shapes, Vector<AABB>()) == OK);
	PoolVector<float> weights;
	weights.push_back(0.5f);
	FF_MESH_CHECK(p_storage.mesh_set_blend_shape_values(mesh, weights) == OK && p_storage.mesh_get_blend_shape_values(mesh).read()[0] == 0.5f);
	FF_MESH_CHECK(p_storage.mesh_surface_get_data(mesh, 0, snapshot) == OK && snapshot.blend_shapes.size() == 1);
	FF_MESH_CHECK(p_storage.free(mesh) == OK && p_storage.get_mesh_count() == 0);
	return true;
}

bool TestFixedFunctionMeshStorage::_test_formats(FixedFunctionMeshStorage &p_storage) {
	VisualServer *server = VisualServer::get_singleton();
	FF_MESH_CHECK(server);
	const RID mesh = p_storage.mesh_create();
	const bool split = GLOBAL_GET("rendering/misc/mesh_storage/split_stream");
	for (int layout = 0; layout < 3; layout++) {
		ProjectSettings::get_singleton()->set("rendering/misc/mesh_storage/split_stream", layout != 0);
		// Exercise the real engine packer, including default octahedral normals/tangents.
		for (int compression = 0; compression < 4; compression++) {
			const bool two_dimensions = compression >= 2;
			const RID source = server->mesh_create();
			Array arrays;
			arrays.resize(VS::ARRAY_MAX);
			PoolVector<Vector3> vertices;
			PoolVector<Vector2> vertices_2d;
			PoolVector<Vector3> normals;
			PoolVector<real_t> tangents;
			PoolVector<Vector2> uvs;
			PoolVector<Color> colors;
			PoolVector<int> bones;
			PoolVector<real_t> weights;
			PoolVector<int> indices;
			for (int i = 0; i < 3; i++) {
				vertices.push_back(Vector3(i, i + 1, -2));
				vertices_2d.push_back(Vector2(i, i + 1));
				normals.push_back(Vector3(0, 0, 1));
				tangents.push_back(1);
				tangents.push_back(0);
				tangents.push_back(0);
				tangents.push_back(1);
				uvs.push_back(Vector2(0.25, 0.5));
				colors.push_back(Color(0.25, 0.5, 0.75, 1));
				for (int j = 0; j < 4; j++) {
					bones.push_back(two_dimensions ? 256 + j : j);
					weights.push_back(0.25);
				}
				indices.push_back(i);
			}
			arrays[VS::ARRAY_VERTEX] = two_dimensions ? Variant(vertices_2d) : Variant(vertices);
			arrays[VS::ARRAY_NORMAL] = normals;
			arrays[VS::ARRAY_TANGENT] = tangents;
			arrays[VS::ARRAY_TEX_UV] = uvs;
			arrays[VS::ARRAY_TEX_UV2] = uvs;
			arrays[VS::ARRAY_COLOR] = colors;
			arrays[VS::ARRAY_BONES] = bones;
			arrays[VS::ARRAY_WEIGHTS] = weights;
			arrays[VS::ARRAY_INDEX] = indices;
			uint32_t flags = (compression & 1) ? uint32_t(VS::ARRAY_COMPRESS_DEFAULT | VS::ARRAY_COMPRESS_VERTEX) : 0;
			if (layout == 2) {
				flags |= VS::ARRAY_FLAG_USE_DYNAMIC_UPDATE;
			}
			server->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_TRIANGLES, arrays, Array(), flags);
			FF_MESH_CHECK(server->mesh_get_surface_count(source) == 1);
			const uint32_t format = server->mesh_surface_get_format(source, 0);
			FF_MESH_CHECK(p_storage.mesh_add_surface(mesh, format, VS::PRIMITIVE_TRIANGLES, server->mesh_surface_get_array(source, 0), 3, server->mesh_surface_get_index_array(source, 0), 3, server->mesh_surface_get_aabb(source, 0), Vector<PoolVector<uint8_t>>(), server->mesh_surface_get_skeleton_aabb(source, 0)) == OK);
			PoolVector<Vector3> decoded;
			FF_MESH_CHECK(p_storage.mesh_surface_get_positions(mesh, p_storage.mesh_get_surface_count(mesh) - 1, decoded) == OK && decoded.read()[2] == Vector3(2, 3, two_dimensions ? 0 : -2));
			server->free(source);
		}
	}
	ProjectSettings::get_singleton()->set("rendering/misc/mesh_storage/split_stream", split);
	// Invalid lengths/flags and index bounds fail before publishing any surface.
	const int before = p_storage.mesh_get_surface_count(mesh);
	PoolVector<uint8_t> indices;
	indices.resize(2);
	{
		PoolVector<uint8_t>::Write write = indices.write();
		write[0] = 3;
		write[1] = 0;
	}
	FF_MESH_CHECK(p_storage.mesh_add_surface(mesh, VS::ARRAY_FORMAT_VERTEX | VS::ARRAY_FORMAT_INDEX, VS::PRIMITIVE_POINTS, _vertices(), 3, indices, 1, AABB(), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == ERR_INVALID_DATA);
	FF_MESH_CHECK(p_storage.mesh_add_surface(mesh, VS::ARRAY_FORMAT_VERTEX, VS::PRIMITIVE_POINTS, _vertices(), INT32_MAX, PoolVector<uint8_t>(), 0, AABB(), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == ERR_INVALID_PARAMETER);
	FF_MESH_CHECK(p_storage.mesh_add_surface(mesh, VS::ARRAY_FORMAT_VERTEX | (1U << 31), VS::PRIMITIVE_POINTS, _vertices(), 3, PoolVector<uint8_t>(), 0, AABB(), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == ERR_INVALID_PARAMETER);
	FF_MESH_CHECK(p_storage.mesh_get_surface_count(mesh) == before);
	indices.resize(4);
	{
		PoolVector<uint8_t>::Write write = indices.write();
		const uint32_t index = 65535;
		memcpy(write.ptr(), &index, 4);
	}
	FF_MESH_CHECK(p_storage.mesh_add_surface(mesh, VS::ARRAY_FORMAT_VERTEX | VS::ARRAY_FORMAT_INDEX, VS::PRIMITIVE_POINTS, _vertices(65536), 65536, indices, 1, AABB(), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
	FF_MESH_CHECK(p_storage.free(mesh) == OK);
	return true;
}

bool TestFixedFunctionMeshStorage::_test_dependencies(FixedFunctionMeshStorage &p_storage, FixedFunctionMaterialStorage &p_materials) {
	const RID mesh = p_storage.mesh_create();
	FF_MESH_CHECK(_add(p_storage, mesh) == OK && _add(p_storage, mesh) == OK);
	FFInstance instance(p_storage, mesh);
	FF_MESH_CHECK(instance.success && p_storage.update_dependencies() == OK && instance.changed == 1 && instance.aabb && instance.materials);
	const RID material = p_materials.material_create();
	FF_MESH_CHECK(p_storage.mesh_surface_set_material(mesh, 0, material) == OK && p_storage.mesh_surface_set_material(mesh, 1, material) == OK);
	FF_MESH_CHECK(p_storage.update_dependencies() == OK && instance.changed == 2 && !instance.aabb && instance.materials);
	FF_MESH_CHECK(p_materials.material_set_param(material, "albedo", Color(1, 0, 0)) == OK && p_materials.update_dependencies() == OK && p_storage.update_dependencies() == OK && instance.changed == 3);
	FF_MESH_CHECK(p_storage.mesh_remove_surface(mesh, 0) == OK && p_storage.update_dependencies() == OK);
	FF_MESH_CHECK(p_materials.free(material) == OK && !p_storage.mesh_surface_get_material(mesh, 0).is_valid());
	FF_MESH_CHECK(p_storage.update_dependencies() == OK && instance.materials && !instance.aabb);
	instance.action = FFInstance::ACTION_DIRTY;
	FF_MESH_CHECK(p_storage.mesh_set_custom_aabb(mesh, AABB()) == OK && p_storage.update_dependencies() == ERR_SKIP && instance.success);
	FF_MESH_CHECK(p_storage.update_dependencies() == OK && instance.aabb && !instance.materials);
	instance.action = FFInstance::ACTION_FREE;
	FF_MESH_CHECK(p_storage.mesh_set_custom_aabb(mesh, AABB()) == OK && p_storage.update_dependencies() == OK && instance.success && instance.removed == 1 && !instance.base.is_valid());
	// Rebinding at the same address invalidates an old notification snapshot.
	const RID first = p_storage.mesh_create();
	const RID second = p_storage.mesh_create();
	FFInstance left(p_storage, first);
	FFInstance right(p_storage, first);
	FFInstance retained(p_storage, first);
	FFInstance outside(p_storage, second);
	Map<RasterizerScene::InstanceBase *, bool> order;
	order[&left] = true;
	order[&right] = true;
	FFInstance *early = static_cast<FFInstance *>(order.front()->key());
	FFInstance *late = early == &left ? &right : &left;
	early->action = FFInstance::ACTION_REBIND;
	early->other = late;
	early->replacement = first;
	FF_MESH_CHECK(p_storage.update_dependencies() == ERR_SKIP && early->success && late->changed == 0);
	FF_MESH_CHECK(p_storage.update_dependencies() == OK && late->changed == 1);
	// Reentrant clear during a removal still notifies the retired mesh's remaining owners.
	early->action = FFInstance::ACTION_CLEAR;
	late->action = FFInstance::ACTION_CLEAR;
	retained.action = FFInstance::ACTION_CLEAR;
	FF_MESH_CHECK(p_storage.free(first) == OK && early->removed == 1 && late->removed == 1 && retained.removed == 1 && outside.removed == 1 && early->success && late->success && retained.success && outside.success && p_storage.get_mesh_count() == 0);
	return true;
}

bool TestFixedFunctionMeshStorage::test() {
	FixedFunctionMaterialStorage materials;
	FixedFunctionMeshStorage storage(materials);
	FF_MESH_CHECK(_test_data(storage) && _test_formats(storage) && _test_dependencies(storage, materials));
	printf("PASS: mesh CPU snapshots, engine packed formats, checked edits, bounds and material/instance lifetimes.\n");
	return true;
}
