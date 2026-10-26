#include "renderer_compositor_fixed_function.h"

#include "core/os/os.h"
#include "drivers/fixed_function/viewport_recovery.h"
#include "servers/rendering/rendering_server_globals.h"

RendererCompositorFixedFunction *RendererCompositorFixedFunction::_singleton = nullptr;

void RendererCompositorFixedFunction::make_current(RendererCompositor *(*p_factory)()) {
	_create_func = p_factory;
	low_end = true;
}

void RendererCompositorFixedFunction::_report_error(const char *p_operation) const {
	const Error error = _last_error;
	if (error != OK && error != ERR_SKIP) {
		ERR_PRINT(String("Fixed Function ") + p_operation + " failed (Error " + itos(error) + ").");
	}
}

void RendererCompositorFixedFunction::initialize() {
	_last_error = _context.initialize();
	if (_last_error == OK && !_context.get_device().get_capabilities().render_target_opaque) {
		ERR_PRINT("Fixed Function requires opaque render targets for Godot viewports.");
		_context.finalize();
		_last_error = ERR_UNAVAILABLE;
	}
	_report_error("initialize");
}

void RendererCompositorFixedFunction::finalize() {
	_last_error = _context.finalize();
	_report_error("finalize");
}

void RendererCompositorFixedFunction::_wait_for_frame(uint32_t p_remaining_usec) {
	OS::get_singleton()->delay_usec(p_remaining_usec);
}

void RendererCompositorFixedFunction::begin_frame(double p_frame_step) {
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
	_utilities.begin_frame_timestamps();
	_last_error = _render_targets.begin_frame(p_frame_step);
	_report_error("begin_frame");
	if (_last_error == OK && RSG::viewport) {
		_last_error = FixedFunctionViewportRecovery::update(_context.get_texture_storage(), *RSG::viewport);
		_report_error("viewport recovery");
	}
}

void RendererCompositorFixedFunction::end_frame(bool p_present) {
	_last_error = _render_targets.end_frame(p_present);
	_utilities.end_frame_timestamps();
	_report_error("end_frame");
}

void RendererCompositorFixedFunction::set_boot_image_with_stretch(const Ref<Image> &p_image, const Color &p_color, RSE::SplashStretchMode p_stretch_mode, bool p_use_filter) {
	const FixedFunctionGraphicsDevice &device = _context.get_device();
	Size2i upload_size;
	if (!device.get_capabilities().blend_mix || (p_image.is_valid() && device.get_texture_upload_size(p_image->get_width(), p_image->get_height(), upload_size) == ERR_UNAVAILABLE)) {
		return;
	}
	_last_error = _context.draw_boot_image(p_image, p_color, p_stretch_mode, true, p_use_filter);
	_report_error("set_boot_image_with_stretch");
}

RendererCompositorFixedFunction::RendererCompositorFixedFunction(FixedFunctionGraphicsDevice &p_device, void *p_native_window, uint32_t p_width, uint32_t p_height, bool p_vsync) :
		_context(p_device, p_native_window, p_width, p_height, p_vsync),
		_render_targets(_context),
		_textures(_context.get_texture_storage(), &_render_targets),
		_materials(_context),
		_canvas(_context, _render_targets),
		_scene(_context, _render_targets),
		_lights(_context),
		_meshes(_context),
		_utilities(_context, _textures, _materials, _lights, _scene),
		_last_error(ERR_UNCONFIGURED) {
	_singleton = this;
}

RendererCompositorFixedFunction::~RendererCompositorFixedFunction() {
	// Release Variant payloads while every facade they can call is still alive.
	_context.finalize();
	_singleton = nullptr;
}

void RendererCompositorFixedFunction::blit_render_targets_to_screen(DisplayServerEnums::WindowID p_screen, const RenderingServerTypes::BlitToScreen *p_render_targets, int p_amount) {
	if (p_amount < 0 || (p_amount && !p_render_targets)) {
		_last_error = ERR_INVALID_PARAMETER;
	} else if (p_screen != DisplayServerEnums::MAIN_WINDOW_ID) {
		_last_error = ERR_UNAVAILABLE;
	} else {
		_last_error = _render_targets.select_render_target(RID());
		for (int i = 0; i < p_amount && _last_error == OK; i++) {
			const RenderingServerTypes::BlitToScreen &blit = p_render_targets[i];
			if (blit.multi_view.use_layer || blit.lens_distortion.apply || blit.src_rect != Rect2(0, 0, 1, 1)) {
				_last_error = ERR_UNAVAILABLE;
			} else {
				_last_error = _render_targets.blit_to_screen(blit.render_target, Rect2(blit.dst_rect));
			}
		}
	}
	if (_last_error != OK) {
		_render_targets.report_draw_error(_last_error);
	}
	_report_error("blit_render_targets_to_screen");
}
