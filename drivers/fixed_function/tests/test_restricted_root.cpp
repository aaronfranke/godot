#include "test_restricted_root.h"

#include "test_pixel_lighting.h"

#include "core/print_string.h"
#include "core/project_settings.h"
#include "scene/resources/mesh.h"
#include "scene/resources/style_box.h"
#include "servers/register_server_types.h"
#include "servers/visual/visual_server_globals.h"
#include "servers/visual/visual_server_raster.h"

#include <stdio.h>
#include <type_traits>

#define FF_ROOT_CHECK(m_condition)                                              \
	do {                                                                        \
		if (!(m_condition)) {                                                   \
			printf("Restricted root FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                       \
		}                                                                       \
	} while (false)

static_assert(!std::is_abstract<RasterizerFixedFunction>::value && !std::is_abstract<RasterizerStorageFixedFunction>::value, "The restricted renderer must be concrete.");

TestFixedFunctionRestrictedRoot::FFReleaseProbe::FFReleaseProbe(bool *p_storage_alive) :
		_storage_alive(p_storage_alive) {}
TestFixedFunctionRestrictedRoot::FFReleaseProbe::~FFReleaseProbe() {
	*_storage_alive = RasterizerStorage::base_singleton != nullptr;
	if (*_storage_alive) {
		RasterizerStorage::base_singleton->textures_keep_original(false);
	}
}

FixedFunctionGraphicsDevice *TestFixedFunctionRestrictedRoot::FFRoot::_device = nullptr;
void *TestFixedFunctionRestrictedRoot::FFRoot::_window = nullptr;

TestFixedFunctionRestrictedRoot::FFRoot::FFRoot() :
		RasterizerFixedFunction(*_device, _window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false) {}

Rasterizer *TestFixedFunctionRestrictedRoot::FFRoot::_create() { return memnew(FFRoot); }
bool TestFixedFunctionRestrictedRoot::FFRoot::install(FixedFunctionGraphicsDevice &p_device, void *p_window) {
	if (VisualServer::get_singleton() || RasterizerStorage::base_singleton || _device || _create_func) {
		return false;
	}
	_device = &p_device;
	_window = p_window;
	_create_func = _create;
	return true;
}
void TestFixedFunctionRestrictedRoot::FFRoot::uninstall() {
	_create_func = nullptr;
	_device = nullptr;
	_window = nullptr;
}

void TestFixedFunctionRestrictedRoot::FFExpectedErrors::_error(void *p_userdata, const char *p_function, const char *p_file, int p_line, const char *p_error, const char *p_message, ErrorHandlerType p_type) {
	if (p_type == ERR_HANDLER_ERROR) {
		static_cast<FFExpectedErrors *>(p_userdata)->_count++;
	}
}
TestFixedFunctionRestrictedRoot::FFExpectedErrors::FFExpectedErrors() :
		_count(0), _printing(_print_error_enabled) {
	_handler.errfunc = _error;
	_handler.userdata = this;
	add_error_handler(&_handler);
	_print_error_enabled = false;
}
TestFixedFunctionRestrictedRoot::FFExpectedErrors::~FFExpectedErrors() {
	_print_error_enabled = _printing;
	remove_error_handler(&_handler);
}

bool TestFixedFunctionRestrictedRoot::_test_unavailable(RasterizerStorageFixedFunction &p_storage, RID p_texture) {
	FFExpectedErrors errors;
	const RID rejected[] = {
		p_storage.reflection_probe_create(),
		p_storage.gi_probe_create(),
		p_storage.gi_probe_dynamic_data_create(1, 1, 1, RasterizerStorage::GI_PROBE_UNCOMPRESSED),
		p_storage.lightmap_capture_create(),
		p_storage.canvas_light_shadow_buffer_create(128),
		p_storage.canvas_light_occluder_create(),
	};
	for (unsigned int i = 0; i < sizeof(rejected) / sizeof(rejected[0]); i++) {
		FF_ROOT_CHECK(!rejected[i].is_valid());
	}
	FF_ROOT_CHECK(errors.get_count() == 6 && p_storage.get_last_error() == ERR_UNAVAILABLE);
	const RID particles = p_storage.particles_create();
	FF_ROOT_CHECK(!particles.is_valid() && errors.get_count() == 6 && p_storage.get_last_error() == ERR_UNAVAILABLE);
	p_storage.particles_set_emitting(particles, true);
	FF_ROOT_CHECK(errors.get_count() == 6 && p_storage.get_last_error() == ERR_UNAVAILABLE);
	// Allocation must validate ownership before using the requested count.
	p_storage.multimesh_allocate(p_texture, INT32_MAX, VS::MULTIMESH_TRANSFORM_3D, VS::MULTIMESH_COLOR_FLOAT);
	FF_ROOT_CHECK(errors.get_count() == 8 && p_storage.get_last_error() == ERR_INVALID_PARAMETER);
	p_storage.multimesh_set_physics_interpolated(p_texture, true);
	FF_ROOT_CHECK(errors.get_count() == 8);
	p_storage.textures_keep_original(false);
	FF_ROOT_CHECK(p_storage.get_last_error() == OK);
	FF_ROOT_CHECK(p_storage.mesh_get_surface_count(p_texture) == 0 && p_storage.get_last_error() == OK);
	FF_ROOT_CHECK(!p_storage.lightmap_capture_get_octree_ptr(p_texture));
	FF_ROOT_CHECK(p_storage.particles_is_inactive(particles) && !p_storage.particles_get_emitting(particles));
	FF_ROOT_CHECK(errors.get_count() == 9 && p_storage.texture_get_width(p_texture) == 2);
	FF_ROOT_CHECK(p_storage.get_base_type(p_texture) == VS::INSTANCE_NONE);
	return true;
}

bool TestFixedFunctionRestrictedRoot::_test_meshes(VisualServer &p_server, FixedFunctionRenderContext &p_context) {
	Ref<ArrayMesh> mesh;
	mesh.instance();
	PoolVector<Vector3> vertices;
	vertices.push_back(Vector3(-1, -1, -1));
	vertices.push_back(Vector3(1, -1, -1));
	vertices.push_back(Vector3(0, 1, 1));
	Array arrays;
	arrays.resize(VS::ARRAY_MAX);
	arrays[VS::ARRAY_VERTEX] = vertices;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	FixedFunctionMeshStorage &storage = p_context.get_mesh_storage();
	FF_ROOT_CHECK(storage.owns_mesh(mesh->get_rid()) && storage.mesh_get_surface_count(mesh->get_rid()) == 1);
	const Array returned = mesh->surface_get_arrays(0);
	const PoolVector<Vector3> roundtrip = returned[VS::ARRAY_VERTEX];
	FF_ROOT_CHECK(roundtrip.size() == 3 && roundtrip.read()[2] == vertices.read()[2]);
	const RID scenario = p_server.scenario_create();
	const RID instance = p_server.instance_create();
	p_server.instance_set_base(instance, mesh->get_rid());
	p_server.instance_set_scenario(instance, scenario);
	p_server.instance_attach_object_instance_id(instance, mesh->get_instance_id());
	RasterizerStorage::base_singleton->update_dirty_resources();
	const AABB origin(Vector3(-2, -2, -2), Vector3(4, 4, 4));
	FF_ROOT_CHECK(p_server.instances_cull_aabb(origin, scenario).size() == 1);
	const AABB distant(Vector3(20, 20, 20), Vector3(2, 2, 2));
	mesh->set_custom_aabb(distant);
	RasterizerStorage::base_singleton->update_dirty_resources();
	FF_ROOT_CHECK(p_server.instances_cull_aabb(origin, scenario).empty() && p_server.instances_cull_aabb(distant, scenario).size() == 1);
	// Releasing the actual resource detaches its live server instance before RID deletion.
	mesh.unref();
	FF_ROOT_CHECK(storage.get_mesh_count() == 0 && p_server.instances_cull_aabb(distant, scenario).empty());
	p_server.free(instance);
	p_server.free(scenario);
	printf("PASS: ArrayMesh packing/readback, scene bounds invalidation and live-instance detachment.\n");
	return true;
}

bool TestFixedFunctionRestrictedRoot::_test_unlit_scene(VisualServer &p_server, FFRoot &p_root, FixedFunctionGraphicsDevice &p_device) {
	const RID viewport = p_server.viewport_create();
	const RID scenario = p_server.scenario_create();
	const RID camera = p_server.camera_create();
	const RID mesh = p_server.mesh_create();
	const RID material = p_server.material_create();
	const RID shader = p_server.shader_create();
	const RID instance = p_server.instance_create();
	const RID light = p_server.directional_light_create();
	const RID light_instance = p_server.instance_create2(light, scenario);
	FF_ROOT_CHECK(light.is_valid() && light_instance.is_valid());
	p_server.light_set_shadow(light, true);
	p_server.light_set_color(light, Color(0, 1, 0));
	p_server.light_set_param(light, VS::LIGHT_PARAM_ENERGY, 3);
	PoolVector<Vector3> vertices;
	vertices.push_back(Vector3(-1, -1, -3));
	vertices.push_back(Vector3(0, 1, -3));
	vertices.push_back(Vector3(1, -1, -3));
	Array arrays;
	arrays.resize(VS::ARRAY_MAX);
	arrays[VS::ARRAY_VERTEX] = vertices;
	p_server.mesh_add_surface_from_arrays(mesh, VS::PRIMITIVE_TRIANGLES, arrays);
	p_server.shader_set_code(shader, "shader_type spatial; render_mode unshaded; void fragment(){ALBEDO=vec3(1.0,0.0,0.0);}");
	p_server.material_set_shader(material, shader);
	p_server.mesh_surface_set_material(mesh, 0, material);
	p_server.instance_set_base(instance, mesh);
	p_server.instance_set_scenario(instance, scenario);
	p_server.camera_set_perspective(camera, 60, 0.1, 20);
	p_server.viewport_set_size(viewport, CAPTURE_WIDTH, CAPTURE_HEIGHT);
	p_server.viewport_set_usage(viewport, VS::VIEWPORT_USAGE_3D_NO_EFFECTS);
	p_server.viewport_set_scenario(viewport, scenario);
	p_server.viewport_attach_camera(viewport, camera);
	p_server.viewport_set_active(viewport, true);
	p_server.viewport_set_update_mode(viewport, VS::VIEWPORT_UPDATE_ALWAYS);
	p_server.viewport_attach_to_screen(viewport, Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT));
	p_server.set_default_clear_color(Color(0, 0, 1));
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK);
	std::vector<uint32_t> pixels(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FF_ROOT_CHECK(p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[48 * CAPTURE_WIDTH + 64] == 0xffff0000 && pixels[0] == 0xff0000ff);
	FF_ROOT_CHECK(p_server.viewport_get_render_info(viewport, VS::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME) == 1 && p_server.viewport_get_render_info(viewport, VS::VIEWPORT_RENDER_INFO_VERTICES_IN_FRAME) == 3 && p_server.viewport_get_render_info(viewport, VS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME) == 1);
	FF_ROOT_CHECK(p_root.get_context().get_light_storage().get_light_count() == 1 && p_root.get_context().get_light_storage().get_instance_count() == 1);
	// Exercise light culling, environment state and live shading through the real frontend.
	const RID environment = p_server.environment_create();
	p_server.environment_set_ambient_light(environment, Color(0.25, 0.25, 0.25), 1, 0);
	p_server.scenario_set_environment(scenario, environment);
	p_server.light_set_color(light, Color(1, 1, 1));
	p_server.light_set_param(light, VS::LIGHT_PARAM_ENERGY, 0.5);
	p_server.shader_set_code(shader, "shader_type spatial; void fragment(){ ALBEDO=vec3(1); }");
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && ABS(int(pixels[48 * CAPTURE_WIDTH + 64] & 255) - 191) <= 3);
	p_server.light_set_cull_mask(light, 2);
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && ABS(int(pixels[48 * CAPTURE_WIDTH + 64] & 255) - 64) <= 3);
	p_server.light_set_cull_mask(light, 1);
	p_server.light_set_param(light, VS::LIGHT_PARAM_ENERGY, 0.25);
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && ABS(int(pixels[48 * CAPTURE_WIDTH + 64] & 255) - 128) <= 3);
	p_server.light_set_param(light, VS::LIGHT_PARAM_ENERGY, 0);
	p_server.environment_set_ambient_light(environment, Color(0, 0, 0), 1, 0);
	for (int spot = 0; spot < 2; spot++) {
		const RID local = spot ? p_server.spot_light_create() : p_server.omni_light_create();
		const RID local_instance = p_server.instance_create2(local, scenario);
		p_server.light_set_color(local, Color(0, 0, 1));
		p_server.light_set_param(local, VS::LIGHT_PARAM_RANGE, 6);
		p_server.light_set_param(local, VS::LIGHT_PARAM_ATTENUATION, 1);
		p_server.light_set_param(local, VS::LIGHT_PARAM_SPOT_ANGLE, 45);
		p_server.light_set_param(local, VS::LIGHT_PARAM_SPOT_ATTENUATION, 1);
		p_server.draw(false, 1.0 / 60.0);
		const bool per_pixel = p_device.get_max_per_pixel_lights() && bool(GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting"));
		int expected = 128; // Pixel evaluation versus Gouraud interpolation.
		if (!per_pixel) {
			expected = spot ? 82 : 109;
		}
		FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && ABS(int(pixels[48 * CAPTURE_WIDTH + 64] & 255) - expected) <= 3 && (pixels[48 * CAPTURE_WIDTH + 64] & 0xffff00) == 0);
		p_server.light_set_cull_mask(local, 2);
		p_server.draw(false, 1.0 / 60.0);
		FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[48 * CAPTURE_WIDTH + 64] == 0xff000000);
		p_server.light_set_cull_mask(local, 1);
		p_server.instance_set_transform(local_instance, spot ? Transform(Basis(Vector3(0, 1, 0), Math_PI), Vector3()) : Transform(Basis(), Vector3(0, 0, 4)));
		p_server.draw(false, 1.0 / 60.0);
		FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[48 * CAPTURE_WIDTH + 64] == 0xff000000);
		p_server.instance_set_transform(local_instance, Transform());
		p_server.viewport_set_debug_draw(viewport, VS::VIEWPORT_DEBUG_DRAW_UNSHADED);
		p_server.draw(false, 1.0 / 60.0);
		FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[48 * CAPTURE_WIDTH + 64] == 0xffffffff);
		p_server.viewport_set_debug_draw(viewport, VS::VIEWPORT_DEBUG_DRAW_DISABLED);
		p_server.free(local);
		p_server.draw(false, 1.0 / 60.0);
		FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[48 * CAPTURE_WIDTH + 64] == 0xff000000);
		p_server.free(local_instance);
	}
	printf("PASS: real VisualServer omni/spot Gouraud pixels, masks, moved/rotated lights, unshaded bypass and source release.\n");
	p_server.shader_set_code(shader, "shader_type spatial; render_mode unshaded; uniform float cutoff=0.5; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.25; ALPHA_SCISSOR=cutoff; }");
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[48 * CAPTURE_WIDTH + 64] == 0xff0000ff);
	p_server.material_set_param(material, "cutoff", 0.2);
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[48 * CAPTURE_WIDTH + 64] == 0xffff0000);
	p_server.shader_set_code(shader, "shader_type spatial; void fragment(){ ALBEDO=vec3(sin(UV.x)); }");
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[48 * CAPTURE_WIDTH + 64] == 0xffff00ff);
	p_server.mesh_surface_set_material(mesh, 0, RID());
	p_server.viewport_set_debug_draw(viewport, VS::VIEWPORT_DEBUG_DRAW_UNSHADED);
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[48 * CAPTURE_WIDTH + 64] == 0xffffffff);
	const RID line_mesh = p_server.mesh_create();
	const RID line_shader = p_server.shader_create();
	const RID line_material = p_server.material_create();
	PoolVector<Vector3> line_vertices;
	line_vertices.push_back(Vector3(-1, 0.75, -2));
	line_vertices.push_back(Vector3(1, 0.75, -2));
	Array line_arrays;
	line_arrays.resize(VS::ARRAY_MAX);
	line_arrays[VS::ARRAY_VERTEX] = line_vertices;
	p_server.mesh_add_surface_from_arrays(line_mesh, VS::PRIMITIVE_LINES, line_arrays);
	p_server.shader_set_code(line_shader, "shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=vec3(1,0,0); }");
	p_server.material_set_shader(line_material, line_shader);
	p_server.material_set_line_width(line_material, 5);
	p_server.mesh_surface_set_material(line_mesh, 0, line_material);
	const RID line_instance = p_server.instance_create2(line_mesh, scenario);
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[17 * CAPTURE_WIDTH + 64] == 0xffff0000);
	const int line_vertices_drawn = GLOBAL_GET("rendering/quality/fixed_function/antialiased_lines") && p_device.get_capabilities().blend_mix ? 30 : 6;
	FF_ROOT_CHECK(p_server.viewport_get_render_info(viewport, VS::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME) == 2 && p_server.viewport_get_render_info(viewport, VS::VIEWPORT_RENDER_INFO_VERTICES_IN_FRAME) == 3 + line_vertices_drawn && p_server.viewport_get_render_info(viewport, VS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME) == 2);
	p_server.free(line_instance);
	p_server.free(line_mesh);
	p_server.free(line_material);
	p_server.free(line_shader);
	// A live-instance mesh release reaches the real scene's empty-camera path on the next draw.
	p_server.free(mesh);
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(pixels.data(), pixels.size()) == OK && pixels[48 * CAPTURE_WIDTH + 64] == 0xff0000ff);
	p_server.free(light); // Detach a live frontend light instance before releasing it.
	FF_ROOT_CHECK(p_root.get_context().get_light_storage().get_instance_count() == 0);
	p_server.free(light_instance);
	p_server.free(instance);
	p_server.free(viewport);
	p_server.free(camera);
	p_server.free(scenario);
	p_server.free(material);
	p_server.free(shader);
	p_server.free(environment);
	FF_ROOT_CHECK(p_root.get_context().prepare_resources() == OK);
	FixedFunctionRenderTargetManager targets(p_root.get_context());
	FixedFunctionSceneRenderer renderer(p_root.get_context(), targets);
	FF_ROOT_CHECK(TestFixedFunctionPixelLighting::test(p_root.get_context(), targets, renderer));
	printf("PASS: real VisualServer camera/mesh/material draws, depth, viewport counters and live release pixels.\n");
	return true;
}

