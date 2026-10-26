#include "test_lambert.h"

#include <stdio.h>
#include <string.h>

#define FF_MORPH_CHECK(m_condition)                                          \
	do {                                                                     \
		if (!(m_condition)) {                                                \
			printf("Blend shapes FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                    \
		}                                                                    \
	} while (false)

bool TestFixedFunctionLambert::_test_blend_shapes(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	FixedFunctionSceneStorage &scenes = p_context.get_scene_storage();
	VisualServer *server = VisualServer::get_singleton();
	Array arrays, target;
	arrays.resize(VS::ARRAY_MAX);
	target.resize(VS::ARRAY_MAX);
	PoolVector<Vector3> positions, target_positions, normals, target_normals;
	PoolVector<Vector2> uvs, target_uvs;
	PoolVector<Color> colors, target_colors;
	PoolIntArray bones, indices;
	PoolRealArray weights, target_weights;
	positions.push_back(Vector3(-0.45f, -0.45f, 0));
	positions.push_back(Vector3(0, 0.45f, 0));
	positions.push_back(Vector3(0.45f, -0.45f, 0));
	for (int i = 0; i < 3; i++) {
		target_positions.push_back(positions[i] + Vector3(1, 0, 0));
		normals.push_back(Vector3(0, 0, 1));
		target_normals.push_back(Vector3(1, 0, 0));
		uvs.push_back(Vector2(0.25f, 0.75f));
		target_uvs.push_back(Vector2(0.75f, 0.25f));
		colors.push_back(Color(1, 0, 0, 0.25f));
		target_colors.push_back(Color(0, 0, 1, 0.75f));
		for (int j = 0; j < VS::ARRAY_WEIGHTS_SIZE; j++) {
			bones.push_back(j == 1 ? 1 : 0);
			weights.push_back(j == 0 ? 1 : 0);
			target_weights.push_back(j == 1 ? 1 : 0);
		}
		indices.push_back(i);
	}
	arrays[VS::ARRAY_VERTEX] = positions;
	arrays[VS::ARRAY_NORMAL] = normals;
	arrays[VS::ARRAY_TEX_UV2] = uvs;
	arrays[VS::ARRAY_COLOR] = colors;
	arrays[VS::ARRAY_BONES] = bones;
	arrays[VS::ARRAY_WEIGHTS] = weights;
	arrays[VS::ARRAY_INDEX] = indices;
	target[VS::ARRAY_VERTEX] = target_positions;
	target[VS::ARRAY_NORMAL] = target_normals;
	target[VS::ARRAY_TEX_UV2] = target_uvs;
	target[VS::ARRAY_COLOR] = target_colors;
	target[VS::ARRAY_BONES] = bones;
	target[VS::ARRAY_WEIGHTS] = target_weights;
	Array targets;
	targets.push_back(target);
	const RID source = server->mesh_create();
	server->mesh_set_blend_shape_count(source, 1);
	server->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_TRIANGLES, arrays, targets, VS::ARRAY_COMPRESS_WEIGHTS);
	const RID mesh = meshes.mesh_create();
	FF_MORPH_CHECK(meshes.mesh_set_blend_shape_count(mesh, 1) == OK);
	FF_MORPH_CHECK(meshes.mesh_add_surface(mesh, server->mesh_surface_get_format(source, 0), VS::PRIMITIVE_TRIANGLES, server->mesh_surface_get_array(source, 0), 3, server->mesh_surface_get_index_array(source, 0), 3, server->mesh_surface_get_aabb(source, 0), server->mesh_surface_get_blend_shapes(source, 0), server->mesh_surface_get_skeleton_aabb(source, 0)) == OK);
	server->free(source);

	const RID shader = materials.shader_create(), material = materials.material_create(), environment = scenes.environment_create();
	FF_MORPH_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1); }") == OK);
	FF_MORPH_CHECK(materials.material_set_shader(material, shader) == OK);
	FF_MORPH_CHECK(scenes.environment_set_ambient(environment, Color(0.125f, 0.125f, 0.125f), 1, 0) == OK);
	const RID light = lights.light_create(VS::LIGHT_DIRECTIONAL), light_instance = lights.light_instance_create(light);
	FixedFunctionLightStorage::FFLightData light_data;
	FF_MORPH_CHECK(lights.light_get_data(light, light_data) == OK);
	light_data.parameters[VS::LIGHT_PARAM_ENERGY] = 0.5f;
	FF_MORPH_CHECK(lights.light_set_data(light, light_data) == OK);
	Vector<RID> active;
	active.push_back(light_instance);
	FFInstance instance(mesh, material);
	instance.blend_values.push_back(0.5f);
	PoolVector<float> unrelated_weights;
	unrelated_weights.push_back(0);
	FF_MORPH_CHECK(meshes.mesh_set_blend_shape_values(mesh, unrelated_weights) == OK);
	std::vector<uint32_t> pixels;
	const int center = 48 * 128 + 64;
	FF_MORPH_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_MORPH_CHECK(_color(pixels[center], 0xff0000ff) && _color(pixels[center + 16], 0xff7a7a7a));

	const RID skeleton = meshes.skeleton_create();
	FF_MORPH_CHECK(meshes.skeleton_allocate(skeleton, 2) == OK);
	FF_MORPH_CHECK(meshes.skeleton_set_bone_transform(skeleton, 1, Transform(Basis(), Vector3(1, 0, 0))) == OK);
	instance.skeleton = skeleton;
	FF_MORPH_CHECK(meshes.instance_add_skeleton(skeleton, &instance) == OK);
	FF_MORPH_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_MORPH_CHECK(_color(pixels[center + 16], 0xff0000ff) && _color(pixels[center + 32], 0xff7a7a7a));
	FF_MORPH_CHECK(meshes.free(skeleton) == OK);

	FixedFunctionSceneRenderer::FFDrawPacket normalized, relative;
	FF_MORPH_CHECK(meshes.mesh_surface_get_data(mesh, 0, normalized.surface) == OK);
	normalized.material.albedo_uses_vertex_color = true;
	relative.surface = normalized.surface;
	relative.material = normalized.material;
	FF_MORPH_CHECK(FixedFunctionSceneRenderer::_prepare_deformation(normalized, Vector<Transform>(), instance.blend_values, VS::BLEND_SHAPE_MODE_NORMALIZED) == OK);
	FF_MORPH_CHECK(FixedFunctionSceneRenderer::_prepare_deformation(relative, Vector<Transform>(), instance.blend_values, VS::BLEND_SHAPE_MODE_RELATIVE) == OK);
	FF_MORPH_CHECK(Math::is_equal_approx(normalized.deformed_vertices[0].position.x, real_t(0.05)));
	FF_MORPH_CHECK(Math::is_equal_approx(relative.deformed_vertices[0].position.x, real_t(-0.175)));
	FF_MORPH_CHECK(normalized.morph_attributes[0].uv[1] == Vector2(0.5f, 0.5f));
	FF_MORPH_CHECK(normalized.morph_attributes[0].color == Color(0.5f, 0, 0.5f, 0.5f));
	FF_MORPH_CHECK(relative.morph_attributes[0].color == Color(1, 0, 0.5f, 0.625f));
	const float edited_x = -5;
	PoolVector<uint8_t> edit;
	FF_MORPH_CHECK(edit.resize(sizeof(edited_x)) == OK);
	{
		PoolVector<uint8_t>::Write bytes = edit.write();
		memcpy(bytes.ptr(), &edited_x, sizeof(edited_x));
	}
	FF_MORPH_CHECK(meshes.mesh_surface_update_region(mesh, 0, normalized.surface.offsets[VS::ARRAY_VERTEX], edit) == OK);
	AABB bounds;
	FF_MORPH_CHECK(meshes.mesh_get_aabb(mesh, RID(), bounds) == OK && bounds.position.x < -10);
	FF_MORPH_CHECK(meshes.mesh_set_blend_shape_mode(mesh, VS::BLEND_SHAPE_MODE_RELATIVE) == OK);
	FF_MORPH_CHECK(meshes.mesh_get_aabb(mesh, RID(), bounds) == OK && bounds.position.x < -6 && bounds.position.x > -7);
	FF_MORPH_CHECK(meshes.free(mesh) == OK && materials.free(material) == OK && materials.free(shader) == OK);
	FF_MORPH_CHECK(scenes.free(environment) == OK && lights.free(light_instance) == OK && lights.free(light) == OK);
	printf("PASS: CPU blend shapes, per-instance weights, normalized and relative modes, blended normals/UV2/color, morph-before-skin and edited bounds.\n");
	return true;
}

#undef FF_MORPH_CHECK
