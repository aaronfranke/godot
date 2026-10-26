#include "test_scene_bootstrap.h"

#include <stdio.h>

#define FF_HEMISPHERE_CHECK(m_condition)                                       \
	do {                                                                       \
		if (!(m_condition)) {                                                  \
			printf("Sky hemisphere FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                      \
		}                                                                      \
	} while (false)

bool TestFixedFunctionSceneBootstrap::_test_panorama_averages(FixedFunctionTextureStorage &p_textures) {
	const RID panorama = p_textures.texture_create();
	const RID proxy = p_textures.texture_create();
	FF_HEMISPHERE_CHECK(p_textures.texture_set_proxy(proxy, panorama) == OK);
	Ref<Image> image = memnew(Image(4, 3, false, Image::FORMAT_RGB8));
	image->fill(Color(0, 1, 0));
	image->fill_rect(Rect2(0, 0, 4, 1), Color(1, 0, 0));
	image->fill_rect(Rect2(0, 2, 4, 1), Color(0, 0, 1));
	FF_HEMISPHERE_CHECK(p_textures.texture_request_data(panorama, image) == OK);
	Color sky, ground, average;
	FF_HEMISPHERE_CHECK(p_textures.texture_get_panorama_hemispheres(proxy, sky, ground));
	FF_HEMISPHERE_CHECK(sky.to_rgba32() == Color(0.5f, 0.5f, 0).to_rgba32());
	FF_HEMISPHERE_CHECK(ground.to_rgba32() == Color(0, 0.5f, 0.5f).to_rgba32());
	FF_HEMISPHERE_CHECK(p_textures.texture_get_panorama_average(panorama, average));
	FF_HEMISPHERE_CHECK(average.to_rgba32() == Color(0.25f, 0.5f, 0.25f).to_rgba32());
	image->create(8, 8, false, Image::FORMAT_RGB8);
	image->fill(Color(0, 1, 0));
	image->fill_rect(Rect2(0, 0, 8, 1), Color(1, 0, 0));
	image->fill_rect(Rect2(0, 7, 8, 1), Color(1, 0, 0));
	FF_HEMISPHERE_CHECK(p_textures.texture_request_data(panorama, image) == OK && p_textures.texture_get_panorama_average(panorama, average));
	FF_HEMISPHERE_CHECK(average.to_rgba32() == 0x13ec00ff); // Preserve the existing sky-ambient weighting.
	image->create(4, 1, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 1, 0));
	FF_HEMISPHERE_CHECK(p_textures.texture_request_data(panorama, image) == OK);
	FF_HEMISPHERE_CHECK(p_textures.texture_get_panorama_hemispheres(proxy, sky, ground));
	FF_HEMISPHERE_CHECK(sky == Color(1, 1, 0) && ground == sky);
	image->fill(Color(0, 0, 0));
	FF_HEMISPHERE_CHECK(p_textures.texture_request_data(panorama, image) == OK);
	FF_HEMISPHERE_CHECK(p_textures.texture_get_panorama_hemispheres(proxy, sky, ground) && sky == Color() && ground == sky);
	image->create(4, 2, false, Image::FORMAT_RGBE9995);
	image->fill(Color(0.25f, 0.25f, 0.25f));
	image->fill_rect(Rect2(0, 1, 4, 1), Color(0, 0, 0));
	FF_HEMISPHERE_CHECK(p_textures.texture_request_data(panorama, image) == OK);
	FF_HEMISPHERE_CHECK(p_textures.texture_get_panorama_hemispheres(proxy, sky, ground));
	FF_HEMISPHERE_CHECK(sky.r > 0.53f && sky.r < 0.55f && sky.r == sky.g && sky.g == sky.b && ground == Color());
	FF_HEMISPHERE_CHECK(p_textures.texture_request_allocate_2d(panorama, Size2i(4, 2), Image::FORMAT_RGBA8, 0, 0) == OK);
	FF_HEMISPHERE_CHECK(!p_textures.texture_get_panorama_hemispheres(proxy, sky, ground));
	FF_HEMISPHERE_CHECK(p_textures.free(proxy) == OK && p_textures.free(panorama) == OK);
	printf("PASS: cached hemisphere averages, odd equatorial rows, one-row images, proxies, RGBE and deferred CPU updates.\n");
	return true;
}

bool TestFixedFunctionSceneBootstrap::_test_sky_hemispheres(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RasterizerSceneFixedFunction &p_scene, RID p_environment) {
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionSceneStorage &scenes = p_context.get_scene_storage();
	const RID sky = scenes.sky_create();
	const RID panorama = textures.texture_create();
	Ref<Image> image = memnew(Image(8, 8, false, Image::FORMAT_RGBA8));
	image->fill(Color(0, 0, 1));
	image->fill_rect(Rect2(0, 0, 8, 4), Color(1, 0, 0));
	FF_HEMISPHERE_CHECK(textures.texture_request_data(panorama, image) == OK && scenes.sky_set_panorama(sky, panorama) == OK);
	FF_HEMISPHERE_CHECK(scenes.environment_set_sky(p_environment, sky) == OK && scenes.environment_set_background(p_environment, VS::ENV_BG_SKY) == OK);
	FF_HEMISPHERE_CHECK(scenes.environment_set_energy(p_environment, 1) == OK);
	CameraMatrix projection;
	projection.set_perspective(60, real_t(CAPTURE_WIDTH) / real_t(CAPTURE_HEIGHT), 0.1, 20);
	std::vector<uint32_t> pixels(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	const int top = CAPTURE_WIDTH * (CAPTURE_HEIGHT / 4) + CAPTURE_WIDTH / 2;
	const int bottom = CAPTURE_WIDTH * (CAPTURE_HEIGHT * 3 / 4) + CAPTURE_WIDTH / 2;
	const auto draw = [&](const Transform &p_camera) {
		FF_HEMISPHERE_CHECK(p_manager.begin_frame(0) == OK && p_manager.clear_render_target(Color(1, 0, 1)) == OK);
		p_scene.render_scene(p_camera, projection, 0, projection.is_orthogonal(), nullptr, 0, nullptr, 0, nullptr, 0, p_environment, RID(), RID(), RID(), -1);
		FF_HEMISPHERE_CHECK(p_scene.get_last_error() == OK && p_manager.end_frame(false) == OK);
		return p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK;
	};
	FF_HEMISPHERE_CHECK(draw(Transform()) && pixels[top] == 0xffff0000 && pixels[bottom] == 0xff0000ff);
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_HEMISPHERE_CHECK(pixels[i] == (i < pixels.size() / 2 ? 0xffff0000 : 0xff0000ff));
	}
	const std::vector<uint32_t> original = pixels;
	FF_HEMISPHERE_CHECK(draw(Transform(Basis(), Vector3(10, 20, -30))) && pixels == original);
	FF_HEMISPHERE_CHECK(draw(Transform(Basis(Vector3(0, 1, 0), Math_PI / 3), Vector3())) && pixels == original);
	FF_HEMISPHERE_CHECK(draw(Transform(Basis(Vector3(1, 0, 0), Math_PI / 4), Vector3())));
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_HEMISPHERE_CHECK(pixels[i] == 0xffff0000);
	}
	FF_HEMISPHERE_CHECK(draw(Transform(Basis(Vector3(1, 0, 0), -Math_PI / 4), Vector3())));
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_HEMISPHERE_CHECK(pixels[i] == 0xff0000ff);
	}
	const Basis pitched(Vector3(1, 0, 0), Math_PI / 12);
	FF_HEMISPHERE_CHECK(draw(Transform(pitched, Vector3())) && pixels[top] == 0xffff0000 && pixels[bottom] == 0xff0000ff);
	const std::vector<uint32_t> pitched_pixels = pixels;
	const real_t rolls[] = { -Math_PI, -Math_PI / 2, -0.01, -0.001, 0, 0.001, 0.01, Math_PI / 2, Math_PI };
	for (unsigned i = 0; i < sizeof(rolls) / sizeof(rolls[0]); i++) {
		const Basis rolled = pitched * Basis(Vector3(0, 0, 1), rolls[i]);
		FF_HEMISPHERE_CHECK(draw(Transform(rolled, Vector3())) && pixels == pitched_pixels);
	}
	FF_HEMISPHERE_CHECK(draw(Transform(pitched.scaled(Vector3(3, 3, 3)), Vector3())) && pixels == pitched_pixels);
	FF_HEMISPHERE_CHECK(draw(Transform(Basis(Vector3(1, 0, 0), Math_PI / 2), Vector3())) && pixels.front() == 0xffff0000 && pixels.back() == 0xffff0000);
	FF_HEMISPHERE_CHECK(draw(Transform(Basis(Vector3(1, 0, 0), -Math_PI / 2), Vector3())) && pixels.front() == 0xff0000ff && pixels.back() == 0xff0000ff);
	FF_HEMISPHERE_CHECK(scenes.environment_set_sky_orientation(p_environment, Basis(Vector3(0, 0, 1), Math_PI / 2)) == OK && draw(Transform()));
	FF_HEMISPHERE_CHECK(pixels == original);
	FF_HEMISPHERE_CHECK(scenes.environment_set_sky_orientation(p_environment, Basis(Vector3(1, 0, 0), Math_PI / 2)) == OK && draw(Transform()) && pixels == original);
	FF_HEMISPHERE_CHECK(scenes.environment_set_sky_orientation(p_environment, Basis()) == OK);
	FF_HEMISPHERE_CHECK(scenes.environment_set_energy(p_environment, 0.5f) == OK && draw(Transform()));
	FF_HEMISPHERE_CHECK(pixels[top] == 0xff800000 && pixels[bottom] == 0xff000080);
	FF_HEMISPHERE_CHECK(scenes.environment_set_energy(p_environment, 1) == OK);
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	FF_HEMISPHERE_CHECK(draw(Transform()) && pixels == original);
	projection.set_perspective(60, real_t(CAPTURE_WIDTH) / real_t(CAPTURE_HEIGHT), 0.1, 20);
	CameraMatrix asymmetric;
	asymmetric.set_frustum(-0.1, 0.2, -0.2, 0.1, 0.1, 20);
	projection = asymmetric;
	FF_HEMISPHERE_CHECK(draw(Transform()) && pixels[top] == 0xffff0000 && pixels[CAPTURE_WIDTH * (CAPTURE_HEIGHT / 2) + CAPTURE_WIDTH / 2] == 0xff0000ff);
	projection.set_perspective(179.9, real_t(CAPTURE_WIDTH) / real_t(CAPTURE_HEIGHT), 0.1, 20);
	FF_HEMISPHERE_CHECK(draw(Transform()) && pixels == original);
	projection.set_perspective(60, real_t(CAPTURE_WIDTH) / real_t(CAPTURE_HEIGHT), 0.1, 20);
	FF_HEMISPHERE_CHECK(scenes.environment_set_sky_custom_fov(p_environment, 120) == OK);
	FF_HEMISPHERE_CHECK(draw(Transform(Basis(Vector3(1, 0, 0), Math_PI / 4), Vector3())) && pixels.front() == 0xffff0000 && pixels.back() == 0xff0000ff);
	FF_HEMISPHERE_CHECK(scenes.environment_set_sky_custom_fov(p_environment, 0) == OK);
	if (p_context.get_device().get_capabilities().render_target_opaque) {
		const RID target = textures.render_target_create();
		FF_HEMISPHERE_CHECK(textures.render_target_set_size(target, CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
		FF_HEMISPHERE_CHECK(p_manager.begin_frame(0) == OK && p_manager.select_render_target(target) == OK);
		p_scene.render_scene(Transform(), projection, 0, false, nullptr, 0, nullptr, 0, nullptr, 0, p_environment, RID(), RID(), RID(), -1);
		FF_HEMISPHERE_CHECK(p_scene.get_last_error() == OK && p_manager.select_render_target(RID()) == OK);
		FF_HEMISPHERE_CHECK(p_manager.blit_to_screen(target, Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT), 0) == OK && p_manager.end_frame(false) == OK);
		FF_HEMISPHERE_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
		FF_HEMISPHERE_CHECK((pixels[top] & 0xfffcfcfc) == 0xfffc0000 && (pixels[bottom] & 0xfffcfcfc) == 0xff0000fc);
		FF_HEMISPHERE_CHECK(textures.free(target) == OK);
	}
	FF_HEMISPHERE_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FF_HEMISPHERE_CHECK(draw(Transform()) && pixels == original);
	image->fill(Color(0, 1, 0));
	FF_HEMISPHERE_CHECK(textures.texture_request_data(panorama, image) == OK && draw(Transform()) && pixels[top] == 0xff00ff00 && pixels[bottom] == 0xff00ff00);
	FF_HEMISPHERE_CHECK(textures.free(panorama) == OK && draw(Transform()) && pixels[top] == 0xffff00ff);
	FF_HEMISPHERE_CHECK(scenes.free(sky) == OK);
	printf("PASS: DirectDraw sky hemispheres, pitch, roll invariance, projection, brightness, nested viewport and reset.\n");
	return true;
}

#undef FF_HEMISPHERE_CHECK