bool TestFixedFunctionRestrictedRoot::_draw(VisualServer &p_server, FFRoot &p_root, FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FF_ROOT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			uint32_t expected = 0xffff0000;
			if (x >= 64) {
				expected = y < 48 ? 0xff00ff00 : 0xff204080;
			}
			const uint32_t actual = r_pixels[size_t(y) * CAPTURE_WIDTH + x];
			FF_ROOT_CHECK((actual >> 24) == 255);
			for (int shift = 0; shift < 24; shift += 8) {
				const int delta = int((actual >> shift) & 255) - int((expected >> shift) & 255);
				FF_ROOT_CHECK(delta >= -8 && delta <= 8);
			}
		}
	}
	return true;
}

bool TestFixedFunctionRestrictedRoot::_test_gui(VisualServer &p_server, FFRoot &p_root, FixedFunctionGraphicsDevice &p_device, RID p_canvas, std::vector<uint32_t> &r_pixels) {
	const RID panel = p_server.canvas_item_create();
	p_server.canvas_item_set_parent(panel, p_canvas);
	Ref<StyleBoxFlat> style;
	style.instance();
	style->set_bg_color(Color(0, 1, 0));
	style->set_border_color(Color(0, 0, 1));
	style->set_border_width_all(4);
	style->set_border_blend(false);
	style->set_corner_radius_all(12);
	style->set_anti_aliased(true);
	// Exercise the real StyleBox tessellator, including its coverage-alpha fringe.
	style->draw(panel, Rect2(8, 8, 48, 48));

	const RID outer = p_server.canvas_item_create();
	p_server.canvas_item_set_parent(outer, p_canvas);
	p_server.canvas_item_set_transform(outer, Transform2D(0, Vector2(72, 8)));
	p_server.canvas_item_add_rect(outer, Rect2(0, 0, 40, 40), Color(0, 0, 0));
	p_server.canvas_item_set_clip(outer, true);
	const RID inner = p_server.canvas_item_create();
	p_server.canvas_item_set_parent(inner, outer);
	p_server.canvas_item_set_transform(inner, Transform2D(0, Vector2(12, 12)));
	p_server.canvas_item_add_rect(inner, Rect2(0, 0, 32, 32), Color(0, 0, 0));
	p_server.canvas_item_set_clip(inner, true);
	const RID contents = p_server.canvas_item_create();
	p_server.canvas_item_set_parent(contents, inner);
	p_server.canvas_item_add_rect(contents, Rect2(-20, -20, 96, 96), Color(1, 1, 0));
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FF_ROOT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FF_ROOT_CHECK(r_pixels[32 * CAPTURE_WIDTH + 32] == 0xff00ff00); // Flat infill.
	FF_ROOT_CHECK(r_pixels[32 * CAPTURE_WIDTH + 9] == 0xff0000ff); // Border.
	FF_ROOT_CHECK(r_pixels[8 * CAPTURE_WIDTH + 8] == 0xffff0000); // Rounded corner exposes background.
	bool coverage_found = false;
	for (int y = 8; y < 20; y++) {
		for (int x = 8; x < 20; x++) {
			const uint32_t color = r_pixels[y * CAPTURE_WIDTH + x];
			const int red = (color >> 16) & 255;
			const int blue = color & 255;
			coverage_found |= (color >> 24) == 255 && (color & 0xff00) == 0 && red > 8 && red < 247 && blue > 8 && blue < 247;
		}
	}
	FF_ROOT_CHECK(coverage_found);
	FF_ROOT_CHECK(r_pixels[24 * CAPTURE_WIDTH + 68] == 0xff00ff00); // Outside outer clip.
	FF_ROOT_CHECK(r_pixels[24 * CAPTURE_WIDTH + 80] == 0xff000000); // Inside outer, outside inner.
	FF_ROOT_CHECK(r_pixels[24 * CAPTURE_WIDTH + 90] == 0xffffff00); // Both clips.
	FF_ROOT_CHECK(r_pixels[46 * CAPTURE_WIDTH + 110] == 0xffffff00);
	FF_ROOT_CHECK(r_pixels[46 * CAPTURE_WIDTH + 114] == 0xff00ff00); // Inner must intersect outer.
	p_server.free(contents);
	p_server.free(inner);
	p_server.free(outer);
	p_server.free(panel);
	printf("PASS: real flat StyleBox geometry, coverage-alpha pixels, translated nested clips and clip intersection.\n");
	return true;
}

