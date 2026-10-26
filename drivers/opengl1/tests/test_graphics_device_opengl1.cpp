#include "test_graphics_device_opengl1.h"

#include "core/os/os.h"
#include "drivers/fixed_function/tests/test_blending.h"
#include "drivers/fixed_function/tests/test_canvas_skinning.h"
#include "drivers/fixed_function/tests/test_clipping.h"
#include "drivers/fixed_function/tests/test_depth.h"
#include "drivers/fixed_function/tests/test_geometry.h"
#include "drivers/fixed_function/tests/test_lambert.h"
#include "drivers/fixed_function/tests/test_projection.h"
#include "drivers/fixed_function/tests/test_render_target_manager.h"
#include "drivers/fixed_function/tests/test_render_targets.h"
#include "drivers/fixed_function/tests/test_texture_adaptation.h"
#include "drivers/opengl1/graphics_device_opengl1.h"
#include "drivers/opengl1/opengl1_headers.h"
#include "drivers/opengl1/tests/test_mipmaps_opengl1.h"
#include "drivers/opengl1/wgl_context.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define FF_OPENGL1_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("OpenGL 1.1 FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

class TestFixedFunctionOpenGL1Probe {
	class GL11ProbeContext : public FixedFunctionWGLContext {
		bool _ext_only;
		bool _no_procs;
		bool _no_blend_procs;
		bool _broken_blend_procs;
		bool _broken_buffer_upload;

		static void APIENTRY _broken_blend(GLenum, GLenum, GLenum, GLenum) {
			glBlendFunc(GL_TEXTURE_2D, GL_ZERO); // Deliberately generate GL_INVALID_ENUM.
		}
		static void APIENTRY _broken_buffer_data(GLenum, ptrdiff_t, const void *, GLenum) {
			glEnable(GL_TRIANGLES); // Deliberately generate GL_INVALID_ENUM.
		}

	public:
		GL11ProbeContext(bool p_software, bool p_ext_only, bool p_no_procs, bool p_no_blend_procs, bool p_broken_blend_procs, bool p_broken_buffer_upload) :
				FixedFunctionWGLContext(p_software), _ext_only(p_ext_only), _no_procs(p_no_procs), _no_blend_procs(p_no_blend_procs), _broken_blend_procs(p_broken_blend_procs), _broken_buffer_upload(p_broken_buffer_upload) {}
		ProcAddress get_proc_address(const char *p_name) const override {
			const size_t length = strlen(p_name);
			if (_no_procs || (_ext_only && (length < 3 || strcmp(p_name + length - 3, "EXT")))) {
				return nullptr;
			}
			if (!strcmp(p_name, "glBlendFuncSeparate") || !strcmp(p_name, "glBlendFuncSeparateEXT")) {
				if (_no_blend_procs) {
					return nullptr;
				}
				if (_broken_blend_procs) {
					return reinterpret_cast<ProcAddress>(&_broken_blend);
				}
			}
			if (_broken_buffer_upload && (!strcmp(p_name, "glBufferData") || !strcmp(p_name, "glBufferDataARB"))) {
				return reinterpret_cast<ProcAddress>(&_broken_buffer_data);
			}
			return FixedFunctionWGLContext::get_proc_address(p_name);
		}
	};

	static Error _draw_quad(FixedFunctionGraphicsDeviceOpenGL1 &p_device, RID p_texture, float p_u, float p_v, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, uint32_t p_color = 0xffffffff) {
		const FixedFunctionVertex vertices[] = {
			{ -1, 1, 0, p_color, p_u, p_v },
			{ 1, 1, 0, p_color, p_u, p_v },
			{ 1, -1, 0, p_color, p_u, p_v },
			{ -1, 1, 0, p_color, p_u, p_v },
			{ 1, -1, 0, p_color, p_u, p_v },
			{ -1, -1, 0, p_color, p_u, p_v },
		};
		return p_device.draw_triangles(vertices, 6, Projection(), p_texture, p_state);
	}

