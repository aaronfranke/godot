#include "test_mesh_storage.h"

#include "servers/rendering/rendering_server.h"

#include <stdio.h>
#include <string.h>

#include <limits>

#define FF_MESH_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Mesh FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

TestFixedFunctionMeshStorage::FFDependencyState::FFDependencyState() : storage(nullptr), changed(0), deleted(0), action(0), success(true) {}

void TestFixedFunctionMeshStorage::_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
	FFDependencyState *state = static_cast<FFDependencyState *>(p_tracker->userdata);
	state->changed++;
	const int action = state->action;
	state->action = 0;
	if (action == 1) {
		state->success &= state->storage->mesh_set_custom_aabb(state->mesh, AABB(Vector3(), Vector3(2, 3, 4))) == OK;
		state->success &= state->storage->update_dependencies() == ERR_BUSY;
	} else if (action == 2) {
		state->success &= state->storage->free(state->other) == OK;
	} else if (action == 3) {
		state->storage->clear();
	}
}

void TestFixedFunctionMeshStorage::_deleted(const RID &p_rid, DependencyTracker *p_tracker) {
	static_cast<FFDependencyState *>(p_tracker->userdata)->deleted++;
}

bool TestFixedFunctionMeshStorage::_test_layouts(FixedFunctionMeshStorage &p_meshes) {
	for (int flags = 0; flags < 4; flags++) {
		Array arrays;
		arrays.resize(RSE::ARRAY_MAX);
		Vector<Vector3> positions;
		positions.push_back(Vector3(2, 3, 4));
		positions.push_back(Vector3(6, 3, 4));
		positions.push_back(Vector3(2, 9, 4));
		if (flags & 2) {
			Vector<Vector2> positions2;
			for (const Vector3 &position : positions) {
				positions2.push_back(Vector2(position.x, position.y));
			}
			arrays[RSE::ARRAY_VERTEX] = positions2;
		} else {
			arrays[RSE::ARRAY_VERTEX] = positions;
		}
		Vector<Vector2> uv, uv2;
		Vector<Color> colors;
		for (int i = 0; i < 3; i++) {
			uv.push_back(Vector2(-2 + i * 2, 0.25));
			uv2.push_back(Vector2(3 + i, -4));
			colors.push_back(Color(0.2, 0.4, 0.8, 0.5));
		}
		arrays[RSE::ARRAY_TEX_UV] = uv;
		arrays[RSE::ARRAY_TEX_UV2] = uv2;
		arrays[RSE::ARRAY_COLOR] = colors;
		RenderingServerTypes::SurfaceData data;
		const uint64_t format = (flags & 1 ? uint64_t(RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) : 0) | (flags & 2 ? uint64_t(RSE::ARRAY_FLAG_USE_2D_VERTICES) : 0);
		FF_MESH_CHECK(RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&data, RSE::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), format) == OK);
		const RID mesh = p_meshes.mesh_create();
		FF_MESH_CHECK(p_meshes.mesh_add_surface(mesh, data) == OK);
		FixedFunctionMeshStorage::FFSurfaceData snapshot;
		FF_MESH_CHECK(p_meshes.mesh_surface_get_data(mesh, 0, snapshot) == OK && snapshot.source.vertex_data == data.vertex_data && snapshot.source.attribute_data == data.attribute_data);
		for (int i = 0; i < 3; i++) {
			const Vector3 actual = FixedFunctionMeshStorage::surface_get_position(snapshot, snapshot.source.vertex_data.ptr(), i);
			Vector3 expected = positions[i];
			if (flags & 2) {
				expected.z = 0;
			}
			FF_MESH_CHECK(actual.distance_to(expected) < 0.001);
		}
		AABB aabb;
		FF_MESH_CHECK(p_meshes.mesh_get_aabb(mesh, RID(), aabb) == OK && aabb == data.aabb);
		FF_MESH_CHECK(p_meshes.mesh_set_custom_aabb(mesh, AABB(Vector3(), Vector3(20, 20, 20))) == OK && p_meshes.mesh_get_aabb(mesh, RID(), aabb) == OK && aabb.size == Vector3(20, 20, 20));
		FF_MESH_CHECK(p_meshes.mesh_set_custom_aabb(mesh, AABB()) == OK);
		Vector<uint8_t> edit;
		edit.resize(1);
		edit.set(0, data.attribute_data[0] ^ 255);
		FF_MESH_CHECK(p_meshes.mesh_surface_update_region(mesh, 0, FixedFunctionMeshStorage::BUFFER_ATTRIBUTE, 0, edit) == OK);
		FixedFunctionMeshStorage::FFSurfaceData latest;
		FF_MESH_CHECK(p_meshes.mesh_surface_get_data(mesh, 0, latest) == OK && latest.source.attribute_data[0] == edit[0] && snapshot.source.attribute_data == data.attribute_data);
		FF_MESH_CHECK(p_meshes.mesh_surface_update_region(mesh, 0, FixedFunctionMeshStorage::BUFFER_VERTEX, INT32_MAX, edit) == ERR_INVALID_PARAMETER);
		if (!(flags & 1) || (flags & 2)) {
			Vector<uint8_t> invalid;
			invalid.resize(4);
			const float nan = std::numeric_limits<float>::quiet_NaN();
			memcpy(invalid.ptrw(), &nan, 4);
			FF_MESH_CHECK(p_meshes.mesh_surface_update_region(mesh, 0, FixedFunctionMeshStorage::BUFFER_VERTEX, 0, invalid) == ERR_INVALID_DATA);
			FF_MESH_CHECK(p_meshes.mesh_surface_get_data(mesh, 0, latest) == OK && latest.source.vertex_data == snapshot.source.vertex_data);
		}
		RenderingServerTypes::SurfaceData invalid = data;
		invalid.vertex_data.resize(data.vertex_data.size() - 1);
		FF_MESH_CHECK(p_meshes.mesh_add_surface(mesh, invalid) == ERR_INVALID_PARAMETER);
		invalid = data;
		invalid.format |= uint64_t(1) << 63;
		FF_MESH_CHECK(p_meshes.mesh_add_surface(mesh, invalid) == ERR_INVALID_PARAMETER);
		FF_MESH_CHECK(p_meshes.mesh_get_surface_count(mesh) == 1);
		FF_MESH_CHECK(p_meshes.mesh_remove_surface(mesh, 0) == OK && p_meshes.mesh_get_surface_count(mesh) == 0 && p_meshes.free(mesh) == OK);
	}
	return true;
}