bool TestFixedFunctionRestrictedRoot::_test_sub_viewports(VisualServer &p_server, FFRoot &p_root, FixedFunctionGraphicsDevice &p_device, RID p_parent_viewport, RID p_parent_canvas, RID p_scenario, std::vector<uint32_t> &r_pixels) {
	RID viewports[2];
	RID canvases[2];
	RID backgrounds[2];
	for (int i = 0; i < 2; i++) {
		viewports[i] = p_server.viewport_create();
		canvases[i] = p_server.canvas_create();
		backgrounds[i] = p_server.canvas_item_create();
		p_server.viewport_set_parent_viewport(viewports[i], i == 0 ? p_parent_viewport : viewports[0]);
		p_server.viewport_set_size(viewports[i], i == 0 ? 32 : 16, i == 0 ? 24 : 12);
		p_server.viewport_set_usage(viewports[i], VS::VIEWPORT_USAGE_2D);
		p_server.viewport_set_scenario(viewports[i], p_scenario);
		p_server.viewport_set_active(viewports[i], true);
		p_server.viewport_set_update_mode(viewports[i], VS::VIEWPORT_UPDATE_WHEN_VISIBLE);
		p_server.viewport_attach_canvas(viewports[i], canvases[i]);
		p_server.canvas_item_set_parent(backgrounds[i], canvases[i]);
		p_server.canvas_item_add_rect(backgrounds[i], Rect2(0, 0, 32, 24), i == 0 ? Color(0, 0, 1) : Color(1, 1, 0));
	}
	const RID child_texture = p_server.viewport_get_texture(viewports[0]);
	const RID grandchild_texture = p_server.viewport_get_texture(viewports[1]);
	const RID foreground = p_server.canvas_item_create();
	p_server.canvas_item_set_parent(foreground, canvases[0]);
	p_server.canvas_item_add_rect(foreground, Rect2(0, 0, 16, 24), Color(1, 0, 0));
	const RID nested = p_server.canvas_item_create();
	p_server.canvas_item_set_parent(nested, canvases[0]);
	p_server.canvas_item_add_texture_rect(nested, Rect2(16, 0, 16, -12), grandchild_texture, false, Color(1, 1, 1));
	const RID sample = p_server.canvas_item_create();
	p_server.canvas_item_set_parent(sample, p_parent_canvas);
	p_server.canvas_item_set_z_index(sample, 10);
	// Match ViewportContainer's negative-height draw, including its implicit V flip.
	p_server.canvas_item_add_texture_rect(sample, Rect2(0, 0, CAPTURE_WIDTH, -CAPTURE_HEIGHT), child_texture, false, Color(1, 1, 1));
	// WHEN_VISIBLE targets start unrendered. Sampling must schedule them without cancelling the parent.
	for (int i = 0; i < 3; i++) {
		p_server.draw(false, 1.0 / 60.0);
		FF_ROOT_CHECK(p_root.get_last_error() == OK);
	}
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FF_ROOT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FF_ROOT_CHECK(r_pixels[24 * CAPTURE_WIDTH + 32] == 0xffff0000);
	FF_ROOT_CHECK(r_pixels[24 * CAPTURE_WIDTH + 96] == 0xffffff00);
	FF_ROOT_CHECK(r_pixels[72 * CAPTURE_WIDTH + 96] == 0xff0000ff);
	// A raw texture consumer uses positive height with render_target_v_flip enabled.
	p_server.canvas_item_clear(sample);
	p_server.canvas_item_add_texture_rect(sample, Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT), child_texture, false, Color(1, 1, 1));
	p_server.viewport_set_vflip(viewports[0], true);
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FF_ROOT_CHECK(r_pixels[24 * CAPTURE_WIDTH + 96] == 0xffffff00 && r_pixels[72 * CAPTURE_WIDTH + 96] == 0xff0000ff);
	// ONCE becomes DISABLED. CPU scene edits should not change the completed frozen texture.
	p_server.viewport_set_update_mode(viewports[0], VS::VIEWPORT_UPDATE_ONCE);
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK);
	p_server.canvas_item_clear(foreground);
	p_server.canvas_item_add_rect(foreground, Rect2(0, 0, 16, 24), Color(0, 1, 0));
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FF_ROOT_CHECK(r_pixels[24 * CAPTURE_WIDTH + 32] == 0xffff0000);
	p_server.viewport_set_update_mode(viewports[0], VS::VIEWPORT_UPDATE_ONCE);
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FF_ROOT_CHECK(r_pixels[24 * CAPTURE_WIDTH + 32] == 0xff00ff00);
	// Size changes preserve the public texture RID and redraw the same content at its new size.
	p_server.viewport_set_size(viewports[0], 64, 48);
	p_server.canvas_item_clear(backgrounds[0]);
	p_server.canvas_item_add_rect(backgrounds[0], Rect2(0, 0, 64, 48), Color(0, 0, 1));
	p_server.canvas_item_clear(foreground);
	p_server.canvas_item_add_rect(foreground, Rect2(0, 0, 32, 48), Color(0, 1, 0));
	p_server.canvas_item_clear(nested);
	p_server.canvas_item_add_texture_rect(nested, Rect2(32, 0, 32, -24), grandchild_texture, false, Color(1, 1, 1));
	p_server.viewport_set_update_mode(viewports[0], VS::VIEWPORT_UPDATE_ONCE);
	p_server.draw(false, 1.0 / 60.0);
	FF_ROOT_CHECK(p_root.get_last_error() == OK && p_server.viewport_get_texture(viewports[0]) == child_texture);
	FF_ROOT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FF_ROOT_CHECK(r_pixels[24 * CAPTURE_WIDTH + 32] == 0xff00ff00 && r_pixels[24 * CAPTURE_WIDTH + 96] == 0xffffff00);
	// Loss of a frozen target schedules a single redraw from the latest scene.
	FF_ROOT_CHECK(p_root.resize(160, 120) == OK && p_root.get_context().prepare_resources() == OK);
	FF_ROOT_CHECK(p_root.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	for (int i = 0; i < 3; i++) {
		p_server.draw(false, 1.0 / 60.0);
		FF_ROOT_CHECK(p_root.get_last_error() == OK);
	}
	FF_ROOT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FF_ROOT_CHECK(r_pixels[24 * CAPTURE_WIDTH + 32] == 0xff00ff00 && r_pixels[24 * CAPTURE_WIDTH + 96] == 0xffffff00);
	p_server.free(sample);
	p_server.free(nested);
	p_server.free(foreground);
	for (int i = 1; i >= 0; i--) {
		p_server.free(backgrounds[i]);
		p_server.free(canvases[i]);
		p_server.free(viewports[i]);
	}
	printf("PASS: nested WHEN_VISIBLE viewports, first-use scheduling, orientation, frozen/explicit redraw, stable texture resize and loss recovery.\n");
	return true;
}

