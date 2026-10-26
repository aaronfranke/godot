#include "test_lambert.h"

#include "core/project_settings.h"

#include <stdio.h>

#define FF_SKIN_CHECK(m_condition)                                       \
	do {                                                                 \
		if (!(m_condition)) {                                            \
			printf("Skinning FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                \
		}                                                                \
	} while (false)

bool TestFixedFunctionLambert::_test_skinning(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	FixedFunctionSceneStorage &scenes = p_context.get_scene_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	const RID environment = scenes.environment_create();
	FF_SKIN_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1); }") == OK);
	FF_SKIN_CHECK(materials.material_set_shader(material, shader) == OK);
	FF_SKIN_CHECK(scenes.environment_set_ambient(environment, Color(0.125f, 0.125f, 0.125f), 1, 0) == OK);
	const RID light = lights.light_create(VS::LIGHT_DIRECTIONAL);
	const RID light_instance = lights.light_instance_create(light);
	FixedFunctionLightStorage::FFLightData data;
	FF_SKIN_CHECK(lights.light_get_data(light, data) == OK);
	data.parameters[VS::LIGHT_PARAM_ENERGY] = 0.5f;
	FF_SKIN_CHECK(lights.light_set_data(light, data) == OK);
	Vector<RID> active;
	active.push_back(light_instance);
	std::vector<uint32_t> pixels;
	const int center = 48 * 128 + 64;
	const int moved = center + 32;
	VisualServer *server = VisualServer::get_singleton();
	for (int wide = 0; wide < 2; wide++) {
		const int second_bone = wide ? 256 : 1;
		Array arrays;
		arrays.resize(VS::ARRAY_MAX);
		PoolVector<Vector3> positions, normals;
		PoolIntArray bones, indices;
		PoolRealArray weights;
		positions.push_back(Vector3(-0.65f, -0.65f, 0));
		positions.push_back(Vector3(0, 0.65f, 0));
		positions.push_back(Vector3(0.65f, -0.65f, 0));
		for (int i = 0; i < 3; i++) {
			indices.push_back(i);
			normals.push_back(Vector3(0, 0, 1));
			bones.push_back(0);
			bones.push_back(second_bone);
			bones.push_back(0);
			bones.push_back(0);
			weights.push_back(0.25f);
			weights.push_back(0.25f);
			weights.push_back(0);
			weights.push_back(0);
		}
		// The wide-influence case also exercises the indexed lighting stream.
		if (wide) {
			for (int i = 0; i < 6; i++) {
				indices.push_back(i % 3);
			}
		}
		arrays[VS::ARRAY_VERTEX] = positions;
		arrays[VS::ARRAY_NORMAL] = normals;
		arrays[VS::ARRAY_BONES] = bones;
		arrays[VS::ARRAY_WEIGHTS] = weights;
		arrays[VS::ARRAY_INDEX] = indices;
		const RID source = server->mesh_create();
		server->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_TRIANGLES, arrays, Array(), wide ? uint32_t(VS::ARRAY_COMPRESS_WEIGHTS) : 0);
		const uint32_t format = server->mesh_surface_get_format(source, 0);
		FF_SKIN_CHECK(bool(format & VS::ARRAY_FLAG_USE_16_BIT_BONES) == bool(wide));
		FF_SKIN_CHECK(bool(format & VS::ARRAY_COMPRESS_WEIGHTS) == bool(wide));
		const RID mesh = meshes.mesh_create();
		const AABB rest = server->mesh_surface_get_aabb(source, 0);
		const Vector<AABB> bone_bounds = wide ? Vector<AABB>() : server->mesh_surface_get_skeleton_aabb(source, 0);
		FF_SKIN_CHECK(meshes.mesh_add_surface(mesh, format, VS::PRIMITIVE_TRIANGLES, server->mesh_surface_get_array(source, 0), 3, server->mesh_surface_get_index_array(source, 0), indices.size(), rest, Vector<PoolVector<uint8_t>>(), bone_bounds) == OK);
		server->free(source);

		const RID skeleton = meshes.skeleton_create();
		FF_SKIN_CHECK(meshes.skeleton_allocate(skeleton, second_bone + 1) == OK);
		FFInstance instance(mesh, material);
		instance.skeleton = skeleton;
		FF_SKIN_CHECK(meshes.instance_add_skeleton(skeleton, &instance) == OK);
		FF_SKIN_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff9f9f9f));
		const Basis rotated(Vector3(0, 1, 0), real_t(0.6));
		const uint32_t revision = meshes.skeleton_get_revision(skeleton);
		FF_SKIN_CHECK(meshes.skeleton_set_bone_transform(skeleton, 0, Transform(rotated, Vector3())) == OK);
		FF_SKIN_CHECK(meshes.skeleton_set_bone_transform(skeleton, second_bone, Transform(rotated, Vector3(2, 0, 0))) == OK);
		FF_SKIN_CHECK(meshes.skeleton_get_revision(skeleton) == revision + 2);
		AABB posed;
		FF_SKIN_CHECK(meshes.mesh_get_aabb(mesh, skeleton, posed) == OK && posed.encloses(rest) && posed.position.x + posed.size.x > 2);
		FF_SKIN_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_SKIN_CHECK(_color(pixels[center], 0xff0000ff) && _color(pixels[moved], 0xff898989));
		if (wide) {
			FF_SKIN_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK);
			FF_SKIN_CHECK(p_context.resize(128, 96) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[moved], 0xff898989));
		}
		if (p_context.get_device().get_max_streamed_vertex_lights()) {
			ProjectSettings *settings = ProjectSettings::get_singleton();
			const char *native = "rendering/quality/fixed_function/native_lighting";
			const char *per_pixel = "rendering/quality/fixed_function/per_pixel_lighting";
			const Variant previous_native = settings->get(native);
			const Variant previous_per_pixel = settings->get(per_pixel);
			settings->set(per_pixel, false);
			settings->set(native, 0);
			instance.transform.basis = Basis().scaled(Vector3(0.8f, 1.2f, 0.55f));
			const Transform camera(Basis(Vector3(0, 1, 0), real_t(0.17)), Vector3());
			std::vector<uint32_t> reference;
			FF_SKIN_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, reference, camera));
			settings->set(native, 1);
			FF_SKIN_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels, camera));
			int drawn = 0;
			for (size_t i = 0; i < pixels.size(); i++) {
				FF_SKIN_CHECK(_color(pixels[i], reference[i]));
				drawn += reference[i] != 0xff0000ff;
			}
			FF_SKIN_CHECK(drawn > 100);
			settings->set(native, previous_native);
			settings->set(per_pixel, previous_per_pixel);
			instance.transform.basis = Basis();
			printf("PASS: streamed vertex lighting matches CPU on a posed, non-uniformly scaled mesh with a rotated camera.\n");
		}
		FF_SKIN_CHECK(meshes.free(skeleton) == OK && !instance.skeleton.is_valid());
		FF_SKIN_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff9f9f9f));
		FF_SKIN_CHECK(meshes.free(mesh) == OK);
	}
	FF_SKIN_CHECK(lights.free(light_instance) == OK && lights.free(light) == OK && scenes.free(environment) == OK);
	FF_SKIN_CHECK(materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: CPU skinning positions and normals, packed and unpacked influences, pose bounds, reset and detachment.\n");
	return true;
}

#undef FF_SKIN_CHECK