bool TestFixedFunctionMeshStorage::_test_dependencies(FixedFunctionMeshStorage &p_meshes) {
	FFDependencyState state;
	state.storage = &p_meshes;
	state.mesh = p_meshes.mesh_create();
	state.other = p_meshes.mesh_create();
	DependencyTracker tracker;
	tracker.userdata = &state;
	tracker.changed_callback = _changed;
	tracker.deleted_callback = _deleted;
	tracker.update_begin();
	FF_MESH_CHECK(p_meshes.mesh_update_dependency(state.mesh, &tracker) == OK && p_meshes.mesh_update_dependency(state.other, &tracker) == OK);
	tracker.update_end();
	state.action = 1;
	FF_MESH_CHECK(p_meshes.mesh_set_custom_aabb(state.mesh, AABB(Vector3(), Vector3(1, 2, 3))) == OK && p_meshes.update_dependencies() == ERR_SKIP && state.success);
	FF_MESH_CHECK(p_meshes.update_dependencies() == OK && state.changed == 2);
	state.action = 2;
	FF_MESH_CHECK(p_meshes.mesh_set_custom_aabb(state.mesh, AABB()) == OK && p_meshes.mesh_set_custom_aabb(state.other, AABB()) == OK);
	FF_MESH_CHECK(p_meshes.update_dependencies() == OK && state.success && state.changed == 3 && state.deleted == 1);
	const RID replacement = p_meshes.mesh_create();
	FF_MESH_CHECK(replacement != state.other);
	state.action = 3;
	FF_MESH_CHECK(p_meshes.mesh_set_custom_aabb(state.mesh, AABB()) == OK && p_meshes.update_dependencies() == OK && state.deleted == 2 && p_meshes.get_mesh_count() == 0);
	return true;
}

