#include "test_storage_integration.h"

#include "core/core_globals.h"
#include "drivers/fixed_function/renderer_material_storage_fixed_function.h"
#include "drivers/fixed_function/renderer_scene_render_fixed_function.h"
#include "drivers/fixed_function/renderer_texture_storage_fixed_function.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering/storage/camera_attributes_storage.h"

#include <stdio.h>
#include <string.h>

#define FF_STORAGE_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Storage integration FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

TestFixedFunctionStorageIntegration::FFDependencyState::FFDependencyState() :
		storage(nullptr), changed(0), deleted(0), free_on_change(false), clear_on_change(false), error(OK) {}

void TestFixedFunctionStorageIntegration::_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
	FFDependencyState *state = static_cast<FFDependencyState *>(p_tracker->userdata);
	state->changed++;
	if (state->clear_on_change) {
		state->storage->clear();
	}
	if (state->free_on_change) {
		state->error = state->storage->free(state->material);
	}
}

void TestFixedFunctionStorageIntegration::_deleted(const RID &p_rid, DependencyTracker *p_tracker) {
	FFDependencyState *state = static_cast<FFDependencyState *>(p_tracker->userdata);
	state->deleted++;
}

bool TestFixedFunctionStorageIntegration::_test_feature_table(FixedFunctionMaterialStorage &p_storage) {
	const RID shader = p_storage.shader_create();
	const RID material = p_storage.material_create();
	FF_STORAGE_CHECK(p_storage.material_set_shader(material, shader) == OK);
	const struct FFCase {
		const char *code;
		bool animated;
		bool shadows;
		bool tangents;
		bool normals;
	} cases[] = {
		{ "shader_type spatial;", false, true, false, false },
		{ "shader_type spatial; render_mode blend_add;", false, false, false, false },
		{ "shader_type spatial; void fragment() { ALPHA = 0.5; }", false, false, false, false },
		{ "shader_type spatial; void fragment() { ALPHA = 0.5; ALPHA_SCISSOR_THRESHOLD = 0.25; }", false, true, false, false },
		{ "shader_type spatial; render_mode depth_prepass_alpha; void fragment() { ALPHA = 0.5; }", false, true, false, false },
		{ "shader_type spatial; void vertex() { VERTEX.x += sin(TIME); }", true, true, false, false },
		{ "shader_type spatial; float phase() { return TIME; } void vertex() { VERTEX.x += phase(); }", true, true, false, false },
		{ "shader_type spatial; float phase(float v) { return 0.0; } float phase(vec2 v) { return TIME; } void vertex() { VERTEX.x += phase(vec2(0.0)); }", true, true, false, false },
		{ "shader_type spatial; float phase(float v) { return TIME; } float phase(vec2 v) { return 0.0; } void vertex() { VERTEX.x += phase(vec2(0.0)); }", false, true, false, false },
		{ "shader_type spatial; void warp(inout vec3 p) { p.x += TIME; } void vertex() { warp(VERTEX); }", true, true, false, false },
		{ "shader_type spatial; void warp(inout vec3 p) { vec3 f = modf(vec3(TIME), p); } void vertex() { warp(VERTEX); }", true, true, false, false },
		{ "shader_type spatial; void vertex() { float f = VERTEX.x + TIME; }", false, true, false, false },
		{ "shader_type spatial; void fragment() { ALBEDO = vec3(TIME); }", false, true, false, false },
		{ "shader_type spatial; void fragment() { if (TIME > 0.0) { discard; } }", true, true, false, false },
		{ "shader_type spatial; void vertex() { VERTEX += TANGENT; }", false, true, true, false },
		{ "shader_type spatial; render_mode ensure_correct_normals;", false, true, false, true },
		{ "shader_type spatial; float unused_phase() { return TIME; } void vertex() { VERTEX.x = 0.0; }", false, true, false, false },
		{ "shader_type canvas_item; void fragment() { COLOR = vec4(TIME); }", false, false, false, false },
		{ "shader_type particles;", false, false, false, false },
	};
	for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		FF_STORAGE_CHECK(p_storage.shader_set_code(shader, cases[i].code) == OK);
		FixedFunctionShaderUsage::FFFeatures flags;
		const Error error = p_storage.material_get_features(material, flags);
		if (error != OK || flags.animated != cases[i].animated || flags.casts_shadows != cases[i].shadows || flags.uses_tangents != cases[i].tangents || flags.ensure_correct_normals != cases[i].normals) {
			printf("Feature case %u: error %d, flags %d %d %d %d\n", i, error, flags.animated, flags.casts_shadows, flags.uses_tangents, flags.ensure_correct_normals);
			return false;
		}
	}
	const RID pass = p_storage.material_create();
	const RID pass_shader = p_storage.shader_create();
	FF_STORAGE_CHECK(p_storage.material_set_shader(pass, pass_shader) == OK && p_storage.material_set_next_pass(material, pass) == OK);
	FF_STORAGE_CHECK(p_storage.shader_set_code(pass_shader, "shader_type spatial; render_mode ensure_correct_normals; void vertex() { VERTEX += TANGENT * TIME; }") == OK);
	FixedFunctionShaderUsage::FFFeatures flags;
	FF_STORAGE_CHECK(p_storage.material_get_features(material, flags) == OK && flags.animated && flags.casts_shadows && !flags.uses_tangents && !flags.ensure_correct_normals);
	FF_STORAGE_CHECK(p_storage.shader_add_custom_define(pass_shader, "#define TEST\n") == OK && p_storage.material_get_features(material, flags) == ERR_UNAVAILABLE);
	FF_STORAGE_CHECK(flags.animated && flags.casts_shadows);
	FF_STORAGE_CHECK(p_storage.shader_remove_custom_define(pass_shader, "#define TEST\n") == OK);
	FF_STORAGE_CHECK(p_storage.shader_set_code(pass_shader, "shader_type spatial; void vertex() { broken; }") == OK && p_storage.material_get_features(material, flags) == ERR_PARSE_ERROR);
	FF_STORAGE_CHECK(flags.animated && flags.casts_shadows);
	// Tangent/normal queries concern only the base pass, including its error status.
	FF_STORAGE_CHECK(p_storage.shader_set_code(shader, "shader_type spatial; render_mode ensure_correct_normals; void vertex() { VERTEX += TANGENT; }") == OK);
	FF_STORAGE_CHECK(p_storage.material_get_features(material, flags, false) == OK && flags.uses_tangents && flags.ensure_correct_normals && !flags.animated);
	p_storage.clear();
	return true;
}