bool TestFixedFunctionRestrictedRoot::_run(VisualServer &p_server, FFRoot &p_root, FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
	RasterizerStorageFixedFunction &storage = *static_cast<RasterizerStorageFixedFunction *>(p_root.get_storage());
	FixedFunctionRenderContext &context = p_root.get_context();
	FF_ROOT_CHECK(RasterizerStorage::base_singleton == &storage && VSG::storage == &storage && p_root.is_initialized());
	FF_ROOT_CHECK(_test_meshes(p_server, context));
	FF_ROOT_CHECK(_test_unlit_scene(p_server, p_root, p_device));
	// Real frontend owners route through the actual renderer's storage/scene interfaces.
	const RID viewport = p_server.viewport_create();
	const RID scenario = p_server.scenario_create();
	const RID canvas = p_server.canvas_create();
	const RID item = p_server.canvas_item_create();
	const RID sprite = p_server.canvas_item_create();
	const RID texture = p_server.texture_create();
	PoolVector<uint8_t> bytes;
	bytes.resize(16);
	{
		PoolVector<uint8_t>::Write write = bytes.write();
		for (int i = 0; i < 16; i++) {
			write[i] = 255;
		}
	}
	Ref<Image> image = memnew(Image(2, 2, false, Image::FORMAT_RGBA8, bytes));
	p_server.texture_allocate(texture, 2, 2, 0, Image::FORMAT_RGBA8, VS::TEXTURE_TYPE_2D, 0);
	p_server.texture_set_data(texture, image);
	FF_ROOT_CHECK(storage.get_last_error() == OK && _test_unavailable(storage, texture));
	FF_ROOT_CHECK(context.get_texture_storage().get_texture_count() == 2 && context.get_texture_storage().get_render_target_count() == 1);
	p_server.viewport_set_size(viewport, CAPTURE_WIDTH, CAPTURE_HEIGHT);
	p_server.viewport_set_usage(viewport, VS::VIEWPORT_USAGE_2D);
	p_server.viewport_set_scenario(viewport, scenario);
	p_server.viewport_set_active(viewport, true);
	p_server.viewport_set_update_mode(viewport, VS::VIEWPORT_UPDATE_ALWAYS);
	p_server.viewport_attach_to_screen(viewport, Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT));
	p_server.viewport_attach_canvas(viewport, canvas);
	p_server.canvas_item_set_parent(item, canvas);
	p_server.canvas_item_set_parent(sprite, canvas);
	p_server.canvas_item_add_rect(item, Rect2(0, 0, 64, 96), Color(1, 0, 0));
	p_server.canvas_item_add_texture_rect(sprite, Rect2(64, 0, 64, 48), texture, false, Color(0, 1, 0));
	p_server.set_default_clear_color(Color(0.125, 0.25, 0.5));
	FF_ROOT_CHECK(_draw(p_server, p_root, p_device, r_pixels));
	FF_ROOT_CHECK(p_server.viewport_get_render_info(viewport, VS::VIEWPORT_RENDER_INFO_2D_ITEMS_IN_FRAME) == 2);
	FF_ROOT_CHECK(p_server.viewport_get_render_info(viewport, VS::VIEWPORT_RENDER_INFO_2D_DRAW_CALLS_IN_FRAME) == 2);
	const std::vector<uint32_t> initial = r_pixels;
	FF_ROOT_CHECK(_test_gui(p_server, p_root, p_device, canvas, r_pixels));
	FF_ROOT_CHECK(_test_sub_viewports(p_server, p_root, p_device, viewport, canvas, scenario, r_pixels));
	FF_ROOT_CHECK(_draw(p_server, p_root, p_device, r_pixels) && r_pixels == initial);
	{
		FFExpectedErrors errors;
		p_root.begin_frame(0);
		p_root.clear_render_target(Color(1, 1, 0));
		p_root.restore_render_target(false);
		FF_ROOT_CHECK(p_root.get_last_error() == OK);
		p_root.output_lens_distorted_to_screen(RID(), Rect2(), 0, 0, Vector2(), 1);
		p_root.end_frame(false);
		FF_ROOT_CHECK(p_root.get_last_error() == ERR_UNAVAILABLE && errors.get_count() == 2);
		FF_ROOT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) != OK);
	}
	FF_ROOT_CHECK(_draw(p_server, p_root, p_device, r_pixels) && r_pixels == initial);
	FF_ROOT_CHECK(p_root.resize(160, 120) == OK && context.prepare_resources() == OK && p_root.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FF_ROOT_CHECK(_draw(p_server, p_root, p_device, r_pixels) && r_pixels == initial);
	p_server.free(item);
	p_server.free(sprite);
	p_server.free(canvas);
	p_server.free(viewport);
	p_server.free(scenario);
	p_server.free(texture);
	FF_ROOT_CHECK(context.get_scene_storage().get_environment_count() == 0 && context.get_scene_storage().get_shadow_atlas_count() == 0 && context.get_scene_storage().get_reflection_atlas_count() == 0);
	// The last reference releases during context finalization, with the real server alive.
	Ref<ArrayMesh> retained;
	retained.instance();
	const RID retaining_material = p_server.material_create();
	p_server.material_set_param(retaining_material, "retained_mesh", retained);
	retained.unref();
	FF_ROOT_CHECK(context.get_mesh_storage().get_mesh_count() == 1);
	return true;
}

