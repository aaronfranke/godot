#include "test_retained_lighting.h"

#include "core/os/os.h"
#include "core/project_settings.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <limits>

#define FF_LIT_CHECK(m_condition)                                                 \
	do {                                                                          \
		if (!(m_condition)) {                                                     \
			printf("Retained lighting FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                         \
		}                                                                         \
	} while (false)

bool TestFixedFunctionRetainedLighting::_images(const std::vector<uint32_t> &p_left, const std::vector<uint32_t> &p_right) {
	if (p_left.size() != p_right.size()) {
		return false;
	}
	for (size_t i = 0; i < p_left.size(); i++) {
		if (!TestFixedFunctionLambert::_color(p_left[i], p_right[i])) {
			printf("Lit pixel mismatch %u: %08x / %08x\n", unsigned(i), p_left[i], p_right[i]);
			return false;
		}
	}
	return true;
}

bool TestFixedFunctionRetainedLighting::_compare(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, TestFixedFunctionLambert::FFInstance &p_instance, RID p_environment, const Vector<RID> &p_lights, const Transform &p_camera, std::vector<uint32_t> *r_pixels) {
	std::vector<uint32_t> reference, pixels;
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", false);
	FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, p_instance, p_environment, p_lights, reference, p_camera));
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", true);
	FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, p_instance, p_environment, p_lights, pixels, p_camera));
	FF_LIT_CHECK(_images(pixels, reference));
	if (r_pixels) {
		*r_pixels = pixels;
	}
	return true;
}

