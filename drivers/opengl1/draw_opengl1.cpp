#include "graphics_device_opengl1.h"

#include "opengl1_headers.h"

void FixedFunctionGraphicsDeviceOpenGL1::_set_enabled(unsigned int p_capability, bool p_enabled) {
	if (p_enabled) {
		glEnable(p_capability);
	} else {
		glDisable(p_capability);
	}
}

void FixedFunctionGraphicsDeviceOpenGL1::_apply_draw_state(const FFDrawParameters &p_state, const Rect2i &p_clip) {
	_set_enabled(GL_SCISSOR_TEST, p_state.clip_enabled);
	if (p_state.clip_enabled) {
		// The clip rect is top-left based. The viewport and projection stay fixed.
		if (_render_target) {
			const Rect2i native = get_texture_native_clip(p_clip, _render_target->size, _render_target->upload_size);
			glScissor(native.position.x, native.position.y, native.size.x, native.size.y);
		} else {
			const int bottom = p_clip.position.y + p_clip.size.y;
			glScissor(p_clip.position.x, int(get_target_height()) - bottom, p_clip.size.x, p_clip.size.y);
		}
	}
	_set_enabled(GL_DEPTH_TEST, p_state.depth_test_enabled || p_state.depth_write_enabled);
	glDepthFunc(p_state.depth_test_enabled ? GL_LEQUAL : GL_ALWAYS);
	glDepthMask(p_state.depth_write_enabled ? GL_TRUE : GL_FALSE);

	_set_enabled(GL_CULL_FACE, p_state.cull_mode != CULL_DISABLED);
	glCullFace(p_state.cull_mode == CULL_FRONT ? GL_FRONT : GL_BACK);

	_set_enabled(GL_ALPHA_TEST, p_state.alpha_cutoff_enabled);
	if (p_state.alpha_cutoff_enabled) {
		glAlphaFunc(p_state.alpha_cutoff > 1 ? GL_NEVER : GL_GEQUAL, float(get_alpha_reference(p_state.alpha_cutoff)) / 255);
	}

	_set_enabled(GL_BLEND, p_state.blend_mode != BLEND_MODE_DISABLED);
	_apply_blend_state(p_state);

	const bool fog_enabled = p_state.fog != nullptr;
	if (fog_enabled != _fog_enabled) {
		_set_enabled(GL_FOG, fog_enabled);
		_fog_enabled = fog_enabled;
	}
	if (fog_enabled) {
		const FFFog &fog = *p_state.fog;
		const Color color = fog.get_blend_color(p_state.blend_mode);
		if (fog.begin != _fog_begin || fog.end != _fog_end || color != _fog_color) {
			glFogi(GL_FOG_MODE, GL_LINEAR);
			glFogf(GL_FOG_START, fog.begin);
			glFogf(GL_FOG_END, fog.end);
			glFogfv(GL_FOG_COLOR, color.components);
			_fog_begin = fog.begin;
			_fog_end = fog.end;
			_fog_color = color;
		}
	}
}

Error FixedFunctionGraphicsDeviceOpenGL1::draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state) {
	return _draw_triangles(p_vertices, p_vertex_count, nullptr, p_vertex_count, p_clip_from_model, p_texture, p_state);
}

Error FixedFunctionGraphicsDeviceOpenGL1::draw_indexed_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count, const CameraMatrix &p_projection, RID p_texture, const FFDrawParameters &p_state) {
	const Error error = validate_triangle_indices(p_vertex_count, p_indices, p_index_count);
	if (error != OK) {
		return error;
	}
	return _draw_triangles(p_vertices, p_vertex_count, p_indices, p_index_count, p_projection, p_texture, p_state);
}

Error FixedFunctionGraphicsDeviceOpenGL1::_draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count, const CameraMatrix &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (!_frame_active) {
		return ERR_UNCONFIGURED;
	}
	if ((p_vertex_count && !p_vertices) || p_index_count % 3 || p_vertex_count > INT32_MAX / 4) {
		return ERR_INVALID_PARAMETER;
	}
	Error error = _prepare_draw(p_clip_from_model, p_texture, p_state);
	if (error == ERR_SKIP || (error == OK && !p_index_count)) {
		return OK;
	}
	if (error != OK) {
		return error;
	}
	const int color_bytes = int(p_vertex_count) * 4;
	if (_draw_colors.size() < color_bytes) {
		error = _draw_colors.resize(color_bytes);
		if (error != OK) {
			return error;
		}
	}
	_store_colors(p_vertices, p_vertex_count, _draw_colors.ptrw());
	return _draw_arrays(p_vertices, p_vertex_count, _draw_colors.ptr(), p_indices, p_index_count);
}

