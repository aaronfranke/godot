#include "graphics_device_d3d9.h"

#include "core/math/math_funcs.h"

#include <float.h>
#include <string.h>

// The fixed-function vertex layout also works on pre-SM3 adapters.
static const DWORD D3D9_VERTEX_LIT_FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;

HRESULT FixedFunctionGraphicsDeviceD3D9::_upload_vertex_lit_batch(D3D9GeometryBatch *p_batch, bool p_ambient_occlusion) {
	if (p_batch->lit_ambient_occlusion != p_ambient_occlusion) {
		if (p_batch->lit_vertices) {
			p_batch->lit_vertices->Release();
			p_batch->lit_vertices = nullptr;
		}
		p_batch->lit_unavailable = false;
		p_batch->lit_ambient_occlusion = p_ambient_occlusion;
	}
	if (p_batch->lit_vertices || p_batch->lit_unavailable) {
		return D3D_OK;
	}
	DWORD usage = D3DUSAGE_WRITEONLY;
	if (_device->GetSoftwareVertexProcessing()) {
		usage |= D3DUSAGE_SOFTWAREPROCESSING;
	}
	const uint32_t vertex_count = p_batch->source.lighting_vertices.size();
	const uint32_t stride = p_ambient_occlusion ? sizeof(FixedFunctionLitColorVertex) : sizeof(FixedFunctionLitVertex);
	const uint32_t vertex_bytes = vertex_count * stride;
	const uint32_t index_bytes = p_batch->source.indices.size() * sizeof(uint16_t);
	const bool create_indices = !p_batch->indices;
	// The compact layout matches an FVF, which pre-DX9 drivers require for fixed-function input.
	const DWORD fvf = D3D9_VERTEX_LIT_FVF | (p_ambient_occlusion ? D3DFVF_DIFFUSE : 0);
	HRESULT result = _device->CreateVertexBuffer(vertex_bytes, usage, fvf, D3DPOOL_DEFAULT, &p_batch->lit_vertices, nullptr);
	if (SUCCEEDED(result) && create_indices) {
		result = _device->CreateIndexBuffer(index_bytes, usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &p_batch->indices, nullptr);
	}
	void *data = nullptr;
	if (SUCCEEDED(result)) {
		result = p_batch->lit_vertices->Lock(0, vertex_bytes, &data, D3DLOCK_NOSYSLOCK);
		if (SUCCEEDED(result)) {
			if (data) {
				FixedFunctionGeometryData::pack_fixed_lit_vertices(p_batch->source.lighting_vertices, data, p_ambient_occlusion);
			}
			result = p_batch->lit_vertices->Unlock();
			if (!data && SUCCEEDED(result)) {
				result = E_FAIL;
			}
		}
	}
	if (SUCCEEDED(result) && create_indices) {
		data = nullptr;
		result = p_batch->indices->Lock(0, index_bytes, &data, D3DLOCK_NOSYSLOCK);
		if (SUCCEEDED(result)) {
			if (data) {
				const PoolVector<uint16_t>::Read source = p_batch->source.indices.read();
				memcpy(data, source.ptr(), index_bytes);
			}
			result = p_batch->indices->Unlock();
			if (!data && SUCCEEDED(result)) {
				result = E_FAIL;
			}
		}
	}
	if (FAILED(result)) {
		if (p_batch->lit_vertices) {
			p_batch->lit_vertices->Release();
			p_batch->lit_vertices = nullptr;
		}
		if (create_indices && p_batch->indices) {
			p_batch->indices->Release();
			p_batch->indices = nullptr;
		}
		if (result == D3DERR_NOTAVAILABLE || result == D3DERR_INVALIDCALL || result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY) {
			// Rejection precedes submission; the renderer keeps its CPU lighting path.
			p_batch->lit_unavailable = true;
			return D3D_OK;
		}
	}
	return result;
}

Error FixedFunctionGraphicsDeviceD3D9::draw_vertex_lit_geometry(RID p_geometry, const CameraMatrix &p_clip_from_model, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D9Geometry *geometry = _geometry_owner.getptr(p_geometry);
	if (!geometry->batches[0]->source.lighting_vertices.size()) {
		return ERR_INVALID_PARAMETER;
	}
	return _draw_vertex_lighting(geometry, nullptr, 0, nullptr, 0, p_clip_from_model, p_lighting, p_color, p_uv_scale, p_uv_offset, p_texture, p_state);
}

Error FixedFunctionGraphicsDeviceD3D9::draw_vertex_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_view, const FFPixelLighting &p_lighting, const Color &p_color, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices, uint32_t p_index_count) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!p_vertices || !p_vertex_count || (p_indices == nullptr) != (p_index_count == 0) || (p_indices ? p_index_count : p_vertex_count) % 3) {
		return ERR_INVALID_PARAMETER;
	}
	return _draw_vertex_lighting(nullptr, p_vertices, p_vertex_count, p_indices, p_index_count, p_clip_from_view, p_lighting, p_color, Vector2(1, 1), Vector2(), p_texture, p_state);
}

