#include "test_lambert.h"

#include "core/project_settings.h"

#include <stdio.h>

#define FF_SKY_AMBIENT_CHECK(m_condition)                                        \
	do {                                                                         \
		if (!(m_condition)) {                                                    \
			printf("Sky ambient FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                        \
		}                                                                        \
	} while (false)

bool TestFixedFunctionLambert::_test_sky_ambient(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionSceneStorage &scenes = p_context.get_scene_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	const RID mesh = _mesh(meshes, 0, true);
	const String source = "shader_type spatial; render_mode cull_disabled; void fragment() { ALBEDO = vec3(1.0); }";
	FF_SKY_AMBIENT_CHECK(mesh.is_valid() && materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, source) == OK);
	FFInstance instance(mesh, material);

	const RID environment = scenes.environment_create();
	const RID sky = scenes.sky_create();
	const RID panorama = textures.texture_create();
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_background(environment, VS::ENV_BG_SKY) == OK);
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_sky(environment, sky) == OK && scenes.sky_set_panorama(sky, panorama) == OK);
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_ambient(environment, Color(0, 0, 0.5f), 1, 1) == OK);

	std::vector<uint32_t> pixels;
	const Vector<RID> lights;
	const auto draw_color = [&](uint32_t p_expected) {
		return _draw(p_context, p_targets, p_renderer, instance, environment, lights, pixels) && _color(pixels[48 * 128 + 64], p_expected);
	};
	FF_SKY_AMBIENT_CHECK(draw_color(0xff000080));
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_ambient(environment, Color(0, 0, 1), 1, 1) == OK);

	Ref<Image> image = memnew(Image(8, 8, false, Image::FORMAT_RGB8));
	image->fill(Color(0, 1, 0));
	image->lock();
	for (int x = 0; x < 8; x++) {
		image->set_pixel(x, 0, Color(1, 0, 0));
		image->set_pixel(x, 7, Color(1, 0, 0));
	}
	image->unlock();
	FF_SKY_AMBIENT_CHECK(textures.texture_set_data(panorama, image) == OK);
	const bool previous = GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting");
	ProjectSettings *settings = ProjectSettings::get_singleton();
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", false);
	// The two red polar bands cover about 7.6% of a sphere.
	const bool vertex_lit = draw_color(0xff13ec00);
	bool pixel_lit = true;
	if (p_context.get_device().get_max_per_pixel_lights() > 0) {
		settings->set("rendering/quality/fixed_function/per_pixel_lighting", true);
		pixel_lit = draw_color(0xff13ec00);
	}
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", previous);
	FF_SKY_AMBIENT_CHECK(vertex_lit && pixel_lit);
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_ambient(environment, Color(0, 0, 1), 1, 0) == OK && draw_color(0xff0000ff));
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_ambient(environment, Color(0, 0, 1), 0.5f, 0.5f) == OK);
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_energy(environment, 0.5f) == OK && draw_color(0xff021e40));
	FF_SKY_AMBIENT_CHECK(materials.shader_set_code(shader, source.replace("cull_disabled", "cull_disabled,ambient_light_disabled")) == OK && draw_color(0xff000000));
	FF_SKY_AMBIENT_CHECK(materials.shader_set_code(shader, source) == OK);

	image->fill(Color(1, 0, 0));
	FF_SKY_AMBIENT_CHECK(textures.texture_request_data(panorama, image) == OK);
	Color average;
	FF_SKY_AMBIENT_CHECK(textures.texture_get_panorama_average(panorama, average) && average == Color(1, 0, 0));
	FF_SKY_AMBIENT_CHECK(draw_color(0xff200040));
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_background(environment, VS::ENV_BG_COLOR_SKY) == OK && draw_color(0xff200040));
	FF_SKY_AMBIENT_CHECK(pixels.front() == 0xff0000ff);
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_background(environment, VS::ENV_BG_CLEAR_COLOR) == OK && draw_color(0xff000080));

	const RID hdr_panorama = textures.texture_create();
	image->create(8, 8, false, Image::FORMAT_RGBE9995);
	image->fill(Color(0.25f, 0.25f, 0.25f));
	FF_SKY_AMBIENT_CHECK(textures.texture_set_data(hdr_panorama, image) == OK);
	FF_SKY_AMBIENT_CHECK(scenes.sky_set_panorama(sky, hdr_panorama) == OK);
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_background(environment, VS::ENV_BG_COLOR_SKY) == OK);
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_energy(environment, 1) == OK);
	FF_SKY_AMBIENT_CHECK(scenes.environment_set_ambient(environment, Color(0, 0, 1), 1, 1) == OK && draw_color(0xff898989));
	FF_SKY_AMBIENT_CHECK(textures.free(hdr_panorama) == OK);
	FF_SKY_AMBIENT_CHECK(textures.free(panorama) == OK && scenes.free(sky) == OK && scenes.free(environment) == OK);
	FF_SKY_AMBIENT_CHECK(meshes.free(mesh) == OK && materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: sky ambient weighting, energy, Color+Sky, RGBE, shader opt-out, and deferred image updates.\n");
	return true;
}

#undef FF_SKY_AMBIENT_CHECK