bool TestFixedFunctionRestrictedRoot::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	FF_ROOT_CHECK(FFRoot::install(p_device, p_window));
	VisualServerRaster *server = memnew(VisualServerRaster);
	FFRoot *root = static_cast<FFRoot *>(VSG::rasterizer);
	server->init();
	printf("Fixed Function per-pixel light limit: %u\n", p_device.get_max_per_pixel_lights());
	// Main registers the server types/OS feature callback only after constructing
	// the server: installing that callback earlier can call a partially built VisualServer.
	register_server_types();
	const bool success = _run(*server, *root, p_device, r_pixels);
	unregister_server_types();
	server->finish();
	const bool finalized = !root->is_initialized() && p_device.get_texture_memory_usage() == 0;
	memdelete(server);
	// The fixture owns this server and its globals; there is no renderer to restore.
	VSG::rasterizer = nullptr;
	VSG::storage = nullptr;
	VSG::canvas = nullptr;
	VSG::viewport = nullptr;
	VSG::scene = nullptr;
	VSG::canvas_render = nullptr;
	VSG::scene_render = nullptr;
	FF_ROOT_CHECK(success && finalized && !VisualServer::get_singleton() && !RasterizerStorage::base_singleton);
	FF_ROOT_CHECK(_test_teardown(p_device, p_window));
	printf("PASS: concrete root/storage own the real VisualServer, unsupported resource families reject without handles, frontend viewport/canvas/texture pixels, lens cancellation, recovery and singleton cleanup.\n");
	return true;
}

