#include "graphics_device_d3d9.h"
#include "graphics_device_d3d9_factory.h"

#include "core/math/math_funcs.h"

#include <string.h>

FixedFunctionGraphicsDevice *FixedFunctionGraphicsDeviceD3D9Factory::create() {
	return memnew(FixedFunctionGraphicsDeviceD3D9);
}

#define D3D9_FAIL_ON_ERROR(m_call)                      \
	do {                                                \
		const Error call_error = _check_result(m_call); \
		if (call_error != OK) {                         \
			return call_error;                          \
		}                                               \
	} while (false)

static_assert(sizeof(FixedFunctionVertex) == 24 && offsetof(FixedFunctionVertex, color) == 12 && offsetof(FixedFunctionVertex, u) == 16, "Unexpected vertex layout.");

static const D3DFORMAT D3D9_DXT_FORMATS[] = { D3DFMT_DXT1, D3DFMT_DXT3, D3DFMT_DXT5 };

Error FixedFunctionGraphicsDeviceD3D9::_check_thread() const {
	if (!_owner_thread) {
		return ERR_UNCONFIGURED;
	}
	return GetCurrentThreadId() == _owner_thread ? OK : ERR_BUG;
}

Error FixedFunctionGraphicsDeviceD3D9::_check_result(HRESULT p_result) {
	_last_result = p_result;
	if (SUCCEEDED(p_result)) {
		return OK;
	}
	switch (p_result) {
		case D3DERR_DEVICELOST:
		case D3DERR_DEVICENOTRESET:
			// Recovery uses the native cooperative level; this is a retryable frame condition.
			_reset_pending = true;
			return ERR_SKIP;
		case D3DERR_NOTAVAILABLE:
			return ERR_UNAVAILABLE;
		case D3DERR_OUTOFVIDEOMEMORY:
		case E_OUTOFMEMORY:
			return ERR_OUT_OF_MEMORY;
		default:
			return FAILED;
	}
}

D3DPRESENT_PARAMETERS FixedFunctionGraphicsDeviceD3D9::_make_present_parameters() const {
	D3DPRESENT_PARAMETERS parameters = {};
	parameters.BackBufferWidth = _requested_width;
	parameters.BackBufferHeight = _requested_height;
	parameters.BackBufferFormat = D3DFMT_UNKNOWN;
	parameters.BackBufferCount = 1;
	parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	parameters.hDeviceWindow = _window;
	parameters.Windowed = TRUE;
	parameters.PresentationInterval = _vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
	return parameters;
}

