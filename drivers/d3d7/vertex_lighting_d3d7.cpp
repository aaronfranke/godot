#include "graphics_device_d3d7.h"

#include "core/math/math_funcs.h"

#include <string.h>

// Native transform and lighting. Devices without hardware
// T&L run the same state through Direct3D's own software vertex pipeline.
static const DWORD D3D7_VERTEX_LIT_FVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;

Error FixedFunctionGraphicsDeviceD3D7::geometry_create_lit(const PoolVector<FixedFunctionLightingVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, RID &r_geometry) {
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
	if (!_capabilities.max_primitives_per_draw || !_vertex_light_limit) {
		return ERR_UNAVAILABLE;
	}
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	error = FixedFunctionGeometryData::partition(p_vertices, p_indices, FixedFunctionGeometryData::MAX_BATCH_VERTICES, _capabilities.max_primitives_per_draw, batches);
	if (error != OK) {
		return error;
	}
	D3D7Geometry *geometry = memnew(D3D7Geometry);
	for (int i = 0; i < batches.size(); i++) {
		D3D7GeometryBatch *batch = memnew(D3D7GeometryBatch);
		batch->source = batches[i];
		if (geometry->batches.push_back(batch)) {
			memdelete(batch);
			for (int prior = 0; prior < geometry->batches.size(); prior++) {
				memdelete(geometry->batches[prior]);
			}
			memdelete(geometry);
			return ERR_OUT_OF_MEMORY;
		}
	}
	geometry->rid = _geometry_owner.make_rid(geometry);
	_geometries.push_back(geometry);
	r_geometry = geometry->rid;
	return OK;
}

HRESULT FixedFunctionGraphicsDeviceD3D7::_upload_vertex_lit_batch(D3D7GeometryBatch *p_batch, bool p_ambient_occlusion) {
	if (p_batch->lit_ambient_occlusion != p_ambient_occlusion) {
		_release_geometry_batch(p_batch);
		p_batch->unavailable = false;
		p_batch->lit_ambient_occlusion = p_ambient_occlusion;
	}
	if (p_batch->vertices || p_batch->unavailable) {
		return D3D_OK;
	}
	D3DVERTEXBUFFERDESC description = {};
	description.dwSize = sizeof(description);
	description.dwCaps = D3DVBCAPS_WRITEONLY;
	if (!_capabilities.hardware_transform_and_light) {
		description.dwCaps |= D3DVBCAPS_SYSTEMMEMORY;
	}
	description.dwFVF = D3D7_VERTEX_LIT_FVF | (p_ambient_occlusion ? D3DFVF_DIFFUSE : 0);
	description.dwNumVertices = p_batch->source.lighting_vertices.size();
	HRESULT result = _d3d->CreateVertexBuffer(&description, &p_batch->vertices, 0);
	void *data = nullptr;
	DWORD size = 0;
	if (SUCCEEDED(result)) {
		result = p_batch->vertices->Lock(DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK, &data, &size);
		if (SUCCEEDED(result)) {
			const uint32_t count = p_batch->source.lighting_vertices.size();
			const size_t stride = p_ambient_occlusion ? sizeof(FixedFunctionLitColorVertex) : sizeof(FixedFunctionLitVertex);
			const bool valid = data && size >= count * stride;
			if (valid) {
				FixedFunctionGeometryData::pack_fixed_lit_vertices(p_batch->source.lighting_vertices, data, p_ambient_occlusion);
			}
			result = p_batch->vertices->Unlock();
			if (!valid && SUCCEEDED(result)) {
				result = E_FAIL;
			}
		}
	}
	if (FAILED(result)) {
		_release_geometry_batch(p_batch);
		if (result == DDERR_UNSUPPORTED || result == DDERR_INVALIDCAPS || result == DDERR_INVALIDPARAMS || result == DDERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY) {
			// Rejection precedes submission; the renderer keeps its CPU lighting path.
			p_batch->unavailable = true;
			return D3D_OK;
		}
	}
	return result;
}

Error FixedFunctionGraphicsDeviceD3D7::draw_vertex_lit_geometry(RID p_geometry, const CameraMatrix &p_clip_from_model, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_vertex_light_limit || p_lighting.light_count > _vertex_light_limit) {
		return ERR_UNAVAILABLE;
	}
	if (!_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D7Geometry *geometry = _geometry_owner.getptr(p_geometry);
	const D3D7GeometryBatch *first = geometry->batches[0];
	if (!first->source.lighting_vertices.size()) {
		return ERR_INVALID_PARAMETER;
	}
	return _draw_vertex_lighting(geometry, nullptr, 0, nullptr, 0, p_clip_from_model, p_lighting, p_color, p_uv_scale, p_uv_offset, p_texture, p_state);
}