bool TestFixedFunctionMeshStorage::_test_large_indices(FixedFunctionMeshStorage &p_meshes) {
	for (uint32_t count : { 65536U, 65538U }) {
		RenderingServerTypes::SurfaceData data;
		data.format |= RSE::ARRAY_FORMAT_VERTEX | RSE::ARRAY_FORMAT_INDEX;
		data.primitive = RSE::PRIMITIVE_TRIANGLES;
		data.vertex_count = count;
		data.vertex_data.resize_initialized(count * 12);
		data.index_count = 3;
		const uint32_t stride = count <= 65536 ? 2 : 4;
		data.index_data.resize_initialized(3 * stride);
		const uint32_t high = count - 1;
		memcpy(data.index_data.ptrw(), &high, stride);
		const RID mesh = p_meshes.mesh_create();
		FF_MESH_CHECK(p_meshes.mesh_add_surface(mesh, data) == OK);
		if (stride == 4) {
			Vector<uint8_t> invalid;
			invalid.resize(4);
			memcpy(invalid.ptrw(), &count, 4);
			FF_MESH_CHECK(p_meshes.mesh_surface_update_region(mesh, 0, FixedFunctionMeshStorage::BUFFER_INDEX, 0, invalid) == ERR_INVALID_DATA);
		}
		FixedFunctionMeshStorage::FFSurfaceData retained;
		FF_MESH_CHECK(p_meshes.mesh_surface_get_data(mesh, 0, retained) == OK && retained.source.index_data == data.index_data);
		FF_MESH_CHECK(p_meshes.free(mesh) == OK);
	}
	return true;
}

bool TestFixedFunctionMeshStorage::_test_revisions(FixedFunctionMeshStorage &p_meshes) {
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	Vector<Vector3> positions;
	positions.push_back(Vector3(-1, -1, 0));
	positions.push_back(Vector3(0, 1, 0));
	positions.push_back(Vector3(1, -1, 0));
	Vector<Vector2> uv;
	Vector<int> bones, indices;
	Vector<float> weights;
	for (int vertex = 0; vertex < 3; vertex++) {
		uv.push_back(Vector2(vertex, 0));
		indices.push_back(vertex);
		for (int bone = 0; bone < 4; bone++) {
			bones.push_back(0);
			weights.push_back(bone ? 0 : 1);
		}
	}
	arrays[RSE::ARRAY_VERTEX] = positions;
	arrays[RSE::ARRAY_TEX_UV] = uv;
	arrays[RSE::ARRAY_BONES] = bones;
	arrays[RSE::ARRAY_WEIGHTS] = weights;
	arrays[RSE::ARRAY_INDEX] = indices;
	RenderingServerTypes::SurfaceData data;
	FF_MESH_CHECK(RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&data, RSE::PRIMITIVE_TRIANGLES, arrays) == OK);
	const RID mesh = p_meshes.mesh_create();
	FF_MESH_CHECK(p_meshes.mesh_add_surface(mesh, data) == OK);
	FixedFunctionMeshStorage::FFSurfaceData previous, latest;
	FF_MESH_CHECK(p_meshes.mesh_surface_get_data(mesh, 0, previous) == OK && previous.identity && previous.version == 1);
	const Vector<uint8_t> buffers[] = { data.vertex_data, data.attribute_data, data.skin_data, data.index_data };
	for (int buffer = 0; buffer < 4; buffer++) {
		const FixedFunctionMeshStorage::FFBuffer kind = FixedFunctionMeshStorage::FFBuffer(buffer);
		FF_MESH_CHECK(!buffers[buffer].is_empty());
		FF_MESH_CHECK(p_meshes.mesh_surface_update_region(mesh, 0, kind, 0, Vector<uint8_t>()) == OK);
		FF_MESH_CHECK(p_meshes.mesh_surface_update_region(mesh, 0, kind, INT32_MAX, buffers[buffer]) == ERR_INVALID_PARAMETER);
		FF_MESH_CHECK(p_meshes.surface_is_current(previous.identity, previous.version));
		FF_MESH_CHECK(p_meshes.mesh_surface_update_region(mesh, 0, kind, 0, buffers[buffer]) == OK);
		FF_MESH_CHECK(p_meshes.mesh_surface_get_data(mesh, 0, latest) == OK && latest.identity == previous.identity && latest.version == previous.version + 1);
		FF_MESH_CHECK(!p_meshes.surface_is_current(previous.identity, previous.version) && p_meshes.surface_is_current(latest.identity, latest.version));
		previous = latest;
	}
	FF_MESH_CHECK(p_meshes.mesh_add_surface(mesh, data) == OK && p_meshes.mesh_surface_get_data(mesh, 1, latest) == OK && latest.identity != previous.identity);
	FF_MESH_CHECK(p_meshes.mesh_remove_surface(mesh, 0) == OK && !p_meshes.surface_is_current(previous.identity, previous.version));
	FF_MESH_CHECK(p_meshes.mesh_surface_get_data(mesh, 0, previous) == OK && previous.identity == latest.identity && previous.version == latest.version);
	FF_MESH_CHECK(p_meshes.free(mesh) == OK && !p_meshes.surface_is_current(previous.identity, previous.version));
	p_meshes.clear();
	const RID next = p_meshes.mesh_create();
	FF_MESH_CHECK(p_meshes.mesh_add_surface(next, data) == OK && p_meshes.mesh_surface_get_data(next, 0, latest) == OK && latest.identity > previous.identity);
	FF_MESH_CHECK(p_meshes.free(next) == OK);
	return true;
}

