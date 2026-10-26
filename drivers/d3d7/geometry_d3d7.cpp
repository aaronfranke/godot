#include "core/math/math_funcs.h"
#include "drivers/d3d7/graphics_device_d3d7.h"

#include <string.h>

FixedFunctionGraphicsDeviceD3D7::D3D7GeometryBatch::D3D7GeometryBatch() :
		vertices(nullptr), unavailable(false) {}

void FixedFunctionGraphicsDeviceD3D7::_release_geometry_batch(D3D7GeometryBatch *p_batch) {
	if (p_batch->vertices) {
		p_batch->vertices->Release();
		p_batch->vertices = nullptr;
	}
}

void FixedFunctionGraphicsDeviceD3D7::_release_geometry_buffers() {
	for (List<D3D7Geometry *>::Element *E = _geometries.front(); E; E = E->next()) {
		D3D7Geometry *geometry = E->get();
		for (int i = 0; i < geometry->batches.size(); i++) {
			_release_geometry_batch(geometry->batches[i]);
			geometry->batches[i]->unavailable = false;
		}
	}
}

void FixedFunctionGraphicsDeviceD3D7::_free_geometry(D3D7Geometry *p_geometry) {
	_geometries.erase(p_geometry);
	_geometry_owner.free(p_geometry->rid);
	for (int i = 0; i < p_geometry->batches.size(); i++) {
		_release_geometry_batch(p_geometry->batches[i]);
		memdelete(p_geometry->batches[i]);
	}
	memdelete(p_geometry);
}

Error FixedFunctionGraphicsDeviceD3D7::geometry_create(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) {
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
	if (!_capabilities.max_primitives_per_draw) {
		return ERR_UNAVAILABLE;
	}
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	error = FixedFunctionGeometryData::partition(p_vertices, p_indices, FixedFunctionGeometryData::MAX_BATCH_VERTICES, _capabilities.max_primitives_per_draw, batches);
	if (error != OK) {
		return error;
	}
	return _create_geometry(batches, r_geometry);
}

Error FixedFunctionGraphicsDeviceD3D7::geometry_create_lod_batch(const FixedFunctionGeometryData::FFGeometryBatch &p_batch, RID &r_geometry) {
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
	if (p_batch.vertices.is_empty() && (p_batch.lighting_vertices.is_empty() || !_vertex_light_limit)) {
		return ERR_UNAVAILABLE;
	}
	error = FixedFunctionGeometryData::validate_lod_batch(p_batch, FixedFunctionGeometryData::MAX_BATCH_VERTICES, _capabilities.max_primitives_per_draw);
	if (error != OK) {
		return error;
	}
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	if (batches.push_back(p_batch)) {
		return ERR_OUT_OF_MEMORY;
	}
	return _create_geometry(batches, r_geometry);
}

