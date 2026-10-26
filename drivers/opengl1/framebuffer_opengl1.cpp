#include "graphics_device_opengl1.h"

#include "opengl1_headers.h"

#ifdef WINDOWS_ENABLED
#define GL11_APIENTRY APIENTRY
#else
#define GL11_APIENTRY
#endif

struct FixedFunctionGraphicsDeviceOpenGL1::GL11FramebufferAPI {
	// Core, ARB and EXT use the same tokens and entry-point signatures here.
	enum {
		FRAMEBUFFER = 0x8D40,
		FRAMEBUFFER_BINDING = 0x8CA6,
		RENDERBUFFER = 0x8D41,
		COLOR_ATTACHMENT = 0x8CE0,
		DEPTH_ATTACHMENT = 0x8D00,
		COMPLETE = 0x8CD5,
		MAX_RENDERBUFFER_SIZE = 0x84E8,
		DEPTH_COMPONENT16 = 0x81A5,
		DEPTH_COMPONENT24 = 0x81A6,
	};
	void(GL11_APIENTRY *gen_framebuffers)(GLsizei, GLuint *);
	void(GL11_APIENTRY *bind_framebuffer)(GLenum, GLuint);
	void(GL11_APIENTRY *delete_framebuffers)(GLsizei, const GLuint *);
	void(GL11_APIENTRY *framebuffer_texture_2d)(GLenum, GLenum, GLenum, GLuint, GLint);
	GLenum(GL11_APIENTRY *check_framebuffer_status)(GLenum);
	void(GL11_APIENTRY *gen_renderbuffers)(GLsizei, GLuint *);
	void(GL11_APIENTRY *bind_renderbuffer)(GLenum, GLuint);
	void(GL11_APIENTRY *delete_renderbuffers)(GLsizei, const GLuint *);
	void(GL11_APIENTRY *renderbuffer_storage)(GLenum, GLenum, GLsizei, GLsizei);
	void(GL11_APIENTRY *framebuffer_renderbuffer)(GLenum, GLenum, GLenum, GLuint);

	template <class T>
	static bool _load_proc(FixedFunctionOpenGL1Context &p_context, const char *p_name, const char *p_suffix, T &r_proc) {
		const CharString name = (String(p_name) + p_suffix).utf8();
		r_proc = reinterpret_cast<T>(p_context.get_proc_address(name.get_data()));
		return r_proc != nullptr;
	}

	bool load(FixedFunctionOpenGL1Context &p_context, const char *p_suffix) {
		return _load_proc(p_context, "glGenFramebuffers", p_suffix, gen_framebuffers) &&
				_load_proc(p_context, "glBindFramebuffer", p_suffix, bind_framebuffer) &&
				_load_proc(p_context, "glDeleteFramebuffers", p_suffix, delete_framebuffers) &&
				_load_proc(p_context, "glFramebufferTexture2D", p_suffix, framebuffer_texture_2d) &&
				_load_proc(p_context, "glCheckFramebufferStatus", p_suffix, check_framebuffer_status) &&
				_load_proc(p_context, "glGenRenderbuffers", p_suffix, gen_renderbuffers) &&
				_load_proc(p_context, "glBindRenderbuffer", p_suffix, bind_renderbuffer) &&
				_load_proc(p_context, "glDeleteRenderbuffers", p_suffix, delete_renderbuffers) &&
				_load_proc(p_context, "glRenderbufferStorage", p_suffix, renderbuffer_storage) &&
				_load_proc(p_context, "glFramebufferRenderbuffer", p_suffix, framebuffer_renderbuffer);
	}
};

#undef GL11_APIENTRY

void FixedFunctionGraphicsDeviceOpenGL1::_initialize_framebuffers(bool p_core, bool p_ext) {
	if (!p_core && !p_ext) {
		return;
	}
	_framebuffers = memnew(GL11FramebufferAPI);
	for (int path = 0; path < 2; path++) {
		const bool supported = path == 0 ? p_core : p_ext;
		if (!supported || !_framebuffers->load(_context, path == 0 ? "" : "EXT")) {
			continue;
		}
		GLint maximum = 0;
		glGetIntegerv(GL11FramebufferAPI::MAX_RENDERBUFFER_SIZE, &maximum);
		if (_check_gl_error() != OK || maximum < 2) {
			continue;
		}
		_max_renderbuffer_size = maximum;
		GL11Texture probe;
		probe.size = Size2i(2, 2);
		probe.upload_size = probe.size;
		Error error = _allocate_framebuffer(probe);
		if (error == OK) {
			_bind_framebuffer(&probe);
			error = _allocate_framebuffer_depth(probe);
			_bind_framebuffer(nullptr);
			_delete_framebuffer(probe);
			glDeleteTextures(TEXTURE_VIEW_COUNT, probe.names);
		}
		if (error == OK) {
			_capabilities.render_target_opaque = true;
			_capabilities.render_target_scaled = true;
			return;
		}
	}
	_clear_framebuffer_api();
}