bool TestFixedFunctionRetainedLighting::_run(FixedFunctionGraphicsDevice &p_device, void *p_window) {
	TestFixedFunctionRetainedScene::FFDevice device(p_device);
	FixedFunctionRenderContext context(device, p_window, 128, 96, false);
	FixedFunctionRenderTargetManager targets(context);
	FixedFunctionSceneRenderer renderer(context, targets);
	FF_LIT_CHECK(context.initialize() == OK);
	if (!device.supports_lit_geometry()) {
		return context.finalize() == OK;
	}
	FixedFunctionMaterialStorage &materials = context.get_material_storage();
	FixedFunctionMeshStorage &meshes = context.get_mesh_storage();
	FixedFunctionLightStorage &lights = context.get_light_storage();
	FixedFunctionGeometryCache &cache = context.get_geometry_cache();
	const RID shader = materials.shader_create(), material = materials.material_create();
	const String source = "shader_type spatial; render_mode cull_disabled; uniform vec4 tint : hint_color=vec4(1); void fragment(){ ALBEDO=tint.rgb; }";
	FF_LIT_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, source) == OK);
	const RID mesh = TestFixedFunctionLambert::_mesh(meshes, 0, true, Vector3(), false, false, true);
	TestFixedFunctionLambert::FFInstance instance(mesh, material);
	const RID environment = context.get_scene_storage().environment_create();
	FF_LIT_CHECK(context.get_scene_storage().environment_set_ambient(environment, Color(0.08, 0.1, 0.12), 1, 0) == OK);
	Vector<RID> sources, active;
	for (int i = 0; i < 3; i++) {
		VS::LightType type = VS::LIGHT_SPOT;
		if (i == 0) {
			type = VS::LIGHT_DIRECTIONAL;
		} else if (i == 1) {
			type = VS::LIGHT_OMNI;
		}
		const RID light = lights.light_create(type);
		FixedFunctionLightStorage::FFLightData data;
		FF_LIT_CHECK(lights.light_get_data(light, data) == OK);
		data.parameters[VS::LIGHT_PARAM_ENERGY] = 0.3;
		data.parameters[VS::LIGHT_PARAM_RANGE] = 6;
		data.parameters[VS::LIGHT_PARAM_ATTENUATION] = 1;
		FF_LIT_CHECK(lights.light_set_data(light, data) == OK);
		const RID light_instance = lights.light_instance_create(light);
		Transform pose;
		pose.origin = Vector3(i == 1 ? 1 : 0, 0, -1);
		FF_LIT_CHECK(lights.light_instance_set_transform(light_instance, pose) == OK);
		sources.push_back(light);
		active.push_back(light_instance);
	}
	FF_LIT_CHECK(_compare(context, targets, renderer, instance, environment, active));
	FF_LIT_CHECK(device.get_creations() == 1 && device.get_retained_draws() == 1);
	FixedFunctionMeshStorage::FFSurfaceData snapshot;
	FF_LIT_CHECK(meshes.mesh_surface_get_data(mesh, 0, snapshot) == OK);
	RID geometry;
	FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
	Vector3 anchor, extent;
	FF_LIT_CHECK(cache.get(snapshot.identity, snapshot.version, 0, geometry, info, anchor, extent, true) == OK);
	const RID original = geometry;
	FF_LIT_CHECK(device.get_geometry_memory_usage() == 3 * sizeof(FixedFunctionLightingVertex) + 3 * sizeof(uint16_t));
	instance.transform.basis = Basis(Vector3(0, 0, 1), real_t(0.17));
	instance.transform.basis.scale(Vector3(-0.8, 0.7, 1.6));
	Basis camera_basis(Vector3(0, 0, 1), real_t(0.09));
	FF_LIT_CHECK(_compare(context, targets, renderer, instance, environment, active, Transform(camera_basis, Vector3(0.1, 0.2, 0))));
	FF_LIT_CHECK(device.get_creations() == 1 && cache.get(snapshot.identity, snapshot.version, 0, geometry, info, anchor, extent, true) == OK && geometry == original);
	FixedFunctionLightStorage::FFLightData data;
	FF_LIT_CHECK(lights.light_get_data(sources[1], data) == OK);
	data.flags |= FixedFunctionLightStorage::LIGHT_NEGATIVE;
	data.cull_mask = 2;
	FF_LIT_CHECK(lights.light_set_data(sources[1], data) == OK);
	instance.layer_mask = 1;
	FF_LIT_CHECK(materials.material_set_param(material, "tint", Color(1.5, 0.6, 0.25)) == OK && _compare(context, targets, renderer, instance, environment, active));
	instance.layer_mask = 3;
	FF_LIT_CHECK(_compare(context, targets, renderer, instance, environment, active) && device.get_creations() == 1);
	// Shader/material changes reuse immutable geometry, including cutoff alpha quantization.
	FF_LIT_CHECK(materials.shader_set_code(shader, source.replace("ALBEDO=tint.rgb;", "ALBEDO=tint.rgb; ALPHA=tint.a; ALPHA_SCISSOR=0.5;")) == OK);
	for (int alpha = 0; alpha < 2; alpha++) {
		FF_LIT_CHECK(materials.material_set_param(material, "tint", Color(0.8, 1, 0.5, alpha ? 0.75 : 0.25)) == OK);
		FF_LIT_CHECK(_compare(context, targets, renderer, instance, environment, active));
	}
	FF_LIT_CHECK(materials.shader_set_code(shader, source) == OK);
	device.set_draw_error(ERR_UNAVAILABLE);
	const int before = device.get_transient_draws();
	FF_LIT_CHECK(_compare(context, targets, renderer, instance, environment, active) && device.get_transient_draws() == before + 2);
	device.set_draw_error(OK);
	// Singular normal transforms need the CPU face/normal fallback, not missing geometry.
	instance.transform.basis = Basis();
	instance.transform.basis.scale(Vector3(1, 1, 0));
	const int singular = device.get_transient_draws();
	FF_LIT_CHECK(_compare(context, targets, renderer, instance, environment, active) && device.get_transient_draws() == singular + 2);
	instance.transform.basis = Basis();
	FF_LIT_CHECK(context.resize(160, 100) == OK && context.prepare_resources() == OK);
	// Restore fixture dimensions after forcing device buffer release/recovery.
	FF_LIT_CHECK(context.resize(128, 96) == OK && context.prepare_resources() == OK);
	FF_LIT_CHECK(_compare(context, targets, renderer, instance, environment, active));
	FF_LIT_CHECK(cache.get(snapshot.identity, snapshot.version, 0, geometry, info, anchor, extent, true) == OK && geometry == original && device.get_creations() == 1);
	// Revision changes cannot draw the old native normal data, even inside an active pass.
	FF_LIT_CHECK(targets.begin_frame(0) == OK && targets.clear_render_target(Color(0, 0, 1)) == OK && targets.begin_draw(true) == OK);
	PoolVector<uint8_t> patch;
	FF_LIT_CHECK(patch.resize(12) == OK);
	{
		PoolVector<uint8_t>::Write write = patch.write();
		const float normal[] = { 0, 1, 0 };
		memcpy(write.ptr(), normal, sizeof(normal));
	}
	FF_LIT_CHECK(meshes.mesh_surface_update_region(mesh, 0, snapshot.offsets[VS::ARRAY_NORMAL], patch) == OK);
	CameraMatrix projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	RasterizerScene::InstanceBase *instances[] = { &instance };
	FF_LIT_CHECK(renderer.render(Transform(), projection, instances, 1, environment, active.ptr(), active.size()) == OK && targets.end_frame(false) == OK);
	FF_LIT_CHECK(cache.get_retired_count() == 1 && device.get_creations() == 1);
	FF_LIT_CHECK(context.prepare_resources() == OK && device.get_creations() == 2);
	FF_LIT_CHECK(_compare(context, targets, renderer, instance, environment, active));
	FF_LIT_CHECK(meshes.free(mesh) == OK && context.prepare_resources() == OK && device.get_geometry_memory_usage() == 0);
	const RID no_normals = TestFixedFunctionLambert::_mesh(meshes, 0, false);
	instance.base = no_normals;
	const int retained = device.get_retained_draws();
	FF_LIT_CHECK(_compare(context, targets, renderer, instance, environment, active) && device.get_retained_draws() == retained + 1);
	FF_LIT_CHECK(meshes.free(no_normals) == OK && context.prepare_resources() == OK);
	FF_LIT_CHECK(_missing_normals(context, targets, renderer, environment, active));
	FF_LIT_CHECK(_benchmark(context, targets, renderer, material, environment, active));
	FF_LIT_CHECK(_multimesh_light_range(context, targets, renderer, material, environment));
	for (int i = 0; i < sources.size(); i++) {
		FF_LIT_CHECK(lights.free(active[i]) == OK && lights.free(sources[i]) == OK);
	}
	FF_LIT_CHECK(materials.free(material) == OK && materials.free(shader) == OK && context.get_scene_storage().free(environment) == OK);
	FF_LIT_CHECK(_normal_maps(context, targets, renderer));
	FF_LIT_CHECK(_transient_occlusion(context, targets, renderer));
	FF_LIT_CHECK(context.finalize() == OK && device.get_geometry_memory_usage() == 0);
	return true;
}

