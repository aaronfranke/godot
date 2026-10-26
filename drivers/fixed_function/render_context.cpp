#include "render_context.h"

#include "core/math/math_funcs.h"

Error FixedFunctionRenderContext::_check_ready() const {
	if (!_initialized) {
		return ERR_UNCONFIGURED;
	}
	return Thread::get_caller_id() == _owner_thread ? OK : ERR_BUG;
}

Error FixedFunctionRenderContext::initialize() {
	if (_initialized) {
		_last_error = ERR_ALREADY_IN_USE;
	} else {
		_last_error = _device.initialize(_native_window, _requested_width, _requested_height, _use_vsync);
		if (_last_error == OK) {
			_owner_thread = Thread::get_caller_id();
			_initialized = true;
			_time_total = _frame_delta = 0;
			_frame_number = 1;
		}
	}
	return _last_error;
}

Error FixedFunctionRenderContext::finalize() {
	_last_error = _initialized ? _check_ready() : _texture_storage.clear();
	if (!_initialized) {
		_material_storage.clear();
		_mesh_storage.clear();
		if (_last_error == OK) {
			_last_error = _geometry_cache.clear();
		}
		_light_storage.clear();
		_scene_storage.clear();
		_storage_queries.clear();
	}
	if (_initialized && _last_error == OK) {
		// Storage releases native textures outside a frame, before device shutdown.
		Error frame_error = OK;
		if (_frame_active) {
			frame_error = _device.end_frame(false);
			_frame_active = _frame_depth = false;
		}
		_frame_open = false;
		_frame_target = RID();
		_last_error = _texture_storage.clear();
		if (_last_error != OK) {
			return _last_error;
		}
		// Parameters may retain Mesh resources whose destructors free their RIDs.
		// Material clear also detaches the still-live meshes' borrowed material slots.
		_material_storage.clear();
		_mesh_storage.clear();
		_last_error = _geometry_cache.clear();
		if (_last_error != OK) {
			return _last_error;
		}
		_light_storage.clear();
		_scene_storage.clear();
		_storage_queries.clear();
		_last_error = _device.finalize();
		if (_last_error == OK) {
			_initialized = _frame_open = _frame_active = _frame_depth = false;
			_frame_error = OK;
			_owner_thread = 0;
			_last_error = frame_error;
		}
	}
	return _last_error;
}

Error FixedFunctionRenderContext::resize(uint32_t p_width, uint32_t p_height) {
	_last_error = _check_ready();
	if (_last_error == OK) {
		// Keep resize requests made by window callbacks during a native reset.
		_last_error = _device.resize(p_width, p_height);
		if (_last_error == OK) {
			_requested_width = p_width;
			_requested_height = p_height;
		}
	}
	return _last_error;
}

Error FixedFunctionRenderContext::set_use_vsync(bool p_enabled) {
	_last_error = _check_ready();
	if (_last_error == OK) {
		_last_error = _device.set_use_vsync(p_enabled);
		if (_last_error == OK) {
			_use_vsync = p_enabled;
		}
	}
	return _last_error;
}

Error FixedFunctionRenderContext::set_shader_time_scale(float p_scale) {
	_last_error = _check_ready();
	if (_last_error == OK) {
		if (Math::is_nan(p_scale) || Math::is_inf(p_scale)) {
			_last_error = ERR_INVALID_PARAMETER;
		} else {
			_time_scale = p_scale;
		}
	}
	return _last_error;
}

Error FixedFunctionRenderContext::_open_frame() {
	Error error = _check_ready();
	if (error != OK) {
		return error;
	}
	if (_frame_open) {
		return ERR_BUSY;
	}
	_frame_open = true;
	_frame_error = OK;
	return OK;
}

Error FixedFunctionRenderContext::_prepare_resources(bool p_release_targets) {
	_last_error = _check_ready();
	if (_last_error == OK) {
		_last_error = _texture_storage.update_pending(p_release_targets);
	}
	if (_last_error == OK) {
		if (!_frame_active) {
			_last_error = _geometry_cache.update_pending();
		}
	}
	if (_last_error == OK) {
		_last_error = _device.prepare_resources();
	}
	return _last_error;
}

Error FixedFunctionRenderContext::prepare_resources() {
	return _prepare_resources(!_frame_open);
}

