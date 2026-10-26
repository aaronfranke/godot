#include "test_graphics_device_d3d7.h"
#include "drivers/fixed_function/tests/test_depth.h"
#include "drivers/fixed_function/tests/test_mesh_storage.h"
#include "drivers/fixed_function/tests/test_unlit_scene.h"
#include "test_resolution_policy_d3d7.h"

#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "drivers/d3d7/graphics_device_d3d7.h"
#include "drivers/fixed_function/tests/test_blending.h"
#include "drivers/fixed_function/tests/test_canvas_item_renderer.h"
#include "drivers/fixed_function/tests/test_clipping.h"
#include "drivers/fixed_function/tests/test_deferred_textures.h"
#include "drivers/fixed_function/tests/test_material_storage.h"
#include "drivers/fixed_function/tests/test_projection.h"
#include "drivers/fixed_function/tests/test_render_context.h"
#include "drivers/fixed_function/tests/test_render_target_manager.h"
#include "drivers/fixed_function/tests/test_render_targets.h"
#include "drivers/fixed_function/tests/test_retained_scene.h"
#include "drivers/fixed_function/tests/test_sampling.h"
#include "drivers/fixed_function/tests/test_scene_bootstrap.h"
#include "drivers/fixed_function/tests/test_storage_queries.h"
#include "drivers/fixed_function/tests/test_texture_metadata.h"
#include "drivers/fixed_function/tests/test_texture_partial_updates.h"
#include "drivers/fixed_function/tests/test_texture_storage.h"
#include "drivers/fixed_function/tests/test_texture_storage_device.h"
#include "drivers/fixed_function/tests/test_textures.h"
#include "drivers/fixed_function/tests/test_viewport_recovery.h"

#include <float.h>
#include <stdio.h>
#include <vector>

