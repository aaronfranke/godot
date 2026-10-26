#include "drivers/opengl1/graphics_device_opengl1.h"
#include "drivers/opengl1/opengl1_headers.h"

#ifdef WINDOWS_ENABLED
#define GL11_APIENTRY APIENTRY
#else
#define GL11_APIENTRY
#endif

struct FixedFunctionGraphicsDeviceOpenGL1::GL11BlendAPI {
	typedef void(GL11_APIENTRY *BlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum);
	BlendFuncSeparate blend_func_separate;

	bool load(FixedFunctionOpenGL1Context &p_context, const char *p_name) {
		blend_func_separate = reinterpret_cast<BlendFuncSeparate>(p_context.get_proc_address(p_name));
		return blend_func_separate != nullptr;
	}
};

#undef GL11_APIENTRY

void FixedFunctionGraphicsDeviceOpenGL1::_initialize_blending(bool p_core, bool p_ext) {
	if (!p_core && !p_ext) {
		return;
	}
	_blending = memnew(GL11BlendAPI);
	for (int path = 0; path < 2; path++) {
		const bool supported = path == 0 ? p_core : p_ext;
		if (!supported || !_blending->load(_context, path == 0 ? "glBlendFuncSeparate" : "glBlendFuncSeparateEXT")) {
			continue;
		}
		_blending->blend_func_separate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		const Error error = _check_gl_error();
		glBlendFunc(GL_ONE, GL_ZERO);
		const Error restored = _check_gl_error();
		if (error == OK && restored == OK) {
			_capabilities.render_target_transparent = true;
			return;
		}
	}
	// Missing or unusable optional procedures must not disable opaque FBO rendering.
	_clear_blend_api();
}

void FixedFunctionGraphicsDeviceOpenGL1::_clear_blend_api() {
	if (_blending) {
		memdelete(_blending);
		_blending = nullptr;
	}
}

void FixedFunctionGraphicsDeviceOpenGL1::_apply_blend_state(const FFDrawParameters &p_state) {
	const bool transparent = _render_target && _render_target->transparent;
	if (transparent) {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, p_state.alpha_write_enabled ? GL_TRUE : GL_FALSE);
	}
	if (p_state.blend_mode == BLEND_MODE_DISABLED) {
		return;
	}
	GLenum source = GL_SRC_ALPHA;
	GLenum destination = GL_ONE_MINUS_SRC_ALPHA;
	GLenum source_alpha = GL_ONE;
	GLenum destination_alpha = GL_ONE_MINUS_SRC_ALPHA;
	switch (p_state.blend_mode) {
		case BLEND_MODE_PREMULTIPLIED_ALPHA:
			source = GL_ONE;
			break;
		case BLEND_MODE_ADD:
			destination = GL_ONE;
			source_alpha = GL_SRC_ALPHA;
			destination_alpha = GL_ONE;
			break;
		case BLEND_MODE_ADD_UNWEIGHTED:
			source = destination = GL_ONE;
			source_alpha = destination_alpha = GL_ONE;
			break;
		case BLEND_MODE_MUL:
			source = GL_DST_COLOR;
			destination = GL_ZERO;
			source_alpha = GL_DST_ALPHA;
			destination_alpha = GL_ZERO;
			break;
		default:
			break;
	}
	if (transparent) {
		_blending->blend_func_separate(source, destination, source_alpha, destination_alpha);
	} else {
		glBlendFunc(source, destination);
	}
}
