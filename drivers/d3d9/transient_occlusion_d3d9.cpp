#include "graphics_device_d3d9.h"

#include "shaders/lighting_bytecode.h"

#include "core/math/math_funcs.h"

static_assert(sizeof(FixedFunctionTransientOcclusionVertex) == 56 && offsetof(FixedFunctionTransientOcclusionVertex, occlusion_u) == 48, "Unexpected transient occlusion vertex layout.");

Error FixedFunctionGraphicsDeviceD3D9::_create_transient_occlusion_shader() {
	if (!supports_transient_occlusion_mapping()) {
		return ERR_UNAVAILABLE;
	}
	const Error error = _create_occlusion_shaders(false);
	if (error != OK || _transient_occlusion_shader) {
		return error;
	}
	HRESULT result = D3D_OK;
	if (!_transient_occlusion_declaration) {
		const D3DVERTEXELEMENT9 declaration[] = {
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
			{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			{ 0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
			{ 0, 48, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
			D3DDECL_END(),
		};
		result = _device->CreateVertexDeclaration(declaration, &_transient_occlusion_declaration);
	}
	if (SUCCEEDED(result)) {
		result = _device->CreateVertexShader(reinterpret_cast<const DWORD *>(FixedFunctionShadersD3D9::VERTEX_TRANSIENT_OCCLUSION), &_transient_occlusion_shader);
	}
	if (SUCCEEDED(result)) {
		return OK;
	}
	_transient_occlusion_unavailable = true;
	if (result == D3DERR_INVALIDCALL || result == D3DERR_NOTAVAILABLE || result == E_OUTOFMEMORY || result == D3DERR_OUTOFVIDEOMEMORY) {
		return ERR_UNAVAILABLE;
	}
	return _check_result(result);
}

Error FixedFunctionGraphicsDeviceD3D9::draw_occlusion_mapped_triangles(const FixedFunctionTransientOcclusionVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFOcclusionMapParameters &p_occlusion, const uint16_t *p_indices, uint32_t p_index_count) {
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
	if (!supports_transient_occlusion_mapping() || !_texture_owner.owns(p_occlusion.texture) || p_occlusion.texture == _render_target || !(p_occlusion.light_affect >= 0 && p_occlusion.light_affect <= 1)) {
		return ERR_UNAVAILABLE;
	}
	const D3D9Texture *occlusion = _texture_owner.getptr(p_occlusion.texture);
	if (!occlusion->native || occlusion->render_target) {
		return ERR_UNAVAILABLE;
	}
	for (int channel = 0; channel < 4; channel++) {
		if (!(Math::abs(p_occlusion.weights[channel]) <= 1.0e10f)) {
			return ERR_UNAVAILABLE;
		}
	}
	FFDrawParameters sampling = p_state;
	sampling.sampler_flags = p_occlusion.sampler_flags;
	if (validate_draw_state(sampling) != OK) {
		return ERR_UNAVAILABLE;
	}
	for (uint32_t i = 0; i < p_vertex_count; i++) {
		const FixedFunctionTransientOcclusionVertex &vertex = p_vertices[i];
		const FixedFunctionLightingVertex &lit = vertex.lighting;
		const float values[] = { lit.x, lit.y, lit.z, lit.nx, lit.ny, lit.nz, lit.u, lit.v, lit.r, lit.g, lit.b, lit.a, vertex.occlusion_u, vertex.occlusion_v };
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
	error = _create_transient_occlusion_shader();
	if (error != OK) {
		return error;
	}
	error = _check_result(_device->SetTexture(2, occlusion->native));
	if (error == OK) {
		error = _check_result(_device->SetSamplerState(2, D3DSAMP_SRGBTEXTURE, FALSE));
	}
	if (error == OK) {
		error = _set_sampler_state(p_occlusion.sampler_flags, 2, occlusion->mip_levels);
	}
	if (error == OK) {
		error = _bind_lighting(constants, VERTEX_REGISTERS, p_lighting, p_texture, p_state, false, nullptr, &p_occlusion);
	}
	if (error != OK) {
		return error;
	}
	const Error draw_error = _check_result(_draw_vertex_stream(p_vertices, p_vertex_count, sizeof(FixedFunctionTransientOcclusionVertex), _occlusion_vertex_stream, p_indices, p_index_count));
	return draw_error == ERR_UNAVAILABLE ? FAILED : draw_error;
}