Error FixedFunctionGraphicsDeviceD3D9::initialize(void *p_native_window, const uint32_t p_width, const uint32_t p_height, const bool p_vsync) {
	if (_owner_thread) {
		return ERR_ALREADY_IN_USE;
	}
	HWND native_window = static_cast<HWND>(p_native_window);
	if (!IsWindow(native_window) || !p_width || !p_height) {
		return ERR_INVALID_PARAMETER;
	}
	if (GetWindowThreadProcessId(native_window, nullptr) != GetCurrentThreadId()) {
		return ERR_BUG;
	}
	_owner_thread = GetCurrentThreadId();
	_window = native_window;
	_requested_width = p_width;
	_requested_height = p_height;
	_vsync = p_vsync;
	_library = LoadLibraryA("d3d9.dll");
	if (!_library) {
		_last_result = HRESULT_FROM_WIN32(GetLastError());
		finalize();
		return ERR_UNAVAILABLE;
	}
	typedef IDirect3D9 *(WINAPI * CreateD3D9)(UINT);
	CreateD3D9 create_d3d9 = reinterpret_cast<CreateD3D9>(reinterpret_cast<void *>(GetProcAddress(_library, "Direct3DCreate9")));
	if (!create_d3d9 || !(_d3d = create_d3d9(D3D_SDK_VERSION))) {
		finalize();
		return _check_result(D3DERR_NOTAVAILABLE);
	}
	D3DCAPS9 caps = {};
	Error error = _check_result(_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps));
	if (error != OK) {
		finalize();
		return error;
	}
	_capabilities.max_texture_width = caps.MaxTextureWidth;
	_fog_raster_caps = caps.RasterCaps;
	_capabilities.max_texture_height = caps.MaxTextureHeight;
	_capabilities.max_texture_stages = caps.MaxSimultaneousTextures < caps.MaxTextureBlendStages ? caps.MaxSimultaneousTextures : caps.MaxTextureBlendStages;
	_capabilities.max_primitives_per_draw = caps.MaxPrimitiveCount;
	_geometry_vertex_limit = MIN(caps.MaxVertexIndex, DWORD(FixedFunctionGeometryData::MAX_BATCH_VERTICES - 1)) + 1;
	_capabilities.hardware_transform_and_light = (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;
	_capabilities.texture_power_of_two = (caps.TextureCaps & D3DPTEXTURECAPS_POW2) != 0;
	_capabilities.texture_square_only = (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0;
	_capabilities.blend_mix = (caps.SrcBlendCaps & D3DPBLENDCAPS_SRCALPHA) && (caps.DestBlendCaps & D3DPBLENDCAPS_INVSRCALPHA);
	_capabilities.blend_premultiplied_alpha = (caps.SrcBlendCaps & D3DPBLENDCAPS_ONE) && (caps.DestBlendCaps & D3DPBLENDCAPS_INVSRCALPHA);
	_capabilities.blend_add = (caps.SrcBlendCaps & D3DPBLENDCAPS_SRCALPHA) && (caps.DestBlendCaps & D3DPBLENDCAPS_ONE);
	_capabilities.blend_add_unweighted = (caps.SrcBlendCaps & D3DPBLENDCAPS_ONE) && (caps.DestBlendCaps & D3DPBLENDCAPS_ONE);
	_capabilities.blend_mul = (caps.SrcBlendCaps & D3DPBLENDCAPS_DESTCOLOR) && (caps.DestBlendCaps & D3DPBLENDCAPS_ZERO);
	_capabilities.cull_back = (caps.PrimitiveMiscCaps & D3DPMISCCAPS_CULLCCW) != 0;
	_capabilities.cull_front = (caps.PrimitiveMiscCaps & D3DPMISCCAPS_CULLCW) != 0;
	_capabilities.alpha_cutoff = (caps.AlphaCmpCaps & D3DPCMPCAPS_GREATEREQUAL) != 0;
	_capabilities.sampler_filter_linear = (caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) && (caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR);
	if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) && caps.MaxAnisotropy > 1) {
		_capabilities.sampler_max_anisotropy = uint8_t(MIN(caps.MaxAnisotropy, DWORD(255)));
	}
	_capabilities.sampler_mip_filters = MIP_FILTER_NONE;
	if (caps.TextureCaps & D3DPTEXTURECAPS_MIPMAP) {
		if (caps.TextureFilterCaps & D3DPTFILTERCAPS_MIPFPOINT) {
			_capabilities.sampler_mip_filters |= MIP_FILTER_POINT;
		}
		if (caps.TextureFilterCaps & D3DPTFILTERCAPS_MIPFLINEAR) {
			_capabilities.sampler_mip_filters |= MIP_FILTER_LINEAR;
		}
	}
	_capabilities.sampler_repeat = (caps.TextureAddressCaps & D3DPTADDRESSCAPS_WRAP) != 0;
	_capabilities.sampler_repeat_mirrored = (caps.TextureAddressCaps & D3DPTADDRESSCAPS_MIRROR) != 0;
	_capabilities.sampler_address_independent = (caps.TextureAddressCaps & D3DPTADDRESSCAPS_INDEPENDENTUV) != 0;

	D3DPRESENT_PARAMETERS parameters = _make_present_parameters();
	DWORD processing = _capabilities.hardware_transform_and_light ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	_resetting = true;
	// Device creation must not lower the precision of Godot's CPU math.
	HRESULT result = _d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _window, processing | D3DCREATE_FPU_PRESERVE, &parameters, &_device);
	if (FAILED(result) && processing == D3DCREATE_HARDWARE_VERTEXPROCESSING) {
		// CreateDevice can modify its parameters even when it fails.
		parameters = _make_present_parameters();
		result = _d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _window, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &parameters, &_device);
	}
	_resetting = false;
	error = _check_result(result);
	if (error != OK) {
		finalize();
		return error;
	}
	_width = parameters.BackBufferWidth;
	_height = parameters.BackBufferHeight;
	error = _check_result(_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &_window_target));
	if (error != OK) {
		finalize();
		return error;
	}
	D3DSURFACE_DESC window_description = {};
	error = _check_result(_window_target->GetDesc(&window_description));
	if (error != OK) {
		finalize();
		return error;
	}
	_adapter_format = window_description.Format;
	_update_depth_support();
	_update_compressed_texture_support();
	D3DCAPS9 device_caps = {};
	// Software vertex processing reports its own limits here, rather than the adapter's.
	if (SUCCEEDED(_device->GetDeviceCaps(&device_caps)) && _geometry_vertex_limit >= 3 &&
			(device_caps.VertexProcessingCaps & D3DVTXPCAPS_DIRECTIONALLIGHTS) && (device_caps.VertexProcessingCaps & D3DVTXPCAPS_POSITIONALLIGHTS)) {
		_vertex_light_limit = MIN(device_caps.MaxActiveLights, DWORD(MAX_PIXEL_LIGHTS));
	}
	error = _create_lighting_shaders(caps);
	if (error != OK) {
		finalize();
		return error;
	}
	_capabilities.render_target_opaque = SUCCEEDED(_d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window_description.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_X8R8G8B8));
	_capabilities.render_target_scaled = _capabilities.render_target_opaque;
	// Keep alpha targets optional: neither these caps nor this format are needed to start.
	const DWORD ALPHA_CAPS = D3DPMISCCAPS_SEPARATEALPHABLEND | D3DPMISCCAPS_COLORWRITEENABLE;
	const DWORD SOURCE_ALPHA_CAPS = D3DPBLENDCAPS_ONE | D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_SRCALPHA | D3DPBLENDCAPS_DESTALPHA;
	const DWORD DESTINATION_ALPHA_CAPS = D3DPBLENDCAPS_ONE | D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_INVSRCALPHA;
	_capabilities.render_target_transparent = _capabilities.render_target_opaque && (caps.PrimitiveMiscCaps & ALPHA_CAPS) == ALPHA_CAPS &&
			(caps.SrcBlendCaps & SOURCE_ALPHA_CAPS) == SOURCE_ALPHA_CAPS && (caps.DestBlendCaps & DESTINATION_ALPHA_CAPS) == DESTINATION_ALPHA_CAPS &&
			SUCCEEDED(_d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window_description.Format, D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8));
	D3DADAPTER_IDENTIFIER9 identifier = {};
	// No WHQL flag: diagnostic identity must not request online certificate checks.
	if (SUCCEEDED(_d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &identifier))) {
		identifier.Description[MAX_DEVICE_IDENTIFIER_STRING - 1] = 0;
		WCHAR description[MAX_DEVICE_IDENTIFIER_STRING];
		if (MultiByteToWideChar(CP_ACP, 0, identifier.Description, -1, description, MAX_DEVICE_IDENTIFIER_STRING)) {
			_adapter_name = description;
		}
		_adapter_vendor_id = identifier.VendorId;
	}
	_reset_pending = _width != _requested_width || _height != _requested_height || parameters.PresentationInterval != (_vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE);
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::texture_get_info(RID p_texture, FFTextureInfo &r_info) const {
	if (!_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D9Texture *texture = _texture_owner.getptr(p_texture);
	FFTextureInfo info;
	info.size = Size2i(texture->width, texture->height);
	if (texture->native) {
		const Size2i size = texture->upload_size;
		info.bytes = Image::get_image_data_size(size.x, size.y, Image::Format(texture->format), texture->mip_levels > 1);
		if (texture->depth) {
			info.bytes += uint64_t(size.x) * size.y * texture->depth_pixel_size;
		}
	}
	r_info = info;
	return OK;
}

uint64_t FixedFunctionGraphicsDeviceD3D9::get_texture_memory_usage() const {
	uint64_t bytes = 0;
	for (const List<D3D9Texture *>::Element *E = _textures.front(); E; E = E->next()) {
		FFTextureInfo info;
		texture_get_info(E->get()->rid, info);
		bytes += MIN(info.bytes, UINT64_MAX - bytes);
	}
	return bytes;
}

Error FixedFunctionGraphicsDeviceD3D9::finalize() {
	if (_owner_thread && _check_thread() != OK) {
		return ERR_BUG;
	}
	if (_resetting) {
		return ERR_BUSY;
	}
	if (_device) {
		if (_frame_active) {
			_device->EndScene();
		}
		if (_window_target) {
			_device->SetTexture(0, nullptr);
			_device->SetTexture(1, nullptr);
			_unbind_directional_shadow();
			_normal_map_bound = false;
		}
		_device->SetDepthStencilSurface(nullptr);
	}
	_release_lighting_shaders();
	_release_vertex_streams();
	_release_geometry_buffers();
	if (_window_depth) {
		_window_depth->Release();
		_window_depth = nullptr;
	}
	while (!_geometries.empty()) {
		_free_geometry(_geometries.front()->get());
	}
	while (!_textures.empty()) {
		D3D9Texture *texture = _textures.front()->get();
		if (texture->depth) {
			texture->depth->Release();
		}
		if (texture->native) {
			texture->native->Release();
		}
		_texture_owner.free(texture->rid);
		_textures.pop_front();
		memdelete(texture);
	}
	if (_device) {
		if (_window_target) {
			_window_target->Release();
			_window_target = nullptr;
		}
		_device->Release();
		_device = nullptr;
	}
	if (_d3d) {
		_d3d->Release();
		_d3d = nullptr;
	}
	if (_library) {
		FreeLibrary(_library);
		_library = nullptr;
	}
	_window = nullptr;
	_owner_thread = 0;
	_requested_width = _requested_height = _width = _height = 0;
	_capabilities = FFCapabilities();
	_compressed_texture_formats = 0;
	_geometry_vertex_limit = 0;
	_vertex_light_limit = _enabled_vertex_lights = 0;
	_adapter_name = String();
	_adapter_vendor_id = 0;
	_render_target = RID();
	_reset_pending = _frame_active = _depth_active = _frame_readable = _window_rendered = _resetting = false;
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::resize(const uint32_t p_width, const uint32_t p_height) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (p_width != _requested_width || p_height != _requested_height) {
		_requested_width = p_width;
		_requested_height = p_height;
		_reset_pending = true;
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::set_use_vsync(const bool p_enabled) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_vsync != p_enabled) {
		_vsync = p_enabled;
		_reset_pending = true;
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::_reset() {
	D3DPRESENT_PARAMETERS parameters = _make_present_parameters();
	const uint32_t new_width = _requested_width;
	const uint32_t new_height = _requested_height;
	const bool new_vsync = _vsync;
	_resetting = true;
	_frame_readable = _window_rendered = _depth_active = false;
	_release_vertex_streams();
	_release_geometry_buffers();
	_device->SetDepthStencilSurface(nullptr);
	if (_window_depth) {
		_window_depth->Release();
		_window_depth = nullptr;
	}
	if (_window_target) {
		_device->SetTexture(0, nullptr);
		_device->SetTexture(1, nullptr);
		_unbind_directional_shadow();
		_normal_map_bound = false;
		_device->SetRenderTarget(0, _window_target);
		_window_target->Release();
		_window_target = nullptr;
	}
	_render_target = RID();
	// Managed uploads survive Reset; volatile target allocations must be released first.
	for (List<D3D9Texture *>::Element *E = _textures.front(); E; E = E->next()) {
		D3D9Texture *texture = E->get();
		if (texture->render_target) {
			if (texture->depth) {
				texture->depth->Release();
				texture->depth = nullptr;
			}
			texture->depth_pixel_size = 0;
			if (texture->native) {
				texture->native->Release();
				texture->native = nullptr;
			}
			texture->rendered = false;
		}
	}
	Error error = _check_result(_device->Reset(&parameters));
	// Reset restores the default light state.
	_enabled_vertex_lights = 0;
	if (error == OK) {
		error = _check_result(_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &_window_target));
	}
	if (error == OK) {
		D3DSURFACE_DESC description = {};
		error = _check_result(_window_target->GetDesc(&description));
		if (error == OK) {
			if (_adapter_format != description.Format) {
				_adapter_format = description.Format;
				_update_compressed_texture_support();
			}
			_update_depth_support();
		}
	}
	if (error == OK) {
		for (List<D3D9Texture *>::Element *E = _textures.front(); E; E = E->next()) {
			if (E->get()->render_target) {
				error = _allocate_render_target(E->get());
				if (error != OK) {
					break;
				}
			}
		}
	}
	_resetting = false;
	if (error == OK) {
		_width = new_width;
		_height = new_height;
		// Reset can dispatch window messages that queue a different size or interval.
		_reset_pending = _requested_width != _width || _requested_height != _height || _vsync != new_vsync;
	} else {
		_reset_pending = true;
	}
	return error;
}

const D3DFORMAT FixedFunctionGraphicsDeviceD3D9::DEPTH_FORMATS[3] = { D3DFMT_D24X8, D3DFMT_D24S8, D3DFMT_D16 };

bool FixedFunctionGraphicsDeviceD3D9::_supports_depth_format(D3DFORMAT p_color, D3DFORMAT p_depth) const {
	return SUCCEEDED(_d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _adapter_format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, p_depth)) &&
			SUCCEEDED(_d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _adapter_format, p_color, p_depth));
}

