#include "core/config/project_settings.h"
#include "drivers/fixed_function/tests/test_retained_lighting.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>
#include <string.h>

#define FF_AO_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Transient AO FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

RID TestFixedFunctionRetainedLighting::_occlusion_mesh(FixedFunctionMeshStorage &p_storage, int p_triangles) {
	Array arrays, target, targets;
	arrays.resize(RSE::ARRAY_MAX);
	Vector<Vector3> positions, normals;
	Vector<Vector2> uv, uv2;
	Vector<Color> colors;
	PackedInt32Array bones;
	PackedFloat32Array weights;
	PackedFloat32Array tangents;
	const Vector3 points[] = { Vector3(-1, -1, 0), Vector3(0, 1, 0), Vector3(1, -1, 0) };
	const Vector2 coordinates[] = { Vector2(0, 0), Vector2(0.5, 1), Vector2(1, 0) };
	for (int triangle = 0; triangle < p_triangles; triangle++) {
		for (int i = 0; i < 3; i++) {
			positions.push_back(points[i]);
			normals.push_back(Vector3(0, 0, 1));
			uv.push_back(Vector2(0.1, 0.1));
			uv2.push_back(coordinates[i]);
			colors.push_back(Color(0.5, 0.75, 1, 0.5));
			tangents.push_back(1);
			tangents.push_back(0);
			tangents.push_back(0);
			tangents.push_back(1);
			for (int j = 0; j < 4; j++) {
				bones.push_back(0);
				weights.push_back(j == 0 ? 1 : 0);
			}
		}
	}
	arrays[RSE::ARRAY_VERTEX] = positions;
	arrays[RSE::ARRAY_NORMAL] = normals;
	arrays[RSE::ARRAY_TEX_UV] = uv;
	arrays[RSE::ARRAY_TEX_UV2] = uv2;
	arrays[RSE::ARRAY_COLOR] = colors;
	arrays[RSE::ARRAY_BONES] = bones;
	arrays[RSE::ARRAY_WEIGHTS] = weights;
	arrays[RSE::ARRAY_TANGENT] = tangents;
	target.resize(RSE::ARRAY_MAX);
	target[RSE::ARRAY_VERTEX] = positions;
	target[RSE::ARRAY_NORMAL] = normals;
	target[RSE::ARRAY_TANGENT] = tangents;
	targets.push_back(target);
	RenderingServerTypes::SurfaceData data;
	if (RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&data, RSE::PRIMITIVE_TRIANGLES, arrays, targets, Dictionary(), 0) != OK) {
		return RID();
	}
	// The 4.x array packer synthesizes tangents. Remove them from both packed
	// streams to verify standalone AO really needs only geometry normals and UVs.
	RenderingServer *server = RenderingServer::get_singleton();
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
	const RID mesh = p_storage.mesh_create();
	Error error = p_storage.mesh_set_blend_shape_count(mesh, 1);
	if (error == OK) {
		error = p_storage.mesh_add_surface(mesh, data);
	}
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
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const bool previous_ao = GLOBAL_GET("rendering/quality/fixed_function/texture_occlusion");
	const bool previous_pixel = GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting");
	const bool previous_retained = GLOBAL_GET("rendering/quality/fixed_function/retained_geometry");
	settings->set("rendering/quality/fixed_function/texture_occlusion", true);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", true);
	const RID shader = materials.shader_create(), material = materials.material_create(), map = textures.texture_create();
	const String source = "shader_type spatial; render_mode cull_disabled$MODE; uniform sampler2D orm; uniform float affect=0.5; uniform float cutoff=0.3; void fragment(){ ALBEDO=vec3(0.5)*COLOR.rgb; ALPHA=0.75*COLOR.a; ALPHA_SCISSOR_THRESHOLD=cutoff; AO=texture(orm,UV2).g; AO_LIGHT_AFFECT=affect; }";
	FF_AO_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, source.replace("$MODE", "")) == OK);
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image->fill(Color(0, 1, 0, 0));

	for (int y = 1; y < 3; y++) {
		for (int x = 1; x < 3; x++) {
			image->set_pixel(x, y, Color(1, 0.25, 0, 0));
		}
	}

	FF_AO_CHECK(textures.texture_set_data(map, image) == OK && materials.material_set_param(material, "orm", map) == OK);
	FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	environment.ambient = Color(0.2, 0.2, 0.2);
	const RID light = lights.light_create(RSE::LIGHT_DIRECTIONAL), light_instance = lights.light_instance_create(light);
	FixedFunctionLightStorage::FFLightData light_data;
	FF_AO_CHECK(lights.light_get_data(light, light_data) == OK);
	light_data.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.4f;
	FF_AO_CHECK(lights.light_set_data(light, light_data) == OK);
	Vector<RID> active;
	active.push_back(light_instance);
	const RID skeleton = meshes.skeleton_allocate();
	meshes.skeleton_initialize(skeleton);
	FF_AO_CHECK(meshes.skeleton_allocate_data(skeleton, 1) == OK);
	const RID mesh = _occlusion_mesh(meshes);
	FF_AO_CHECK(mesh.is_valid());
	TestFixedFunctionLambert::FFInstance instance(p_context, mesh, material);
	instance.mesh_instance = meshes.mesh_instance_create(mesh);
	FF_AO_CHECK(meshes.mesh_instance_set_skeleton(instance.mesh_instance, skeleton) == OK);
	std::vector<uint32_t> pixels, reference, cpu;
	const size_t center = 48 * 128 + 64;
	// Every vertex samples white; only native pixel AO sees the dark interior texels.
	const Color albedo(0.25, 0.375, 0.5);
	for (int pose = 0; pose < 3; pose++) {
		Transform3D bone;
		bone.basis.set_column(0, Vector3(1.1, 0.1, 0));
		bone.basis.set_column(1, Vector3(0.05, 0.9, 0));
		bone.origin.x = pose * real_t(0.02);
		FF_AO_CHECK(meshes.skeleton_bone_set_transform(skeleton, 0, bone) == OK);
		FF_AO_CHECK(meshes.mesh_instance_set_blend_shape_weight(instance.mesh_instance, 0, pose * 0.5f) == OK);
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
		FF_AO_CHECK(materials.shader_set_code(shader, source.replace("$MODE", mode).replace("ALPHA_SCISSOR_THRESHOLD=cutoff;", "")) == OK);
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
	FF_AO_CHECK(meshes.free(instance.mesh_instance) == OK);
	instance.mesh_instance = RID();
	settings->set("rendering/quality/fixed_function/retained_geometry", false);
	FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_AO_CHECK(TestFixedFunctionLambert::_color(pixels[center], Color(albedo.r * 0.3, albedo.g * 0.3, albedo.b * 0.3).to_argb32()));
	// Three additive batches: first native AO, then one refusal and two CPU draws.
	const RID repeated = _occlusion_mesh(meshes, 3);
	FF_AO_CHECK(repeated.is_valid());
	instance.data->base = repeated;
	FF_AO_CHECK(materials.shader_set_code(shader, source.replace("$MODE", ",blend_add").replace("ALPHA=0.75*COLOR.a; ALPHA_SCISSOR_THRESHOLD=cutoff;", "ALPHA=1.0;")) == OK);
	FixedFunctionGraphicsDevice::FFCapabilities &caps = const_cast<FixedFunctionGraphicsDevice::FFCapabilities &>(device.get_capabilities());
	const uint32_t previous_batch = caps.max_primitives_per_draw;
	caps.max_primitives_per_draw = 1;
	device.set_occlusion_refusal(1);
	const bool batched = TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels);
	caps.max_primitives_per_draw = previous_batch;
	FF_AO_CHECK(batched && device.get_occlusion_draws() == 2 && TestFixedFunctionLambert::_color(pixels[center], 0xff608fff));
	device.set_occlusion_refusal(0, FAILED);
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	instances.push_back(&instance);
	FF_AO_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
	const Error draw_error = p_renderer.render(Transform3D(), projection, instances, nullptr, environment, active.ptr(), active.size());
	const Error end_error = p_targets.end_frame(false);
	device.set_occlusion_refusal(-1);
	FF_AO_CHECK(draw_error == FAILED && end_error == FAILED);
	FF_AO_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_AO_CHECK(meshes.free(mesh) == OK);
	FF_AO_CHECK(meshes.free(repeated) == OK);
	meshes.skeleton_free(skeleton);
	FF_AO_CHECK(materials.free(material) == OK && materials.free(shader) == OK && textures.free(map) == OK && lights.free(light_instance) == OK && lights.free(light) == OK);
	settings->set("rendering/quality/fixed_function/texture_occlusion", previous_ao);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", previous_pixel);
	settings->set("rendering/quality/fixed_function/retained_geometry", previous_retained);
	device.set_transient_occlusion_enabled(false);
	printf("PASS: streamed per-pixel AO without normal maps, animated/interior UV sampling, vertex tint/alpha, cutoff, quality/refusal fallback, reset and mixed batches.\n");
	return true;
}

#undef FF_AO_CHECK
