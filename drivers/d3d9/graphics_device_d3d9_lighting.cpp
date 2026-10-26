#include "graphics_device_d3d9.h"

#include "shaders/lighting_bytecode.h"

#include "core/math/math_funcs.h"

#include <float.h>

static_assert(sizeof(FixedFunctionLightingVertex) == 48 && offsetof(FixedFunctionLightingVertex, nx) == 12 && offsetof(FixedFunctionLightingVertex, u) == 24 && offsetof(FixedFunctionLightingVertex, r) == 32, "Unexpected lighting vertex layout.");
static_assert(sizeof(FixedFunctionGraphicsDevice::FFPixelLight) == 64, "Unexpected lighting constant layout.");
static_assert(FixedFunctionGraphicsDevice::MAX_PIXEL_LIGHTS == 8, "Regenerate the shaders when changing the light capacity.");

void FixedFunctionGraphicsDeviceD3D9::_release_lighting_shaders() {
	_pixel_light_limit = 0;
	_shadow_shaders.release();
	_occlusion_shaders.release();
	_normal_occlusion_shaders.release();
	if (_transient_normal_declaration) {
		_transient_normal_declaration->Release();
		_transient_normal_declaration = nullptr;
	}
	if (_transient_normal_vertex_shader) {
		_transient_normal_vertex_shader->Release();
		_transient_normal_vertex_shader = nullptr;
	}
	if (_transient_normal_occlusion_shader) {
		_transient_normal_occlusion_shader->Release();
		_transient_normal_occlusion_shader = nullptr;
	}
	if (_transient_occlusion_declaration) {
		_transient_occlusion_declaration->Release();
		_transient_occlusion_declaration = nullptr;
	}
	if (_transient_occlusion_shader) {
		_transient_occlusion_shader->Release();
		_transient_occlusion_shader = nullptr;
	}
	_transient_occlusion_unavailable = false;
	_transient_normal_unavailable = _transient_normal_occlusion_unavailable = false;
	if (_normal_map_declaration) {
		_normal_map_declaration->Release();
		_normal_map_declaration = nullptr;
	}
	if (_normal_map_vertex_shader) {
		_normal_map_vertex_shader->Release();
		_normal_map_vertex_shader = nullptr;
	}
	if (_normal_map_pixel_shader) {
		_normal_map_pixel_shader->Release();
		_normal_map_pixel_shader = nullptr;
	}
	if (_normal_map_textured_shader) {
		_normal_map_textured_shader->Release();
		_normal_map_textured_shader = nullptr;
	}
	_normal_map_unavailable = false;
	if (_lighting_geometry_shader) {
		_lighting_geometry_shader->Release();
		_lighting_geometry_shader = nullptr;
	}
	if (_lighting_textured_shader) {
		_lighting_textured_shader->Release();
		_lighting_textured_shader = nullptr;
	}
	if (_lighting_pixel_shader) {
		_lighting_pixel_shader->Release();
		_lighting_pixel_shader = nullptr;
	}
	if (_lighting_vertex_shader) {
		_lighting_vertex_shader->Release();
		_lighting_vertex_shader = nullptr;
	}
	if (_lighting_declaration) {
		_lighting_declaration->Release();
		_lighting_declaration = nullptr;
	}
}

