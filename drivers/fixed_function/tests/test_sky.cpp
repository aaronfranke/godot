#include "test_scene_bootstrap.h"

#include <stdio.h>
#include <string.h>

#define FF_SKY_CHECK(m_condition)                                   \
	do {                                                            \
		if (!(m_condition)) {                                       \
			printf("Sky FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                           \
		}                                                           \
	} while (false)

// The D3D3 Software RGB rasterizer rounds saturated channels down during texture modulation.
#define FF_SKY_CHECK_COLOR(m_actual, m_expected) FF_SKY_CHECK(((m_actual)&0xfffcfcfc) == ((m_expected)&0xfffcfcfc))

bool TestFixedFunctionSceneBootstrap::_test_sky(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RasterizerSceneFixedFunction &p_scene, RID p_environment) {
	if (p_context.get_device().get_capabilities().sky_solid_hemispheres) {
		return _test_sky_hemispheres(p_context, p_manager, p_scene, p_environment);
	}
	FixedFunctionSceneStorage &scenes = p_context.get_scene_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID sky = scenes.sky_create();
	const RID panorama = textures.texture_create();
	Ref<Image> image;
	image.instance();
	image->create(64, 32, false, Image::FORMAT_RGBA8);
	const Color longitude[] = { Color(1, 0, 0, 0), Color(0, 1, 0), Color(0, 0, 1), Color(1, 1, 0) };
	image->lock();
	for (int y = 0; y < 32; y++) {
		for (int x = 0; x < 64; x++) {
			Color color = longitude[((x + 8) / 16) % 4];
			if (y < 8) {
				color = Color(0, 1, 1);
			} else if (y >= 24) {
				color = Color(1, 1, 1);
			}
			image->set_pixel(x, y, color);
		}
	}
	image->unlock();
	FF_SKY_CHECK(sky.is_valid() && scenes.sky_set_panorama(sky, panorama) == OK);
	FF_SKY_CHECK(scenes.environment_set_sky(p_environment, sky) == OK && scenes.environment_set_energy(p_environment, 1) == OK);
	FF_SKY_CHECK(scenes.environment_set_background(p_environment, VS::ENV_BG_SKY) == OK);
	CameraMatrix perspective;
	perspective.set_perspective(60, real_t(CAPTURE_WIDTH) / real_t(CAPTURE_HEIGHT), 0.1, 20);
	std::vector<uint32_t> pixels(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	const int center = CAPTURE_WIDTH * (CAPTURE_HEIGHT / 2) + CAPTURE_WIDTH / 2;
	const uint32_t clear_color = 0xffff00ff;
	auto draw = [&](const Transform &p_camera, const CameraMatrix &p_projection) {
		FF_SKY_CHECK(p_manager.begin_frame(0) == OK && p_manager.clear_render_target(Color(1, 0, 1)) == OK);
		p_scene.render_scene(p_camera, p_projection, 0, p_projection.is_orthogonal(), nullptr, 0, nullptr, 0, nullptr, 0, p_environment, RID(), RID(), RID(), -1);
		FF_SKY_CHECK(p_scene.get_last_error() == OK && p_manager.end_frame(false) == OK);
		return p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK;
	};
	FF_SKY_CHECK(draw(Transform(), perspective) && pixels[center] == clear_color);
	FF_SKY_CHECK(textures.texture_request_data(panorama, image) == OK && draw(Transform(), perspective));
	FF_SKY_CHECK_COLOR(pixels[center], 0xffff0000);
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_SKY_CHECK(pixels[i] != clear_color); // Coverage, including the U seam.
		FF_SKY_CHECK((pixels[i] & 0x00ffffff) != 0); // The panorama fixture contains no black texels.
	}
	const std::vector<uint32_t> original = pixels;
	FF_SKY_CHECK(draw(Transform(Basis(), Vector3(10, 4, -31)), perspective) && pixels == original);
	const Basis yaw(Vector3(0, 1, 0), -Math_PI / 2);
	FF_SKY_CHECK(draw(Transform(yaw, Vector3()), perspective));
	FF_SKY_CHECK_COLOR(pixels[center], 0xff00ff00);
	FF_SKY_CHECK(scenes.environment_set_sky_orientation(p_environment, yaw.inverse()) == OK && draw(Transform(), perspective));
	FF_SKY_CHECK_COLOR(pixels[center], 0xff00ff00);
	FF_SKY_CHECK(scenes.environment_set_sky_orientation(p_environment, Basis()) == OK);
	const Basis pitch(Vector3(1, 0, 0), Math_PI / 2);
	FF_SKY_CHECK(draw(Transform(pitch, Vector3()), perspective));
	FF_SKY_CHECK_COLOR(pixels[center], 0xff00ffff);
	size_t clear_pixels = 0;
	for (size_t i = 0; i < pixels.size(); i++) {
		if (pixels[i] == clear_color) {
			// D3D7 RGB can miss one pixel at the shared pole vertex.
			const size_t x = i % CAPTURE_WIDTH;
			const size_t y = i / CAPTURE_WIDTH;
			FF_SKY_CHECK(x >= CAPTURE_WIDTH / 2 - 1 && x <= CAPTURE_WIDTH / 2 &&
					y >= CAPTURE_HEIGHT / 2 - 1 && y <= CAPTURE_HEIGHT / 2);
			FF_SKY_CHECK(++clear_pixels == 1);
		}
		FF_SKY_CHECK((pixels[i] & 0x00ffffff) != 0);
	}
	FF_SKY_CHECK_COLOR(original[center + 56], 0xffff0000);
	FF_SKY_CHECK(scenes.environment_set_sky_custom_fov(p_environment, 120) == OK && draw(Transform(), perspective));
	FF_SKY_CHECK_COLOR(pixels[center + 56], 0xff00ff00);
	FF_SKY_CHECK(scenes.environment_set_sky_custom_fov(p_environment, 0) == OK);
	CameraMatrix orthogonal;
	orthogonal.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	FF_SKY_CHECK(draw(Transform(), orthogonal));
	FF_SKY_CHECK_COLOR(pixels[center], 0xffff0000);
	FF_SKY_CHECK(draw(Transform(), CameraMatrix()) && pixels[center] == clear_color);
	FF_SKY_CHECK(scenes.environment_set_energy(p_environment, 0.5f) == OK && draw(Transform(), perspective));
	const uint32_t red = (pixels[center] >> 16) & 0xff;
	FF_SKY_CHECK(red >= 126 && red <= 129 && (pixels[center] & 0x0000ffff) == 0);
	FF_SKY_CHECK(scenes.environment_set_energy(p_environment, 1) == OK);
	FF_SKY_CHECK(scenes.environment_set_color(p_environment, Color(0, 0, 1)) == OK);
	FF_SKY_CHECK(scenes.environment_set_background(p_environment, VS::ENV_BG_COLOR_SKY) == OK && draw(Transform(), perspective) && pixels[center] == 0xff0000ff);
	FF_SKY_CHECK(scenes.environment_set_background(p_environment, VS::ENV_BG_SKY) == OK);
	FF_SKY_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK);
	FF_SKY_CHECK(p_context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK && draw(Transform(), perspective) && pixels == original);
	if (p_context.get_device().get_capabilities().render_target_opaque && p_context.get_device().get_capabilities().depth_buffer) {
		FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
		FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
		const RID shader = materials.shader_create();
		const RID material = materials.material_create();
		const RID mesh = meshes.mesh_create();
		const float positions[] = { -1, -1, -3, 0, 1, -3, 1, -1, -3 };
		PoolVector<uint8_t> data;
		FF_SKY_CHECK(data.resize(sizeof(positions)) == OK);
		{
			PoolVector<uint8_t>::Write bytes = data.write();
			memcpy(bytes.ptr(), positions, sizeof(positions));
		}
		FF_SKY_CHECK(meshes.mesh_add_surface(mesh, VS::ARRAY_FORMAT_VERTEX, VS::PRIMITIVE_TRIANGLES, data, 3, PoolVector<uint8_t>(), 0, AABB(Vector3(-1, -1, -3), Vector3(2, 2, 0)), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
		FF_SKY_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded,cull_disabled; void fragment(){ ALBEDO=vec3(0,1,0); }") == OK);
		FF_SKY_CHECK(materials.material_set_shader(material, shader) == OK);
		FFInstance instance(materials, material);
		instance.base_type = VS::INSTANCE_MESH;
		instance.base = mesh;
		RasterizerScene::InstanceBase *instances[] = { &instance };
		const RID target = textures.render_target_create();
		FF_SKY_CHECK(textures.render_target_set_size(target, CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
		FF_SKY_CHECK(p_manager.begin_frame(0) == OK && p_manager.select_render_target(target) == OK);
		FF_SKY_CHECK(p_manager.clear_render_target(Color(1, 0, 1)) == OK);
		p_scene.render_scene(Transform(), perspective, 0, false, instances, 1, nullptr, 0, nullptr, 0, p_environment, RID(), RID(), RID(), -1);
		FF_SKY_CHECK(p_scene.get_last_error() == OK && p_manager.select_render_target(RID()) == OK);
		FF_SKY_CHECK(p_manager.blit_to_screen(target, Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT), 0) == OK && p_manager.end_frame(false) == OK);
		FF_SKY_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
		FF_SKY_CHECK_COLOR(pixels[center], 0xff00ff00);
		FF_SKY_CHECK_COLOR(pixels[center + 56], 0xffff0000);
		instance.detach();
		FF_SKY_CHECK(textures.free(target) == OK && meshes.free(mesh) == OK && materials.free(material) == OK && materials.free(shader) == OK);
	}
	// ProceduralSky publishes linear RGBE data. Match GLES2's sRGB display conversion.
	image->create(4, 2, false, Image::FORMAT_RGBE9995);
	image->fill(Color(0.25f, 0.25f, 0.25f));
	FF_SKY_CHECK(textures.texture_request_data(panorama, image) == OK && draw(Transform(), perspective));
	for (int shift = 0; shift < 24; shift += 8) {
		const uint32_t channel = (pixels[center] >> shift) & 0xff;
		FF_SKY_CHECK(channel >= 135 && channel <= 139);
	}
	FF_SKY_CHECK(textures.free(panorama) == OK && draw(Transform(), perspective) && pixels[center] == clear_color);
	FF_SKY_CHECK(scenes.free(sky) == OK && draw(Transform(), perspective) && pixels[center] == clear_color);
	printf("PASS: panorama coverage, camera and sky rotation, translation invariance, projection, brightness, viewport composition, reset and resource detachment.\n");
	return true;
}

#undef FF_SKY_CHECK
#undef FF_SKY_CHECK_COLOR
