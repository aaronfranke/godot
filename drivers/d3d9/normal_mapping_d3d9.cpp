#include "graphics_device_d3d9.h"

#include "shaders/lighting_bytecode.h"

#include "core/math/math_funcs.h"

static_assert(sizeof(FixedFunctionNormalMapVertex) == 72 && offsetof(FixedFunctionNormalMapVertex, tx) == 48 && offsetof(FixedFunctionNormalMapVertex, normal_u) == 64, "Unexpected normal-map vertex layout.");

Error FixedFunctionGraphicsDeviceD3D9::_create_normal_map_shaders() {
	if (!supports_normal_mapping()) {
		return ERR_UNAVAILABLE;
	}
	if (_normal_map_vertex_shader) {
		return OK;
	}
	const D3DVERTEXELEMENT9 declaration[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		{ 0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
		{ 0, 48, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
		{ 0, 64, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		D3DDECL_END(),
	};
	HRESULT result = _device->CreateVertexDeclaration(declaration, &_normal_map_declaration);
	if (SUCCEEDED(result)) {
		result = _device->CreateVertexShader(reinterpret_cast<const DWORD *>(FixedFunctionShadersD3D9::VERTEX_NORMAL_MAP), &_normal_map_vertex_shader);
	}
	if (SUCCEEDED(result)) {
		result = _device->CreatePixelShader(reinterpret_cast<const DWORD *>(FixedFunctionShadersD3D9::PIXEL_NORMAL_MAP), &_normal_map_pixel_shader);
	}
	if (SUCCEEDED(result)) {
		result = _device->CreatePixelShader(reinterpret_cast<const DWORD *>(FixedFunctionShadersD3D9::PIXEL_TEXTURED_NORMAL_MAP), &_normal_map_textured_shader);
	}
	if (SUCCEEDED(result)) {
		return OK;
	}
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
	_normal_map_unavailable = true;
	if (result == D3DERR_INVALIDCALL || result == D3DERR_NOTAVAILABLE || result == E_OUTOFMEMORY || result == D3DERR_OUTOFVIDEOMEMORY) {
		return ERR_UNAVAILABLE; // Retain ordinary SM3 and per-vertex lighting.
	}
	return _check_result(result);
}

void FixedFunctionGraphicsDeviceD3D9::D3D9OcclusionShaders::release() {
	if (declaration) {
		declaration->Release();
		declaration = nullptr;
	}
	if (vertex) {
		vertex->Release();
		vertex = nullptr;
	}
	if (pixel) {
		pixel->Release();
		pixel = nullptr;
	}
	if (textured) {
		textured->Release();
		textured = nullptr;
	}
	unavailable = false;
}

Error FixedFunctionGraphicsDeviceD3D9::_create_occlusion_shaders(bool p_normal_mapped) {
	if (p_normal_mapped ? !supports_normal_mapped_occlusion() : !supports_occlusion_mapping()) {
		return ERR_UNAVAILABLE;
	}
	D3D9OcclusionShaders &shaders = p_normal_mapped ? _normal_occlusion_shaders : _occlusion_shaders;
	if (shaders.vertex) {
		return OK;
	}
	HRESULT result = D3D_OK;
	if (!p_normal_mapped) {
		// The lit color slot can hold a second raw UV. TEXCOORD keeps its float range.
		const D3DVERTEXELEMENT9 declaration[] = {
			{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
			{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			{ 0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
			D3DDECL_END(),
		};
		result = _device->CreateVertexDeclaration(declaration, &shaders.declaration);
	}
	if (SUCCEEDED(result)) {
		const uint32_t *bytecode = p_normal_mapped ? FixedFunctionShadersD3D9::VERTEX_NORMAL_OCCLUSION : FixedFunctionShadersD3D9::VERTEX_OCCLUSION;
		result = _device->CreateVertexShader(reinterpret_cast<const DWORD *>(bytecode), &shaders.vertex);
	}
	if (SUCCEEDED(result)) {
		const uint32_t *bytecode = p_normal_mapped ? FixedFunctionShadersD3D9::PIXEL_NORMAL_OCCLUSION : FixedFunctionShadersD3D9::PIXEL_OCCLUSION;
		result = _device->CreatePixelShader(reinterpret_cast<const DWORD *>(bytecode), &shaders.pixel);
	}
	if (SUCCEEDED(result)) {
		const uint32_t *bytecode = p_normal_mapped ? FixedFunctionShadersD3D9::PIXEL_TEXTURED_NORMAL_OCCLUSION : FixedFunctionShadersD3D9::PIXEL_TEXTURED_OCCLUSION;
		result = _device->CreatePixelShader(reinterpret_cast<const DWORD *>(bytecode), &shaders.textured);
	}
	if (SUCCEEDED(result)) {
		return OK;
	}
	shaders.release();
	if (result == D3DERR_INVALIDCALL || result == D3DERR_NOTAVAILABLE) {
		shaders.unavailable = true;
		return ERR_UNAVAILABLE;
	}
	const Error error = _check_result(result);
	return error == ERR_OUT_OF_MEMORY ? ERR_UNAVAILABLE : error;
}

Error FixedFunctionGraphicsDeviceD3D9::geometry_create_normal_mapped(const PoolVector<FixedFunctionNormalMapVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, RID &r_geometry) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_device) {
		return ERR_UNCONFIGURED;
	}
	if (_frame_active || _resetting) {
		return ERR_BUSY;
	}
	if (_geometry_vertex_limit < 3 || !_capabilities.max_primitives_per_draw) {
		return ERR_UNAVAILABLE;
	}
	error = _create_normal_map_shaders();
	if (error != OK) {
		return error;
	}
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	error = FixedFunctionGeometryData::partition(p_vertices, p_indices, _geometry_vertex_limit, _capabilities.max_primitives_per_draw, batches);
	if (error != OK) {
		return error;
	}
	const PoolVector<FixedFunctionNormalMapVertex>::Read vertices = p_vertices.read();
	for (int i = 0; i < p_vertices.size(); i++) {
		const FixedFunctionNormalMapVertex &v = vertices[i];
		const Vector3 normal(v.lighting.nx, v.lighting.ny, v.lighting.nz), tangent(v.tx, v.ty, v.tz);
		// Bound vector-transform intermediates as well as individual stored floats.
		if (Math::abs(normal.length_squared() - 1) > real_t(0.001) || Math::abs(tangent.length_squared() - 1) > real_t(0.001) || Math::abs(v.tw) != 1 || normal.cross(tangent).length_squared() < real_t(0.000001)) {
			return ERR_UNAVAILABLE;
		}
		const float values[] = { v.lighting.x, v.lighting.y, v.lighting.z, v.lighting.nx, v.lighting.ny, v.lighting.nz, v.lighting.u, v.lighting.v, v.lighting.r, v.lighting.g, v.lighting.b, v.lighting.a, v.tx, v.ty, v.tz, v.tw, v.normal_u, v.normal_v };
		for (int component = 0; component < 18; component++) {
			if (Math::abs(values[component]) > 1.0e10f) {
				return ERR_UNAVAILABLE;
			}
		}
	}
	return _create_geometry(batches, r_geometry);
}
