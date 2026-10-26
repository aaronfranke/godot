#include "test_retained_scene.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#define FF_RETAINED_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Retained scene FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

Error TestFixedFunctionRetainedScene::FFDevice::geometry_create(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) {
	_creations++;
	return _creation_error == OK ? _device.geometry_create(p_vertices, p_indices, r_geometry) : _creation_error;
}

Error TestFixedFunctionRetainedScene::FFDevice::draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_count, const Projection &p_matrix, RID p_texture, const FFDrawParameters &p_state) {
	_transient_draws++;
	return _device.draw_triangles(p_vertices, p_count, p_matrix, p_texture, p_state);
}

Error TestFixedFunctionRetainedScene::FFDevice::draw_geometry(RID p_geometry, const Projection &p_matrix, RID p_texture, const FFDrawParameters &p_state, const Color *p_color, uint32_t p_lod) {
	_retained_draws++;
	return _draw_error == OK ? _device.draw_geometry(p_geometry, p_matrix, p_texture, p_state, p_color, p_lod) : _draw_error;
}

RenderingServerTypes::RenderInfo TestFixedFunctionRetainedScene::_render_info;
TestFixedFunctionRetainedScene::FFInstance::FFInstance(FixedFunctionRenderContext &p_context, RID p_mesh, RID p_material) : FixedFunctionGeometryInstance(p_context, p_mesh) {
	set_material_override(p_material);
	Transform3D pose;
	pose.origin.z = -3;
	set_transform(pose, AABB(), AABB());
}

bool TestFixedFunctionRetainedScene::_color_matches(uint32_t p_actual, uint32_t p_expected, int p_tolerance) {
	for (int shift = 0; shift < 32; shift += 8) {
		if (Math::abs(int((p_actual >> shift) & 255) - int((p_expected >> shift) & 255)) > p_tolerance) {
			printf("Pixel %08x expected %08x (tolerance %d)\n", unsigned(p_actual), unsigned(p_expected), p_tolerance);
			return false;
		}
	}
	return true;
}

bool TestFixedFunctionRetainedScene::_add(FixedFunctionMeshStorage &p_meshes, RID p_mesh) {
	const float coordinates[] = { -0.6f, -0.6f, 0, 0, 0.6f, 0, 0.6f, -0.6f, 0 };
	Vector<uint8_t> vertices;
	FF_RETAINED_CHECK(vertices.resize(sizeof(coordinates)) == OK);
	{
		uint8_t *write = vertices.ptrw();
		memcpy(write, coordinates, sizeof(coordinates));
	}
	RenderingServerTypes::SurfaceData surface;
	surface.format |= RSE::ARRAY_FORMAT_VERTEX;
	surface.primitive = RSE::PRIMITIVE_TRIANGLES;
	surface.vertex_data = vertices;
	surface.vertex_count = 3;
	surface.aabb = AABB(Vector3(-0.6, -0.6, 0), Vector3(1.2, 1.2, 0));
	FF_RETAINED_CHECK(p_meshes.mesh_add_surface(p_mesh, surface) == OK);
	return true;
}

bool TestFixedFunctionRetainedScene::_draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, FFInstance **p_instances, int p_count, std::vector<uint32_t> &r_pixels, bool p_open, const Projection *p_projection, const FixedFunctionSceneRenderer::FFMeshLOD *p_lod) {
	if (p_open) {
		FF_RETAINED_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
	}
	Projection projection;
	if (p_projection) {
		projection = *p_projection;
	} else {
		projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	}
	_render_info = RenderingServerTypes::RenderInfo();
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	for (int i = 0; i < p_count; i++) {
		instances.push_back(p_instances[i]);
	}
	FF_RETAINED_CHECK(p_renderer.render(Transform3D(), projection, instances, &_render_info, FixedFunctionSceneRenderer::FFLightingEnvironment(), nullptr, 0, p_lod) == OK);
	FF_RETAINED_CHECK(p_targets.end_frame(false) == OK);
	r_pixels.resize(size_t(p_context.get_device().get_width()) * p_context.get_device().get_height());
	FF_RETAINED_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	return true;
}

