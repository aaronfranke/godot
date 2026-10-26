#include "drivers/fixed_function/geometry_data.h"
#include "drivers/opengl1/graphics_device_opengl1.h"
#include "drivers/opengl1/opengl1_headers.h"

#include <stddef.h>
#include <string.h>

#ifdef WINDOWS_ENABLED
#define GL11_APIENTRY APIENTRY
#else
#define GL11_APIENTRY
#endif

struct FixedFunctionGraphicsDeviceOpenGL1::GL11GeometryBufferAPI {
	enum {
		ARRAY_BUFFER = 0x8892,
		ELEMENT_ARRAY_BUFFER = 0x8893,
		STATIC_DRAW = 0x88E4,
	};
	typedef void(GL11_APIENTRY *GenBuffers)(GLsizei, GLuint *);
	typedef void(GL11_APIENTRY *DeleteBuffers)(GLsizei, const GLuint *);
	typedef void(GL11_APIENTRY *BindBuffer)(GLenum, GLuint);
	typedef void(GL11_APIENTRY *BufferData)(GLenum, ptrdiff_t, const void *, GLenum);

	GenBuffers gen_buffers;
	DeleteBuffers delete_buffers;
	BindBuffer bind_buffer;
	BufferData buffer_data;

	bool load(FixedFunctionOpenGL1Context &p_context, bool p_arb) {
		gen_buffers = reinterpret_cast<GenBuffers>(p_context.get_proc_address(p_arb ? "glGenBuffersARB" : "glGenBuffers"));
		delete_buffers = reinterpret_cast<DeleteBuffers>(p_context.get_proc_address(p_arb ? "glDeleteBuffersARB" : "glDeleteBuffers"));
		bind_buffer = reinterpret_cast<BindBuffer>(p_context.get_proc_address(p_arb ? "glBindBufferARB" : "glBindBuffer"));
		buffer_data = reinterpret_cast<BufferData>(p_context.get_proc_address(p_arb ? "glBufferDataARB" : "glBufferData"));
		return gen_buffers && delete_buffers && bind_buffer && buffer_data;
	}

	void unbind() const {
		bind_buffer(ARRAY_BUFFER, 0);
		bind_buffer(ELEMENT_ARRAY_BUFFER, 0);
	}
};

#undef GL11_APIENTRY

void FixedFunctionGraphicsDeviceOpenGL1::_initialize_geometry_buffers(bool p_core, bool p_arb) {
	GL11GeometryBufferAPI api;
	if ((p_core && api.load(_context, false)) || (p_arb && api.load(_context, true))) {
		_geometry_buffers = memnew(GL11GeometryBufferAPI(api));
	}
}

void FixedFunctionGraphicsDeviceOpenGL1::_clear_geometry_buffer_api() {
	if (_geometry_buffers) {
		memdelete(_geometry_buffers);
		_geometry_buffers = nullptr;
	}
}

bool FixedFunctionGraphicsDeviceOpenGL1::_upload_geometry_batch(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint16_t> &p_indices, GL11GeometryBatch &r_batch) {
	Vector<FixedFunctionVertex> upload;
	if (upload.resize(p_vertices.size()) != OK) {
		return false;
	}
	const ptrdiff_t vertex_bytes = ptrdiff_t(p_vertices.size()) * ptrdiff_t(sizeof(FixedFunctionVertex));
	FixedFunctionVertex *vertices = upload.ptrw();
	memcpy(vertices, p_vertices.ptr(), vertex_bytes);
	// The shared vertex format stores ARGB integers. OpenGL consumes RGBA bytes.
	for (int64_t i = 0; i < p_vertices.size(); i++) {
		_store_color(vertices[i].color, reinterpret_cast<uint8_t *>(&vertices[i].color));
	}

	return _upload_geometry_buffers(vertices, vertex_bytes, p_indices, r_batch);
}

