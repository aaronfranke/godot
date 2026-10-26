#include "test_material_storage.h"

#include "drivers/fixed_function/rasterizer_storage_fixed_function.h"
#include "scene/2d/canvas_item.h"

#include <stdio.h>
#include <limits>
#include <type_traits>

#define FIXED_FUNCTION_MATERIAL_CHECK(m_condition)                            \
	do {                                                                      \
		if (!(m_condition)) {                                                 \
			printf("Material FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                     \
		}                                                                     \
	} while (false)

static_assert(!std::is_abstract<RasterizerStorageFixedFunction>::value, "The restricted storage boundary must be concrete.");

String TestFixedFunctionMaterialStorage::_get_preset_code(int p_blend, int p_light, bool p_particles) {
	Ref<CanvasItemMaterial> preset;
	preset.instance();
	preset->set_blend_mode(CanvasItemMaterial::BlendMode(p_blend));
	preset->set_light_mode(CanvasItemMaterial::LightMode(p_light));
	preset->set_particles_animation(p_particles);
	CanvasItemMaterial::flush_changes();
	// Capture the actual resource emitter through the existing server; do not replace its singleton.
	return VS::get_singleton()->shader_get_code(preset->get_shader_rid());
}

bool TestFixedFunctionMaterialStorage::_test_ownership(FixedFunctionMaterialStorage &p_storage) {
	const RID shader = p_storage.shader_create();
	const RID material = p_storage.material_create();
	const RID second = p_storage.material_create();
	FIXED_FUNCTION_MATERIAL_CHECK(shader.is_valid() && material.is_valid() && second.is_valid());
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.get_shader_count() == 1 && p_storage.get_material_count() == 2);
	FIXED_FUNCTION_MATERIAL_CHECK(!p_storage.owns_material(shader) && !p_storage.owns_shader(material));
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_set_shader(material, material) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_MATERIAL_CHECK(!p_storage.material_get_shader(material).is_valid());
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_set_shader(material, shader) == OK && p_storage.material_set_shader(second, shader) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_set_param(material, "amount", 2.0) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(double(p_storage.material_get_param(material, "amount")) == 2.0);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_set_param(material, "amount", Variant()) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_get_param(material, "amount").get_type() == Variant::NIL);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_set_param(material, "color", Color(1, 0, 1)) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(Color(p_storage.material_get_param(material, "color")) == Color(1, 0, 1));
	const String code = "// arbitrary formatting/comment\nshader_type canvas_item; render_mode blend_add;";
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_set_code(shader, code) == OK && p_storage.shader_get_code(shader) == code);
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.clip_enabled = true;
	state.clip_rect = Rect2i(1, 2, 3, 4);
	state.alpha_cutoff_enabled = true;
	state.alpha_cutoff = 0.25f;
	state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U;
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_get_canvas_parameters(material, state) == OK && state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD);
	FIXED_FUNCTION_MATERIAL_CHECK(state.clip_enabled && state.clip_rect == Rect2i(1, 2, 3, 4) && state.alpha_cutoff_enabled && state.alpha_cutoff == 0.25f && state.sampler_flags == FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_set_code(shader, "shader_type canvas_item; render_mode blend_mul;") == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_get_canvas_parameters(second, state) == OK && state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MUL);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_set_code(shader, "shader_type canvas_item; uniform float amount = 0.75;") == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(double(p_storage.material_get_param(material, "amount")) == 0.75);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_set_param(material, "amount", 0.5) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(double(p_storage.material_get_param(material, "amount")) == 0.5 && double(p_storage.material_get_param_default(material, "amount")) == 0.75);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_set_param(material, "amount", Variant()) == OK && double(p_storage.material_get_param(material, "amount")) == 0.75);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.free(shader) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(!p_storage.material_get_shader(material).is_valid() && !p_storage.material_get_shader(second).is_valid());
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_get_canvas_parameters(material, state) == OK && state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MIX);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.free(RID()) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.free(material) == OK && p_storage.free(second) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.get_material_count() == 0 && p_storage.get_shader_count() == 0);
	return true;
}

