#include "test_retained_scene.h"

#include "core/os/os.h"
#include "core/project_settings.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

#define FF_RETAINED_CHECK(m_condition)                                         \
	do {                                                                       \
		if (!(m_condition)) {                                                  \
			printf("Retained scene FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                      \
		}                                                                      \
	} while (false)

Error TestFixedFunctionRetainedScene::FFDevice::geometry_create(const PoolVector<FixedFunctionVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, RID &r_geometry) {
	_creations++;
	return _creation_error == OK ? _device.geometry_create(p_vertices, p_indices, r_geometry) : _creation_error;
}

Error TestFixedFunctionRetainedScene::FFDevice::draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_count, const CameraMatrix &p_matrix, RID p_texture, const FFDrawParameters &p_state) {
	_transient_draws++;
	return _device.draw_triangles(p_vertices, p_count, p_matrix, p_texture, p_state);
}

Error TestFixedFunctionRetainedScene::FFDevice::draw_geometry(RID p_geometry, const CameraMatrix &p_matrix, RID p_texture, const FFDrawParameters &p_state, const Color *p_color) {
	_retained_draws++;
	return _draw_error == OK ? _device.draw_geometry(p_geometry, p_matrix, p_texture, p_state, p_color) : _draw_error;
}

TestFixedFunctionRetainedScene::FFInstance::FFInstance(RID p_mesh, RID p_material) {
	base = p_mesh;
	base_type = VS::INSTANCE_MESH;
	material_override = p_material;
	transform.origin.z = -3;
	depth = 0;
}

bool TestFixedFunctionRetainedScene::_color_matches(uint32_t p_actual, uint32_t p_expected, int p_tolerance) {
	for (int shift = 0; shift < 32; shift += 8) {
		if (ABS(int((p_actual >> shift) & 255) - int((p_expected >> shift) & 255)) > p_tolerance) {
			printf("Pixel %08x expected %08x (tolerance %d)\n", unsigned(p_actual), unsigned(p_expected), p_tolerance);
			return false;
		}
	}
	return true;
}

bool TestFixedFunctionRetainedScene::_add(FixedFunctionMeshStorage &p_meshes, RID p_mesh) {
	const float coordinates[] = { -0.6f, -0.6f, 0, 0, 0.6f, 0, 0.6f, -0.6f, 0 };
	PoolVector<uint8_t> vertices;
	FF_RETAINED_CHECK(vertices.resize(sizeof(coordinates)) == OK);
	{
		PoolVector<uint8_t>::Write write = vertices.write();
		memcpy(write.ptr(), coordinates, sizeof(coordinates));
	}
	FF_RETAINED_CHECK(p_meshes.mesh_add_surface(p_mesh, VS::ARRAY_FORMAT_VERTEX, VS::PRIMITIVE_TRIANGLES, vertices, 3, PoolVector<uint8_t>(), 0, AABB(Vector3(-0.6, -0.6, 0), Vector3(1.2, 1.2, 0)), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
	return true;
}

bool TestFixedFunctionRetainedScene::_draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, FFInstance **p_instances, int p_count, std::vector<uint32_t> &r_pixels, bool p_open, const CameraMatrix *p_projection) {
	if (p_open) {
		FF_RETAINED_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
	}
	CameraMatrix projection;
	if (p_projection) {
		projection = *p_projection;
	} else {
		projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	}
	FF_RETAINED_CHECK(p_context.get_storage_queries().render_info_begin_capture() == OK);
	Vector<RasterizerScene::InstanceBase *> instances;
	for (int i = 0; i < p_count; i++) {
		instances.push_back(p_instances[i]);
	}
	FF_RETAINED_CHECK(p_renderer.render(Transform(), projection, instances.ptrw(), instances.size()) == OK);
	FF_RETAINED_CHECK(p_targets.end_frame(false) == OK);
	r_pixels.resize(size_t(p_context.get_device().get_width()) * p_context.get_device().get_height());
	FF_RETAINED_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FF_RETAINED_CHECK(p_context.get_storage_queries().render_info_end_capture() == OK);
	return true;
}

bool TestFixedFunctionRetainedScene::_projection() {
	FixedFunctionSceneRenderer::FFDrawPacket packet;
	packet.view_from_model.origin.z = -3;
	CameraMatrix perspective, result;
	perspective.set_perspective(60, real_t(4) / 3, 0.1, 100);
	FF_RETAINED_CHECK(FixedFunctionSceneRenderer::_geometry_projection(packet, Vector3(), Vector3(1, 1, 1), perspective, Size2i(128, 96), result));
	packet.view_from_model.origin = Vector3(-100000000, 0, -3);
	FF_RETAINED_CHECK(FixedFunctionSceneRenderer::_geometry_projection(packet, Vector3(100000000, 0, 0), Vector3(1, 1, 1), perspective, Size2i(128, 96), result));
	FF_RETAINED_CHECK(!FixedFunctionSceneRenderer::_geometry_projection(packet, Vector3(), Vector3(1, 1, 1), perspective, Size2i(128, 96), result));
	packet.view_from_model.origin = Vector3();
	// Well-conditioned bounds may cross the camera. An arbitrarily close near
	// plane can still amplify float error past the retained depth tolerance.
	FF_RETAINED_CHECK(FixedFunctionSceneRenderer::_geometry_projection(packet, Vector3(), Vector3(1, 1, 1), perspective, Size2i(128, 96), result));
	perspective.set_perspective(60, real_t(4) / 3, 0.001, 100);
	FF_RETAINED_CHECK(!FixedFunctionSceneRenderer::_geometry_projection(packet, Vector3(), Vector3(1, 1, 1), perspective, Size2i(128, 96), result));
	return true;
}

bool TestFixedFunctionRetainedScene::_test_clipped_projection(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_front_material, RID p_back_material) {
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID front_mesh = meshes.mesh_create(), back_mesh = meshes.mesh_create();
	const float coordinates[] = { -0.4f, -0.13f, 0, 0, 0.1f, 0.7f, 0.4f, -0.13f, 0 };
	PoolVector<uint8_t> vertices;
	FF_RETAINED_CHECK(vertices.resize(sizeof(coordinates)) == OK);
	{
		PoolVector<uint8_t>::Write write = vertices.write();
		memcpy(write.ptr(), coordinates, sizeof(coordinates));
	}
	const AABB bounds(Vector3(-0.4, -0.13, 0), Vector3(0.8, 0.23, 0.7));
	FF_RETAINED_CHECK(meshes.mesh_add_surface(front_mesh, VS::ARRAY_FORMAT_VERTEX, VS::PRIMITIVE_TRIANGLES, vertices, 3, PoolVector<uint8_t>(), 0, bounds, Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK && _add(meshes, back_mesh));
	FFInstance front(front_mesh, p_front_material), back(back_mesh, p_back_material);
	front.transform.origin.z = -0.6;
	back.transform.origin.z = -0.4;
	FFInstance *instances[] = { &front, &back };
	CameraMatrix projection;
	projection.set_perspective(60, real_t(4) / 3, 0.1, 20);
	std::vector<uint32_t> reference, pixels;
	ProjectSettings *settings = ProjectSettings::get_singleton();
	settings->set("rendering/quality/fixed_function/retained_geometry", false);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 2, reference, true, &projection));
	settings->set("rendering/quality/fixed_function/retained_geometry", true);
	FFDevice &device = static_cast<FFDevice &>(p_context.get_device());
	const int draws = device.get_retained_draws();
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 2, pixels, true, &projection));
	FF_RETAINED_CHECK(device.get_retained_draws() == draws + 2 && pixels == reference);
	const uint32_t width = device.get_width();
	// The back triangle is visible above the near clip edge and where the red
	// triangle slopes behind it. The red triangle wins in the middle.
	FF_RETAINED_CHECK(pixels[12 * width + width / 2] == 0xff00ff00);
	FF_RETAINED_CHECK(pixels[48 * width + width / 2] == 0xffff0000);
	FF_RETAINED_CHECK(pixels[64 * width + width / 2] == 0xff00ff00);

	// Moving the near plane much closer makes only the crossing draw unsafe.
	projection.set_perspective(60, real_t(4) / 3, 0.001, 20);
	settings->set("rendering/quality/fixed_function/retained_geometry", false);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 2, reference, true, &projection));
	settings->set("rendering/quality/fixed_function/retained_geometry", true);
	const int fallback = device.get_transient_draws();
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 2, pixels, true, &projection));
	FF_RETAINED_CHECK(device.get_retained_draws() == draws + 3 && device.get_transient_draws() == fallback + 1 && pixels == reference);
	FF_RETAINED_CHECK(meshes.free(front_mesh) == OK && meshes.free(back_mesh) == OK && p_context.prepare_resources() == OK);
	printf("PASS: retained near-plane clipping, depth ordering and precision fallback.\n");
	return true;
}

