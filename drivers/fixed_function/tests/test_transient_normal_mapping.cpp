#include "core/config/project_settings.h"
#include "drivers/fixed_function/tests/test_lambert.h"
#include "drivers/fixed_function/tests/test_retained_lighting.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>
#include <string.h>

#include <limits>

#define FF_TRANSIENT_NORMAL_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Transient normal map FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

RID TestFixedFunctionLambert::_animated_normal_mesh(FixedFunctionMeshStorage &p_storage, uint64_t p_flags, bool p_relative, float p_sign, bool p_tangents, bool p_shapes, bool p_indexed) {
	Array arrays, target;
	arrays.resize(RSE::ARRAY_MAX);
	Vector<Vector3> positions, normals, target_positions, target_normals;
	Vector<Vector2> uv, uv2;
	Vector<Color> colors;
	Vector<float> tangents, target_tangents, weights;
	Vector<int> bones;
	const Vector3 points[] = { Vector3(-1, -1, 0), Vector3(0, 1, 0), Vector3(1, -1, 0) };
	const int influences = (p_flags & RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS) ? 8 : 4;
	for (int i = 0; i < 3; i++) {
		positions.push_back(points[i]);
		target_positions.push_back(p_relative ? Vector3() : points[i]);
		normals.push_back(Vector3(0, 0, 1));
		// Octahedral 4.x normal data cannot represent a zero-length relative delta.
		target_normals.push_back(Vector3(0, 0, 1));
		uv.push_back(Vector2(0.25, 0.25));
		uv2.push_back(Vector2(0.75, 0.75));
		colors.push_back(Color(0.5, 0.75, 1, 0.5));
		tangents.push_back(1);
		tangents.push_back(0);
		tangents.push_back(0);
		tangents.push_back(p_sign);
		target_tangents.push_back(0);
		target_tangents.push_back(1);
		target_tangents.push_back(0);
		target_tangents.push_back(-p_sign);
		for (int j = 0; j < influences; j++) {
			// Exercise the upper four influences, not just an eight-entry padded array.
			const int first = influences == 8 ? 4 : 0;
			bones.push_back(j == first + 1 ? 1 : 0);
			float weight = 0;
			if (j == first) {
				weight = 0.25f;
			}
			if (j == first + 1) {
				weight = 0.75f;
			}
			weights.push_back(weight);
		}
	}
	arrays[RSE::ARRAY_VERTEX] = positions;
	arrays[RSE::ARRAY_NORMAL] = normals;
	arrays[RSE::ARRAY_TEX_UV] = uv;
	arrays[RSE::ARRAY_TEX_UV2] = uv2;
	arrays[RSE::ARRAY_COLOR] = colors;
	arrays[RSE::ARRAY_BONES] = bones;
	arrays[RSE::ARRAY_WEIGHTS] = weights;
	if (p_indexed) {
		arrays[RSE::ARRAY_INDEX] = PackedInt32Array{ 0, 1, 2, 0, 1, 2, 0, 1, 2 };
	}
	// The native 4.x array packer synthesizes tangents; strip the layout when testing their absence.
	arrays[RSE::ARRAY_TANGENT] = tangents;
	target.resize(RSE::ARRAY_MAX);
	target[RSE::ARRAY_VERTEX] = target_positions;
	target[RSE::ARRAY_NORMAL] = target_normals;
	target[RSE::ARRAY_TANGENT] = target_tangents;
	Array targets;
	if (p_shapes) {
		targets.push_back(target);
	}
	RenderingServerTypes::SurfaceData data;
	RenderingServer *server = RenderingServer::get_singleton();
	if (server->mesh_create_surface_data_from_arrays(&data, RSE::PRIMITIVE_TRIANGLES, arrays, targets, Dictionary(), p_flags) != OK) {
		return RID();
	}
	if (!p_tangents) {
		uint32_t old_offsets[RSE::ARRAY_MAX], offsets[RSE::ARRAY_MAX];
		uint32_t vertex_stride, old_normal_stride, normal_stride, attribute_stride, skin_stride;
		server->mesh_surface_make_offsets_from_format(data.format, data.vertex_count, 0, old_offsets, vertex_stride, old_normal_stride, attribute_stride, skin_stride);
		data.format &= ~uint64_t(RSE::ARRAY_FORMAT_TANGENT);
		server->mesh_surface_make_offsets_from_format(data.format, data.vertex_count, 0, offsets, vertex_stride, normal_stride, attribute_stride, skin_stride);
		Vector<uint8_t> *buffers[] = { &data.vertex_data, &data.blend_shape_data };
		for (Vector<uint8_t> *buffer : buffers) {
			const Vector<uint8_t> original = *buffer;
			Vector<uint8_t> packed;
			if (packed.resize(uint64_t(data.vertex_count) * (vertex_stride + normal_stride)) != OK) {
				return RID();
			}
			memcpy(packed.ptrw(), original.ptr(), uint64_t(data.vertex_count) * vertex_stride);
			for (uint32_t i = 0; i < data.vertex_count; i++) {
				memcpy(packed.ptrw() + offsets[RSE::ARRAY_NORMAL] + i * normal_stride, original.ptr() + old_offsets[RSE::ARRAY_NORMAL] + i * old_normal_stride, normal_stride);
			}
			*buffer = packed;
		}
	}
	const RID mesh = p_storage.mesh_create();
	Error error = p_storage.mesh_set_blend_shape_count(mesh, p_shapes ? 1 : 0);
	if (error == OK) {
		error = p_storage.mesh_set_blend_shape_mode(mesh, p_relative ? RSE::BLEND_SHAPE_MODE_RELATIVE : RSE::BLEND_SHAPE_MODE_NORMALIZED);
	}
	if (error == OK) {
		error = p_storage.mesh_add_surface(mesh, data);
	}
	return error == OK ? mesh : RID();
}

