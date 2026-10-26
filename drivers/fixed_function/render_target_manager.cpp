#include "render_target_manager.h"

#include "core/math/math_funcs.h"

#include <float.h>

Error FixedFunctionRenderTargetManager::_check_frame(bool p_check_target) const {
	const Error error = _context.get_frame_status();
	if (error != OK || _frame_error != OK) {
		return error != OK ? error : _frame_error;
	}
	return p_check_target ? _target_error : OK;
}

Error FixedFunctionRenderTargetManager::begin_frame(double p_frame_step) {
	const Error error = _context.begin_frame(p_frame_step);
	if (error == OK) {
		_target = RID();
		_clear_color = 0xff000000;
		_frame_error = OK;
		_target_error = OK;
		_clear_requested = _window_started = false;
	}
	return error;
}

Error FixedFunctionRenderTargetManager::begin_draw(bool p_use_depth) {
	Error error = _check_frame();
	if (error != OK) {
		return error;
	}
	FixedFunctionTextureStorage &storage = _context.get_texture_storage();
	const bool storage_clear = _target.is_valid() && storage.render_target_is_clear_requested(_target);
	if (storage_clear) {
		error = clear_render_target(storage.render_target_get_clear_request_color(_target));
		if (error != OK) {
			return error;
		}
	}
	if (p_use_depth && _context.is_frame_active() && !_context.is_frame_depth_enabled()) {
		// A canvas background may already have drawn. Load its color into a depth-enabled pass.
		error = _context.end_device_frame();
		if (error != OK) {
			return _frame_error = error;
		}
	}
	if (_context.is_frame_active()) {
		if (storage_clear) {
			storage.render_target_disable_clear_request(_target);
		}
		return OK;
	}
	if (_target.is_valid()) {
		// Preparation can discard contents, so test readiness only after recovery settles.
		error = _context.prepare_resources();
		if (error == OK) {
			const bool clear = _clear_requested || !_context.get_texture_storage().render_target_is_ready(_target);
			error = _context.begin_render_target(_target, _clear_color, clear, p_use_depth);
		}
	} else {
		error = _context.begin_device_frame(_clear_color, _clear_requested || !_window_started, p_use_depth);
		if (error == OK) {
			_window_started = true;
		}
	}
	if (error == OK) {
		if (storage_clear) {
			storage.render_target_disable_clear_request(_target);
		}
		_clear_requested = false;
	} else if (error == ERR_UNAVAILABLE && _target.is_valid() && _context.get_device().get_capabilities().render_target_opaque && _context.get_frame_status() == OK) {
		// A device with offscreen support can still reject an individual target's size or format.
		return report_draw_error(error);
	} else {
		_frame_error = error;
	}
	return error;
}

Error FixedFunctionRenderTargetManager::_finish_target() {
	if (_target_error != OK) {
		// Close an incomplete native pass without publishing it or ending the engine frame.
		return _context.is_frame_active() ? _context.end_device_frame(true) : OK;
	}
	Error error = OK;
	// An empty first draw with CLEAR_NEVER still needs deterministic, sampleable contents.
	if (_clear_requested || (_target.is_valid() && _context.get_texture_storage().render_target_is_clear_requested(_target)) || (_target.is_valid() && !_context.is_frame_active() && !_context.get_texture_storage().render_target_is_ready(_target))) {
		error = begin_draw();
		if (_target_error != OK) {
			return OK;
		}
	}
	if (error == OK && _context.is_frame_active()) {
		error = _context.end_device_frame();
	}
	if (error != OK) {
		_frame_error = error;
	}
	return error;
}

Error FixedFunctionRenderTargetManager::report_draw_error(Error p_error) {
	Error error = _check_frame();
	if (error != OK) {
		return error;
	}
	if (p_error == ERR_UNAVAILABLE && _target.is_valid()) {
		// TODO: Implement the missing viewport features instead of omitting their texture draws.
		// Discard unsupported previews independently so the editor GUI can still render.
		FixedFunctionTextureStorage &storage = _context.get_texture_storage();
		error = storage.render_target_set_failed(_target, true);
		if (error != OK) {
			return _frame_error = error;
		}
		_target_error = p_error;
		return p_error;
	}
	return _frame_error = p_error;
}

Error FixedFunctionRenderTargetManager::defer_render_target() {
	Error error = _check_frame();
	if (error != OK) {
		return error;
	}
	if (!_target.is_valid()) {
		return _frame_error = ERR_SKIP;
	}
	FixedFunctionTextureStorage &storage = _context.get_texture_storage();
	error = storage.render_target_set_failed(_target, false);
	if (error == OK) {
		// Retrying an incomplete UPDATE_ONCE target must survive the server disabling it.
		error = storage.render_target_mark_for_recovery(_target);
	}
	if (error != OK) {
		return _frame_error = error;
	}
	return _target_error = ERR_SKIP;
}

Error FixedFunctionRenderTargetManager::select_render_target(RID p_target) {
	Error error = _check_frame(false);
	if (error != OK) {
		return error;
	}
	if (p_target.is_valid() && !_context.get_texture_storage().owns_render_target(p_target)) {
		return ERR_INVALID_PARAMETER;
	}
	if (_target == p_target) {
		return _check_frame();
	}
	error = _finish_target();
	if (error == OK) {
		_target = p_target;
		_clear_color = is_render_target_transparent() ? 0 : 0xff000000;
		_clear_requested = false;
		_target_error = OK;
	}
	return error;
}