bool TestFixedFunctionMaterialStorage::_test_shader_metadata(FixedFunctionMaterialStorage &p_storage, FixedFunctionTextureStorage &p_textures) {
	const RID shader = p_storage.shader_create();
	const RID material = p_storage.material_create();
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_set_shader(material, shader) == OK);
	List<PropertyInfo> parameters;
	parameters.push_back(PropertyInfo(Variant::NIL, "sentinel"));
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_get_param_list(shader, &parameters) == OK && parameters.size() == 1);
	const String code =
			"shader_type canvas_item;\n"
			"uniform sampler2D albedo_texture;\n"
			"uniform float gain : hint_range(-2, 2, 0.25) = 0.5;\n"
			"uniform bool enabled = true;\n"
			"uniform bvec2 mask2; uniform bvec3 mask3; uniform bvec4 mask4;\n"
			"uniform int count : hint_range(0, 10, 1) = 3; uniform uint index;\n"
			"uniform ivec2 i2; uniform ivec3 i3; uniform ivec4 i4;\n"
			"uniform uvec2 u2; uniform uvec3 u3; uniform uvec4 u4;\n"
			"uniform vec2 uv_offset; uniform vec3 direction;\n"
			"uniform vec4 tint : hint_color = vec4(0.25, 0.5, 0.75, 1.0);\n"
			"uniform vec4 plane; uniform mat2 m2; uniform mat3 m3; uniform mat4 m4;\n"
			"uniform samplerCube cube; uniform sampler2DArray layers; uniform sampler3D volume;\n";
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_set_code(shader, code) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_get_param_list(shader, &parameters) == OK && parameters.size() == 25);
	struct FFUniformExpectation {
		const char *name;
		const char *hint_string;
		Variant::Type type;
		PropertyHint hint;
	};
	const FFUniformExpectation expected[] = {
		{ "gain", "-2,2,0.25", Variant::REAL, PROPERTY_HINT_RANGE },
		{ "enabled", "", Variant::BOOL, PROPERTY_HINT_NONE },
		{ "mask2", "x,y", Variant::INT, PROPERTY_HINT_FLAGS },
		{ "mask3", "x,y,z", Variant::INT, PROPERTY_HINT_FLAGS },
		{ "mask4", "x,y,z,w", Variant::INT, PROPERTY_HINT_FLAGS },
		{ "count", "0,10,1", Variant::INT, PROPERTY_HINT_RANGE },
		{ "index", "", Variant::INT, PROPERTY_HINT_NONE },
		{ "i2", "", Variant::POOL_INT_ARRAY, PROPERTY_HINT_NONE },
		{ "i3", "", Variant::POOL_INT_ARRAY, PROPERTY_HINT_NONE },
		{ "i4", "", Variant::POOL_INT_ARRAY, PROPERTY_HINT_NONE },
		{ "u2", "", Variant::POOL_INT_ARRAY, PROPERTY_HINT_NONE },
		{ "u3", "", Variant::POOL_INT_ARRAY, PROPERTY_HINT_NONE },
		{ "u4", "", Variant::POOL_INT_ARRAY, PROPERTY_HINT_NONE },
		{ "uv_offset", "", Variant::VECTOR2, PROPERTY_HINT_NONE },
		{ "direction", "", Variant::VECTOR3, PROPERTY_HINT_NONE },
		{ "tint", "", Variant::COLOR, PROPERTY_HINT_NONE },
		{ "plane", "", Variant::PLANE, PROPERTY_HINT_NONE },
		{ "m2", "", Variant::TRANSFORM2D, PROPERTY_HINT_NONE },
		{ "m3", "", Variant::BASIS, PROPERTY_HINT_NONE },
		{ "m4", "", Variant::TRANSFORM, PROPERTY_HINT_NONE },
		{ "albedo_texture", "Texture", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE },
		{ "cube", "CubeMap", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE },
		{ "layers", "TextureArray", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE },
		{ "volume", "Texture3D", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE },
	};
	FIXED_FUNCTION_MATERIAL_CHECK(parameters.front()->get().name == "sentinel");
	List<PropertyInfo>::Element *entry = parameters.front()->next();
	for (unsigned int i = 0; i < sizeof(expected) / sizeof(expected[0]); i++, entry = entry->next()) {
		FIXED_FUNCTION_MATERIAL_CHECK(entry);
		const PropertyInfo &info = entry->get();
		FIXED_FUNCTION_MATERIAL_CHECK(info.name == expected[i].name && info.type == expected[i].type && info.hint == expected[i].hint && info.hint_string == expected[i].hint_string);
	}
	FIXED_FUNCTION_MATERIAL_CHECK(!entry && double(p_storage.material_get_param(material, "gain")) == 0.5);
	const String types[] = { "canvas_item", "spatial", "particles" };
	for (int i = 0; i < 3; i++) {
		FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_set_code(shader, "shader_type " + types[i] + "; uniform float amount = 0.125;") == OK);
		parameters.clear();
		FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_get_param_list(shader, &parameters) == OK && parameters.size() == 1);
		FIXED_FUNCTION_MATERIAL_CHECK(parameters.front()->get().name == "amount" && parameters.front()->get().type == Variant::REAL);
		FIXED_FUNCTION_MATERIAL_CHECK(double(p_storage.material_get_param_default(material, "amount")) == 0.125);
	}
	// Invalid queries and invalid latest code must not clear or partially append to output.
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_get_param_list(RID(), &parameters) == ERR_INVALID_PARAMETER && parameters.size() == 1);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_get_param_list(shader, nullptr) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_set_code(shader, "shader_type canvas_item; uniform float broken = ;") == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_get_param_list(shader, &parameters) == ERR_PARSE_ERROR && parameters.size() == 1);
	FIXED_FUNCTION_MATERIAL_CHECK(parameters.front()->get().name == "amount");
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_get_param_default(material, "amount").get_type() == Variant::NIL);
	const RID texture = p_textures.texture_create();
	FIXED_FUNCTION_MATERIAL_CHECK(!p_textures.texture_can_be_referenced(RID()) && !p_textures.texture_can_be_referenced(material));
	FIXED_FUNCTION_MATERIAL_CHECK(p_textures.texture_can_be_referenced(texture));
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_set_default_texture_param(shader, "albedo_texture", texture) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_get_default_texture_param(shader, "albedo_texture") == texture);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_set_code(shader, "shader_type canvas_item; render_mode blend_add;") == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_get_default_texture_param(shader, "albedo_texture") == texture);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_set_default_texture_param(shader, "albedo_texture", RID()) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(!p_storage.shader_get_default_texture_param(shader, "albedo_texture").is_valid());
	const String define = "#define FF_TEST 1\n";
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_add_custom_define(shader, define) == OK && p_storage.shader_add_custom_define(shader, define) == OK);
	Vector<String> defines;
	defines.push_back("sentinel");
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_get_custom_defines(shader, &defines) == OK && defines.size() == 3);
	FIXED_FUNCTION_MATERIAL_CHECK(defines[0] == "sentinel" && defines[1] == define && defines[2] == define);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_get_custom_defines(shader, nullptr) == ERR_INVALID_PARAMETER);
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_get_canvas_parameters(material, state) == ERR_UNAVAILABLE);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_remove_custom_define(shader, define) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_get_canvas_parameters(material, state) == ERR_UNAVAILABLE);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.shader_remove_custom_define(shader, define) == OK && p_storage.shader_remove_custom_define(shader, define) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.material_get_canvas_parameters(material, state) == OK && state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD);
	FIXED_FUNCTION_MATERIAL_CHECK(!p_storage.is_shader_async_hidden_forbidden());
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.set_shader_async_hidden_forbidden(true) == OK && p_storage.is_shader_async_hidden_forbidden());
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.set_shader_async_hidden_forbidden(false) == OK && !p_storage.is_shader_async_hidden_forbidden());
	FIXED_FUNCTION_MATERIAL_CHECK(p_textures.request_free(texture) == OK && !p_textures.texture_can_be_referenced(texture));
	FIXED_FUNCTION_MATERIAL_CHECK(p_textures.update_pending() == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_storage.free(shader) == OK && p_storage.free(material) == OK);
	printf("PASS: shader metadata ordering/types/hints, three shader modes, latest/default CPU values, bindings, defines, compile policy and closing textures.\n");
	return true;
}