#define CHECK(m_condition)                                           \
	do {                                                             \
		if (!(m_condition)) {                                        \
			printf("FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                            \
		}                                                            \
	} while (false)

const uint32_t TestFixedFunctionGraphicsDeviceD3D7::CLEAR_COLOR;
const FixedFunctionVertex TestFixedFunctionGraphicsDeviceD3D7::TRIANGLE[3] = {
	{ -0.75f, -0.75f, 0.5f, 0xffff0000, 0, 0 },
	{ 0.0f, 0.75f, 0.5f, 0xff00ff00, 0, 0 },
	{ 0.75f, -0.75f, 0.5f, 0xff0000ff, 0, 0 },
};

bool TestFixedFunctionGraphicsDeviceD3D7::_render_and_read(FixedFunctionGraphicsDeviceD3D7 &p_device, const CameraMatrix &p_transform, std::vector<uint32_t> &r_pixels) {
	Error error = p_device.begin_frame(CLEAR_COLOR);
	if (error != OK) {
		printf("begin_frame: error=%d HRESULT=0x%08x\n", int(error), uint32_t(p_device.get_last_result()));
		return false;
	}
	CHECK(p_device.begin_frame(CLEAR_COLOR) == ERR_BUSY);
	CHECK(p_device.draw_triangles(nullptr, 3, p_transform) == ERR_INVALID_PARAMETER);
	CHECK(p_device.draw_triangles(TRIANGLE, 2, p_transform) == ERR_INVALID_PARAMETER);
	CHECK(p_device.draw_triangles(nullptr, 0, p_transform) == OK);
	CHECK(p_device.read_frame(nullptr, 0) == ERR_BUSY);
	CHECK(p_device.draw_triangles(TRIANGLE, 3, p_transform) == OK);
	CHECK(p_device.end_frame(false) == OK);
	CHECK(p_device.end_frame() == ERR_UNCONFIGURED);
	CHECK(p_device.read_frame(nullptr, 0) == ERR_INVALID_PARAMETER);
	r_pixels.resize(size_t(p_device.get_width()) * p_device.get_height());
	CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size() - 1) == ERR_INVALID_PARAMETER);
	CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	return true;
}

bool TestFixedFunctionGraphicsDeviceD3D7::_is_clear_color(uint32_t p_pixel) {
	// Allow the quantization of 16-bit RGB display surfaces.
	for (int shift = 0; shift < 24; shift += 8) {
		int difference = int((p_pixel >> shift) & 255) - int((CLEAR_COLOR >> shift) & 255);
		if (difference < -4 || difference > 4) {
			return false;
		}
	}
	return (p_pixel >> 24) == 255;
}

bool TestFixedFunctionGraphicsDeviceD3D7::_check_pixels(const std::vector<uint32_t> &p_pixels, uint32_t p_width, uint32_t p_height) {
	CHECK(_is_clear_color(p_pixels[size_t(2) * p_width + 2]));
	const uint32_t red = p_pixels[size_t(p_height * 3 / 4) * p_width + p_width / 4];
	const uint32_t green = p_pixels[size_t(p_height / 4) * p_width + p_width / 2];
	const uint32_t blue = p_pixels[size_t(p_height * 3 / 4) * p_width + p_width * 3 / 4];
	CHECK(((red >> 16) & 255) > 160 && (red & 255) < 80);
	CHECK(((green >> 8) & 255) > 160 && ((green >> 16) & 255) < 80);
	CHECK((blue & 255) > 160 && ((blue >> 16) & 255) < 80);
	return true;
}

bool TestFixedFunctionGraphicsDeviceD3D7::_save_frame(const String &p_path, const std::vector<uint32_t> &p_pixels, uint32_t p_width, uint32_t p_height) {
	FileAccessRef file = FileAccess::open(p_path, FileAccess::WRITE);
	CHECK(file);
	file->store_line("P6");
	file->store_line(itos(p_width) + " " + itos(p_height));
	file->store_line("255");
	for (size_t i = 0; i < p_pixels.size(); i++) {
		const uint32_t pixel = p_pixels[i];
		const uint8_t rgb[] = { uint8_t(pixel >> 16), uint8_t(pixel >> 8), uint8_t(pixel) };
		file->store_buffer(rgb, sizeof(rgb));
	}
	CHECK(file->get_error() == OK);
	return true;
}

DWORD WINAPI TestFixedFunctionGraphicsDeviceD3D7::_wrong_thread(void *p_device) {
	FixedFunctionGraphicsDeviceD3D7 *device = static_cast<FixedFunctionGraphicsDeviceD3D7 *>(p_device);
	return device->resize(1, 1) == ERR_BUG && device->prepare_resources() == ERR_BUG ? 0 : 1;
}

bool TestFixedFunctionGraphicsDeviceD3D7::_test_depth16(HWND p_window, bool p_force_rgb) {
	FixedFunctionGraphicsDeviceD3D7 device(D3D7ResolutionPolicy::D3D7_OVERSIZE_RESTRICTED, p_force_rgb);
	CHECK(device.initialize(p_window, 64, 64, false) == OK);
	if (!device.get_capabilities().depth_buffer || !(device._depth_formats[1].dwFlags & DDPF_ZBUFFER)) {
		printf("SKIP: D3D7 has no supported 16-bit depth format.\n");
		return true;
	}
	// Exercise the actual fallback allocator without adding a production format override.
	device._depth_formats[0] = DDPIXELFORMAT();
	CHECK(device.begin_frame(0xff000000, RID(), true, true) == OK);
	DDPIXELFORMAT format = {};
	format.dwSize = sizeof(format);
	CHECK(SUCCEEDED(device._window_depth->GetPixelFormat(&format)) && format.dwZBufferBitDepth == 16);
	FixedFunctionGraphicsDevice::FFDrawParameters depth;
	depth.depth_test_enabled = depth.depth_write_enabled = true;
	FixedFunctionVertex quad[] = {
		{ -1, 1, -0.5f, 0xffff0000, 0, 0 }, { 1, 1, -0.5f, 0xffff0000, 1, 0 }, { 1, -1, -0.5f, 0xffff0000, 1, 1 }, { -1, 1, -0.5f, 0xffff0000, 0, 0 }, { 1, -1, -0.5f, 0xffff0000, 1, 1 }, { -1, -1, -0.5f, 0xffff0000, 0, 1 }
	};
	CHECK(device.draw_triangles(quad, 6, CameraMatrix(), RID(), depth) == OK);
	for (int i = 0; i < 6; i++) {
		quad[i].z = 0.5f;
		quad[i].color = 0xff0000ff;
	}
	CHECK(device.draw_triangles(quad, 6, CameraMatrix(), RID(), depth) == OK);
	CHECK(device.end_frame(false) == OK);
	std::vector<uint32_t> pixels(64 * 64);
	CHECK(device.read_frame(pixels.data(), pixels.size()) == OK && pixels[32 * 64 + 32] == 0xffff0000);
	CHECK(device.finalize() == OK);
	printf("PASS: D3D7 16-bit depth occlusion and release.\n");
	return true;
}

bool TestFixedFunctionGraphicsDeviceD3D7::_test_streamed_lighting(HWND p_window, bool p_force_rgb) {
	FixedFunctionGraphicsDeviceD3D7 device(D3D7ResolutionPolicy::D3D7_OVERSIZE_RESTRICTED, p_force_rgb);
	FixedFunctionRenderContext context(device, p_window, 128, 96, false);
	CHECK(context.initialize() == OK);
	const uint32_t light_limit = device.get_max_streamed_vertex_lights();
	if (!light_limit) {
		printf("SKIP: D3D7 device reports no native lights.\n");
		return context.finalize() == OK;
	}

	FixedFunctionGraphicsDevice::FFPixelLighting lighting = {};
	lighting.ambient = Color(1, 0, 0);
	lighting.vertex_ambient_occlusion = true;
	FixedFunctionLightingVertex vertices[3] = {};
	for (int i = 0; i < 3; i++) {
		vertices[i].x = TRIANGLE[i].x;
		vertices[i].y = TRIANGLE[i].y;
		vertices[i].z = TRIANGLE[i].z;
		vertices[i].nz = 1;
		vertices[i].r = vertices[i].g = vertices[i].b = 0.5f;
	}
	const Color white(1, 1, 1);
	const FixedFunctionGraphicsDevice::FFDrawParameters state;
	const uint16_t invalid_indices[] = { 0, 1, 3 };
	const uint16_t indices[] = { 0, 1, 2 };
	CHECK(device.begin_frame(CLEAR_COLOR) == OK);
	CHECK(device.draw_vertex_lit_triangles(vertices, 3, CameraMatrix(), lighting, white, RID(), state, invalid_indices, 3) == ERR_INVALID_DATA);
	lighting.light_count = light_limit + 1;
	CHECK(device.draw_vertex_lit_triangles(vertices, 3, CameraMatrix(), lighting, white, RID(), state, indices, 3) == ERR_UNAVAILABLE);
	lighting.light_count = 0;
	CHECK(device.draw_vertex_lit_triangles(vertices, 3, CameraMatrix(), lighting, white, RID(), state, indices, 3) == OK);
	CHECK(!device._vertex_lit_data.empty());
	DWORD lighting_enabled = TRUE;
	CHECK(SUCCEEDED(device._device->GetRenderState(D3DRENDERSTATE_LIGHTING, &lighting_enabled)) && !lighting_enabled);
	CHECK(device.end_frame(false) == OK);
	std::vector<uint32_t> pixels(128 * 96);
	CHECK(device.read_frame(pixels.data(), pixels.size()) == OK);
	const uint32_t sample = pixels[48 * 128 + 64];
	CHECK(Math::abs(int((sample >> 16) & 255) - 128) <= 8 && (sample & 0xffff) == 0);
	CHECK(context.finalize() == OK && device._vertex_lit_data.empty());
	printf("PASS: D3D7 streamed indexed ambient occlusion, invalid indices, light limit and lighting state restoration.\n");
	return true;
}

bool TestFixedFunctionGraphicsDeviceD3D7::_run_probe(HWND p_window, const String &p_capture_path, bool p_force_rgb) {
	CHECK(TestD3D7ResolutionPolicy::test());
	FixedFunctionGraphicsDeviceD3D7 device(D3D7ResolutionPolicy::D3D7_OVERSIZE_RESTRICTED, p_force_rgb);
	const unsigned int fpu_control = _control87(0, 0) & (_MCW_PC | _MCW_RC);
	printf("CPU real_t: %u-bit\n", unsigned(sizeof(real_t) * 8));
	CHECK(device.begin_frame(CLEAR_COLOR) == ERR_UNCONFIGURED);
	CHECK(device.prepare_resources() == ERR_UNCONFIGURED);
	CHECK(device.initialize(nullptr, 256, 192, false) == ERR_INVALID_PARAMETER);
	Error initialized = device.initialize(p_window, 256, 192, false);
	if (initialized != OK) {
		printf("initialize: error=%d HRESULT=0x%08x\n", int(initialized), uint32_t(device.get_last_result()));
		return false;
	}
	CHECK(device.initialize(p_window, 256, 192, false) == ERR_ALREADY_IN_USE);
	CHECK(!p_force_rgb || device._rgb_device);
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = device.get_capabilities();
	printf("D3D7 caps: textures=%ux%u stages=%u primitives=%u hardware_tnl=%s device=%s\n",
			caps.max_texture_width, caps.max_texture_height, caps.max_texture_stages,
			caps.max_primitives_per_draw, caps.hardware_transform_and_light ? "yes" : "no", device._rgb_device ? "RGB" : "HAL");
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const char *anisotropy_setting = "rendering/quality/filters/anisotropic_filter_level";
	const Variant previous_anisotropy = settings->get(anisotropy_setting);
	settings->set(anisotropy_setting, 1024);
	CHECK(device._set_sampler_state(FixedFunctionGraphicsDevice::SAMPLER_FILTER_ANISOTROPIC) == OK);
	DWORD filter = 0;
	CHECK(SUCCEEDED(device._device->GetTextureStageState(0, D3DTSS_MINFILTER, &filter)));
	CHECK(filter == DWORD(caps.sampler_max_anisotropy > 1 ? D3DTFN_ANISOTROPIC : D3DTFN_POINT));
	if (caps.sampler_max_anisotropy > 1) {
		DWORD limit = 0;
		CHECK(SUCCEEDED(device._device->GetTextureStageState(0, D3DTSS_MAXANISOTROPY, &limit)) && limit == caps.sampler_max_anisotropy);
	}
	CHECK(SUCCEEDED(device._device->GetTextureStageState(0, D3DTSS_MAGFILTER, &filter)) && filter == D3DTFG_POINT);
	settings->set(anisotropy_setting, 1);
	CHECK(device._set_sampler_state(FixedFunctionGraphicsDevice::SAMPLER_FILTER_ANISOTROPIC) == OK);
	CHECK(SUCCEEDED(device._device->GetTextureStageState(0, D3DTSS_MINFILTER, &filter)) && filter == D3DTFN_POINT);
	settings->set(anisotropy_setting, previous_anisotropy);
	printf("PASS: D3D7 anisotropic minification and point fallback (native maximum %u).\n", unsigned(caps.sampler_max_anisotropy));
	HANDLE thread = CreateThread(nullptr, 0, _wrong_thread, &device, 0, nullptr);
	CHECK(thread != nullptr);
	CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
	DWORD thread_result = 1;
	CHECK(GetExitCodeThread(thread, &thread_result));
	CloseHandle(thread);
	CHECK(thread_result == 0);

	std::vector<uint32_t> pixels;
	CameraMatrix transform;
	CHECK(device.draw_triangles(TRIANGLE, 3, transform) == ERR_UNCONFIGURED);
	CHECK(_render_and_read(device, transform, pixels));
	CHECK(_check_pixels(pixels, device.get_width(), device.get_height()));
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path, pixels, device.get_width(), device.get_height()));
	}

	CHECK(TestFixedFunctionTextures::test(device, pixels));
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_texture.ppm", pixels, device.get_width(), device.get_height()));
	}

	CHECK(TestFixedFunctionBlending::test(device, pixels));
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_blend.ppm", pixels, device.get_width(), device.get_height()));
	}

	CHECK(TestFixedFunctionClipping::test(device, pixels));
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_clip.ppm", pixels, device.get_width(), device.get_height()));
	}

	CHECK(TestFixedFunctionProjection::test(device, pixels));
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_projection.ppm", pixels, device.get_width(), device.get_height()));
	}

	CameraMatrix translated;
	translated.matrix[3][0] = real_t(0.25) + real_t(1e-12);
	CHECK(_render_and_read(device, translated, pixels));
	CHECK(_is_clear_color(pixels[size_t(device.get_height() * 3 / 4) * device.get_width() + device.get_width() / 4]));
	CHECK(device.resize(320, 240) == OK);
	CHECK(_render_and_read(device, transform, pixels));
	CHECK(!p_force_rgb || device._rgb_device);
	CHECK(device.get_width() == 320 && device.get_height() == 240);
	CHECK(_check_pixels(pixels, 320, 240));
	CHECK(device.set_use_vsync(true) == OK);
	CHECK(_render_and_read(device, transform, pixels));
	CHECK(_check_pixels(pixels, 320, 240));
	CHECK(device.resize(0, 0) == OK);
	CHECK(device.begin_frame(CLEAR_COLOR) == ERR_SKIP);
	CHECK(device.resize(320, 240) == OK);
	CHECK(_render_and_read(device, transform, pixels));
	CHECK(_check_pixels(pixels, 320, 240));

	CHECK(device.begin_frame(CLEAR_COLOR) == OK);
	CHECK(device.draw_triangles(TRIANGLE, 3, transform) == OK);
	CHECK(device.end_frame() == OK);
	CHECK(device.read_frame(pixels.data(), pixels.size()) == ERR_UNCONFIGURED);
	CHECK(device.finalize() == OK);
	CHECK(device.finalize() == OK);
	CHECK(device.get_width() == 0 && device.get_capabilities().max_texture_width == 0);
	CHECK(device.initialize(p_window, 256, 192, false) == OK);
	CHECK(_render_and_read(device, transform, pixels));
	CHECK(_check_pixels(pixels, 256, 192));
	// Also exercise cleanup while a scene is active.
	CHECK(device.begin_frame(CLEAR_COLOR) == OK);
	CHECK(device.finalize() == OK);
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	printf("PASS: pixels, transforms, resize/reset, vsync toggle, suspension, presentation, threading, reinitialization, and FPU preservation.\n");
	CHECK(TestFixedFunctionRenderContext::test(device, p_window, pixels));
	CHECK(TestFixedFunctionTextureStorage::test(device));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_boot.ppm", pixels, TestFixedFunctionRenderContext::CAPTURE_WIDTH, TestFixedFunctionRenderContext::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionTextureStorageDevice::test(device, p_window, pixels));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_storage.ppm", pixels, TestFixedFunctionTextureStorageDevice::CAPTURE_WIDTH, TestFixedFunctionTextureStorageDevice::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionRenderTargets::test(device, p_window, pixels));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty() && !pixels.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_target.ppm", pixels, TestFixedFunctionRenderTargets::CAPTURE_WIDTH, TestFixedFunctionRenderTargets::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionSampling::test(device, p_window, pixels));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_sampling.ppm", pixels, TestFixedFunctionSampling::CAPTURE_WIDTH, TestFixedFunctionSampling::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionViewportRecovery::test(device, p_window, pixels));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty() && !pixels.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_recovery.ppm", pixels, TestFixedFunctionViewportRecovery::CAPTURE_WIDTH, TestFixedFunctionViewportRecovery::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionRenderTargetManager::test(device, p_window, pixels));
	if (!p_capture_path.empty() && !pixels.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_passes.ppm", pixels, TestFixedFunctionRenderTargetManager::CAPTURE_WIDTH, TestFixedFunctionRenderTargetManager::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionCanvasItemRenderer::test(device, p_window, pixels));
	if (!p_capture_path.empty() && !pixels.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_canvas.ppm", pixels, TestFixedFunctionCanvasItemRenderer::CAPTURE_WIDTH, TestFixedFunctionCanvasItemRenderer::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionTexturePartialUpdates::test(device, p_window, pixels));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_partial.ppm", pixels, TestFixedFunctionTexturePartialUpdates::CAPTURE_WIDTH, TestFixedFunctionTexturePartialUpdates::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionTextureMetadata::test(device, p_window, pixels));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_metadata.ppm", pixels, TestFixedFunctionTextureMetadata::CAPTURE_WIDTH, TestFixedFunctionTextureMetadata::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionDeferredTextures::test(device, p_window, pixels));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_deferred.ppm", pixels, TestFixedFunctionDeferredTextures::CAPTURE_WIDTH, TestFixedFunctionDeferredTextures::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionMaterialStorage::test(device, p_window, pixels));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_materials.ppm", pixels, TestFixedFunctionMaterialStorage::CAPTURE_WIDTH, TestFixedFunctionMaterialStorage::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionSceneBootstrap::test(device, p_window, pixels));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_scene.ppm", pixels, TestFixedFunctionSceneBootstrap::CAPTURE_WIDTH, TestFixedFunctionSceneBootstrap::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionStorageQueries::test(device, p_window, pixels));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_queries.ppm", pixels, TestFixedFunctionStorageQueries::CAPTURE_WIDTH, TestFixedFunctionStorageQueries::CAPTURE_HEIGHT));
	}
	CHECK(TestFixedFunctionDepth::test(device, p_window, pixels));
	CHECK(TestFixedFunctionMeshStorage::test());
	CHECK(_test_depth16(p_window, p_force_rgb));
	CHECK(_test_vertex_stream(p_window, p_force_rgb));
	CHECK(_test_streamed_lighting(p_window, p_force_rgb));
	CHECK(_test_geometry(p_window, p_force_rgb));
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_depth.ppm", pixels, TestFixedFunctionDepth::CAPTURE_WIDTH, TestFixedFunctionDepth::CAPTURE_HEIGHT));
	}
	CHECK(TestD3D7ResolutionPolicy::test_device(device, p_window));
	CHECK(TestFixedFunctionUnlitScene::test(device, p_window, pixels));
	CHECK(TestFixedFunctionRetainedScene::test(device, p_window));
	if (!p_capture_path.empty()) {
		CHECK(_save_frame(p_capture_path.get_basename() + "_unlit.ppm", pixels, TestFixedFunctionUnlitScene::CAPTURE_WIDTH, TestFixedFunctionUnlitScene::CAPTURE_HEIGHT));
	}

