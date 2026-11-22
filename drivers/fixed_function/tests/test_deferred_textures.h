#ifndef TEST_FIXED_FUNCTION_DEFERRED_TEXTURES_H
#define TEST_FIXED_FUNCTION_DEFERRED_TEXTURES_H

#include "drivers/fixed_function/render_target_manager.h"

#include <vector>

class TestFixedFunctionDeferredTextures {
	// All rendering forwards to the real backend. Hooks exercise request admission
	// during native calls; a one-shot error checks retry without depending on driver loss.
	class FFDevice : public FixedFunctionGraphicsDevice {
		Ref<Image> _replacement;
		RID _texture;
		FixedFunctionGraphicsDevice &_delegate;
		FixedFunctionTextureStorage *_storage;
		int _request;
		int _point;
		int _creations;
		Error _callback_error;
		Error _nested_error;
		Error _injected_error;

		void _apply_request();

	public:
		enum Request {
			NONE,
			REPLACE_DATA,
			REALLOCATE,
			FREE_RESOURCE,
			FAIL_UPLOAD
		};
		enum CallbackPoint {
			DURING_PREPARE,
			DURING_UPLOAD
		};
		void set_storage(FixedFunctionTextureStorage &p_storage) { _storage = &p_storage; }
		void arm(Request p_request, CallbackPoint p_point, RID p_texture, const Ref<Image> &p_image = Ref<Image>());
		void arm_failure(Error p_error);
		int get_creations() const { return _creations; }
		Error get_callback_error() const { return _callback_error; }
		Error get_nested_error() const { return _nested_error; }
		Error initialize(void *p_window, uint32_t p_width, uint32_t p_height, bool p_vsync) override;
		Error finalize() override;
		Error resize(uint32_t p_width, uint32_t p_height) override;
		Error set_use_vsync(bool p_enabled) override;
		Error prepare_resources() override;
		const FFCapabilities &get_capabilities() const override;
		uint32_t get_width() const override;
		uint32_t get_height() const override;
		uint32_t get_target_width() const override;
		uint32_t get_target_height() const override;
		bool supports_compressed_texture(Image::Format p_format) const override;
		Error texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame = false) override;
		Error texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent = false) override;
		bool texture_is_render_target_ready(RID p_texture) const override;
		Error texture_free(RID p_texture) override;
		Error begin_frame(uint32_t p_clear_color, RID p_target = RID(), bool p_clear = true, bool p_use_depth = false) override;
		Error clear_color(uint32_t p_color) override;
		Error clear_depth(float p_depth = 1.0f) override;
		Error draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_count, const CameraMatrix &p_projection, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters()) override;
		Error end_frame(bool p_present = true, bool p_discard = false) override;
		Error read_frame(uint32_t *r_pixels, size_t p_count) override;
		FFDevice(FixedFunctionGraphicsDevice &p_delegate);
	};

	static Ref<Image> _make_image(int p_size, const Color &p_color);
	static Ref<Image> _make_patch();
	static bool _check_snapshot(FixedFunctionTextureStorage &p_storage, RID p_texture, const uint32_t *p_expected);
	static bool _render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RID p_texture, const uint32_t *p_expected, std::vector<uint32_t> &r_pixels);
	static bool _test_reentrant_requests(FFDevice &p_device, FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, std::vector<uint32_t> &r_pixels);
	static bool _test_first_upload(FFDevice &p_device, FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, std::vector<uint32_t> &r_pixels);
	static bool _test_distance_field(FFDevice &p_device, FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, std::vector<uint32_t> &r_pixels);

public:
	enum {
		CAPTURE_WIDTH = 128,
		CAPTURE_HEIGHT = 96
	};
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_DEFERRED_TEXTURES_H