Error FixedFunctionGraphicsDeviceD3D9::_create_lighting_shaders(const D3DCAPS9 &p_caps) {
	if (p_caps.PixelShaderVersion < D3DPS_VERSION(3, 0) ||
			(p_caps.VertexShaderVersion < D3DVS_VERSION(3, 0) && !_device->GetSoftwareVertexProcessing())) {
		return OK;
	}
	const D3DVERTEXELEMENT9 declaration[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		{ 0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
		D3DDECL_END(),
	};
	HRESULT result = _device->CreateVertexDeclaration(declaration, &_lighting_declaration);
	if (SUCCEEDED(result)) {
		result = _device->CreateVertexShader(reinterpret_cast<const DWORD *>(FixedFunctionShadersD3D9::VERTEX), &_lighting_vertex_shader);
	}
	if (SUCCEEDED(result)) {
		result = _device->CreatePixelShader(reinterpret_cast<const DWORD *>(FixedFunctionShadersD3D9::PIXEL), &_lighting_pixel_shader);
	}
	if (SUCCEEDED(result)) {
		result = _device->CreatePixelShader(reinterpret_cast<const DWORD *>(FixedFunctionShadersD3D9::PIXEL_TEXTURED), &_lighting_textured_shader);
	}
	if (FAILED(result)) {
		_release_lighting_shaders();
		// An unsupported optional pipeline must not prevent baseline rendering.
		if (result == D3DERR_INVALIDCALL || result == D3DERR_NOTAVAILABLE) {
			return OK;
		}
		return _check_result(result);
	}
	_pixel_light_limit = MAX_PIXEL_LIGHTS;
	result = _device->CreateVertexShader(reinterpret_cast<const DWORD *>(FixedFunctionShadersD3D9::VERTEX_GEOMETRY), &_lighting_geometry_shader);
	if (FAILED(result) && result != D3DERR_INVALIDCALL && result != D3DERR_NOTAVAILABLE && result != E_OUTOFMEMORY && result != D3DERR_OUTOFVIDEOMEMORY) {
		return _check_result(result);
	}
	// Rejection of this optional cache shader leaves the transient SM3 path available.
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::draw_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices, uint32_t p_index_count) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	error = _validate_pixel_lighting(p_lighting);
	if (error != OK) {
		return error;
	}
	Rect2i viewport;
	error = _prepare_lighting_draw(p_vertices, p_vertex_count, p_indices, p_index_count, p_texture, p_state, viewport);
	if (error != OK || !viewport.size.x || !viewport.size.y) {
		return error;
	}
	for (uint32_t i = 0; i < p_vertex_count; i++) {
		const FixedFunctionLightingVertex &vertex = p_vertices[i];
		const float values[] = { vertex.x, vertex.y, vertex.z, vertex.nx, vertex.ny, vertex.nz, vertex.u, vertex.v, vertex.r, vertex.g, vertex.b, vertex.a };
		for (int component = 0; component < 12; component++) {
			if (Math::is_nan(values[component]) || Math::is_inf(values[component]) || Math::abs(values[component]) > 1.0e10f) {
				return ERR_UNAVAILABLE;
			}
		}
	}
	float vertex_constants[7 * 4];
	store_camera(_get_draw_projection(p_clip_from_view, viewport), vertex_constants);
	for (int component = 0; component < 16; component++) {
		if (Math::is_nan(vertex_constants[component]) || Math::is_inf(vertex_constants[component]) || Math::abs(vertex_constants[component]) > 1.0e10f) {
			return ERR_UNAVAILABLE;
		}
	}
	for (int column = 0; column < 3; column++) {
		const Vector3 axis = p_lighting.camera_basis.get_axis(column);
		for (int row = 0; row < 3; row++) {
			vertex_constants[16 + column * 4 + row] = float(axis[row]);
		}
		vertex_constants[16 + column * 4 + 3] = 0;
	}
	error = _bind_lighting(vertex_constants, 7, p_lighting, p_texture, p_state, false);
	if (error != OK) {
		return error;
	}
	const HRESULT result = _draw_vertex_stream(p_vertices, p_vertex_count, sizeof(FixedFunctionLightingVertex), _lighting_vertex_stream, p_indices, p_index_count);
	const Error draw_error = _check_result(result);
	// Only pre-submission rejection may request the renderer's fallback.
	return draw_error == ERR_UNAVAILABLE ? FAILED : draw_error;
}

