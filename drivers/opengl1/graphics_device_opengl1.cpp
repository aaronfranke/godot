#include "graphics_device_opengl1.h"

#include "drivers/opengl1/opengl1_headers.h"

#include <string.h>

FixedFunctionGraphicsDeviceOpenGL1::FixedFunctionGraphicsDeviceOpenGL1(FixedFunctionOpenGL1Context &p_context, uint8_t p_optional_features) :
		_context(p_context),
		_framebuffers(nullptr),
		_blending(nullptr),
		_render_target(nullptr),
		_owner_thread(0),
		_max_renderbuffer_size(0),
		_optional_features(p_optional_features),
		_initialized(false),
		_frame_active(false),
		_frame_readable(false),
		_depth_active(false),
		_window_has_depth(false),
		_window_depth_initialized(false),
		_clamp_to_edge(false),
		_owns_context(false) {}

FixedFunctionGraphicsDeviceOpenGL1::FixedFunctionGraphicsDeviceOpenGL1(FixedFunctionOpenGL1Context *p_owned_context, uint8_t p_optional_features) :
		FixedFunctionGraphicsDeviceOpenGL1(*p_owned_context, p_optional_features) {
	_owns_context = true;
}

FixedFunctionGraphicsDeviceOpenGL1::~FixedFunctionGraphicsDeviceOpenGL1() {
	finalize();
	if (_owns_context) {
		memdelete(&_context);
	}
}

FixedFunctionGraphicsDeviceOpenGL1::GL11CurrentContext::GL11CurrentContext(FixedFunctionGraphicsDeviceOpenGL1 &p_device) :
		context(p_device._context), error(p_device._make_current()) {}

FixedFunctionGraphicsDeviceOpenGL1::GL11CurrentContext::~GL11CurrentContext() {
	if (error == OK && context.release_current() != OK) {
		ERR_PRINT("Could not restore the previous OpenGL context.");
	}
}

Error FixedFunctionGraphicsDeviceOpenGL1::_make_current() {
	if (!_initialized) {
		return ERR_UNCONFIGURED;
	}
	if (Thread::get_caller_id() != _owner_thread) {
		return ERR_BUG;
	}
	return _context.make_current();
}

bool FixedFunctionGraphicsDeviceOpenGL1::_has_extension(const char *p_extensions, const char *p_name) {
	if (!p_extensions) {
		return false;
	}
	const size_t length = strlen(p_name);
	for (const char *match = strstr(p_extensions, p_name); match; match = strstr(match + length, p_name)) {
		if ((match == p_extensions || match[-1] == ' ') && (!match[length] || match[length] == ' ')) {
			return true;
		}
	}
	return false;
}

Error FixedFunctionGraphicsDeviceOpenGL1::_check_gl_error() {
	GLenum error = glGetError();
	if (error == GL_NO_ERROR) {
		return OK;
	}
	Error result = FAILED;
	// Keep an optional operation's errors from leaking into subsequent core GL calls.
	for (; error != GL_NO_ERROR; error = glGetError()) {
		if (error == GL_OUT_OF_MEMORY) {
			result = ERR_OUT_OF_MEMORY;
		}
	}
	return result;
}

Error FixedFunctionGraphicsDeviceOpenGL1::initialize(void *p_native_window, uint32_t p_width, uint32_t p_height, bool p_vsync) {
	if (_initialized) {
		return ERR_ALREADY_IN_USE;
	}
	if (!p_native_window || !p_width || !p_height) {
		return ERR_INVALID_PARAMETER;
	}
	Error error = _context.initialize(p_native_window);
	if (error != OK) {
		return error;
	}
	_owner_thread = Thread::get_caller_id();
	_initialized = true;
	error = _initialize_gl(p_width, p_height);
	if (error != OK) {
		finalize();
	}
	return error;
}

