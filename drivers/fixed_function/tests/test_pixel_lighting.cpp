#include "test_pixel_lighting.h"

#include "core/config/project_settings.h"
#include "drivers/fixed_function/tests/test_lambert.h"

#include <stdio.h>

#define FF_PIXEL_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Pixel lighting FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionPixelLighting::_test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	const RID shader = materials.shader_create(), material = materials.material_create();
	const String source = "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1); }";
	FF_PIXEL_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, source) == OK);
	const RID mesh = TestFixedFunctionLambert::_mesh(meshes, 0, true);
	const RID gradient = TestFixedFunctionLambert::_mesh(meshes, 0, true, Vector3(), false, false, true);
	TestFixedFunctionLambert::FFInstance instance(p_context, gradient, material);
	FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	environment.ambient = Color(0.1, 0.1, 0.1);
	const RID directional = lights.light_create(RSE::LIGHT_DIRECTIONAL), directional_instance = lights.light_instance_create(directional);
	Vector<RID> active;
	active.push_back(directional_instance);
	std::vector<uint32_t> pixels;
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	const uint32_t pixel_color = pixels[48 * 128 + 64];
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_color(pixel_color, 0xff1a1a1a));
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/per_pixel_lighting", false);
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(int(pixels[48 * 128 + 64] & 255) > int(pixel_color & 255) + 40);
	const uint32_t vertex_color = pixels[48 * 128 + 64];
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/per_pixel_lighting", true);
	FF_PIXEL_CHECK(materials.shader_set_code(shader, source.replace("cull_disabled", "cull_disabled,vertex_lighting")) == OK);
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(pixels[48 * 128 + 64] == vertex_color);
	FF_PIXEL_CHECK(materials.shader_set_code(shader, source) == OK);
	instance.data->base = mesh;
	environment.ambient = Color(0, 0, 0);
	const RID omni = lights.light_create(RSE::LIGHT_OMNI), omni_instance = lights.light_instance_create(omni);
	FixedFunctionLightStorage::FFLightData data;
	FF_PIXEL_CHECK(lights.light_get_data(omni, data) == OK);
	data.parameters[RSE::LIGHT_PARAM_RANGE] = 4;
	data.parameters[RSE::LIGHT_PARAM_ATTENUATION] = 1;
	FF_PIXEL_CHECK(lights.light_set_data(omni, data) == OK);
	active.clear();
	active.push_back(omni_instance);
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_color(pixels[48 * 128 + 64], 0xff808080));
	FF_PIXEL_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(2,0,0); }") == OK);
	data.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.25;
	FF_PIXEL_CHECK(lights.light_set_data(omni, data) == OK);
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_color(pixels[48 * 128 + 64], 0xff400000));
	data.parameters[RSE::LIGHT_PARAM_ENERGY] = 1;
	FF_PIXEL_CHECK(lights.light_set_data(omni, data) == OK && materials.shader_set_code(shader, source) == OK);
	Basis camera_basis(Vector3(0, 0, 1), real_t(0.3));
	camera_basis.scale(Vector3(-2, 3, real_t(0.5)));
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels, Transform3D(camera_basis, Vector3())));
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_color(pixels[48 * 128 + 64], 0xff808080));
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID texture = textures.texture_create();
	Ref<Image> image;
	image.instantiate();
	image->initialize_data(1, 1, false, Image::FORMAT_RGBA8);
	image->fill(Color(0.5, 0.25, 1, 1));
	FF_PIXEL_CHECK(textures.texture_set_data(texture, image) == OK);
	FF_PIXEL_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; uniform sampler2D image; void fragment(){ ALBEDO=texture(image,UV).rgb; }") == OK && materials.material_set_param(material, "image", texture) == OK);
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_color(pixels[48 * 128 + 64], 0xff402080));
	FF_PIXEL_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.5; }") == OK);
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_color(pixels[48 * 128 + 64], 0xff400080));
	FF_PIXEL_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.5; ALPHA_SCISSOR_THRESHOLD=0.5; }") == OK);
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_color(pixels[48 * 128 + 64], 0xff800000));
	FF_PIXEL_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.5; ALPHA_SCISSOR_THRESHOLD=0.6; }") == OK);
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(pixels[48 * 128 + 64] == 0xff0000ff);
	FF_PIXEL_CHECK(materials.shader_set_code(shader, source) == OK);
	// Nine overlapping lights: retain per-pixel evaluation and omit the weakest.
	Vector<RID> sources, instances;
	active.clear();
	for (int i = 0; i < 9; i++) {
		const RID light = lights.light_create(RSE::LIGHT_OMNI), light_instance = lights.light_instance_create(light);
		FF_PIXEL_CHECK(lights.light_get_data(light, data) == OK);
		data.parameters[RSE::LIGHT_PARAM_RANGE] = 4;
		data.parameters[RSE::LIGHT_PARAM_ATTENUATION] = 1;
		data.parameters[RSE::LIGHT_PARAM_ENERGY] = i == 8 ? 0.001f : 0.125f;
		FF_PIXEL_CHECK(lights.light_set_data(light, data) == OK);
		sources.push_back(light);
		instances.push_back(light_instance);
		active.push_back(light_instance);
	}
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_color(pixels[48 * 128 + 64], 0xff808080));
	FF_PIXEL_CHECK(lights.light_get_data(sources[8], data) == OK);
	data.flags |= FixedFunctionLightStorage::LIGHT_NEGATIVE;
	data.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.5;
	FF_PIXEL_CHECK(lights.light_set_data(sources[8], data) == OK);
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_color(pixels[48 * 128 + 64], 0xff303030)); // (7 * .125 - .5) * .5.
	FF_PIXEL_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(128, 96) == OK && p_context.prepare_resources() == OK);
	FF_PIXEL_CHECK(p_context.get_device().get_max_per_pixel_lights() == 8);
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_PIXEL_CHECK(TestFixedFunctionLambert::_color(pixels[48 * 128 + 64], 0xff303030));
	for (int i = 0; i < sources.size(); i++) {
		FF_PIXEL_CHECK(lights.free(instances[i]) == OK && lights.free(sources[i]) == OK);
	}
	FF_PIXEL_CHECK(lights.free(directional_instance) == OK && lights.free(directional) == OK && lights.free(omni_instance) == OK && lights.free(omni) == OK);
	FF_PIXEL_CHECK(materials.free(material) == OK && materials.free(shader) == OK && textures.free(texture) == OK);
	FF_PIXEL_CHECK(meshes.free(mesh) == OK && meshes.free(gradient) == OK);
	printf("PASS: SM3 per-pixel normals/position, vertex fallback, texture/alpha/cutoff, brightest-eight signed lights and reset recovery.\n");
	return true;
}

bool TestFixedFunctionPixelLighting::test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	if (!p_context.get_device().get_max_per_pixel_lights()) {
		return true;
	}
	const bool previous = GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting");
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/per_pixel_lighting", true);
	p_renderer.set_debug_unshaded(false);
	const bool success = _test(p_context, p_targets, p_renderer);
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/per_pixel_lighting", previous);
	p_renderer.set_debug_unshaded(true);
	return success;
}

#undef FF_PIXEL_CHECK