	static bool _test_texture_views(FixedFunctionGraphicsDeviceOpenGL1 &p_device, std::vector<uint32_t> &r_pixels) {
		Ref<Image> image = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
		image->fill(Color(0, 1, 0, 1));

		image->set_pixel(0, 0, Color(1, 0, 0, 0));

		RID texture;
		FF_OPENGL1_CHECK(p_device.texture_create(image, texture) == OK);
		image->fill(Color(0, 0, 1, 1)); // Later lazy views must use the accepted snapshot.

		const struct {
			float u;
			float v;
			uint32_t expected;
			bool rgb;
			bool alpha;
			bool repeat_u;
		} cases[] = {
			{ -0.5f, -0.5f, 0xff0000ff, true, true, false },
			{ -0.5f, -0.5f, 0xffff0000, true, false, false },
			{ 1.25f, -0.5f, 0xffff0000, true, false, true },
			{ -0.5f, -0.5f, 0xff0000ff, false, true, false },
			{ 1.5f, -0.5f, 0xffffffff, false, true, false },
			{ 1.5f, -0.5f, 0xff00ff00, true, true, false },
		};
		const uint32_t width = p_device.get_width();
		const uint32_t height = p_device.get_height();
		r_pixels.resize(size_t(width) * height);
		for (const auto &test : cases) {
			FixedFunctionGraphicsDevice::FFDrawParameters state;
			state.alpha_cutoff_enabled = true;
			state.texture_rgb_enabled = test.rgb;
			state.texture_alpha_enabled = test.alpha;
			state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
			if (test.repeat_u) {
				state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U;
			}
			FF_OPENGL1_CHECK(p_device.begin_frame(0xff0000ff) == OK);
			FF_OPENGL1_CHECK(p_device.texture_free(texture) == ERR_BUSY);
			FF_OPENGL1_CHECK(_draw_quad(p_device, texture, test.u, test.v, state) == OK);
			FF_OPENGL1_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
			const uint32_t pixel = r_pixels[size_t(height / 2) * width + width / 2];
			if (pixel != test.expected) {
				printf("Texture view pixel: actual=%08x expected=%08x UV=%g,%g RGB=%d alpha=%d\n", pixel, test.expected, double(test.u), double(test.v), test.rgb, test.alpha);
				return false;
			}
		}
		FF_OPENGL1_CHECK(p_device.texture_free(texture) == OK);
		printf("PASS: OpenGL 1.1 independent texture channels, bilinear clamping, repeat and upload snapshots.\n");
		return true;
	}