void FixedFunctionGraphicsDeviceD3D9::_update_depth_support() {
	D3DCAPS9 caps = {};
	_capabilities.depth_buffer = false;
	if (FAILED(_device->GetDeviceCaps(&caps)) || !(caps.ZCmpCaps & D3DPCMPCAPS_LESSEQUAL) || !(caps.ZCmpCaps & D3DPCMPCAPS_ALWAYS)) {
		return;
	}
	for (int i = 0; i < 3; i++) {
		if (_supports_depth_format(_adapter_format, DEPTH_FORMATS[i])) {
			_capabilities.depth_buffer = true;
			return;
		}
	}
}

Error FixedFunctionGraphicsDeviceD3D9::_allocate_depth(IDirect3DSurface9 *p_color, IDirect3DSurface9 *&r_depth, uint8_t &r_pixel_size) {
	D3DSURFACE_DESC color = {};
	D3D9_FAIL_ON_ERROR(p_color->GetDesc(&color));
	Error error = ERR_UNAVAILABLE;
	for (int i = 0; i < 3; i++) {
		if (!_supports_depth_format(color.Format, DEPTH_FORMATS[i])) {
			continue;
		}
		// Preserve depth across target switches and load passes; reset releases it explicitly.
		IDirect3DSurface9 *depth = nullptr;
		error = _check_result(_device->CreateDepthStencilSurface(color.Width, color.Height, DEPTH_FORMATS[i], D3DMULTISAMPLE_NONE, 0, FALSE, &depth, nullptr));
		if (error == OK) {
			r_depth = depth;
			r_pixel_size = DEPTH_FORMATS[i] == D3DFMT_D16 ? 2 : 4;
			return OK;
		}
		if (depth) {
			depth->Release();
		}
		if (error == ERR_SKIP) {
			return error;
		}
	}
	return error;
}