bool TestFixedFunctionRetainedScene::_run(FixedFunctionGraphicsDevice &p_device, void *p_window) {
	FF_RETAINED_CHECK(_projection());
	FFDevice device(p_device);
	FixedFunctionRenderContext context(device, p_window, 128, 96, false);
	FixedFunctionRenderTargetManager targets(context);
	FixedFunctionSceneRenderer renderer(context, targets);
	FF_RETAINED_CHECK(context.initialize() == OK);
	FixedFunctionMeshStorage &meshes = context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = context.get_material_storage();
	FixedFunctionGeometryCache &cache = context.get_geometry_cache();
	const RID shader = materials.shader_create(), other_shader = materials.shader_create();
	const RID material = materials.material_create(), other_material = materials.material_create();
	const String source = "shader_type spatial; render_mode unshaded,cull_disabled; uniform vec4 tint : hint_color=vec4(1); void fragment(){ ALBEDO=tint.rgb; }";
	FF_RETAINED_CHECK(materials.shader_set_code(shader, source) == OK && materials.material_set_shader(material, shader) == OK);
	FF_RETAINED_CHECK(materials.material_set_shader(other_material, shader) == OK);
	FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(1, 0, 0)) == OK && materials.material_set_param(other_material, "tint", Color(0, 1, 0)) == OK);
	RID mesh = meshes.mesh_create();
	FF_RETAINED_CHECK(_add(meshes, mesh));
	FixedFunctionMeshStorage::FFSurfaceData snapshot;
	FF_RETAINED_CHECK(meshes.mesh_surface_get_data(mesh, 0, snapshot) == OK);
	FFInstance instance(mesh, material), other(mesh, other_material);
	FFInstance *instances[] = { &instance, &other };
	std::vector<uint32_t> pixels, reference;
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", false);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, reference));
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", true);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && pixels == reference);
	FF_RETAINED_CHECK(device.get_creations() == 1 && device.get_retained_draws() == 1 && cache.get_entry_count() == 1);
	RID native;
	FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
	Vector3 anchor, extent;
	FF_RETAINED_CHECK(cache.get(snapshot.identity, snapshot.version, 0, native, info, anchor, extent) == OK);
	const uint32_t original_id = native.get_id();
	FF_RETAINED_CHECK(info.vertex_count == 3 && info.primitive_count == 1 && info.draw_calls == 1);
	device.set_draw_error(ERR_UNAVAILABLE);
	const int before_rejection = device.get_transient_draws();
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && pixels == reference);
	FF_RETAINED_CHECK(device.get_transient_draws() == before_rejection + 1);
	device.set_draw_error(OK);
	int count = 0;
	FF_RETAINED_CHECK(context.get_storage_queries().get_captured_render_info(VS::INFO_DRAW_CALLS_IN_FRAME, count) == OK && count == 1);
	FF_RETAINED_CHECK(context.get_storage_queries().get_captured_render_info(VS::INFO_VERTICES_IN_FRAME, count) == OK && count == 3);
	FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(0, 1, 0)) == OK);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && pixels[48 * 128 + 64] == 0xff00ff00);
	FF_RETAINED_CHECK(cache.get(snapshot.identity, snapshot.version, 0, native, info, anchor, extent) == OK && native.get_id() == original_id && device.get_creations() == 1);
	// Same resource, different materials/transforms in one pass.
	instance.transform.origin.x = -1;
	other.transform.origin.x = 1;
	FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(1, 0, 0)) == OK);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 2, pixels) && pixels[48 * 128 + 32] == 0xffff0000 && pixels[48 * 128 + 96] == 0xff00ff00);
	FF_RETAINED_CHECK(device.get_creations() == 1);
	FF_RETAINED_CHECK(context.get_storage_queries().get_captured_render_info(VS::INFO_OBJECTS_IN_FRAME, count) == OK && count == 2);
	// Blend/cutoff changes reuse geometry; alpha still follows the normal 8-bit path.
	instance.transform.origin.x = 0;
	if (device.get_capabilities().alpha_cutoff) {
		FF_RETAINED_CHECK(materials.shader_set_code(shader, source.replace("ALBEDO=tint.rgb;", "ALBEDO=tint.rgb; ALPHA=tint.a; ALPHA_SCISSOR=0.5;")) == OK);
		FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(1, 0, 0, 0.25)) == OK);
		FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && pixels[48 * 128 + 64] == 0xff0000ff);
		FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(1, 0, 0, 0.75)) == OK);
		FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && pixels[48 * 128 + 64] == 0xffff0000 && device.get_creations() == 1);
	}
	FF_RETAINED_CHECK(materials.shader_set_code(shader, source) == OK);
	instance.transform.origin.x = -1;
	// A following CPU draw must restore diffuse RGBA after the color override.
	FF_RETAINED_CHECK(materials.shader_set_code(other_shader, source.replace("tint.rgb", "tint.rgb*COLOR.rgb")) == OK && materials.material_set_shader(other_material, other_shader) == OK);
	const int transient = device.get_transient_draws();
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 2, pixels) && pixels[48 * 128 + 32] == 0xffff0000 && pixels[48 * 128 + 96] == 0xff00ff00);
	FF_RETAINED_CHECK(device.get_transient_draws() == transient + 1 && device.get_creations() == 1);
	instance.transform.origin.x = 0;
	FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(0, 1, 0)) == OK);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && pixels[48 * 128 + 56] == 0xff00ff00);
	// Accept CPU writes during an open depth pass; old native ownership retires later.
	FF_RETAINED_CHECK(targets.begin_frame(0) == OK && targets.clear_render_target(Color(0, 0, 1)) == OK && targets.begin_draw(true) == OK);
	PoolVector<uint8_t> patch;
	FF_RETAINED_CHECK(patch.resize(4) == OK);
	{
		PoolVector<uint8_t>::Write write = patch.write();
		const float x = -0.2f;
		memcpy(write.ptr(), &x, 4);
	}
	FF_RETAINED_CHECK(meshes.mesh_surface_update_region(mesh, 0, 0, patch) == OK);
	FixedFunctionMeshStorage::FFSurfaceData updated;
	FF_RETAINED_CHECK(meshes.mesh_surface_get_data(mesh, 0, updated) == OK && updated.version == snapshot.version + 1);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels, false) && pixels[48 * 128 + 56] == 0xff0000ff);
	FF_RETAINED_CHECK(cache.get_retired_count() == 1 && device.get_creations() == 1);
	FF_RETAINED_CHECK(context.prepare_resources() == OK && cache.get_retired_count() == 0 && device.get_creations() == 2);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && pixels[48 * 128 + 56] == 0xff0000ff);
	// Removing surface zero must preserve surface one's independent identity/cache.
	FF_RETAINED_CHECK(_add(meshes, mesh) && _draw(context, targets, renderer, instances, 1, pixels));
	FF_RETAINED_CHECK(meshes.mesh_surface_get_data(mesh, 1, snapshot) == OK && cache.get(snapshot.identity, snapshot.version, 0, native, info, anchor, extent) == OK);
	const uint32_t second_id = native.get_id();
	const int created = device.get_creations();
	FF_RETAINED_CHECK(meshes.mesh_remove_surface(mesh, 0) == OK && _draw(context, targets, renderer, instances, 1, pixels));
	FF_RETAINED_CHECK(cache.get(snapshot.identity, snapshot.version, 0, native, info, anchor, extent) == OK && native.get_id() == second_id && device.get_creations() == created);
	FF_RETAINED_CHECK(context.resize(160, 96) == OK && context.prepare_resources() == OK);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels));
	FF_RETAINED_CHECK(cache.get(snapshot.identity, snapshot.version, 0, native, info, anchor, extent) == OK && native.get_id() == second_id && device.get_creations() == created);
	FF_RETAINED_CHECK(meshes.free(mesh) == OK && context.prepare_resources() == OK && cache.get_entry_count() == 0 && device.get_geometry_memory_usage() == 0);
	// Unsupported caching is remembered, and keeps current CPU geometry drawable.
	device.set_creation_error(ERR_UNAVAILABLE);
	mesh = meshes.mesh_create();
	FF_RETAINED_CHECK(_add(meshes, mesh));
	instance.base = mesh;
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels));
	const int rejected = device.get_creations();
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && device.get_creations() == rejected);
	FF_RETAINED_CHECK(pixels[48 * 160 + 80] == 0xff00ff00);
	device.set_creation_error(ERR_OUT_OF_MEMORY);
	FF_RETAINED_CHECK(meshes.mesh_surface_update_region(mesh, 0, 0, patch) == OK && _draw(context, targets, renderer, instances, 1, pixels));
	FF_RETAINED_CHECK(pixels[48 * 160 + 80] == 0xff00ff00);
	const int exhausted = device.get_creations();
	device.set_creation_error(OK);
	FF_RETAINED_CHECK(meshes.mesh_surface_update_region(mesh, 0, 0, patch) == OK && _draw(context, targets, renderer, instances, 1, pixels));
	FF_RETAINED_CHECK(device.get_creations() == exhausted + 1);
	FF_RETAINED_CHECK(meshes.free(mesh) == OK && context.prepare_resources() == OK);
	FF_RETAINED_CHECK(_test_attributes(context, targets, renderer));
	FF_RETAINED_CHECK(_test_triangle_topology(context, targets, renderer));
	FF_RETAINED_CHECK(_test_billboards(context, targets, renderer));
	FF_RETAINED_CHECK(_test_points(context, targets, renderer));
	FF_RETAINED_CHECK(_test_spatial_blends(context, targets, renderer));
	FF_RETAINED_CHECK(_benchmark(context, targets, renderer, material));
	FF_RETAINED_CHECK(context.resize(128, 96) == OK && context.prepare_resources() == OK);
	FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(1, 0, 0)) == OK);
	FF_RETAINED_CHECK(materials.shader_set_code(other_shader, source) == OK);
	FF_RETAINED_CHECK(_test_clipped_projection(context, targets, renderer, material, other_material));
	FF_RETAINED_CHECK(_test_multimesh(context, targets, renderer, material));
	FF_RETAINED_CHECK(materials.free(material) == OK && materials.free(other_material) == OK && materials.free(shader) == OK && materials.free(other_shader) == OK);
	FF_RETAINED_CHECK(context.finalize() == OK && cache.get_entry_count() == 0 && cache.get_retired_count() == 0 && device.get_geometry_memory_usage() == 0);
	return true;
}