bool TestFixedFunctionStorageIntegration::_test_materials(FixedFunctionMaterialStorage &p_storage) {
	const RID shader = p_storage.shader_create();
	const RID material = p_storage.material_create();
	const RID next = p_storage.material_create();
	FF_STORAGE_CHECK(shader.is_valid() && material.is_valid() && next.is_valid());
	FF_STORAGE_CHECK(p_storage.material_set_shader(material, shader) == OK);
	FF_STORAGE_CHECK(p_storage.material_set_next_pass(material, next) == OK);
	FF_STORAGE_CHECK(p_storage.material_set_next_pass(next, material) == ERR_CYCLIC_LINK);
	FF_STORAGE_CHECK(p_storage.material_set_render_priority(material, -128) == OK);
	FF_STORAGE_CHECK(p_storage.material_set_render_priority(material, 128) == ERR_INVALID_PARAMETER);
	FF_STORAGE_CHECK(p_storage.material_get_render_priority(material) == -128);
	FF_STORAGE_CHECK(p_storage.shader_set_code(shader, "shader_type canvas_item; render_mode blend_add, unshaded;") == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters parameters;
	parameters.alpha_cutoff_enabled = true;
	parameters.alpha_cutoff = 0.4f;
	FF_STORAGE_CHECK(p_storage.material_get_canvas_parameters(material, parameters) == OK);
	FF_STORAGE_CHECK(parameters.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD && parameters.alpha_cutoff == 0.4f);
	const String code = "shader_type canvas_item; uniform float amount : hint_range(0, 1) = 0.75; uniform vec4 tint : source_color = vec4(1.0, 0.5, 0.0, 1.0); uniform sampler2D layers[2];";
	FF_STORAGE_CHECK(p_storage.shader_set_code(shader, code) == OK && p_storage.shader_get_code(shader) == code);
	List<PropertyInfo> properties;
	FF_STORAGE_CHECK(p_storage.shader_get_param_list(shader, &properties) == OK && properties.size() == 3);
	FF_STORAGE_CHECK(properties.front()->get().name == "amount" && properties.front()->get().hint == PROPERTY_HINT_RANGE);
	FF_STORAGE_CHECK(double(p_storage.material_get_param(material, "amount")) == 0.75);
	FF_STORAGE_CHECK(Color(p_storage.material_get_param(material, "tint")) == Color(1, 0.5, 0, 1));
	FF_STORAGE_CHECK(p_storage.material_set_param(material, "amount", 0.25) == OK && double(p_storage.material_get_param(material, "amount")) == 0.25);
	FF_STORAGE_CHECK(p_storage.material_set_param(material, "amount", Variant()) == OK && double(p_storage.material_get_param(material, "amount")) == 0.75);
	FF_STORAGE_CHECK(p_storage.material_get_canvas_parameters(material, parameters) == ERR_UNAVAILABLE);
	FF_STORAGE_CHECK(parameters.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD);
	FF_STORAGE_CHECK(p_storage.shader_set_code(shader, "shader_type spatial; instance uniform float amount = 0.5; void move_vertex(inout vec3 v) { v.x += TIME; } void vertex() { move_vertex(VERTEX); } void fragment() { if (TIME > 1.0) { discard; } }") == OK);
	FixedFunctionShaderUsage::FFFeatures features;
	FF_STORAGE_CHECK(p_storage.material_get_features(material, features) == OK && features.animated && features.casts_shadows);
	List<RendererMaterialStorage::InstanceShaderParam> instances;
	FF_STORAGE_CHECK(p_storage.material_get_instance_shader_parameters(material, &instances) == OK && instances.size() == 1);
	FF_STORAGE_CHECK(instances.front()->get().info.name == "amount" && double(instances.front()->get().default_value) == 0.5);

	const RID next_shader = p_storage.shader_create();
	FF_STORAGE_CHECK(p_storage.material_set_shader(next, next_shader) == OK);
	FF_STORAGE_CHECK(p_storage.shader_set_code(next_shader, "shader_type spatial; instance uniform float detail : instance_index(7) = 0.25;") == OK);
	instances.clear();
	FF_STORAGE_CHECK(p_storage.material_get_instance_shader_parameters(material, &instances) == OK && instances.size() == 2);
	FF_STORAGE_CHECK(instances.front()->get().info.name == "amount");
	FF_STORAGE_CHECK(instances.back()->get().info.name == "detail" && instances.back()->get().index == 7 && double(instances.back()->get().default_value) == 0.25);
	FF_STORAGE_CHECK(p_storage.shader_set_code(next_shader, "shader_type spatial; void vertex() { broken; }") == OK);
	FF_STORAGE_CHECK(p_storage.material_get_instance_shader_parameters(material, &instances) == ERR_PARSE_ERROR && instances.size() == 2);
	FF_STORAGE_CHECK(p_storage.shader_set_code(next_shader, String()) == OK);
	FF_STORAGE_CHECK(p_storage.shader_set_code(shader, "shader_type spatial; instance uniform float updated = 0.125;") == OK);
	instances.clear();
	FF_STORAGE_CHECK(p_storage.material_get_instance_shader_parameters(material, &instances) == OK && instances.size() == 1);
	FF_STORAGE_CHECK(instances.front()->get().info.name == "updated" && double(instances.front()->get().default_value) == 0.125);
	FF_STORAGE_CHECK(p_storage.shader_set_code(shader, "shader_type spatial; uniform float updated = 0.125;") == OK);
	instances.clear();
	FF_STORAGE_CHECK(p_storage.material_get_instance_shader_parameters(material, &instances) == OK && instances.is_empty());

	FFDependencyState state;
	state.storage = &p_storage;
	state.material = material;
	DependencyTracker tracker;
	tracker.userdata = &state;
	tracker.changed_callback = _changed;
	tracker.deleted_callback = _deleted;
	tracker.update_begin();
	FF_STORAGE_CHECK(p_storage.material_update_dependency(material, &tracker) == OK);
	tracker.update_end();
	FF_STORAGE_CHECK(p_storage.update_dependencies() == OK && state.changed == 1);
	FF_STORAGE_CHECK(p_storage.update_dependencies() == OK && state.changed == 1);
	FF_STORAGE_CHECK(p_storage.material_set_param(material, "amount", 0.8) == OK);
	FF_STORAGE_CHECK(p_storage.material_set_param(material, "amount", 0.9) == OK);
	state.free_on_change = true;
	FF_STORAGE_CHECK(p_storage.update_dependencies() == ERR_SKIP && state.error == OK && state.changed == 2 && state.deleted == 1);
	FF_STORAGE_CHECK(!p_storage.owns_material(material));
	FF_STORAGE_CHECK(p_storage.free(next) == OK && p_storage.free(shader) == OK);
	state.material = p_storage.material_create();
	state.free_on_change = false;
	state.clear_on_change = true;
	tracker.update_begin();
	FF_STORAGE_CHECK(p_storage.material_update_dependency(state.material, &tracker) == OK);
	tracker.update_end();
	FF_STORAGE_CHECK(p_storage.material_set_param(state.material, "clear", true) == OK);
	FF_STORAGE_CHECK(p_storage.update_dependencies() == OK && state.deleted == 2);
	FF_STORAGE_CHECK(p_storage.get_material_count() == 0 && p_storage.get_shader_count() == 0);
	return true;
}

bool TestFixedFunctionStorageIntegration::_test_unlit_scene(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, RendererSceneRenderFixedFunction &p_scene, const Ref<RenderSceneBuffers> &p_buffers, RID p_target, std::vector<uint32_t> &r_pixels) {
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	const RID mesh = meshes.mesh_create();
	RenderingServerTypes::SurfaceData data;
	data.format |= RSE::ARRAY_FORMAT_VERTEX;
	data.primitive = RSE::PRIMITIVE_TRIANGLES;
	data.vertex_count = 3;
	data.aabb = AABB(Vector3(-1, -1, 0), Vector3(2, 2, 0));
	data.vertex_data.resize(36);
	const float positions[] = { -1, -1, 0, 0, 1, 0, 1, -1, 0 };
	memcpy(data.vertex_data.ptrw(), positions, sizeof(positions));
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	FF_STORAGE_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded; uniform vec4 tint : source_color = vec4(1,0,0,1); void fragment(){ALBEDO=tint.rgb;}") == OK && materials.material_set_shader(material, shader) == OK);
	data.material = material;
	FF_STORAGE_CHECK(meshes.mesh_add_surface(mesh, data) == OK);
	RenderGeometryInstance *instance = p_scene.geometry_instance_create(mesh);
	FF_STORAGE_CHECK(instance);
	instance->set_transform(Transform3D(Basis(), Vector3(0, 0, -3)), data.aabb, data.aabb);
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	instances.push_back(instance);
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	RendererSceneRender::CameraData camera;
	camera.set_camera(Transform3D(), projection, true, false);
	for (int frame = 0; frame < 2; frame++) {
		RenderingServerTypes::RenderInfo info;
		FF_STORAGE_CHECK(p_context.get_texture_storage().render_target_request_clear(p_target, Color(0, 0, 1)) == OK && p_targets.begin_frame(0) == OK);
		// Actual Camera3D submission uses pass -1, unlike the empty-scene helper.
		p_scene.render_scene(p_buffers, &camera, &camera, instances, PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), RID(), RID(), RID(), RID(), RID(), RID(), RID(), -1, 0, nullptr, 0, nullptr, 0, 1, nullptr, &info);
		FF_STORAGE_CHECK(p_scene.get_last_error() == OK);
		FF_STORAGE_CHECK(info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] == (frame == 0 ? 1 : 0));
		FF_STORAGE_CHECK(info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == (frame == 0 ? 1 : 0));
		FF_STORAGE_CHECK(info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] == (frame == 0 ? 1 : 0));
		FF_STORAGE_CHECK(p_targets.select_render_target(RID()) == OK && p_targets.blit_to_screen(p_target, Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT)) == OK && p_targets.end_frame(false) == OK);
		FF_STORAGE_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
		const uint32_t pixel = r_pixels[(CAPTURE_HEIGHT / 2) * CAPTURE_WIDTH + CAPTURE_WIDTH / 2];
		// Viewport sampling may round the modulated channel down by one.
		FF_STORAGE_CHECK(frame == 0 ? (pixel == 0xffff0000 || pixel == 0xfffe0000) : (pixel == 0xff0000ff || pixel == 0xff0000fe));
		if (frame == 0) {
			FF_STORAGE_CHECK(meshes.free(mesh) == OK);
		}
	}
	p_scene.geometry_instance_free(instance);
	FF_STORAGE_CHECK(materials.free(material) == OK && materials.free(shader) == OK);
	return true;
}