bool TestFixedFunctionRestrictedRoot::_test_teardown(FixedFunctionGraphicsDevice &p_device, void *p_window) {
	for (int initialized = 1; initialized >= 0; initialized--) {
		FFRoot::uninstall();
		FF_ROOT_CHECK(FFRoot::install(p_device, initialized ? p_window : nullptr));
		FFRoot *root = static_cast<FFRoot *>(Rasterizer::create());
		RasterizerStorage *storage = root->get_storage();
		bool alive_during_release = false;
		Ref<Reference> release = memnew(FFReleaseProbe(&alive_during_release));
		const RID material = storage->material_create();
		storage->material_set_param(material, "release_probe", release);
		release.unref();
		if (initialized) {
			root->initialize();
			FF_ROOT_CHECK(root->is_initialized());
		} else {
			FFExpectedErrors errors;
			root->initialize();
			FF_ROOT_CHECK(!root->is_initialized() && root->get_last_error() == ERR_INVALID_PARAMETER && errors.get_count() == 1);
		}
		// No explicit finalize: payloads must release before the storage facade is destroyed.
		memdelete(root);
		FF_ROOT_CHECK(alive_during_release && !RasterizerStorage::base_singleton && p_device.get_texture_memory_usage() == 0);
	}
	FFRoot::uninstall();
	return true;
}

#undef FF_ROOT_CHECK