bool TestFixedFunctionRetainedScene::_test_multimesh(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material) {
	FFDevice &device = static_cast<FFDevice &>(p_context.get_device());
	device.set_indexed_triangles_enabled(true);
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", true);
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionGeometryCache &cache = p_context.get_geometry_cache();
	const RID mesh = meshes.mesh_create();
	FF_RETAINED_CHECK(_add(meshes, mesh));
	const RID multimesh = meshes.multimesh_create();
	FF_RETAINED_CHECK(meshes.multimesh_allocate(multimesh, 9, VS::MULTIMESH_COLOR_NONE, VS::MULTIMESH_CUSTOM_DATA_NONE, VS::MULTIMESH_TRANSFORM_3D) == OK);
	FF_RETAINED_CHECK(meshes.multimesh_set_mesh(multimesh, mesh) == OK);
	const Basis basis = Basis().scaled(Vector3(0.25, 0.25, 0.25));
	for (int i = 0; i < 8; i++) {
		const Vector3 position(real_t(i % 4) * real_t(0.5) - real_t(0.75), i < 4 ? real_t(-0.25) : real_t(0.25), 0);
		FF_RETAINED_CHECK(meshes.multimesh_set_transform(multimesh, i, Transform(basis, position)) == OK);
	}
	// The ninth copy is collapsed, as with unused CPU particle slots.
	FF_RETAINED_CHECK(meshes.multimesh_set_transform(multimesh, 8, Transform(Basis(0, 0, 0, 0, 0, 0, 0, 0, 0), Vector3())) == OK);
	FFInstance instance(multimesh, p_material);
	instance.base_type = VS::INSTANCE_MULTIMESH;
	FFInstance *instances[] = { &instance };
	std::vector<uint32_t> reference, pixels;
	FixedFunctionStorageQueries &queries = p_context.get_storage_queries();
	int draws, corners;
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, reference));
	FF_RETAINED_CHECK(queries.get_captured_render_info(VS::INFO_DRAW_CALLS_IN_FRAME, draws) == OK && draws == 8);
	for (int i = 0; i < 7; i++) {
		FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == reference);
	}
	FF_RETAINED_CHECK(queries.get_captured_render_info(VS::INFO_DRAW_CALLS_IN_FRAME, draws) == OK && draws == 1);
	FF_RETAINED_CHECK(queries.get_captured_render_info(VS::INFO_VERTICES_IN_FRAME, corners) == OK && corners == 24);
	device.set_draw_error(ERR_UNAVAILABLE);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == reference);
	FF_RETAINED_CHECK(queries.get_captured_render_info(VS::INFO_DRAW_CALLS_IN_FRAME, draws) == OK && draws == 8);
	device.set_draw_error(OK);
	FF_RETAINED_CHECK(meshes.multimesh_set_transform(multimesh, 0, Transform(basis, Vector3(-1.5, -0.25, 0))) == OK);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels != reference);
	FF_RETAINED_CHECK(queries.get_captured_render_info(VS::INFO_DRAW_CALLS_IN_FRAME, draws) == OK && draws == 8);
	reference = pixels;
	for (int i = 0; i < 7; i++) {
		FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == reference);
	}
	FF_RETAINED_CHECK(queries.get_captured_render_info(VS::INFO_DRAW_CALLS_IN_FRAME, draws) == OK && draws == 1);
	FF_RETAINED_CHECK(meshes.multimesh_set_visible(multimesh, 4) == OK);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels != reference);
	FF_RETAINED_CHECK(queries.get_captured_render_info(VS::INFO_DRAW_CALLS_IN_FRAME, draws) == OK && draws == 4);
	FF_RETAINED_CHECK(meshes.multimesh_set_visible(multimesh, 8) == OK);
	for (int i = 0; i < 7; i++) {
		FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == reference);
	}
	FF_RETAINED_CHECK(queries.get_captured_render_info(VS::INFO_DRAW_CALLS_IN_FRAME, draws) == OK && draws == 1);
	FF_RETAINED_CHECK(cache.get_entry_count() == 2);
	FF_RETAINED_CHECK(meshes.free(multimesh) == OK && p_context.prepare_resources() == OK && cache.get_entry_count() == 1);
	FF_RETAINED_CHECK(meshes.free(mesh) == OK && p_context.prepare_resources() == OK && cache.get_entry_count() == 0);
	device.set_indexed_triangles_enabled(false);
	printf("PASS: retained MultiMesh batches preserve pixels, copy visibility, transform edits, fallback and retirement.\n");
	return true;
}