Error FixedFunctionGraphicsDeviceD3D7::draw_vertex_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_view, const FFPixelLighting &p_lighting, const Color &p_color, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices, uint32_t p_index_count) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	const uint32_t element_count = p_indices ? p_index_count : p_vertex_count;
	if (!p_vertices || !p_vertex_count || p_vertex_count > D3DMAXNUMVERTICES || (p_indices == nullptr) != (p_index_count == 0) ||
			element_count % 3 || element_count / 3 > _capabilities.max_primitives_per_draw) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_indices) {
		error = validate_triangle_indices(p_vertex_count, reinterpret_cast<const uint8_t *>(p_indices), p_index_count);
		if (error != OK) {
			return error;
		}
	}
	return _draw_vertex_lighting(nullptr, p_vertices, p_vertex_count, p_indices, p_index_count, p_clip_from_view, p_lighting, p_color, Vector2(1, 1), Vector2(), p_texture, p_state);
}

Error FixedFunctionGraphicsDeviceD3D7::_draw_vertex_lighting(const D3D7Geometry *p_geometry, const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const uint16_t *p_indices, uint32_t p_index_count, const CameraMatrix &p_projection, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state) {
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
	D3DLIGHT7 lights[MAX_PIXEL_LIGHTS] = {};
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
		D3DLIGHT7 &light = lights[i];
		light.dcvDiffuse.r = source.color_type[0];
		light.dcvDiffuse.g = source.color_type[1];
		light.dcvDiffuse.b = source.color_type[2];
		light.dcvDiffuse.a = 1;
		const float *direction = source.direction_attenuation;
		if (source.color_type[3] == 0) {
			light.dltType = D3DLIGHT_DIRECTIONAL;
			// The renderer stores the direction towards a directional light.
			light.dvDirection.x = -direction[0];
			light.dvDirection.y = -direction[1];
			light.dvDirection.z = -direction[2];
			continue;
		}
		const float reciprocal_range = source.position_range[3];
		if (!(reciprocal_range > 1.0e-10f)) {
			return ERR_UNAVAILABLE;
		}
		light.dltType = source.color_type[3] == 2 ? D3DLIGHT_SPOT : D3DLIGHT_POINT;
		light.dvPosition.x = source.position_range[0];
		light.dvPosition.y = source.position_range[1];
		light.dvPosition.z = source.position_range[2];
		light.dvRange = 1.0f / reciprocal_range;
		light.dvAttenuation0 = 1;
		light.dvAttenuation2 = get_vertex_light_quadratic_attenuation(direction[3]) * reciprocal_range * reciprocal_range;
		if (light.dltType == D3DLIGHT_SPOT) {
			light.dvDirection.x = direction[0];
			light.dvDirection.y = direction[1];
			light.dvDirection.z = direction[2];
			// A zero inner cone gives ((cos - cos_outer) / (1 - cos_outer))^falloff.
			light.dvTheta = 0;
			light.dvPhi = 2.0f * Math::acos(CLAMP(source.cone[0], 0.0f, 1.0f));
			light.dvFalloff = get_vertex_light_spot_falloff(source.cone[1]);
			if (!(light.dvPhi > 0)) {
				return ERR_UNAVAILABLE;
			}
		}
	}
	Rect2i viewport;
	Error error;
	if (p_geometry) {
		const D3D7GeometryBatch *first = p_geometry->batches[0];
		const PoolVector<FixedFunctionLightingVertex>::Read vertices = first->source.lighting_vertices.read();
		error = _prepare_draw(vertices.ptr(), first->source.indices.size(), p_texture, p_state, viewport);
	} else {
		error = _prepare_draw(p_vertices, p_indices ? p_index_count : p_vertex_count, p_texture, p_state, viewport);
	}
	if (error != OK || !viewport.size.x || !viewport.size.y) {
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
	if (!p_geometry) {
		const size_t stride = occlusion ? sizeof(FixedFunctionLitColorVertex) : sizeof(FixedFunctionLitVertex);
		const int64_t size = p_vertex_count * stride;
		if (size > _vertex_lit_data.size() && _vertex_lit_data.resize(size) != OK) {
			return ERR_UNAVAILABLE;
		}
		FixedFunctionGeometryData::pack_fixed_lit_vertices(p_vertices, p_vertex_count, _vertex_lit_data.ptrw(), occlusion);
	}
	for (int i = 0; p_geometry && SUCCEEDED(result) && i < p_geometry->batches.size(); i++) {
		result = _upload_vertex_lit_batch(p_geometry->batches[i], occlusion);
		if (SUCCEEDED(result) && !p_geometry->batches[i]->vertices) {
			return ERR_UNAVAILABLE;
		}
	}
	if (FAILED(result)) {
		return _check_result(result);
	}
	D3DMATERIAL7 material = {};
	material.dcvDiffuse.r = p_color.r;
	material.dcvDiffuse.g = p_color.g;
	material.dcvDiffuse.b = p_color.b;
	material.dcvDiffuse.a = p_color.a;
	if (!occlusion) {
		// Ordinary native draws keep the unclamped float ambient term.
		material.dcvEmissive.r = ambient.r;
		material.dcvEmissive.g = ambient.g;
		material.dcvEmissive.b = ambient.b;
	}
	result = _device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &projection);
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
		D3DMATRIX texture = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		texture._11 = float(p_uv_scale.x);
		texture._22 = float(p_uv_scale.y);
		texture._31 = float(p_uv_offset.x);
		texture._32 = float(p_uv_offset.y);
		result = _device->SetTransform(D3DTRANSFORMSTATE_TEXTURE0, &texture);
		if (SUCCEEDED(result)) {
			result = _device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
			uv_transform_enabled = SUCCEEDED(result);
		}
	}
	if (SUCCEEDED(result) && occlusion) {
		result = _device->SetRenderState(D3DRENDERSTATE_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
		if (SUCCEEDED(result)) {
			result = _device->SetRenderState(D3DRENDERSTATE_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
		}
	}
	const struct {
		D3DRENDERSTATETYPE state;
		DWORD value;
	} states[] = {
		{ D3DRENDERSTATE_AMBIENT, ambient_state },
		{ D3DRENDERSTATE_SPECULARENABLE, FALSE },
		{ D3DRENDERSTATE_COLORVERTEX, DWORD(occlusion) },
		// Model-space unit normals meet identity world and view transforms.
		{ D3DRENDERSTATE_NORMALIZENORMALS, FALSE },
		{ D3DRENDERSTATE_LIGHTING, TRUE },
	};
	for (size_t i = 0; SUCCEEDED(result) && i < sizeof(states) / sizeof(states[0]); i++) {
		result = _device->SetRenderState(states[i].state, states[i].value);
	}
	const bool lighting_enabled = SUCCEEDED(result);
	bool submitted = false;
	for (int i = 0; p_geometry && SUCCEEDED(result) && i < p_geometry->batches.size(); i++) {
		const D3D7GeometryBatch *batch = p_geometry->batches[i];
		const PoolVector<uint16_t>::Read indices = batch->source.indices.read();
		submitted = true;
		// Direct3D 7 takes a mutable pointer to its read-only index input.
		result = _device->DrawIndexedPrimitiveVB(D3DPT_TRIANGLELIST, batch->vertices, 0, batch->source.lighting_vertices.size(), const_cast<WORD *>(indices.ptr()), batch->source.indices.size(), 0);
	}
	if (!p_geometry && SUCCEEDED(result)) {
		const DWORD fvf = D3D7_VERTEX_LIT_FVF | (occlusion ? D3DFVF_DIFFUSE : 0);
		void *vertices = _vertex_lit_data.ptrw();
		submitted = true;
		if (p_indices) {
			result = _device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, fvf, vertices, p_vertex_count, const_cast<WORD *>(p_indices), p_index_count, 0);
		} else {
			result = _device->DrawPrimitive(D3DPT_TRIANGLELIST, fvf, vertices, p_vertex_count, 0);
		}
	}
	// Later draws assume the ordinary unlit, untransformed-UV pipeline.
	const HRESULT lighting_result = lighting_enabled ? _device->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE) : D3D_OK;
	const HRESULT texture_result = uv_transform_enabled ? _device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE) : D3D_OK;
	error = _check_result(result);
	if (error == ERR_SKIP) {
		return error;
	}
	if (FAILED(lighting_result) || FAILED(texture_result)) {
		const Error restore_error = _check_result(FAILED(lighting_result) ? lighting_result : texture_result);
		return restore_error == ERR_UNAVAILABLE ? FAILED : restore_error;
	}
	if (!submitted && (result == DDERR_INVALIDPARAMS || result == DDERR_INVALIDCAPS || error == ERR_OUT_OF_MEMORY)) {
		return ERR_UNAVAILABLE;
	}
	// Only a rejection before the first submission may request the CPU fallback.
	return error == ERR_UNAVAILABLE && submitted ? FAILED : error;
}