Error FixedFunctionGraphicsDeviceOpenGL1::_initialize_gl(uint32_t p_width, uint32_t p_height) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	const String version = String::utf8(reinterpret_cast<const char *>(glGetString(GL_VERSION)));
	const int major = version.get_slicec('.', 0).to_int();
	const int minor = version.get_slicec('.', 1).to_int();
	if (major < 1 || (major == 1 && minor < 1)) {
		return ERR_UNAVAILABLE;
	}
	const char *extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
	_clamp_to_edge = major >= 2 || minor >= 2 || _has_extension(extensions, "GL_EXT_texture_edge_clamp") || _has_extension(extensions, "GL_SGIS_texture_edge_clamp");
	GLint max_texture = 0;
	GLint max_viewport[2] = {};
	GLint depth_bits = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport);
	glGetIntegerv(GL_DEPTH_BITS, &depth_bits);
	_max_viewport = Size2i(max_viewport[0], max_viewport[1]);
	Error error = _check_gl_error();
	if (error != OK || max_texture <= 0 || _max_viewport.x <= 0 || _max_viewport.y <= 0) {
		return error == OK ? ERR_UNAVAILABLE : error;
	}
	error = resize(p_width, p_height);
	if (error != OK) {
		return error;
	}
	_adapter_name = String::utf8(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
	_capabilities.max_texture_width = max_texture;
	_capabilities.max_texture_height = max_texture;
	_capabilities.max_texture_stages = 1;
	_capabilities.max_primitives_per_draw = INT32_MAX / 12; // Three vertices with four staging bytes each.
	_capabilities.texture_power_of_two = !(_optional_features & FEATURE_NPOT_TEXTURE) || (major < 2 && !_has_extension(extensions, "GL_ARB_texture_non_power_of_two"));
	_capabilities.blend_mix = true;
	_capabilities.blend_premultiplied_alpha = true;
	_capabilities.blend_add = true;
	_capabilities.blend_add_unweighted = true;
	_capabilities.blend_mul = true;
	_capabilities.alpha_cutoff = true;
	_window_has_depth = depth_bits > 0;
	_capabilities.depth_buffer = _window_has_depth;
	_capabilities.sampler_filter_linear = true;
	_capabilities.sampler_mip_filters = MIP_FILTER_POINT | MIP_FILTER_LINEAR;
	_capabilities.sampler_repeat = true;
	_capabilities.sampler_address_independent = true;
	_capabilities.cull_front = true;
	_capabilities.cull_back = true;

	GLint max_lights = 0;
	glGetIntegerv(GL_MAX_LIGHTS, &max_lights);
	_vertex_light_limit = MIN(uint32_t(MAX(max_lights, 0)), uint32_t(MAX_PIXEL_LIGHTS));
	_enabled_vertex_lights = 0;
	glDisable(GL_DITHER);
	glDisable(GL_FOG);
	_fog_enabled = false;
	_fog_begin = _fog_end = 0;
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	glFrontFace(GL_CW);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	_model_view_loaded = false;
	_model_view_scale = 1;
	glMatrixMode(GL_PROJECTION);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	error = _check_gl_error();
	if (error != OK) {
		return error;
	}
	if ((_optional_features & FEATURE_FRAMEBUFFER) && _clamp_to_edge) {
		_initialize_framebuffers(major >= 3 || _has_extension(extensions, "GL_ARB_framebuffer_object"), _has_extension(extensions, "GL_EXT_framebuffer_object"));
		_capabilities.depth_buffer |= _capabilities.render_target_opaque;
	}
	if (_capabilities.render_target_opaque && (_optional_features & FEATURE_SEPARATE_ALPHA)) {
		_initialize_blending(major > 1 || (major == 1 && minor >= 4), _has_extension(extensions, "GL_EXT_blend_func_separate"));
	}
	_initialize_geometry_buffers(major > 1 || (major == 1 && minor >= 5), _has_extension(extensions, "GL_ARB_vertex_buffer_object"));
	if ((_optional_features & FEATURE_S3TC) && _clamp_to_edge && _has_extension(extensions, "GL_EXT_texture_compression_s3tc")) {
		if (major > 1 || minor >= 3) {
			_compressed_texture_upload = _context.get_proc_address("glCompressedTexImage2D");
		}
		if (!_compressed_texture_upload && _has_extension(extensions, "GL_ARB_texture_compression")) {
			_compressed_texture_upload = _context.get_proc_address("glCompressedTexImage2DARB");
		}
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::finalize() {
	if (!_initialized) {
		return OK;
	}
	if (Thread::get_caller_id() != _owner_thread) {
		return ERR_BUG;
	}
	// The provider releases the private context and all its GL objects together.
	const Error error = _context.finalize();
	_clear_framebuffer_api();
	_clear_blend_api();
	_clear_geometry_buffer_api();
	_compressed_texture_upload = nullptr;
	_render_target = nullptr;
	while (!_textures.is_empty()) {
		GL11Texture *texture = _textures.front()->get();
		_textures.pop_front();
		_texture_owner.free(texture->rid);
		memdelete(texture);
	}
	for (const RID &rid : _geometry_owner.get_owned_list()) {
		GL11Geometry *geometry = _geometry_owner.get_or_null(rid);
		_geometry_owner.free(rid);
		memdelete(geometry);
	}
	_draw_colors.clear();
	_adapter_name = String();
	_capabilities = FFCapabilities();
	_owner_thread = 0;
	_size = Size2i();
	_max_viewport = Size2i();
	_initialized = false;
	_frame_active = false;
	_frame_readable = false;
	_depth_active = false;
	_window_has_depth = false;
	_window_depth_initialized = false;
	_clamp_to_edge = false;
	return error;
}

Error FixedFunctionGraphicsDeviceOpenGL1::resize(uint32_t p_width, uint32_t p_height) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (p_width > uint32_t(_max_viewport.x) || p_height > uint32_t(_max_viewport.y)) {
		return ERR_UNAVAILABLE;
	}
	const Size2i size(p_width, p_height);
	if (_size != size) {
		_size = size;
		_frame_readable = false;
		_window_depth_initialized = false;
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::set_use_vsync(bool p_enabled) {
	// OpenGL 1.1 leaves swap timing to the window system.
	const GL11CurrentContext current(*this);
	return current.error;
}

Error FixedFunctionGraphicsDeviceOpenGL1::prepare_resources() {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	return _size.x && _size.y ? OK : ERR_SKIP;
}

Error FixedFunctionGraphicsDeviceOpenGL1::begin_frame(uint32_t p_clear_color, RID p_render_target, bool p_clear, bool p_use_depth) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (!_size.x || !_size.y) {
		return ERR_SKIP; // Suspension also defers offscreen drawing; retained FBO contents survive.
	}
	GL11Texture *target = nullptr;
	if (p_render_target.is_valid()) {
		if (!_capabilities.render_target_opaque) {
			return ERR_UNAVAILABLE;
		}
		if (!_texture_owner.owns(p_render_target)) {
			return ERR_INVALID_PARAMETER;
		}
		target = _texture_owner.get_or_null(p_render_target);
		if (!target->framebuffer) {
			return ERR_INVALID_PARAMETER;
		}
	} else {
		if (p_use_depth && !_window_has_depth) {
			return ERR_UNAVAILABLE;
		}
		if (!_size.x || !_size.y) {
			return ERR_SKIP;
		}
	}
	bool &readable = target ? target->rendered : _frame_readable;
	bool &depth_initialized = target ? target->depth_initialized : _window_depth_initialized;
	if (!p_clear && !readable) {
		return ERR_UNCONFIGURED;
	}
	_bind_framebuffer(target);
	Error error = _check_gl_error();
	if (error == OK && target && p_use_depth) {
		error = _allocate_framebuffer_depth(*target);
	}
	if (error != OK) {
		_bind_framebuffer(nullptr);
		return error;
	}
	_render_target = target;
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, target && target->transparent ? GL_TRUE : GL_FALSE);
	if (target) {
		target->dirty_views = (1 << TEXTURE_RGB) | (1 << TEXTURE_ALPHA);
	}
	glViewport(0, 0, target ? target->upload_size.x : get_target_width(), target ? target->upload_size.y : get_target_height());
	// FBO projection flips Y so the logical top is sampled at v=0.
	glFrontFace(target ? GL_CCW : GL_CW);
	_frame_active = true;
	readable = false;
	_depth_active = p_use_depth;
	if (p_clear) {
		error = clear_color(p_clear_color);
	}
	if (error == OK && _depth_active && (p_clear || !depth_initialized)) {
		error = clear_depth();
	}
	if (error == OK) {
		error = _check_gl_error();
	}
	if (error != OK) {
		_bind_framebuffer(nullptr);
		_render_target = nullptr;
		_frame_active = false;
		_depth_active = false;
		depth_initialized = false;
	}
	return error;
}

Error FixedFunctionGraphicsDeviceOpenGL1::clear_color(uint32_t p_color) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (!_frame_active) {
		return ERR_UNCONFIGURED;
	}
	glDisable(GL_SCISSOR_TEST);
	const bool transparent = _render_target && _render_target->transparent;
	const float alpha = transparent ? float(p_color >> 24) / 255 : 1;
	glClearColor(float((p_color >> 16) & 255) / 255, float((p_color >> 8) & 255) / 255, float(p_color & 255) / 255, alpha);
	if (_render_target) {
		// Clears write alpha even after a color-only auxiliary draw.
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
	glClear(GL_COLOR_BUFFER_BIT);
	if (_render_target) {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, transparent ? GL_TRUE : GL_FALSE);
	}
	return _check_gl_error();
}