	static bool _test_transparent_views(FixedFunctionGraphicsDeviceOpenGL1 &p_device, std::vector<uint32_t> &r_pixels) {
		if (!p_device.get_capabilities().render_target_transparent) {
			return true;
		}
		RID source, destination;
		const Size2i size(19, 11);
		Size2i upload_size;
		FF_OPENGL1_CHECK(p_device.get_texture_upload_size(size.x, size.y, upload_size) == OK);
		FF_OPENGL1_CHECK(p_device.texture_create_render_target(size.x, size.y, source, true) == OK);
		FF_OPENGL1_CHECK(p_device.texture_create_render_target(16, 8, destination) == OK);
		const struct GL11ViewCase {
			uint32_t color;
			uint32_t expected[2];
			bool rgb;
			bool alpha;
		} cases[] = {
			{ 0x80ffffff, { 0xff200000, 0xff000020 }, true, false },
			{ 0x8000ff00, { 0xff004000, 0xff006000 }, false, true },
			{ 0x8000ff00, { 0xff008000, 0xff008000 }, false, false },
		};
		for (int revision = 0; revision < 2; revision++) {
			const uint32_t clear = revision == 0 ? 0x80400000 : 0xc0000040;
			FF_OPENGL1_CHECK(p_device.begin_frame(clear, source) == OK && p_device.end_frame(false) == OK);
			for (int target = 0; target < 2; target++) {
				for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
					FixedFunctionGraphicsDevice::FFDrawParameters state;
					state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
					state.texture_rgb_enabled = cases[i].rgb;
					state.texture_alpha_enabled = cases[i].alpha;
					FF_OPENGL1_CHECK(p_device.begin_frame(0, target ? destination : RID()) == OK);
					FF_OPENGL1_CHECK(_draw_quad(p_device, source, 0.5f, 0.5f, state, cases[i].color) == OK);
					FF_OPENGL1_CHECK(p_device.end_frame(false) == OK);
					r_pixels.resize(size_t(p_device.get_width()) * p_device.get_height());
					if (target) {
						FF_OPENGL1_CHECK(p_device.texture_read_render_target(destination, r_pixels.data(), r_pixels.size()) == OK);
					} else {
						FF_OPENGL1_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
					}
					const uint32_t pixel = r_pixels[0];
					if (pixel != cases[i].expected[revision]) {
						printf("Transparent view revision=%d target=%d RGB=%d alpha=%d actual=%08x expected=%08x\n", revision, target, cases[i].rgb, cases[i].alpha, pixel, cases[i].expected[revision]);
						return false;
					}
				}
			}
			FixedFunctionGraphicsDevice::FFTextureInfo info;
			FF_OPENGL1_CHECK(p_device.texture_get_info(source, info) == OK && info.bytes == uint64_t(upload_size.x) * upload_size.y * 8);
		}
		FF_OPENGL1_CHECK(p_device.texture_free(source) == OK && p_device.texture_free(destination) == OK);
		FF_OPENGL1_CHECK(p_device.get_texture_memory_usage() == 0);
		printf("PASS: transparent FBO RGB/alpha views, lazy copy refresh, native memory and window/FBO binding restoration.\n");
		return true;
	}

	static bool _test_npot(FixedFunctionGraphicsDeviceOpenGL1 &p_device, std::vector<uint32_t> &r_pixels) {
		Ref<Image> image = Image::create_empty(3, 5, false, Image::FORMAT_RGBA8);
		image->fill(Color(0, 0, 1, 1));

		image->set_pixel(0, 0, Color(1, 0, 0, 0));

		RID texture;
		const Error uploaded = p_device.texture_create(image, texture);
		FF_OPENGL1_CHECK(uploaded == OK);
		FixedFunctionGraphicsDevice::FFTextureInfo info;
		FF_OPENGL1_CHECK(p_device.texture_get_info(texture, info) == OK && info.size == Size2i(3, 5));
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V;
		state.texture_alpha_enabled = false;
		const uint32_t width = p_device.get_width();
		const uint32_t height = p_device.get_height();
		r_pixels.resize(size_t(width) * height);
		FF_OPENGL1_CHECK(p_device.begin_frame(0xff000000) == OK);
		Size2i native_size;
		FF_OPENGL1_CHECK(p_device.get_texture_upload_size(3, 5, native_size) == OK);
		FF_OPENGL1_CHECK(_draw_quad(p_device, texture, 1 + 0.5f / native_size.x, -1 + 0.5f / native_size.y, state) == OK);
		FF_OPENGL1_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FF_OPENGL1_CHECK(r_pixels[size_t(height / 2) * width + width / 2] == 0xffff0000);
		FF_OPENGL1_CHECK(p_device.texture_free(texture) == OK);
		printf("PASS: OpenGL 1.1 native/adapted NPOT uploads and bilinear repeat.\n");
		return true;
	}

	static bool _test_framebuffers(FixedFunctionGraphicsDeviceOpenGL1 &p_device, std::vector<uint32_t> &r_pixels) {
		RID source, nested;
		if (!p_device.get_capabilities().render_target_opaque) {
			FF_OPENGL1_CHECK(p_device.texture_create_render_target(16, 8, source) == ERR_UNAVAILABLE && !source.is_valid());
			printf("PASS: OpenGL 1.1 window-only fallback.\n");
			return true;
		}
		const Size2i size(19, 11);
		FF_OPENGL1_CHECK(p_device.texture_create_render_target(size.x, size.y, source) == OK);
		FF_OPENGL1_CHECK(p_device.texture_create_render_target(16, 8, nested) == OK);
		FF_OPENGL1_CHECK(p_device.begin_frame(0, source, false) == ERR_UNCONFIGURED);
		FF_OPENGL1_CHECK(!p_device.texture_is_render_target_ready(source));
		const uint32_t width = p_device.get_width();
		const uint32_t height = p_device.get_height();
		r_pixels.resize(size_t(width) * height);
		FF_OPENGL1_CHECK(p_device.begin_frame(0xff00ff00) == OK && p_device.end_frame(false) == OK);
		FF_OPENGL1_CHECK(p_device.begin_frame(0x000000ff, source) == OK);
		FF_OPENGL1_CHECK(p_device.get_target_width() == uint32_t(size.x) && p_device.get_target_height() == uint32_t(size.y));
		FF_OPENGL1_CHECK(p_device.get_width() == width && p_device.get_height() == height);
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.clip_enabled = true;
		state.clip_rect = Rect2i(0, 0, size.x, size.y / 2);
		state.cull_mode = FixedFunctionGraphicsDevice::CULL_BACK;
		FF_OPENGL1_CHECK(_draw_quad(p_device, source, 0, 0, state) == ERR_INVALID_PARAMETER);
		FF_OPENGL1_CHECK(p_device.texture_free(source) == ERR_BUSY);
		FF_OPENGL1_CHECK(_draw_quad(p_device, RID(), 0, 0, state, 0x00ff0000) == OK);
		state.cull_mode = FixedFunctionGraphicsDevice::CULL_FRONT;
		FF_OPENGL1_CHECK(_draw_quad(p_device, RID(), 0, 0, state, 0xffffff00) == OK);
		FF_OPENGL1_CHECK(p_device.end_frame(false) == OK && p_device.texture_is_render_target_ready(source));
		FF_OPENGL1_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK && r_pixels[0] == 0xff00ff00);
		std::vector<uint32_t> readback(size_t(size.x) * size.y, 0x12345678);
		FF_OPENGL1_CHECK(p_device.texture_read_render_target(RID(), readback.data(), readback.size()) == ERR_INVALID_PARAMETER);
		FF_OPENGL1_CHECK(p_device.texture_read_render_target(source, readback.data(), readback.size() - 1) == ERR_INVALID_PARAMETER && readback.front() == 0x12345678);
		FF_OPENGL1_CHECK(p_device.texture_read_render_target(source, readback.data(), readback.size()) == OK && readback[size.x / 2] == 0xffff0000 && readback[readback.size() - 1] == 0xff0000ff);
		FF_OPENGL1_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK && r_pixels[0] == 0xff00ff00);

		FF_OPENGL1_CHECK(p_device.begin_frame(0xff000000, nested) == OK);
		state = FixedFunctionGraphicsDevice::FFDrawParameters();
		state.alpha_cutoff_enabled = true;
		state.alpha_cutoff = 1;
		// This must survive the cutoff even though the source draw used zero alpha.
		FF_OPENGL1_CHECK(_draw_quad(p_device, source, 0.5f, 0.25f, state) == OK);
		FF_OPENGL1_CHECK(p_device.end_frame(false) == OK && p_device.texture_is_render_target_ready(nested));
		FF_OPENGL1_CHECK(p_device.begin_frame(0, RID(), false) == OK);
		state.clip_enabled = true;
		state.clip_rect = Rect2i(0, 0, width / 2, height);
		FF_OPENGL1_CHECK(_draw_quad(p_device, source, 0.5f, 0.75f, state) == OK);
		state.clip_rect.position.x = width / 2;
		FF_OPENGL1_CHECK(_draw_quad(p_device, nested, 0.5f, 0.5f, state) == OK);
		FF_OPENGL1_CHECK(p_device.end_frame(false) == OK && p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FF_OPENGL1_CHECK(r_pixels[width / 4] == 0xff0000ff && r_pixels[width * 3 / 4] == 0xffff0000);

		FF_OPENGL1_CHECK(p_device.begin_frame(0, nested, false) == OK && !p_device.texture_is_render_target_ready(nested));
		FF_OPENGL1_CHECK(p_device.end_frame(true) == ERR_UNAVAILABLE && p_device.texture_is_render_target_ready(nested));
		FF_OPENGL1_CHECK(p_device.begin_frame(0, nested, false) == OK && p_device.end_frame(false, true) == OK);
		FF_OPENGL1_CHECK(!p_device.texture_is_render_target_ready(nested) && p_device.begin_frame(0, nested, false) == ERR_UNCONFIGURED);
		FF_OPENGL1_CHECK(p_device.resize(0, 0) == OK && p_device.prepare_resources() == ERR_SKIP);
		FF_OPENGL1_CHECK(p_device.begin_frame(0, source) == ERR_SKIP && p_device.texture_is_render_target_ready(source));
		FF_OPENGL1_CHECK(p_device.resize(width, height) == OK);
		FF_OPENGL1_CHECK(p_device.resize(width + 1, height + 1) == OK && p_device.texture_is_render_target_ready(source));
		FF_OPENGL1_CHECK(p_device.resize(width, height) == OK && p_device.texture_free(nested) == OK && p_device.texture_free(source) == OK);
		FF_OPENGL1_CHECK(p_device.get_texture_memory_usage() == 0);
		printf("PASS: OpenGL 1.1 FBO winding, clipping, opaque alpha, feedback, nested sampling and pass ownership.\n");
		return true;
	}

