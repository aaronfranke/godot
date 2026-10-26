#include "test_scene_bootstrap.h"

#include "drivers/fixed_function/rasterizer_fixed_function.h"
#include "drivers/fixed_function/rasterizer_storage_fixed_function.h"

#include <stdio.h>
#include <limits>
#include <type_traits>

#define FF_BOOT_CHECK(m_condition)                                              \
	do {                                                                        \
		if (!(m_condition)) {                                                   \
			printf("Scene bootstrap FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                       \
		}                                                                       \
	} while (false)

static_assert(!std::is_abstract<RasterizerSceneFixedFunction>::value, "The 2D scene boundary must be concrete.");
static_assert(!std::is_abstract<RasterizerFixedFunction>::value && !std::is_abstract<RasterizerStorageFixedFunction>::value, "The restricted root and storage boundary must be concrete.");

TestFixedFunctionSceneBootstrap::FFInstance::FFInstance(FixedFunctionMaterialStorage &p_storage, RID p_material) :
		storage(p_storage), other(nullptr), survivor(nullptr), nested(OK), changes(0), removals(0), action(NONE), valid_flags(true) { bind(p_material); }
TestFixedFunctionSceneBootstrap::FFInstance::~FFInstance() { detach(); }
void TestFixedFunctionSceneBootstrap::FFInstance::bind(RID p_material) {
	material_override = p_material;
	if (p_material.is_valid()) {
		storage.material_add_instance_owner(p_material, this);
	}
}
void TestFixedFunctionSceneBootstrap::FFInstance::detach() {
	if (material_override.is_valid()) {
		storage.material_remove_instance_owner(material_override, this);
		material_override = RID();
	}
	if (material_overlay.is_valid()) {
		storage.material_remove_instance_owner(material_overlay, this);
		material_overlay = RID();
	}
	for (int i = 0; i < materials.size(); i++) {
		if (materials[i].is_valid()) {
			storage.material_remove_instance_owner(materials[i], this);
		}
	}
	materials.clear();
}
void TestFixedFunctionSceneBootstrap::FFInstance::base_removed() { removals++; }
void TestFixedFunctionSceneBootstrap::FFInstance::base_changed(bool p_aabb, bool p_materials) {
	changes++;
	valid_flags &= !p_aabb && p_materials;
	const FFAction request = action;
	action = NONE;
	if (request == CHANGE) {
		storage.material_set_param(material_override, "amount", changes);
		nested = storage.update_dependencies();
	}
	if ((request == REMOVE_OTHER || request == REBIND_OTHER) && other) {
		const RID material = other->material_override;
		other->action = NONE;
		other->detach();
		if (request == REBIND_OTHER) {
			other->bind(material);
		}
	}
	if (request == DELETE_OTHER && other) {
		if (survivor) {
			*survivor = this;
		}
		FFInstance *retired = other;
		other = nullptr;
		memdelete(retired);
	}
	if (request == FREE_MATERIAL) {
		storage.free(material_override);
	}
	if (request == CLEAR) {
		storage.clear();
	}
}

bool TestFixedFunctionSceneBootstrap::_test_features(FixedFunctionMaterialStorage &p_storage) {
	const RID shader = p_storage.shader_create();
	const RID material = p_storage.material_create();
	FF_BOOT_CHECK(p_storage.material_set_shader(material, shader) == OK);
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
		{ "shader_type spatial; void fragment() { ALPHA = 0.5; ALPHA_SCISSOR = 0.25; }", false, true, false, false },
		{ "shader_type spatial; render_mode depth_draw_alpha_prepass; void fragment() { ALPHA = 0.5; }", false, true, false, false },
		{ "shader_type spatial; void vertex() { VERTEX.x += sin(TIME); }", true, true, false, false },
		{ "shader_type spatial; float phase() { return TIME; } void vertex() { VERTEX.x += phase(); }", true, true, false, false },
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
		FF_BOOT_CHECK(p_storage.shader_set_code(shader, cases[i].code) == OK);
		FixedFunctionShaderUsage::FFFeatures flags;
		const Error error = p_storage.material_get_features(material, flags);
		if (error != OK || flags.animated != cases[i].animated || flags.casts_shadows != cases[i].shadows || flags.uses_tangents != cases[i].tangents || flags.ensure_correct_normals != cases[i].normals) {
			printf("Feature case %u: error %d, flags %d %d %d %d\n", i, error, flags.animated, flags.casts_shadows, flags.uses_tangents, flags.ensure_correct_normals);
			return false;
		}
	}
	const RID pass = p_storage.material_create();
	const RID pass_shader = p_storage.shader_create();
	FF_BOOT_CHECK(p_storage.material_set_shader(pass, pass_shader) == OK && p_storage.material_set_next_pass(material, pass) == OK);
	FF_BOOT_CHECK(p_storage.shader_set_code(pass_shader, "shader_type spatial; render_mode ensure_correct_normals; void vertex() { VERTEX += TANGENT * TIME; }") == OK);
	FixedFunctionShaderUsage::FFFeatures flags;
	FF_BOOT_CHECK(p_storage.material_get_features(material, flags) == OK && flags.animated && flags.casts_shadows && !flags.uses_tangents && !flags.ensure_correct_normals);
	FF_BOOT_CHECK(p_storage.shader_add_custom_define(pass_shader, "#define TEST\n") == OK && p_storage.material_get_features(material, flags) == ERR_UNAVAILABLE);
	FF_BOOT_CHECK(flags.animated && flags.casts_shadows);
	FF_BOOT_CHECK(p_storage.shader_remove_custom_define(pass_shader, "#define TEST\n") == OK);
	FF_BOOT_CHECK(p_storage.shader_set_code(pass_shader, "shader_type spatial; void vertex() { broken; }") == OK && p_storage.material_get_features(material, flags) == ERR_PARSE_ERROR);
	FF_BOOT_CHECK(flags.animated && flags.casts_shadows);
	// Tangent/normal queries concern only the base pass, including its error status.
	FF_BOOT_CHECK(p_storage.shader_set_code(shader, "shader_type spatial; render_mode ensure_correct_normals; void vertex() { VERTEX += TANGENT; }") == OK);
	FF_BOOT_CHECK(p_storage.material_get_features(material, flags, false) == OK && flags.uses_tangents && flags.ensure_correct_normals && !flags.animated);
	p_storage.clear();
	return true;
}

bool TestFixedFunctionSceneBootstrap::_test_dependencies(FixedFunctionMaterialStorage &p_storage) {
	RID material = p_storage.material_create();
	{
		FFInstance owner(p_storage, material);
		owner.material_overlay = material;
		owner.materials.push_back(material);
		FF_BOOT_CHECK(p_storage.material_add_instance_owner(material, &owner) == OK && p_storage.material_add_instance_owner(material, &owner) == OK);
		FF_BOOT_CHECK(p_storage.update_dependencies() == OK && owner.changes == 1 && owner.valid_flags);
		FF_BOOT_CHECK(p_storage.material_remove_instance_owner(material, &owner) == OK);
		owner.material_overlay = RID();
		FF_BOOT_CHECK(p_storage.material_set_param(material, "x", 1) == OK && p_storage.update_dependencies() == OK && owner.changes == 2);
		owner.action = FFInstance::CHANGE;
		FF_BOOT_CHECK(p_storage.material_set_line_width(material, 2) == OK && p_storage.update_dependencies() == ERR_SKIP && owner.nested == ERR_BUSY);
		FF_BOOT_CHECK(p_storage.update_dependencies() == OK && owner.changes == 4);
	}
	FF_BOOT_CHECK(p_storage.update_dependencies() == OK);
	for (int mode = FFInstance::REMOVE_OTHER; mode <= FFInstance::REBIND_OTHER; mode++) {
		FFInstance a(p_storage, material), b(p_storage, material);
		a.other = &b;
		b.other = &a;
		a.action = b.action = FFInstance::FFAction(mode);
		const Error error = p_storage.update_dependencies();
		FF_BOOT_CHECK(error == (mode == FFInstance::REBIND_OTHER ? ERR_SKIP : OK));
		FF_BOOT_CHECK(a.changes + b.changes == 1);
		if (mode == FFInstance::REBIND_OTHER) {
			FF_BOOT_CHECK(p_storage.update_dependencies() == OK && a.changes + b.changes == 3);
		}
	}
	{
		FFInstance *survivor = nullptr;
		FFInstance *a = memnew(FFInstance(p_storage, material));
		FFInstance *b = memnew(FFInstance(p_storage, material));
		a->other = b;
		b->other = a;
		a->survivor = b->survivor = &survivor;
		a->action = b->action = FFInstance::DELETE_OTHER;
		FF_BOOT_CHECK(p_storage.update_dependencies() == OK && survivor && survivor->changes == 1);
		memdelete(survivor);
	}
	{
		FFInstance a(p_storage, material), b(p_storage, material);
		a.action = b.action = FFInstance::FREE_MATERIAL;
		FF_BOOT_CHECK(p_storage.update_dependencies() == ERR_SKIP && a.changes + b.changes == 1);
		FF_BOOT_CHECK(!a.material_override.is_valid() && !b.material_override.is_valid() && a.removals == 0 && b.removals == 0);
		FF_BOOT_CHECK(p_storage.update_dependencies() == OK);
	}
	material = p_storage.material_create();
	{
		FFInstance a(p_storage, material), b(p_storage, material);
		a.action = b.action = FFInstance::CLEAR;
		FF_BOOT_CHECK(p_storage.update_dependencies() == OK && a.changes + b.changes == 1 && !a.material_override.is_valid() && !b.material_override.is_valid());
	}
	FF_BOOT_CHECK(p_storage.get_material_count() == 0 && p_storage.update_dependencies() == OK);
	printf("PASS: derived material flags, reachable shader usage, counted dependencies, reentrant changes/removal/rebind/destruction/free/clear and slot cleanup.\n");
	return true;
}

bool TestFixedFunctionSceneBootstrap::_test_owners(FixedFunctionSceneStorage &p_storage) {
	const RID shadow = p_storage.shadow_atlas_create();
	const RID reflection = p_storage.reflection_atlas_create();
	const RID env = p_storage.environment_create();
	FF_BOOT_CHECK(shadow.is_valid() && reflection.is_valid() && env.is_valid());
	FF_BOOT_CHECK(p_storage.shadow_atlas_set_size(shadow, 1024) == OK && p_storage.shadow_atlas_get_size(shadow) == 1024);
	for (int i = 0; i < 4; i++) {
		FF_BOOT_CHECK(p_storage.shadow_atlas_set_subdivision(shadow, i, i == 3 ? 8 : 4) == OK && p_storage.shadow_atlas_get_subdivision(shadow, i) == (i == 3 ? 8 : 4));
	}
	FF_BOOT_CHECK(p_storage.shadow_atlas_set_size(shadow, -1) == ERR_INVALID_PARAMETER && p_storage.shadow_atlas_get_size(shadow) == 1024);
	FF_BOOT_CHECK(p_storage.shadow_atlas_set_subdivision(shadow, 4, 1) == ERR_INVALID_PARAMETER && p_storage.shadow_atlas_set_size(env, 1) == ERR_INVALID_PARAMETER);
	FF_BOOT_CHECK(p_storage.reflection_atlas_set_size(reflection, 128) == OK && p_storage.reflection_atlas_set_subdivision(reflection, 16) == OK);
	FF_BOOT_CHECK(p_storage.reflection_atlas_get_size(reflection) == 128 && p_storage.reflection_atlas_get_subdivision(reflection) == 16);
	FF_BOOT_CHECK(p_storage.environment_get_background(env) == VS::ENV_BG_CLEAR_COLOR && p_storage.environment_get_energy(env) == 1);
	FF_BOOT_CHECK(p_storage.environment_set_background(env, VS::ENV_BG_SKY) == OK && p_storage.environment_get_background(env) == VS::ENV_BG_SKY);
	FF_BOOT_CHECK(p_storage.environment_set_background(env, VS::ENV_BG_CLEAR_COLOR) == OK);
	FF_BOOT_CHECK(p_storage.environment_set_background(env, VS::ENV_BG_KEEP) == ERR_UNAVAILABLE);
	FF_BOOT_CHECK(p_storage.environment_set_canvas_max_layer(env, -7) == OK && p_storage.environment_get_canvas_max_layer(env) == -7);
	FF_BOOT_CHECK(p_storage.environment_set_camera_feed(env, 1) == OK && p_storage.environment_set_ambient(env, Color(0.2, 0.3, 0.4), 2, 1) == OK);
	FF_BOOT_CHECK(p_storage.validate_empty_scene(env, shadow, reflection, 0, 0, 0) == OK && p_storage.validate_empty_scene(env, shadow, reflection, 1, 0, 0) == ERR_UNAVAILABLE);
	FF_BOOT_CHECK(p_storage.validate_empty_scene(env, reflection, shadow, 0, 0, 0) == ERR_INVALID_PARAMETER);
	p_storage.clear();
	FF_BOOT_CHECK(p_storage.get_shadow_atlas_count() == 0 && p_storage.get_reflection_atlas_count() == 0 && p_storage.get_environment_count() == 0);
	return true;
}

bool TestFixedFunctionSceneBootstrap::_render(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RasterizerSceneFixedFunction &p_scene, RID p_env, RID p_shadow, RID p_reflection, std::vector<uint32_t> &r_pixels) {
	FF_BOOT_CHECK(p_context.get_scene_storage().environment_set_background(p_env, VS::ENV_BG_COLOR) == OK);
	FF_BOOT_CHECK(p_manager.begin_frame(0) == OK);
	p_scene.render_scene(Transform(), CameraMatrix(), 0, true, nullptr, 0, nullptr, 0, nullptr, 0, p_env, p_shadow, p_reflection, RID(), 0);
	FF_BOOT_CHECK(p_scene.get_last_error() == OK);
	FixedFunctionCanvasItemRenderer canvas(p_context, p_manager);
	RasterizerCanvas::Item item;
	RasterizerCanvas::Item::CommandRect *rect = memnew(RasterizerCanvas::Item::CommandRect);
	rect->rect = Rect2(0, 0, 64, 48);
	rect->modulate = Color(1, 0, 0);
	item.commands.push_back(rect);
	FF_BOOT_CHECK(canvas.render_items(&item, Color(1, 1, 1)) == OK);
	p_scene.environment_set_background(p_env, VS::ENV_BG_CANVAS);
	p_scene.render_scene(Transform(), CameraMatrix(), 0, true, nullptr, 0, nullptr, 0, nullptr, 0, p_env, p_shadow, p_reflection, RID(), 0);
	FF_BOOT_CHECK(p_scene.get_last_error() == OK && p_manager.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FF_BOOT_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			const uint32_t expected = x < 64 && y < 48 ? 0xffff0000 : 0xff8040bf;
			const uint32_t actual = r_pixels[size_t(y) * CAPTURE_WIDTH + x];
			FF_BOOT_CHECK((actual >> 24) == 255);
			for (int shift = 0; shift < 24; shift += 8) {
				const int delta = int((actual >> shift) & 255) - int((expected >> shift) & 255);
				FF_BOOT_CHECK(delta >= -8 && delta <= 8);
			}
		}
	}
	return true;
}

bool TestFixedFunctionSceneBootstrap::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	RasterizerStorage *const singleton = RasterizerStorage::base_singleton;
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FixedFunctionRenderTargetManager manager(context);
	RasterizerSceneFixedFunction scene(context, manager);
	FF_BOOT_CHECK(context.initialize() == OK);
	FF_BOOT_CHECK(_test_panorama_averages(context.get_texture_storage()));
	FF_BOOT_CHECK(_test_features(context.get_material_storage()) && _test_dependencies(context.get_material_storage()) && _test_owners(context.get_scene_storage()));
	const RID shadow = scene.shadow_atlas_create();
	const RID reflection = scene.reflection_atlas_create();
	const RID env = scene.environment_create();
	scene.shadow_atlas_set_size(shadow, 1024);
	scene.shadow_atlas_set_quadrant_subdivision(shadow, 3, 8);
	scene.reflection_atlas_set_size(reflection, 128);
	scene.reflection_atlas_set_subdivision(reflection, 16);
	scene.environment_set_bg_color(env, Color(0.25, 0.125, 0.375));
	scene.environment_set_bg_energy(env, 2);
	scene.environment_set_canvas_max_layer(env, 3);
	// Default Environment requests must not poison an otherwise supported 2D frame.
	scene.environment_set_sky(env, RID());
	scene.environment_set_sky_custom_fov(env, 0);
	scene.environment_set_sky_orientation(env, Basis());
	scene.environment_set_ambient_light(env, Color(), 1, 1);
	scene.environment_set_camera_feed_id(env, 1);
	scene.environment_set_dof_blur_near(env, false, 2, 1, 0.1, VS::ENV_DOF_BLUR_QUALITY_MEDIUM);
	scene.environment_set_dof_blur_far(env, false, 10, 5, 0.1, VS::ENV_DOF_BLUR_QUALITY_MEDIUM);
	scene.environment_set_glow(env, false, 20, 0.8, 1, 0, VS::GLOW_BLEND_MODE_SOFTLIGHT, 1, 2, 12, false, false);
	scene.environment_set_glow_map(env, 0.8, RID());
	scene.environment_set_ssr(env, false, 64, 0.15, 2, 0.2, true);
	scene.environment_set_ssao(env, false, 1, 1, 0, 1, 0.01, 0, 0, Color(), VS::ENV_SSAO_QUALITY_MEDIUM, VS::ENV_SSAO_BLUR_3x3, 4);
	scene.environment_set_tonemap(env, VS::ENV_TONE_MAPPER_LINEAR, 1, 1, false, 0.05, 8, 0.5, 0.4);
	scene.environment_set_adjustment(env, false, 1, 1, 1, RID());
	scene.environment_set_fog(env, false, Color(0.5, 0.6, 0.7), Color(1, 0.9, 0.7), 0);
	scene.environment_set_fog(env, false, 10, 100, RID());
	scene.environment_set_fog_depth(env, true, 10, 100, 1, false, 1);
	scene.environment_set_fog_height(env, false, 10, 0, 1);
	scene.set_directional_shadow_count(0);
	scene.set_debug_draw_mode(VS::VIEWPORT_DEBUG_DRAW_DISABLED);
	scene.set_scene_pass(42);
	FF_BOOT_CHECK(scene.get_last_error() == OK && scene.get_scene_pass() == 42 && scene.environment_get_canvas_max_layer(env) == 3 && !scene.light_instances_can_render_shadow_cube());
	FF_BOOT_CHECK(_render(context, manager, scene, env, shadow, reflection, r_pixels));
	const std::vector<uint32_t> capture = r_pixels;
	scene.environment_set_background(env, VS::ENV_BG_SKY);
	scene.environment_set_tonemap(env, VS::ENV_TONE_MAPPER_FILMIC, 2, 3, true, 0.05, 8, 0.5, 0.4);
	FF_BOOT_CHECK(scene.get_last_error() == OK);
	Vector<Variant> effect;
	FF_BOOT_CHECK(context.get_scene_storage().environment_get_effect(env, "tonemap", effect) == OK && effect.size() == 8 && int(effect[0]) == VS::ENV_TONE_MAPPER_FILMIC && double(effect[1]) == 2);
	effect.write[1] = 7;
	FF_BOOT_CHECK(context.get_scene_storage().environment_get_effect(env, "tonemap", effect) == OK && double(effect[1]) == 2);
	Variant invalid[] = { std::numeric_limits<double>::quiet_NaN() };
	FF_BOOT_CHECK(context.get_scene_storage().environment_set_effect(env, "tonemap", invalid, 1) == ERR_INVALID_PARAMETER);
	FF_BOOT_CHECK(context.get_scene_storage().environment_get_effect(env, "tonemap", effect) == OK && double(effect[1]) == 2);
	FF_BOOT_CHECK(manager.begin_frame(0) == OK && manager.clear_render_target(Color(0, 0, 1)) == OK);
	scene.render_scene(Transform(), CameraMatrix(), 0, true, nullptr, 0, nullptr, 0, nullptr, 0, env, shadow, reflection, RID(), 0);
	FF_BOOT_CHECK(scene.get_last_error() == OK && manager.end_frame(false) == OK);
	FF_BOOT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (size_t i = 0; i < r_pixels.size(); i++) {
		FF_BOOT_CHECK(r_pixels[i] == 0xff0000ff);
	}
	FF_BOOT_CHECK(context.resize(160, 120) == OK && context.prepare_resources() == OK && context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FF_BOOT_CHECK(_render(context, manager, scene, env, shadow, reflection, r_pixels) && r_pixels == capture);
	FF_BOOT_CHECK(_test_sky(context, manager, scene, env));
	FF_BOOT_CHECK(context.finalize() == OK && context.get_scene_storage().get_environment_count() == 0 && context.get_scene_storage().get_shadow_atlas_count() == 0 && context.get_scene_storage().get_reflection_atlas_count() == 0);
	FF_BOOT_CHECK(RasterizerStorage::base_singleton == singleton);
	printf("PASS: concrete scene bootstrap, real atlas/environment ownership, empty-scene color/canvas retention, validation, reset and shutdown.\n");
	return true;
}

#undef FF_BOOT_CHECK
