#include "graphics_device_d3d7.h"

#include <string.h>

static_assert(sizeof(FixedFunctionMaskedVertex) == 32 && offsetof(FixedFunctionMaskedVertex, mask_u) == 24, "Unexpected masked vertex layout.");

#define D3D7_MASK_CHECK(m_call)                         \
	do {                                                \
		const Error call_error = _check_result(m_call); \
		if (call_error != OK) {                         \
			return call_error;                          \
		}                                               \
	} while (false)

Error FixedFunctionGraphicsDeviceD3D7::_draw_masked_triangles(const FixedFunctionMaskedVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_projection, RID p_color_texture, RID p_mask_texture, uint8_t p_mask_sampler_flags, const FFDrawParameters &p_state) {
	if (!p_color_texture.is_valid() || !_texture_owner.owns(p_mask_texture) || p_mask_texture == _render_target) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D7Texture *mask = _texture_owner.getptr(p_mask_texture);
	if (!mask->native || (mask->render_target && !mask->rendered)) {
		return ERR_UNCONFIGURED;
	}
	FFDrawParameters mask_state = p_state;
	mask_state.sampler_flags = p_mask_sampler_flags;
	Error error = validate_draw_state(mask_state);
	if (error != OK) {
		return error;
	}
	Rect2i viewport;
	error = _prepare_draw(p_vertices, p_vertex_count, p_color_texture, p_state, viewport);
	if (error != OK || !viewport.size.x || !viewport.size.y) {
		return error;
	}
	// Stage zero supplies emission RGB and vertex alpha. Stage one only multiplies
	// that alpha by the independently sampled base mask; its RGB never contributes.
	D3D7_MASK_CHECK(_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2));
	D3D7_MASK_CHECK(_device->SetTexture(1, mask->native));
	error = _set_sampler_state(p_mask_sampler_flags, mask->mip_levels, 1);
	if (error != OK) {
		return error;
	}
	D3D7_MASK_CHECK(_device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1));
	D3D7_MASK_CHECK(_device->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE));
	D3D7_MASK_CHECK(_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_SELECTARG1));
	D3D7_MASK_CHECK(_device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT));
	D3D7_MASK_CHECK(_device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE));
	D3D7_MASK_CHECK(_device->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT));
	D3D7_MASK_CHECK(_device->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TEXTURE));
	float matrix[16];
	store_camera(_get_draw_projection(p_projection, viewport), matrix);
	D3DMATRIX projection;
	memcpy(&projection, matrix, sizeof(projection));
	D3D7_MASK_CHECK(_device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &projection));
	// Old drivers can reject a combination even with two reported texture stages.
	// Refuse before drawing, so an optional channel cannot blank the base surface.
	DWORD passes = 0;
	error = _check_result(_device->ValidateDevice(&passes));
	if (error == FAILED || (error == OK && passes != 1)) {
		return ERR_UNAVAILABLE;
	}
	if (error != OK) {
		return error;
	}
	error = _check_result(_device->DrawPrimitive(D3DPT_TRIANGLELIST, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2, const_cast<FixedFunctionMaskedVertex *>(p_vertices), p_vertex_count, 0));
	return error == ERR_UNAVAILABLE ? FAILED : error; // Submission failures cannot be optional refusals.
}

Error FixedFunctionGraphicsDeviceD3D7::draw_masked_triangles(const FixedFunctionMaskedVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_model, RID p_color_texture, RID p_mask_texture, uint8_t p_mask_sampler_flags, const FFDrawParameters &p_state) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!supports_texture_mask()) {
		return ERR_UNAVAILABLE;
	}
	error = _draw_masked_triangles(p_vertices, p_vertex_count, p_clip_from_model, p_color_texture, p_mask_texture, p_mask_sampler_flags, p_state);
	const HRESULT draw_result = _last_result;
	// Restore the single-texture cascade on success and on every early refusal.
	const Error stage_error = _check_result(_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE));
	const Error texture_error = _check_result(_device->SetTexture(1, nullptr));
	if (error != OK) {
		_last_result = draw_result;
		return error;
	}
	const Error cleanup_error = stage_error != OK ? stage_error : texture_error;
	return cleanup_error == ERR_UNAVAILABLE ? FAILED : cleanup_error;
}

#undef D3D7_MASK_CHECK