bool TestFixedFunctionRetainedLighting::_multimesh_light_range(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material, RID p_environment) {
	TestFixedFunctionRetainedScene::FFDevice &device = static_cast<TestFixedFunctionRetainedScene::FFDevice &>(p_context.get_device());
	device.set_indexed_triangles_enabled(true);
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	const RID mesh = TestFixedFunctionLambert::_mesh(meshes, 0, true);
	const RID multimesh = meshes.multimesh_create();
	FF_LIT_CHECK(meshes.multimesh_allocate(multimesh, 8, VS::MULTIMESH_COLOR_NONE, VS::MULTIMESH_CUSTOM_DATA_NONE) == OK && meshes.multimesh_set_mesh(multimesh, mesh) == OK);
	const Basis basis = Basis().scaled(Vector3(0.125, 0.125, 0.125));
	for (int i = 0; i < 8; i++) {
		const Vector3 position(real_t(2.5) + (i % 4) * real_t(0.375), i < 4 ? real_t(-0.25) : real_t(0.25), 0);
		FF_LIT_CHECK(meshes.multimesh_set_transform(multimesh, i, Transform(basis, position)) == OK);
	}
	// The culling override covers neither the light nor the copies.
	FF_LIT_CHECK(meshes.mesh_set_custom_aabb(mesh, AABB(Vector3(0, 8, 0), Vector3(1, 1, 1))) == OK);
	TestFixedFunctionLambert::FFInstance instance(multimesh, p_material);
	instance.base_type = VS::INSTANCE_MULTIMESH;
	instance.transform.origin.x = -2;
	instance.layer_mask = 1;
	const RID source = lights.light_create(VS::LIGHT_OMNI);
	FixedFunctionLightStorage::FFLightData light;
	FF_LIT_CHECK(lights.light_get_data(source, light) == OK);
	light.color = Color(1, 0, 0);
	light.parameters[VS::LIGHT_PARAM_RANGE] = 0.5;
	FF_LIT_CHECK(lights.light_set_data(source, light) == OK);
	const RID active = lights.light_instance_create(source);
	Vector<RID> local_lights;
	local_lights.push_back(active);
	Transform pose;
	pose.origin = Vector3(-2, 0, -1.75);
	FF_LIT_CHECK(lights.light_instance_set_transform(active, pose) == OK);
	// This light reaches the source mesh AABB, but every copy is out of range.
	std::vector<uint32_t> reference, pixels;
	FF_LIT_CHECK(_compare(p_context, p_targets, p_renderer, instance, p_environment, local_lights, Transform(), &reference));
	for (int i = 0; i < 7; i++) {
		FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, p_environment, local_lights, pixels) && _images(pixels, reference));
	}
	int draws = device.get_retained_draws();
	FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, p_environment, local_lights, pixels) && device.get_retained_draws() == draws + 1 && _images(pixels, reference));
	pose.origin = Vector3(1.625, 0.25, -1.75);
	FF_LIT_CHECK(lights.light_instance_set_transform(active, pose) == OK);
	const int batched = device.get_retained_draws();
	std::vector<uint32_t> lit, per_copy;
	FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, p_environment, local_lights, lit) && device.get_retained_draws() == batched + 1 && lit != reference);
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const char *retained_setting = "rendering/quality/fixed_function/retained_geometry";
	settings->set(retained_setting, false);
	FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, p_environment, local_lights, per_copy) && _images(lit, per_copy));
	settings->set(retained_setting, true);

	// Each copy may choose its own subset when the shared shader budget is exceeded.
	Vector<RID> crowded = local_lights;
	for (uint32_t i = 0; i < device.get_max_per_pixel_lights(); i++) {
		const RID extra = lights.light_instance_create(source);
		FF_LIT_CHECK(lights.light_instance_set_transform(extra, pose) == OK);
		crowded.push_back(extra);
	}
	draws = device.get_retained_draws();
	FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, p_environment, crowded, pixels) && device.get_retained_draws() == draws + 8 && pixels != lit);
	settings->set(retained_setting, false);
	FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, p_environment, crowded, per_copy) && _images(pixels, per_copy));
	settings->set(retained_setting, true);
	for (int i = 1; i < crowded.size(); i++) {
		FF_LIT_CHECK(lights.free(crowded[i]) == OK);
	}
	light.flags |= FixedFunctionLightStorage::LIGHT_NEGATIVE;
	FF_LIT_CHECK(lights.light_set_data(source, light) == OK);
	draws = device.get_retained_draws();
	FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, p_environment, local_lights, pixels) && device.get_retained_draws() == draws + 8);
	light.flags &= ~FixedFunctionLightStorage::LIGHT_NEGATIVE;
	FF_LIT_CHECK(lights.light_set_data(source, light) == OK);
	const char *pixel_setting = "rendering/quality/fixed_function/per_pixel_lighting";
	settings->set(pixel_setting, false);
	draws = device.get_retained_draws();
	FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, p_environment, local_lights, pixels) && device.get_retained_draws() == draws);
	settings->set(pixel_setting, true);
	light.cull_mask = 2;
	FF_LIT_CHECK(lights.light_set_data(source, light) == OK);
	draws = device.get_retained_draws();
	FF_LIT_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, p_environment, local_lights, pixels) && device.get_retained_draws() == draws + 1 && _images(pixels, reference));
	FF_LIT_CHECK(lights.free(active) == OK && lights.free(source) == OK);
	FF_LIT_CHECK(meshes.free(multimesh) == OK && meshes.free(mesh) == OK && p_context.prepare_resources() == OK);
	device.set_indexed_triangles_enabled(false);
	printf("PASS: lit MultiMesh batches respect copy bounds, moving lights, layer masks, and the shared shader budget.\n");
	return true;
}