bool TestFixedFunctionMeshStorage::test() {
	FixedFunctionMaterialStorage materials;
	FixedFunctionMeshStorage meshes(materials);
	FF_MESH_CHECK(_test_layouts(meshes) && _test_large_indices(meshes) && _test_dependencies(meshes) && _test_revisions(meshes));
	const RID material = materials.material_create();
	const RID mesh = meshes.mesh_create();
	RenderingServerTypes::SurfaceData data;
	data.format |= RSE::ARRAY_FORMAT_VERTEX;
	data.primitive = RSE::PRIMITIVE_TRIANGLES;
	data.vertex_count = 3;
	data.vertex_data.resize_initialized(36);
	data.material = material;
	FF_MESH_CHECK(meshes.mesh_add_surface(mesh, data) == OK && meshes.mesh_surface_get_material(mesh, 0) == material);
	FF_MESH_CHECK(materials.free(material) == OK && !meshes.mesh_surface_get_material(mesh, 0).is_valid());
	FF_MESH_CHECK(meshes.mesh_set_path(mesh, "res://cpu.mesh") == OK && meshes.mesh_get_path(mesh) == "res://cpu.mesh");
	List<RenderingServerTypes::MeshInfo> info;
	meshes.mesh_debug_usage(&info);
	FF_MESH_CHECK(info.size() == 1 && info.front()->get().vertex_count == 3 && info.front()->get().vertex_buffer_size == 0);
	FF_MESH_CHECK(meshes.mesh_clear(mesh) == OK && meshes.mesh_set_blend_shape_count(mesh, 1) == OK && meshes.mesh_set_blend_shape_mode(mesh, RSE::BLEND_SHAPE_MODE_RELATIVE) == OK);
	data.material = RID();
	data.blend_shape_data = data.vertex_data;
	FF_MESH_CHECK(meshes.mesh_add_surface(mesh, data) == OK);
	const RID instance = meshes.mesh_instance_create(mesh);
	FF_MESH_CHECK(instance.is_valid() && meshes.mesh_instance_get_draw_status(instance, mesh) == OK);
	FF_MESH_CHECK(meshes.mesh_instance_set_blend_shape_weight(instance, 0, 0.5) == OK && meshes.mesh_instance_get_draw_status(instance, mesh) == OK);
	FF_MESH_CHECK(meshes.mesh_instance_set_blend_shape_weight(instance, 0, std::numeric_limits<float>::infinity()) == ERR_INVALID_PARAMETER);
	FF_MESH_CHECK(meshes.mesh_instance_set_blend_shape_weight(instance, 0, 0) == OK && meshes.mesh_instance_get_draw_status(instance, mesh) == OK);
	FF_MESH_CHECK(meshes.free(mesh) == OK && meshes.mesh_instance_get_draw_status(instance, mesh) == ERR_INVALID_PARAMETER && meshes.free(instance) == OK);
	printf("PASS: 4.x mesh CPU snapshots, packed/compressed layouts, bounds, atomic edits, index boundary, materials, blend metadata, native dependency mutation and release.\n");
	return true;
}

#undef FF_MESH_CHECK