bool FixedFunctionGraphicsDeviceOpenGL1::_upload_geometry_buffers(const void *p_vertices, ptrdiff_t p_vertex_bytes, const Vector<uint16_t> &p_indices, GL11GeometryBatch &r_batch) {
	const ptrdiff_t index_bytes = ptrdiff_t(p_indices.size()) * ptrdiff_t(sizeof(uint16_t));
	const GL11GeometryBufferAPI &api = *_geometry_buffers;
	GLuint buffers[2] = {};
	api.gen_buffers(2, buffers);
	if (buffers[0] && buffers[1]) {
		api.bind_buffer(api.ARRAY_BUFFER, buffers[0]);
		api.buffer_data(api.ARRAY_BUFFER, p_vertex_bytes, p_vertices, api.STATIC_DRAW);
		api.bind_buffer(api.ELEMENT_ARRAY_BUFFER, buffers[1]);
		api.buffer_data(api.ELEMENT_ARRAY_BUFFER, index_bytes, p_indices.ptr(), api.STATIC_DRAW);
		api.unbind();
	}
	if (_check_gl_error() != OK || !buffers[0] || !buffers[1]) {
		api.delete_buffers(2, buffers);
		_check_gl_error();
		return false;
	}
	r_batch.buffers[0] = buffers[0];
	r_batch.buffers[1] = buffers[1];
	return true;
}

void FixedFunctionGraphicsDeviceOpenGL1::_release_geometry_buffers(const GL11Geometry &p_geometry) {
	if (!_geometry_buffers) {
		return;
	}
	for (const GL11GeometryBatch &batch : p_geometry.batches) {
		if (batch.buffers[0]) {
			_geometry_buffers->delete_buffers(2, batch.buffers);
		}
	}
}

Error FixedFunctionGraphicsDeviceOpenGL1::_draw_buffered_geometry_batch(const GL11GeometryBatch &p_batch, const FixedFunctionGeometryData::FFIndexRange &p_range, bool p_vertex_colors) {
	const GL11GeometryBufferAPI &api = *_geometry_buffers;
	api.bind_buffer(api.ARRAY_BUFFER, p_batch.buffers[0]);
	api.bind_buffer(api.ELEMENT_ARRAY_BUFFER, p_batch.buffers[1]);
	glVertexPointer(3, GL_FLOAT, sizeof(FixedFunctionVertex), reinterpret_cast<const void *>(offsetof(FixedFunctionVertex, x)));
	if (p_vertex_colors) {
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FixedFunctionVertex), reinterpret_cast<const void *>(offsetof(FixedFunctionVertex, color)));
	}
	glTexCoordPointer(2, GL_FLOAT, sizeof(FixedFunctionVertex), reinterpret_cast<const void *>(offsetof(FixedFunctionVertex, u)));
	const Error error = _check_gl_error();
	if (error == OK) {
		glDrawElements(GL_TRIANGLES, p_range.count, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(size_t(p_range.first) * sizeof(uint16_t)));
	}
	// The next batch or canvas draw may use client pointers.
	api.unbind();
	const Error draw_error = _check_gl_error();
	return error == OK ? draw_error : error;
}

