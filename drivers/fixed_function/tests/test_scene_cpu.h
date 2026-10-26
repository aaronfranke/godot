#ifndef TEST_FIXED_FUNCTION_SCENE_CPU_H
#define TEST_FIXED_FUNCTION_SCENE_CPU_H

#include "drivers/fixed_function/scene_renderer.h"

// No window, renderer initialization, native device calls or GPU synchronization.
class TestFixedFunctionSceneCPU {
	friend class TestFixedFunctionLambert;
	friend class TestFixedFunctionTextureAdaptation;
	class FFUnusedDevice : public FixedFunctionGraphicsDevice {
		FFCapabilities _capabilities;
		uint32_t _pixel_limit;
		bool _compressed;

	public:
		FFUnusedDevice() :
				_pixel_limit(0), _compressed(false) {}
		using FixedFunctionGraphicsDevice::copy_texture_readback;
		using FixedFunctionGraphicsDevice::prepare_texture_upload;
		void set_capabilities(const FFCapabilities &p_caps) { _capabilities = p_caps; }
		void set_compressed(bool p_enabled) { _compressed = p_enabled; }
		bool supports_compressed_texture(Image::Format p_format) const override { return _compressed && p_format >= Image::FORMAT_DXT1 && p_format <= Image::FORMAT_DXT5; }
		void set_pixel_limit(uint32_t p_limit) { _pixel_limit = p_limit; }
		uint32_t get_max_per_pixel_lights() const override { return _pixel_limit; }
		Error initialize(void *, uint32_t, uint32_t, bool) override { return ERR_UNAVAILABLE; }
		Error finalize() override { return ERR_UNAVAILABLE; }
		Error resize(uint32_t, uint32_t) override { return ERR_UNAVAILABLE; }
		Error set_use_vsync(bool) override { return ERR_UNAVAILABLE; }
		Error prepare_resources() override { return ERR_UNAVAILABLE; }
		const FFCapabilities &get_capabilities() const override { return _capabilities; }
		uint32_t get_width() const override { return 0; }
		uint32_t get_height() const override { return 0; }
		uint32_t get_target_width() const override { return 0; }
		uint32_t get_target_height() const override { return 0; }
		Error texture_create(const Ref<Image> &, RID &, bool) override { return ERR_UNAVAILABLE; }
		Error texture_create_render_target(uint32_t, uint32_t, RID &, bool = false) override { return ERR_UNAVAILABLE; }
		bool texture_is_render_target_ready(RID) const override { return false; }
		Error texture_free(RID) override { return ERR_UNAVAILABLE; }
		Error begin_frame(uint32_t, RID, bool, bool) override { return ERR_UNAVAILABLE; }
		Error clear_color(uint32_t) override { return ERR_UNAVAILABLE; }
		Error draw_triangles(const FixedFunctionVertex *, uint32_t, const CameraMatrix &, RID, const FFDrawParameters &) override { return ERR_UNAVAILABLE; }
		Error end_frame(bool, bool) override { return ERR_UNAVAILABLE; }
		Error read_frame(uint32_t *, size_t) override { return ERR_UNAVAILABLE; }
	};
	static bool _falloff();
	static bool _occlusion_sampling();
	static bool _prepare(FixedFunctionSceneRenderer &p_renderer, const Transform &p_camera, const Vector<RID> &p_lights);
	static bool _snapshots(FixedFunctionRenderContext &p_context, FixedFunctionSceneRenderer &p_renderer);
	static bool _triangles(FixedFunctionRenderContext &p_context, FixedFunctionSceneRenderer &p_renderer, bool p_benchmark);
	static bool _pixel_parameters(FixedFunctionRenderContext &p_context, FixedFunctionSceneRenderer &p_renderer, FFUnusedDevice &p_device, bool p_benchmark);
	static bool _light_selection();
	static bool _generated_normals();
	static bool _deformed_tangents();
	static bool _texture_adaptation();

public:
	static bool test(bool p_benchmark = false);
};

#endif // TEST_FIXED_FUNCTION_SCENE_CPU_H