Error FixedFunctionGraphicsDeviceD3D9::_allocate_render_target(D3D9Texture *p_texture) {
	p_texture->rendered = false;
	const D3DFORMAT format = p_texture->transparent ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
	return _check_result(_device->CreateTexture(p_texture->upload_size.x, p_texture->upload_size.y, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &p_texture->native, nullptr));
}

Error FixedFunctionGraphicsDeviceD3D9::texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active || _resetting || _reset_pending) {
		return ERR_BUSY;
	}
	if (r_texture.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	Size2i upload_size;
	error = get_texture_upload_size(p_width, p_height, upload_size);
	if (error != OK) {
		return error;
	}
	if (p_transparent ? !_capabilities.render_target_transparent : !_capabilities.render_target_opaque) {
		return ERR_UNAVAILABLE;
	}
	D3D9_FAIL_ON_ERROR(_device->TestCooperativeLevel());
	D3D9Texture *texture = memnew(D3D9Texture);
	texture->width = p_width;
	texture->height = p_height;
	texture->upload_size = upload_size;
	texture->render_target = true;
	texture->transparent = p_transparent;
	error = _allocate_render_target(texture);
	if (error != OK) {
		memdelete(texture);
		return error;
	}
	texture->rid = _texture_owner.make_rid(texture);
	_textures.push_back(texture);
	r_texture = texture->rid;
	return OK;
}

void FixedFunctionGraphicsDeviceD3D9::_update_compressed_texture_support() {
	_compressed_texture_formats = 0;
	for (unsigned i = 0; i < sizeof(D3D9_DXT_FORMATS) / sizeof(D3D9_DXT_FORMATS[0]); i++) {
		if (SUCCEEDED(_d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _adapter_format, 0, D3DRTYPE_TEXTURE, D3D9_DXT_FORMATS[i]))) {
			_compressed_texture_formats |= 1 << i;
		}
	}
}

bool FixedFunctionGraphicsDeviceD3D9::supports_compressed_texture(Image::Format p_format) const {
	return p_format >= Image::FORMAT_DXT1 && p_format <= Image::FORMAT_DXT5 &&
			(_compressed_texture_formats & (1 << (p_format - Image::FORMAT_DXT1))) != 0;
}