bool TestFixedFunctionLambert::_test_transient_normal_maps(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	if (!p_context.get_device().supports_transient_normal_mapping()) {
		return true;
	}
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const bool previous_normal = GLOBAL_GET("rendering/quality/fixed_function/normal_mapping");
	const bool previous_pixel = GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting");
	const bool previous_ao = GLOBAL_GET("rendering/quality/fixed_function/texture_occlusion");
	settings->set("rendering/quality/fixed_function/normal_mapping", true);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", true);
	settings->set("rendering/quality/fixed_function/texture_occlusion", true);
	const RID shader = materials.shader_create(), material = materials.material_create();
	const String code = "shader_type spatial; render_mode cull_disabled; uniform sampler2D base : filter_nearest, repeat_disable; uniform sampler2D bump:hint_normal, filter_nearest, repeat_disable; uniform sampler2D orm : filter_nearest, repeat_disable; uniform float depth=1.0; uniform float cutoff=0.3; void fragment(){ ALBEDO=vec3(0.5)*texture(base,UV).rgb*COLOR.rgb; ALPHA=0.75*texture(base,UV).a*COLOR.a; ALPHA_SCISSOR_THRESHOLD=cutoff; NORMAL_MAP=texture(bump,UV2).rgb; NORMAL_MAP_DEPTH=depth; $AO }";
	FF_TRANSIENT_NORMAL_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, code.replace("$AO", "")) == OK);
	Ref<Image> image = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	const RID base = textures.texture_create(), bump = textures.texture_create(), orm = textures.texture_create();
	image->fill(Color(1, 0, 0));
	image->set_pixel(0, 0, Color(1, 1, 1));
	FF_TRANSIENT_NORMAL_CHECK(textures.texture_set_data(base, image) == OK);
	image->fill(Color(0.5, 0.5, 1));
	const Color texel(0.4, 0.75, 0);
	image->set_pixel(1, 1, texel);
	FF_TRANSIENT_NORMAL_CHECK(textures.texture_set_data(bump, image) == OK);
	image->fill(Color(1, 0.25, 0));
	FF_TRANSIENT_NORMAL_CHECK(textures.texture_set_data(orm, image) == OK);
	FF_TRANSIENT_NORMAL_CHECK(materials.material_set_param(material, "base", base) == OK && materials.material_set_param(material, "bump", bump) == OK && materials.material_set_param(material, "orm", orm) == OK);
	FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	environment.ambient = Color(0.05, 0.05, 0.05);
	const RID light = lights.light_create(RSE::LIGHT_DIRECTIONAL), light_instance = lights.light_instance_create(light);
	FixedFunctionLightStorage::FFLightData light_data;
	FF_TRANSIENT_NORMAL_CHECK(lights.light_get_data(light, light_data) == OK);
	light_data.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.6f;
	FF_TRANSIENT_NORMAL_CHECK(lights.light_set_data(light, light_data) == OK);
	Transform3D light_transform;
	light_transform.basis.set_column(2, Vector3(0.5, 0.4, 1).normalized());
	FF_TRANSIENT_NORMAL_CHECK(lights.light_instance_set_transform(light_instance, light_transform) == OK);
	Vector<RID> active;
	active.push_back(light_instance);
	const RID skeleton = meshes.skeleton_allocate();
	meshes.skeleton_initialize(skeleton);
	FF_TRANSIENT_NORMAL_CHECK(meshes.skeleton_allocate_data(skeleton, 2) == OK);
	Transform3D bone;
	bone.basis.set_column(0, Vector3(-1.5, 0.2, 0));
	bone.basis.set_column(1, Vector3(0.35, 1.2, 0));
	bone.basis.set_column(2, Vector3(0, 0, 0.8));
	FF_TRANSIENT_NORMAL_CHECK(meshes.skeleton_bone_set_transform(skeleton, 1, bone) == OK);
	const Basis skin = Basis() * real_t(0.25) + bone.basis * real_t(0.75);
	std::vector<uint32_t> pixels;
	const size_t center = 48 * 128 + 64;
	const Color albedo(0.25, 0.375, 0.5);
	for (int format = 0; format < 4; format++) {
		const uint64_t formats[] = { 0, RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS, 0, RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS };
		const bool relative = format >= 2;
		for (int sign_index = 0; sign_index < 2; sign_index++) {
			const float sign = sign_index ? -1 : 1;
			const RID mesh = _animated_normal_mesh(meshes, formats[format], relative, sign);
			FF_TRANSIENT_NORMAL_CHECK(mesh.is_valid());
			FFInstance instance(p_context, mesh, material);
			instance.mesh_instance = meshes.mesh_instance_create(mesh);
			FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_instance_set_skeleton(instance.mesh_instance, skeleton) == OK);
			instance.transform.basis = Basis(Vector3(0, 1, 0), real_t(0.15));
			instance.transform.basis.scale(Vector3(-0.8, 1.2, 0.9));
			FixedFunctionMeshStorage::FFSurfaceData before, after;
			FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, before) == OK);
			for (int pose = 0; pose < 3; pose++) {
				const real_t weight = real_t(pose) * real_t(0.5);
				FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_instance_set_blend_shape_weight(instance.mesh_instance, 0, float(weight)) == OK);
				const real_t angle = Math::atan2(weight, relative ? real_t(1) : 1 - weight);
				const Basis frame = instance.transform.basis * skin * Basis(Vector3(0, 0, 1), angle);
				FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
				if (!TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, sign, frame, albedo)) {
					Vector<FixedFunctionMeshStorage::FFDeformedVertex> pose_vertices;
					Vector<FixedFunctionMeshStorage::FFDeformedTangentFrame> pose_frames;
					AABB bounds;
					meshes.mesh_instance_deform(instance.mesh_instance, before, pose_vertices, bounds, &pose_frames);
					printf("Animated case format=%d sign=%f pose=%d N=%s T=%s B=%s\n", format, sign, pose, String(pose_vertices[0].normal).utf8().get_data(), String(pose_frames[0].tangent).utf8().get_data(), String(pose_frames[0].bitangent).utf8().get_data());
					return false;
				}
				const Transform3D camera(Basis(Vector3(0, 1, 0), real_t(0.1)), Vector3());
				FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels, camera));
				FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, sign, frame, albedo));
			}
			const Basis frame = instance.transform.basis * skin * Basis(Vector3(0, 0, 1), relative ? Math::PI / 4 : Math::PI / 2);
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
			FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, after) == OK && after.version == before.version && after.source.vertex_data.size() == before.source.vertex_data.size());
			FF_TRANSIENT_NORMAL_CHECK(memcmp(before.source.vertex_data.ptr(), after.source.vertex_data.ptr(), before.source.vertex_data.size()) == 0);
			FF_TRANSIENT_NORMAL_CHECK(meshes.free(instance.mesh_instance) == OK && meshes.free(mesh) == OK);
		}
	}
	for (int authored = 0; authored < 4; authored++) {
		uint64_t flags = 0;
		if (authored >= 2) {
			flags = RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES;
			if (authored == 3) {
				flags |= RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
			}
		}
		const RID mesh = _animated_normal_mesh(meshes, flags, false, 1, authored != 0, authored < 2);
		FF_TRANSIENT_NORMAL_CHECK(mesh.is_valid());
		FFInstance instance(p_context, mesh, material);
		instance.mesh_instance = meshes.mesh_instance_create(mesh);
		FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_instance_set_skeleton(instance.mesh_instance, skeleton) == OK);
		FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, authored ? 1 : 0, 1, skin, albedo));
		if (authored) {
			FF_TRANSIENT_NORMAL_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(128, 96) == OK);
			FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
			FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, 1, skin, albedo));
			if (authored == 1) {
				FFInstance morph_only(p_context, mesh, material);
				morph_only.mesh_instance = meshes.mesh_instance_create(mesh);
				FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_instance_set_blend_shape_weight(morph_only.mesh_instance, 0, 0.5f) == OK);
				FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, morph_only, environment, active, pixels));
				FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, 1, Basis(Vector3(0, 0, 1), Math::PI / 4), albedo));
				FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_instance_set_blend_shape_weight(morph_only.mesh_instance, 0, 0) == OK);
				FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, morph_only, environment, active, pixels));
				FF_TRANSIENT_NORMAL_CHECK(TestFixedFunctionRetainedLighting::_normal_pixel(pixels[center], texel, 1, 1, Basis(), albedo));
				FF_TRANSIENT_NORMAL_CHECK(meshes.free(morph_only.mesh_instance) == OK);
			}
		}
		FF_TRANSIENT_NORMAL_CHECK(meshes.free(instance.mesh_instance) == OK && meshes.free(mesh) == OK);
	}
	meshes.skeleton_free(skeleton);
	FF_TRANSIENT_NORMAL_CHECK(lights.free(light_instance) == OK && lights.free(light) == OK);
	FF_TRANSIENT_NORMAL_CHECK(materials.free(material) == OK && materials.free(shader) == OK && textures.free(base) == OK && textures.free(bump) == OK && textures.free(orm) == OK);
	settings->set("rendering/quality/fixed_function/normal_mapping", previous_normal);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", previous_pixel);
	settings->set("rendering/quality/fixed_function/texture_occlusion", previous_ao);
	printf("PASS: streamed normal maps, tangent morphs and skinning, packed frames, reflections/shear, vertex tint/alpha, independent AO and immutable source.\n");
	return true;
}