void FixedFunctionGraphicsDeviceOpenGL1::_clear_framebuffer_api() {
	if (_framebuffers) {
		memdelete(_framebuffers);
		_framebuffers = nullptr;
	}
	_max_renderbuffer_size = 0;
}

void FixedFunctionGraphicsDeviceOpenGL1::_bind_framebuffer(const GL11Texture *p_texture) {
	if (_framebuffers) {
		_framebuffers->bind_framebuffer(GL11FramebufferAPI::FRAMEBUFFER, p_texture ? p_texture->framebuffer : 0);
	}
}

void FixedFunctionGraphicsDeviceOpenGL1::_delete_framebuffer(GL11Texture &p_texture) {
	_framebuffers->delete_framebuffers(1, &p_texture.framebuffer);
	_framebuffers->delete_renderbuffers(1, &p_texture.depth);
	p_texture.framebuffer = 0;
	p_texture.depth = 0;
}

Error FixedFunctionGraphicsDeviceOpenGL1::_allocate_framebuffer(GL11Texture &r_texture) {
	GLuint &color = r_texture.names[TEXTURE_RGBA];
	glGenTextures(1, &color);
	if (!color) {
		const Error error = _check_gl_error();
		return error == OK ? ERR_CANT_CREATE : error;
	}
	glBindTexture(GL_TEXTURE_2D, color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, r_texture.upload_size.x, r_texture.upload_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	Error error = _check_gl_error();
	if (error == OK) {
		_framebuffers->gen_framebuffers(1, &r_texture.framebuffer);
		error = _check_gl_error();
		if (error == OK && !r_texture.framebuffer) {
			error = ERR_CANT_CREATE;
		}
	}
	if (error == OK) {
		_bind_framebuffer(&r_texture);
		_framebuffers->framebuffer_texture_2d(GL11FramebufferAPI::FRAMEBUFFER, GL11FramebufferAPI::COLOR_ATTACHMENT, GL_TEXTURE_2D, color, 0);
		glDrawBuffer(GL11FramebufferAPI::COLOR_ATTACHMENT);
		glReadBuffer(GL11FramebufferAPI::COLOR_ATTACHMENT);
		const GLenum status = _framebuffers->check_framebuffer_status(GL11FramebufferAPI::FRAMEBUFFER);
		_bind_framebuffer(nullptr);
		error = _check_gl_error();
		if (error == OK && status != GL11FramebufferAPI::COMPLETE) {
			error = ERR_UNAVAILABLE;
		}
	}
	if (error != OK) {
		_delete_framebuffer(r_texture);
		glDeleteTextures(1, &color);
		color = 0;
	}
	return error;
}

Error FixedFunctionGraphicsDeviceOpenGL1::_allocate_framebuffer_depth(GL11Texture &r_texture) {
	if (r_texture.depth) {
		return OK;
	}
	if (uint32_t(r_texture.upload_size.x) > _max_renderbuffer_size || uint32_t(r_texture.upload_size.y) > _max_renderbuffer_size) {
		return ERR_UNAVAILABLE;
	}
	GLuint depth = 0;
	_framebuffers->gen_renderbuffers(1, &depth);
	if (!depth) {
		const Error error = _check_gl_error();
		return error == OK ? ERR_CANT_CREATE : error;
	}
	_framebuffers->bind_renderbuffer(GL11FramebufferAPI::RENDERBUFFER, depth);
	// Prefer 24-bit depth for perspective scenes. Older drivers can fall back to 16-bit.
	const GLenum formats[] = { GL11FramebufferAPI::DEPTH_COMPONENT24, GL11FramebufferAPI::DEPTH_COMPONENT16 };
	Error error = ERR_UNAVAILABLE;
	for (GLenum format : formats) {
		_framebuffers->renderbuffer_storage(GL11FramebufferAPI::RENDERBUFFER, format, r_texture.upload_size.x, r_texture.upload_size.y);
		error = _check_gl_error();
		if (error != OK) {
			continue;
		}
		_framebuffers->framebuffer_renderbuffer(GL11FramebufferAPI::FRAMEBUFFER, GL11FramebufferAPI::DEPTH_ATTACHMENT, GL11FramebufferAPI::RENDERBUFFER, depth);
		const GLenum status = _framebuffers->check_framebuffer_status(GL11FramebufferAPI::FRAMEBUFFER);
		error = _check_gl_error();
		if (error == OK && status == GL11FramebufferAPI::COMPLETE) {
			break;
		}
		if (error == OK) {
			error = ERR_UNAVAILABLE;
		}
	}
	_framebuffers->bind_renderbuffer(GL11FramebufferAPI::RENDERBUFFER, 0);
	if (error != OK) {
		// Deleting an attached renderbuffer detaches it from the bound framebuffer.
		_framebuffers->delete_renderbuffers(1, &depth);
		return error;
	}
	r_texture.depth = depth;
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::_copy_framebuffer_view(GL11Texture &p_texture, GL11TextureView p_view) {
	if (p_view == TEXTURE_RGBA) {
		return OK;
	}
	const uint8_t mask = uint8_t(1 << p_view);
	GLuint &name = p_texture.names[p_view];
	if (name && !(p_texture.dirty_views & mask)) {
		return OK;
	}
	// GL 1.1's texture environment chooses channels by the texture's internal format.
	// Copy a requested view lazily on the GPU, at most once per completed target pass.
	GLint previous = 0;
	glGetIntegerv(GL11FramebufferAPI::FRAMEBUFFER_BINDING, &previous);
	Error error = _check_gl_error();
	if (error != OK) {
		return error;
	}
	const bool allocate = name == 0;
	if (allocate) {
		glGenTextures(1, &name);
	}
	if (name) {
		_bind_framebuffer(&p_texture);
		glBindTexture(GL_TEXTURE_2D, name);
		if (allocate) {
			glCopyTexImage2D(GL_TEXTURE_2D, 0, p_view == TEXTURE_RGB ? GL_RGB8 : GL_ALPHA8, 0, 0, p_texture.upload_size.x, p_texture.upload_size.y, 0);
		} else {
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, p_texture.upload_size.x, p_texture.upload_size.y);
		}
	}
	error = _check_gl_error();
	if (error == OK && !name) {
		error = ERR_CANT_CREATE;
	}
	_framebuffers->bind_framebuffer(GL11FramebufferAPI::FRAMEBUFFER, static_cast<GLuint>(previous));
	const Error restored = _check_gl_error();
	if (error == OK) {
		error = restored;
	}
	if (error == OK) {
		p_texture.dirty_views &= ~mask;
	} else if (allocate && name) {
		glDeleteTextures(1, &name);
		name = 0;
	}
	return error;
}

Error FixedFunctionGraphicsDeviceOpenGL1::texture_read_render_target(RID p_texture, uint32_t *r_pixels, size_t p_pixel_count) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (!_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	const GL11Texture *texture = _texture_owner.getptr(p_texture);
	if (!texture->framebuffer) {
		return ERR_INVALID_PARAMETER;
	}
	if (!texture->rendered) {
		return ERR_UNCONFIGURED;
	}
	const uint64_t count = uint64_t(texture->size.x) * texture->size.y;
	if (!r_pixels || p_pixel_count < count || count > INT32_MAX / 4) {
		return ERR_INVALID_PARAMETER;
	}
	GLint previous = 0;
	glGetIntegerv(GL11FramebufferAPI::FRAMEBUFFER_BINDING, &previous);
	Error error = _check_gl_error();
	if (error != OK) {
		return error;
	}
	_bind_framebuffer(texture);
	error = _check_gl_error();
	if (error == OK) {
		if (texture->size == texture->upload_size) {
			error = _read_pixels(texture->size, r_pixels, p_pixel_count, false, texture->transparent);
		} else {
			Vector<uint32_t> native;
			const uint64_t pixels = uint64_t(texture->upload_size.x) * texture->upload_size.y;
			error = pixels > INT32_MAX / 4 ? ERR_OUT_OF_MEMORY : native.resize(int(pixels));
			if (error == OK) {
				error = _read_pixels(texture->upload_size, native.ptrw(), native.size(), false, texture->transparent);
			}
			if (error == OK) {
				// Snapshots retain the requested logical dimensions, sampling native pixel centers.
				error = copy_texture_readback(reinterpret_cast<const uint8_t *>(native.ptr()), texture->upload_size, size_t(texture->upload_size.x) * 4, texture->size, r_pixels, p_pixel_count);
			}
		}
	}
	_framebuffers->bind_framebuffer(GL11FramebufferAPI::FRAMEBUFFER, static_cast<GLuint>(previous));
	const Error restored = _check_gl_error();
	return error == OK ? restored : error;
}