Error FixedFunctionGraphicsDeviceD3D9::texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if ((_frame_active && !p_allow_during_frame) || _resetting || _reset_pending) {
		return ERR_BUSY;
	}
	if (r_texture.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	Ref<Image> upload;
	error = prepare_texture_upload(p_image, upload);
	if (error != OK) {
		return error;
	}
	D3D9_FAIL_ON_ERROR(_device->TestCooperativeLevel());
	const uint32_t image_width = upload->get_width();
	const uint32_t image_height = upload->get_height();
	const Image::Format format = upload->get_format();
	const bool compressed = upload->is_compressed();
	const D3DFORMAT native_format = compressed ? D3D9_DXT_FORMATS[format - Image::FORMAT_DXT1] : D3DFMT_A8R8G8B8;
	IDirect3DTexture9 *native = nullptr;
	uint8_t levels = _capabilities.sampler_mip_filters ? upload->get_mipmap_count() + 1 : 1;
	for (;;) {
		HRESULT result = _device->CreateTexture(image_width, image_height, levels, 0, native_format, D3DPOOL_MANAGED, &native, nullptr);
		if (SUCCEEDED(result) && native->GetLevelCount() != levels) {
			result = D3DERR_NOTAVAILABLE;
		}
		const PoolVector<uint8_t>::Read source = upload->get_data().read();
		uint32_t width = image_width, height = image_height;
		size_t offset = 0;
		for (uint32_t level = 0; SUCCEEDED(result) && level < levels; level++) {
			D3DLOCKED_RECT locked = {};
			result = native->LockRect(level, &locked, nullptr, 0);
			if (FAILED(result)) {
				break;
			}
			bool copied = true;
			if (compressed) {
				const uint32_t row_bytes = Image::get_image_data_size(width, 1, format, false);
				const uint32_t rows = (height + 3) / 4;
				copied = copy_compressed_texture_rows(locked.pBits, locked.Pitch, source.ptr() + offset, row_bytes, rows);
				offset += size_t(row_bytes) * rows;
			} else {
				for (uint32_t y = 0; y < height; y++) {
					uint32_t *row = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(locked.pBits) + ptrdiff_t(y) * locked.Pitch);
					for (uint32_t x = 0; x < width; x++) {
						const uint8_t *rgba = source.ptr() + offset + (size_t(y) * width + x) * 4;
						row[x] = (uint32_t(rgba[3]) << 24) | (uint32_t(rgba[0]) << 16) | (uint32_t(rgba[1]) << 8) | rgba[2];
					}
				}
				offset += size_t(width) * height * 4;
			}
			result = native->UnlockRect(level);
			if (SUCCEEDED(result) && !copied) {
				result = D3DERR_NOTAVAILABLE;
			}
			width = MAX(width >> 1, 1u);
			height = MAX(height >> 1, 1u);
		}
		error = _check_result(result);
		if (error == OK) {
			break;
		}
		if (native) {
			native->Release();
			native = nullptr;
		}
		if (levels == 1 || compressed || error == ERR_SKIP) {
			return error;
		}
		levels = 1;
	}
	D3D9Texture *texture = memnew(D3D9Texture);
	texture->native = native;
	texture->width = p_image->get_width();
	texture->height = p_image->get_height();
	texture->upload_size = Size2i(image_width, image_height);
	texture->mip_levels = levels;
	texture->format = format;
	texture->rid = _texture_owner.make_rid(texture);
	_textures.push_back(texture);
	r_texture = texture->rid;
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::texture_free(RID p_texture) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active || _resetting) {
		return ERR_BUSY;
	}
	if (!_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	D3D9Texture *texture = _texture_owner.getptr(p_texture);
	if (p_texture == _render_target) {
		D3D9_FAIL_ON_ERROR(_restore_window_target());
	}
	if (_window_target) {
		_device->SetTexture(0, nullptr);
		_device->SetTexture(1, nullptr);
		_unbind_directional_shadow();
		_normal_map_bound = false;
	}
	if (texture->depth) {
		texture->depth->Release();
	}
	if (texture->native) {
		texture->native->Release();
	}
	_texture_owner.free(p_texture);
	_textures.erase(texture);
	memdelete(texture);
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::_restore_window_target() {
	if (!_render_target.is_valid()) {
		return OK;
	}
	if (!_window_target) {
		return ERR_SKIP;
	}
	Error error = _check_result(_device->SetDepthStencilSurface(nullptr));
	if (error == OK) {
		error = _check_result(_device->SetRenderTarget(0, _window_target));
	}
	if (error == OK) {
		_render_target = RID();
	}
	return error;
}

bool FixedFunctionGraphicsDeviceD3D9::texture_is_render_target_ready(RID p_texture) const {
	if (_check_thread() != OK || _resetting || _reset_pending || !_texture_owner.owns(p_texture) || _render_target == p_texture) {
		return false;
	}
	const D3D9Texture *texture = _texture_owner.getptr(p_texture);
	return texture->render_target && texture->native && texture->rendered;
}

Error FixedFunctionGraphicsDeviceD3D9::begin_frame(const uint32_t p_clear_color, RID p_render_target, bool p_clear, bool p_use_depth) {
	const Error error = _begin_frame(p_clear_color, p_render_target, p_clear, p_use_depth);
	if (error != OK && _check_thread() == OK && !_resetting && !_frame_active && _render_target.is_valid()) {
		_restore_window_target();
	}
	return error;
}

Error FixedFunctionGraphicsDeviceD3D9::prepare_resources() {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active || _resetting) {
		return ERR_BUSY;
	}
	if (!_requested_width || !_requested_height) {
		return ERR_SKIP;
	}
	HRESULT cooperative = _device->TestCooperativeLevel();
	if (FAILED(cooperative) && cooperative != D3DERR_DEVICENOTRESET) {
		return _check_result(cooperative);
	}
	if (_reset_pending || cooperative == D3DERR_DEVICENOTRESET) {
		error = _reset();
		if (error != OK) {
			return error;
		}
		if (_reset_pending) {
			return ERR_SKIP;
		}
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::_begin_frame(uint32_t p_clear_color, RID p_render_target, bool p_clear, bool p_use_depth) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active || _resetting) {
		return ERR_BUSY;
	}
	if (p_render_target.is_valid() && (!_texture_owner.owns(p_render_target) || !_texture_owner.getptr(p_render_target)->render_target)) {
		return ERR_INVALID_PARAMETER;
	}
	_frame_readable = _depth_active = false;
	error = prepare_resources();
	if (error != OK) {
		return error;
	}
	if (p_use_depth && !_capabilities.depth_buffer) {
		return ERR_UNAVAILABLE;
	}

	if (!p_clear && (p_render_target.is_valid() ? !_texture_owner.getptr(p_render_target)->rendered : !_window_rendered)) {
		return ERR_UNCONFIGURED;
	}
	D3D9_FAIL_ON_ERROR(_device->SetDepthStencilSurface(nullptr));
	D3D9_FAIL_ON_ERROR(_device->SetTexture(0, nullptr));
	D3D9_FAIL_ON_ERROR(_device->SetTexture(1, nullptr));
	error = _unbind_directional_shadow();
	if (error != OK) {
		return error;
	}
	_normal_map_bound = false;
	if (p_render_target.is_valid()) {
		D3D9Texture *texture = _texture_owner.getptr(p_render_target);
		if (!texture->native) {
			return ERR_UNCONFIGURED;
		}
		IDirect3DSurface9 *surface = nullptr;
		D3D9_FAIL_ON_ERROR(texture->native->GetSurfaceLevel(0, &surface));
		const HRESULT result = _device->SetRenderTarget(0, surface);
		surface->Release();
		D3D9_FAIL_ON_ERROR(result);
		_render_target = p_render_target;
		_render_width = texture->width;
		_render_height = texture->height;
		texture->rendered = false;
	} else {
		D3D9_FAIL_ON_ERROR(_device->SetRenderTarget(0, _window_target));
		_render_target = RID();
		_window_rendered = false;
	}
	bool new_depth = false;
	if (p_use_depth) {
		D3D9Texture *texture = _render_target.is_valid() ? _texture_owner.getptr(_render_target) : nullptr;
		IDirect3DSurface9 *&depth = texture ? texture->depth : _window_depth;
		new_depth = depth == nullptr;
		uint8_t window_pixel_size = 0;
		if (new_depth) {
			IDirect3DSurface9 *color = nullptr;
			D3D9_FAIL_ON_ERROR(_device->GetRenderTarget(0, &color));
			error = _allocate_depth(color, depth, texture ? texture->depth_pixel_size : window_pixel_size);
			color->Release();
			if (error != OK) {
				return error;
			}
		}
		D3D9_FAIL_ON_ERROR(_device->SetDepthStencilSurface(depth));
	}
	D3DMATRIX identity = {};
	identity._11 = identity._22 = identity._33 = identity._44 = 1;
	const struct {
		D3DRENDERSTATETYPE state;
		DWORD value;
	} states[] = {
		{ D3DRS_LIGHTING, FALSE },
		{ D3DRS_ZENABLE, D3DZB_FALSE },
		{ D3DRS_ZWRITEENABLE, FALSE },
		{ D3DRS_CULLMODE, D3DCULL_NONE },
		{ D3DRS_CLIPPING, TRUE },
		{ D3DRS_ALPHABLENDENABLE, FALSE },
		{ D3DRS_ALPHATESTENABLE, FALSE },
		{ D3DRS_FOGENABLE, FALSE },
		{ D3DRS_SHADEMODE, D3DSHADE_GOURAUD },
	};
	for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
		error = _check_result(_device->SetRenderState(states[i].state, states[i].value));
		if (error != OK) {
			return error;
		}
	}
	if (_capabilities.render_target_transparent) {
		const bool transparent = _render_target.is_valid() && _texture_owner.getptr(_render_target)->transparent;
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, transparent));
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA));
	}
	const Size2i native_size = _get_native_target_size();
	D3DVIEWPORT9 viewport = { 0, 0, DWORD(native_size.x), DWORD(native_size.y), 0.0f, 1.0f };
	D3D9_FAIL_ON_ERROR(_device->SetViewport(&viewport));
	D3D9_FAIL_ON_ERROR(_device->SetVertexShader(nullptr));
	D3D9_FAIL_ON_ERROR(_device->SetPixelShader(nullptr));
	D3D9_FAIL_ON_ERROR(_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1));
	D3D9_FAIL_ON_ERROR(_device->SetTransform(D3DTS_WORLD, &identity));
	D3D9_FAIL_ON_ERROR(_device->SetTransform(D3DTS_VIEW, &identity));
	D3D9_FAIL_ON_ERROR(_device->SetTexture(0, nullptr));
	D3D9_FAIL_ON_ERROR(_device->SetTexture(1, nullptr));
	_normal_map_bound = false;
	D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1));
	D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE));
	D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE));
	D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1));
	D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE));
	D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE));
	_color_source = COLOR_SOURCE_DIFFUSE;
	_fog_enabled = false;
	D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0));
	D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE));
	error = _set_sampler_state(0);
	if (error != OK) {
		return error;
	}
	if (_capabilities.max_texture_stages > 1) {
		D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE));
	}
	if (p_clear || new_depth) {
		const DWORD flags = (p_clear ? D3DCLEAR_TARGET : 0) | (p_use_depth ? D3DCLEAR_ZBUFFER : 0);
		const bool transparent = _render_target.is_valid() && _texture_owner.getptr(_render_target)->transparent;
		D3D9_FAIL_ON_ERROR(_device->Clear(0, nullptr, flags, transparent ? p_clear_color : p_clear_color | 0xff000000u, 1.0f, 0));
	}
	error = _check_result(_device->BeginScene());
	_frame_active = error == OK;
	_depth_active = _frame_active && p_use_depth;
	return error;
}