bool TestFixedFunctionRetainedLighting::_missing_normals(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_environment, const Vector<RID> &p_lights) {
	TestFixedFunctionRetainedScene::FFDevice &device = static_cast<TestFixedFunctionRetainedScene::FFDevice &>(p_context.get_device());
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID shader = materials.shader_create(), material = materials.material_create();
	FF_LIT_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; uniform sampler2D image; void fragment(){ ALBEDO=texture(image,UV).rgb; }") == OK && materials.material_set_shader(material, shader) == OK);
	Ref<Image> image;
	image.instance();
	image->create(2, 2, false, Image::FORMAT_RGBA8);
	image->lock();
	image->set_pixel(0, 0, Color(1, 0.25, 0.25));
	image->set_pixel(1, 0, Color(0.25, 1, 0.25));
	image->set_pixel(0, 1, Color(0.25, 0.25, 1));
	image->set_pixel(1, 1, Color(1, 1, 1));
	image->unlock();
	const RID texture = textures.texture_create();
	FF_LIT_CHECK(textures.texture_allocate_2d(texture, Size2i(2, 2), Image::FORMAT_RGBA8, VS::TEXTURE_FLAG_FILTER, 0) == OK && textures.texture_set_data(texture, image) == OK && materials.material_set_param(material, "image", texture) == OK);
	Array arrays;
	arrays.resize(VS::ARRAY_MAX);
	PoolVector<Vector3> positions, normals;
	positions.push_back(Vector3(-1, -1, 0));
	positions.push_back(Vector3(1, -1, 0));
	positions.push_back(Vector3(0, 1, 0));
	positions.push_back(Vector3(0, -1, 1));
	PoolVector<Vector2> uv;
	for (int i = 0; i < 4; i++) {
		uv.push_back(Vector2(i & 1 ? 0.75 : 0.25, i & 2 ? 0.75 : 0.25));
		normals.push_back(Vector3());
	}
	PoolVector<int> indices;
	const int order[] = { 0, 2, 1, 0, 1, 3 };
	for (int i = 0; i < 6; i++) {
		indices.push_back(order[i]);
	}
	arrays[VS::ARRAY_VERTEX] = positions;
	arrays[VS::ARRAY_TEX_UV] = uv;
	arrays[VS::ARRAY_INDEX] = indices;
	for (int mode = 0; mode < 4; mode++) {
		normals.write()[0] = mode == 1 || mode == 2 ? Vector3(0.6, 0, 0.8) : Vector3();
		arrays[VS::ARRAY_NORMAL] = mode ? Variant(normals) : Variant();
		const RID source = VS::get_singleton()->mesh_create();
		VS::get_singleton()->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_TRIANGLES, arrays, Array(), 0);
		const RID mesh = meshes.mesh_create();
		FF_LIT_CHECK(meshes.mesh_add_surface(mesh, VS::get_singleton()->mesh_surface_get_format(source, 0), VS::PRIMITIVE_TRIANGLES, VS::get_singleton()->mesh_surface_get_array(source, 0), 4, VS::get_singleton()->mesh_surface_get_index_array(source, 0), 6, AABB(Vector3(-1, -1, 0), Vector3(2, 2, 1)), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
		VS::get_singleton()->free(source);
		FixedFunctionMeshStorage::FFSurfaceData before, after;
		FF_LIT_CHECK(meshes.mesh_surface_get_data(mesh, 0, before) == OK);
		if (mode == 2) {
			PoolVector<uint8_t> patch;
			patch.resize(4);
			{
				PoolVector<uint8_t>::Write data = patch.write();
				const float invalid = std::numeric_limits<float>::quiet_NaN();
				memcpy(data.ptr(), &invalid, 4);
			}
			FF_LIT_CHECK(meshes.mesh_surface_update_region(mesh, 0, before.offsets[VS::ARRAY_NORMAL] + 2 * before.strides[VS::ARRAY_NORMAL], patch) == OK && meshes.mesh_surface_get_data(mesh, 0, before) == OK);
		}
		TestFixedFunctionLambert::FFInstance instance(mesh, material);
		const Transform camera(Basis(Vector3(1, 0, 0), real_t(-0.29)), Vector3(0.07, 0.11, 0.05));
		const int retained = device.get_retained_draws(), created = device.get_creations();
		std::vector<uint32_t> pixels;
		FF_LIT_CHECK(_compare(p_context, p_targets, p_renderer, instance, p_environment, p_lights, camera, &pixels));
		// Both differently oriented faces must be visible, rather than comparing two empty images.
		const Vector3 centers[] = { Vector3(0, -real_t(1) / 3, 0), Vector3(0, -1, real_t(1) / 3) };
		for (int face = 0; face < 2; face++) {
			const Vector3 view = camera.affine_inverse().xform(instance.transform.xform(centers[face]));
			const int x = int(Math::floor((view.x + 2) * 32));
			const int y = int(Math::floor((real_t(1.5) - view.y) * 32));
			FF_LIT_CHECK(x >= 0 && x < 128 && y >= 0 && y < 96 && pixels[y * 128 + x] != 0xff0000ff);
		}
		instance.transform.basis = Basis(Vector3(0, 1, 0), real_t(0.17));
		instance.transform.basis.scale(Vector3(-0.8, 1.1, 0.7));
		FF_LIT_CHECK(_compare(p_context, p_targets, p_renderer, instance, p_environment, p_lights, camera));
		FF_LIT_CHECK(device.get_retained_draws() == retained + 2 && device.get_creations() == created + 1);
		const int generated_count = mode == 1 || mode == 2 ? 5 : 6;
		FF_LIT_CHECK(device.get_geometry_memory_usage() == uint64_t(generated_count) * sizeof(FixedFunctionLightingVertex) + 6 * sizeof(uint16_t));
		FF_LIT_CHECK(meshes.mesh_surface_get_data(mesh, 0, after) == OK && after.vertex_count == 4 && after.index_count == 6 && after.identity == before.identity && after.version == before.version && after.array.size() == before.array.size() && after.index_array.size() == before.index_array.size());
		{
			const PoolVector<uint8_t>::Read old_vertices = before.array.read(), new_vertices = after.array.read();
			const PoolVector<uint8_t>::Read old_indices = before.index_array.read(), new_indices = after.index_array.read();
			FF_LIT_CHECK(memcmp(old_vertices.ptr(), new_vertices.ptr(), before.array.size()) == 0 && memcmp(old_indices.ptr(), new_indices.ptr(), before.index_array.size()) == 0);
		}
		FF_LIT_CHECK(meshes.free(mesh) == OK && p_context.prepare_resources() == OK && device.get_geometry_memory_usage() == 0);
	}
	FF_LIT_CHECK(materials.free(material) == OK && materials.free(shader) == OK && textures.free(texture) == OK);
	printf("PASS: retained missing/zero/nonfinite normals, mixed authored corners, hard edges, UVs, mirrors and immutable source data.\n");
	return true;
}

bool TestFixedFunctionRetainedLighting::_benchmark(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material, RID p_environment, const Vector<RID> &p_lights) {
	if (!OS::get_singleton()->get_cmdline_args().find("--lit-geometry-benchmark")) {
		return true;
	}
	FF_LIT_CHECK(p_context.resize(64, 64) == OK && p_context.prepare_resources() == OK);
	Array arrays;
	arrays.resize(VS::ARRAY_MAX);
	PoolVector<Vector3> positions, normals;
	PoolVector<int> indices;
	for (int y = 0; y < 9; y++) {
		for (int x = 0; x < 17; x++) {
			positions.push_back(Vector3(real_t(x) / 8 - 1, real_t(y) / 4 - 1, 0));
			normals.push_back(Vector3(0, 0, 1));
		}
	}
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 16; x++) {
			const int corners[] = { y * 17 + x, (y + 1) * 17 + x, y * 17 + x + 1, y * 17 + x + 1, (y + 1) * 17 + x, (y + 1) * 17 + x + 1 };
			for (int i = 0; i < 6; i++) {
				indices.push_back(corners[i]);
			}
		}
	}
	for (int generated = 0; generated < 2; generated++) {
		arrays[VS::ARRAY_VERTEX] = positions;
		arrays[VS::ARRAY_NORMAL] = generated ? Variant() : Variant(normals);
		arrays[VS::ARRAY_INDEX] = indices;
		const RID source = VS::get_singleton()->mesh_create();
		VS::get_singleton()->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_TRIANGLES, arrays, Array(), 0);
		FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
		const RID mesh = meshes.mesh_create();
		FF_LIT_CHECK(meshes.mesh_add_surface(mesh, VS::get_singleton()->mesh_surface_get_format(source, 0), VS::PRIMITIVE_TRIANGLES, VS::get_singleton()->mesh_surface_get_array(source, 0), 153, VS::get_singleton()->mesh_surface_get_index_array(source, 0), 768, AABB(Vector3(-1, -1, 0), Vector3(2, 2, 0)), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
		VS::get_singleton()->free(source);
		Vector<RasterizerScene::InstanceBase *> instances;
		for (int i = 0; i < 16; i++) {
			instances.push_back(memnew(TestFixedFunctionLambert::FFInstance(mesh, p_material)));
		}
		CameraMatrix projection;
		projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
		std::vector<uint32_t> reference, pixels(64 * 64);
		for (int profile = 0; profile < 2; profile++) {
			ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", profile == 0);
			std::vector<uint64_t> submitted, completed;
			for (int sample = 0; sample < 10; sample++) {
				const uint64_t start = OS::get_singleton()->get_ticks_usec();
				FF_LIT_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
				FF_LIT_CHECK(p_renderer.render(Transform(), projection, instances.ptrw(), instances.size(), p_environment, p_lights.ptr(), p_lights.size()) == OK && p_targets.end_frame(false) == OK);
				const uint64_t submission = OS::get_singleton()->get_ticks_usec() - start;
				FF_LIT_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
				const uint64_t completion = OS::get_singleton()->get_ticks_usec() - start;
				if (!profile) {
					reference = pixels;
				} else {
					FF_LIT_CHECK(_images(pixels, reference));
				}
				if (sample) {
					submitted.push_back(submission);
					completed.push_back(completion);
				}
			}
			std::sort(submitted.begin(), submitted.end());
			std::sort(completed.begin(), completed.end());
			printf("PERF3D_LIT_GEOMETRY normals=%s retained=%d pointer_bits=%u triangles=4096 instances=16 source_vertices=153 retained_vertices=%d lights=%d samples=9 submit_median_us=%llu complete_median_us=%llu complete_max_us=%llu\n", generated ? "generated" : "authored", int(!profile), unsigned(sizeof(void *) * 8), generated ? 768 : 153, p_lights.size(), static_cast<unsigned long long>(submitted[4]), static_cast<unsigned long long>(completed[4]), static_cast<unsigned long long>(completed[8]));
		}
		for (int i = 0; i < instances.size(); i++) {
			memdelete(instances[i]);
		}
		FF_LIT_CHECK(meshes.free(mesh) == OK && p_context.prepare_resources() == OK);
	}
	return true;
}

bool TestFixedFunctionRetainedLighting::test(FixedFunctionGraphicsDevice &p_device, void *p_window) {
	const Variant retained = GLOBAL_GET("rendering/quality/fixed_function/retained_geometry");
	const Variant pixel = GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting");
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/per_pixel_lighting", true);
	const bool passed = _run(p_device, p_window);
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", retained);
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/per_pixel_lighting", pixel);
	if (passed) {
		printf("PASS: retained SM3 object-space normals, live transforms/lights/materials, masks, fallback, source revisions and recovery.\n");
	}
	return passed;
}

#undef FF_LIT_CHECK
