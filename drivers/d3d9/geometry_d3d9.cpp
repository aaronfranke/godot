#include "core/math/math_funcs.h"
#include "drivers/d3d9/graphics_device_d3d9.h"

#include <string.h>

FixedFunctionGraphicsDeviceD3D9::D3D9GeometryBatch::D3D9GeometryBatch() :
		vertices(nullptr), indices(nullptr), unavailable(false) {}

void FixedFunctionGraphicsDeviceD3D9::_release_geometry_batch(D3D9GeometryBatch *p_batch) {
	if (p_batch->indices) {
		p_batch->indices->Release();
		p_batch->indices = nullptr;
	}
	if (p_batch->lit_vertices) {
		p_batch->lit_vertices->Release();
		p_batch->lit_vertices = nullptr;
	}
	if (p_batch->vertices) {
		p_batch->vertices->Release();
		p_batch->vertices = nullptr;
	}
}

void FixedFunctionGraphicsDeviceD3D9::_release_geometry_buffers() {
	if (_device) {
		_device->SetStreamSource(0, nullptr, 0, 0);
		_device->SetIndices(nullptr);
	}
	for (List<D3D9Geometry *>::Element *E = _geometries.front(); E; E = E->next()) {
		D3D9Geometry *geometry = E->get();
		for (int i = 0; i < geometry->batches.size(); i++) {
			_release_geometry_batch(geometry->batches[i]);
			geometry->batches[i]->unavailable = false;
			geometry->batches[i]->lit_unavailable = false;
		}
	}
}

void FixedFunctionGraphicsDeviceD3D9::_free_geometry(D3D9Geometry *p_geometry) {
	_geometries.erase(p_geometry);
	_geometry_owner.free(p_geometry->rid);
	for (int i = 0; i < p_geometry->batches.size(); i++) {
		_release_geometry_batch(p_geometry->batches[i]);
		memdelete(p_geometry->batches[i]);
	}
	memdelete(p_geometry);
}

Error FixedFunctionGraphicsDeviceD3D9::geometry_create(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) {
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
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	error = FixedFunctionGeometryData::partition(p_vertices, p_indices, _geometry_vertex_limit, _capabilities.max_primitives_per_draw, batches);
	if (error != OK) {
		return error;
	}
	return _create_geometry(batches, r_geometry);
}