bool TestFixedFunctionMaterialStorage::_test_properties(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer) {
	FixedFunctionMaterialStorage &storage = p_context.get_material_storage();
	const RID shader = storage.shader_create();
	const RID unsupported_shader = storage.shader_create();
	const RID first = storage.material_create();
	const RID second = storage.material_create();
	const RID extra_pass = storage.material_create();
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_get_render_priority(first) == 0 && storage.material_get_line_width(first) == 1.0f && !storage.material_get_next_pass(first).is_valid());
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_render_priority(first, 127) == OK && storage.material_set_render_priority(second, -128) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_get_render_priority(first) == 127 && storage.material_get_render_priority(second) == -128);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_render_priority(first, 128) == ERR_INVALID_PARAMETER && storage.material_get_render_priority(first) == 127);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_render_priority(second, -129) == ERR_INVALID_PARAMETER && storage.material_get_render_priority(second) == -128);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_line_width(first, -2.0f) == OK && storage.material_get_line_width(first) == -2.0f);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_line_width(first, std::numeric_limits<float>::quiet_NaN()) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_line_width(first, std::numeric_limits<float>::infinity()) == ERR_INVALID_PARAMETER && storage.material_get_line_width(first) == -2.0f);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_line_width(second, 16.0f) == OK && storage.material_get_line_width(second) == 16.0f);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_next_pass(first, extra_pass) == OK && storage.material_get_next_pass(first) == extra_pass);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_next_pass(first, first) == ERR_CYCLIC_LINK && storage.material_get_next_pass(first) == extra_pass);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_next_pass(extra_pass, first) == ERR_CYCLIC_LINK && !storage.material_get_next_pass(extra_pass).is_valid());
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_next_pass(first, shader) == ERR_INVALID_PARAMETER && storage.material_get_next_pass(first) == extra_pass);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_render_priority(RID(), 0) == ERR_INVALID_PARAMETER && storage.material_set_line_width(shader, 1) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.shader_set_code(shader, "shader_type canvas_item; render_mode blend_disabled;") == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.shader_set_code(unsupported_shader, "shader_type canvas_item; void fragment() { COLOR = vec4(0.0, 1.0, 0.0, 1.0); }") == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_shader(first, shader) == OK && storage.material_set_shader(second, shader) == OK && storage.material_set_shader(extra_pass, unsupported_shader) == OK);
	// Canvas order/geometry uses the server commands; these properties govern 3D passes.
	RasterizerCanvas::Item items[2];
	items[0].next = &items[1];
	items[0].material = first;
	items[1].material = second;
	for (int i = 0; i < 2; i++) {
		RasterizerCanvas::Item::CommandRect *rect = memnew(RasterizerCanvas::Item::CommandRect);
		rect->rect = Rect2(0, 0, 16, 16);
		rect->modulate = i ? Color(0, 0, 1) : Color(1, 0, 0);
		items[i].commands.push_back(rect);
	}
	FIXED_FUNCTION_MATERIAL_CHECK(p_manager.begin_frame(0) == OK && p_renderer.render_items(items, Color(1, 1, 1)) == OK && p_manager.end_frame(false) == OK);
	std::vector<uint32_t> pixels(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_MATERIAL_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK && pixels[CAPTURE_WIDTH + 1] == 0xff0000ff);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.free(extra_pass) == OK && !storage.material_get_next_pass(first).is_valid());
	RID chain[64];
	for (int i = 0; i < 64; i++) {
		chain[i] = storage.material_create();
		if (i) {
			FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_next_pass(chain[i - 1], chain[i]) == OK);
		}
	}
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_next_pass(chain[63], chain[0]) == ERR_CYCLIC_LINK && !storage.material_get_next_pass(chain[63]).is_valid());
	FIXED_FUNCTION_MATERIAL_CHECK(storage.free(chain[32]) == OK && !storage.material_get_next_pass(chain[31]).is_valid());
	for (int i = 0; i < 64; i++) {
		if (i != 32) {
			FIXED_FUNCTION_MATERIAL_CHECK(storage.free(chain[i]) == OK);
		}
	}
	FIXED_FUNCTION_MATERIAL_CHECK(storage.free(first) == OK && storage.free(second) == OK && storage.free(shader) == OK && storage.free(unsupported_shader) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.get_material_count() == 0 && storage.get_shader_count() == 0);
	printf("PASS: common material properties, finite line widths, priority bounds, acyclic next passes, detachment and unchanged canvas ordering.\n");
	return true;
}