bool TestFixedFunctionRetainedScene::_test_attributes(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FFDevice &device = static_cast<FFDevice &>(p_context.get_device());
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	Ref<Image> image;
	image.instance();
	image->create(2, 1, false, Image::FORMAT_RGBA8);
	image->lock();
	image->set_pixel(0, 0, Color(1, 0, 0));
	image->set_pixel(1, 0, Color(0, 1, 0));
	image->unlock();
	const RID texture = textures.texture_create();
	FF_RETAINED_CHECK(textures.texture_allocate_2d(texture, Size2i(2, 1), Image::FORMAT_RGBA8, 0, 0) == OK && textures.texture_set_data(texture, image) == OK);
	const RID shader = materials.shader_create(), material = materials.material_create();
	FF_RETAINED_CHECK(materials.material_set_shader(material, shader) == OK && materials.material_set_param(material, "image", texture) == OK);
	Array arrays;
	arrays.resize(VS::ARRAY_MAX);
	PoolVector<Vector3> positions;
	positions.push_back(Vector3(-0.6, -0.6, 0));
	positions.push_back(Vector3(0, 0.6, 0));
	positions.push_back(Vector3(0.6, -0.6, 0));
	PoolVector<Vector2> uv1, uv2;
	PoolVector<int> indices;
	for (int i = 0; i < 3; i++) {
		uv1.push_back(Vector2(0.25, 0.5));
		uv2.push_back(Vector2(0.75, 0.5));
		indices.push_back(i);
	}
	arrays[VS::ARRAY_VERTEX] = positions;
	arrays[VS::ARRAY_TEX_UV] = uv1;
	arrays[VS::ARRAY_TEX_UV2] = uv2;
	arrays[VS::ARRAY_INDEX] = indices;
	for (int compressed = 0; compressed < 2; compressed++) {
		const RID source = VS::get_singleton()->mesh_create();
		VS::get_singleton()->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_TRIANGLES, arrays, Array(), compressed ? uint32_t(VS::ARRAY_COMPRESS_DEFAULT) : 0);
		const RID mesh = meshes.mesh_create();
		FF_RETAINED_CHECK(meshes.mesh_add_surface(mesh, VS::get_singleton()->mesh_surface_get_format(source, 0), VS::PRIMITIVE_TRIANGLES, VS::get_singleton()->mesh_surface_get_array(source, 0), 3, VS::get_singleton()->mesh_surface_get_index_array(source, 0), 3, AABB(Vector3(-0.6, -0.6, 0), Vector3(1.2, 1.2, 0)), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
		VS::get_singleton()->free(source);
		FFInstance instance(mesh, material);
		FFInstance *instances[] = { &instance };
		for (int uv_set = 0; uv_set < 2; uv_set++) {
			const String code = "shader_type spatial; render_mode unshaded,cull_disabled; uniform sampler2D image; void fragment(){ ALBEDO=texture(image," + String(uv_set ? "UV2" : "UV") + ").rgb; }";
			FF_RETAINED_CHECK(materials.shader_set_code(shader, code) == OK);
			std::vector<uint32_t> reference, pixels;
			ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", false);
			FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, reference));
			ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", true);
			const int retained = device.get_retained_draws();
			FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == reference);
			FF_RETAINED_CHECK(device.get_retained_draws() == retained + 1);
			// D3D7 RGB can round a fully modulated texture channel down to 254.
			const uint32_t center = pixels[48 * device.get_width() + device.get_width() / 2];
			const uint32_t expected = uv_set ? 0xff00ff00 : 0xffff0000;
			const uint32_t rounded = expected - (uv_set ? 0x00000100 : 0x00010000);
			FF_RETAINED_CHECK(center == expected || center == rounded);
			FF_RETAINED_CHECK(p_context.get_geometry_cache().get_entry_count() == uv_set + 1);
		}
		FF_RETAINED_CHECK(meshes.free(mesh) == OK && p_context.prepare_resources() == OK && p_context.get_geometry_cache().get_entry_count() == 0);
	}
	FF_RETAINED_CHECK(materials.free(material) == OK && materials.free(shader) == OK && textures.free(texture) == OK);
	return true;
}

