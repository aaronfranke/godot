#ifndef RASTERIZER_FIXED_FUNCTION_H
#define RASTERIZER_FIXED_FUNCTION_H

#include "rasterizer_canvas_fixed_function.h"
#include "rasterizer_scene_fixed_function.h"
#include "rasterizer_storage_fixed_function.h"

#include "servers/visual/rasterizer.h"

// Concrete root for the restricted 2D/opaque-unlit-3D profile. Platform selection is separate.
class RasterizerFixedFunction : public Rasterizer {
	enum {
		// Avoid system instability problems due to extremely high frame rates (above about 3000-4000 FPS).
		MIN_FRAME_INTERVAL_USEC = 250,
	};

	// Fill the inherited x86_32 vptr alignment gap before the context's 64-bit fields.
	Error _last_error;
	FixedFunctionRenderContext _context;
	FixedFunctionRenderTargetManager _render_targets;
	RasterizerStorageFixedFunction _storage;
	RasterizerCanvasFixedFunction _canvas;
	RasterizerSceneFixedFunction _scene;
	uint64_t _next_frame_usec;

	void _report_error(const char *p_operation) const;

	RasterizerFixedFunction(const RasterizerFixedFunction &);
	RasterizerFixedFunction &operator=(const RasterizerFixedFunction &);

protected:
	virtual void _wait_for_frame(uint32_t p_remaining_usec);
	FixedFunctionRenderContext &get_render_context() { return _context; }
	FixedFunctionRenderTargetManager &get_render_target_manager() { return _render_targets; }

public:
	RasterizerStorage *get_storage() override { return &_storage; }
	RasterizerCanvas *get_canvas() override { return &_canvas; }
	RasterizerScene *get_scene() override { return &_scene; }
	void initialize() override;
	void finalize() override;
	void begin_frame(double p_frame_step) override;
	void end_frame(bool p_present) override;
	void set_current_render_target(RID p_render_target) override;
	void restore_render_target(bool p_3d) override;
	void clear_render_target(const Color &p_color) override;
	void blit_render_target_to_screen(RID p_render_target, const Rect2 &p_screen_rect, int p_screen = 0) override;
	void output_lens_distorted_to_screen(RID p_render_target, const Rect2 &p_screen_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample) override;
	void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) override;
	void set_shader_time_scale(float p_scale) override;
	bool is_low_end() const override { return true; }
	double get_time_total() const { return _context.get_total_time(); }
	double get_frame_delta() const { return _context.get_frame_delta_time(); }
	bool is_initialized() const { return _context.is_initialized(); }
	bool is_frame_active() const { return _context.is_frame_active(); }

	Error resize(uint32_t p_width, uint32_t p_height) { return _last_error = _context.resize(p_width, p_height); }
	Error set_use_vsync(bool p_enabled) { return _last_error = _context.set_use_vsync(p_enabled); }
	Error get_last_error() const { return _last_error; }

	RasterizerFixedFunction(FixedFunctionGraphicsDevice &p_device, void *p_native_window, uint32_t p_width, uint32_t p_height, bool p_vsync);
	~RasterizerFixedFunction() override;
};

#endif // RASTERIZER_FIXED_FUNCTION_H