bool TestFixedFunctionStorageIntegration::_test_scene(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, std::vector<uint32_t> &r_pixels) {
	RendererSceneRenderFixedFunction scene(p_context, p_targets);
	const RID attributes = RSG::camera_attributes->camera_attributes_allocate();
	RSG::camera_attributes->camera_attributes_initialize(attributes);
	FF_STORAGE_CHECK(scene.free(attributes) && !RSG::camera_attributes->owns_camera_attributes(attributes));
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID target = textures.render_target_create();
	FF_STORAGE_CHECK(textures.render_target_set_size(target, CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	const RID environment = scene.environment_allocate();
	scene.environment_initialize(environment);
	scene.environment_set_bg_color(environment, Color(1, 0, 0));
	Ref<RenderSceneBuffers> buffers = scene.render_buffers_create();
	RenderSceneBuffersConfiguration config;
	config.set_render_target(target);
	config.set_internal_size(Size2i(CAPTURE_WIDTH, CAPTURE_HEIGHT));
	config.set_target_size(Size2i(CAPTURE_WIDTH, CAPTURE_HEIGHT));
	buffers->configure(&config);
	RendererSceneRender::CameraData camera;
	camera.set_camera(Transform3D(), Projection(), true, false);
	RendererSceneRender::RenderSDFGIUpdateData sdfgi_update;
	for (int mode = 0; mode < 4; mode++) {
		RSE::EnvironmentBG background = RSE::ENV_BG_SKY;
		if (mode == 0) {
			background = RSE::ENV_BG_COLOR;
		} else if (mode == 1) {
			background = RSE::ENV_BG_CANVAS;
		} else if (mode == 2) {
			background = RSE::ENV_BG_CLEAR_COLOR;
		}
		scene.environment_set_background(environment, background);
		if (mode == 3) {
			scene.environment_set_tonemap(environment, RSE::ENV_TONE_MAPPER_FILMIC, 2, 1);
		}
		FF_STORAGE_CHECK(textures.render_target_request_clear(target, Color(0, 0, 1)) == OK);
		FF_STORAGE_CHECK(p_targets.begin_frame(0) == OK);
		// These are the native empty-scene arguments, with no allocated 3D resources.
		scene.render_scene(buffers, &camera, &camera, PagedArray<RenderGeometryInstance *>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), environment, RID(), RID(), RID(), RID(), RID(), RID(), 0, 0, nullptr, 0, nullptr, 0, 1, &sdfgi_update);
		FF_STORAGE_CHECK(scene.get_last_error() == OK);
		if (mode == 3) {
			FF_STORAGE_CHECK(scene.environment_get_background(environment) == RSE::ENV_BG_SKY && scene.environment_get_tone_mapper(environment) == RSE::ENV_TONE_MAPPER_FILMIC && scene.environment_get_exposure(environment) == 2);
		}
		FF_STORAGE_CHECK(p_targets.select_render_target(RID()) == OK);
		FF_STORAGE_CHECK(p_targets.blit_to_screen(target, Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT)) == OK);
		FF_STORAGE_CHECK(p_targets.end_frame(false) == OK);
		FF_STORAGE_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
		const uint32_t expected = mode == 0 ? 0xffff0000 : 0xff0000ff;
		const uint32_t minimum = mode == 0 ? 0xfffe0000 : 0xff0000fe;
		for (size_t i = 0; i < r_pixels.size(); i++) {
			// Blitting the viewport modulates its color by white.
			FF_STORAGE_CHECK(r_pixels[i] == expected || r_pixels[i] == minimum);
		}
	}
	FF_STORAGE_CHECK(_test_unlit_scene(p_context, p_targets, scene, buffers, target, r_pixels));
	// A base-class metadata setter may accept KEEP, but this renderer must reject drawing it.
	scene.environment_set_background(environment, RSE::ENV_BG_KEEP);
	FF_STORAGE_CHECK(p_targets.begin_frame(0) == OK);
	const bool print_errors = CoreGlobals::print_error_enabled;
	CoreGlobals::print_error_enabled = false;
	scene.render_scene(buffers, &camera, &camera, PagedArray<RenderGeometryInstance *>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), PagedArray<RID>(), environment, RID(), RID(), RID(), RID(), RID(), RID(), 0, 0, nullptr, 0, nullptr, 0, 1);
	CoreGlobals::print_error_enabled = print_errors;
	FF_STORAGE_CHECK(scene.get_last_error() == ERR_UNAVAILABLE && p_targets.end_frame(false) == OK);
	FF_STORAGE_CHECK(textures.render_target_has_failed(target) && !textures.render_target_is_ready(target));
	FF_STORAGE_CHECK(scene.free(environment));
	FF_STORAGE_CHECK(textures.free(target) == OK);
	return true;
}