bool TestFixedFunctionMaterialStorage::_test_rejections(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer) {
	FixedFunctionMaterialStorage &storage = p_context.get_material_storage();
	const RID shader = storage.shader_create();
	const RID material = storage.material_create();
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_shader(material, shader) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.shader_set_code(shader, "shader_type canvas_item; render_mode blend_add;") == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters previous;
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_get_canvas_parameters(material, previous) == OK && previous.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD);
	const String unsupported[] = {
		_get_preset_code(CanvasItemMaterial::BLEND_MODE_SUB, CanvasItemMaterial::LIGHT_MODE_NORMAL),
		_get_preset_code(CanvasItemMaterial::BLEND_MODE_MIX, CanvasItemMaterial::LIGHT_MODE_LIGHT_ONLY),
		_get_preset_code(CanvasItemMaterial::BLEND_MODE_MIX, CanvasItemMaterial::LIGHT_MODE_NORMAL, true),
		"shader_type canvas_item; void fragment() { COLOR = vec4(1.0); }",
		"shader_type canvas_item; uniform float amount = 1.0;",
		"shader_type canvas_item; render_mode skip_vertex_transform;",
		"shader_type spatial; render_mode unshaded;",
	};
	for (unsigned int i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); i++) {
		FIXED_FUNCTION_MATERIAL_CHECK(storage.shader_set_code(shader, unsupported[i]) == OK && storage.shader_get_code(shader) == unsupported[i]);
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_ADD;
		FIXED_FUNCTION_MATERIAL_CHECK(storage.material_get_canvas_parameters(material, state) == ERR_UNAVAILABLE);
		FIXED_FUNCTION_MATERIAL_CHECK(state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD);
	}
	const String invalid = "shader_type canvas_item; render_mode this_is_invalid;";
	FIXED_FUNCTION_MATERIAL_CHECK(storage.shader_set_code(shader, invalid) == OK && storage.shader_get_code(shader) == invalid);
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_get_canvas_parameters(material, state) == ERR_PARSE_ERROR);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.shader_set_code(shader, String()) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.material_get_canvas_parameters(material, state) == ERR_UNCONFIGURED);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.shader_set_code(shader, "shader_type canvas_item; render_mode blend_mix;") == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.shader_add_custom_define(shader, "#define FF_TEST 1\n") == OK);
	RasterizerCanvas::Item first;
	RasterizerCanvas::Item rejected;
	first.next = &rejected;
	rejected.material = material;
	RasterizerCanvas::Item::CommandRect *rect = memnew(RasterizerCanvas::Item::CommandRect);
	rect->rect = Rect2(0, 0, 16, 16);
	rect->modulate = Color(1, 0, 1);
	first.commands.push_back(rect);
	RasterizerCanvas::Item::CommandRect *fallback_rect = memnew(RasterizerCanvas::Item::CommandRect);
	fallback_rect->rect = Rect2(16, 0, 16, 16);
	fallback_rect->modulate = Color(0, 1, 0);
	rejected.commands.push_back(fallback_rect);
	const RID target = p_context.get_texture_storage().render_target_create();
	FIXED_FUNCTION_MATERIAL_CHECK(p_context.get_texture_storage().render_target_set_size(target, 32, 32) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_manager.begin_frame(0) == OK);
	if (p_context.get_device().get_capabilities().render_target_opaque) {
		FIXED_FUNCTION_MATERIAL_CHECK(p_manager.select_render_target(target) == OK);
	}
	FIXED_FUNCTION_MATERIAL_CHECK(p_renderer.render_items(&first, Color(1, 1, 1)) == OK);
	if (p_context.get_device().get_capabilities().render_target_opaque) {
		FIXED_FUNCTION_MATERIAL_CHECK(p_manager.select_render_target(RID()) == OK && p_manager.blit_to_screen(target, Rect2(0, 0, 32, 32)) == OK);
		FIXED_FUNCTION_MATERIAL_CHECK(p_context.get_texture_storage().render_target_is_ready(target));
	}
	FIXED_FUNCTION_MATERIAL_CHECK(p_manager.end_frame(false) == OK);
	std::vector<uint32_t> fallback_pixels(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_MATERIAL_CHECK(p_context.get_device().read_frame(fallback_pixels.data(), fallback_pixels.size()) == OK);
	const uint32_t magenta = fallback_pixels[8 * CAPTURE_WIDTH + 8];
	const uint32_t green = fallback_pixels[8 * CAPTURE_WIDTH + 24];
	// Drawing and blitting the viewport can compound fixed-point rounding on D3D7 RGB.
	const bool colors_match = (magenta & 0xff00ff00) == 0xff000000 && ((magenta >> 16) & 255) >= 252 && (magenta & 255) >= 252 &&
			(green & 0xffff00ff) == 0xff000000 && ((green >> 8) & 255) >= 252;
	if (!colors_match) {
		printf("Canvas fallback pixels: %08x %08x\n", unsigned(magenta), unsigned(green));
	}
	FIXED_FUNCTION_MATERIAL_CHECK(colors_match);
	FIXED_FUNCTION_MATERIAL_CHECK(p_context.get_texture_storage().free(target) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(storage.free(shader) == OK && storage.free(material) == OK);
	return true;
}

bool TestFixedFunctionMaterialStorage::_render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer, std::vector<uint32_t> &r_pixels) {
	FixedFunctionMaterialStorage &storage = p_context.get_material_storage();
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = p_context.get_device().get_capabilities();
	const bool supported[] = { caps.blend_mix, caps.blend_add, caps.blend_mul, caps.blend_premultiplied_alpha, true };
	const int modes[] = { CanvasItemMaterial::BLEND_MODE_MIX, CanvasItemMaterial::BLEND_MODE_ADD, CanvasItemMaterial::BLEND_MODE_MUL, CanvasItemMaterial::BLEND_MODE_PREMULT_ALPHA, CanvasItemMaterial::BLEND_MODE_DISABLED };
	const uint32_t expected[] = { 0xff504040, 0xff606070, 0xff10100c, 0xff906050, 0xff804020 };
	RasterizerCanvas::Item items[10];
	RasterizerCanvas::Item owners[5];
	for (int i = 0; i < 10; i++) {
		const RID shader = storage.shader_create();
		const RID material = storage.material_create();
		const String code = _get_preset_code(modes[i % 5], i < 5 ? CanvasItemMaterial::LIGHT_MODE_NORMAL : CanvasItemMaterial::LIGHT_MODE_UNSHADED);
		FIXED_FUNCTION_MATERIAL_CHECK(!code.empty() && storage.shader_set_code(shader, code) == OK && storage.material_set_shader(material, shader) == OK);
		items[i].next = i < 9 ? &items[i + 1] : nullptr;
		if (i < 5) {
			items[i].material = material;
		} else {
			owners[i - 5].material = material;
			items[i].material_owner = &owners[i - 5];
			items[i].material = shader; // A parent material overrides even this foreign local RID.
		}
		if (!supported[i % 5]) {
			continue;
		}
		RasterizerCanvas::Item::CommandRect *rect = memnew(RasterizerCanvas::Item::CommandRect);
		rect->rect = Rect2((i % 5) * 32, i < 5 ? 0 : 48, 32, 48);
		rect->modulate = Color(128.0f / 255.0f, 64.0f / 255.0f, 32.0f / 255.0f, 128.0f / 255.0f);
		items[i].commands.push_back(rect);
	}
	FIXED_FUNCTION_MATERIAL_CHECK(p_manager.begin_frame(0) == OK && p_manager.clear_render_target(Color(32.0f / 255.0f, 64.0f / 255.0f, 96.0f / 255.0f)) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_renderer.render_items(items, Color(1, 1, 1)) == OK && p_manager.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_MATERIAL_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			const uint32_t reference = supported[x / 32] ? expected[x / 32] : 0xff204060;
			const uint32_t actual = r_pixels[size_t(y) * CAPTURE_WIDTH + x];
			FIXED_FUNCTION_MATERIAL_CHECK((actual >> 24) == 255);
			for (int shift = 0; shift < 24; shift += 8) {
				const int delta = int((actual >> shift) & 255) - int((reference >> shift) & 255);
				FIXED_FUNCTION_MATERIAL_CHECK(delta >= -12 && delta <= 12);
			}
		}
	}
	storage.clear();
	return true;
}

