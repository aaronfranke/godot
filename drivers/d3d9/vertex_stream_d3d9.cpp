#include "drivers/d3d9/graphics_device_d3d9.h"

#include <string.h>

FixedFunctionGraphicsDeviceD3D9::D3D9VertexStream::D3D9VertexStream() :
		native(nullptr), position(0), unavailable(false) {}

void FixedFunctionGraphicsDeviceD3D9::_release_vertex_streams() {
	if (_device) {
		// Reset requires releasing the device's reference to default-pool buffers too.
		_device->SetStreamSource(0, nullptr, 0, 0);
		_device->SetIndices(nullptr);
	}
	if (_stream_indices) {
		_stream_indices->Release();
		_stream_indices = nullptr;
	}
	_stream_index_position = 0;
	_stream_indices_unavailable = false;
	D3D9VertexStream *streams[] = { &_vertex_stream, &_lighting_vertex_stream, &_vertex_lit_stream, &_vertex_lit_ao_stream, &_masked_vertex_stream, &_normal_vertex_stream, &_occlusion_vertex_stream };
	for (size_t i = 0; i < sizeof(streams) / sizeof(streams[0]); i++) {
		if (streams[i]->native) {
			streams[i]->native->Release();
		}
		*streams[i] = D3D9VertexStream();
	}
}

Error FixedFunctionGraphicsDeviceD3D9::_prepare_lighting_draw(const void *p_vertices, uint32_t p_vertex_count, const uint16_t *p_indices, uint32_t p_index_count, RID p_texture, const FFDrawParameters &p_state, Rect2i &r_viewport) {
	if (p_indices || p_index_count) {
		const Error error = validate_triangle_indices(p_vertex_count, reinterpret_cast<const uint8_t *>(p_indices), p_index_count);
		if (error != OK) {
			return error;
		}
		if (!p_vertices || !p_index_count) {
			return ERR_INVALID_PARAMETER;
		}
		if (p_vertex_count > _geometry_vertex_limit) {
			return ERR_UNAVAILABLE;
		}
		return _prepare_draw(p_vertices, p_index_count, p_texture, p_state, r_viewport);
	}
	return _prepare_draw(p_vertices, p_vertex_count, p_texture, p_state, r_viewport);
}

HRESULT FixedFunctionGraphicsDeviceD3D9::_upload_stream_indices(const uint16_t *p_indices, uint32_t p_count, uint32_t &r_start) {
	if (!_stream_indices) {
		DWORD usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;
		if (_device->GetSoftwareVertexProcessing()) {
			usage |= D3DUSAGE_SOFTWAREPROCESSING;
		}
		const HRESULT result = _device->CreateIndexBuffer(VERTEX_STREAM_CAPACITY * sizeof(uint16_t), usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &_stream_indices, nullptr);
		if (FAILED(result)) {
			return result;
		}
	}
	if (p_count > VERTEX_STREAM_CAPACITY - _stream_index_position) {
		_stream_index_position = 0;
	}
	const uint32_t start = _stream_index_position;
	const DWORD flags = D3DLOCK_NOSYSLOCK | (start ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD);
	void *data = nullptr;
	const HRESULT lock = _stream_indices->Lock(start * sizeof(uint16_t), p_count * sizeof(uint16_t), &data, flags);
	if (FAILED(lock)) {
		return lock;
	}
	if (!data) {
		_stream_indices->Unlock();
		return E_FAIL;
	}
	memcpy(data, p_indices, p_count * sizeof(uint16_t));
	const HRESULT unlock = _stream_indices->Unlock();
	if (SUCCEEDED(unlock)) {
		r_start = start;
		_stream_index_position += p_count;
	}
	return unlock;
}