Error FixedFunctionGraphicsDeviceD3D9::clear_color(uint32_t p_color) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_frame_active) {
		return ERR_UNCONFIGURED;
	}
	const Size2i native_size = _get_native_target_size();
	D3DVIEWPORT9 viewport = { 0, 0, DWORD(native_size.x), DWORD(native_size.y), 0.0f, 1.0f };
	D3D9_FAIL_ON_ERROR(_device->SetViewport(&viewport));
	const bool transparent = _render_target.is_valid() && _texture_owner.getptr(_render_target)->transparent;
	return _check_result(_device->Clear(0, nullptr, D3DCLEAR_TARGET, transparent ? p_color : p_color | 0xff000000u, 1.0f, 0));
}

Error FixedFunctionGraphicsDeviceD3D9::clear_depth(float p_depth) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_frame_active || !_depth_active) {
		return ERR_UNCONFIGURED;
	}
	if ((Math::is_nan(p_depth) || Math::is_inf(p_depth)) || p_depth < 0 || p_depth > 1) {
		return ERR_INVALID_PARAMETER;
	}
	const Size2i native_size = _get_native_target_size();
	D3DVIEWPORT9 viewport = { 0, 0, DWORD(native_size.x), DWORD(native_size.y), 0.0f, 1.0f };
	D3D9_FAIL_ON_ERROR(_device->SetViewport(&viewport));
	return _check_result(_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, p_depth, 0));
}

