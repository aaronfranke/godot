#ifndef FIXED_FUNCTION_RENDER_TARGET_MANAGER_H
#define FIXED_FUNCTION_RENDER_TARGET_MANAGER_H

#include "render_context.h"

// Exclusively orchestrates the borrowed context during an engine frame. Selection and
// clear requests defer native scenes until drawing or a clear-only target is flushed.
class FixedFunctionRenderTargetManager {
	FixedFunctionRenderContext &_context;
	Color _clear_color;
	RID _target;
	Error _frame_error;
	Error _target_error;
	bool _clear_requested;
	bool _window_started;

	Error _check_frame(bool p_check_target = true) const;
	Error _finish_target();
	FixedFunctionRenderTargetManager(const FixedFunctionRenderTargetManager &);
	FixedFunctionRenderTargetManager &operator=(const FixedFunctionRenderTargetManager &);

public:
	Error begin_frame(double p_frame_step);
	Error select_render_target(RID p_target);
	RID get_current_render_target() const { return _target; }
	Color get_clear_color() const { return _clear_color; }
	bool is_render_target_transparent() const;
	Error clear_render_target(const Color &p_color);
	// Canvas/scene renderers call before emitting native draws to the selected target.
	Error begin_draw(bool p_use_depth = false);
	// Unsupported or temporarily incomplete offscreen output is discarded independently.
	// Native device/frame failures and invalid draws still cancel the frame.
	Error report_draw_error(Error p_error);
	// Only known texture dependencies may defer one target; native ERR_SKIP remains frame-wide.
	Error defer_render_target();
	Error blit_to_screen(RID p_target, const Rect2 &p_rect, int p_screen = 0);
	Error end_frame(bool p_present);

	FixedFunctionRenderTargetManager(FixedFunctionRenderContext &p_context);
};

#endif // FIXED_FUNCTION_RENDER_TARGET_MANAGER_H