Error FixedFunctionGraphicsDeviceOpenGL1::clear_depth(float p_depth) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (!_frame_active || !_depth_active) {
		return ERR_UNCONFIGURED;
	}
	if (!(p_depth >= 0 && p_depth <= 1)) {
		return ERR_INVALID_PARAMETER;
	}
	glDisable(GL_SCISSOR_TEST);
	glDepthMask(GL_TRUE);
	glClearDepth(p_depth);
	glClear(GL_DEPTH_BUFFER_BIT);
	const Error cleared = _check_gl_error();
	bool &depth_initialized = _render_target ? _render_target->depth_initialized : _window_depth_initialized;
	depth_initialized = cleared == OK;
	return cleared;
}

Error FixedFunctionGraphicsDeviceOpenGL1::end_frame(bool p_present, bool p_discard) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (!_frame_active) {
		return ERR_UNCONFIGURED;
	}
	_frame_active = false;
	_depth_active = false;
	glFlush();
	Error error = _check_gl_error();
	if (_render_target) {
		_bind_framebuffer(nullptr);
		const Error restored = _check_gl_error();
		if (error == OK) {
			error = restored;
		}
		_render_target->rendered = error == OK && !p_discard;
		if (!_render_target->rendered) {
			_render_target->depth_initialized = false;
		}
		_render_target = nullptr;
		if (error == OK && p_present && !p_discard) {
			return ERR_UNAVAILABLE;
		}
		return error;
	}
	if (error == OK && p_present && !p_discard) {
		error = _context.present();
	}
	_frame_readable = error == OK && !p_present && !p_discard;
	if (!_frame_readable) {
		_window_depth_initialized = false;
	}
	return error;
}

Error FixedFunctionGraphicsDeviceOpenGL1::read_frame(uint32_t *p_pixels, size_t p_pixel_count) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (!_frame_readable) {
		return ERR_UNCONFIGURED;
	}
	return _read_pixels(_size, p_pixels, p_pixel_count, true);
}
