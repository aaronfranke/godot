#pragma once

#include "drivers/fixed_function/scene_renderer.h"

class TestFixedFunctionSceneCPU {
	friend class TestFixedFunctionTextureAdaptation;
	class FFUnusedDevice : public FixedFunctionGraphicsDevice {
		FFCapabilities _capabilities;
		bool _compressed;

	public:
		FFUnusedDevice() :
				_compressed(false) {}
		using FixedFunctionGraphicsDevice::copy_texture_readback;
		using FixedFunctionGraphicsDevice::prepare_texture_upload;
		void set_capabilities(const FFCapabilities &p_caps) { _capabilities = p_caps; }
		void set_compressed(bool p_enabled) { _compressed = p_enabled; }
		bool supports_compressed_texture(Image::Format p_format) const override { return _compressed && p_format >= Image::FORMAT_DXT1 && p_format <= Image::FORMAT_DXT5; }
		Error initialize(void *, uint32_t, uint32_t, bool) override { return ERR_BUG; }
		Error finalize() override { return OK; }
		Error resize(uint32_t, uint32_t) override { return ERR_BUG; }
		Error set_use_vsync(bool) override { return ERR_BUG; }
		Error prepare_resources() override { return ERR_BUG; }
		const FFCapabilities &get_capabilities() const override { return _capabilities; }
		uint32_t get_max_per_pixel_lights() const override { return MAX_PIXEL_LIGHTS; }
		uint32_t get_width() const override { return 128; }
		uint32_t get_height() const override { return 96; }
		uint32_t get_target_width() const override { return 128; }
		uint32_t get_target_height() const override { return 96; }
		Error texture_create(const Ref<Image> &, RID &, bool) override { return ERR_BUG; }
		Error texture_create_render_target(uint32_t, uint32_t, RID &, bool = false) override { return ERR_BUG; }
		bool texture_is_render_target_ready(RID) const override { return false; }
		Error texture_free(RID) override { return ERR_BUG; }
		Error begin_frame(uint32_t, RID, bool, bool) override { return ERR_BUG; }
		Error clear_color(uint32_t) override { return ERR_BUG; }
		Error draw_triangles(const FixedFunctionVertex *, uint32_t, const Projection &, RID, const FFDrawParameters &) override { return ERR_BUG; }
		Error end_frame(bool, bool) override { return ERR_BUG; }
		Error read_frame(uint32_t *, size_t) override { return ERR_BUG; }
	};
	static bool _prepare(FixedFunctionSceneRenderer &p_renderer, const Vector<RID> &p_lights);
	static bool _generated_normals();
	static bool _falloff();
	static bool _occlusion_sampling();
	static bool _run(bool p_benchmark);
	static bool _texture_adaptation();

public:
	static bool test(bool p_benchmark = false);
	static bool test_timestamps();
	static bool test_deformed_tangents();
	static bool test_resource_contracts();
};