Error FixedFunctionGraphicsDeviceD3D9::_draw_vertex_lighting(const D3D9Geometry *p_geometry, const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const uint16_t *p_indices, uint32_t p_index_count, const CameraMatrix &p_projection, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state) {
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
	const bool occlusion = p_lighting.vertex_ambient_occlusion;
	const Color ambient = p_color * p_lighting.ambient;
	uint32_t ambient_state = 0;
	if (occlusion && !pack_vertex_ambient_color(ambient, ambient_state)) {
		return ERR_UNAVAILABLE;
	}
	D3DLIGHT9 lights[MAX_PIXEL_LIGHTS] = {};
	for (uint32_t i = 0; i < p_lighting.light_count; i++) {
		const FFPixelLight &source = p_lighting.lights[i];
		const float *registers[] = { source.color_type, source.position_range, source.direction_attenuation, source.cone };
		for (int reg = 0; reg < 4; reg++) {
			for (int component = 0; component < 4; component++) {
				if (!(Math::abs(registers[reg][component]) <= 1.0e10f)) {
					return ERR_UNAVAILABLE;
				}
			}
		}
		D3DLIGHT9 &light = lights[i];
		light.Diffuse.r = source.color_type[0];
		light.Diffuse.g = source.color_type[1];
		light.Diffuse.b = source.color_type[2];
		light.Diffuse.a = 1;
		const float *direction = source.direction_attenuation;
		if (source.color_type[3] == 0) {
			light.Type = D3DLIGHT_DIRECTIONAL;
			// The renderer stores the direction towards a directional light.
			light.Direction.x = -direction[0];
			light.Direction.y = -direction[1];
			light.Direction.z = -direction[2];
			continue;
		}
		const float reciprocal_range = source.position_range[3];
		if (!(reciprocal_range > 1.0e-10f)) {
			return ERR_UNAVAILABLE;
		}
		light.Type = source.color_type[3] == 2 ? D3DLIGHT_SPOT : D3DLIGHT_POINT;
		light.Position.x = source.position_range[0];
		light.Position.y = source.position_range[1];
		light.Position.z = source.position_range[2];
		light.Range = 1.0f / reciprocal_range;
		light.Attenuation0 = 1;
		light.Attenuation2 = get_vertex_light_quadratic_attenuation(direction[3]) * reciprocal_range * reciprocal_range;
		if (light.Type == D3DLIGHT_SPOT) {
			light.Direction.x = direction[0];
			light.Direction.y = direction[1];
			light.Direction.z = direction[2];
			// A zero inner cone gives ((cos - cos_outer) / (1 - cos_outer))^falloff.
			light.Theta = 0;
			light.Phi = 2.0f * Math::acos(CLAMP(source.cone[0], 0.0f, 1.0f));
			light.Falloff = get_vertex_light_spot_falloff(source.cone[1]);
			if (!(light.Phi > 0)) {
				return ERR_UNAVAILABLE;
			}
		}
	}
	Rect2i viewport;
	Error error;
	if (p_geometry) {
		const D3D9GeometryBatch *first = p_geometry->batches[0];
		const PoolVector<FixedFunctionLightingVertex>::Read vertices = first->source.lighting_vertices.read();
		error = _prepare_draw(vertices.ptr(), first->source.indices.size(), p_texture, p_state, viewport);
	} else {
		error = _prepare_lighting_draw(p_vertices, p_vertex_count, p_indices, p_index_count, p_texture, p_state, viewport);
	}
	if (error != OK || !viewport.size.x || !viewport.size.y) {
		return error;
	}
	error = _set_table_fog(p_state.fog, p_state.blend_mode);
	if (error != OK) {
		return error;
	}
	float matrix[16];
	store_camera(_get_draw_projection(p_projection, viewport), matrix);
	for (int i = 0; i < 16; i++) {
		if (Math::is_nan(matrix[i]) || Math::is_inf(matrix[i])) {
			return ERR_UNAVAILABLE;
		}
	}
	D3DMATRIX projection;
	memcpy(&projection, matrix, sizeof(projection));
	// Settle every optional upload before changing lighting state or submitting.
	HRESULT result = D3D_OK;
	const UINT stride = occlusion ? sizeof(FixedFunctionLitColorVertex) : sizeof(FixedFunctionLitVertex);
	if (!p_geometry) {
		if (p_vertex_count > INT32_MAX / stride || _vertex_lit_data.resize(p_vertex_count * stride) != OK) {
			return ERR_UNAVAILABLE;
		}
		FixedFunctionGeometryData::pack_fixed_lit_vertices(p_vertices, p_vertex_count, _vertex_lit_data.ptrw(), occlusion);
	}
	for (int i = 0; p_geometry && SUCCEEDED(result) && i < p_geometry->batches.size(); i++) {
		result = _upload_vertex_lit_batch(p_geometry->batches[i], occlusion);
		if (SUCCEEDED(result) && !p_geometry->batches[i]->lit_vertices) {
			return ERR_UNAVAILABLE;
		}
	}
	if (FAILED(result)) {
		return _check_result(result);
	}
	D3DMATERIAL9 material = {};
	material.Diffuse.r = p_color.r;
	material.Diffuse.g = p_color.g;
	material.Diffuse.b = p_color.b;
	material.Diffuse.a = p_color.a;
	if (!occlusion) {
		// Ordinary native draws keep the unclamped float ambient term.
		material.Emissive.r = ambient.r;
		material.Emissive.g = ambient.g;
		material.Emissive.b = ambient.b;
	}
	const DWORD fvf = D3D9_VERTEX_LIT_FVF | (occlusion ? D3DFVF_DIFFUSE : 0);
	result = _device->SetVertexShader(nullptr);
	if (SUCCEEDED(result)) {
		result = _device->SetPixelShader(nullptr);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetFVF(fvf);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetTransform(D3DTS_PROJECTION, &projection);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetMaterial(&material);
	}
	for (uint32_t i = 0; SUCCEEDED(result) && i < p_lighting.light_count; i++) {
		result = _device->SetLight(i, &lights[i]);
		if (SUCCEEDED(result)) {
			result = _device->LightEnable(i, TRUE);
			if (SUCCEEDED(result)) {
				_enabled_vertex_lights = MAX(_enabled_vertex_lights, i + 1);
			}
		}
	}
	while (SUCCEEDED(result) && _enabled_vertex_lights > p_lighting.light_count) {
		result = _device->LightEnable(_enabled_vertex_lights - 1, FALSE);
		if (SUCCEEDED(result)) {
			_enabled_vertex_lights--;
		}
	}
	const bool transform_uv = p_uv_scale != Vector2(1, 1) || p_uv_offset != Vector2();
	bool uv_transform_enabled = false;
	if (SUCCEEDED(result) && transform_uv) {
		// Two-component coordinates take their translation from the third row.
		D3DMATRIX texture = {};
		texture._11 = float(p_uv_scale.x);
		texture._22 = float(p_uv_scale.y);
		texture._31 = float(p_uv_offset.x);
		texture._32 = float(p_uv_offset.y);
		texture._33 = texture._44 = 1;
		result = _device->SetTransform(D3DTS_TEXTURE0, &texture);
		if (SUCCEEDED(result)) {
			result = _device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
			uv_transform_enabled = SUCCEEDED(result);
		}
	}
	if (SUCCEEDED(result) && occlusion) {
		result = _device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
		if (SUCCEEDED(result)) {
			result = _device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
		}
	}
	const struct {
		D3DRENDERSTATETYPE state;
		DWORD value;
	} states[] = {
		{ D3DRS_AMBIENT, ambient_state },
		{ D3DRS_SPECULARENABLE, FALSE },
		{ D3DRS_COLORVERTEX, DWORD(occlusion) },
		// The input already supplies unit normals in the lighting space.
		{ D3DRS_NORMALIZENORMALS, FALSE },
		{ D3DRS_LIGHTING, TRUE },
	};
	for (size_t i = 0; SUCCEEDED(result) && i < sizeof(states) / sizeof(states[0]); i++) {
		result = _device->SetRenderState(states[i].state, states[i].value);
	}
	const bool lighting_enabled = SUCCEEDED(result);
	bool submitted = false;
	if (SUCCEEDED(result) && !p_geometry) {
		submitted = true;
		D3D9VertexStream &stream = occlusion ? _vertex_lit_ao_stream : _vertex_lit_stream;
		result = _draw_vertex_stream(_vertex_lit_data.ptr(), p_vertex_count, stride, stream, p_indices, p_index_count, fvf);
	}
	for (int i = 0; p_geometry && SUCCEEDED(result) && i < p_geometry->batches.size(); i++) {
		D3D9GeometryBatch *batch = p_geometry->batches[i];
		result = _device->SetStreamSource(0, batch->lit_vertices, 0, stride);
		if (SUCCEEDED(result)) {
			result = _device->SetIndices(batch->indices);
		}
		if (SUCCEEDED(result)) {
			submitted = true;
			result = _device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, batch->source.lighting_vertices.size(), 0, batch->source.indices.size() / 3);
		}
	}
	// Later draws assume the ordinary unlit, untransformed-UV pipeline.
	const HRESULT lighting_result = lighting_enabled ? _device->SetRenderState(D3DRS_LIGHTING, FALSE) : D3D_OK;
	const HRESULT texture_result = uv_transform_enabled ? _device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE) : D3D_OK;
	error = _check_result(result);
	if (error == ERR_SKIP) {
		return error;
	}
	if (FAILED(lighting_result) || FAILED(texture_result)) {
		const Error restore_error = _check_result(FAILED(lighting_result) ? lighting_result : texture_result);
		return restore_error == ERR_UNAVAILABLE ? FAILED : restore_error;
	}
	if (!submitted && (result == D3DERR_INVALIDCALL || error == ERR_OUT_OF_MEMORY)) {
		return ERR_UNAVAILABLE;
	}
	// Only a rejection before the first submission may request the CPU fallback.
	return error == ERR_UNAVAILABLE && submitted ? FAILED : error;
}
