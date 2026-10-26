#include "drivers/fixed_function/tests/test_lambert.h"

#include <stdio.h>

#define FF_SKY_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Sky ambient FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionLambert::_test_sky_ambient(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionSceneRenderer::FFSkyBackground sky;
	sky.panorama = textures.texture_create();
	Color average;
	FF_SKY_CHECK(!p_renderer.get_sky_ambient(sky, average));
	Ref<Image> image = Image::create_empty(8, 8, false, Image::FORMAT_RGB8);
	image->fill(Color(0, 1, 0));
	for (int x = 0; x < 8; x++) {
		image->set_pixel(x, 0, Color(1, 0, 0));
		image->set_pixel(x, 7, Color(1, 0, 0));
	}
	FF_SKY_CHECK(textures.texture_set_data(sky.panorama, image) == OK && p_renderer.get_sky_ambient(sky, average));
	FF_SKY_CHECK(_color(average.to_argb32(), 0xff13ec00));

	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	const RID mesh = _mesh(meshes, 0, true);
	FF_SKY_CHECK(mesh.is_valid() && materials.material_set_shader(material, shader) == OK);
	FF_SKY_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1); }") == OK);
	FFInstance instance(p_context, mesh, material);
	FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	environment.ambient = Color(0, 0, 1);
	environment.ambient_energy = 0.5f;
	environment.sky_ambient = average;
	environment.sky_energy = 0.5f;
	environment.sky_contribution = 0.5f;
	std::vector<uint32_t> pixels;
	FF_SKY_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, Vector<RID>(), pixels) && _color(pixels[48 * 128 + 64], 0xff053b40));
	environment.exposure_normalization = 0.5f;
	FF_SKY_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, Vector<RID>(), pixels) && _color(pixels[48 * 128 + 64], 0xff021e20));

	image->fill(Color(1, 0, 0));
	FF_SKY_CHECK(textures.texture_request_data(sky.panorama, image) == OK && p_renderer.get_sky_ambient(sky, average) && average == Color(1, 0, 0));
	FF_SKY_CHECK(textures.free(sky.panorama) == OK);
	sky.panorama = RID();
	sky.procedural = true;
	sky.top = sky.horizon = Color(1, 0, 0);
	sky.ground = sky.ground_horizon = Color(0, 0, 1);
	FF_SKY_CHECK(p_renderer.get_sky_ambient(sky, average) && average.is_equal_approx(Color(0.5, 0, 0.5)));
	FF_SKY_CHECK(meshes.free(mesh) == OK && materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: panorama ambient weighting, procedural gradient, deferred cache updates, separate energies and exposure.\n");
	return true;
}

#undef FF_SKY_CHECK
