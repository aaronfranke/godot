#include "test_retained_lighting.h"

#include "core/project_settings.h"
#include "servers/visual_server.h"

#include <stdio.h>
#include <string.h>

#define FF_AO_CHECK(m_condition)                                             \
	do {                                                                     \
		if (!(m_condition)) {                                                \
			printf("Transient AO FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                    \
		}                                                                    \
	} while (false)

RID TestFixedFunctionRetainedLighting::_occlusion_mesh(FixedFunctionMeshStorage &p_storage, int p_triangles) {
	Array arrays, target, targets;
	arrays.resize(VS::ARRAY_MAX);
	PoolVector<Vector3> positions, normals;
	PoolVector<Vector2> uv, uv2;
	PoolVector<Color> colors;
	PoolIntArray bones;
	PoolRealArray weights;
	const Vector3 points[] = { Vector3(-1, -1, 0), Vector3(0, 1, 0), Vector3(1, -1, 0) };
	const Vector2 coordinates[] = { Vector2(0, 0), Vector2(0.5, 1), Vector2(1, 0) };
	for (int triangle = 0; triangle < p_triangles; triangle++) {
		for (int i = 0; i < 3; i++) {
			positions.push_back(points[i]);
			normals.push_back(Vector3(0, 0, 1));
			uv.push_back(Vector2(0.1, 0.1));
			uv2.push_back(coordinates[i]);
			colors.push_back(Color(0.5, 0.75, 1, 0.5));
			for (int j = 0; j < 4; j++) {
				bones.push_back(0);
				weights.push_back(j == 0 ? 1 : 0);
			}
		}
	}
	arrays[VS::ARRAY_VERTEX] = positions;
	arrays[VS::ARRAY_NORMAL] = normals;
	arrays[VS::ARRAY_TEX_UV] = uv;
	arrays[VS::ARRAY_TEX_UV2] = uv2;
	arrays[VS::ARRAY_COLOR] = colors;
	arrays[VS::ARRAY_BONES] = bones;
	arrays[VS::ARRAY_WEIGHTS] = weights;
	target = arrays.duplicate();
	targets.push_back(target);
	VisualServer *server = VisualServer::get_singleton();
	const RID source = server->mesh_create();
	server->mesh_set_blend_shape_count(source, 1);
	server->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_TRIANGLES, arrays, targets, 0);
	const RID mesh = p_storage.mesh_create();
	Error error = p_storage.mesh_set_blend_shape_count(mesh, 1);
	if (error == OK)
		error = p_storage.mesh_add_surface(mesh, server->mesh_surface_get_format(source, 0), VS::PRIMITIVE_TRIANGLES, server->mesh_surface_get_array(source, 0), positions.size(), PoolVector<uint8_t>(), 0, server->mesh_surface_get_aabb(source, 0), server->mesh_surface_get_blend_shapes(source, 0), Vector<AABB>());
	server->free(source);
	return error == OK ? mesh : RID();
}

bool TestFixedFunctionRetainedLighting::_transient_occlusion(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	TestFixedFunctionRetainedScene::FFDevice &device = static_cast<TestFixedFunctionRetainedScene::FFDevice &>(p_context.get_device());
	device.set_transient_occlusion_enabled(true);
	if (!device.supports_transient_occlusion_mapping()) {
		device.set_transient_occlusion_enabled(false);
		return true;
	}
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionSceneStorage &scenes = p_context.get_scene_storage();
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const bool previous_ao = GLOBAL_GET("rendering/quality/fixed_function/texture_occlusion");
	const bool previous_pixel = GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting");
	const bool previous_retained = GLOBAL_GET("rendering/quality/fixed_function/retained_geometry");
	settings->set("rendering/quality/fixed_function/texture_occlusion", true);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", true);
	const RID shader = materials.shader_create(), material = materials.material_create(), map = textures.texture_create();
	const String source = "shader_type spatial; render_mode cull_disabled$MODE; uniform sampler2D orm; uniform float affect=0.5; uniform float cutoff=0.3; void fragment(){ ALBEDO=vec3(0.5)*COLOR.rgb; ALPHA=0.75*COLOR.a; ALPHA_SCISSOR=cutoff; AO=texture(orm,UV2).g; AO_LIGHT_AFFECT=affect; }";
	FF_AO_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, source.replace("$MODE", "")) == OK);
	Ref<Image> image = memnew(Image(4, 4, false, Image::FORMAT_RGBA8));
	image->fill(Color(0, 1, 0, 0));
	image->lock();
	for (int y = 1; y < 3; y++)
		for (int x = 1; x < 3; x++)
			image->set_pixel(x, y, Color(1, 0.25, 0, 0));
	image->unlock();
	FF_AO_CHECK(textures.texture_set_data(map, image) == OK && materials.material_set_param(material, "orm", map) == OK);
	const RID environment = scenes.environment_create();
	FF_AO_CHECK(scenes.environment_set_ambient(environment, Color(0.2, 0.2, 0.2), 1, 0) == OK);
	const RID light = lights.light_create(VS::LIGHT_DIRECTIONAL), light_instance = lights.light_instance_create(light);
	FixedFunctionLightStorage::FFLightData light_data;
	FF_AO_CHECK(lights.light_get_data(light, light_data) == OK);
	light_data.parameters[VS::LIGHT_PARAM_ENERGY] = 0.4f;
	FF_AO_CHECK(lights.light_set_data(light, light_data) == OK);
	Vector<RID> active;
	active.push_back(light_instance);
	const RID skeleton = meshes.skeleton_create();
	FF_AO_CHECK(meshes.skeleton_allocate(skeleton, 1) == OK);
	const RID mesh = _occlusion_mesh(meshes);
	FF_AO_CHECK(mesh.is_valid());
	TestFixedFunctionLambert::FFInstance instance(mesh, material);
	instance.skeleton = skeleton;
	FF_AO_CHECK(meshes.instance_add_skeleton(skeleton, &instance) == OK);
	instance.blend_values.push_back(0.5f);
	std::vector<uint32_t> pixels, reference, cpu;
	const size_t center = 48 * 128 + 64;
	// Every vertex samples white; only native pixel AO sees the dark interior texels.
	const Color albedo(0.25, 0.375, 0.5);
	for (int pose = 0; pose < 3; pose++) {
		Transform bone;
		bone.basis.set_axis(0, Vector3(1.1, 0.1, 0));
		bone.basis.set_axis(1, Vector3(0.05, 0.9, 0));
		bone.origin.x = pose * real_t(0.02);
		FF_AO_CHECK(meshes.skeleton_set_bone_transform(skeleton, 0, bone) == OK);
		instance.blend_values.write()[0] = pose * 0.5f;
		const int before = device.get_occlusion_draws();
		FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_AO_CHECK(device.get_occlusion_draws() == before + 1 && TestFixedFunctionLambert::_color(pixels[center], Color(albedo.r * 0.3, albedo.g * 0.3, albedo.b * 0.3).to_argb32()));
	}
	reference = pixels;
	device.set_transient_occlusion_enabled(false);
	FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, cpu));
	FF_AO_CHECK(TestFixedFunctionLambert::_color(cpu[center], Color(albedo.r * 0.6, albedo.g * 0.6, albedo.b * 0.6).to_argb32()) && reference != cpu);
	device.set_transient_occlusion_enabled(true);
	for (int affect = 0; affect < 2; affect++) {
		FF_AO_CHECK(materials.material_set_param(material, "affect", affect) == OK);
		const float value = affect ? 0.15f : 0.45f;
		FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_AO_CHECK(TestFixedFunctionLambert::_color(pixels[center], Color(albedo.r * value, albedo.g * value, albedo.b * value).to_argb32()));
	}
	FF_AO_CHECK(materials.material_set_param(material, "affect", 0.5) == OK);
	FF_AO_CHECK(materials.material_set_param(material, "cutoff", 0.4) == OK && TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && pixels[center] == 0xff0000ff);
	FF_AO_CHECK(materials.material_set_param(material, "cutoff", 0.3) == OK);
	// Both ordinary transparent blends retain independent vertex opacity.
	for (int additive = 0; additive < 2; additive++) {
		const String mode = additive ? ",blend_add" : "";
		FF_AO_CHECK(materials.shader_set_code(shader, source.replace("$MODE", mode).replace("ALPHA_SCISSOR=cutoff;", "")) == OK);
		FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		const Color lit(albedo.r * 0.3, albedo.g * 0.3, albedo.b * 0.3);
		Color expected(lit.r * 0.375, lit.g * 0.375, 1);
		if (!additive) {
			expected.b = lit.b * 0.375 + 0.625;
		}
		FF_AO_CHECK(TestFixedFunctionLambert::_color(pixels[center], expected.to_argb32()));
	}
	FF_AO_CHECK(materials.shader_set_code(shader, source.replace("$MODE", "")) == OK);
	// Optional pre-submission refusal must preserve the complete CPU result.
	device.set_occlusion_refusal(0);
	FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && pixels == cpu && device.get_occlusion_draws() == 1);
	device.set_occlusion_refusal(-1);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", false);
	const int vertex_only = device.get_occlusion_draws();
	FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && pixels == cpu && device.get_occlusion_draws() == vertex_only);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", true);
	settings->set("rendering/quality/fixed_function/texture_occlusion", false);
	const int disabled = device.get_occlusion_draws();
	FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _images(pixels, cpu) && device.get_occlusion_draws() == disabled);
	settings->set("rendering/quality/fixed_function/texture_occlusion", true);
	FF_AO_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(128, 96) == OK);
	FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && pixels == reference);
	// Static immediate draws need the same independent UVs and vertex colors.
	FF_AO_CHECK(meshes.instance_remove_skeleton(skeleton, &instance) == OK);
	instance.skeleton = RID();
	instance.blend_values.resize(0);
	settings->set("rendering/quality/fixed_function/retained_geometry", false);
	FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_AO_CHECK(TestFixedFunctionLambert::_color(pixels[center], Color(albedo.r * 0.3, albedo.g * 0.3, albedo.b * 0.3).to_argb32()));
	// Three additive batches: first native AO, then one refusal and two CPU draws.
	const RID repeated = _occlusion_mesh(meshes, 3);
	FF_AO_CHECK(repeated.is_valid());
	instance.base = repeated;
	FF_AO_CHECK(materials.shader_set_code(shader, source.replace("$MODE", ",blend_add").replace("ALPHA=0.75*COLOR.a; ALPHA_SCISSOR=cutoff;", "ALPHA=1.0;")) == OK);
	FixedFunctionGraphicsDevice::FFCapabilities &caps = const_cast<FixedFunctionGraphicsDevice::FFCapabilities &>(device.get_capabilities());
	const uint32_t previous_batch = caps.max_primitives_per_draw;
	caps.max_primitives_per_draw = 1;
	device.set_occlusion_refusal(1);
	const bool batched = TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels);
	caps.max_primitives_per_draw = previous_batch;
	FF_AO_CHECK(batched && device.get_occlusion_draws() == 2 && TestFixedFunctionLambert::_color(pixels[center], 0xff608fff));
	device.set_occlusion_refusal(0, FAILED);
	CameraMatrix projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	RasterizerScene::InstanceBase *instances[] = { &instance };
	FF_AO_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
	const Error draw_error = p_renderer.render(Transform(), projection, instances, 1, environment, active.ptr(), active.size());
	const Error end_error = p_targets.end_frame(false);
	device.set_occlusion_refusal(-1);
	FF_AO_CHECK(draw_error == FAILED && end_error == FAILED);
	FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_AO_CHECK(meshes.free(mesh) == OK && meshes.free(repeated) == OK && meshes.free(skeleton) == OK);
	FF_AO_CHECK(materials.free(material) == OK && materials.free(shader) == OK && textures.free(map) == OK && scenes.free(environment) == OK && lights.free(light_instance) == OK && lights.free(light) == OK);
	settings->set("rendering/quality/fixed_function/texture_occlusion", previous_ao);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", previous_pixel);
	settings->set("rendering/quality/fixed_function/retained_geometry", previous_retained);
	device.set_transient_occlusion_enabled(false);
	printf("PASS: streamed per-pixel AO without normal maps, animated/interior UV sampling, vertex tint/alpha, cutoff, quality/refusal fallback, reset and mixed batches.\n");
	return true;
}

#undef FF_AO_CHECK