#if defined(_MSC_VER) && defined(_M_IX86)
	CHECK((_control87(0, 0) & (_MCW_PC | _MCW_RC)) == fpu_control);
#endif

	return true;
}

LRESULT CALLBACK TestFixedFunctionGraphicsDeviceD3D7::_window_proc(HWND p_window, UINT p_message, WPARAM p_wparam, LPARAM p_lparam) {
	return DefWindowProcA(p_window, p_message, p_wparam, p_lparam);
}

MainLoop *TestFixedFunctionGraphicsDeviceD3D7::test(const List<String> &p_args) {
	OS::get_singleton()->set_exit_code(1);
	String capture_path;
	int resolution_mode = -1;
	bool block_override = false;
	bool force_rgb = false;
	const String capture_prefix = "--d3d7-capture=";
	const String resolution_prefix = "--d3d7-resolution-mode=";
	for (const List<String>::Element *E = p_args.front(); E; E = E->next()) {
		if (E->get() == "--d3d7-block-override") {
			block_override = true;
		}
		if (E->get() == "--d3d7-force-rgb") {
			force_rgb = true;
		}
		if (E->get().begins_with(capture_prefix)) {
			capture_path = E->get().substr(capture_prefix.length());
		}
		if (E->get().begins_with(resolution_prefix)) {
			resolution_mode = E->get().substr(resolution_prefix.length()).to_int();
			if (resolution_mode < 0 || resolution_mode >= D3D7ResolutionPolicy::D3D7_OVERSIZE_MODE_MAX) {
				return nullptr;
			}
		}
	}
	WNDCLASSA window_class = {};
	window_class.lpfnWndProc = _window_proc;
	window_class.hInstance = GetModuleHandleA(nullptr);
	window_class.lpszClassName = "GodotD3D7Probe";
	if (!RegisterClassA(&window_class)) {
		return nullptr;
	}
	HWND window = CreateWindowA(window_class.lpszClassName, "Godot Fixed Function D3D7 probe", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 320, 240, nullptr, nullptr, window_class.hInstance, nullptr);
	if (!window) {
		UnregisterClassA(window_class.lpszClassName, window_class.hInstance);
		return nullptr;
	}
	bool passed = resolution_mode < 0 ? _run_probe(window, capture_path, force_rgb) : TestD3D7ResolutionPolicy::test_mode(window, D3D7ResolutionPolicy::D3D7OversizeMode(resolution_mode), block_override, force_rgb);
	DestroyWindow(window);
	UnregisterClassA(window_class.lpszClassName, window_class.hInstance);
	OS::get_singleton()->set_exit_code(passed ? 0 : 1);
	return nullptr;
}