bool TestFixedFunctionStorageIntegration::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FixedFunctionRenderTargetManager targets(context);
	FixedFunctionCanvasItemRenderer canvas(context, targets);
	RendererTextureStorageFixedFunction facade(context.get_texture_storage(), &targets);
	RendererMaterialStorageFixedFunction materials(context);
	FF_STORAGE_CHECK(context.initialize() == OK);
	FF_STORAGE_CHECK(_test_feature_table(context.get_material_storage()));
	FF_STORAGE_CHECK(_test_materials(context.get_material_storage()));
	FixedFunctionTextureStorage &textures = context.get_texture_storage();
	const RID texture = facade.texture_allocate();
	const RID replacement = facade.texture_allocate();
	const RID proxy = facade.texture_allocate();
	const RID wrapper = facade.canvas_texture_allocate();
	facade.canvas_texture_initialize(wrapper);
	facade.canvas_texture_set_channel(wrapper, RSE::CANVAS_TEXTURE_CHANNEL_DIFFUSE, proxy);
	facade.canvas_texture_set_texture_filter(wrapper, RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC);
	facade.canvas_texture_set_texture_repeat(wrapper, RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	facade.texture_proxy_initialize(proxy, texture);
	facade.texture_set_path(texture, "res://retained.png");
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 0));
	facade.texture_2d_initialize(texture, image);
	RID native;
	FF_STORAGE_CHECK(textures.texture_get_or_upload_device_texture(proxy, native) == OK);
	image->fill(Color(0, 1, 0));
	facade.texture_2d_initialize(replacement, image);
	facade.texture_replace(texture, replacement);
	FF_STORAGE_CHECK(facade.get_last_error() == OK && !textures.owns_texture(replacement));
	FF_STORAGE_CHECK(textures.get_pending_release_count() == 1 && textures.get_pending_upload_count() == 0);
	FF_STORAGE_CHECK(textures.texture_can_upload_on_use(proxy) && textures.texture_get_device_texture(proxy, native) == ERR_UNCONFIGURED);
	FF_STORAGE_CHECK(facade.texture_get_path(texture) == "res://retained.png" && textures.texture_get_proxy(proxy) == texture);
	FF_STORAGE_CHECK(facade.texture_2d_get(texture)->get_pixel(0, 0) == Color(0, 1, 0));
	const RID shader = materials.shader_allocate();
	materials.shader_initialize(shader, false);
	materials.shader_set_default_texture_parameter(shader, "layers", texture, 1);
	FF_STORAGE_CHECK(materials.get_last_error() == OK && materials.shader_get_default_texture_parameter(shader, "layers", 1) == texture);
	FF_STORAGE_CHECK(!materials.shader_get_default_texture_parameter(shader, "layers", 0).is_valid());
	textures.texture_set_force_redraw_if_visible(proxy, true);
	RendererCanvasRender::Item item;
	RendererCanvasRender::Item::CommandRect *rect = item.alloc_command<RendererCanvasRender::Item::CommandRect>();
	rect->rect = Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT);
	rect->texture = wrapper;
	rect->modulate = Color(1, 1, 1);
	FF_STORAGE_CHECK(targets.begin_frame(0) == OK);
	FF_STORAGE_CHECK(canvas.render_items(&item, Color(1, 1, 1)) == OK && canvas.is_redraw_requested());
	FF_STORAGE_CHECK(targets.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FF_STORAGE_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (size_t i = 0; i < r_pixels.size(); i++) {
		// RGB software modulation and blending can round the green channel down.
		FF_STORAGE_CHECK((r_pixels[i] & 0xffff00ff) == 0xff000000 && ((r_pixels[i] >> 8) & 0xff) >= 252);
	}
	FF_STORAGE_CHECK(canvas.get_render_info().info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] == 1);
	List<FixedFunctionTextureStorage::FFTextureUsage> usage;
	FF_STORAGE_CHECK(textures.texture_debug_usage(&usage) == OK);
	uint64_t native_bytes = 0;
	for (const FixedFunctionTextureStorage::FFTextureUsage &row : usage) {
		native_bytes += row.native_bytes;
	}
	FF_STORAGE_CHECK(native_bytes >= 4 * 4 * 4 && !p_device.get_adapter_name().is_empty());
	facade.texture_free(texture);
	FF_STORAGE_CHECK(!textures.texture_can_be_referenced(texture));
	materials.shader_free(shader);
	FF_STORAGE_CHECK(_test_scene(context, targets, r_pixels));
	FF_STORAGE_CHECK(context.finalize() == OK);
	printf("PASS: 4.x material reflection/defaults, preset blend lowering, next-pass cycles, native dependencies, texture replacement/wrappers, visibility, counters, diagnostics, empty-scene backgrounds and cancellation.\n");
	return true;
}

#undef FF_STORAGE_CHECK