Error FixedFunctionGraphicsDeviceD3D9::_create_geometry(const Vector<FixedFunctionGeometryData::FFGeometryBatch> &p_batches, RID &r_geometry) {
	D3D9Geometry *geometry = memnew(D3D9Geometry);
	for (int i = 0; i < p_batches.size(); i++) {
		D3D9GeometryBatch *batch = memnew(D3D9GeometryBatch);
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
	for (int i = 0; i < p_batches.size(); i++) {
		const FixedFunctionLightingVertex *vertices = p_batches[i].lighting_vertices.ptr();
		const FixedFunctionNormalMapVertex *normal = p_batches[i].normal_vertices.ptr();
		for (int v = 0; v < (p_batches[i].normal_vertices.size() ? p_batches[i].normal_vertices.size() : p_batches[i].lighting_vertices.size()); v++) {
			const FixedFunctionLightingVertex &vertex = p_batches[i].normal_vertices.size() ? normal[v].lighting : vertices[v];
			geometry->extent.x = MAX(geometry->extent.x, real_t(Math::abs(vertex.x)));
			geometry->extent.y = MAX(geometry->extent.y, real_t(Math::abs(vertex.y)));
			geometry->extent.z = MAX(geometry->extent.z, real_t(Math::abs(vertex.z)));
		}
	}
	geometry->rid = _geometry_owner.make_rid(geometry);
	_geometries.push_back(geometry);
	r_geometry = geometry->rid;
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::geometry_create_lod_batch(const FixedFunctionGeometryData::FFGeometryBatch &p_batch, RID &r_geometry) {
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
	error = FixedFunctionGeometryData::validate_lod_batch(p_batch, _geometry_vertex_limit, _capabilities.max_primitives_per_draw);
	if (error != OK) {
		return error;
	}
	if (!p_batch.normal_vertices.is_empty()) {
		error = _create_normal_map_shaders();
		if (error == OK) {
			error = _validate_normal_vertices(p_batch.normal_vertices);
		}
	} else if (!p_batch.lighting_vertices.is_empty()) {
		error = (supports_lit_geometry() || _vertex_light_limit) ? _validate_lit_vertices(p_batch.lighting_vertices) : ERR_UNAVAILABLE;
	}
	if (error != OK) {
		return error;
	}
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	if (batches.push_back(p_batch)) {
		return ERR_OUT_OF_MEMORY;
	}
	return _create_geometry(batches, r_geometry);
}

Error FixedFunctionGraphicsDeviceD3D9::geometry_free(RID p_geometry) {
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
	// Also release the device's binding references before the owning references.
	_device->SetStreamSource(0, nullptr, 0, 0);
	_device->SetIndices(nullptr);
	_free_geometry(_geometry_owner.get_or_null(p_geometry));
	return OK;
}

uint64_t FixedFunctionGraphicsDeviceD3D9::get_geometry_memory_usage() const {
	uint64_t bytes = 0;
	for (const List<D3D9Geometry *>::Element *E = _geometries.front(); E; E = E->next()) {
		const D3D9Geometry *geometry = E->get();
		for (int i = 0; i < geometry->batches.size(); i++) {
			const D3D9GeometryBatch *batch = geometry->batches[i];
			if (batch->vertices) {
				bytes += MIN(uint64_t(batch->source.get_vertex_count()) * batch->source.get_vertex_stride(), UINT64_MAX - bytes);
			}
			if (batch->lit_vertices) {
				const size_t stride = batch->lit_ambient_occlusion ? sizeof(FixedFunctionLitColorVertex) : sizeof(FixedFunctionLitVertex);
				bytes += MIN(uint64_t(batch->source.lighting_vertices.size()) * stride, UINT64_MAX - bytes);
			}
			if (batch->indices) {
				bytes += MIN(uint64_t(batch->source.indices.size()) * sizeof(uint16_t), UINT64_MAX - bytes);
			}
		}
	}
	return bytes;
}

HRESULT FixedFunctionGraphicsDeviceD3D9::_upload_geometry_batch(D3D9GeometryBatch *p_batch) {
	if (p_batch->vertices || p_batch->unavailable) {
		return D3D_OK;
	}
	DWORD usage = D3DUSAGE_WRITEONLY;
	if (_device->GetSoftwareVertexProcessing()) {
		usage |= D3DUSAGE_SOFTWAREPROCESSING;
	}
	const uint32_t vertex_bytes = p_batch->source.get_vertex_count() * p_batch->source.get_vertex_stride();
	const uint32_t index_bytes = p_batch->source.indices.size() * sizeof(uint16_t);
	const bool create_indices = !p_batch->indices;
	HRESULT result = _device->CreateVertexBuffer(vertex_bytes, usage, 0, D3DPOOL_DEFAULT, &p_batch->vertices, nullptr);
	if (SUCCEEDED(result) && create_indices) {
		result = _device->CreateIndexBuffer(index_bytes, usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &p_batch->indices, nullptr);
	}
	void *data = nullptr;
	if (SUCCEEDED(result)) {
		result = p_batch->vertices->Lock(0, vertex_bytes, &data, D3DLOCK_NOSYSLOCK);
		if (SUCCEEDED(result)) {
			if (data) {
				const FixedFunctionVertex *source = p_batch->source.vertices.ptr();
				const FixedFunctionNormalMapVertex *normal = p_batch->source.normal_vertices.ptr();
				const FixedFunctionLightingVertex *lighting = p_batch->source.lighting_vertices.ptr();
				const void *source_data = source;
				if (p_batch->source.normal_vertices.size()) {
					source_data = normal;
				} else if (p_batch->source.lighting_vertices.size()) {
					source_data = lighting;
				}
				memcpy(data, source_data, vertex_bytes);
			}
			result = p_batch->vertices->Unlock();
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
				const uint16_t *source = p_batch->source.indices.ptr();
				memcpy(data, source, index_bytes);
			}
			result = p_batch->indices->Unlock();
			if (!data && SUCCEEDED(result)) {
				result = E_FAIL;
			}
		}
	}
	if (FAILED(result)) {
		if (p_batch->vertices) {
			p_batch->vertices->Release();
			p_batch->vertices = nullptr;
		}
		if (create_indices && p_batch->indices) {
			p_batch->indices->Release();
			p_batch->indices = nullptr;
		}
		if (result == D3DERR_NOTAVAILABLE || result == D3DERR_INVALIDCALL || result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY) {
			// Rejection precedes submission. Keep the CPU copy drawable until reset.
			p_batch->unavailable = true;
			return D3D_OK;
		}
	}
	return result;
}

Error FixedFunctionGraphicsDeviceD3D9::draw_geometry(RID p_geometry, const Projection &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state, const Color *p_color_override, uint32_t p_lod) {
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
	D3D9Geometry *geometry = _geometry_owner.get_or_null(p_geometry);
	const D3D9GeometryBatch *first = geometry->batches[0];
	if (first->source.lighting_vertices.size() || first->source.normal_vertices.size()) {
		return ERR_INVALID_PARAMETER;
	}
	const FixedFunctionVertex *first_vertices = first->source.vertices.ptr();
	const uint32_t index_count = first->source.get_index_range(p_lod).count;
	if (!index_count) {
		return ERR_UNAVAILABLE;
	}
	Rect2i viewport;
	error = _prepare_draw(first_vertices, index_count, p_texture, p_state, viewport, p_color_override != nullptr);
	if (error != OK || !viewport.size.x || !viewport.size.y) {
		return error;
	}
	error = _set_table_fog(p_state.fog, p_state.blend_mode);
	if (error != OK) {
		return error;
	}
	float matrix[16];
	store_camera(_get_draw_projection(p_clip_from_model, viewport), matrix);
	for (int i = 0; i < 16; i++) {
		if (Math::is_nan(matrix[i]) || Math::is_inf(matrix[i])) {
			return ERR_UNAVAILABLE;
		}
	}
	// Set every ordinary pipeline binding: a prior draw may use the SM3 path.
	HRESULT result = _device->SetVertexShader(nullptr);
	if (SUCCEEDED(result)) {
		result = _device->SetPixelShader(nullptr);
	}
	if (SUCCEEDED(result)) {
		result = _device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
	}
	D3DMATRIX projection;
	memcpy(&projection, matrix, sizeof(projection));
	if (SUCCEEDED(result)) {
		result = _device->SetTransform(D3DTS_PROJECTION, &projection);
	}
	if (p_color_override && SUCCEEDED(result)) {
		result = _device->SetRenderState(D3DRS_TEXTUREFACTOR, color);
		if (SUCCEEDED(result) && _color_source != COLOR_SOURCE_CONSTANT) {
			// Track partial state changes too, so an error cannot leave stale diffuse state.
			_color_source = COLOR_SOURCE_DIRTY;
			result = _device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
			if (SUCCEEDED(result)) {
				result = _device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
			}
			if (SUCCEEDED(result)) {
				_color_source = COLOR_SOURCE_CONSTANT;
			}
		}
	}
	// Settle all optional uploads before submitting the first batch.
	for (int i = 0; SUCCEEDED(result) && i < geometry->batches.size(); i++) {
		result = _upload_geometry_batch(geometry->batches[i]);
	}
	if (FAILED(result)) {
		// State/setup rejection precedes every draw and may use the CPU fallback.
		return _check_result(result);
	}
	result = _draw_geometry_batches(geometry, p_lod);
	error = _check_result(result);
	// A failure can follow an earlier submitted batch; never request an outer retry.
	return error == ERR_UNAVAILABLE ? FAILED : error;
}

Error FixedFunctionGraphicsDeviceD3D9::geometry_get_draw_info(RID p_geometry, FFGeometryDrawInfo &r_info) const {
	if (!_geometry_owner.owns(p_geometry)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D9Geometry *geometry = _geometry_owner.get_or_null(p_geometry);
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

Error FixedFunctionGraphicsDeviceD3D9::geometry_create_lit(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) {
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
	if (!supports_lit_geometry() && !_vertex_light_limit) {
		return ERR_UNAVAILABLE;
	}
	error = _validate_lit_vertices(p_vertices);
	if (error != OK) {
		return error;
	}
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	error = FixedFunctionGeometryData::partition(p_vertices, p_indices, _geometry_vertex_limit, _capabilities.max_primitives_per_draw, batches);
	if (error != OK) {
		return error;
	}
	return _create_geometry(batches, r_geometry);
}

Error FixedFunctionGraphicsDeviceD3D9::_validate_lit_vertices(const Vector<FixedFunctionLightingVertex> &p_vertices) {
	for (const FixedFunctionLightingVertex &v : p_vertices) {
		const float values[] = { v.x, v.y, v.z, v.nx, v.ny, v.nz, v.u, v.v, v.r, v.g, v.b, v.a };
		for (int component = 0; component < 12; component++) {
			if (Math::is_nan(values[component]) || Math::is_inf(values[component]) || Math::abs(values[component]) > 1.0e10f) {
				return ERR_UNAVAILABLE;
			}
		}
	}
	return OK;
}

HRESULT FixedFunctionGraphicsDeviceD3D9::_draw_geometry_batches(D3D9Geometry *p_geometry, uint32_t p_lod) {
	HRESULT result = D3D_OK;
	for (int i = 0; SUCCEEDED(result) && i < p_geometry->batches.size(); i++) {
		D3D9GeometryBatch *batch = p_geometry->batches[i];
		const FixedFunctionGeometryData::FFIndexRange range = batch->source.get_index_range(p_lod);
		const uint32_t count = range.count / 3;
		if (batch->vertices) {
			result = _device->SetStreamSource(0, batch->vertices, 0, batch->source.get_vertex_stride());
			if (SUCCEEDED(result)) {
				result = _device->SetIndices(batch->indices);
			}
			if (SUCCEEDED(result)) {
				result = _device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, batch->source.get_vertex_count(), range.first, count);
			}
		} else {
			const FixedFunctionVertex *vertices = batch->source.vertices.ptr();
			const FixedFunctionLightingVertex *lighting = batch->source.lighting_vertices.ptr();
			const FixedFunctionNormalMapVertex *normal = batch->source.normal_vertices.ptr();
			const void *data = vertices;
			if (batch->source.normal_vertices.size()) {
				data = normal;
			} else if (batch->source.lighting_vertices.size()) {
				data = lighting;
			}
			const uint16_t *indices = batch->source.indices.ptr() + range.first;
			result = _device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, batch->source.get_vertex_count(), count, indices, D3DFMT_INDEX16, data, batch->source.get_vertex_stride());
		}
	}
	return result;
}
