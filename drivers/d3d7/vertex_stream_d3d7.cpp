#include "drivers/d3d7/graphics_device_d3d7.h"

#include <string.h>

void FixedFunctionGraphicsDeviceD3D7::_release_vertex_stream() {
	if (_vertex_buffer) {
		_vertex_buffer->Release();
		_vertex_buffer = nullptr;
	}
	_vertex_buffer_position = 0;
	_vertex_buffer_unavailable = false;
}

HRESULT FixedFunctionGraphicsDeviceD3D7::_draw_vertex_stream(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count) {
	const DWORD fvf = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
	// Larger caller batches retain the immediate path; this is not a mesh-size limit.
	if (!_vertex_buffer_unavailable && p_vertex_count <= VERTEX_STREAM_CAPACITY) {
		if (!_vertex_buffer) {
			D3DVERTEXBUFFERDESC description = {};
			description.dwSize = sizeof(description);
			description.dwCaps = D3DVBCAPS_WRITEONLY;
			if (!_capabilities.hardware_transform_and_light) {
				description.dwCaps |= D3DVBCAPS_SYSTEMMEMORY;
			}
			description.dwFVF = fvf;
			description.dwNumVertices = VERTEX_STREAM_CAPACITY;
			const HRESULT result = _d3d->CreateVertexBuffer(&description, &_vertex_buffer, 0);
			if (FAILED(result)) {
				if (result != DDERR_UNSUPPORTED && result != DDERR_INVALIDCAPS && result != DDERR_INVALIDPARAMS && result != DDERR_OUTOFVIDEOMEMORY && result != E_OUTOFMEMORY) {
					return result;
				}
				// Optional allocation failure must not make otherwise drawable geometry disappear.
				_vertex_buffer_unavailable = true;
			}
		}
		if (_vertex_buffer) {
			if (p_vertex_count > VERTEX_STREAM_CAPACITY - _vertex_buffer_position) {
				_vertex_buffer_position = 0;
			}
			const uint32_t start = _vertex_buffer_position;
			const DWORD flags = DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK | (start ? DDLOCK_NOOVERWRITE : DDLOCK_DISCARDCONTENTS);
			void *data = nullptr;
			DWORD size = 0;
			HRESULT result = _vertex_buffer->Lock(flags, &data, &size);
			if (FAILED(result)) {
				if (result == DDERR_UNSUPPORTED || result == DDERR_INVALIDPARAMS || result == DDERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY) {
					// Nothing has been submitted; older lock implementations can use immediate draws.
					_vertex_buffer_unavailable = true;
					return _device->DrawPrimitive(D3DPT_TRIANGLELIST, fvf, const_cast<FixedFunctionVertex *>(p_vertices), p_vertex_count, 0);
				}
				return result;
			}
			if (!data || size < (start + p_vertex_count) * sizeof(FixedFunctionVertex)) {
				_vertex_buffer->Unlock();
				return E_FAIL;
			}
			memcpy(static_cast<FixedFunctionVertex *>(data) + start, p_vertices, p_vertex_count * sizeof(FixedFunctionVertex));
			result = _vertex_buffer->Unlock();
			if (FAILED(result)) {
				return result;
			}
			// Append across frames; discard on wrap preserves vertices still used by earlier draws.
			_vertex_buffer_position += p_vertex_count;
			// Never retry a submitted draw: errors could follow partial submission.
			return _device->DrawPrimitiveVB(D3DPT_TRIANGLELIST, _vertex_buffer, start, p_vertex_count, 0);
		}
	}
	// The historical API takes mutable data but does not modify the input vertices.
	return _device->DrawPrimitive(D3DPT_TRIANGLELIST, fvf, const_cast<FixedFunctionVertex *>(p_vertices), p_vertex_count, 0);
}