bool TestFixedFunctionMaterialStorage::_test_antialiased_lines(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_renderer, std::vector<uint32_t> &r_pixels) {
	FixedFunctionMaterialStorage &storage = p_context.get_material_storage();
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = p_context.get_device().get_capabilities();
	const bool supported[] = { caps.blend_mix, caps.blend_add, caps.blend_mul, caps.blend_premultiplied_alpha };
	const int modes[] = { CanvasItemMaterial::BLEND_MODE_MIX, CanvasItemMaterial::BLEND_MODE_ADD, CanvasItemMaterial::BLEND_MODE_MUL, CanvasItemMaterial::BLEND_MODE_PREMULT_ALPHA };
	const uint32_t center[] = { 0xff504040, 0xff606070, 0xff10100c, 0xff906050 };
	const uint32_t fringe[] = { 0xff384050, 0xff405068, 0xff182836, 0xff585058 };
	RasterizerCanvas::Item items[4];
	for (int i = 0; i < 4; i++) {
		items[i].next = i < 3 ? &items[i + 1] : nullptr;
		if (!supported[i]) {
			continue;
		}
		const RID shader = storage.shader_create();
		items[i].material = storage.material_create();
		FIXED_FUNCTION_MATERIAL_CHECK(storage.shader_set_code(shader, _get_preset_code(modes[i], CanvasItemMaterial::LIGHT_MODE_NORMAL)) == OK);
		FIXED_FUNCTION_MATERIAL_CHECK(storage.material_set_shader(items[i].material, shader) == OK);
		RasterizerCanvas::Item::CommandLine *line = memnew(RasterizerCanvas::Item::CommandLine);
		line->from = Vector2(i * 32 + 4, 16);
		line->to = Vector2(i * 32 + 28, 16);
		line->width = 3;
		line->antialiased = true;
		line->color = Color(128.0f / 255.0f, 64.0f / 255.0f, 32.0f / 255.0f, 128.0f / 255.0f);
		items[i].commands.push_back(line);
	}
	FIXED_FUNCTION_MATERIAL_CHECK(p_manager.begin_frame(0) == OK && p_manager.clear_render_target(Color(32.0f / 255.0f, 64.0f / 255.0f, 96.0f / 255.0f)) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(p_renderer.render_items(items, Color(1, 1, 1)) == OK && p_manager.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_MATERIAL_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 3; j++) {
			uint32_t expected = 0xff204060;
			if (supported[i] && j < 2) {
				expected = j == 0 ? center[i] : fringe[i];
			}
			const uint32_t actual = r_pixels[size_t(15 - j) * CAPTURE_WIDTH + i * 32 + 16];
			for (int shift = 0; shift < 24; shift += 8) {
				const int delta = int((actual >> shift) & 255) - int((expected >> shift) & 255);
				FIXED_FUNCTION_MATERIAL_CHECK(delta >= -8 && delta <= 8);
			}
		}
	}
	storage.clear();
	printf("PASS: antialiased line coverage with mix/add/multiply/premultiplied materials.\n");
	return true;
}