bool TestFixedFunctionLambert::test_streamed_vertex_lighting(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	if (!p_context.get_device().get_max_streamed_vertex_lights()) {
		return true;
	}
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	FF_TRANSIENT_NORMAL_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(0.6,0.4,0.3); }") == OK);
	FF_TRANSIENT_NORMAL_CHECK(materials.material_set_shader(material, shader) == OK);
	FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	environment.ambient = Color(0.15, 0.13, 0.17);
	const RID light = lights.light_create(RSE::LIGHT_DIRECTIONAL);
	const RID light_instance = lights.light_instance_create(light);
	FixedFunctionLightStorage::FFLightData light_data;
	FF_TRANSIENT_NORMAL_CHECK(lights.light_get_data(light, light_data) == OK);
	light_data.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.5f;
	FF_TRANSIENT_NORMAL_CHECK(lights.light_set_data(light, light_data) == OK);
	Vector<RID> active;
	active.push_back(light_instance);
	const RID skeleton = meshes.skeleton_allocate();
	meshes.skeleton_initialize(skeleton);
	FF_TRANSIENT_NORMAL_CHECK(meshes.skeleton_allocate_data(skeleton, 2) == OK);
	const Transform3D bone(Basis(Vector3(0, 1, 0), real_t(0.6)), Vector3(0.3, 0.08, 0));
	FF_TRANSIENT_NORMAL_CHECK(meshes.skeleton_bone_set_transform(skeleton, 1, bone) == OK);
	const Transform3D camera(Basis(Vector3(0, 1, 0), real_t(0.17)), Vector3());
	const char *native = "rendering/quality/fixed_function/native_lighting";
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const Variant previous_native = settings->get(native);
	for (int indexed = 0; indexed < 2; indexed++) {
		const uint64_t flags = indexed ? uint64_t(RSE::ARRAY_FLAG_USE_8_BONE_WEIGHTS) : 0;
		const RID mesh = _animated_normal_mesh(meshes, flags, false, 1, true, false, indexed != 0);
		FF_TRANSIENT_NORMAL_CHECK(mesh.is_valid());
		FFInstance instance(p_context, mesh, material);
		instance.mesh_instance = meshes.mesh_instance_create(mesh);
		FF_TRANSIENT_NORMAL_CHECK(meshes.mesh_instance_set_skeleton(instance.mesh_instance, skeleton) == OK);
		instance.transform.basis = Basis().scaled(Vector3(0.8, 1.2, 0.55));
		std::vector<uint32_t> reference, pixels;
		settings->set(native, 0);
		FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, reference, camera));
		settings->set(native, 1);
		FF_TRANSIENT_NORMAL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels, camera));
		int drawn = 0;
		for (size_t i = 0; i < pixels.size(); i++) {
			FF_TRANSIENT_NORMAL_CHECK(_color(pixels[i], reference[i]));
			drawn += reference[i] != 0xff0000ff;
		}
		FF_TRANSIENT_NORMAL_CHECK(drawn > 100);
		FF_TRANSIENT_NORMAL_CHECK(meshes.free(instance.mesh_instance) == OK && meshes.free(mesh) == OK);
	}
	settings->set(native, previous_native);
	meshes.skeleton_free(skeleton);
	FF_TRANSIENT_NORMAL_CHECK(lights.free(light_instance) == OK && lights.free(light) == OK);
	FF_TRANSIENT_NORMAL_CHECK(materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: streamed vertex lighting matches CPU on a posed, non-uniformly scaled mesh with a rotated camera.\n");
	return true;
}

#undef FF_TRANSIENT_NORMAL_CHECK