Error FixedFunctionGraphicsDeviceOpenGL1::_prepare_draw(const CameraMatrix &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state) {
	Error error = validate_draw_state(p_state);
	if (error != OK) {
		return error;
	}
	if ((p_state.depth_test_enabled || p_state.depth_write_enabled) && !_depth_active) {
		return ERR_UNCONFIGURED;
	}
	GL11Texture *texture = nullptr;
	if (p_texture.is_valid()) {
		if (!_texture_owner.owns(p_texture)) {
			return ERR_INVALID_PARAMETER;
		}
		texture = _texture_owner.getptr(p_texture);
		if (texture == _render_target) {
			return ERR_INVALID_PARAMETER;
		}
		if (texture->framebuffer && !texture->rendered) {
			return ERR_UNCONFIGURED;
		}
	}
	const Rect2i clip = get_draw_viewport(p_state);
	if (!clip.size.x || !clip.size.y) {
		return ERR_SKIP;
	}

	// GL shares Godot's clip depth and column order. The first uploaded texture
	// row is sampled at v=0, so the shared top-left UVs also pass through unchanged.
	// Fixed-function fog uses the eye-space depth after the model-view matrix. Model-space
	// vertices therefore need their view transform there, rather than in the projection.
	const FFFogTransforms *fog_transforms = p_state.fog ? p_state.fog_transforms : nullptr;
	float projection[16];
	store_camera(fog_transforms ? fog_transforms->clip_from_view : p_clip_from_model, projection);
	for (int i = 0; i < 16; i++) {
		if (Math::is_nan(projection[i]) || Math::is_inf(projection[i])) {
			return ERR_UNAVAILABLE;
		}
	}
	float model_view[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	if (fog_transforms) {
		const Transform &transform = fog_transforms->view_from_model;
		for (int column = 0; column < 4; column++) {
			const Vector3 axis = column < 3 ? transform.basis.get_axis(column) : transform.origin;
			for (int row = 0; row < 3; row++) {
				if (!(Math::abs(axis[row]) <= real_t(1.0e30))) {
					return ERR_UNAVAILABLE;
				}
				model_view[column * 4 + row] = float(axis[row]);
			}
		}
	}
	error = _bind_texture(texture, p_state);
	if (error != OK) {
		return error;
	}
	_apply_draw_state(p_state, clip);
	if (_render_target) {
		for (int column = 0; column < 4; column++) {
			projection[column * 4 + 1] = -projection[column * 4 + 1];
		}
	}
	if (fog_transforms || _model_view_loaded) {
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(model_view);
		glMatrixMode(GL_PROJECTION);
		_model_view_loaded = fog_transforms != nullptr;
		_model_view_scale = fog_transforms ? float(fog_transforms->view_from_model.basis.get_axis(0).length()) : 1.0f;
	}
	glLoadMatrixf(projection);
	return _check_gl_error();
}

void FixedFunctionGraphicsDeviceOpenGL1::_store_colors(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, uint8_t *r_colors) {
	for (uint32_t i = 0; i < p_vertex_count; i++) {
		_store_color(p_vertices[i].color, r_colors);
		r_colors += 4;
	}
}

Error FixedFunctionGraphicsDeviceOpenGL1::_draw_arrays(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_colors, const void *p_indices, uint32_t p_index_count) {
	glVertexPointer(3, GL_FLOAT, sizeof(FixedFunctionVertex), &p_vertices->x);
	if (p_colors) {
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, p_colors);
	}
	glTexCoordPointer(2, GL_FLOAT, sizeof(FixedFunctionVertex), &p_vertices->u);
	const Error error = _check_gl_error();
	if (error != OK) {
		return error;
	}
	if (p_indices) {
		glDrawElements(GL_TRIANGLES, p_index_count, GL_UNSIGNED_SHORT, p_indices);
	} else {
		glDrawArrays(GL_TRIANGLES, 0, p_vertex_count);
	}
	return _check_gl_error();
}