bool TestFixedFunctionRetainedScene::_projection() {
	FixedFunctionSceneRenderer::FFDrawPacket packet;
	packet.view_from_model.origin.z = -3;
	Projection perspective, result;
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
	RenderingServerTypes::SurfaceData surface;
	surface.format |= RSE::ARRAY_FORMAT_VERTEX;
	surface.primitive = RSE::PRIMITIVE_TRIANGLES;
	surface.vertex_count = 3;
	surface.aabb = AABB(Vector3(-0.4, -0.13, 0), Vector3(0.8, 0.23, 0.7));
	FF_RETAINED_CHECK(surface.vertex_data.resize(sizeof(coordinates)) == OK);
	memcpy(surface.vertex_data.ptrw(), coordinates, sizeof(coordinates));
	FF_RETAINED_CHECK(meshes.mesh_add_surface(front_mesh, surface) == OK && _add(meshes, back_mesh));
	FFInstance front(p_context, front_mesh, p_front_material), back(p_context, back_mesh, p_back_material);
	front.transform.origin.z = -0.6;
	back.transform.origin.z = -0.4;
	FFInstance *instances[] = { &front, &back };
	Projection projection;
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

bool TestFixedFunctionRetainedScene::_test_lods(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material) {
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID mesh = meshes.mesh_create();
	RenderingServerTypes::SurfaceData data;
	data.format |= RSE::ARRAY_FORMAT_VERTEX | RSE::ARRAY_FORMAT_INDEX;
	data.primitive = RSE::PRIMITIVE_TRIANGLES;
	data.vertex_count = data.index_count = 6;
	data.aabb = AABB(Vector3(-1.6, -0.6, 0), Vector3(3.2, 1.2, 0));
	const float positions[] = { -1.6f, -0.6f, 0, -1, 0.6f, 0, -0.4f, -0.6f, 0, 0.4f, -0.6f, 0, 1, 0.6f, 0, 1.6f, -0.6f, 0 };
	const uint16_t indices[] = { 0, 1, 2, 3, 4, 5 };
	RenderingServerTypes::SurfaceData::LOD authored;
	authored.edge_length = 0.1f;
	FF_RETAINED_CHECK(data.vertex_data.resize(sizeof(positions)) == OK && data.index_data.resize(sizeof(indices)) == OK && authored.index_data.resize(3 * sizeof(uint16_t)) == OK);
	memcpy(data.vertex_data.ptrw(), positions, sizeof(positions));
	memcpy(data.index_data.ptrw(), indices, sizeof(indices));
	memcpy(authored.index_data.ptrw(), indices + 3, 3 * sizeof(uint16_t));
	FF_RETAINED_CHECK(!data.lods.push_back(authored) && meshes.mesh_add_surface(mesh, data) == OK);
	FixedFunctionMeshStorage::FFSurfaceData snapshot;
	FF_RETAINED_CHECK(meshes.mesh_surface_get_data(mesh, 0, snapshot) == OK);
	FF_RETAINED_CHECK(FixedFunctionSceneRenderer::_select_mesh_lod(snapshot, 1, 0, 1) == 0);
	FF_RETAINED_CHECK(FixedFunctionSceneRenderer::_select_mesh_lod(snapshot, 1, 10, 0.01f) == 1);
	FF_RETAINED_CHECK(FixedFunctionSceneRenderer::_select_mesh_lod(snapshot, 2, 10, 0.01f) == 0);

	FFInstance instance(p_context, mesh, p_material);
	instance.set_lod_bias(1);
	FFInstance *instances[] = { &instance };
	FFDevice &device = static_cast<FFDevice &>(p_context.get_device());
	const int creations = device.get_creations();
	FixedFunctionSceneRenderer::FFMeshLOD lod = { Vector3(), 1, 0.2f, true };
	std::vector<uint32_t> pixels, streamed;
	ProjectSettings *settings = ProjectSettings::get_singleton();
	settings->set("rendering/quality/fixed_function/retained_geometry", false);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, streamed, true, nullptr, &lod));
	settings->set("rendering/quality/fixed_function/retained_geometry", true);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels[48 * 128 + 32] == 0xffff0000);
	const int draws = device.get_retained_draws();
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels, true, nullptr, &lod) && pixels == streamed);
	FF_RETAINED_CHECK(pixels[48 * 128 + 32] == 0xff0000ff && pixels[48 * 128 + 96] == 0xffff0000);
	FF_RETAINED_CHECK(_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 1);
	FF_RETAINED_CHECK(device.get_retained_draws() == draws + 1 && device.get_creations() == creations + 1);
	RID geometry;
	FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
	Vector3 anchor, extent;
	FF_RETAINED_CHECK(p_context.get_geometry_cache().get(snapshot.identity, snapshot.version, 0, geometry, info, anchor, extent) == OK && info.lod_count == 1);
	FF_RETAINED_CHECK(_test_multimesh_lods(p_context, p_targets, p_renderer, mesh, p_material, lod));
	FF_RETAINED_CHECK(meshes.free(mesh) == OK && p_context.prepare_resources() == OK);
	printf("PASS: viewport LOD selection, bias, shared cache entry, streamed equivalence and draw statistics.\n");
	return true;
}