Error FixedFunctionGraphicsDeviceD3D9::_set_sampler_state(uint8_t p_flags, uint32_t p_sampler, uint8_t p_levels) {
	const D3DTEXTUREFILTERTYPE filter = (p_flags & SAMPLER_FILTER_LINEAR) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
	const uint8_t anisotropy = get_sampler_anisotropy(p_flags);
	if (anisotropy > 1) {
		D3D9_FAIL_ON_ERROR(_device->SetSamplerState(p_sampler, D3DSAMP_MAXANISOTROPY, anisotropy));
	}
	D3D9_FAIL_ON_ERROR(_device->SetSamplerState(p_sampler, D3DSAMP_MINFILTER, anisotropy > 1 ? D3DTEXF_ANISOTROPIC : filter));
	D3D9_FAIL_ON_ERROR(_device->SetSamplerState(p_sampler, D3DSAMP_MAGFILTER, filter));
	const FFMipFilter mip = get_mip_filter(p_flags, p_levels);
	D3DTEXTUREFILTERTYPE mip_filter = D3DTEXF_NONE;
	if (mip != MIP_FILTER_NONE) {
		mip_filter = mip == MIP_FILTER_LINEAR ? D3DTEXF_LINEAR : D3DTEXF_POINT;
	}
	D3D9_FAIL_ON_ERROR(_device->SetSamplerState(p_sampler, D3DSAMP_MIPFILTER, mip_filter));
	const D3DTEXTUREADDRESS repeat = (p_flags & SAMPLER_REPEAT_MIRRORED) ? D3DTADDRESS_MIRROR : D3DTADDRESS_WRAP;
	D3D9_FAIL_ON_ERROR(_device->SetSamplerState(p_sampler, D3DSAMP_ADDRESSU, (p_flags & SAMPLER_REPEAT_U) ? repeat : D3DTADDRESS_CLAMP));
	D3D9_FAIL_ON_ERROR(_device->SetSamplerState(p_sampler, D3DSAMP_ADDRESSV, (p_flags & SAMPLER_REPEAT_V) ? repeat : D3DTADDRESS_CLAMP));
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::_prepare_draw(const void *p_vertices, uint32_t p_vertex_count, RID p_texture, const FFDrawParameters &p_state, Rect2i &r_viewport, bool p_constant_color) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_frame_active) {
		return ERR_UNCONFIGURED;
	}
	if (p_texture.is_valid() && !_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_texture.is_valid()) {
		if (p_texture == _render_target) {
			return ERR_INVALID_PARAMETER;
		}
		const D3D9Texture *texture = _texture_owner.getptr(p_texture);
		if (texture->render_target && (!texture->native || !texture->rendered)) {
			return ERR_UNCONFIGURED;
		}
	}
	if (p_vertex_count % 3 || p_vertex_count / 3 > _capabilities.max_primitives_per_draw || (p_vertex_count && !p_vertices)) {
		return ERR_INVALID_PARAMETER;
	}
	error = validate_draw_state(p_state);
	if (error != OK) {
		return error;
	}
	if ((p_state.depth_test_enabled || p_state.depth_write_enabled) && !_depth_active) {
		return ERR_UNCONFIGURED;
	}
	r_viewport = get_texture_native_clip(get_draw_viewport(p_state), Size2i(get_target_width(), get_target_height()), _get_native_target_size());
	if (!p_vertex_count || !r_viewport.size.x || !r_viewport.size.y || (p_state.alpha_cutoff_enabled && p_state.alpha_cutoff > 1.0f)) {
		r_viewport = Rect2i();
		return OK;
	}
	if (_depth_active) {
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_ZENABLE, (p_state.depth_test_enabled || p_state.depth_write_enabled) ? D3DZB_TRUE : D3DZB_FALSE));
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_ZWRITEENABLE, p_state.depth_write_enabled));
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_ZFUNC, p_state.depth_test_enabled ? D3DCMP_LESSEQUAL : D3DCMP_ALWAYS));
	}
	D3DVIEWPORT9 viewport = { DWORD(r_viewport.position.x), DWORD(r_viewport.position.y), DWORD(r_viewport.size.x), DWORD(r_viewport.size.y), 0.0f, 1.0f };
	D3D9_FAIL_ON_ERROR(_device->SetViewport(&viewport));
	if (_render_target.is_valid() && _texture_owner.getptr(_render_target)->transparent) {
		const DWORD COLOR_MASK = D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_COLORWRITEENABLE, COLOR_MASK | (p_state.alpha_write_enabled ? D3DCOLORWRITEENABLE_ALPHA : 0)));
		if (p_state.blend_mode != BLEND_MODE_DISABLED) {
			// Same accumulated alpha as GLES canvas draws, independently of RGB factors.
			D3DBLEND source_alpha = D3DBLEND_ONE;
			D3DBLEND destination_alpha = D3DBLEND_INVSRCALPHA;
			if (p_state.blend_mode == BLEND_MODE_ADD) {
				source_alpha = D3DBLEND_SRCALPHA;
				destination_alpha = D3DBLEND_ONE;
			} else if (p_state.blend_mode == BLEND_MODE_ADD_UNWEIGHTED) {
				source_alpha = destination_alpha = D3DBLEND_ONE;
			} else if (p_state.blend_mode == BLEND_MODE_MUL) {
				source_alpha = D3DBLEND_DESTALPHA;
				destination_alpha = D3DBLEND_ZERO;
			}
			D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_SRCBLENDALPHA, source_alpha));
			D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_DESTBLENDALPHA, destination_alpha));
		}
	}
	if (p_state.blend_mode != BLEND_MODE_DISABLED) {
		D3DBLEND source = D3DBLEND_SRCALPHA;
		D3DBLEND destination = D3DBLEND_INVSRCALPHA;
		if (p_state.blend_mode == BLEND_MODE_PREMULTIPLIED_ALPHA) {
			source = D3DBLEND_ONE;
		} else if (p_state.blend_mode == BLEND_MODE_ADD) {
			destination = D3DBLEND_ONE;
		} else if (p_state.blend_mode == BLEND_MODE_ADD_UNWEIGHTED) {
			source = destination = D3DBLEND_ONE;
		} else if (p_state.blend_mode == BLEND_MODE_MUL) {
			source = D3DBLEND_DESTCOLOR;
			destination = D3DBLEND_ZERO;
		}
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_SRCBLEND, source));
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_DESTBLEND, destination));
	}
	D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_ALPHABLENDENABLE, p_state.blend_mode != BLEND_MODE_DISABLED));
	if (p_state.alpha_cutoff_enabled) {
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL));
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_ALPHAREF, get_alpha_reference(p_state.alpha_cutoff)));
	}
	// Godot clockwise front faces survive CCW culling; clip-depth conversion preserves winding.
	D3DCULL cull_mode = D3DCULL_NONE;
	if (p_state.cull_mode == CULL_BACK) {
		cull_mode = D3DCULL_CCW;
	} else if (p_state.cull_mode == CULL_FRONT) {
		cull_mode = D3DCULL_CW;
	}
	D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_CULLMODE, cull_mode));
	D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_ALPHATESTENABLE, p_state.alpha_cutoff_enabled));
	if (_normal_map_bound) {
		D3D9_FAIL_ON_ERROR(_device->SetTexture(1, nullptr));
		_normal_map_bound = false;
	}
	error = _unbind_directional_shadow();
	if (error != OK) {
		return error;
	}
	const D3D9Texture *texture = p_texture.is_valid() ? _texture_owner.getptr(p_texture) : nullptr;
	D3D9_FAIL_ON_ERROR(_device->SetTexture(0, texture ? texture->native : nullptr));
	error = _set_sampler_state(p_state.sampler_flags, 0, texture ? texture->mip_levels : 1);
	if (error != OK) {
		return error;
	}
	// Restore only on a transition; ordinary GUI draws need no extra state calls.
	if (!p_constant_color && _color_source != COLOR_SOURCE_DIFFUSE) {
		_color_source = COLOR_SOURCE_DIRTY;
		D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE));
		D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE));
		_color_source = COLOR_SOURCE_DIFFUSE;
	}
	D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_COLOROP, p_texture.is_valid() && p_state.texture_rgb_enabled ? D3DTOP_MODULATE : D3DTOP_SELECTARG1));
	D3D9_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_ALPHAOP, p_texture.is_valid() && p_state.texture_alpha_enabled ? D3DTOP_MODULATE : D3DTOP_SELECTARG1));
	return OK;
}

Size2i FixedFunctionGraphicsDeviceD3D9::_get_native_target_size() const {
	return _render_target.is_valid() ? _texture_owner.getptr(_render_target)->upload_size : Size2i(_width, _height);
}

CameraMatrix FixedFunctionGraphicsDeviceD3D9::_get_draw_projection(const CameraMatrix &p_projection, const Rect2i &p_viewport) const {
	const Size2i native_size = _get_native_target_size();
	CameraMatrix correction;
	// Preserve Godot's handedness; remap depth without Y reversal or reverse Z.
	correction.matrix[2][2] = real_t(0.5);
	correction.matrix[3][2] = real_t(0.5);
	// Native D3D pixel centers are integers, rather than Godot's half-pixel centers.
	correction.matrix[3][0] = -real_t(1) / native_size.x;
	correction.matrix[3][1] = real_t(1) / native_size.y;
	return get_viewport_projection(correction * p_projection, p_viewport, native_size);
}

Error FixedFunctionGraphicsDeviceD3D9::_set_table_fog(const FFFog *p_fog, FFBlendMode p_blend_mode) {
	if (p_fog && (_fog_raster_caps & D3DPRASTERCAPS_FOGTABLE)) {
		const bool use_w = !p_fog->orthogonal && (_fog_raster_caps & D3DPRASTERCAPS_WFOG);
		const float begin = use_w ? p_fog->begin : p_fog->projected_begin;
		const float end = use_w ? p_fog->end : p_fog->projected_end;
		if ((use_w || (_fog_raster_caps & D3DPRASTERCAPS_ZFOG)) && end > begin) {
			const uint32_t color = p_fog->get_blend_color(p_blend_mode).to_argb32();
			if (!_fog_enabled || begin != _fog_begin || end != _fog_end || color != _fog_color) {
				DWORD native_begin, native_end;
				memcpy(&native_begin, &begin, sizeof(begin));
				memcpy(&native_end, &end, sizeof(end));
				D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR));
				D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_FOGSTART, native_begin));
				D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_FOGEND, native_end));
				D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_FOGCOLOR, color));
				_fog_begin = begin;
				_fog_end = end;
				_fog_color = color;
				if (!_fog_enabled) {
					D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_FOGENABLE, TRUE));
					_fog_enabled = true;
				}
			}
			return OK;
		}
	}
	if (_fog_enabled) {
		D3D9_FAIL_ON_ERROR(_device->SetRenderState(D3DRS_FOGENABLE, FALSE));
		_fog_enabled = false;
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D9::draw_triangles(const FixedFunctionVertex *p_vertices, const uint32_t p_vertex_count, const CameraMatrix &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state) {
	return _draw_fixed_triangles(p_vertices, p_vertex_count, nullptr, p_vertex_count, p_clip_from_model, p_texture, p_state);
}

Error FixedFunctionGraphicsDeviceD3D9::draw_indexed_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count, const CameraMatrix &p_projection, RID p_texture, const FFDrawParameters &p_state) {
	const Error error = validate_triangle_indices(p_vertex_count, p_indices, p_index_count);
	if (error != OK) {
		return error;
	}
	if (p_index_count && p_vertex_count > _geometry_vertex_limit) {
		return ERR_UNAVAILABLE;
	}
	return _draw_fixed_triangles(p_vertices, p_vertex_count, reinterpret_cast<const uint16_t *>(p_indices), p_index_count, p_projection, p_texture, p_state);
}

