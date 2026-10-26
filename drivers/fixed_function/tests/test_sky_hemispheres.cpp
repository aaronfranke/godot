#include "test_sky_hemispheres.h"

#include <stdio.h>

#include <vector>

#define FF_HEMISPHERE_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Sky hemisphere FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionSkyHemispheres::_test_averages(FixedFunctionTextureStorage &p_textures) {
	const RID panorama = p_textures.texture_create();
	const RID proxy = p_textures.texture_create();
	FF_HEMISPHERE_CHECK(p_textures.texture_set_proxy(proxy, panorama) == OK);
	Ref<Image> image = Image::create_empty(4, 3, false, Image::FORMAT_RGB8);
	image->fill(Color(0, 1, 0));
	image->fill_rect(Rect2i(0, 0, 4, 1), Color(1, 0, 0));
	image->fill_rect(Rect2i(0, 2, 4, 1), Color(0, 0, 1));
	FF_HEMISPHERE_CHECK(p_textures.texture_request_data(panorama, image) == OK);
	Color sky, ground, average;
	FF_HEMISPHERE_CHECK(p_textures.texture_get_panorama_hemispheres(proxy, sky, ground));
	FF_HEMISPHERE_CHECK(sky.to_rgba32() == Color(0.5f, 0.5f, 0).to_rgba32());
	FF_HEMISPHERE_CHECK(ground.to_rgba32() == Color(0, 0.5f, 0.5f).to_rgba32());
	FF_HEMISPHERE_CHECK(p_textures.texture_get_panorama_average(panorama, average));
	FF_HEMISPHERE_CHECK(average.to_rgba32() == Color(0.25f, 0.5f, 0.25f).to_rgba32());
	image = Image::create_empty(4, 1, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 1, 0));
	FF_HEMISPHERE_CHECK(p_textures.texture_request_data(panorama, image) == OK);
	FF_HEMISPHERE_CHECK(p_textures.texture_get_panorama_hemispheres(proxy, sky, ground));
	FF_HEMISPHERE_CHECK(sky == Color(1, 1, 0) && ground == sky);
	image->fill(Color(0, 0, 0));
	FF_HEMISPHERE_CHECK(p_textures.texture_request_data(panorama, image) == OK);
	FF_HEMISPHERE_CHECK(p_textures.texture_get_panorama_hemispheres(proxy, sky, ground) && sky == Color() && ground == sky);
	image = Image::create_empty(4, 2, false, Image::FORMAT_RGBE9995);
	image->fill(Color(0.25f, 0.25f, 0.25f));
	image->fill_rect(Rect2i(0, 1, 4, 1), Color(0, 0, 0));
	FF_HEMISPHERE_CHECK(p_textures.texture_request_data(panorama, image) == OK);
	FF_HEMISPHERE_CHECK(p_textures.texture_get_panorama_hemispheres(proxy, sky, ground));
	FF_HEMISPHERE_CHECK(sky.r > 0.53f && sky.r < 0.55f && sky.r == sky.g && sky.g == sky.b && ground == Color());
	FF_HEMISPHERE_CHECK(p_textures.free(proxy) == OK && p_textures.free(panorama) == OK);
	return true;
}

bool TestFixedFunctionSkyHemispheres::test(FixedFunctionGraphicsDevice &p_device, void *p_window) {
	enum {
		WIDTH = 128,
		HEIGHT = 96
	};
	FixedFunctionRenderContext context(p_device, p_window, WIDTH, HEIGHT, false);
	FixedFunctionRenderTargetManager targets(context);
	FixedFunctionSceneRenderer renderer(context, targets);
	FF_HEMISPHERE_CHECK(context.initialize() == OK);
	FF_HEMISPHERE_CHECK(_test_averages(context.get_texture_storage()));
	FixedFunctionSceneRenderer::FFSkyBackground sky;
	sky.panorama = context.get_texture_storage().texture_create();
	Ref<Image> image = Image::create_empty(8, 8, false, Image::FORMAT_RGBA8);
	image->fill(Color(0, 0, 1));
	image->fill_rect(Rect2i(0, 0, 8, 4), Color(1, 0, 0));
	FF_HEMISPHERE_CHECK(context.get_texture_storage().texture_request_data(sky.panorama, image) == OK);
	Projection projection;
	projection.set_perspective(60, real_t(WIDTH) / real_t(HEIGHT), 0.1, 20);
	std::vector<uint32_t> pixels(WIDTH * HEIGHT);
	const auto draw = [&](const Transform3D &p_camera) {
		FF_HEMISPHERE_CHECK(targets.begin_frame(0) == OK && targets.clear_render_target(Color(1, 0, 1)) == OK);
		FF_HEMISPHERE_CHECK(renderer.render_sky(p_camera, projection, sky, nullptr) == OK && targets.end_frame(false) == OK);
		return p_device.read_frame(pixels.data(), pixels.size()) == OK;
	};
	FF_HEMISPHERE_CHECK(draw(Transform3D()));
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_HEMISPHERE_CHECK(pixels[i] == (i < pixels.size() / 2 ? 0xffff0000 : 0xff0000ff));
	}
	const std::vector<uint32_t> original = pixels;
	FF_HEMISPHERE_CHECK(draw(Transform3D(Basis(Vector3(0, 0, 1), real_t(Math::PI / 3)), Vector3(10, 20, -30))) && pixels == original);
	FF_HEMISPHERE_CHECK(draw(Transform3D(Basis(Vector3(1, 0, 0), real_t(Math::PI / 4)), Vector3())));
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_HEMISPHERE_CHECK(pixels[i] == 0xffff0000);
	}
	FF_HEMISPHERE_CHECK(draw(Transform3D(Basis(Vector3(1, 0, 0), real_t(-Math::PI / 4)), Vector3())));
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_HEMISPHERE_CHECK(pixels[i] == 0xff0000ff);
	}
	// 4.x procedural skies carry the gradient directly instead of a generated panorama.
	sky.procedural = true;
	sky.top = Color(1, 0, 0);
	sky.horizon = Color(0, 0, 0);
	sky.ground = Color(0, 0, 1);
	sky.ground_horizon = Color(0, 0, 0);
	sky.sky_curve = sky.ground_curve = 1;
	FF_HEMISPHERE_CHECK(draw(Transform3D()));
	FF_HEMISPHERE_CHECK((pixels.front() & 0xfff8f8f8) == (Color(0.5f, 0, 0).to_argb32() & 0xfff8f8f8) && (pixels.back() & 0xfff8f8f8) == (Color(0, 0, 0.5f).to_argb32() & 0xfff8f8f8));
	FF_HEMISPHERE_CHECK(context.finalize() == OK);
	printf("PASS: cached hemisphere averages, odd rows, RGBE, binary sky fills, pitch, roll invariance and procedural gradients.\n");
	return true;
}

#undef FF_HEMISPHERE_CHECK
