#include "rasterizer_fixed_function.h"

#include "viewport_recovery.h"

#include "core/os/os.h"
#include "servers/visual/visual_server_globals.h"

void RasterizerFixedFunction::_report_error(const char *p_operation) const {
	const Error error = _last_error;
	if (error != OK && error != ERR_SKIP) {
		ERR_PRINT(String("Fixed Function ") + p_operation + " failed (Error " + itos(error) + ").");
	}
}

void RasterizerFixedFunction::initialize() {
	_last_error = _context.initialize();
	if (_last_error == OK && !_context.get_device().get_capabilities().render_target_opaque) {
		ERR_PRINT("Fixed Function requires opaque render targets for Godot viewports.");
		_context.finalize();
		_last_error = ERR_UNAVAILABLE;
	}
	_report_error("initialize");
}

void RasterizerFixedFunction::finalize() {
	_last_error = _context.finalize();
	_report_error("finalize");
}

void RasterizerFixedFunction::_wait_for_frame(uint32_t p_remaining_usec) {
	OS::get_singleton()->delay_usec(p_remaining_usec);
}

void RasterizerFixedFunction::begin_frame(double p_frame_step) {
	if (_context.is_initialized()) {
		const OS *os = OS::get_singleton();
		uint64_t now = os->get_ticks_usec();
		while (now < _next_frame_usec) {
			_wait_for_frame(uint32_t(_next_frame_usec - now));
			now = os->get_ticks_usec();
		}
		// Anchor the next deadline to this frame's actual start to avoid catch-up bursts.
		_next_frame_usec = now + MIN_FRAME_INTERVAL_USEC;
	}
	_last_error = _render_targets.begin_frame(p_frame_step);
	_report_error("begin_frame");
	if (_last_error == OK) {
		_canvas.reset_frame();
	}
	if (_last_error == OK && VSG::viewport) {
		_last_error = FixedFunctionViewportRecovery::update(_context.get_texture_storage(), *VSG::viewport);
		_report_error("viewport recovery");
	}
}

void RasterizerFixedFunction::end_frame(bool p_present) {
	_last_error = _render_targets.end_frame(p_present);
	_report_error("end_frame");
}

void RasterizerFixedFunction::set_current_render_target(RID p_render_target) {
	_last_error = _render_targets.select_render_target(p_render_target);
	_report_error("set_current_render_target");
}

void RasterizerFixedFunction::restore_render_target(bool p_3d) {
	// Every native draw rebinds its state; no separate 2D/3D restoration is needed yet.
	_last_error = _render_targets.begin_draw();
	_report_error("restore_render_target");
}

void RasterizerFixedFunction::clear_render_target(const Color &p_color) {
	_last_error = _render_targets.clear_render_target(p_color);
	_report_error("clear_render_target");
}

void RasterizerFixedFunction::blit_render_target_to_screen(RID p_render_target, const Rect2 &p_screen_rect, int p_screen) {
	_last_error = _render_targets.blit_to_screen(p_render_target, p_screen_rect, p_screen);
	_report_error("blit_render_target_to_screen");
}

void RasterizerFixedFunction::output_lens_distorted_to_screen(RID p_render_target, const Rect2 &p_screen_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample) {
	// Do not publish a partial frame after a request this profile cannot render.
	_render_targets.report_draw_error(ERR_UNAVAILABLE);
	_last_error = ERR_UNAVAILABLE;
	_report_error("output_lens_distorted_to_screen");
}

void RasterizerFixedFunction::set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter) {
	const FixedFunctionGraphicsDevice &device = _context.get_device();
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = device.get_capabilities();
	if (!caps.blend_mix) {
		return; // The splash uses alpha blending.
	}
	Size2i upload_size;
	if (p_image.is_valid() && device.get_texture_upload_size(p_image->get_width(), p_image->get_height(), upload_size) == ERR_UNAVAILABLE) {
		return;
	}
	_last_error = _context.draw_boot_image(p_image, p_color, p_scale, true, p_use_filter);
	_report_error("set_boot_image");
}

void RasterizerFixedFunction::set_shader_time_scale(float p_scale) {
	_last_error = _context.set_shader_time_scale(p_scale);
	_report_error("set_shader_time_scale");
}

RasterizerFixedFunction::RasterizerFixedFunction(FixedFunctionGraphicsDevice &p_device, void *p_native_window, uint32_t p_width, uint32_t p_height, bool p_vsync) :
		_last_error(ERR_UNCONFIGURED),
		_context(p_device, p_native_window, p_width, p_height, p_vsync),
		_render_targets(_context),
		_storage(_context.get_texture_storage(), _context.get_material_storage(), _context.get_mesh_storage(), _context.get_light_storage(), _context.get_scene_storage(), _context.get_storage_queries()),
		_canvas(_context, _render_targets),
		_scene(_context, _render_targets),
		_next_frame_usec(0) {
}

RasterizerFixedFunction::~RasterizerFixedFunction() {
	// Variant payload destructors may call storage APIs; finalize while the facades still live.
	_last_error = _context.finalize();
	_report_error("destruction");
}