bool TestFixedFunctionMaterialStorage::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	RasterizerStorage *const singleton = RasterizerStorage::base_singleton;
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FixedFunctionRenderTargetManager manager(context);
	FixedFunctionCanvasItemRenderer renderer(context, manager);
	FIXED_FUNCTION_MATERIAL_CHECK(context.initialize() == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(_test_ownership(context.get_material_storage()));
	FIXED_FUNCTION_MATERIAL_CHECK(_test_shader_metadata(context.get_material_storage(), context.get_texture_storage()));
	FIXED_FUNCTION_MATERIAL_CHECK(_test_properties(context, manager, renderer));
	FIXED_FUNCTION_MATERIAL_CHECK(_test_rejections(context, manager, renderer));
	FIXED_FUNCTION_MATERIAL_CHECK(_test_antialiased_lines(context, manager, renderer, r_pixels));
	FIXED_FUNCTION_MATERIAL_CHECK(_render_and_check(context, manager, renderer, r_pixels));
	const std::vector<uint32_t> capture = r_pixels;
	FIXED_FUNCTION_MATERIAL_CHECK(context.resize(192, 128) == OK && context.prepare_resources() == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(_render_and_check(context, manager, renderer, r_pixels) && r_pixels == capture);
	const RID shader = context.get_material_storage().shader_create();
	const RID material = context.get_material_storage().material_create();
	FIXED_FUNCTION_MATERIAL_CHECK(context.get_material_storage().material_set_shader(material, shader) == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(context.finalize() == OK);
	FIXED_FUNCTION_MATERIAL_CHECK(context.get_material_storage().get_shader_count() == 0 && context.get_material_storage().get_material_count() == 0);
	FIXED_FUNCTION_MATERIAL_CHECK(RasterizerStorage::base_singleton == singleton);
	printf("PASS: CPU shader/material ownership, actual CanvasItemMaterial emission, supported blend pixels, inherited materials, latest-code rejection, parameter readback, release and recovery.\n");
	return true;
}

#undef FIXED_FUNCTION_MATERIAL_CHECK
