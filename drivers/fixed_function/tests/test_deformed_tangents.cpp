#include "drivers/fixed_function/tests/test_fixed_function_scene_cpu.h"
#include "drivers/fixed_function/tests/test_lambert.h"

#include <stdio.h>
#include <string.h>

#define FF_TANGENT_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Deformed tangent FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionSceneCPU::test_deformed_tangents() {
	FFUnusedDevice unused;
	FixedFunctionRenderContext context(unused, nullptr, 128, 96, false);
	FixedFunctionMeshStorage &meshes = context.get_mesh_storage();
	const RID skeleton = meshes.skeleton_allocate();
	meshes.skeleton_initialize(skeleton);
	FF_TANGENT_CHECK(meshes.skeleton_allocate_data(skeleton, 2) == OK);
	Transform3D bone;
	bone.basis.set_column(0, Vector3(-2, 0, 0));
	bone.basis.set_column(1, Vector3(0.5, 1, 0));
	bone.basis.set_column(2, Vector3(0, 0, 3));
	bone.origin = Vector3(1, 2, 3);
	FF_TANGENT_CHECK(meshes.skeleton_bone_set_transform(skeleton, 1, bone) == OK);
	for (int eight = 0; eight < 2; eight++) {
		for (int relative = 0; relative < 2; relative++) {
			const RID mesh = TestFixedFunctionLambert::_animated_normal_mesh(meshes, eight ? uint64_t(RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS) : 0, relative, -1);
			FF_TANGENT_CHECK(mesh.is_valid());
			const RID instance = meshes.mesh_instance_create(mesh);
			FF_TANGENT_CHECK(meshes.mesh_instance_set_skeleton(instance, skeleton) == OK && meshes.mesh_instance_set_blend_shape_weight(instance, 0, 0.5f) == OK);
			FixedFunctionMeshStorage::FFSurfaceData surface;
			FF_TANGENT_CHECK(meshes.mesh_surface_get_data(mesh, 0, surface) == OK);
			// Native 4.x surfaces can place the skin in a different coordinate system.
			surface.source.mesh_to_skeleton_xform = Transform3D(Basis(Vector3(0, 1, 0), real_t(0.2)), Vector3(0.2, 0.1, 0));
			const Vector<uint8_t> original = surface.source.vertex_data;
			Vector<FixedFunctionMeshStorage::FFDeformedVertex> vertices, ordinary, positions_only;
			Vector<FixedFunctionMeshStorage::FFDeformedTangentFrame> frames;
			AABB bounds, ordinary_bounds;
			FF_TANGENT_CHECK(meshes.mesh_instance_deform(instance, surface, vertices, bounds, &frames) == OK && frames.size() == 3);
			const Vector4 source_t = FixedFunctionMeshStorage::surface_get_tangent(surface, surface.source.vertex_data.ptr(), 0);
			const Vector4 shape_t = FixedFunctionMeshStorage::surface_get_tangent(surface, surface.source.blend_shape_data.ptr(), 0);
			Vector3 tangent(source_t.x, source_t.y, source_t.z);
			const Vector3 target(shape_t.x, shape_t.y, shape_t.z);
			tangent += (relative ? target : target - tangent) * real_t(0.5);
			const Vector3 bitangent = Vector3(0, 0, 1).cross(tangent).normalized() * source_t.w;
			const Basis weighted = Basis() * real_t(0.25) + bone.basis * real_t(0.75);
			const Basis conversion = surface.source.mesh_to_skeleton_xform.basis;
			const Basis skin = conversion.inverse() * weighted * conversion;
			FF_TANGENT_CHECK(frames[0].tangent.distance_to(skin.xform(tangent).normalized()) < real_t(0.0002));
			FF_TANGENT_CHECK(frames[0].bitangent.distance_to(skin.xform(bitangent).normalized()) < real_t(0.0002));
			FF_TANGENT_CHECK(Math::abs(frames[0].tangent.dot(frames[0].bitangent)) > real_t(0.1));
			FF_TANGENT_CHECK(meshes.mesh_instance_deform(instance, surface, ordinary, ordinary_bounds) == OK && ordinary_bounds == bounds);
			FF_TANGENT_CHECK(meshes.mesh_instance_deform(instance, surface, positions_only, ordinary_bounds, nullptr, nullptr, true) == OK && ordinary_bounds == bounds);
			FF_TANGENT_CHECK(positions_only.size() == vertices.size());
			for (int i = 0; i < vertices.size(); i++) {
				FF_TANGENT_CHECK(vertices[i].position == ordinary[i].position && vertices[i].normal == ordinary[i].normal);
				FF_TANGENT_CHECK(positions_only[i].position == vertices[i].position && positions_only[i].normal == Vector3());
			}
			FF_TANGENT_CHECK(memcmp(original.ptr(), surface.source.vertex_data.ptr(), original.size()) == 0);
			FF_TANGENT_CHECK(meshes.free(instance) == OK && meshes.free(mesh) == OK);
		}
	}

	const RID source_mesh = TestFixedFunctionLambert::_animated_normal_mesh(meshes, 0, false, 1);
	FixedFunctionMeshStorage::FFSurfaceData surface;
	FF_TANGENT_CHECK(meshes.mesh_surface_get_data(source_mesh, 0, surface) == OK);
	RenderingServerTypes::SurfaceData data = surface.source;
	data.mesh_to_skeleton_xform = Transform3D(Basis(Vector3(0, 1, 0), real_t(0.2)), Vector3(0.2, 0.1, 0));
	FF_TANGENT_CHECK(data.bone_aabbs.size() == 2);
	data.bone_aabbs.fill(AABB()); // Stale metadata must not shrink bounds for decoded positions.
	const float distant_vertex = 40;
	memcpy(data.blend_shape_data.ptrw() + surface.offsets[RSE::ARRAY_VERTEX], &distant_vertex, sizeof(distant_vertex));
	FF_TANGENT_CHECK(meshes.free(source_mesh) == OK);

	const RID mesh = meshes.mesh_create();
	FF_TANGENT_CHECK(meshes.mesh_set_blend_shape_count(mesh, 1) == OK && meshes.mesh_add_surface(mesh, data) == OK);
	const RID instance = meshes.mesh_instance_create(mesh);
	FF_TANGENT_CHECK(meshes.mesh_instance_set_skeleton(instance, skeleton) == OK);
	FF_TANGENT_CHECK(meshes.mesh_surface_get_data(mesh, 0, surface) == OK && surface.skinning_bounds.size() == 2);
	Vector<FixedFunctionMeshStorage::FFDeformedVertex> posed;
	AABB bind_pose_bounds, posed_bounds, culling_bounds;
	FF_TANGENT_CHECK(meshes.mesh_instance_deform(instance, surface, posed, posed_bounds) == OK);
	FF_TANGENT_CHECK(meshes.mesh_get_aabb(mesh, skeleton, bind_pose_bounds) == OK && bind_pose_bounds.encloses(posed_bounds));
	FF_TANGENT_CHECK(meshes.mesh_instance_set_blend_shape_weight(instance, 0, 1) == OK);
	FF_TANGENT_CHECK(meshes.mesh_instance_deform(instance, surface, posed, posed_bounds) == OK && !bind_pose_bounds.encloses(posed_bounds));
	FF_TANGENT_CHECK(meshes.mesh_get_aabb(mesh, skeleton, culling_bounds) == OK && culling_bounds.encloses(posed_bounds));
	{
		FixedFunctionMaterialStorage &materials = context.get_material_storage();
		const RID base_shader = materials.shader_create();
		const RID next_shader = materials.shader_create();
		FF_TANGENT_CHECK(materials.shader_set_code(base_shader, "shader_type spatial; render_mode unshaded; void fragment() { ALBEDO = vec3(1.0); }") == OK);
		FF_TANGENT_CHECK(materials.shader_set_code(next_shader, "shader_type spatial; void fragment() { ALBEDO = vec3(1.0); }") == OK);
		const RID base_material = materials.material_create();
		const RID next_material = materials.material_create();
		FF_TANGENT_CHECK(materials.material_set_shader(base_material, base_shader) == OK && materials.material_set_shader(next_material, next_shader) == OK);
		FF_TANGENT_CHECK(materials.material_set_next_pass(base_material, next_material) == OK);

		FixedFunctionRenderTargetManager targets(context);
		FixedFunctionSceneRenderer renderer(context, targets);
		FixedFunctionGeometryInstance geometry(context, mesh);
		geometry.mesh_instance = instance;
		geometry.set_material_override(base_material);
		FF_TANGENT_CHECK(renderer._prepare_mesh(geometry, mesh, 0, Transform3D(), Transform3D(), Transform3D(), Projection(), Plane(), false, false, Color(1, 1, 1)) == OK);
		FF_TANGENT_CHECK(renderer._packets.size() == 2 && renderer._packets[0].deformed.size() == 3 && renderer._packets[1].deformed.size() == 3);
		FF_TANGENT_CHECK(renderer._packets[0].deformed[0].position == posed[0].position && renderer._packets[0].deformed[0].normal == Vector3());
		FF_TANGENT_CHECK(renderer._packets[1].deformed[0].position == posed[0].position && renderer._packets[1].deformed[0].normal == posed[0].normal);
		FF_TANGENT_CHECK(materials.free(base_material) == OK && materials.free(next_material) == OK);
		FF_TANGENT_CHECK(materials.free(base_shader) == OK && materials.free(next_shader) == OK);
	}
	FF_TANGENT_CHECK(meshes.mesh_instance_set_blend_shape_weight(instance, 0, 0) == OK);

	const RID edited_mesh = meshes.mesh_create();
	FF_TANGENT_CHECK(meshes.mesh_set_blend_shape_count(edited_mesh, 1) == OK && meshes.mesh_add_surface(edited_mesh, data) == OK);
	Vector<uint8_t> edited_weight;
	FF_TANGENT_CHECK(edited_weight.resize(sizeof(uint16_t)) == OK);
	const uint16_t full_weight = UINT16_MAX;
	memcpy(edited_weight.ptrw(), &full_weight, sizeof(full_weight));
	FF_TANGENT_CHECK(meshes.mesh_surface_update_region(edited_mesh, 0, FixedFunctionMeshStorage::BUFFER_SKIN, surface.offsets[RSE::ARRAY_WEIGHTS], edited_weight) == OK);
	FF_TANGENT_CHECK(meshes.mesh_surface_get_data(edited_mesh, 0, surface) == OK && surface.skinning_bounds.is_empty());
	FF_TANGENT_CHECK(meshes.free(edited_mesh) == OK);

	Vector<uint8_t> edited_position;
	FF_TANGENT_CHECK(edited_position.resize(sizeof(distant_vertex)) == OK);
	memcpy(edited_position.ptrw(), &distant_vertex, sizeof(distant_vertex));
	FF_TANGENT_CHECK(meshes.mesh_surface_update_region(mesh, 0, FixedFunctionMeshStorage::BUFFER_VERTEX, surface.offsets[RSE::ARRAY_VERTEX], edited_position) == OK);
	FF_TANGENT_CHECK(meshes.mesh_surface_get_data(mesh, 0, surface) == OK && surface.skinning_bounds.is_empty());
	FF_TANGENT_CHECK(meshes.mesh_instance_deform(instance, surface, posed, posed_bounds) == OK && !bind_pose_bounds.encloses(posed_bounds));
	FF_TANGENT_CHECK(meshes.mesh_get_aabb(mesh, skeleton, culling_bounds) == OK && culling_bounds.encloses(posed_bounds));
	FF_TANGENT_CHECK(meshes.free(instance) == OK && meshes.free(mesh) == OK);
	meshes.skeleton_free(skeleton);
	printf("PASS: 4.x tangent morph/sign, four/eight influences, mesh-to-skeleton conversion, optional frames, position-only draws and conservative bounds.\n");
	return true;
}

#undef FF_TANGENT_CHECK