bool TestFixedFunctionRetainedScene::_test_multimesh_lods(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_mesh, RID p_material, const FixedFunctionSceneRenderer::FFMeshLOD &p_lod) {
	FFDevice &device = static_cast<FFDevice &>(p_context.get_device());
	device.set_indexed_triangles_enabled(true);
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID multimesh = meshes.multimesh_allocate();
	meshes.multimesh_initialize(multimesh);
	FF_RETAINED_CHECK(meshes.multimesh_allocate_data(multimesh, 8, false, false, false) == OK && meshes.multimesh_set_mesh(multimesh, p_mesh) == OK);
	const Basis basis = Basis().scaled(Vector3(0.25, 0.25, 0.25));
	for (int i = 0; i < 8; i++) {
		const Vector3 position((i % 4) * real_t(0.875) - real_t(1.3125), i < 4 ? real_t(-0.375) : real_t(0.375), 0);
		FF_RETAINED_CHECK(meshes.multimesh_set_transform(multimesh, i, Transform3D(basis, position)) == OK);
	}
	FFInstance instance(p_context, multimesh, p_material);
	instance.set_lod_bias(1);
	FFInstance *instances[] = { &instance };
	std::vector<uint32_t> base, lower, pixels;
	const int *stats = _render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE];
	const int draws = RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME;
	const int primitives = RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME;
	ProjectSettings *settings = ProjectSettings::get_singleton();
	settings->set("rendering/quality/fixed_function/retained_geometry", false);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, base) && stats[draws] == 8 && stats[primitives] == 16);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, lower, true, nullptr, &p_lod) && stats[draws] == 8 && stats[primitives] == 8 && lower != base);
	settings->set("rendering/quality/fixed_function/retained_geometry", true);
	for (int i = 0; i < 7; i++) {
		FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels, true, nullptr, &p_lod) && pixels == lower);
	}
	FF_RETAINED_CHECK(stats[draws] == 1 && stats[primitives] == 8);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == base && stats[draws] == 8);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == base && stats[draws] == 1 && stats[primitives] == 16);
	const int creations = device.get_creations();
	for (int i = 0; i < 12; i++) {
		FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == base && stats[draws] == 1);
	}
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels, true, nullptr, &p_lod) && pixels == lower && stats[draws] == 1 && stats[primitives] == 8);
	FF_RETAINED_CHECK(device.get_creations() == creations);
	device.set_draw_error(ERR_UNAVAILABLE);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels, true, nullptr, &p_lod) && pixels == lower && stats[draws] == 8 && stats[primitives] == 8);
	device.set_draw_error(OK);
	for (int i = 0; i < 31; i++) {
		FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == base && stats[draws] == 1);
	}
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels, true, nullptr, &p_lod) && pixels == lower && stats[draws] == 8);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels, true, nullptr, &p_lod) && pixels == lower && stats[draws] == 1);
	meshes.multimesh_free(multimesh);
	device.set_indexed_triangles_enabled(false);
	printf("PASS: retained MultiMesh batches preserve authored LOD indices, switching, statistics and fallback.\n");
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
	const String source = "shader_type spatial; render_mode unshaded,cull_disabled; uniform vec4 tint : source_color=vec4(1); void fragment(){ ALBEDO=tint.rgb; }";
	FF_RETAINED_CHECK(materials.shader_set_code(shader, source) == OK && materials.material_set_shader(material, shader) == OK);
	FF_RETAINED_CHECK(materials.material_set_shader(other_material, shader) == OK);
	FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(1, 0, 0)) == OK && materials.material_set_param(other_material, "tint", Color(0, 1, 0)) == OK);
	RID mesh = meshes.mesh_create();
	FF_RETAINED_CHECK(_add(meshes, mesh));
	FixedFunctionMeshStorage::FFSurfaceData snapshot;
	FF_RETAINED_CHECK(meshes.mesh_surface_get_data(mesh, 0, snapshot) == OK);
	FFInstance instance(context, mesh, material), other(context, mesh, other_material);
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
	const uint64_t original_id = native.get_id();
	FF_RETAINED_CHECK(info.vertex_count == 3 && info.primitive_count == 1 && info.draw_calls == 1);
	device.set_draw_error(ERR_UNAVAILABLE);
	const int before_rejection = device.get_transient_draws();
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && pixels == reference);
	FF_RETAINED_CHECK(device.get_transient_draws() == before_rejection + 1);
	device.set_draw_error(OK);
	FF_RETAINED_CHECK(_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] == 1);
	FF_RETAINED_CHECK(_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 1);
	FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(0, 1, 0)) == OK);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && pixels[48 * 128 + 64] == 0xff00ff00);
	FF_RETAINED_CHECK(cache.get(snapshot.identity, snapshot.version, 0, native, info, anchor, extent) == OK && native.get_id() == original_id && device.get_creations() == 1);
	// Same resource, different materials/transforms in one pass.
	instance.transform.origin.x = -1;
	other.transform.origin.x = 1;
	FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(1, 0, 0)) == OK);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 2, pixels) && pixels[48 * 128 + 32] == 0xffff0000 && pixels[48 * 128 + 96] == 0xff00ff00);
	FF_RETAINED_CHECK(device.get_creations() == 1);
	FF_RETAINED_CHECK(_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] == 2);
	// Blend/cutoff changes reuse geometry; alpha still follows the normal 8-bit path.
	instance.transform.origin.x = 0;
	if (device.get_capabilities().alpha_cutoff) {
		FF_RETAINED_CHECK(materials.shader_set_code(shader, source.replace("ALBEDO=tint.rgb;", "ALBEDO=tint.rgb; ALPHA=tint.a; ALPHA_SCISSOR_THRESHOLD=0.5;")) == OK);
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
	Vector<uint8_t> patch;
	FF_RETAINED_CHECK(patch.resize(4) == OK);
	{
		uint8_t *write = patch.ptrw();
		const float x = -0.2f;
		memcpy(write, &x, 4);
	}
	FF_RETAINED_CHECK(meshes.mesh_surface_update_region(mesh, 0, FixedFunctionMeshStorage::BUFFER_VERTEX, 0, patch) == OK);
	FixedFunctionMeshStorage::FFSurfaceData updated;
	FF_RETAINED_CHECK(meshes.mesh_surface_get_data(mesh, 0, updated) == OK && updated.version == snapshot.version + 1);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels, false) && pixels[48 * 128 + 56] == 0xff0000ff);
	FF_RETAINED_CHECK(cache.get_retired_count() == 1 && device.get_creations() == 1);
	FF_RETAINED_CHECK(context.prepare_resources() == OK && cache.get_retired_count() == 0 && device.get_creations() == 2);
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && pixels[48 * 128 + 56] == 0xff0000ff);
	// Removing surface zero must preserve surface one's independent identity/cache.
	FF_RETAINED_CHECK(_add(meshes, mesh) && _draw(context, targets, renderer, instances, 1, pixels));
	FF_RETAINED_CHECK(meshes.mesh_surface_get_data(mesh, 1, snapshot) == OK && cache.get(snapshot.identity, snapshot.version, 0, native, info, anchor, extent) == OK);
	const uint64_t second_id = native.get_id();
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
	instance.data->base = mesh;
	instance._mark_dirty();
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels));
	const int rejected = device.get_creations();
	FF_RETAINED_CHECK(_draw(context, targets, renderer, instances, 1, pixels) && device.get_creations() == rejected);
	FF_RETAINED_CHECK(pixels[48 * 160 + 80] == 0xff00ff00);
	device.set_creation_error(ERR_OUT_OF_MEMORY);
	FF_RETAINED_CHECK(meshes.mesh_surface_update_region(mesh, 0, FixedFunctionMeshStorage::BUFFER_VERTEX, 0, patch) == OK && _draw(context, targets, renderer, instances, 1, pixels));
	FF_RETAINED_CHECK(pixels[48 * 160 + 80] == 0xff00ff00);
	const int exhausted = device.get_creations();
	device.set_creation_error(OK);
	FF_RETAINED_CHECK(meshes.mesh_surface_update_region(mesh, 0, FixedFunctionMeshStorage::BUFFER_VERTEX, 0, patch) == OK && _draw(context, targets, renderer, instances, 1, pixels));
	FF_RETAINED_CHECK(device.get_creations() == exhausted + 1);
	FF_RETAINED_CHECK(meshes.free(mesh) == OK && context.prepare_resources() == OK);
	FF_RETAINED_CHECK(_test_attributes(context, targets, renderer));
	FF_RETAINED_CHECK(_test_spatial_blends(context, targets, renderer));
	FF_RETAINED_CHECK(_test_triangle_topology(context, targets, renderer));
	FF_RETAINED_CHECK(_test_billboards(context, targets, renderer));
	FF_RETAINED_CHECK(_test_points(context, targets, renderer));
	FF_RETAINED_CHECK(_benchmark(context, targets, renderer, material));
	FF_RETAINED_CHECK(context.resize(128, 96) == OK && context.prepare_resources() == OK);
	FF_RETAINED_CHECK(materials.material_set_param(material, "tint", Color(1, 0, 0)) == OK);
	FF_RETAINED_CHECK(materials.shader_set_code(other_shader, source) == OK);
	FF_RETAINED_CHECK(_test_clipped_projection(context, targets, renderer, material, other_material));
	FF_RETAINED_CHECK(_test_lods(context, targets, renderer, material));
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
	const RID multimesh = meshes.multimesh_allocate();
	meshes.multimesh_initialize(multimesh);
	FF_RETAINED_CHECK(meshes.multimesh_allocate_data(multimesh, 9, false, false, false) == OK);
	FF_RETAINED_CHECK(meshes.multimesh_set_mesh(multimesh, mesh) == OK);
	const Basis basis = Basis().scaled(Vector3(0.25, 0.25, 0.25));
	for (int i = 0; i < 8; i++) {
		const Vector3 position(real_t(i % 4) * real_t(0.5) - real_t(0.75), i < 4 ? real_t(-0.25) : real_t(0.25), 0);
		FF_RETAINED_CHECK(meshes.multimesh_set_transform(multimesh, i, Transform3D(basis, position)) == OK);
	}
	// The ninth copy is collapsed, as with unused CPU particle slots.
	FFInstance instance(p_context, multimesh, p_material);
	FFInstance *instances[] = { &instance };
	std::vector<uint32_t> reference, pixels;
	const int index = RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME;
	const int *stats = _render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE];
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, reference) && stats[index] == 8);
	for (int i = 0; i < 7; i++) {
		FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == reference);
	}
	FF_RETAINED_CHECK(stats[index] == 1 && stats[RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 8);
	device.set_draw_error(ERR_UNAVAILABLE);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && stats[index] == 8 && pixels == reference);
	device.set_draw_error(OK);
	FF_RETAINED_CHECK(meshes.multimesh_set_transform(multimesh, 0, Transform3D(basis, Vector3(-1.5, -0.25, 0))) == OK);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && stats[index] == 8 && pixels != reference);
	reference = pixels;
	for (int i = 0; i < 7; i++) {
		FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels == reference);
	}
	FF_RETAINED_CHECK(stats[index] == 1);
	FF_RETAINED_CHECK(meshes.multimesh_set_visible_instances(multimesh, 4) == OK);
	FF_RETAINED_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && stats[index] == 4 && pixels != reference);
	meshes.multimesh_free(multimesh);
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
	const Ref<Image> image = Image::create_empty(2, 1, false, Image::FORMAT_RGBA8);
	image->set_pixel(0, 0, Color(1, 0, 0));
	image->set_pixel(1, 0, Color(0, 1, 0));
	const RID texture = textures.texture_create();
	FF_RETAINED_CHECK(textures.texture_allocate_2d(texture, Size2i(2, 1), Image::FORMAT_RGBA8, 0, 0) == OK && textures.texture_set_data(texture, image) == OK);
	const RID shader = materials.shader_create(), material = materials.material_create();
	FF_RETAINED_CHECK(materials.material_set_shader(material, shader) == OK && materials.material_set_param(material, "image", texture) == OK);
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	Vector<Vector3> positions;
	positions.push_back(Vector3(-0.6, -0.6, 0));
	positions.push_back(Vector3(0, 0.6, 0));
	positions.push_back(Vector3(0.6, -0.6, 0));
	Vector<Vector2> uv1, uv2;
	Vector<int> indices;
	for (int i = 0; i < 3; i++) {
		uv1.push_back(Vector2(0.25, 0.5));
		uv2.push_back(Vector2(0.75, 0.5));
		indices.push_back(i);
	}
	arrays[RSE::ARRAY_VERTEX] = positions;
	arrays[RSE::ARRAY_TEX_UV] = uv1;
	arrays[RSE::ARRAY_TEX_UV2] = uv2;
	arrays[RSE::ARRAY_INDEX] = indices;
	for (int compressed = 0; compressed < 2; compressed++) {
		RenderingServerTypes::SurfaceData surface;
		FF_RETAINED_CHECK(RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&surface, RSE::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), compressed ? uint64_t(RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) : 0) == OK);
		const RID mesh = meshes.mesh_create();
		FF_RETAINED_CHECK(meshes.mesh_add_surface(mesh, surface) == OK);
		FFInstance instance(p_context, mesh, material);
		FFInstance *instances[] = { &instance };
		for (int uv_set = 0; uv_set < 2; uv_set++) {
			const String code = "shader_type spatial; render_mode unshaded,cull_disabled; uniform sampler2D image : filter_nearest,repeat_disable; void fragment(){ ALBEDO=texture(image," + String(uv_set ? "UV2" : "UV") + ").rgb; }";
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
	Vector<uint8_t> vertices, indices;
	FF_RETAINED_CHECK(vertices.resize(17 * 9 * 12) == OK && indices.resize(256 * 3 * 2) == OK);
	{
		uint8_t *vertex = vertices.ptrw();
		for (int y = 0; y < 9; y++) {
			for (int x = 0; x < 17; x++) {
				const float position[] = { x / 8.0f - 1.0f, y / 4.0f - 1.0f, 0 };
				memcpy(vertex + (y * 17 + x) * 12, position, sizeof(position));
			}
		}
		uint8_t *index = indices.ptrw();
		int cursor = 0;
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 16; x++) {
				const uint16_t corners[] = { uint16_t(y * 17 + x), uint16_t((y + 1) * 17 + x), uint16_t(y * 17 + x + 1), uint16_t(y * 17 + x + 1), uint16_t((y + 1) * 17 + x), uint16_t((y + 1) * 17 + x + 1) };
				memcpy(index + cursor, corners, sizeof(corners));
				cursor += sizeof(corners);
			}
		}
	}
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID mesh = meshes.mesh_create();
	RenderingServerTypes::SurfaceData surface;
	surface.format |= RSE::ARRAY_FORMAT_VERTEX | RSE::ARRAY_FORMAT_INDEX;
	surface.primitive = RSE::PRIMITIVE_TRIANGLES;
	surface.vertex_data = vertices;
	surface.index_data = indices;
	surface.vertex_count = 153;
	surface.index_count = 768;
	surface.aabb = AABB(Vector3(-1, -1, 0), Vector3(2, 2, 0));
	FF_RETAINED_CHECK(meshes.mesh_add_surface(mesh, surface) == OK);
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	for (int i = 0; i < 16; i++) {
		instances.push_back(memnew(FFInstance(p_context, mesh, p_material)));
	}
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	std::vector<uint32_t> pixels(64 * 64), reference;
	for (int profile = 0; profile < 2; profile++) {
		ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", profile == 0);
		std::vector<uint64_t> submitted, completed;
		for (int sample = 0; sample < 10; sample++) {
			const uint64_t start = OS::get_singleton()->get_ticks_usec();
			FF_RETAINED_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
			FF_RETAINED_CHECK(p_renderer.render(Transform3D(), projection, instances, &_render_info) == OK && p_targets.end_frame(false) == OK);
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
		printf("PERF3D_MESH4 retained=%d pointer_bits=%u triangles=4096 instances=16 unique_per_mesh=153 samples=9 submit_median_us=%llu complete_median_us=%llu complete_max_us=%llu\n", int(!profile), unsigned(sizeof(void *) * 8), static_cast<unsigned long long>(submitted[4]), static_cast<unsigned long long>(completed[4]), static_cast<unsigned long long>(completed[8]));
	}
	for (uint64_t i = 0; i < instances.size(); i++) {
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