public:
	static bool run(HWND p_window, bool p_force_software, uint8_t p_features, bool p_ext_only, bool p_no_procs, bool p_no_blend_procs, bool p_broken_blend_procs, bool p_broken_buffer_upload) {
		GL11ProbeContext provider(p_force_software, p_ext_only, p_no_procs, p_no_blend_procs, p_broken_blend_procs, p_broken_buffer_upload);
		FixedFunctionGraphicsDeviceOpenGL1 device(provider, p_features);
		const HGLRC previous = wglGetCurrentContext();
		const HDC previous_dc = wglGetCurrentDC();
		const Error initialized = device.initialize(p_window, 192, 144, false);
		FF_OPENGL1_CHECK(initialized == OK);
		FF_OPENGL1_CHECK(!previous || (wglGetCurrentContext() == previous && wglGetCurrentDC() == previous_dc));
		FF_OPENGL1_CHECK(provider.make_current() == OK);
		printf("OpenGL 1.1 probe renderer: %s, version: %s\n", device.get_adapter_name().utf8().get_data(), reinterpret_cast<const char *>(glGetString(GL_VERSION)));
		FF_OPENGL1_CHECK(provider.release_current() == OK);
		FF_OPENGL1_CHECK(device.get_capabilities().depth_buffer);
		if (p_force_software || p_no_procs || !(p_features & FixedFunctionGraphicsDeviceOpenGL1::FEATURE_FRAMEBUFFER)) {
			FF_OPENGL1_CHECK(!device.get_capabilities().render_target_opaque);
		}
		if (p_force_software || !(p_features & FixedFunctionGraphicsDeviceOpenGL1::FEATURE_NPOT_TEXTURE)) {
			FF_OPENGL1_CHECK(device.get_capabilities().texture_power_of_two);
		}
		if (!device.get_capabilities().render_target_opaque || p_no_blend_procs || p_broken_blend_procs || !(p_features & FixedFunctionGraphicsDeviceOpenGL1::FEATURE_SEPARATE_ALPHA)) {
			FF_OPENGL1_CHECK(!device.get_capabilities().render_target_transparent);
		}
		printf("Transparent OpenGL FBOs: %s\n", device.get_capabilities().render_target_transparent ? "yes" : "no");
		std::vector<uint32_t> pixels;
		FF_OPENGL1_CHECK(_test_texture_views(device, pixels));
		FF_OPENGL1_CHECK(_test_npot(device, pixels));
		FF_OPENGL1_CHECK(_test_framebuffers(device, pixels));
		FF_OPENGL1_CHECK(_test_transparent_views(device, pixels));
		FF_OPENGL1_CHECK(TestFixedFunctionProjection::test(device, pixels));
		FF_OPENGL1_CHECK(TestFixedFunctionBlending::test(device, pixels));
		FF_OPENGL1_CHECK(TestFixedFunctionClipping::test(device, pixels));
		FF_OPENGL1_CHECK(device.resize(64, 64) == OK);
		RID geometry;
		FF_OPENGL1_CHECK(TestFixedFunctionGeometry::create(device, geometry));
		const auto &batch = device._geometry_owner.get_or_null(geometry)->batches[0];
		const bool buffered = batch.buffers[0] && batch.buffers[1];
		printf("Retained OpenGL geometry: %s\n", buffered ? "buffer objects" : "client arrays");
		if (p_force_software || p_no_procs || p_broken_buffer_upload) {
			FF_OPENGL1_CHECK(!buffered);
		}
		if (buffered) {
			FF_OPENGL1_CHECK(batch.vertices.is_empty() && batch.indices.is_empty() && batch.colors.is_empty());
		}
		std::vector<uint32_t> reference;
		FF_OPENGL1_CHECK(TestFixedFunctionGeometry::draw(device, geometry, true, reference));
		FF_OPENGL1_CHECK(TestFixedFunctionGeometry::draw(device, geometry, false, pixels) && pixels == reference);
		FixedFunctionGraphicsDevice::FFGeometryDrawInfo draw_info;
		FF_OPENGL1_CHECK(device.geometry_get_draw_info(geometry, draw_info) == OK && draw_info.vertex_count == 6 && draw_info.draw_calls == 1);
		const Color blue(0, 0, 1);
		FF_OPENGL1_CHECK(device.begin_frame(0xff000000) == OK);
		FF_OPENGL1_CHECK(device.draw_geometry(geometry, Projection(), RID(), FixedFunctionGraphicsDevice::FFDrawParameters(), &blue) == OK);
		FF_OPENGL1_CHECK(device.end_frame(false) == OK && device.read_frame(pixels.data(), pixels.size()) == OK);
		FF_OPENGL1_CHECK(pixels[48 * 64 + 64 / 5] == 0xff0000ff);
		FF_OPENGL1_CHECK(TestFixedFunctionGeometry::draw(device, geometry, false, pixels) && pixels == reference);
		FF_OPENGL1_CHECK(device.geometry_free(geometry) == OK);
		FF_OPENGL1_CHECK(TestFixedFunctionGeometry::test_lods(device));
		FF_OPENGL1_CHECK(device.begin_frame(0) == OK && device.end_frame(true) == OK);
		FF_OPENGL1_CHECK(device.read_frame(pixels.data(), pixels.size()) == ERR_UNCONFIGURED);
		FF_OPENGL1_CHECK(device.finalize() == OK && wglGetCurrentContext() == previous && wglGetCurrentDC() == previous_dc);
		FF_OPENGL1_CHECK(TestFixedFunctionDepth::test(device, p_window, pixels, true));
		FF_OPENGL1_CHECK(TestFixedFunctionRenderTargetManager::test(device, p_window, pixels));
		FF_OPENGL1_CHECK(TestFixedFunctionRenderTargets::test_transparency(device, p_window, true));

		FixedFunctionRenderContext context(device, p_window, 128, 96, false);
		FixedFunctionRenderTargetManager targets(context);
		FixedFunctionSceneRenderer renderer(context, targets);
		FF_OPENGL1_CHECK(context.initialize() == OK);
		FF_OPENGL1_CHECK(TestFixedFunctionOpenGL1Mipmaps::test(context, device, pixels));
		FF_OPENGL1_CHECK(TestFixedFunctionTextureAdaptation::test(context, device, pixels));
		FF_OPENGL1_CHECK(TestFixedFunctionCanvasSkinning::test(context, targets, pixels));
		FF_OPENGL1_CHECK(TestFixedFunctionLambert::test(context, targets, renderer));
		FF_OPENGL1_CHECK(context.finalize() == OK && wglGetCurrentContext() == previous && wglGetCurrentDC() == previous_dc);
		printf("PASS: OpenGL 1.1 window device, shared 3D Lambert path, presentation and context restoration.\n");
		return true;
	}
};