HRESULT FixedFunctionGraphicsDeviceD3D9::_draw_vertex_stream(const void *p_vertices, uint32_t p_vertex_count, uint32_t p_stride, D3D9VertexStream &p_stream, const uint16_t *p_indices, uint32_t p_index_count, DWORD p_fvf) {
	const auto draw_up = [&]() -> HRESULT {
		if (p_indices) {
			return _device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, p_vertex_count, p_index_count / 3, p_indices, D3DFMT_INDEX16, p_vertices, p_stride);
		}
		return _device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, p_vertex_count / 3, p_vertices, p_stride);
	};
	// Larger caller batches retain the immediate path; this is not a mesh-size limit.
	if (!p_stream.unavailable && p_vertex_count <= VERTEX_STREAM_CAPACITY && (!p_indices || (!_stream_indices_unavailable && p_index_count <= VERTEX_STREAM_CAPACITY))) {
		if (!p_stream.native) {
			DWORD usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;
			if (_device->GetSoftwareVertexProcessing()) {
				usage |= D3DUSAGE_SOFTWAREPROCESSING;
			}
			const HRESULT result = _device->CreateVertexBuffer(VERTEX_STREAM_CAPACITY * p_stride, usage, p_fvf, D3DPOOL_DEFAULT, &p_stream.native, nullptr);
			if (FAILED(result)) {
				if (result != D3DERR_NOTAVAILABLE && result != D3DERR_INVALIDCALL && result != D3DERR_OUTOFVIDEOMEMORY && result != E_OUTOFMEMORY) {
					return result;
				}
				// Optional allocation failure must not make otherwise drawable geometry disappear.
				p_stream.unavailable = true;
			}
		}
		if (p_stream.native) {
			uint32_t index_start = 0;
			if (p_indices) {
				const HRESULT result = _upload_stream_indices(p_indices, p_index_count, index_start);
				if (FAILED(result)) {
					if (result != D3DERR_NOTAVAILABLE && result != D3DERR_INVALIDCALL && result != D3DERR_OUTOFVIDEOMEMORY && result != E_OUTOFMEMORY) {
						return result;
					}
					_stream_indices_unavailable = true;
					return draw_up();
				}
			}
			const uint32_t capacity = p_indices ? MIN(_geometry_vertex_limit, uint32_t(VERTEX_STREAM_CAPACITY)) : uint32_t(VERTEX_STREAM_CAPACITY);
			if (p_stream.position > capacity - p_vertex_count) {
				p_stream.position = 0;
			}
			const uint32_t start = p_stream.position;
			const DWORD flags = D3DLOCK_NOSYSLOCK | (start ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD);
			void *data = nullptr;
			HRESULT result = p_stream.native->Lock(start * p_stride, p_vertex_count * p_stride, &data, flags);
			if (FAILED(result)) {
				if (result == D3DERR_NOTAVAILABLE || result == D3DERR_INVALIDCALL || result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY) {
					// Nothing has been submitted; a driver rejecting streaming can use UP safely.
					p_stream.unavailable = true;
					return draw_up();
				}
				return result;
			}
			if (!data) {
				p_stream.native->Unlock();
				return E_FAIL;
			}
			memcpy(data, p_vertices, p_vertex_count * p_stride);
			result = p_stream.native->Unlock();
			if (FAILED(result)) {
				return result;
			}
			// Append across frames; DISCARD on wrap lets the driver preserve in-flight draws.
			p_stream.position += p_vertex_count;
			// Keep the stream offset zero for devices without D3DDEVCAPS2_STREAMOFFSET.
			result = _device->SetStreamSource(0, p_stream.native, 0, p_stride);
			if (FAILED(result)) {
				return result;
			}
			// Never retry a submitted draw: errors could follow partial submission.
			if (p_indices) {
				result = _device->SetIndices(_stream_indices);
				if (FAILED(result)) {
					return result;
				}
				return _device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, start, 0, p_vertex_count, index_start, p_index_count / 3);
			}
			return _device->DrawPrimitive(D3DPT_TRIANGLELIST, start, p_vertex_count / 3);
		}
	}
	return draw_up();
}
