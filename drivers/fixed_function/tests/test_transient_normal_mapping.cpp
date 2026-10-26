#include "test_lambert.h"
#include "test_retained_lighting.h"

#include "core/project_settings.h"

#include <stdio.h>
#include <string.h>
#include <limits>

#define FF_TRANSIENT_NORMAL_CHECK(m_condition)                                       \
	do {                                                                             \
		if (!(m_condition)) {                                                        \
			printf("Transient normal map FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                            \
		}                                                                            \
	} while (false)

RID TestFixedFunctionLambert::_animated_normal_mesh(FixedFunctionMeshStorage &p_storage, uint32_t p_compression, bool p_relative, float p_sign, bool p_tangents) {
	Array arrays, target;
	arrays.resize(VS::ARRAY_MAX);
	target.resize(VS::ARRAY_MAX);
	PoolVector<Vector3> positions, target_positions, normals, target_normals;
	PoolVector<Vector2> uv, uv2, target_uv, target_uv2;
	PoolVector<Color> colors, target_colors;
	PoolRealArray tangents, target_tangents, weights, target_weights;
	PoolIntArray bones;
	const Vector3 points[] = { Vector3(-1, -1, 0), Vector3(0, 1, 0), Vector3(1, -1, 0) };
	for (int i = 0; i < 3; i++) {
		positions.push_back(points[i]);
		target_positions.push_back(p_relative ? Vector3() : points[i]);
		normals.push_back(Vector3(0, 0, 1));
		target_normals.push_back(p_relative ? Vector3() : Vector3(0, 0, 1));
		uv.push_back(Vector2(0.25, 0.25));
		uv2.push_back(Vector2(0.75, 0.75));
		colors.push_back(Color(0.5, 0.75, 1, 0.5));
		target_uv.push_back(p_relative ? Vector2() : uv[i]);
		target_uv2.push_back(p_relative ? Vector2() : uv2[i]);
		target_colors.push_back(p_relative ? Color(0, 0, 0, 0) : colors[i]);
		for (int j = 0; j < 4; j++) {
			float base_component = 0;
			if (j == 0)
				base_component = 1;
			if (j == 3)
				base_component = p_sign;
			tangents.push_back(base_component);
			float component = 0;
			if (j == 1)
				component = 1;
			if (p_relative && j == 0)
				component = -1;
			if (!p_relative && j == 3)
				component = -p_sign;
			target_tangents.push_back(component);
			bones.push_back(j == 1 ? 1 : 0);
			float weight = 0;
			if (j == 0)
				weight = 0.25f;
			if (j == 1)
				weight = 0.75f;
			weights.push_back(weight);
			target_weights.push_back(p_relative ? 0 : weight);
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
	target[VS::ARRAY_VERTEX] = target_positions;
	target[VS::ARRAY_NORMAL] = target_normals;
	target[VS::ARRAY_TEX_UV] = target_uv;
	target[VS::ARRAY_TEX_UV2] = target_uv2;
	target[VS::ARRAY_COLOR] = target_colors;
	target[VS::ARRAY_WEIGHTS] = target_weights;
	if (p_tangents) {
		arrays[VS::ARRAY_TANGENT] = tangents;
		target[VS::ARRAY_TANGENT] = target_tangents;
	}
	Array targets;
	targets.push_back(target);
	VisualServer *server = VisualServer::get_singleton();
	const RID source = server->mesh_create();
	server->mesh_set_blend_shape_count(source, 1);
	server->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_TRIANGLES, arrays, targets, p_compression);
	const RID mesh = p_storage.mesh_create();
	Error error = p_storage.mesh_set_blend_shape_count(mesh, 1);
	if (error == OK) {
		error = p_storage.mesh_set_blend_shape_mode(mesh, p_relative ? VS::BLEND_SHAPE_MODE_RELATIVE : VS::BLEND_SHAPE_MODE_NORMALIZED);
	}
	if (error == OK) {
		error = p_storage.mesh_add_surface(mesh, server->mesh_surface_get_format(source, 0), VS::PRIMITIVE_TRIANGLES, server->mesh_surface_get_array(source, 0), 3, PoolVector<uint8_t>(), 0, server->mesh_surface_get_aabb(source, 0), server->mesh_surface_get_blend_shapes(source, 0), Vector<AABB>());
	}
	server->free(source);
	return error == OK ? mesh : RID();
}

bool TestFixedFunctionLambert::_test_transient_normal_maps(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	if (!p_context.get_device().supports_transient_normal_mapping())
		return true;
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	FixedFunctionSceneStorage &scenes = p_context.get_scene_storage();
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const bool previous_normal = GLOBAL_GET("rendering/quality/fixed_function/normal_mapping");
	const bool previous_pixel = GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting");
	const bool previous_ao = GLOBAL_GET("rendering/quality/fixed_function/texture_occlusion");
	settings->set("rendering/quality/fixed_function/normal_mapping", true);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", true);
	settings->set("rendering/quality/fixed_function/texture_occlusion", true);
	const RID shader = materials.shader_create(), material = materials.material_create();
	const String code = "shader_type spatial; render_mode cull_disabled; uniform sampler2D base; uniform sampler2D bump:hint_normal; uniform sampler2D orm; uniform float depth=1.0; uniform float cutoff=0.3; void fragment(){ ALBEDO=vec3(0.5)*texture(base,UV).rgb*COLOR.rgb; ALPHA=0.75*texture(base,UV).a*COLOR.a; ALPHA_SCISSOR=cutoff; NORMALMAP=texture(bump,UV2).rgb; NORMALMAP_DEPTH=depth; $AO }";
	FF_TRANSIENT_NORMAL_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, code.replace("$AO", "")) == OK);
	Ref<Image> image = memnew(Image(2, 2, false, Image::FORMAT_RGBA8));
	const RID base = textures.texture_create(), bump = textures.texture_create(), orm = textures.texture_create();
	image->fill(Color(1, 0, 0));
	image->lock();
	image->set_pixel(0, 0, Color(1, 1, 1));
	image->unlock();
	FF_TRANSIENT_NORMAL_CHECK(textures.texture_set_data(base, image) == OK);
	image->fill(Color(0.5, 0.5, 1));
	const Color texel(0.4, 0.75, 0);
	image->lock();
	image->set_pixel(1, 1, texel);
	image->unlock();
	FF_TRANSIENT_NORMAL_CHECK(textures.texture_set_data(bump, image) == OK);
	image->fill(Color(1, 0.25, 0));
	FF_TRANSIENT_NORMAL_CHECK(textures.texture_set_data(orm, image) == OK);
	FF_TRANSIENT_NORMAL_CHECK(materials.material_set_param(material, "base", base) == OK && materials.material_set_param(material, "bump", bump) == OK && materials.material_set_param(material, "orm", orm) == OK);
	const RID environment = scenes.environment_create();
	FF_TRANSIENT_NORMAL_CHECK(scenes.environment_set_ambient(environment, Color(0.05, 0.05, 0.05), 1, 0) == OK);
	const RID light = lights.light_create(VS::LIGHT_DIRECTIONAL), light_instance = lights.light_instance_create(light);
	FixedFunctionLightStorage::FFLightData light_data;
	FF_TRANSIENT_NORMAL_CHECK(lights.light_get_data(light, light_data) == OK);
	light_data.parameters[VS::LIGHT_PARAM_ENERGY] = 0.6f;
	FF_TRANSIENT_NORMAL_CHECK(lights.light_set_data(light, light_data) == OK);
	Transform light_transform;
	light_transform.basis.set_axis(2, Vector3(0.5, 0.4, 1).normalized());
	FF_TRANSIENT_NORMAL_CHECK(lights.light_instance_set_transform(light_instance, light_transform) == OK);
	Vector<RID> active;
	active.push_back(light_instance);
	const RID skeleton = meshes.skeleton_create();
	FF_TRANSIENT_NORMAL_CHECK(meshes.skeleton_allocate(skeleton, 2) == OK);
	Transform bone;
	bone.basis.set_axis(0, Vector3(-1.5, 0.2, 0));
	bone.basis.set_axis(1, Vector3(0.35, 1.2, 0));
	bone.basis.set_axis(2, Vector3(0, 0, 0.8));
	FF_TRANSIENT_NORMAL_CHECK(meshes.skeleton_set_bone_transform(skeleton, 1, bone) == OK);
	const Basis skin = Basis() * real_t(0.25) + bone.basis * real_t(0.75);
	std::vector<uint32_t> pixels;
	const size_t center = 48 * 128 + 64;
	const Color albedo(0.25, 0.375, 0.5);
	for (int format = 0; format < 5; format++) {
		const uint32_t formats[] = { 0, VS::ARRAY_COMPRESS_NORMAL | VS::ARRAY_COMPRESS_TANGENT | VS::ARRAY_COMPRESS_WEIGHTS, VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION | VS::ARRAY_COMPRESS_NORMAL | VS::ARRAY_COMPRESS_TANGENT, VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION, 0 };
		const bool relative = format == 4;
		for (int sign_index = 0; sign_index < 2; sign_index++) {
			const float sign = sign_index ? -1 : 1;
			const RID mesh = _animated_normal_mesh(meshes, formats[format], relative, sign);
			FF_TRANSIENT_NORMAL_CHECK(mesh.is_valid());
			FFInstance instance(mesh, material);
			instance.skeleton = skeleton;
			FF_TRANSIENT_NORMAL_CHECK(meshes.instance_add_skeleton(skeleton, &instance) == OK);
			instance.transform.basis = Basis(Vector3(0, 1, 0), real_t(0.15));
			instance.transform.basis.scale(Vector3(-0.8, 1.2, 0.9));
			FixedFunctionMeshStorage::FFSurfaceData before, after;
			FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, before) == OK);
			for (int pose = 0; pose < 3; pose++) {
				const real_t weight = real_t(pose) * real_t(0.5);
				instance.blend_values.resize(1);
				instance.blend_values.write()[0] = weight;
				const real_t angle = Math::atan2(weight, 1 - weight);
				const Basis frame = instance.transform.basis * skin * Basis(Vector3(0, 0, 1), angle);
				FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
				FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, sign, frame, albedo));
				const Transform camera(Basis(Vector3(0, 1, 0), real_t(0.1)), Vector3());
				FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels, camera));
				FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, sign, frame, albedo));
			}
			const Basis frame = instance.transform.basis * skin * Basis(Vector3(0, 0, 1), Math_PI / 2);
			const String combined = code.replace("$AO", "AO=texture(orm,UV2*vec2(0)+vec2(0.25)).g; AO_LIGHT_AFFECT=0.5;");
			FF_TRANSIENT_NORMAL_CHECK(materials.shader_set_code(shader, combined) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
			FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, sign, frame, albedo, 0.25, 0.5));
			FF_TRANSIENT_NORMAL_CHECK(materials.material_set_param(material, "cutoff", 0.4) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff0000ff));
			FF_TRANSIENT_NORMAL_CHECK(materials.material_set_param(material, "cutoff", 0.3) == OK);
			settings->set("rendering/quality/fixed_function/normal_mapping", false);
			FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
			FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 0, sign, frame, albedo, 0.25, 0.5));
			settings->set("rendering/quality/fixed_function/normal_mapping", true);
			FF_TRANSIENT_NORMAL_CHECK(materials.shader_set_code(shader, code.replace("$AO", "")) == OK);
			FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, after) == OK && after.version == before.version && after.array.size() == before.array.size());
			{
				const PoolVector<uint8_t>::Read a = before.array.read(), b = after.array.read();
				FF_TRANSIENT_NORMAL_CHECK(memcmp(a.ptr(), b.ptr(), before.array.size()) == 0);
			}
			FF_TRANSIENT_NORMAL_CHECK(meshes.instance_remove_skeleton(skeleton, &instance) == OK && meshes.free(mesh) == OK);
		}
	}
	for (int authored = 0; authored < 2; authored++) {
		const RID mesh = _animated_normal_mesh(meshes, 0, false, 1, authored != 0);
		FF_TRANSIENT_NORMAL_CHECK(mesh.is_valid());
		FFInstance instance(mesh, material);
		instance.skeleton = skeleton;
		FF_TRANSIENT_NORMAL_CHECK(meshes.instance_add_skeleton(skeleton, &instance) == OK);
		FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, authored ? 1 : 0, 1, skin, albedo));
		if (authored) {
			FixedFunctionMeshStorage::FFSurfaceData snapshot;
			FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, snapshot) == OK);
			PoolVector<uint8_t> bad;
			bad.resize(sizeof(float));
			{
				const float value = std::numeric_limits<float>::quiet_NaN();
				PoolVector<uint8_t>::Write write = bad.write();
				memcpy(write.ptr(), &value, sizeof(value));
			}
			FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_surface_update_region(mesh, 0, snapshot.offsets[VS::ARRAY_TANGENT], bad) == OK);
			FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
			FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 0, 1, skin, albedo));
			FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_surface_update_region(mesh, 0, 0, snapshot.array) == OK);
			FF_TRANSIENT_NORMAL_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(128, 96) == OK);
			FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
			FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, 1, skin, albedo));
			FFInstance morph_only(mesh, material);
			morph_only.blend_values.push_back(0.5f);
			FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, morph_only, environment, active, pixels));
			FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, 1, Basis(Vector3(0, 0, 1), Math_PI / 4), albedo));
			morph_only.blend_values.write()[0] = 0;
			FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, morph_only, environment, active, pixels));
			FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, 1, Basis(), albedo));
		}
		FF_TRANSIENT_NORMAL_CHECK(meshes.instance_remove_skeleton(skeleton, &instance) == OK && meshes.free(mesh) == OK);
	}
	FF_TRANSIENT_NORMAL_CHECK(meshes.free(skeleton) == OK && lights.free(light_instance) == OK && lights.free(light) == OK && scenes.free(environment) == OK);
	FF_TRANSIENT_NORMAL_CHECK(materials.free(material) == OK && materials.free(shader) == OK && textures.free(base) == OK && textures.free(bump) == OK && textures.free(orm) == OK);
	settings->set("rendering/quality/fixed_function/normal_mapping", previous_normal);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", previous_pixel);
	settings->set("rendering/quality/fixed_function/texture_occlusion", previous_ao);
	printf("PASS: streamed normal maps, tangent morphs and skinning, packed frames, reflections/shear, vertex tint/alpha, independent AO and immutable source.\n");
	return true;
}

#undef FF_TRANSIENT_NORMAL_CHECK