Error FixedFunctionRenderTargetManager::clear_render_target(const Color &p_color) {
	const Error error = _check_frame();
	if (error != OK) {
		return error;
	}
	for (int i = 0; i < 4; i++) {
		if (Math::is_nan(p_color[i]) || Math::is_inf(p_color[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	const Color color(CLAMP(p_color.r, 0.0f, 1.0f), CLAMP(p_color.g, 0.0f, 1.0f), CLAMP(p_color.b, 0.0f, 1.0f), CLAMP(p_color.a, 0.0f, 1.0f));
	if (_target.is_valid()) {
		// Explicit scene clears supersede the viewport's queued color. Keep the request
		// across skipped frames until the native clear has actually succeeded.
		const Error queue_error = _context.get_texture_storage().render_target_request_clear(_target, color);
		if (queue_error != OK) {
			return queue_error;
		}
	}
	_clear_color = color.to_argb32();
	if (_context.is_frame_active()) {
		_frame_error = _context.get_device().clear_color(_clear_color);
		if (_frame_error == OK && _target.is_valid()) {
			_context.get_texture_storage().render_target_disable_clear_request(_target);
		}
		return _frame_error;
	}
	_clear_requested = true;
	return OK;
}

Error FixedFunctionRenderTargetManager::blit_to_screen(RID p_target, const Rect2 &p_rect, int p_screen) {
	Error error = _check_frame();
	if (error != OK) {
		return error;
	}
	if (p_screen != 0) {
		return ERR_UNAVAILABLE;
	}
	FixedFunctionTextureStorage &storage = _context.get_texture_storage();
	if (_target.is_valid() || !storage.owns_render_target(p_target) || p_rect.size.x < 0 || p_rect.size.y < 0) {
		return ERR_INVALID_PARAMETER;
	}
	const real_t bounds[] = { p_rect.position.x, p_rect.position.y, p_rect.position.x + p_rect.size.x, p_rect.position.y + p_rect.size.y };
	for (int i = 0; i < 4; i++) {
		if (Math::is_nan(bounds[i]) || Math::is_inf(bounds[i]) || bounds[i] < -FLT_MAX || bounds[i] > FLT_MAX) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (p_rect.size.x == 0 || p_rect.size.y == 0) {
		return OK;
	}
	if (!storage.render_target_is_ready(p_target)) {
		return storage.render_target_mark_used(p_target);
	}
	RID texture;
	error = storage.texture_get_device_texture(storage.render_target_get_texture(p_target), texture);
	if (error != OK) {
		return error;
	}
	error = begin_draw();
	if (error != OK) {
		return error;
	}
	// Recovery during begin_draw may have invalidated this previously completed source.
	if (!storage.render_target_is_ready(p_target)) {
		return _frame_error = ERR_SKIP;
	}
	FixedFunctionGraphicsDevice &device = _context.get_device();
	const bool flip = storage.render_target_get_flag(p_target, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP);
	const bool transparent = storage.render_target_get_flag(p_target, FixedFunctionTextureStorage::RENDER_TARGET_TRANSPARENT);
	if (!transparent && !flip && p_rect == Rect2(0, 0, device.get_width(), device.get_height())) {
		error = device.copy_render_target_to_window(texture);
		if (error != ERR_UNAVAILABLE) {
			return _frame_error = error == OK ? storage.render_target_mark_used(p_target) : error;
		}
	}
	const float left = static_cast<float>(bounds[0]);
	const float top = static_cast<float>(bounds[1]);
	const float right = static_cast<float>(bounds[2]);
	const float bottom = static_cast<float>(bounds[3]);
	const float top_v = flip ? 1.0f : 0.0f;
	const float bottom_v = 1.0f - top_v;
	const FixedFunctionVertex vertices[] = {
		{ left, top, 0, 0xffffffff, 0, top_v },
		{ right, top, 0, 0xffffffff, 1, top_v },
		{ right, bottom, 0, 0xffffffff, 1, bottom_v },
		{ left, top, 0, 0xffffffff, 0, top_v },
		{ right, bottom, 0, 0xffffffff, 1, bottom_v },
		{ left, bottom, 0, 0xffffffff, 0, bottom_v },
	};
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	if (transparent) {
		state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_PREMULTIPLIED_ALPHA;
	}
	state.sampler_flags = storage.texture_get_sampler_flags(storage.render_target_get_texture(p_target));
	_frame_error = transparent ? ERR_UNAVAILABLE : device.copy_canvas_render_target(vertices, 6, texture, state);
	if (_frame_error == ERR_UNAVAILABLE) {
		Projection projection;
		projection.columns[0][0] = real_t(2) / device.get_width();
		projection.columns[1][1] = real_t(-2) / device.get_height();
		projection.columns[3][0] = -1;
		projection.columns[3][1] = 1;
		_frame_error = device.draw_triangles(vertices, 6, projection, texture, state);
	}
	if (_frame_error == OK) {
		_frame_error = storage.render_target_mark_used(p_target);
	}
	return _frame_error;
}

bool FixedFunctionRenderTargetManager::is_render_target_transparent() const {
	return _target.is_valid() && _context.get_texture_storage().render_target_get_flag(_target, FixedFunctionTextureStorage::RENDER_TARGET_TRANSPARENT);
}

Error FixedFunctionRenderTargetManager::end_frame(bool p_present) {
	Error error = _check_frame(false);
	if (error == OK && _target.is_valid()) {
		error = select_render_target(RID());
	}
	if (error == OK) {
		error = begin_draw();
	}
	if (error == OK) {
		return _context.end_frame(p_present);
	}
	// Preserve the original failure and close an open pass without presenting partial output.
	_context.cancel_frame();
	return error;
}

FixedFunctionRenderTargetManager::FixedFunctionRenderTargetManager(FixedFunctionRenderContext &p_context) :
		_context(p_context),
		_clear_color(0xff000000),
		_frame_error(OK),
		_target_error(OK),
		_clear_requested(false),
		_window_started(false) {
}