bool TestFixedFunctionGraphicsDeviceOpenGL1::test(const List<String> &p_args) {
	bool software = false;
	bool ext_only = false;
	bool no_procs = false;
	bool no_blend_procs = false;
	bool broken_blend_procs = false;
	bool broken_buffer_upload = false;
	uint8_t features = FixedFunctionGraphicsDeviceOpenGL1::FEATURE_ALL;
	for (const List<String>::Element *E = p_args.front(); E; E = E->next()) {
		software |= E->get() == "--opengl1-software";
		ext_only |= E->get() == "--opengl1-ext-fbo";
		no_procs |= E->get() == "--opengl1-no-fbo-procs";
		no_blend_procs |= E->get() == "--opengl1-no-blend-procs";
		broken_blend_procs |= E->get() == "--opengl1-broken-blend-procs";
		broken_buffer_upload |= E->get() == "--opengl1-broken-buffer-upload";
		if (E->get() == "--opengl1-disable-fbo") {
			features &= ~FixedFunctionGraphicsDeviceOpenGL1::FEATURE_FRAMEBUFFER;
		} else if (E->get() == "--opengl1-disable-npot") {
			features &= ~FixedFunctionGraphicsDeviceOpenGL1::FEATURE_NPOT_TEXTURE;
		} else if (E->get() == "--opengl1-disable-separate-alpha") {
			features &= ~FixedFunctionGraphicsDeviceOpenGL1::FEATURE_SEPARATE_ALPHA;
		}
	}
	WNDCLASSA window_class = {};
	window_class.style = CS_OWNDC;
	window_class.lpfnWndProc = DefWindowProcA;
	window_class.hInstance = GetModuleHandleA(nullptr);
	window_class.lpszClassName = "GodotOpenGL1Probe";
	if (!RegisterClassA(&window_class)) {
		return false;
	}
	HWND window = CreateWindowA(window_class.lpszClassName, "Godot Fixed Function OpenGL 1.1 probe",
			WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT, 384, 300,
			nullptr, nullptr, window_class.hInstance, nullptr);
	const bool passed = window && TestFixedFunctionOpenGL1Probe::run(window, software, features, ext_only, no_procs, no_blend_procs, broken_blend_procs, broken_buffer_upload);
	if (window) {
		DestroyWindow(window);
	}
	UnregisterClassA(window_class.lpszClassName, window_class.hInstance);
	return passed;
}

#undef FF_OPENGL1_CHECK

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_graphics_device_opengl1)

TEST_CASE("[FixedFunction][OpenGL1] Native window device probe" * doctest::skip()) {
	CHECK(TestFixedFunctionGraphicsDeviceOpenGL1::test(OS::get_singleton()->get_cmdline_args()));
}