Error FixedFunctionGraphicsDeviceD3D7::_create_geometry(const Vector<FixedFunctionGeometryData::FFGeometryBatch> &p_batches, RID &r_geometry) {
	D3D7Geometry *geometry = memnew(D3D7Geometry);
	for (int i = 0; i < p_batches.size(); i++) {
		D3D7GeometryBatch *batch = memnew(D3D7GeometryBatch);
		batch->source = p_batches[i];
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

Error FixedFunctionGraphicsDeviceD3D7::geometry_free(RID p_geometry) {
	const Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active || _resetting) {
		return ERR_BUSY;
	}
	if (!_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	_free_geometry(_geometry_owner.get_or_null(p_geometry));
	return OK;
}

uint64_t FixedFunctionGraphicsDeviceD3D7::get_geometry_memory_usage() const {
	uint64_t bytes = 0;
	for (const List<D3D7Geometry *>::Element *E = _geometries.front(); E; E = E->next()) {
		const D3D7Geometry *geometry = E->get();
		for (int i = 0; i < geometry->batches.size(); i++) {
			const D3D7GeometryBatch *batch = geometry->batches[i];
			if (batch->vertices) {
				const bool lit = !batch->source.lighting_vertices.is_empty();
				size_t stride = sizeof(FixedFunctionVertex);
				if (lit) {
					stride = batch->lit_ambient_occlusion ? sizeof(FixedFunctionLitColorVertex) : sizeof(FixedFunctionLitVertex);
				}
				const uint64_t size = uint64_t(batch->source.get_vertex_count()) * stride;
				bytes += MIN(size, UINT64_MAX - bytes);
			}
		}
	}
	return bytes;
}

HRESULT FixedFunctionGraphicsDeviceD3D7::_upload_geometry_batch(D3D7GeometryBatch *p_batch) {
	if (p_batch->vertices || p_batch->unavailable) {
		return D3D_OK;
	}
	D3DVERTEXBUFFERDESC description = {};
	description.dwSize = sizeof(description);
	description.dwCaps = D3DVBCAPS_WRITEONLY;
	if (!_capabilities.hardware_transform_and_light) {
		description.dwCaps |= D3DVBCAPS_SYSTEMMEMORY;
	}
	description.dwFVF = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
	description.dwNumVertices = p_batch->source.vertices.size();
	HRESULT result = _d3d->CreateVertexBuffer(&description, &p_batch->vertices, 0);
	void *data = nullptr;
	DWORD size = 0;
	if (SUCCEEDED(result)) {
		result = p_batch->vertices->Lock(DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK, &data, &size);
		if (SUCCEEDED(result)) {
			const uint32_t bytes = p_batch->source.vertices.size() * sizeof(FixedFunctionVertex);
			const bool valid = data && size >= bytes;
			if (valid) {
				const FixedFunctionVertex *source = p_batch->source.vertices.ptr();
				memcpy(data, source, bytes);
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
			// Rejection precedes submission. Keep the CPU copy drawable until reset.
			p_batch->unavailable = true;
			return D3D_OK;
		}
	}
	return result;
}

Error FixedFunctionGraphicsDeviceD3D7::draw_geometry(RID p_geometry, const Projection &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state, const Color *p_color_override, uint32_t p_lod) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	uint32_t color = 0;
	if (p_color_override) {
		error = pack_geometry_color(*p_color_override, color);
		if (error != OK) {
			return error;
		}
	}
	D3D7Geometry *geometry = _geometry_owner.get_or_null(p_geometry);
	const D3D7GeometryBatch *first = geometry->batches[0];
	if (first->source.vertices.is_empty()) {
		return ERR_UNAVAILABLE;
	}
	const FixedFunctionVertex *first_vertices = first->source.vertices.ptr();
	const uint32_t index_count = first->source.get_index_range(p_lod).count;
	if (!index_count) {
		return ERR_UNAVAILABLE;
	}
	Rect2i viewport;
	// The RGB rasterizer ignores the texture factor when no texture is bound.
	const bool emissive_color = _rgb_device && p_color_override && !p_texture.is_valid();
	error = _prepare_draw(first_vertices, index_count, p_texture, p_state, viewport, p_color_override != nullptr && !emissive_color);
	if (error != OK || !viewport.size.x || !viewport.size.y) {
		return error;
	}
	float matrix[16];
	store_camera(_get_draw_projection(p_clip_from_model, viewport), matrix);
	for (int i = 0; i < 16; i++) {
		if (Math::is_nan(matrix[i]) || Math::is_inf(matrix[i])) {
			return ERR_UNAVAILABLE;
		}
	}
	D3DMATRIX projection;
	memcpy(&projection, matrix, sizeof(projection));
	HRESULT result = _device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &projection);
	// Settle all optional uploads before changing lighting state or submitting.
	for (int i = 0; SUCCEEDED(result) && i < geometry->batches.size(); i++) {
		result = _upload_geometry_batch(geometry->batches[i]);
	}
	if (emissive_color && SUCCEEDED(result)) {
		D3DMATERIAL7 material = {};
		// Zero diffuse RGB keeps any enabled lights from contributing.
		material.dcvEmissive.r = ((color >> 16) & 0xff) / 255.0f;
		material.dcvEmissive.g = ((color >> 8) & 0xff) / 255.0f;
		material.dcvEmissive.b = (color & 0xff) / 255.0f;
		material.dcvDiffuse.a = (color >> 24) / 255.0f;
		result = _device->SetMaterial(&material);
		const D3DRENDERSTATETYPE states[] = { D3DRENDERSTATE_AMBIENT, D3DRENDERSTATE_SPECULARENABLE, D3DRENDERSTATE_COLORVERTEX, D3DRENDERSTATE_LIGHTING };
		const DWORD values[] = { 0, FALSE, FALSE, TRUE };
		for (int i = 0; SUCCEEDED(result) && i < 4; i++) {
			result = _device->SetRenderState(states[i], values[i]);
		}
	} else if (p_color_override && SUCCEEDED(result)) {
		result = _device->SetRenderState(D3DRENDERSTATE_TEXTUREFACTOR, color);
		if (SUCCEEDED(result) && _color_source != COLOR_SOURCE_CONSTANT) {
			// Track partial state changes too, so an error cannot leave stale diffuse state.
			_color_source = COLOR_SOURCE_DIRTY;
			result = _device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
			if (SUCCEEDED(result)) {
				result = _device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
			}
			if (SUCCEEDED(result)) {
				_color_source = COLOR_SOURCE_CONSTANT;
			}
		}
	}
	if (FAILED(result)) {
		// State/setup rejection precedes every draw and may use the CPU fallback.
		return _check_result(result);
	}
	for (int i = 0; SUCCEEDED(result) && i < geometry->batches.size(); i++) {
		D3D7GeometryBatch *batch = geometry->batches[i];
		const FixedFunctionGeometryData::FFIndexRange range = batch->source.get_index_range(p_lod);
		const uint16_t *indices = batch->source.indices.ptr() + range.first;
		// D3D7 has no separate index-buffer resource. Its historical draw methods
		// take mutable pointers but do not change the retained CPU arrays.
		WORD *native_indices = const_cast<WORD *>(indices);
		if (batch->vertices) {
			result = _device->DrawIndexedPrimitiveVB(D3DPT_TRIANGLELIST, batch->vertices, 0, batch->source.vertices.size(), native_indices, range.count, 0);
		} else {
			const FixedFunctionVertex *vertices = batch->source.vertices.ptr();
			result = _device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, const_cast<FixedFunctionVertex *>(vertices), batch->source.vertices.size(), native_indices, range.count, 0);
		}
	}
	error = _check_result(result);
	if (emissive_color) {
		const HRESULT lighting_result = _device->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
		if (FAILED(lighting_result)) {
			// The next draw must start with lighting off, even after a failed restoration.
			_reset_pending = true;
			if (error != ERR_SKIP) {
				error = _check_result(lighting_result);
			}
		}
	}
	// A failure can follow an earlier submitted batch; never request an outer retry.
	return error == ERR_UNAVAILABLE ? FAILED : error;
}

