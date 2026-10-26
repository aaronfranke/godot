#include "core/math/math_funcs.h"
#include "drivers/d3d9/graphics_device_d3d9.h"
#include "drivers/d3d9/shaders/lighting_bytecode.h"

FixedFunctionGraphicsDeviceD3D9::D3D9ShadowShaders::D3D9ShadowShaders() :
		vertex(nullptr), unavailable(false) {
	for (int textured = 0; textured < 2; textured++) {
		caster[textured] = nullptr;
		for (int feature = 0; feature < 4; feature++) {
			receiver[feature][textured] = nullptr;
		}
	}
}

void FixedFunctionGraphicsDeviceD3D9::D3D9ShadowShaders::release() {
	if (vertex) {
		vertex->Release();
		vertex = nullptr;
	}
	for (int textured = 0; textured < 2; textured++) {
		if (caster[textured]) {
			caster[textured]->Release();
			caster[textured] = nullptr;
		}
		for (int feature = 0; feature < 4; feature++) {
			if (receiver[feature][textured]) {
				receiver[feature][textured]->Release();
				receiver[feature][textured] = nullptr;
			}
		}
	}
	unavailable = false;
}

Error FixedFunctionGraphicsDeviceD3D9::_create_shadow_shaders() {
	if (!supports_directional_shadows()) {
		return ERR_UNAVAILABLE;
	}
	if (_shadow_shaders.vertex) {
		return OK;
	}
	const uint32_t *casters[2] = { FixedFunctionShadersD3D9::SHADOW_PIXEL, FixedFunctionShadersD3D9::SHADOW_PIXEL_TEXTURED };
	const uint32_t *receivers[4][2] = {
		{ FixedFunctionShadersD3D9::PIXEL_SHADOW, FixedFunctionShadersD3D9::PIXEL_TEXTURED_SHADOW },
		{ FixedFunctionShadersD3D9::PIXEL_NORMAL_SHADOW, FixedFunctionShadersD3D9::PIXEL_TEXTURED_NORMAL_SHADOW },
		{ FixedFunctionShadersD3D9::PIXEL_OCCLUSION_SHADOW, FixedFunctionShadersD3D9::PIXEL_TEXTURED_OCCLUSION_SHADOW },
		{ FixedFunctionShadersD3D9::PIXEL_NORMAL_OCCLUSION_SHADOW, FixedFunctionShadersD3D9::PIXEL_TEXTURED_NORMAL_OCCLUSION_SHADOW },
	};
	HRESULT result = _device->CreateVertexShader(reinterpret_cast<const DWORD *>(FixedFunctionShadersD3D9::SHADOW_VERTEX), &_shadow_shaders.vertex);
	for (int textured = 0; textured < 2 && SUCCEEDED(result); textured++) {
		result = _device->CreatePixelShader(reinterpret_cast<const DWORD *>(casters[textured]), &_shadow_shaders.caster[textured]);
		for (int feature = 0; feature < 4 && SUCCEEDED(result); feature++) {
			result = _device->CreatePixelShader(reinterpret_cast<const DWORD *>(receivers[feature][textured]), &_shadow_shaders.receiver[feature][textured]);
		}
	}
	if (FAILED(result)) {
		const Error error = _check_result(result);
		_shadow_shaders.release();
		if (error == ERR_SKIP) {
			return error;
		}
		_shadow_shaders.unavailable = true;
		return ERR_UNAVAILABLE; // Optional shader refusal retains ordinary Lambert rendering.
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::_unbind_directional_shadow() {
	if (!_shadow_bound) {
		return OK;
	}
	const Error error = _check_result(_device->SetTexture(3, nullptr));
	if (error == OK) {
		_shadow_bound = false;
	}
	return error;
}

Error FixedFunctionGraphicsDeviceD3D9::draw_shadow_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_projection, RID p_texture, const FFDrawParameters &p_state) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_frame_active || !_depth_active) {
		return ERR_UNCONFIGURED;
	}
	if (!_render_target.is_valid() || _texture_owner.get_or_null(_render_target)->transparent || p_state.blend_mode != BLEND_MODE_DISABLED || !p_state.depth_test_enabled || !p_state.depth_write_enabled) {
		return ERR_INVALID_PARAMETER;
	}
	error = _create_shadow_shaders();
	if (error != OK) {
		return error;
	}
	Rect2i viewport;
	error = _prepare_draw(p_vertices, p_vertex_count, p_texture, p_state, viewport);
	if (error != OK || !viewport.size.x || !viewport.size.y) {
		return error;
	}
	error = _set_table_fog(nullptr);
	if (error != OK) {
		return error;
	}
	float matrix[16];
	store_camera(_get_draw_projection(p_projection, viewport), matrix);
	for (int i = 0; i < 16; i++) {
		if (Math::is_nan(matrix[i]) || Math::is_inf(matrix[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	const float settings[4] = { p_state.texture_alpha_enabled ? 1.0f : 0.0f, 0, 0, 0 };
	HRESULT result = _device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
	if (SUCCEEDED(result)) {
		result = _device->SetVertexShader(_shadow_shaders.vertex);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetPixelShader(_shadow_shaders.caster[p_texture.is_valid() ? 1 : 0]);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetVertexShaderConstantF(0, matrix, 4);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetPixelShaderConstantF(0, settings, 1);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
	}
	if (SUCCEEDED(result)) {
		result = _draw_vertex_stream(p_vertices, p_vertex_count, sizeof(FixedFunctionVertex), _vertex_stream);
	}
	const Error draw_error = _check_result(result);
	return draw_error == ERR_UNAVAILABLE ? FAILED : draw_error;
}

Error FixedFunctionGraphicsDeviceD3D9::_bind_directional_shadow(const FFPixelLighting &p_lighting, bool p_normal, bool p_occlusion, bool p_textured, IDirect3DPixelShader9 *&r_shader) {
	const FFDirectionalShadowParameters &shadow = p_lighting.shadow;
	if (!shadow.texture.is_valid() || shadow.light_index >= p_lighting.light_count || !texture_is_render_target_ready(shadow.texture) || p_lighting.lights[shadow.light_index].color_type[3] != 0) {
		return OK;
	}
	float constants[7 * 4] = {}; // c36-c42: four matrix columns, controls, color, light direction.
	store_camera(shadow.clip_from_world, constants);
	constants[16] = float(shadow.light_index);
	constants[17] = shadow.bias;
	constants[18] = shadow.normal_bias;
	for (int channel = 0; channel < 3; channel++) {
		constants[20 + channel] = CLAMP(shadow.color[channel], 0.0f, 1.0f);
		constants[24 + channel] = p_lighting.lights[shadow.light_index].direction_attenuation[channel];
	}
	for (int i = 0; i < 28; i++) {
		if (Math::is_nan(constants[i]) || Math::is_inf(constants[i]) || Math::abs(constants[i]) > 1.0e10f) {
			return OK; // An unsafe upload copy leaves this draw unshadowed.
		}
	}
	Error error = _create_shadow_shaders();
	if (error == ERR_UNAVAILABLE) {
		return OK;
	}
	if (error != OK) {
		return error;
	}
	D3D9Texture *texture = _texture_owner.get_or_null(shadow.texture);
	error = _check_result(_device->SetTexture(3, texture->native));
	if (error == OK) {
		_shadow_bound = true;
		error = _set_sampler_state(0, 3); // Encoded depth must be point sampled, never color-filtered.
	}
	if (error == OK) {
		error = _check_result(_device->SetSamplerState(3, D3DSAMP_SRGBTEXTURE, FALSE));
	}
	if (error == OK) {
		error = _check_result(_device->SetPixelShaderConstantF(36, constants, 7));
	}
	if (error == OK) {
		const int feature = int(p_normal) + 2 * int(p_occlusion);
		r_shader = _shadow_shaders.receiver[feature][p_textured ? 1 : 0];
	}
	return error;
}
