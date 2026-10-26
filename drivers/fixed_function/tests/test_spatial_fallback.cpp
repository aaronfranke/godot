#include "drivers/fixed_function/tests/test_preset_scope.h"
#include "drivers/fixed_function/tests/test_unlit_scene.h"
#include "scene/resources/material.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>
#include <string.h>

#include <limits>

#define FF_FALLBACK_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Fallback FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionUnlitScene::_test_fallbacks(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	TestFixedFunctionPresetScope scope(p_context.get_device(), nullptr);
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
	FF_FALLBACK_CHECK(materials.material_get_spatial_draw_parameters(RID(), parameters) == OK && parameters.albedo.factor == Color(1, 1, 1) && parameters.state.depth_test_enabled);
	FF_FALLBACK_CHECK(materials.material_get_spatial_draw_parameters(material, parameters) == OK && parameters.albedo.factor == Color(1, 1, 1));
	FF_FALLBACK_CHECK(materials.material_set_shader(material, shader) == OK);
	FF_FALLBACK_CHECK(materials.shader_set_code(shader, String()) == OK && materials.material_get_spatial_draw_parameters(material, parameters) == OK && parameters.albedo.factor == Color(1, 1, 1));
	const RID mesh = _mesh(p_context.get_mesh_storage());
	FFInstance instance(p_context, mesh, material);
	instance.transform.origin.z = -3;
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	std::vector<uint32_t> pixels;
	const String unknown = "shader_type spatial; void fragment(){ ALBEDO=vec3(sin(UV.x)); }";
	FF_FALLBACK_CHECK(materials.shader_set_code(shader, unknown) == OK);
	FF_FALLBACK_CHECK(materials.material_get_spatial_parameters(material, parameters) == ERR_UNAVAILABLE);
	FF_FALLBACK_CHECK(materials.material_get_spatial_draw_parameters(material, parameters) == OK && parameters.is_fallback && parameters.albedo.factor == Color(1, 0, 1));
	FF_FALLBACK_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xffff00ff));
	const String partial = "shader_type spatial; uniform vec4 tint : source_color=vec4(0,1,1,1); void vertex(){ VERTEX.x+=sin(TIME); } void fragment(){ ALBEDO=tint.rgb; EMISSION=vec3(1); }";
	FF_FALLBACK_CHECK(materials.shader_set_code(shader, partial) == OK && materials.material_get_spatial_draw_parameters(material, parameters) == OK && parameters.is_fallback && parameters.albedo.factor == Color(0, 1, 1));
	FF_FALLBACK_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xff00ffff));
	FF_FALLBACK_CHECK(materials.material_set_param(material, "tint", Color(1, 0, 0)) == OK && _draw(p_context, p_targets, p_renderer, instance, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xffff0000));
	FF_FALLBACK_CHECK(materials.material_set_param(material, "tint", Color(std::numeric_limits<float>::quiet_NaN(), 1, 1)) == OK);
	FF_FALLBACK_CHECK(materials.material_get_spatial_draw_parameters(material, parameters) == OK && parameters.albedo.factor == Color(1, 0, 1));
	FF_FALLBACK_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xffff00ff));
	FF_FALLBACK_CHECK(Math::is_nan(Color(materials.material_get_param(material, "tint")).r));
	FF_FALLBACK_CHECK(materials.material_set_param(material, "tint", Variant()) == OK);
	const String supported = "shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=vec3(0,1,0); }";
	FF_FALLBACK_CHECK(materials.shader_set_code(shader, supported) == OK && materials.material_get_spatial_draw_parameters(material, parameters) == OK && !parameters.is_fallback);
	FF_FALLBACK_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xff00ff00));
	// Malformed source must not retain the last successfully cached color.
	FF_FALLBACK_CHECK(materials.shader_set_code(shader, "shader_type spatial; invalid syntax") == OK);
	FF_FALLBACK_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xffff00ff));
	FF_FALLBACK_CHECK(materials.shader_set_code(shader, supported) == OK && materials.material_set_next_pass(material, materials.material_create()) == OK);
	instance.data->material_overlay = materials.material_create();
	FF_FALLBACK_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xff00ff00));
	// Line helpers retain supported triangle output in the same viewport.
	const RID lines = p_context.get_mesh_storage().mesh_create();
	RenderingServerTypes::SurfaceData line_data;
	line_data.format |= RSE::ARRAY_FORMAT_VERTEX;
	line_data.primitive = RSE::PRIMITIVE_LINES;
	line_data.vertex_count = 2;
	line_data.vertex_data.resize(24);
	const float line_positions[] = { -1, 0, 0, 1, 0, 0 };
	memcpy(line_data.vertex_data.ptrw(), line_positions, sizeof(line_positions));
	line_data.aabb = AABB(Vector3(-1, 0, 0), Vector3(2, 0, 0));
	FF_FALLBACK_CHECK(p_context.get_mesh_storage().mesh_add_surface(lines, line_data) == OK);
	FFInstance helper(p_context, lines, material);
	helper.transform.origin.z = -2;
	RenderGeometryInstance *instances[] = { &helper, &instance };
	FF_FALLBACK_CHECK(p_targets.begin_frame(0) == OK && _render(p_renderer, projection, instances, 2) == OK && p_targets.end_frame(false) == OK);
	FF_FALLBACK_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xff00ff00));
	// Bounded lit presets lower directly and retain their albedo/UV data.
	FF_FALLBACK_CHECK(materials.material_set_next_pass(material, RID()) == OK);
	instance.data->material_overlay = RID();
	Ref<StandardMaterial3D> preset;
	preset.instantiate();
	BaseMaterial3D::flush_changes();
	const String code = RenderingServer::get_singleton()->shader_get_code(preset->get_shader_rid());
	FF_FALLBACK_CHECK(materials.shader_set_code(shader, code) == OK && materials.material_set_param(material, "albedo", Color(0.25, 0.5, 0.75)) == OK);
	FF_FALLBACK_CHECK(materials.material_set_param(material, "uv1_scale", Vector3(2, 3, 1)) == OK && materials.material_set_param(material, "uv1_offset", Vector3(0.25, 0.5, 0)) == OK);
	FF_FALLBACK_CHECK(materials.material_get_spatial_draw_parameters(material, parameters) == OK && !parameters.is_fallback && !parameters.lighting_flags && parameters.albedo.factor == Color(0.25, 0.5, 0.75) && parameters.albedo.uv_scale == Vector2(2, 3) && parameters.albedo.uv_offset == Vector2(0.25, 0.5));
	FF_FALLBACK_CHECK(materials.shader_get_code(shader) == code);
	// A failed producer or released image cannot turn an unsupported shader into missing geometry.
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID target = textures.render_target_create();
	const RID target_texture = textures.render_target_get_texture(target);
	FF_FALLBACK_CHECK(textures.render_target_set_size(target, 2, 2) == OK && textures.render_target_set_failed(target, true) == OK);
	FF_FALLBACK_CHECK(materials.shader_set_code(shader, "shader_type spatial; uniform sampler2D image : filter_nearest, repeat_disable; void fragment(){ ALBEDO=texture(image,UV).rgb; ROUGHNESS=sin(TIME); }") == OK && materials.material_set_param(material, "image", target_texture) == OK);
	FF_FALLBACK_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xffffffff));
	FF_FALLBACK_CHECK(textures.free(target) == OK && _draw(p_context, p_targets, p_renderer, instance, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xffffffff));
	instance.data->material_override = RID();
	FF_FALLBACK_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xffffffff));
	FF_FALLBACK_CHECK(p_context.get_mesh_storage().free(mesh) == OK && p_context.get_mesh_storage().free(lines) == OK);
	FF_FALLBACK_CHECK(materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: visible spatial fallbacks, recovered albedo/defaults/UVs, magenta/reset, live edits, white default materials and editor helper isolation.\n");
	return true;
}

#undef FF_FALLBACK_CHECK