Error FixedFunctionRenderContext::begin_frame(double p_frame_step) {
	if ((Math::is_nan(p_frame_step) || Math::is_inf(p_frame_step)) || p_frame_step < 0) {
		_last_error = ERR_INVALID_PARAMETER;
	} else {
		_last_error = _open_frame();
		if (_last_error == OK) {
			// Prepare before the engine updates dirty resources; the native scene stays deferred.
			// A skipped device still permits the logical frame/clock, but cannot draw this frame.
			_geometry_cache.advance_frame(_frame_number + 1);
			_mesh_storage.advance_frame(_frame_number + 1);
			_texture_storage.advance_frame(_frame_number + 1);
			_frame_error = _prepare_resources(true);
			_frame_number++;
			_storage_queries.begin_frame();
			_frame_delta = p_frame_step;
			// Preserve 3.x shader-time scaling while retaining the unscaled frame delta.
			_time_total += p_frame_step * _time_scale;
			_last_error = OK;
		}
	}
	return _last_error;
}

Error FixedFunctionRenderContext::begin_device_frame(uint32_t p_clear_color, bool p_clear, bool p_use_depth) {
	return _begin_device_frame(p_clear_color, RID(), p_clear, p_use_depth);
}

Error FixedFunctionRenderContext::begin_render_target(RID p_render_target, uint32_t p_clear_color, bool p_clear, bool p_use_depth) {
	if (!p_render_target.is_valid()) {
		return _last_error = ERR_INVALID_PARAMETER;
	}
	return _begin_device_frame(p_clear_color, p_render_target, p_clear, p_use_depth);
}

Error FixedFunctionRenderContext::_begin_device_frame(uint32_t p_clear_color, RID p_render_target, bool p_clear, bool p_use_depth) {
	Error error = _check_ready();
	if (error != OK) {
		return _last_error = error;
	}
	if (!_frame_open) {
		return _last_error = ERR_UNCONFIGURED;
	}
	if (_frame_active && (_frame_target != p_render_target || (p_use_depth && !_frame_depth))) {
		return _last_error = ERR_BUSY;
	}
	if (!_frame_active && _frame_error == OK) {
		if (p_render_target.is_valid() && !_texture_storage.owns_render_target(p_render_target)) {
			return _last_error = ERR_INVALID_PARAMETER;
		}
		_frame_error = prepare_resources();
		if (_frame_error != OK) {
			return _last_error = _frame_error;
		}
		RID native_target;
		if (p_render_target.is_valid()) {
			error = _texture_storage.render_target_allocate(p_render_target);
			if (error == OK) {
				error = _texture_storage.texture_get_device_texture(_texture_storage.render_target_get_texture(p_render_target), native_target);
			}
			if (error != OK) {
				return _last_error = error;
			}
		}
		_frame_error = _device.begin_frame(p_clear_color, native_target, p_clear, p_use_depth);
		// A reset between window passes invalidates earlier composition; rebuild next frame.
		if (_frame_error == ERR_UNCONFIGURED && !p_render_target.is_valid() && !p_clear) {
			_frame_error = ERR_SKIP;
		}
		_frame_active = _frame_error == OK;
		_frame_depth = _frame_active && p_use_depth;
		_frame_target = _frame_active ? p_render_target : RID();
	}
	// Do not retry a skipped/failed begin within the same logical frame.
	_last_error = _frame_error;
	return _last_error;
}

Error FixedFunctionRenderContext::end_render_target() {
	_last_error = _check_ready();
	if (_last_error != OK) {
		return _last_error;
	}
	if (!_frame_open || !_frame_active || !_frame_target.is_valid()) {
		return _last_error = ERR_UNCONFIGURED;
	}
	return end_device_frame();
}

Error FixedFunctionRenderContext::get_frame_status() const {
	const Error error = _check_ready();
	if (error != OK) {
		return error;
	}
	return _frame_open ? _frame_error : ERR_UNCONFIGURED;
}

Error FixedFunctionRenderContext::end_device_frame(bool p_discard) {
	_last_error = _check_ready();
	if (_last_error != OK) {
		return _last_error;
	}
	if (!_frame_open || !_frame_active) {
		return _last_error = ERR_UNCONFIGURED;
	}
	_frame_error = _device.end_frame(false, p_discard);
	_frame_active = _frame_depth = false;
	if (_frame_error == OK && _frame_target.is_valid() && !p_discard) {
		_frame_error = _texture_storage.render_target_set_failed(_frame_target, false);
		if (_frame_error == OK) {
			_frame_error = _texture_storage.render_target_mark_for_recovery(_frame_target);
		}
	}
	_frame_target = RID();
	return _last_error = _frame_error;
}

Error FixedFunctionRenderContext::cancel_frame() {
	_last_error = _check_ready();
	if (_last_error != OK) {
		return _last_error;
	}
	if (!_frame_open) {
		return _last_error = ERR_UNCONFIGURED;
	}
	_last_error = _frame_error;
	if (_frame_active) {
		const Error error = _device.end_frame(false, true);
		if (_last_error == OK) {
			_last_error = error;
		}
		_frame_active = _frame_depth = false;
	}
	_frame_open = false;
	_frame_target = RID();
	return _last_error;
}