Error FixedFunctionGraphicsDeviceD3D7::draw_indexed_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count, const Projection &p_projection, RID p_texture, const FFDrawParameters &p_state) {
	Error error = validate_triangle_indices(p_vertex_count, p_indices, p_index_count);
	if (error != OK || p_vertex_count > D3DMAXNUMVERTICES) {
		return error != OK ? error : ERR_INVALID_PARAMETER;
	}
	Rect2i viewport;
	error = _prepare_draw(p_vertices, p_index_count, p_texture, p_state, viewport);
	if (error != OK || !p_index_count || !viewport.size.x || !viewport.size.y) {
		return error;
	}
	float matrix[16];
	store_camera(_get_draw_projection(p_projection, viewport), matrix);
	D3DMATRIX projection;
	memcpy(&projection, matrix, sizeof(projection));
	error = _check_result(_device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &projection));
	if (error != OK) {
		return error;
	}
	// Direct3D 7 takes mutable pointers to its read-only input arrays.
	WORD *native_indices = reinterpret_cast<WORD *>(const_cast<uint8_t *>(p_indices));
	error = _check_result(_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, const_cast<FixedFunctionVertex *>(p_vertices), p_vertex_count, native_indices, p_index_count, 0));
	// Only a refusal before submission may fall back to expanded triangles.
	return error == ERR_UNAVAILABLE ? FAILED : error;
}

Error FixedFunctionGraphicsDeviceD3D7::geometry_get_draw_info(RID p_geometry, FFGeometryDrawInfo &r_info) const {
	if (!_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D7Geometry *geometry = _geometry_owner.get_or_null(p_geometry);
	FFGeometryDrawInfo info;
	info.draw_calls = geometry->batches.size();
	info.lod_count = geometry->batches[0]->source.lod_offsets.size();
	for (int i = 0; i < geometry->batches.size(); i++) {
		info.vertex_count += geometry->batches[i]->source.get_index_range(0).count;
	}
	info.primitive_count = info.vertex_count / 3;
	r_info = info;
	return OK;
}