Error FixedFunctionGraphicsDeviceOpenGL1::geometry_create(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) {
	if (!_initialized || Thread::get_caller_id() != _owner_thread) {
		return ERR_UNCONFIGURED;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	const Error error = FixedFunctionGeometryData::partition(p_vertices, p_indices, FixedFunctionGeometryData::MAX_BATCH_VERTICES, _capabilities.max_primitives_per_draw, batches);
	if (error != OK) {
		return error;
	}
	return _create_geometry(batches, r_geometry);
}

Error FixedFunctionGraphicsDeviceOpenGL1::geometry_create_lod_batch(const FixedFunctionGeometryData::FFGeometryBatch &p_batch, RID &r_geometry) {
	if (!_initialized || Thread::get_caller_id() != _owner_thread) {
		return ERR_UNCONFIGURED;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (p_batch.vertices.is_empty() && (p_batch.lighting_vertices.is_empty() || !_vertex_light_limit)) {
		return ERR_UNAVAILABLE;
	}
	const Error error = FixedFunctionGeometryData::validate_lod_batch(p_batch, FixedFunctionGeometryData::MAX_BATCH_VERTICES, _capabilities.max_primitives_per_draw);
	if (error != OK) {
		return error;
	}
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	if (batches.push_back(p_batch)) {
		return ERR_OUT_OF_MEMORY;
	}
	return _create_geometry(batches, r_geometry);
}

Error FixedFunctionGraphicsDeviceOpenGL1::_create_geometry(const Vector<FixedFunctionGeometryData::FFGeometryBatch> &p_batches, RID &r_geometry) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	GL11Geometry *geometry = memnew(GL11Geometry);
	geometry->lit = !p_batches[0].lighting_vertices.is_empty();
	if (geometry->batches.resize(p_batches.size()) != OK) {
		memdelete(geometry);
		return ERR_OUT_OF_MEMORY;
	}
	for (int i = 0; i < p_batches.size(); i++) {
		GL11GeometryBatch &batch = geometry->batches.write[i];
		const FixedFunctionGeometryData::FFGeometryBatch &source = p_batches[i];
		batch.index_count = uint32_t(source.indices.size());
		batch.lod_offsets = source.lod_offsets;
		if (geometry->lit) {
			const ptrdiff_t vertex_bytes = ptrdiff_t(source.lighting_vertices.size()) * ptrdiff_t(sizeof(FixedFunctionLightingVertex));
			if (!_geometry_buffers || !_upload_geometry_buffers(source.lighting_vertices.ptr(), vertex_bytes, source.indices, batch)) {
				batch.lit_vertices = source.lighting_vertices;
				batch.indices = source.indices;
			}
		} else if (!_geometry_buffers || !_upload_geometry_batch(source.vertices, source.indices, batch)) {
			batch.vertices = source.vertices;
			batch.indices = source.indices;
			if (batch.colors.resize(batch.vertices.size() * 4) != OK) {
				_release_geometry_buffers(*geometry);
				memdelete(geometry);
				return ERR_OUT_OF_MEMORY;
			}
			_store_colors(batch.vertices.ptr(), batch.vertices.size(), batch.colors.ptrw());
		}
		const uint32_t base_count = source.get_index_range(0).count;
		geometry->info.vertex_count += base_count;
		geometry->info.primitive_count += base_count / 3;
	}
	geometry->info.draw_calls = p_batches.size();
	geometry->info.lod_count = p_batches[0].lod_offsets.size();
	r_geometry = _geometry_owner.make_rid(geometry);
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::geometry_free(RID p_geometry) {
	if (!_initialized || Thread::get_caller_id() != _owner_thread) {
		return ERR_UNCONFIGURED;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (!_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	GL11Geometry *geometry = _geometry_owner.get_or_null(p_geometry);
	if (_geometry_buffers) {
		const GL11CurrentContext current(*this);
		if (current.error != OK) {
			return current.error;
		}
		_release_geometry_buffers(*geometry);
	}
	_geometry_owner.free(p_geometry);
	memdelete(geometry);
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::geometry_get_draw_info(RID p_geometry, FFGeometryDrawInfo &r_info) const {
	if (!_initialized || Thread::get_caller_id() != _owner_thread || !_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	r_info = _geometry_owner.get_or_null(p_geometry)->info;
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::draw_geometry(RID p_geometry, const Projection &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state, const Color *p_color_override, uint32_t p_lod) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (!_frame_active) {
		return ERR_UNCONFIGURED;
	}
	if (!_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	const GL11Geometry *geometry = _geometry_owner.get_or_null(p_geometry);
	if (geometry->lit) {
		return ERR_UNAVAILABLE;
	}
	if (p_lod > geometry->info.lod_count) {
		return ERR_UNAVAILABLE;
	}
	uint32_t color = 0;
	if (p_color_override) {
		const Error error = pack_geometry_color(*p_color_override, color);
		if (error != OK) {
			return error;
		}
	}
	Error error = _prepare_draw(p_clip_from_model, p_texture, p_state);
	if (error != OK) {
		return error == ERR_SKIP ? OK : error;
	}
	if (p_color_override) {
		glDisableClientState(GL_COLOR_ARRAY);
		glColor4ub(uint8_t(color >> 16), uint8_t(color >> 8), uint8_t(color), uint8_t(color >> 24));
	}
	for (int i = 0; error == OK && i < geometry->batches.size(); i++) {
		const GL11GeometryBatch &batch = geometry->batches[i];
		const FixedFunctionGeometryData::FFIndexRange range = FixedFunctionGeometryData::get_index_range(batch.lod_offsets, batch.index_count, p_lod);
		if (batch.buffers[0]) {
			error = _draw_buffered_geometry_batch(batch, range, !p_color_override);
		} else {
			error = _draw_arrays(batch.vertices.ptr(), batch.vertices.size(), p_color_override ? nullptr : batch.colors.ptr(), batch.indices.ptr() + range.first, range.count);
		}
	}
	if (p_color_override) {
		glEnableClientState(GL_COLOR_ARRAY);
	}
	return error;
}

// Native transform and lighting for retained geometry, using core OpenGL 1.1 state.

Error FixedFunctionGraphicsDeviceOpenGL1::geometry_create_lit(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) {
	if (!_initialized || Thread::get_caller_id() != _owner_thread) {
		return ERR_UNCONFIGURED;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (!_vertex_light_limit) {
		return ERR_UNAVAILABLE;
	}
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	const Error error = FixedFunctionGeometryData::partition(p_vertices, p_indices, FixedFunctionGeometryData::MAX_BATCH_VERTICES, _capabilities.max_primitives_per_draw, batches);
	if (error != OK) {
		return error;
	}
	return _create_geometry(batches, r_geometry);
}

Error FixedFunctionGraphicsDeviceOpenGL1::draw_vertex_lit_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state, uint32_t p_lod) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (!_frame_active) {
		return ERR_UNCONFIGURED;
	}
	if (!_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	const GL11Geometry *geometry = _geometry_owner.get_or_null(p_geometry);
	if (!geometry->lit) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_lod > geometry->info.lod_count) {
		return ERR_UNAVAILABLE;
	}
	return _draw_vertex_lighting(geometry, nullptr, 0, nullptr, 0, p_clip_from_model, p_lighting, p_color, p_uv_scale, p_uv_offset, p_texture, p_state, p_lod);
}

Error FixedFunctionGraphicsDeviceOpenGL1::draw_vertex_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, const Color &p_color, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices, uint32_t p_index_count) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (!_frame_active) {
		return ERR_UNCONFIGURED;
	}
	const uint32_t element_count = p_indices ? p_index_count : p_vertex_count;
	if (!p_vertices || !p_vertex_count || (p_indices == nullptr) != (p_index_count == 0) ||
			element_count % 3 || element_count / 3 > _capabilities.max_primitives_per_draw) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_indices) {
		const Error index_error = validate_triangle_indices(p_vertex_count, reinterpret_cast<const uint8_t *>(p_indices), p_index_count);
		if (index_error != OK) {
			return index_error;
		}
	}
	return _draw_vertex_lighting(nullptr, p_vertices, p_vertex_count, p_indices, p_index_count, p_clip_from_view, p_lighting, p_color, Vector2(1, 1), Vector2(), p_texture, p_state, 0);
}

Error FixedFunctionGraphicsDeviceOpenGL1::_draw_vertex_lighting(const GL11Geometry *p_geometry, const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const uint16_t *p_indices, uint32_t p_index_count, const Projection &p_projection, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state, uint32_t p_lod) {
	if (!_vertex_light_limit || p_lighting.light_count > _vertex_light_limit) {
		return ERR_UNAVAILABLE;
	}
	const float inputs[] = { p_color.r, p_color.g, p_color.b, p_color.a, p_lighting.ambient.r, p_lighting.ambient.g, p_lighting.ambient.b,
		float(p_uv_scale.x), float(p_uv_scale.y), float(p_uv_offset.x), float(p_uv_offset.y) };
	for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
		if (!(Math::abs(inputs[i]) <= 1.0e10f)) {
			return ERR_UNAVAILABLE;
		}
	}
	for (uint32_t i = 0; i < p_lighting.light_count; i++) {
		const FFPixelLight &light = p_lighting.lights[i];
		const float *registers[] = { light.color_type, light.position_range, light.direction_attenuation, light.cone };
		for (int reg = 0; reg < 4; reg++) {
			for (int component = 0; component < 4; component++) {
				if (!(Math::abs(registers[reg][component]) <= 1.0e10f)) {
					return ERR_UNAVAILABLE;
				}
			}
		}
		if (light.color_type[3] != 0 && !(light.position_range[3] > 1.0e-10f)) {
			return ERR_UNAVAILABLE;
		}
	}
	Error error = _prepare_draw(p_projection, p_texture, p_state);
	if (error != OK) {
		return error == ERR_SKIP ? OK : error;
	}
	const float black[4] = { 0, 0, 0, 1 };
	const float diffuse[4] = { p_color.r, p_color.g, p_color.b, p_color.a };
	const float ambient[4] = { p_color.r * p_lighting.ambient.r, p_color.g * p_lighting.ambient.g, p_color.b * p_lighting.ambient.b, 1 };
	const bool occlusion = p_lighting.vertex_ambient_occlusion;
	// With baked AO the vertex color scales ambient only. Diffuse and alpha
	// still come from the material, and the albedo texture modulates both terms.
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, occlusion ? ambient : black);
	if (occlusion) {
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT);
		glEnable(GL_COLOR_MATERIAL);
	} else {
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
	}
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, occlusion ? black : ambient);
	// Light positions and directions pass through the current model-view matrix. It is
	// identity unless fog needs eye-space depth, which also scales distances and normals.
	const float eye_scale = _model_view_scale > 0 ? _model_view_scale : 1.0f;
	_set_enabled(GL_NORMALIZE, _model_view_loaded);
	for (uint32_t i = 0; i < p_lighting.light_count; i++) {
		const FFPixelLight &source = p_lighting.lights[i];
		const GLenum light = GL_LIGHT0 + i;
		const float color[4] = { source.color_type[0], source.color_type[1], source.color_type[2], 1 };
		const float *direction = source.direction_attenuation;
		glLightfv(light, GL_AMBIENT, black);
		glLightfv(light, GL_SPECULAR, black);
		glLightfv(light, GL_DIFFUSE, color);
		float quadratic = 0;
		float cutoff = 180;
		float exponent = 0;
		if (source.color_type[3] == 0) {
			const float position[4] = { direction[0], direction[1], direction[2], 0 };
			glLightfv(light, GL_POSITION, position);
		} else {
			const float position[4] = { source.position_range[0], source.position_range[1], source.position_range[2], 1 };
			glLightfv(light, GL_POSITION, position);
			// OpenGL has no range cutoff; the fitted falloff continues beyond the Godot range.
			const float reciprocal_range = source.position_range[3] / eye_scale;
			quadratic = get_vertex_light_quadratic_attenuation(direction[3]) * reciprocal_range * reciprocal_range;
			if (source.color_type[3] == 2) {
				const float cone = CLAMP(source.cone[0], 0.0f, 1.0f);
				const float spot_direction[3] = { direction[0], direction[1], direction[2] };
				glLightfv(light, GL_SPOT_DIRECTION, spot_direction);
				cutoff = MIN(Math::rad_to_deg(Math::acos(cone)), 90.0f);
				// Match Godot's falloff at the middle of the cone; the edge remains a step.
				const float middle = 1.0f - 0.5f * (1.0f - cone);
				const float target = 1.0f - Math::pow(0.5f, MAX(source.cone[1], 0.001f));
				if (middle > 0 && middle < 1 && target > 0) {
					exponent = CLAMP(Math::log(target) / Math::log(middle), 0.0f, 128.0f);
				}
			}
		}
		glLightf(light, GL_CONSTANT_ATTENUATION, 1);
		glLightf(light, GL_LINEAR_ATTENUATION, 0);
		glLightf(light, GL_QUADRATIC_ATTENUATION, quadratic);
		glLightf(light, GL_SPOT_CUTOFF, cutoff);
		glLightf(light, GL_SPOT_EXPONENT, exponent);
		glEnable(light);
	}
	for (uint32_t i = p_lighting.light_count; i < _enabled_vertex_lights; i++) {
		glDisable(GL_LIGHT0 + i);
	}
	_enabled_vertex_lights = p_lighting.light_count;
	const bool transform_uv = p_uv_scale != Vector2(1, 1) || p_uv_offset != Vector2();
	if (transform_uv) {
		const float texture[16] = { float(p_uv_scale.x), 0, 0, 0, 0, float(p_uv_scale.y), 0, 0, 0, 0, 1, 0, float(p_uv_offset.x), float(p_uv_offset.y), 0, 1 };
		glMatrixMode(GL_TEXTURE);
		glLoadMatrixf(texture);
		glMatrixMode(GL_PROJECTION);
	}
	glEnable(GL_LIGHTING);
	if (!occlusion) {
		glDisableClientState(GL_COLOR_ARRAY);
	}
	glEnableClientState(GL_NORMAL_ARRAY);
	error = _check_gl_error();
	const size_t stride = sizeof(FixedFunctionLightingVertex);
	const auto draw_client_arrays = [&](const FixedFunctionLightingVertex *p_data, const uint16_t *p_elements, uint32_t p_count) {
		glVertexPointer(3, GL_FLOAT, stride, &p_data->x);
		glNormalPointer(GL_FLOAT, stride, &p_data->nx);
		glTexCoordPointer(2, GL_FLOAT, stride, &p_data->u);
		if (occlusion) {
			glColorPointer(4, GL_FLOAT, stride, &p_data->r);
		}
		if (p_elements) {
			glDrawElements(GL_TRIANGLES, p_count, GL_UNSIGNED_SHORT, p_elements);
		} else {
			glDrawArrays(GL_TRIANGLES, 0, p_count);
		}
		return _check_gl_error();
	};
	if (error == OK && !p_geometry) {
		error = draw_client_arrays(p_vertices, p_indices, p_indices ? p_index_count : p_vertex_count);
	}
	for (int i = 0; p_geometry && error == OK && i < p_geometry->batches.size(); i++) {
		const GL11GeometryBatch &batch = p_geometry->batches[i];
		const FixedFunctionGeometryData::FFIndexRange range = FixedFunctionGeometryData::get_index_range(batch.lod_offsets, batch.index_count, p_lod);
		if (batch.buffers[0]) {
			const GL11GeometryBufferAPI &api = *_geometry_buffers;
			api.bind_buffer(api.ARRAY_BUFFER, batch.buffers[0]);
			api.bind_buffer(api.ELEMENT_ARRAY_BUFFER, batch.buffers[1]);
			glVertexPointer(3, GL_FLOAT, stride, reinterpret_cast<const void *>(offsetof(FixedFunctionLightingVertex, x)));
			glNormalPointer(GL_FLOAT, stride, reinterpret_cast<const void *>(offsetof(FixedFunctionLightingVertex, nx)));
			glTexCoordPointer(2, GL_FLOAT, stride, reinterpret_cast<const void *>(offsetof(FixedFunctionLightingVertex, u)));
			if (occlusion) {
				glColorPointer(4, GL_FLOAT, stride, reinterpret_cast<const void *>(offsetof(FixedFunctionLightingVertex, r)));
			}
			glDrawElements(GL_TRIANGLES, range.count, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>(size_t(range.first) * sizeof(uint16_t)));
			api.unbind();
		} else {
			error = draw_client_arrays(batch.lit_vertices.ptr(), batch.indices.ptr() + range.first, range.count);
			continue;
		}
		error = _check_gl_error();
	}
	// Later draws assume unlit vertex colors and untransformed UVs.
	glDisableClientState(GL_NORMAL_ARRAY);
	if (occlusion) {
		glDisable(GL_COLOR_MATERIAL);
	} else {
		glEnableClientState(GL_COLOR_ARRAY);
	}
	glDisable(GL_LIGHTING);
	glDisable(GL_NORMALIZE);
	if (transform_uv) {
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
	}
	const Error restore_error = _check_gl_error();
	return error == OK ? restore_error : error;
}