Error FixedFunctionGraphicsDeviceD3D9::_draw_fixed_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint16_t *p_indices, uint32_t p_element_count, const CameraMatrix &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state) {
	Rect2i viewport;
	Error error = _prepare_draw(p_vertices, p_element_count, p_texture, p_state, viewport);
	if (error != OK || !p_element_count || !viewport.size.x || !viewport.size.y) {
		return error;
	}
	error = _set_table_fog(p_state.fog, p_state.blend_mode);
	if (error != OK) {
		return error;
	}
	// A previous 3D draw may have selected the optional shader pipeline.
	D3D9_FAIL_ON_ERROR(_device->SetVertexShader(nullptr));
	D3D9_FAIL_ON_ERROR(_device->SetPixelShader(nullptr));
	D3D9_FAIL_ON_ERROR(_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1));
	float matrix[16];
	store_camera(_get_draw_projection(p_clip_from_model, viewport), matrix);
	D3DMATRIX projection;
	// Column-major column-vector data is the transpose in D3D's row-vector layout.
	memcpy(&projection, matrix, sizeof(projection));
	error = _check_result(_device->SetTransform(D3DTS_PROJECTION, &projection));
	if (error != OK) {
		return error;
	}
	error = _check_result(_draw_vertex_stream(p_vertices, p_vertex_count, sizeof(FixedFunctionVertex), _vertex_stream, p_indices, p_indices ? p_element_count : 0));
	// Only a refusal before submission can fall back to expanded triangles.
	return p_indices && error == ERR_UNAVAILABLE ? FAILED : error;
}

Error FixedFunctionGraphicsDeviceD3D9::end_frame(const bool p_present, bool p_discard) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_frame_active) {
		return ERR_UNCONFIGURED;
	}
	_frame_active = _depth_active = false;
	error = _check_result(_device->EndScene());
	if (_render_target.is_valid()) {
		_texture_owner.getptr(_render_target)->rendered = error == OK && !p_discard;
		const Error restore_error = _restore_window_target();
		if (error == OK) {
			error = restore_error;
		}
		if (error != OK) {
			return error;
		}
		return p_present && !p_discard ? ERR_UNAVAILABLE : OK;
	}
	// Never load a presented window buffer; D3D9 uses DISCARD swap semantics.
	_window_rendered = error == OK && !p_present && !p_discard;
	if (error == OK) {
		if (p_present && !p_discard) {
			error = _check_result(_device->Present(nullptr, nullptr, nullptr, nullptr));
		} else if (!p_discard) {
			_frame_readable = true;
		}
	}
	return error;
}

Error FixedFunctionGraphicsDeviceD3D9::read_frame(uint32_t *r_pixels, const size_t p_pixel_count) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active || _resetting || _reset_pending) {
		return ERR_BUSY;
	}
	if (!_frame_readable) {
		return ERR_UNCONFIGURED;
	}
	if (!r_pixels || p_pixel_count < uint64_t(_width) * _height) {
		return ERR_INVALID_PARAMETER;
	}
	IDirect3DSurface9 *target = nullptr;
	const HRESULT result = _device->GetRenderTarget(0, &target);
	if (FAILED(result)) {
		return _check_result(result);
	}
	error = _read_surface(target, _width, _height, r_pixels);
	target->Release();
	return error;
}

FixedFunctionGraphicsDeviceD3D9::FixedFunctionGraphicsDeviceD3D9() :
		_library(nullptr),
		_d3d(nullptr),
		_device(nullptr),
		_lighting_declaration(nullptr),
		_lighting_vertex_shader(nullptr),
		_lighting_geometry_shader(nullptr),
		_lighting_pixel_shader(nullptr),
		_lighting_textured_shader(nullptr),
		_normal_map_declaration(nullptr),
		_normal_map_vertex_shader(nullptr),
		_normal_map_pixel_shader(nullptr),
		_normal_map_textured_shader(nullptr),
		_transient_normal_declaration(nullptr),
		_transient_normal_vertex_shader(nullptr),
		_transient_normal_occlusion_shader(nullptr),
		_transient_occlusion_declaration(nullptr),
		_transient_occlusion_shader(nullptr),
		_occlusion_shaders(),
		_normal_occlusion_shaders(),
		_window_target(nullptr),
		_window_depth(nullptr),
		_window(nullptr),
		_owner_thread(0),
		_last_result(D3D_OK),
		_capabilities(),
		_adapter_format(D3DFMT_UNKNOWN),
		_requested_width(0),
		_requested_height(0),
		_width(0),
		_height(0),
		_render_width(0),
		_render_height(0),
		_pixel_light_limit(0),
		_geometry_vertex_limit(0),
		_compressed_texture_formats(0),
		_normal_map_unavailable(false),
		_transient_normal_unavailable(false),
		_transient_normal_occlusion_unavailable(false),
		_transient_occlusion_unavailable(false),
		_normal_map_bound(false),
		_shadow_bound(false),
		_vsync(true),
		_reset_pending(false),
		_frame_active(false),
		_depth_active(false),
		_frame_readable(false),
		_window_rendered(false),
		_resetting(false),
		_color_source(COLOR_SOURCE_DIFFUSE) {
}

FixedFunctionGraphicsDeviceD3D9::~FixedFunctionGraphicsDeviceD3D9() {
	finalize();
}

#undef D3D9_FAIL_ON_ERROR
