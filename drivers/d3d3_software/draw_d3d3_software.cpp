#include "graphics_device_d3d3_software.h"

#include <string.h>

template <class T>
static void _write_d3d3_software_instruction(uint8_t *&r_output, D3DOPCODE p_opcode, const T *p_data, WORD p_count) {
	const D3DINSTRUCTION header = { BYTE(p_opcode), BYTE(sizeof(T)), p_count };
	memcpy(r_output, &header, sizeof(header));
	r_output += sizeof(header);
	const size_t bytes = size_t(p_count) * sizeof(T);
	memcpy(r_output, p_data, bytes);
	r_output += bytes;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_draw_batch(const FixedFunctionVertex *p_vertices, uint32_t p_count, D3DTEXTUREHANDLE p_texture, const FFDrawParameters &p_state, const Vector2 &p_uv_scale) {
	D3DEXECUTEBUFFERDESC buffer = {};
	buffer.dwSize = sizeof(buffer);
	Error error = _check_result(_pass->execute->Lock(&buffer));
	if (error != OK) {
		return error;
	}
	uint8_t *start = static_cast<uint8_t *>(buffer.lpData);
	D3DLVERTEX *vertices = reinterpret_cast<D3DLVERTEX *>(start);
	for (uint32_t i = 0; i < p_count; i++) {
		const FixedFunctionVertex &vertex = p_vertices[i];
		D3DLVERTEX &out = vertices[i];
		out.x = vertex.x;
		out.y = vertex.y;
		out.z = vertex.z;
		out.dwReserved = 0;
		out.color = vertex.color;
		out.specular = 0;
		out.tu = float(vertex.u * p_uv_scale.x);
		out.tv = float(vertex.v * p_uv_scale.y);
	}
	D3DSTATE transforms[3] = {};
	transforms[0].dtstTransformStateType = D3DTRANSFORMSTATE_WORLD;
	transforms[0].dwArg[0] = _pass->identity_matrix;
	transforms[1].dtstTransformStateType = D3DTRANSFORMSTATE_VIEW;
	transforms[1].dwArg[0] = _pass->identity_matrix;
	transforms[2].dtstTransformStateType = D3DTRANSFORMSTATE_PROJECTION;
	transforms[2].dwArg[0] = _pass->projection_matrix;
	const DWORD filter = p_state.sampler_flags & SAMPLER_FILTER_LINEAR ? D3DFILTER_LINEAR : D3DFILTER_NEAREST;
	const DWORD address = p_state.sampler_flags & (SAMPLER_REPEAT_U | SAMPLER_REPEAT_V) ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
	const bool blend = p_state.blend_mode == BLEND_MODE_MIX;
	// The white texture enables vertex alpha. Its texels are identical, so affine sampling is enough.
	const bool white_texture = p_texture && _blend_white.device_id == _pass->device_id && p_texture == _blend_white.handle;
	D3DCULL cull = D3DCULL_NONE;
	if (p_state.cull_mode == CULL_BACK) {
		cull = D3DCULL_CCW;
	} else if (p_state.cull_mode == CULL_FRONT) {
		cull = D3DCULL_CW;
	}
	const struct {
		D3DRENDERSTATETYPE type;
		DWORD value;
	} settings[] = {
		{ D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD },
		{ D3DRENDERSTATE_CULLMODE, DWORD(cull) },
		{ D3DRENDERSTATE_ZENABLE, p_state.depth_test_enabled || p_state.depth_write_enabled },
		{ D3DRENDERSTATE_ZFUNC, DWORD(p_state.depth_test_enabled ? D3DCMP_LESSEQUAL : D3DCMP_ALWAYS) },
		{ D3DRENDERSTATE_ZWRITEENABLE, p_state.depth_write_enabled },
		{ D3DRENDERSTATE_TEXTUREHANDLE, p_texture },
		{ D3DRENDERSTATE_TEXTUREMAPBLEND, DWORD(blend ? D3DTBLEND_MODULATEALPHA : D3DTBLEND_MODULATE) },
		{ D3DRENDERSTATE_BLENDENABLE, blend },
		{ D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA },
		{ D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA },
		{ D3DRENDERSTATE_TEXTUREMIN, filter },
		{ D3DRENDERSTATE_TEXTUREMAG, filter },
		{ D3DRENDERSTATE_TEXTUREADDRESS, address },
		{ D3DRENDERSTATE_TEXTUREPERSPECTIVE, !white_texture },
	};
	D3DSTATE states[sizeof(settings) / sizeof(settings[0])] = {};
	for (unsigned i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
		states[i].drstRenderStateType = settings[i].type;
		states[i].dwArg[0] = settings[i].value;
	}
	const D3DPROCESSVERTICES transform = { D3DPROCESSVERTICES_TRANSFORM, 0, 0, p_count, 0 };
	const D3DINSTRUCTION triangles = { D3DOP_TRIANGLE, sizeof(D3DTRIANGLE), WORD(p_count / 3) };
	const D3DINSTRUCTION exit = { D3DOP_EXIT, 0, 0 };
	static_assert(6 * sizeof(D3DINSTRUCTION) + sizeof(transforms) + sizeof(states) + sizeof(transform) <= EXECUTE_STATE_BYTES, "Execute buffer state does not fit.");

	uint8_t *output = start + p_count * sizeof(D3DLVERTEX);
	const uint8_t *instructions = output;
	_write_d3d3_software_instruction(output, D3DOP_STATETRANSFORM, transforms, 3);
	_write_d3d3_software_instruction(output, D3DOP_STATERENDER, states, WORD(sizeof(states) / sizeof(states[0])));
	_write_d3d3_software_instruction(output, D3DOP_PROCESSVERTICES, &transform, 1);
	memcpy(output, &triangles, sizeof(triangles));
	output += sizeof(triangles);
	for (uint32_t i = 0; i < p_count; i += 3) {
		D3DTRIANGLE triangle = {};
		triangle.v1 = WORD(i);
		triangle.v2 = WORD(i + 1);
		triangle.v3 = WORD(i + 2);
		triangle.wFlags = D3DTRIFLAG_EDGEENABLETRIANGLE;
		memcpy(output, &triangle, sizeof(triangle));
		output += sizeof(triangle);
	}
	memcpy(output, &exit, sizeof(exit));
	output += sizeof(exit);
	D3DEXECUTEDATA data = {};
	data.dwSize = sizeof(data);
	data.dwVertexCount = p_count;
	data.dwInstructionOffset = DWORD(instructions - start);
	data.dwInstructionLength = DWORD(output - instructions);
	error = _check_result(_pass->execute->Unlock());
	if (error == OK) {
		error = _check_result(_pass->execute->SetExecuteData(&data));
	}
	return error == OK ? _check_result(_pass->device->Execute(_pass->execute, _pass->viewport, D3DEXECUTE_CLIPPED)) : error;
}

bool FixedFunctionGraphicsDeviceD3D3Software::_probe_alpha_blend() {
	// The legacy RGB device can blend even when GetCaps omits the relevant bits.
	// Verify a pixel without presenting or allocating another window-sized surface.
	enum { PROBE_SIZE = 8 };
	if (_pass->native_size.x < PROBE_SIZE || _pass->native_size.y < PROBE_SIZE) {
		return false;
	}
	uint8_t rgba[PROBE_SIZE * PROBE_SIZE * 4];
	memset(rgba, 255, sizeof(rgba));
	for (size_t i = 3; i < sizeof(rgba); i += 4) {
		rgba[i] = 128;
	}
	D3D3SoftwareTextureView view;
	if (_upload_texture_view(view, Size2i(PROBE_SIZE, PROBE_SIZE), _alpha_texture_format, rgba) != OK ||
			FAILED(view.texture->GetHandle(_pass->device, &view.handle)) || !view.handle ||
			_set_viewport(Rect2i(0, 0, PROBE_SIZE, PROBE_SIZE)) != OK) {
		return false;
	}
	D3DMATERIAL background = {};
	background.dwSize = sizeof(background);
	background.diffuse.b = background.diffuse.a = 1;
	D3DMATRIX identity = {};
	identity._11 = identity._22 = identity._33 = identity._44 = 1;
	D3DRECT area = {};
	area.x2 = area.y2 = PROBE_SIZE;
	_pass->projection_valid = false;
	if (FAILED(_pass->background->SetMaterial(&background)) || FAILED(_pass->device->SetMatrix(_pass->projection_matrix, &identity)) || FAILED(_pass->device->BeginScene())) {
		return false;
	}
	const FixedFunctionVertex vertices[] = {
		{ -1, 1, 0.5f, 0x80ff0000, 0, 0 },
		{ 1, 1, 0.5f, 0x80ff0000, 0, 0 },
		{ 0, -1, 0.5f, 0x80ff0000, 0, 0 },
	};
	FFDrawParameters state;
	state.blend_mode = BLEND_MODE_MIX;
	const bool drawn = SUCCEEDED(_pass->viewport->Clear(1, &area, D3DCLEAR_TARGET)) && _draw_batch(vertices, 3, view.handle, state) == OK;
	if (FAILED(_pass->device->EndScene()) || !drawn) {
		return false;
	}
	DDSURFACEDESC desc = {};
	desc.dwSize = sizeof(desc);
	if (FAILED(_pass->color->Lock(nullptr, &desc, DDLOCK_WAIT | DDLOCK_READONLY, nullptr))) {
		return false;
	}
	const DDPIXELFORMAT &format = desc.ddpfPixelFormat;
	const uint32_t bytes = format.dwRGBBitCount / 8;
	uint32_t pixel = 0;
	if ((format.dwFlags & DDPF_RGB) && bytes >= 2 && bytes <= 4 && format.dwRBitMask && format.dwBBitMask) {
		const uint8_t *row = static_cast<const uint8_t *>(desc.lpSurface) + (PROBE_SIZE / 2) * desc.lPitch;
		memcpy(&pixel, row + (PROBE_SIZE / 2) * bytes, bytes);
	}
	if (FAILED(_pass->color->Unlock(nullptr)) || !pixel) {
		return false;
	}
	const uint32_t red = _unpack_channel(pixel, format.dwRBitMask);
	const uint32_t blue = _unpack_channel(pixel, format.dwBBitMask);
	// 50% texture coverage times 50% vertex opacity gives roughly 25% red.
	return red >= 52 && red <= 76 && blue >= 179 && blue <= 203;
}

Rect2i FixedFunctionGraphicsDeviceD3D3Software::_get_native_draw_viewport(const FFDrawParameters &p_state) const {
	Rect2i viewport = get_draw_viewport(p_state);
	const Size2i &native_size = _pass->native_size;
	const Size2i &logical_size = _pass->size;
	if (native_size != logical_size) {
		// Round shared clip edges consistently so adjacent logical rectangles stay adjacent.
		const int left = (int64_t(viewport.position.x) * native_size.x + logical_size.x / 2) / logical_size.x;
		const int top = (int64_t(viewport.position.y) * native_size.y + logical_size.y / 2) / logical_size.y;
		const int right = (int64_t(viewport.position.x + viewport.size.x) * native_size.x + logical_size.x / 2) / logical_size.x;
		const int bottom = (int64_t(viewport.position.y + viewport.size.y) * native_size.y + logical_size.y / 2) / logical_size.y;
		viewport = Rect2i(left, top, right - left, bottom - top);
	}
	return viewport;
}

bool FixedFunctionGraphicsDeviceD3D3Software::_projection_matches(const CameraMatrix &p_first, const CameraMatrix &p_second) {
	for (int column = 0; column < 4; column++) {
		for (int row = 0; row < 4; row++) {
			if (p_first.matrix[column][row] != p_second.matrix[column][row]) {
				return false;
			}
		}
	}
	return true;
}

Error FixedFunctionGraphicsDeviceD3D3Software::draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state) {
	Error error = _check_thread();
	if (error != OK || !_frame_active) {
		return error == OK ? ERR_UNCONFIGURED : error;
	}
	if (!p_vertices || !p_vertex_count || p_vertex_count % 3 || (!_depth_active && (p_state.depth_test_enabled || p_state.depth_write_enabled))) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_texture.is_valid() && !_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	error = validate_draw_state(p_state);
	if (error != OK) {
		return error;
	}
	const Rect2i viewport = _get_native_draw_viewport(p_state);
	if (!viewport.size.x || !viewport.size.y) {
		return OK;
	}
	D3DTEXTUREHANDLE handle = 0;
	FFDrawParameters state = p_state;
	Vector2 uv_scale(1, 1);
	const bool blend = state.blend_mode == BLEND_MODE_MIX;
	bool uses_texture_alpha = false;
	// The RGB rasterizer sources alpha from the texture stage when blending.
	D3D3SoftwareTextureView *view = blend ? &_blend_white : nullptr;
	if (p_texture.is_valid()) {
		D3D3SoftwareTexture *texture = _texture_owner.getptr(p_texture);
		if (texture->target) {
			if (texture->target == _pass) {
				return ERR_INVALID_PARAMETER;
			}
			if (!texture->target->readable) {
				return ERR_UNCONFIGURED;
			}
			uv_scale = texture->uv_scale;
			if (uv_scale != Vector2(1, 1)) {
				// Point sampling avoids blending with unused backing pixels.
				state.sampler_flags &= ~(SAMPLER_FILTER_LINEAR | SAMPLER_REPEAT_U | SAMPLER_REPEAT_V | SAMPLER_REPEAT_MIRRORED);
			}
		}
		if (state.texture_rgb_enabled) {
			uses_texture_alpha = blend && state.texture_alpha_enabled && texture->alpha.texture;
			view = uses_texture_alpha ? &texture->alpha : &texture->opaque;
			if (uses_texture_alpha && texture->sprite_blit_eligible && p_vertex_count == 6 && !state.sampler_flags && !state.font_glyph &&
					!state.alpha_cutoff_enabled && !state.depth_test_enabled && !state.depth_write_enabled && state.cull_mode == CULL_DISABLED) {
				error = _blit_canvas_sprite(*texture, p_vertices, p_clip_from_model, viewport);
				if (error != ERR_UNAVAILABLE) {
					return error;
				}
			}
			if (uses_texture_alpha && state.font_glyph) {
				error = _prepare_binary_glyph_view(*texture);
				if (error != OK) {
					return error;
				}
				view = &texture->binary_glyph;
				// Filtering the mask would recreate partial glyph coverage.
				state.sampler_flags &= ~SAMPLER_FILTER_LINEAR;
			}
		}
	}
	if (blend && !uses_texture_alpha) {
		bool opaque = true;
		for (uint32_t i = 0; i < p_vertex_count; i++) {
			if ((p_vertices[i].color >> 24) != 255) {
				opaque = false;
				break;
			}
		}
		if (opaque) {
			state.blend_mode = BLEND_MODE_DISABLED;
			if (view == &_blend_white) {
				view = nullptr;
			}
		}
	}
	if (view) {
		// A texture handle belongs to the sampling device and expires when that device is recreated.
		if (view->device_id != _pass->device_id || !view->handle) {
			error = _check_result(view->texture->GetHandle(_pass->device, &view->handle));
			if (error != OK) {
				return error;
			}
			view->device_id = _pass->device_id;
		}
		handle = view->handle;
	}
	error = _set_viewport(viewport);
	if (error != OK) {
		return error;
	}
	if (!_pass->projection_valid || !_projection_matches(_pass->submitted_projection, p_clip_from_model)) {
		// Account for the native clip rectangle and integer pixel centers in D3D's [0, w] depth range.
		const Size2i &native_size = _pass->native_size;
		CameraMatrix correction;
		correction.matrix[2][2] = correction.matrix[3][2] = real_t(0.5);
		correction.matrix[0][0] = real_t(native_size.x) / viewport.size.x;
		correction.matrix[1][1] = real_t(native_size.y) / viewport.size.y;
		correction.matrix[3][0] = (real_t(native_size.x) - real_t(2) * viewport.position.x - viewport.size.x - 1) / viewport.size.x;
		correction.matrix[3][1] = (real_t(2) * viewport.position.y + viewport.size.y - native_size.y + 1) / viewport.size.y;
		float matrix[16];
		store_camera(correction * p_clip_from_model, matrix);
		D3DMATRIX projection;
		memcpy(&projection, matrix, sizeof(projection));
		error = _check_result(_pass->device->SetMatrix(_pass->projection_matrix, &projection));
		_pass->projection_valid = error == OK;
		if (error != OK) {
			return error;
		}
		_pass->submitted_projection = p_clip_from_model;
	}
	const uint32_t limit = 3 * _capabilities.max_primitives_per_draw;
	while (error == OK && p_vertex_count) {
		const uint32_t count = MIN(p_vertex_count, limit);
		error = _draw_batch(p_vertices, count, handle, state, uv_scale);
		p_vertices += count;
		p_vertex_count -= count;
	}
	return error;
}