bool TestFixedFunctionRetainedScene::_benchmark(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material) {
	if (!OS::get_singleton()->get_cmdline_args().find("--mesh-geometry-benchmark")) {
		return true;
	}
	FF_RETAINED_CHECK(p_context.resize(64, 64) == OK && p_context.prepare_resources() == OK);
	PoolVector<uint8_t> vertices, indices;
	FF_RETAINED_CHECK(vertices.resize(17 * 9 * 12) == OK && indices.resize(256 * 3 * 2) == OK);
	{
		PoolVector<uint8_t>::Write vertex = vertices.write();
		for (int y = 0; y < 9; y++) {
			for (int x = 0; x < 17; x++) {
				const float position[] = { x / 8.0f - 1.0f, y / 4.0f - 1.0f, 0 };
				memcpy(vertex.ptr() + (y * 17 + x) * 12, position, sizeof(position));
			}
		}
		PoolVector<uint8_t>::Write index = indices.write();
		int cursor = 0;
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 16; x++) {
				const uint16_t corners[] = { uint16_t(y * 17 + x), uint16_t((y + 1) * 17 + x), uint16_t(y * 17 + x + 1), uint16_t(y * 17 + x + 1), uint16_t((y + 1) * 17 + x), uint16_t((y + 1) * 17 + x + 1) };
				memcpy(index.ptr() + cursor, corners, sizeof(corners));
				cursor += sizeof(corners);
			}
		}
	}
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID mesh = meshes.mesh_create();
	FF_RETAINED_CHECK(meshes.mesh_add_surface(mesh, VS::ARRAY_FORMAT_VERTEX | VS::ARRAY_FORMAT_INDEX, VS::PRIMITIVE_TRIANGLES, vertices, 153, indices, 768, AABB(Vector3(-1, -1, 0), Vector3(2, 2, 0)), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
	Vector<RasterizerScene::InstanceBase *> instances;
	for (int i = 0; i < 16; i++) {
		instances.push_back(memnew(FFInstance(mesh, p_material)));
	}
	CameraMatrix projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	std::vector<uint32_t> pixels(64 * 64), reference;
	for (int profile = 0; profile < 2; profile++) {
		ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", profile == 0);
		std::vector<uint64_t> submitted, completed;
		for (int sample = 0; sample < 10; sample++) {
			const uint64_t start = OS::get_singleton()->get_ticks_usec();
			FF_RETAINED_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
			FF_RETAINED_CHECK(p_renderer.render(Transform(), projection, instances.ptrw(), instances.size()) == OK && p_targets.end_frame(false) == OK);
			const uint64_t submission = OS::get_singleton()->get_ticks_usec() - start;
			FF_RETAINED_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
			const uint64_t completion = OS::get_singleton()->get_ticks_usec() - start;
			if (!profile) {
				reference = pixels;
			} else {
				FF_RETAINED_CHECK(pixels == reference);
			}
			if (sample) {
				submitted.push_back(submission);
				completed.push_back(completion);
			}
		}
		std::sort(submitted.begin(), submitted.end());
		std::sort(completed.begin(), completed.end());
		printf("PERF3D_MESH retained=%d pointer_bits=%u triangles=4096 instances=16 unique_per_mesh=153 samples=9 submit_median_us=%llu complete_median_us=%llu complete_max_us=%llu\n", int(!profile), unsigned(sizeof(void *) * 8), static_cast<unsigned long long>(submitted[4]), static_cast<unsigned long long>(completed[4]), static_cast<unsigned long long>(completed[8]));
	}
	for (int i = 0; i < instances.size(); i++) {
		memdelete(instances[i]);
	}
	FF_RETAINED_CHECK(meshes.free(mesh) == OK && p_context.prepare_resources() == OK);
	return true;
}

bool TestFixedFunctionRetainedScene::test(FixedFunctionGraphicsDevice &p_device, void *p_window) {
	const Variant previous = GLOBAL_GET("rendering/quality/fixed_function/retained_geometry");
	const bool result = _run(p_device, p_window);
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", previous);
	if (result) {
		printf("PASS: retained unlit meshes, source versions, material/instance reuse, deferred retirement, precision fallback and recovery.\n");
	}
	return result;
}

#undef FF_RETAINED_CHECK
