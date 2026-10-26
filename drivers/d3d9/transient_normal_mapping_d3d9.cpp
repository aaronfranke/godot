#include "graphics_device_d3d9.h"

#include "shaders/lighting_bytecode.h"

#include "core/math/math_funcs.h"

static_assert(sizeof(FixedFunctionTransientNormalMapVertex) == 88 && offsetof(FixedFunctionTransientNormalMapVertex, tx) == 48 && offsetof(FixedFunctionTransientNormalMapVertex, normal_u) == 72 && offsetof(FixedFunctionTransientNormalMapVertex, occlusion_u) == 80, "Unexpected transient normal-map vertex layout.");

Error FixedFunctionGraphicsDeviceD3D9::_create_transient_normal_map_shaders(bool p_occlusion) {
	if (p_occlusion ? !supports_transient_normal_mapped_occlusion() : !supports_transient_normal_mapping()) {
		return ERR_UNAVAILABLE;
	}
	Error error = _create_normal_map_shaders();
	if (error == OK && p_occlusion) {
		error = _create_occlusion_shaders(true);
	}
	if (error != OK) {
		return error;
	}
	IDirect3DVertexShader9 *&shader = p_occlusion ? _transient_normal_occlusion_shader : _transient_normal_vertex_shader;
	if (shader) {
		return OK;
	}
	HRESULT result = D3D_OK;
	if (!_transient_normal_declaration) {
		const D3DVERTEXELEMENT9 declaration[] = {
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
			{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			{ 0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
			{ 0, 48, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
			{ 0, 60, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },
			{ 0, 72, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
			{ 0, 80, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
			D3DDECL_END(),
		};
		result = _device->CreateVertexDeclaration(declaration, &_transient_normal_declaration);
	}
	if (SUCCEEDED(result)) {
		const uint32_t *bytecode = p_occlusion ? FixedFunctionShadersD3D9::VERTEX_TRANSIENT_NORMAL_OCCLUSION : FixedFunctionShadersD3D9::VERTEX_TRANSIENT_NORMAL_MAP;
		result = _device->CreateVertexShader(reinterpret_cast<const DWORD *>(bytecode), &shader);
	}
	if (SUCCEEDED(result)) {
		return OK;
	}
	if (p_occlusion) {
		_transient_normal_occlusion_unavailable = true;
	} else {
		_transient_normal_unavailable = true;
	}
	if (result == D3DERR_INVALIDCALL || result == D3DERR_NOTAVAILABLE || result == E_OUTOFMEMORY || result == D3DERR_OUTOFVIDEOMEMORY) {
		return ERR_UNAVAILABLE;
	}
	return _check_result(result);
}

Error FixedFunctionGraphicsDeviceD3D9::draw_normal_mapped_triangles(const FixedFunctionTransientNormalMapVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters &p_normal, const FFOcclusionMapParameters *p_occlusion, const uint16_t *p_indices, uint32_t p_index_count) {
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
	if (!_texture_owner.owns(p_normal.texture) || p_normal.texture == _render_target || !(Math::abs(p_normal.strength) <= 1.0e10f)) {
		return ERR_UNAVAILABLE;
	}
	const D3D9Texture *normal = _texture_owner.getptr(p_normal.texture);
	if (!normal->native || (normal->render_target && !normal->rendered)) {
		return ERR_UNAVAILABLE;
	}
	FFDrawParameters sampling = p_state;
	sampling.sampler_flags = p_normal.sampler_flags;
	if (validate_draw_state(sampling) != OK) {
		return ERR_UNAVAILABLE;
	}
	const D3D9Texture *occlusion = nullptr;
	if (p_occlusion) {
		if (!_texture_owner.owns(p_occlusion->texture) || p_occlusion->texture == _render_target || !(p_occlusion->light_affect >= 0 && p_occlusion->light_affect <= 1)) {
			return ERR_UNAVAILABLE;
		}
		occlusion = _texture_owner.getptr(p_occlusion->texture);
		if (!occlusion->native || occlusion->render_target) {
			return ERR_UNAVAILABLE;
		}
		for (int channel = 0; channel < 4; channel++) {
			if (!(Math::abs(p_occlusion->weights[channel]) <= 1.0e10f)) {
				return ERR_UNAVAILABLE;
			}
		}
		sampling.sampler_flags = p_occlusion->sampler_flags;
		if (validate_draw_state(sampling) != OK) {
			return ERR_UNAVAILABLE;
		}
	}
	for (uint32_t i = 0; i < p_vertex_count; i++) {
		const FixedFunctionTransientNormalMapVertex &vertex = p_vertices[i];
		const FixedFunctionLightingVertex &lit = vertex.lighting;
		const float values[] = { lit.x, lit.y, lit.z, lit.nx, lit.ny, lit.nz, lit.u, lit.v, lit.r, lit.g, lit.b, lit.a,
			vertex.tx, vertex.ty, vertex.tz, vertex.bx, vertex.by, vertex.bz, vertex.normal_u, vertex.normal_v, vertex.occlusion_u, vertex.occlusion_v };
		for (size_t component = 0; component < sizeof(values) / sizeof(values[0]); component++) {
			if (!(Math::abs(values[component]) <= 1.0e10f)) {
				return ERR_UNAVAILABLE;
			}
		}
	}
	enum {
		CAMERA_BASIS_REGISTER = 4,
		CAMERA_BASIS_REGISTERS = 3,
		VERTEX_REGISTERS = CAMERA_BASIS_REGISTER + CAMERA_BASIS_REGISTERS
	};
	float constants[VERTEX_REGISTERS * 4];
	store_camera(_get_draw_projection(p_clip_from_view, viewport), constants);
	for (int component = 0; component < CAMERA_BASIS_REGISTER * 4; component++) {
		if (!(Math::abs(constants[component]) <= 1.0e10f)) {
			return ERR_UNAVAILABLE;
		}
	}
	for (int column = 0; column < CAMERA_BASIS_REGISTERS; column++) {
		const Vector3 axis = p_lighting.camera_basis.get_axis(column);
		for (int row = 0; row < 3; row++) {
			constants[(CAMERA_BASIS_REGISTER + column) * 4 + row] = float(axis[row]);
		}
		constants[(CAMERA_BASIS_REGISTER + column) * 4 + 3] = 0;
	}
	error = _create_transient_normal_map_shaders(p_occlusion != nullptr);
	if (error != OK) {
		return error;
	}
	if (occlusion) {
		error = _check_result(_device->SetTexture(2, occlusion->native));
		if (error == OK) {
			error = _check_result(_device->SetSamplerState(2, D3DSAMP_SRGBTEXTURE, FALSE));
		}
		if (error == OK) {
			error = _set_sampler_state(p_occlusion->sampler_flags, 2, occlusion->mip_levels);
		}
	}
	if (error == OK) {
		error = _bind_lighting(constants, VERTEX_REGISTERS, p_lighting, p_texture, p_state, false, &p_normal, p_occlusion);
	}
	if (error != OK) {
		return error;
	}
	const Error draw_error = _check_result(_draw_vertex_stream(p_vertices, p_vertex_count, sizeof(FixedFunctionTransientNormalMapVertex), _normal_vertex_stream, p_indices, p_index_count));
	return draw_error == ERR_UNAVAILABLE ? FAILED : draw_error;
}