Error FixedFunctionGraphicsDeviceD3D9::_validate_pixel_lighting(const FFPixelLighting &p_lighting) const {
	if (!_pixel_light_limit) {
		return ERR_UNAVAILABLE;
	}
	if (p_lighting.light_count > _pixel_light_limit) {
		return ERR_UNAVAILABLE;
	}
	// Restrict the upload copy to finite ranges where the shader's distance and
	// matrix intermediates cannot overflow. The renderer retains the CPU fallback.
	for (int column = 0; column < 3; column++) {
		const Vector3 axis = p_lighting.camera_basis.get_axis(column);
		for (int row = 0; row < 3; row++) {
			if (Math::is_nan(axis[row]) || Math::is_inf(axis[row]) || Math::abs(axis[row]) > real_t(10000)) {
				return ERR_UNAVAILABLE;
			}
		}
	}
	for (int channel = 0; channel < 3; channel++) {
		if (Math::is_nan(p_lighting.ambient[channel]) || Math::is_inf(p_lighting.ambient[channel]) || Math::abs(p_lighting.ambient[channel]) > 1.0e10f) {
			return ERR_UNAVAILABLE;
		}
	}
	for (uint32_t i = 0; i < p_lighting.light_count; i++) {
		const FFPixelLight &light = p_lighting.lights[i];
		const float *registers[] = { light.color_type, light.position_range, light.direction_attenuation, light.cone };
		for (int reg = 0; reg < 4; reg++) {
			for (int component = 0; component < 4; component++) {
				const float value = registers[reg][component];
				if (Math::is_nan(value) || Math::is_inf(value) || Math::abs(value) > 1.0e10f) {
					return ERR_UNAVAILABLE;
				}
			}
		}
		if (light.color_type[3] < 0 || light.color_type[3] > 2 || light.direction_attenuation[3] < 0 || light.cone[1] < 0 ||
				(light.color_type[3] != 0 && light.position_range[3] <= 0) || (light.color_type[3] == 2 && (light.cone[0] < -1 || 1.0f - light.cone[0] < 1.0e-6f))) {
			return ERR_UNAVAILABLE;
		}
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::_bind_lighting(const float *p_vertex_constants, uint32_t p_register_count, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, bool p_retained, const FFNormalMapParameters *p_normal, const FFOcclusionMapParameters *p_occlusion) {
	if (p_texture.is_valid() && !p_state.texture_rgb_enabled) {
		return ERR_UNAVAILABLE;
	}
	const Error fog_error = _set_table_fog(nullptr);
	if (fog_error != OK) {
		return fog_error;
	}
	float fog_depth[4] = {};
	Color fog_color(0, 0, 0);
	if (p_state.fog) {
		const FFFog &fog = *p_state.fog;
		const float begin = fog.orthogonal ? fog.projected_begin : fog.begin;
		const float end = fog.orthogonal ? fog.projected_end : fog.end;
		if (end > begin) {
			const double inverse_range = 1.0 / (double(end) - begin);
			const double bias = -begin * inverse_range;
			if (inverse_range <= FLT_MAX && Math::abs(bias) <= FLT_MAX) {
				fog_depth[fog.orthogonal ? 0 : 1] = float(inverse_range);
				fog_depth[2] = float(bias);
				fog_color = fog.get_blend_color(p_state.blend_mode);
			}
		}
	}
	float pixel_constants[2 * 4 + MAX_PIXEL_LIGHTS * 16] = {};
	for (int channel = 0; channel < 3; channel++) {
		pixel_constants[channel] = p_lighting.ambient[channel];
	}
	pixel_constants[5] = p_texture.is_valid() && p_state.texture_alpha_enabled ? 1.0f : 0.0f;
	pixel_constants[6] = float(p_lighting.light_count);
	if (p_occlusion) {
		pixel_constants[7] = p_occlusion->light_affect;
	}
	for (uint32_t i = 0; i < p_lighting.light_count; i++) {
		const FFPixelLight &light = p_lighting.lights[i];
		const float *registers[] = { light.color_type, light.position_range, light.direction_attenuation, light.cone };
		for (int reg = 0; reg < 4; reg++) {
			for (int component = 0; component < 4; component++) {
				pixel_constants[8 + i * 16 + reg * 4 + component] = registers[reg][component];
			}
		}
	}
	IDirect3DVertexShader9 *vertex_shader = p_retained ? _lighting_geometry_shader : _lighting_vertex_shader;
	IDirect3DPixelShader9 *pixel_shader = p_texture.is_valid() ? _lighting_textured_shader : _lighting_pixel_shader;
	IDirect3DVertexDeclaration9 *declaration = _lighting_declaration;
	if (p_normal) {
		declaration = p_retained ? _normal_map_declaration : _transient_normal_declaration;
		vertex_shader = p_retained ? _normal_map_vertex_shader : _transient_normal_vertex_shader;
		pixel_shader = p_texture.is_valid() ? _normal_map_textured_shader : _normal_map_pixel_shader;
	}
	if (p_occlusion) {
		const D3D9OcclusionShaders &shaders = p_normal ? _normal_occlusion_shaders : _occlusion_shaders;
		if (p_retained) {
			vertex_shader = shaders.vertex;
			if (shaders.declaration) {
				declaration = shaders.declaration;
			}
		} else if (p_normal) {
			vertex_shader = _transient_normal_occlusion_shader;
		} else {
			vertex_shader = _transient_occlusion_shader;
			declaration = _transient_occlusion_declaration;
		}
		pixel_shader = p_texture.is_valid() ? shaders.textured : shaders.pixel;
	}
	const Error shadow_error = _bind_directional_shadow(p_lighting, p_normal != nullptr, p_occlusion != nullptr, p_texture.is_valid(), pixel_shader);
	if (shadow_error != OK) {
		return shadow_error;
	}
	HRESULT result = _device->SetVertexDeclaration(declaration);
	if (SUCCEEDED(result)) {
		result = _device->SetVertexShader(vertex_shader);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetPixelShader(pixel_shader);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetVertexShaderConstantF(0, p_vertex_constants, p_register_count);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetPixelShaderConstantF(0, pixel_constants, 2 + MAX_PIXEL_LIGHTS * 4);
	}
	// Register assignments match fog_depth and fog_color in lighting.hlsl.
	if (SUCCEEDED(result)) {
		result = _device->SetVertexShaderConstantF(16, fog_depth, 1);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetPixelShaderConstantF(43, fog_color.components, 1);
	}
	if (SUCCEEDED(result) && p_normal) {
		const float strength[4] = { p_normal->strength, 0, 0, 0 };
		result = _device->SetPixelShaderConstantF(2 + MAX_PIXEL_LIGHTS * 4, strength, 1);
		if (SUCCEEDED(result)) {
			result = _device->SetTexture(1, _texture_owner.getptr(p_normal->texture)->native);
			_normal_map_bound = SUCCEEDED(result);
		}
		if (SUCCEEDED(result)) {
			result = _device->SetSamplerState(1, D3DSAMP_SRGBTEXTURE, FALSE);
		}
		if (SUCCEEDED(result)) {
			const Error error = _set_sampler_state(p_normal->sampler_flags, 1, _texture_owner.getptr(p_normal->texture)->mip_levels);
			if (error != OK) {
				return error;
			}
		}
	}
	if (SUCCEEDED(result) && p_occlusion) {
		const Color &weights = p_occlusion->weights;
		const float components[4] = { weights.r, weights.g, weights.b, weights.a };
		// Occlusion weights follow the normal-strength register in lighting.hlsl.
		result = _device->SetPixelShaderConstantF(3 + MAX_PIXEL_LIGHTS * 4, components, 1);
	}
	return _check_result(result);
}