Error FixedFunctionRenderContext::end_frame(bool p_swap_buffers) {
	_last_error = _check_ready();
	if (_last_error == OK) {
		if (!_frame_open) {
			_last_error = ERR_UNCONFIGURED;
		} else if (_frame_target.is_valid()) {
			return _last_error = ERR_BUSY;
		} else {
			_last_error = begin_device_frame(0xff000000);
			if (_last_error == OK) {
				_last_error = _device.end_frame(p_swap_buffers);
			}
			_frame_open = _frame_active = _frame_depth = false;
		}
	}
	return _last_error;
}

Error FixedFunctionRenderContext::_draw_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_present, bool p_use_filter) {
	Error error = _check_ready();
	if (error != OK) {
		return error;
	}
	if (_frame_open) {
		return ERR_BUSY;
	}
	if (p_image.is_null() || p_image->empty()) {
		return OK;
	}
	if (p_image->is_compressed()) {
		return ERR_UNAVAILABLE;
	}
	// Settle pending resize/recovery before uploading without opening or clearing a pass.
	error = prepare_resources();
	if (error != OK) {
		return error;
	}
	Ref<Image> image = p_image;
	if (image->get_format() != Image::FORMAT_RGBA8) {
		image = image->duplicate();
		image->convert(Image::FORMAT_RGBA8);
	}
	RID texture;
	error = _device.texture_create(image, texture);
	if (error != OK) {
		return error;
	}
	_open_frame();
	error = begin_device_frame(p_color.to_argb32());
	if (error == OK) {
		const real_t width = _device.get_width();
		const real_t height = _device.get_height();
		Size2 size(image->get_width(), image->get_height());
		if (p_scale) {
			// Match GLES2's boot-image scaling along the shorter window dimension.
			size *= width > height ? height / size.y : width / size.x;
		}
		Vector2 position = (Size2(width, height) - size) / 2;
		if (!p_scale) {
			position = position.floor();
		}
		const float left = static_cast<float>(position.x);
		const float top = static_cast<float>(position.y);
		const float right = static_cast<float>(position.x + size.x);
		const float bottom = static_cast<float>(position.y + size.y);
		const FixedFunctionVertex vertices[] = {
			{ left, top, 0, 0xffffffff, 0, 0 },
			{ right, top, 0, 0xffffffff, 1, 0 },
			{ right, bottom, 0, 0xffffffff, 1, 1 },
			{ left, top, 0, 0xffffffff, 0, 0 },
			{ right, bottom, 0, 0xffffffff, 1, 1 },
			{ left, bottom, 0, 0xffffffff, 0, 1 },
		};
		CameraMatrix projection;
		projection.matrix[0][0] = real_t(2) / width;
		projection.matrix[1][1] = real_t(-2) / height;
		projection.matrix[3][0] = -1;
		projection.matrix[3][1] = 1;
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		// Boot filtering is a preference; preserve the nearest fallback on limited devices.
		if (p_use_filter && _device.get_capabilities().sampler_filter_linear) {
			state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
		}
		error = _device.draw_triangles(vertices, 6, projection, texture, state);
	}
	// Close a successful begin even if drawing fails, then free outside the frame.
	end_frame(error == OK && p_present);
	if (error == OK) {
		error = _last_error;
	}
	const Error free_error = _device.texture_free(texture);
	return error == OK ? free_error : error;
}

Error FixedFunctionRenderContext::draw_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_present, bool p_use_filter) {
	_last_error = _draw_boot_image(p_image, p_color, p_scale, p_present, p_use_filter);
	return _last_error;
}

FixedFunctionRenderContext::FixedFunctionRenderContext(FixedFunctionGraphicsDevice &p_device, void *p_native_window, uint32_t p_width, uint32_t p_height, bool p_vsync) :
		_device(p_device),
		_texture_storage(p_device, &_scene_storage),
		_mesh_storage(_material_storage),
		_geometry_cache(p_device, _mesh_storage),
		_storage_queries(p_device, _texture_storage, _material_storage, _mesh_storage, _light_storage),
		_native_window(p_native_window),
		_owner_thread(0),
		_frame_number(1),
		_time_total(0),
		_frame_delta(0),
		_requested_width(p_width),
		_requested_height(p_height),
		_time_scale(1),
		_last_error(ERR_UNCONFIGURED),
		_frame_error(OK),
		_use_vsync(p_vsync),
		_initialized(false),
		_frame_open(false),
		_frame_active(false),
		_frame_depth(false) {
}

FixedFunctionRenderContext::~FixedFunctionRenderContext() {
	const Error error = finalize();
	if (error != OK) {
		ERR_PRINT(String("Fixed Function context finalize failed (Error ") + itos(error) + ").");
	}
}
