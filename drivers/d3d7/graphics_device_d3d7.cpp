#include "graphics_device_d3d7.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "drivers/d3d7/graphics_device_d3d7_factory.h"

#include <string.h>

FixedFunctionGraphicsDevice *FixedFunctionGraphicsDeviceD3D7Factory::create() {
	const bool force_rgb = ProjectSettings::get_singleton() && bool(GLOBAL_DEF_RST("rendering/fixed_function/d3d7/force_rgb", false));
	return memnew(FixedFunctionGraphicsDeviceD3D7(D3D7ResolutionPolicy::get_configured_mode(), force_rgb));
}

#define D3D7_FAIL_ON_ERROR(m_call) \
	do { \
		const Error call_error = _check_result(m_call); \
		if (call_error != OK) { \
			return call_error; \
		} \
	} while (false)

static_assert(sizeof(FixedFunctionVertex) == 24 && offsetof(FixedFunctionVertex, color) == 12 && offsetof(FixedFunctionVertex, u) == 16, "Unexpected vertex layout.");

static const DWORD D3D7_DXT_FORMATS[] = { MAKEFOURCC('D', 'X', 'T', '1'), MAKEFOURCC('D', 'X', 'T', '3'), MAKEFOURCC('D', 'X', 'T', '5') };

Error FixedFunctionGraphicsDeviceD3D7::_check_thread() const {
	if (!_owner_thread) {
		return ERR_UNCONFIGURED;
	}
	return GetCurrentThreadId() == _owner_thread ? OK : ERR_BUG;
}

Error FixedFunctionGraphicsDeviceD3D7::_check_result(HRESULT p_result) {
	_last_result = p_result;
	if (SUCCEEDED(p_result)) {
		return OK;
	}
	switch (p_result) {
		case DDERR_SURFACELOST:
		case DDERR_WRONGMODE:
			_reset_pending = true;
			return ERR_SKIP;
		case DDERR_EXCLUSIVEMODEALREADYSET:
		case DDERR_NOEXCLUSIVEMODE:
		case DDERR_WASSTILLDRAWING:
			return ERR_SKIP;
		case DDERR_NO3D:
		case DDERR_UNSUPPORTED:
		case E_NOINTERFACE:
			return ERR_UNAVAILABLE;
		case DDERR_OUTOFVIDEOMEMORY:
		case E_OUTOFMEMORY:
			return ERR_OUT_OF_MEMORY;
		default:
			return FAILED;
	}
}

Size2i FixedFunctionGraphicsDeviceD3D7::_get_native_target_size() const {
	return _render_target.is_valid() ? _texture_owner.get_or_null(_render_target)->native_size : _native_size;
}

Error FixedFunctionGraphicsDeviceD3D7::initialize(void *p_native_window, const uint32_t p_width, const uint32_t p_height, const bool p_vsync) {
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
	_library = LoadLibraryA("ddraw.dll");
	if (!_library) {
		_last_result = HRESULT_FROM_WIN32(GetLastError());
		finalize();
		return ERR_UNAVAILABLE;
	}
	typedef HRESULT(WINAPI * CreateDirectDraw)(GUID *, void **, REFIID, IUnknown *);
	CreateDirectDraw create_ddraw = reinterpret_cast<CreateDirectDraw>(reinterpret_cast<void *>(GetProcAddress(_library, "DirectDrawCreateEx")));
	if (!create_ddraw) {
		finalize();
		return _check_result(E_NOINTERFACE);
	}
	_resetting = true;
	HRESULT result = create_ddraw(nullptr, reinterpret_cast<void **>(&_ddraw), IID_IDirectDraw7, nullptr);
	if (SUCCEEDED(result)) {
		result = _ddraw->SetCooperativeLevel(_window, DDSCL_NORMAL | DDSCL_FPUPRESERVE);
	}
	if (SUCCEEDED(result)) {
		result = _ddraw->QueryInterface(IID_IDirect3D7, reinterpret_cast<void **>(&_d3d));
	}
	if (SUCCEEDED(result)) {
		result = _ddraw->CreateClipper(0, &_clipper, nullptr);
	}
	if (SUCCEEDED(result)) {
		result = _clipper->SetHWnd(0, _window);
	}
	_resetting = false;
	Error error = _check_result(result);
	if (error == OK) {
		error = _reset();
	}
	if (error == OK) {
		// Bootstrap within the original limit before querying the selected device's caps.
		const Size2i screen = D3D7ResolutionPolicy::get_screen_size(_window);
		const uint32_t old_limit = _resolution.get_limit();
		error = _resolution.configure(_capabilities.max_texture_width, _capabilities.max_texture_height, screen);
		if (error == OK && old_limit != _resolution.get_limit() &&
				_resolution.get_render_size(_requested_width, _requested_height, _capabilities.max_texture_width, _capabilities.max_texture_height) != _native_size) {
			error = _reset();
		}
	}
	if (error != OK) {
		finalize();
	}
	if (error == OK) {
		DDDEVICEIDENTIFIER2 identifier = {};
		if (SUCCEEDED(_ddraw->GetDeviceIdentifier(&identifier, 0))) {
			identifier.szDescription[MAX_DDDEVICEID_STRING - 1] = 0;
			WCHAR description[MAX_DDDEVICEID_STRING];
			if (MultiByteToWideChar(CP_ACP, 0, identifier.szDescription, -1, description, MAX_DDDEVICEID_STRING)) {
				_adapter_name = description;
			}
			_adapter_vendor_id = identifier.dwVendorId;
		}
	}
	return error;
}

Error FixedFunctionGraphicsDeviceD3D7::texture_get_info(RID p_texture, FFTextureInfo &r_info) const {
	if (!_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D7Texture *texture = _texture_owner.get_or_null(p_texture);
	FFTextureInfo info;
	info.size = Size2i(texture->width, texture->height);
	const uint32_t bits = texture->render_target ? _render_target_format.dwRGBBitCount : _texture_format.dwRGBBitCount;
	if (texture->native) {
		const Image::Format format = Image::Format(texture->format);
		info.bytes = Image::get_image_data_size(texture->native_size.x, texture->native_size.y, format, texture->mip_levels > 1);
		if (format == Image::FORMAT_RGBA8) {
			info.bytes = info.bytes * bits / 32;
		}
		if (texture->depth) {
			info.bytes += uint64_t(texture->native_size.x) * texture->native_size.y * texture->depth_pixel_size;
		}
	}
	r_info = info;
	return OK;
}

uint64_t FixedFunctionGraphicsDeviceD3D7::get_texture_memory_usage() const {
	uint64_t bytes = 0;
	for (const List<D3D7Texture *>::Element *E = _textures.front(); E; E = E->next()) {
		FFTextureInfo info;
		texture_get_info(E->get()->rid, info);
		bytes += MIN(info.bytes, UINT64_MAX - bytes);
	}
	return bytes;
}

void FixedFunctionGraphicsDeviceD3D7::_release_surfaces() {
	if (_device) {
		if (_frame_active) {
			_device->EndScene();
		}
		_device->SetTexture(0, nullptr);
		_release_vertex_stream();
		_release_geometry_buffers();
		_device->Release();
		_device = nullptr;
		// A new device starts with every light disabled.
		_enabled_vertex_lights = 0;
	}
	_rgb_device = false;
	for (List<D3D7Texture *>::Element *E = _textures.front(); E; E = E->next()) {
		D3D7Texture *texture = E->get();
		if (texture->render_target) {
			texture->rendered = false;
		}
		_release_depth(texture->native, texture->depth);
		texture->depth_pixel_size = 0;
		if (texture->native) {
			texture->native->Release();
			texture->native = nullptr;
		}
	}
	_render_target = RID();
	_release_depth(_target, _window_depth);
	if (_target) {
		_target->Release();
		_target = nullptr;
	}
	if (_primary) {
		_primary->Release();
		_primary = nullptr;
	}
	_frame_active = _depth_active = _frame_readable = _window_rendered = false;
}

Error FixedFunctionGraphicsDeviceD3D7::finalize() {
	if (_owner_thread && _check_thread() != OK) {
		return ERR_BUG;
	}
	if (_resetting) {
		return ERR_BUSY;
	}
	_release_surfaces();
	_vertex_lit_data.clear();
	while (!_geometries.is_empty()) {
		_free_geometry(_geometries.front()->get());
	}
	while (!_textures.is_empty()) {
		D3D7Texture *texture = _textures.front()->get();
		_texture_owner.free(texture->rid);
		_textures.pop_front();
		memdelete(texture);
	}
	if (_clipper) {
		_clipper->Release();
		_clipper = nullptr;
	}
	if (_d3d) {
		_d3d->Release();
		_d3d = nullptr;
	}
	if (_ddraw) {
		_ddraw->Release();
		_ddraw = nullptr;
	}
	if (_library) {
		FreeLibrary(_library);
		_library = nullptr;
	}
	_window = nullptr;
	_owner_thread = 0;
	_requested_width = _requested_height = _width = _height = 0;
	_native_size = Size2i();
	_capabilities = FFCapabilities();
	_adapter_name = String();
	_adapter_vendor_id = 0;
	_render_bit_depth = 0;
	_texture_format = DDPIXELFORMAT();
	memset(_compressed_texture_formats, 0, sizeof(_compressed_texture_formats));
	_render_target_format = DDPIXELFORMAT();
	_depth_formats[0] = _depth_formats[1] = DDPIXELFORMAT();
	_hardware_rasterizer = false;
	_reset_pending = false;
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D7::resize(const uint32_t p_width, const uint32_t p_height) {
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

Error FixedFunctionGraphicsDeviceD3D7::set_use_vsync(const bool p_enabled) {
	Error error = _check_thread();
	if (error == OK) {
		_vsync = p_enabled;
	}
	return error;
}

Error FixedFunctionGraphicsDeviceD3D7::_reset() {
	const uint32_t new_width = _requested_width;
	const uint32_t new_height = _requested_height;
	const Size2i native_size = _resolution.get_render_size(new_width, new_height, _capabilities.max_texture_width, _capabilities.max_texture_height);
	if (!native_size.x || !native_size.y) {
		return ERR_INVALID_PARAMETER;
	}
	_resetting = true;
	_reset_pending = true;
	// Recreate the windowed surfaces, including after desktop format changes.
	// Texture RIDs and CPU snapshots survive; native surfaces are recreated below.
	_release_surfaces();
	DDSURFACEDESC2 description = {};
	description.dwSize = sizeof(description);
	description.dwFlags = DDSD_CAPS;
	description.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	HRESULT result = _ddraw->CreateSurface(&description, &_primary, nullptr);
	if (SUCCEEDED(result)) {
		result = _primary->SetClipper(_clipper);
	}
	if (SUCCEEDED(result)) {
		const GUID *device_types[] = { &IID_IDirect3DTnLHalDevice, &IID_IDirect3DHALDevice, &IID_IDirect3DRGBDevice };
		const size_t first_device_type = _force_rgb ? 2 : 0;
		for (size_t i = first_device_type; i < sizeof(device_types) / sizeof(device_types[0]); i++) {
			description = DDSURFACEDESC2();
			description.dwSize = sizeof(description);
			description.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
			description.dwWidth = native_size.x;
			description.dwHeight = native_size.y;
			description.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE | (i == 2 ? DDSCAPS_SYSTEMMEMORY : DDSCAPS_VIDEOMEMORY);
			result = _ddraw->CreateSurface(&description, &_target, nullptr);
			if (SUCCEEDED(result)) {
				// Preserve the caller's FPU precision, including double-precision engine builds.
				result = _d3d->CreateDevice(*device_types[i], _target, &_device);
			}
			if (SUCCEEDED(result)) {
				break;
			}
			if (_target) {
				_target->Release();
				_target = nullptr;
			}
		}
	}
	D3DDEVICEDESC7 caps = {};
	if (SUCCEEDED(result)) {
		result = _device->GetCaps(&caps);
	}
	if (SUCCEEDED(result)) {
		_rgb_device = IsEqualGUID(caps.deviceGUID, IID_IDirect3DRGBDevice);
	}
	_texture_format = DDPIXELFORMAT();
	memset(_compressed_texture_formats, 0, sizeof(_compressed_texture_formats));
	if (SUCCEEDED(result)) {
		result = _device->EnumTextureFormats(_select_texture_format, this);
	}
	_render_target_format = DDPIXELFORMAT();
	if (SUCCEEDED(result)) {
		_render_bit_depth = caps.dwDeviceRenderBitDepth;
		result = _device->EnumTextureFormats(_select_render_target_format, this);
	}
	Error error = _check_result(result);
	if (error == OK) {
		_depth_formats[0] = _depth_formats[1] = DDPIXELFORMAT();
		// Optional depth support must not prevent a color-only device from initializing.
		if ((caps.dpcTriCaps.dwZCmpCaps & D3DPCMPCAPS_LESSEQUAL) && (caps.dpcTriCaps.dwZCmpCaps & D3DPCMPCAPS_ALWAYS)) {
			if (FAILED(_d3d->EnumZBufferFormats(caps.deviceGUID, _select_depth_format, this))) {
				_depth_formats[0] = _depth_formats[1] = DDPIXELFORMAT();
			}
		}
		_capabilities.depth_buffer = (_depth_formats[0].dwFlags & DDPF_ZBUFFER) || (_depth_formats[1].dwFlags & DDPF_ZBUFFER);
		_fog_raster_caps = caps.dpcTriCaps.dwRasterCaps;
		_capabilities.max_texture_width = caps.dwMaxTextureWidth;
		_capabilities.max_texture_height = caps.dwMaxTextureHeight;
		_capabilities.max_texture_stages = caps.wMaxSimultaneousTextures < caps.wMaxTextureBlendStages ? caps.wMaxSimultaneousTextures : caps.wMaxTextureBlendStages;
		_capabilities.max_primitives_per_draw = D3DMAXNUMVERTICES / 3;
		_capabilities.hardware_transform_and_light = (caps.dwDevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;
		_vertex_light_limit = 0;
		if ((caps.dwVertexProcessingCaps & D3DVTXPCAPS_DIRECTIONALLIGHTS) && (caps.dwVertexProcessingCaps & D3DVTXPCAPS_POSITIONALLIGHTS)) {
			_vertex_light_limit = MIN(caps.dwMaxActiveLights, DWORD(MAX_PIXEL_LIGHTS));
		}
		_capabilities.texture_power_of_two = (caps.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_POW2) != 0;
		_capabilities.texture_square_only = (caps.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0;
		_capabilities.blend_mix = (caps.dpcTriCaps.dwSrcBlendCaps & D3DPBLENDCAPS_SRCALPHA) && (caps.dpcTriCaps.dwDestBlendCaps & D3DPBLENDCAPS_INVSRCALPHA);
		_capabilities.blend_premultiplied_alpha = (caps.dpcTriCaps.dwSrcBlendCaps & D3DPBLENDCAPS_ONE) && (caps.dpcTriCaps.dwDestBlendCaps & D3DPBLENDCAPS_INVSRCALPHA);
		_capabilities.blend_add = (caps.dpcTriCaps.dwSrcBlendCaps & D3DPBLENDCAPS_SRCALPHA) && (caps.dpcTriCaps.dwDestBlendCaps & D3DPBLENDCAPS_ONE);
		_capabilities.blend_add_unweighted = (caps.dpcTriCaps.dwSrcBlendCaps & D3DPBLENDCAPS_ONE) && (caps.dpcTriCaps.dwDestBlendCaps & D3DPBLENDCAPS_ONE);
		_capabilities.blend_mul = (caps.dpcTriCaps.dwSrcBlendCaps & D3DPBLENDCAPS_DESTCOLOR) && (caps.dpcTriCaps.dwDestBlendCaps & D3DPBLENDCAPS_ZERO);
		_capabilities.cull_back = (caps.dpcTriCaps.dwMiscCaps & D3DPMISCCAPS_CULLCCW) != 0;
		_capabilities.cull_front = (caps.dpcTriCaps.dwMiscCaps & D3DPMISCCAPS_CULLCW) != 0;
		_capabilities.alpha_cutoff = (caps.dpcTriCaps.dwAlphaCmpCaps & D3DPCMPCAPS_GREATEREQUAL) != 0;
		_capabilities.sampler_filter_linear = (caps.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) && (caps.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR);
		_capabilities.sampler_max_anisotropy = 1;
		if ((caps.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) && caps.dwMaxAnisotropy > 1) {
			_capabilities.sampler_max_anisotropy = uint8_t(MIN(caps.dwMaxAnisotropy, DWORD(255)));
		}
		_capabilities.sampler_mip_filters = MIP_FILTER_NONE;
		if (caps.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_MIPFPOINT) {
			_capabilities.sampler_mip_filters |= MIP_FILTER_POINT;
		}
		if (caps.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_MIPFLINEAR) {
			_capabilities.sampler_mip_filters |= MIP_FILTER_LINEAR;
		}
		_capabilities.sampler_repeat = (caps.dpcTriCaps.dwTextureAddressCaps & D3DPTADDRESSCAPS_WRAP) != 0;
		_capabilities.sampler_repeat_mirrored = (caps.dpcTriCaps.dwTextureAddressCaps & D3DPTADDRESSCAPS_MIRROR) != 0;
		_capabilities.sampler_address_independent = (caps.dpcTriCaps.dwTextureAddressCaps & D3DPTADDRESSCAPS_INDEPENDENTUV) != 0;
		_capabilities.render_target_opaque = _capabilities.max_texture_stages && _render_target_format_rank(_render_target_format) != 0;
		_capabilities.render_target_scaled = _capabilities.render_target_opaque;
		_hardware_rasterizer = (caps.dwDevCaps & D3DDEVCAPS_HWRASTERIZATION) != 0;
		for (List<D3D7Texture *>::Element *E = _textures.front(); E; E = E->next()) {
			error = _restore_texture(E->get());
			if (error != OK) {
				break;
			}
		}
	}
	_resetting = false;
	if (error == OK) {
		_width = new_width;
		_height = new_height;
		_native_size = native_size;
		_reset_pending = _requested_width != _width || _requested_height != _height;
	}
	if (error != OK && error != ERR_SKIP && (native_size.x > D3D7ResolutionPolicy::UNPATCHED_LIMIT || native_size.y > D3D7ResolutionPolicy::UNPATCHED_LIMIT)) {
		// Advertised texture caps do not guarantee equally large render targets.
		_resolution.use_stretch_fallback();
		return _reset();
	}
	return error;
}

HRESULT CALLBACK FixedFunctionGraphicsDeviceD3D7::_select_depth_format(DDPIXELFORMAT *p_format, void *p_context) {
	if (!(p_format->dwFlags & DDPF_ZBUFFER) || (p_format->dwZBufferBitDepth != 16 && p_format->dwZBufferBitDepth != 32) || (p_format->dwZBufferBitDepth == 16 && (p_format->dwFlags & DDPF_STENCILBUFFER))) {
		return D3DENUMRET_OK;
	}
	FixedFunctionGraphicsDeviceD3D7 *device = static_cast<FixedFunctionGraphicsDeviceD3D7 *>(p_context);
	DDPIXELFORMAT &selected = device->_depth_formats[p_format->dwZBufferBitDepth == 16 ? 1 : 0];
	if (!(selected.dwFlags & DDPF_ZBUFFER) || ((selected.dwFlags & DDPF_STENCILBUFFER) && !(p_format->dwFlags & DDPF_STENCILBUFFER))) {
		selected = *p_format;
	}
	return D3DENUMRET_OK;
}

void FixedFunctionGraphicsDeviceD3D7::_release_depth(IDirectDrawSurface7 *p_color, IDirectDrawSurface7 *&r_depth) {
	if (r_depth) {
		// Explicit attachments hold an additional COM reference; release both owners.
		p_color->DeleteAttachedSurface(0, r_depth);
		r_depth->Release();
		r_depth = nullptr;
	}
}

Error FixedFunctionGraphicsDeviceD3D7::_bind_depth_target(IDirectDrawSurface7 *p_color, IDirectDrawSurface7 *&r_depth, uint8_t &r_pixel_size) {
	if (r_depth) {
		D3D7_FAIL_ON_ERROR(r_depth->IsLost());
		return _check_result(_device->SetRenderTarget(p_color, 0));
	}
	DDSURFACEDESC2 color = {};
	color.dwSize = sizeof(color);
	D3D7_FAIL_ON_ERROR(p_color->GetSurfaceDesc(&color));
	Error error = ERR_UNAVAILABLE;
	for (int i = 0; i < 2; i++) {
		if (!(_depth_formats[i].dwFlags & DDPF_ZBUFFER)) {
			continue;
		}
		DDSURFACEDESC2 description = {};
		description.dwSize = sizeof(description);
		description.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
		description.dwWidth = color.dwWidth;
		description.dwHeight = color.dwHeight;
		description.ddpfPixelFormat = _depth_formats[i];
		description.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | (_hardware_rasterizer ? DDSCAPS_VIDEOMEMORY : DDSCAPS_SYSTEMMEMORY);
		IDirectDrawSurface7 *depth = nullptr;
		HRESULT result = _ddraw->CreateSurface(&description, &depth, nullptr);
		bool attached = false;
		if (SUCCEEDED(result)) {
			result = p_color->AddAttachedSurface(depth);
			attached = SUCCEEDED(result);
		}
		if (SUCCEEDED(result)) {
			result = _device->SetRenderTarget(p_color, 0);
		}
		error = _check_result(result);
		if (error == OK) {
			r_depth = depth;
			r_pixel_size = _depth_formats[i].dwZBufferBitDepth / 8;
			return OK;
		}
		if (attached) {
			p_color->DeleteAttachedSurface(0, depth);
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

int FixedFunctionGraphicsDeviceD3D7::_texture_format_rank(const DDPIXELFORMAT &p_format) {
	if ((p_format.dwFlags & (DDPF_RGB | DDPF_ALPHAPIXELS)) != (DDPF_RGB | DDPF_ALPHAPIXELS)) {
		return 0;
	}
	if (p_format.dwRGBBitCount == 32 && p_format.dwRBitMask == 0xff0000 && p_format.dwGBitMask == 0xff00 && p_format.dwBBitMask == 0xff && p_format.dwRGBAlphaBitMask == 0xff000000) {
		return 3;
	}
	if (p_format.dwRGBBitCount == 16 && p_format.dwRBitMask == 0xf00 && p_format.dwGBitMask == 0xf0 && p_format.dwBBitMask == 0xf && p_format.dwRGBAlphaBitMask == 0xf000) {
		return 2;
	}
	if (p_format.dwRGBBitCount == 16 && p_format.dwRBitMask == 0x7c00 && p_format.dwGBitMask == 0x3e0 && p_format.dwBBitMask == 0x1f && p_format.dwRGBAlphaBitMask == 0x8000) {
		return 1;
	}
	return 0;
}

HRESULT CALLBACK FixedFunctionGraphicsDeviceD3D7::_select_texture_format(DDPIXELFORMAT *p_format, void *p_context) {
	FixedFunctionGraphicsDeviceD3D7 *device = static_cast<FixedFunctionGraphicsDeviceD3D7 *>(p_context);
	if (p_format->dwFlags & DDPF_FOURCC) {
		for (unsigned i = 0; i < sizeof(D3D7_DXT_FORMATS) / sizeof(D3D7_DXT_FORMATS[0]); i++) {
			if (p_format->dwFourCC == D3D7_DXT_FORMATS[i]) {
				device->_compressed_texture_formats[i] = *p_format;
				break;
			}
		}
	} else if (_texture_format_rank(*p_format) > _texture_format_rank(device->_texture_format)) {
		device->_texture_format = *p_format;
	}
	return D3DENUMRET_OK;
}

int FixedFunctionGraphicsDeviceD3D7::_render_target_format_rank(const DDPIXELFORMAT &p_format) {
	if (!(p_format.dwFlags & DDPF_RGB) || (p_format.dwFlags & DDPF_ALPHAPIXELS)) {
		return 0;
	}
	if (p_format.dwRGBBitCount == 32 && p_format.dwRBitMask == 0xff0000 && p_format.dwGBitMask == 0xff00 && p_format.dwBBitMask == 0xff) {
		return 3;
	}
	if (p_format.dwRGBBitCount == 16 && p_format.dwRBitMask == 0xf800 && p_format.dwGBitMask == 0x7e0 && p_format.dwBBitMask == 0x1f) {
		return 2;
	}
	if (p_format.dwRGBBitCount == 16 && p_format.dwRBitMask == 0x7c00 && p_format.dwGBitMask == 0x3e0 && p_format.dwBBitMask == 0x1f) {
		return 1;
	}
	return 0;
}

HRESULT CALLBACK FixedFunctionGraphicsDeviceD3D7::_select_render_target_format(DDPIXELFORMAT *p_format, void *p_context) {
	FixedFunctionGraphicsDeviceD3D7 *device = static_cast<FixedFunctionGraphicsDeviceD3D7 *>(p_context);
	// A sampleable texture format is not sufficient: this device must render at its bit depth.
	if ((p_format->dwRGBBitCount == 32 && !(device->_render_bit_depth & DDBD_32)) ||
			(p_format->dwRGBBitCount == 16 && !(device->_render_bit_depth & DDBD_16))) {
		return D3DENUMRET_OK;
	}
	if (_render_target_format_rank(*p_format) > _render_target_format_rank(device->_render_target_format)) {
		device->_render_target_format = *p_format;
	}
	return D3DENUMRET_OK;
}

bool FixedFunctionGraphicsDeviceD3D7::supports_compressed_texture(Image::Format p_format) const {
	return p_format >= Image::FORMAT_DXT1 && p_format <= Image::FORMAT_DXT5 &&
			(_compressed_texture_formats[p_format - Image::FORMAT_DXT1].dwFlags & DDPF_FOURCC) != 0;
}

Error FixedFunctionGraphicsDeviceD3D7::_restore_texture(D3D7Texture *p_texture) {
	const Error error = _upload_texture(p_texture);
	const Image::Format format = Image::Format(p_texture->format);
	if (format == Image::FORMAT_RGBA8 || (error != ERR_UNAVAILABLE && error != FAILED && error != ERR_OUT_OF_MEMORY)) {
		return error;
	}
	// The recreated device can refuse a format it previously accepted. Retain the blocks
	// until a decoded replacement succeeds, so a transient failure can be retried.
	const Vector<uint8_t> blocks = p_texture->data;
	const Ref<Image> source = Image::create_from_data(p_texture->upload_size.x, p_texture->upload_size.y, p_texture->source_mip_levels > 1, format, blocks);
	Ref<Image> decoded;
	const Error decode_error = prepare_rgba8_upload(source, decoded);
	if (decode_error != OK) {
		return decode_error;
	}
	p_texture->data = decoded->get_data();
	p_texture->format = Image::FORMAT_RGBA8;
	const Error upload_error = _upload_texture(p_texture);
	if (upload_error != OK) {
		p_texture->data = blocks;
		p_texture->format = format;
	}
	return upload_error;
}

Error FixedFunctionGraphicsDeviceD3D7::_upload_texture(D3D7Texture *p_texture) {
	Size2i native_size = p_texture->upload_size;
	if (p_texture->render_target) {
		const Size2i requested = _resolution.get_render_size(p_texture->width, p_texture->height, _capabilities.max_texture_width, _capabilities.max_texture_height);
		const Error adapted = get_texture_upload_size(requested.x, requested.y, native_size);
		if (adapted != OK) {
			return adapted;
		}
	}
	Error error = validate_texture_size(native_size.x, native_size.y);
	if (error != OK) {
		return error;
	}
	const Image::Format format = Image::Format(p_texture->format);
	const bool compressed = format != Image::FORMAT_RGBA8;
	const DDPIXELFORMAT *native_format = nullptr;
	if (p_texture->render_target && _render_target_format_rank(_render_target_format)) {
		native_format = &_render_target_format;
	} else if (compressed && supports_compressed_texture(format)) {
		native_format = &_compressed_texture_formats[format - Image::FORMAT_DXT1];
	} else if (!p_texture->render_target && !compressed && _texture_format_rank(_texture_format)) {
		native_format = &_texture_format;
	}
	if (!native_format) {
		return ERR_UNAVAILABLE;
	}
	DDSURFACEDESC2 description = {};
	description.dwSize = sizeof(description);
	description.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	description.dwWidth = native_size.x;
	description.dwHeight = native_size.y;
	description.ddpfPixelFormat = *native_format;
	description.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
	if (p_texture->render_target) {
		description.ddsCaps.dwCaps |= DDSCAPS_3DDEVICE | (_hardware_rasterizer ? DDSCAPS_VIDEOMEMORY : DDSCAPS_SYSTEMMEMORY);
	} else if (_hardware_rasterizer) {
		description.ddsCaps.dwCaps2 = DDSCAPS2_D3DTEXTUREMANAGE;
	} else {
		description.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
	}
	uint8_t levels = _capabilities.sampler_mip_filters ? p_texture->source_mip_levels : 1;
	if (levels > 1) {
		description.dwFlags |= DDSD_MIPMAPCOUNT;
		description.dwMipMapCount = levels;
		description.ddsCaps.dwCaps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;
	}
	for (;;) {
		HRESULT result = _ddraw->CreateSurface(&description, &p_texture->native, nullptr);
		if (SUCCEEDED(result) && !p_texture->render_target) {
			result = _upload_texture_levels(p_texture, levels);
		}
		error = _check_result(result);
		if (error == OK) {
			p_texture->native_size = native_size;
			p_texture->mip_levels = levels;
			p_texture->rendered = false;
			return OK;
		}
		if (p_texture->native) {
			p_texture->native->Release();
			p_texture->native = nullptr;
		}
		if (levels == 1 || compressed || error == ERR_SKIP) {
			return error;
		}
		levels = 1;
		description.dwFlags &= ~DDSD_MIPMAPCOUNT;
		description.dwMipMapCount = 0;
		description.ddsCaps.dwCaps &= ~(DDSCAPS_COMPLEX | DDSCAPS_MIPMAP);
	}
}

HRESULT FixedFunctionGraphicsDeviceD3D7::_upload_texture_levels(const D3D7Texture *p_texture, uint8_t p_levels) {
	const uint8_t *source = p_texture->data.ptr();
	const Image::Format format = Image::Format(p_texture->format);
	const bool compressed = format != Image::FORMAT_RGBA8;
	const uint32_t pixel_size = _texture_format.dwRGBBitCount / 8;
	IDirectDrawSurface7 *surface = p_texture->native;
	uint32_t width = p_texture->upload_size.x, height = p_texture->upload_size.y;
	size_t offset = 0;
	HRESULT result = DD_OK;
	for (int level = 0; level < p_levels; level++) {
		DDSURFACEDESC2 description = {};
		description.dwSize = sizeof(description);
		result = surface->Lock(nullptr, &description, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr);
		if (FAILED(result)) {
			break;
		}
		bool copied = description.dwWidth == width && description.dwHeight == height;
		if (copied && compressed) {
			const uint32_t row_bytes = Image::get_image_data_size(width, 1, format, false);
			const uint32_t rows = (height + 3) / 4;
			int pitch = 0;
			if (description.dwFlags & DDSD_LINEARSIZE) {
				// Lock can report the byte count in the same field as pitch. Use packed
				// block rows only when the reported byte count confirms that layout.
				if (description.dwLinearSize == row_bytes * rows) {
					pitch = row_bytes;
				}
			} else if (description.dwFlags & DDSD_PITCH) {
				pitch = description.lPitch;
			}
			copied = copy_compressed_texture_rows(description.lpSurface, pitch, source + offset, row_bytes, rows);
		}
		for (uint32_t y = 0; copied && !compressed && y < height; y++) {
			uint8_t *row = static_cast<uint8_t *>(description.lpSurface) + ptrdiff_t(y) * description.lPitch;
			for (uint32_t x = 0; x < width; x++) {
				const uint8_t *rgba = source + offset + (size_t(y) * width + x) * 4;
				uint32_t pixel;
				if (pixel_size == 4) {
					pixel = (uint32_t(rgba[3]) << 24) | (uint32_t(rgba[0]) << 16) | (uint32_t(rgba[1]) << 8) | rgba[2];
				} else if (_texture_format.dwRGBAlphaBitMask == 0xf000) {
					pixel = ((rgba[3] >> 4) << 12) | ((rgba[0] >> 4) << 8) | ((rgba[1] >> 4) << 4) | (rgba[2] >> 4);
				} else {
					pixel = ((rgba[3] >> 7) << 15) | ((rgba[0] >> 3) << 10) | ((rgba[1] >> 3) << 5) | (rgba[2] >> 3);
				}
				memcpy(row + size_t(x) * pixel_size, &pixel, pixel_size);
			}
		}
		result = surface->Unlock(nullptr);
		if (SUCCEEDED(result) && !copied) {
			result = DDERR_UNSUPPORTED;
		}
		if (FAILED(result) || level + 1 == p_levels) {
			break;
		}
		DDSCAPS2 mip_caps = {};
		mip_caps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
		IDirectDrawSurface7 *next = nullptr;
		result = surface->GetAttachedSurface(&mip_caps, &next);
		if (FAILED(result)) {
			break;
		}
		if (surface != p_texture->native) {
			surface->Release();
		}
		surface = next;
		offset += Image::get_image_data_size(width, height, format, false);
		width = MAX(width >> 1, 1u);
		height = MAX(height >> 1, 1u);
	}
	if (surface != p_texture->native) {
		surface->Release();
	}
	return result;
}

Error FixedFunctionGraphicsDeviceD3D7::texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent) {
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
	if (!p_width || !p_height || p_width > INT32_MAX || p_height > INT32_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_transparent || !_capabilities.render_target_opaque) {
		return ERR_UNAVAILABLE;
	}
	D3D7_FAIL_ON_ERROR(_ddraw->TestCooperativeLevel());
	D3D7Texture *texture = memnew(D3D7Texture);
	texture->width = p_width;
	texture->height = p_height;
	texture->render_target = true;
	error = _upload_texture(texture);
	if (error != OK) {
		memdelete(texture);
		return error;
	}
	texture->rid = _texture_owner.make_rid(texture);
	_textures.push_back(texture);
	r_texture = texture->rid;
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D7::texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame) {
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
	D3D7_FAIL_ON_ERROR(_ddraw->TestCooperativeLevel());
	D3D7Texture *texture = memnew(D3D7Texture);
	texture->width = p_image->get_width();
	texture->height = p_image->get_height();
	texture->upload_size = Size2i(upload->get_width(), upload->get_height());
	texture->data = upload->get_data();
	texture->source_mip_levels = upload->get_mipmap_count() + 1;
	texture->format = upload->get_format();
	error = _upload_texture(texture);
	if (error != OK) {
		memdelete(texture);
		return error;
	}
	texture->rid = _texture_owner.make_rid(texture);
	_textures.push_back(texture);
	r_texture = texture->rid;
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D7::texture_free(RID p_texture) {
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
	D3D7Texture *texture = _texture_owner.get_or_null(p_texture);
	if (p_texture == _render_target) {
		D3D7_FAIL_ON_ERROR(_restore_window_target());
	}
	if (_device) {
		_device->SetTexture(0, nullptr);
	}
	_release_depth(texture->native, texture->depth);
	if (texture->native) {
		texture->native->Release();
	}
	_texture_owner.free(p_texture);
	_textures.erase(texture);
	memdelete(texture);
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D7::_restore_window_target() {
	if (!_render_target.is_valid()) {
		return OK;
	}
	const Error error = _check_result(_device->SetRenderTarget(_target, 0));
	if (error == OK) {
		_render_target = RID();
	}
	return error;
}

bool FixedFunctionGraphicsDeviceD3D7::texture_is_render_target_ready(RID p_texture) const {
	if (_check_thread() != OK || _resetting || _reset_pending || !_texture_owner.owns(p_texture) || _render_target == p_texture) {
		return false;
	}
	const D3D7Texture *texture = _texture_owner.get_or_null(p_texture);
	return texture->render_target && texture->native && texture->rendered;
}

Error FixedFunctionGraphicsDeviceD3D7::begin_frame(const uint32_t p_clear_color, RID p_render_target, bool p_clear, bool p_use_depth) {
	const Error error = _begin_frame(p_clear_color, p_render_target, p_clear, p_use_depth);
	if (error != OK && _check_thread() == OK && !_resetting && !_frame_active && _render_target.is_valid()) {
		_restore_window_target();
	}
	return error;
}

Error FixedFunctionGraphicsDeviceD3D7::prepare_resources() {
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
	HRESULT cooperative = _ddraw->TestCooperativeLevel();
	if (cooperative == DDERR_WRONGMODE) {
		_reset_pending = true;
	} else if (FAILED(cooperative)) {
		return _check_result(cooperative);
	}
	if (!_reset_pending) {
		error = _check_result(_primary->IsLost());
		if (error == OK) {
			error = _check_result(_target->IsLost());
		}
		if (error != OK && !_reset_pending) {
			return error;
		}
	}
	if (_reset_pending) {
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

Error FixedFunctionGraphicsDeviceD3D7::_begin_frame(uint32_t p_clear_color, RID p_render_target, bool p_clear, bool p_use_depth) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active || _resetting) {
		return ERR_BUSY;
	}
	if (p_render_target.is_valid() && (!_texture_owner.owns(p_render_target) || !_texture_owner.get_or_null(p_render_target)->render_target)) {
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
	if (!p_clear && (p_render_target.is_valid() ? !_texture_owner.get_or_null(p_render_target)->rendered : !_window_rendered)) {
		return ERR_UNCONFIGURED;
	}
	D3D7_FAIL_ON_ERROR(_device->SetTexture(0, nullptr));
	bool new_depth = false;
	if (p_render_target.is_valid()) {
		D3D7Texture *texture = _texture_owner.get_or_null(p_render_target);
		if (!texture->native) {
			return ERR_UNCONFIGURED;
		}
		if (p_use_depth) {
			new_depth = texture->depth == nullptr;
			error = _bind_depth_target(texture->native, texture->depth, texture->depth_pixel_size);
			if (error != OK) {
				return error;
			}
		} else {
			D3D7_FAIL_ON_ERROR(_device->SetRenderTarget(texture->native, 0));
		}
		_render_target = p_render_target;
		_render_width = texture->width;
		_render_height = texture->height;
		texture->rendered = false;
	} else {
		if (p_use_depth) {
			new_depth = _window_depth == nullptr;
			uint8_t pixel_size = 0;
			error = _bind_depth_target(_target, _window_depth, pixel_size);
			if (error != OK) {
				return error;
			}
		} else {
			D3D7_FAIL_ON_ERROR(_device->SetRenderTarget(_target, 0));
		}
		_render_target = RID();
		_window_rendered = false;
	}
	D3DMATRIX identity = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	const struct {
		D3DRENDERSTATETYPE state;
		DWORD value;
	} states[] = {
		{ D3DRENDERSTATE_LIGHTING, FALSE },
		{ D3DRENDERSTATE_ZENABLE, D3DZB_FALSE },
		{ D3DRENDERSTATE_ZWRITEENABLE, FALSE },
		{ D3DRENDERSTATE_CULLMODE, D3DCULL_NONE },
		{ D3DRENDERSTATE_CLIPPING, TRUE },
		{ D3DRENDERSTATE_ALPHABLENDENABLE, FALSE },
		{ D3DRENDERSTATE_ALPHATESTENABLE, FALSE },
		{ D3DRENDERSTATE_FOGENABLE, FALSE },
		{ D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD },
	};
	for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
		D3D7_FAIL_ON_ERROR(_device->SetRenderState(states[i].state, states[i].value));
	}
	const Size2i native_size = _get_native_target_size();
	D3DVIEWPORT7 viewport = { 0, 0, DWORD(native_size.x), DWORD(native_size.y), 0.0f, 1.0f };
	D3D7_FAIL_ON_ERROR(_device->SetViewport(&viewport));
	D3D7_FAIL_ON_ERROR(_device->SetTransform(D3DTRANSFORMSTATE_WORLD, &identity));
	D3D7_FAIL_ON_ERROR(_device->SetTransform(D3DTRANSFORMSTATE_VIEW, &identity));
	D3D7_FAIL_ON_ERROR(_device->SetTexture(0, nullptr));
	// RGB software modulation requires the texture in argument one.
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2));
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE));
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE));
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2));
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE));
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE));
	_color_source = COLOR_SOURCE_DIFFUSE;
	_fog_enabled = false;
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0));
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE));
	error = _set_sampler_state(0);
	if (error != OK) {
		return error;
	}
	if (_capabilities.max_texture_stages > 1) {
		D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE));
	}
	if (p_clear || new_depth) {
		const DWORD flags = (p_clear ? D3DCLEAR_TARGET : 0) | (p_use_depth ? D3DCLEAR_ZBUFFER : 0);
		D3D7_FAIL_ON_ERROR(_device->Clear(0, nullptr, flags, p_clear_color | 0xff000000u, 1.0f, 0));
	}
	error = _check_result(_device->BeginScene());
	_frame_active = error == OK;
	_depth_active = _frame_active && p_use_depth;
	return error;
}

Error FixedFunctionGraphicsDeviceD3D7::clear_color(uint32_t p_color) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_frame_active) {
		return ERR_UNCONFIGURED;
	}
	const Size2i native_size = _get_native_target_size();
	D3DVIEWPORT7 viewport = { 0, 0, DWORD(native_size.x), DWORD(native_size.y), 0.0f, 1.0f };
	D3D7_FAIL_ON_ERROR(_device->SetViewport(&viewport));
	return _check_result(_device->Clear(0, nullptr, D3DCLEAR_TARGET, p_color | 0xff000000u, 1.0f, 0));
}

Error FixedFunctionGraphicsDeviceD3D7::clear_depth(float p_depth) {
	Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_frame_active || !_depth_active) {
		return ERR_UNCONFIGURED;
	}
	if (Math::is_nan(p_depth) || Math::is_inf(p_depth) || p_depth < 0 || p_depth > 1) {
		return ERR_INVALID_PARAMETER;
	}
	const Size2i size = _get_native_target_size();
	D3DVIEWPORT7 viewport = { 0, 0, DWORD(size.x), DWORD(size.y), 0.0f, 1.0f };
	D3D7_FAIL_ON_ERROR(_device->SetViewport(&viewport));
	return _check_result(_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, p_depth, 0));
}

Error FixedFunctionGraphicsDeviceD3D7::_set_sampler_state(uint8_t p_flags, uint8_t p_levels, uint32_t p_stage) {
	const uint8_t anisotropy = get_sampler_anisotropy(p_flags);
	D3DTEXTUREMINFILTER min_filter = (p_flags & SAMPLER_FILTER_LINEAR) ? D3DTFN_LINEAR : D3DTFN_POINT;
	if (anisotropy > 1) {
		D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(p_stage, D3DTSS_MAXANISOTROPY, anisotropy));
		min_filter = D3DTFN_ANISOTROPIC;
	}
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(p_stage, D3DTSS_MINFILTER, min_filter));
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(p_stage, D3DTSS_MAGFILTER, (p_flags & SAMPLER_FILTER_LINEAR) ? D3DTFG_LINEAR : D3DTFG_POINT));
	const FFMipFilter mip = get_mip_filter(p_flags, p_levels);
	D3DTEXTUREMIPFILTER mip_filter = D3DTFP_NONE;
	if (mip != MIP_FILTER_NONE) {
		mip_filter = mip == MIP_FILTER_LINEAR ? D3DTFP_LINEAR : D3DTFP_POINT;
	}
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(p_stage, D3DTSS_MIPFILTER, mip_filter));
	const D3DTEXTUREADDRESS repeat = (p_flags & SAMPLER_REPEAT_MIRRORED) ? D3DTADDRESS_MIRROR : D3DTADDRESS_WRAP;
	const D3DTEXTUREADDRESS address_u = (p_flags & SAMPLER_REPEAT_U) ? repeat : D3DTADDRESS_CLAMP;
	const D3DTEXTUREADDRESS address_v = (p_flags & SAMPLER_REPEAT_V) ? repeat : D3DTADDRESS_CLAMP;
	if (_capabilities.sampler_address_independent) {
		D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(p_stage, D3DTSS_ADDRESSU, address_u));
		D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(p_stage, D3DTSS_ADDRESSV, address_v));
	} else {
		// Devices without independent U/V require one shared mode; validation enforces this.
		D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(p_stage, D3DTSS_ADDRESS, address_u));
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D7::_prepare_draw(const void *p_vertices, uint32_t p_vertex_count, RID p_texture, const FFDrawParameters &p_state, Rect2i &r_viewport, bool p_constant_color) {
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
		const D3D7Texture *texture = _texture_owner.get_or_null(p_texture);
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
	const Size2i native_size = _get_native_target_size();
	r_viewport = D3D7ResolutionPolicy::get_native_clip(get_draw_viewport(p_state), Size2i(get_target_width(), get_target_height()), native_size);
	if (!p_vertex_count || !r_viewport.size.x || !r_viewport.size.y || (p_state.alpha_cutoff_enabled && p_state.alpha_cutoff > 1.0f)) {
		r_viewport = Rect2i();
		return OK;
	}
	error = _set_table_fog(p_state.fog, p_state.blend_mode);
	if (error != OK) {
		return error;
	}
	if (_depth_active) {
		D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_ZENABLE, (p_state.depth_test_enabled || p_state.depth_write_enabled) ? D3DZB_TRUE : D3DZB_FALSE));
		D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, p_state.depth_write_enabled));
		D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_ZFUNC, p_state.depth_test_enabled ? D3DCMP_LESSEQUAL : D3DCMP_ALWAYS));
	}
	D3DVIEWPORT7 viewport = { DWORD(r_viewport.position.x), DWORD(r_viewport.position.y), DWORD(r_viewport.size.x), DWORD(r_viewport.size.y), 0.0f, 1.0f };
	D3D7_FAIL_ON_ERROR(_device->SetViewport(&viewport));
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
		D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_SRCBLEND, source));
		D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_DESTBLEND, destination));
	}
	D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, p_state.blend_mode != BLEND_MODE_DISABLED));
	if (p_state.alpha_cutoff_enabled) {
		D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATEREQUAL));
		D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_ALPHAREF, get_alpha_reference(p_state.alpha_cutoff)));
	}
	// Godot clockwise front faces survive CCW culling; clip-depth conversion preserves winding.
	D3DCULL cull_mode = D3DCULL_NONE;
	if (p_state.cull_mode == CULL_BACK) {
		cull_mode = D3DCULL_CCW;
	} else if (p_state.cull_mode == CULL_FRONT) {
		cull_mode = D3DCULL_CW;
	}
	D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_CULLMODE, cull_mode));
	D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, p_state.alpha_cutoff_enabled));
	const D3D7Texture *texture = p_texture.is_valid() ? _texture_owner.get_or_null(p_texture) : nullptr;
	D3D7_FAIL_ON_ERROR(_device->SetTexture(0, texture ? texture->native : nullptr));
	error = _set_sampler_state(p_state.sampler_flags, texture ? texture->mip_levels : 1);
	if (error != OK) {
		return error;
	}
	// Restore only on a transition; ordinary GUI draws need no extra state calls.
	if (!p_constant_color && _color_source != COLOR_SOURCE_DIFFUSE) {
		_color_source = COLOR_SOURCE_DIRTY;
		D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE));
		D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE));
		_color_source = COLOR_SOURCE_DIFFUSE;
	}
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_COLOROP, p_texture.is_valid() && p_state.texture_rgb_enabled ? D3DTOP_MODULATE : D3DTOP_SELECTARG2));
	D3D7_FAIL_ON_ERROR(_device->SetTextureStageState(0, D3DTSS_ALPHAOP, p_texture.is_valid() && p_state.texture_alpha_enabled ? D3DTOP_MODULATE : D3DTOP_SELECTARG2));
	return OK;
}

Projection FixedFunctionGraphicsDeviceD3D7::_get_draw_projection(const Projection &p_projection, const Rect2i &p_viewport) const {
	const Size2i native_size = _get_native_target_size();
	Projection correction;
	// Preserve Godot's handedness; remap depth without Y reversal or reverse Z.
	correction.set_depth_correction(false, false, true);
	// Native D3D pixel centers are integers, rather than Godot's half-pixel centers.
	// Account for quantized native clipping while retaining the logical target's mapping.
	correction.columns[0][0] = real_t(native_size.x) / p_viewport.size.x;
	correction.columns[1][1] = real_t(native_size.y) / p_viewport.size.y;
	correction.columns[3][0] = (real_t(native_size.x) - real_t(2) * p_viewport.position.x - p_viewport.size.x - 1) / p_viewport.size.x;
	correction.columns[3][1] = (real_t(2) * p_viewport.position.y + p_viewport.size.y - native_size.y + 1) / p_viewport.size.y;
	return correction * p_projection;
}

Error FixedFunctionGraphicsDeviceD3D7::_set_table_fog(const FFFog *p_fog, FFBlendMode p_blend_mode) {
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
				D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_FOGTABLEMODE, D3DFOG_LINEAR));
				D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_FOGSTART, native_begin));
				D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_FOGEND, native_end));
				D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_FOGCOLOR, color));
				_fog_begin = begin;
				_fog_end = end;
				_fog_color = color;
				if (!_fog_enabled) {
					D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_FOGENABLE, TRUE));
					_fog_enabled = true;
				}
			}
			return OK;
		}
	}
	if (_fog_enabled) {
		D3D7_FAIL_ON_ERROR(_device->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE));
		_fog_enabled = false;
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D7::draw_triangles(const FixedFunctionVertex *p_vertices, const uint32_t p_vertex_count, const Projection &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state) {
	Rect2i viewport;
	const Error error = _prepare_draw(p_vertices, p_vertex_count, p_texture, p_state, viewport);
	if (error != OK || !viewport.size.x || !viewport.size.y) {
		return error;
	}
	float matrix[16];
	store_camera(_get_draw_projection(p_clip_from_model, viewport), matrix);
	D3DMATRIX projection;
	memcpy(&projection, matrix, sizeof(projection));
	D3D7_FAIL_ON_ERROR(_device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &projection));
	return _check_result(_draw_vertex_stream(p_vertices, p_vertex_count));
}

Error FixedFunctionGraphicsDeviceD3D7::end_frame(const bool p_present, bool p_discard) {
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
		_texture_owner.get_or_null(_render_target)->rendered = error == OK && !p_discard;
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
	if (error != OK) {
		return error;
	}
	if (!p_present || p_discard) {
		_frame_readable = !p_discard;
		return OK;
	}
	RECT destination;
	POINT origin = {};
	if (!GetClientRect(_window, &destination) || !ClientToScreen(_window, &origin)) {
		return _check_result(HRESULT_FROM_WIN32(GetLastError()));
	}
	if (!destination.right || !destination.bottom) {
		return ERR_SKIP;
	}
	OffsetRect(&destination, origin.x, origin.y);
	if (_vsync) {
		// Windowed blits have best-effort synchronization; some drivers cannot wait.
		HRESULT result = _ddraw->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, nullptr);
		if (result != DDERR_UNSUPPORTED && result != DDERR_NOVSYNCHW) {
			D3D7_FAIL_ON_ERROR(result);
		}
	}
	HRESULT result = _primary->Blt(&destination, _target, nullptr, DDBLT_WAIT, nullptr);
	if ((result == DDERR_NOSTRETCHHW || result == DDERR_NOBLTHW || result == DDERR_UNSUPPORTED) &&
			(destination.right - destination.left != _native_size.x || destination.bottom - destination.top != _native_size.y)) {
		// Some old drivers lack stretch blits. Use GDI's opaque scaling path instead.
		HDC source;
		D3D7_FAIL_ON_ERROR(_target->GetDC(&source));
		HDC target = GetDC(_window);
		if (!target) {
			const DWORD dc_error = GetLastError();
			_target->ReleaseDC(source);
			return _check_result(HRESULT_FROM_WIN32(dc_error ? dc_error : ERROR_GEN_FAILURE));
		}
		const int old_mode = SetStretchBltMode(target, COLORONCOLOR);
		const BOOL stretched = StretchBlt(target, 0, 0, destination.right - destination.left, destination.bottom - destination.top,
				source, 0, 0, _native_size.x, _native_size.y, SRCCOPY);
		const DWORD stretch_error = stretched ? ERROR_SUCCESS : GetLastError();
		if (old_mode) {
			SetStretchBltMode(target, old_mode);
		}
		ReleaseDC(_window, target);
		result = _target->ReleaseDC(source);
		if (!stretched) {
			result = HRESULT_FROM_WIN32(stretch_error ? stretch_error : ERROR_GEN_FAILURE);
		}
	}
	return _check_result(result);
}

uint32_t FixedFunctionGraphicsDeviceD3D7::_unpack_channel(uint32_t p_pixel, uint32_t p_mask) {
	return uint32_t((uint64_t(p_pixel & p_mask) * 255 + p_mask / 2) / p_mask);
}

Error FixedFunctionGraphicsDeviceD3D7::read_frame(uint32_t *r_pixels, const size_t p_pixel_count) {
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
	return _read_surface(_target, Size2i(_width, _height), _native_size, r_pixels);
}

FixedFunctionGraphicsDeviceD3D7::FixedFunctionGraphicsDeviceD3D7(D3D7ResolutionPolicy::D3D7OversizeMode p_oversize_mode, bool p_force_rgb) :
		_texture_format(),
		_compressed_texture_formats(),
		_render_target_format(),
		_library(nullptr),
		_ddraw(nullptr),
		_d3d(nullptr),
		_device(nullptr),
		_vertex_buffer(nullptr),
		_primary(nullptr),
		_target(nullptr),
		_clipper(nullptr),
		_window_depth(nullptr),
		_window(nullptr),
		_owner_thread(0),
		_last_result(DD_OK),
		_render_bit_depth(0),
		_capabilities(),
		_resolution(p_oversize_mode),
		_requested_width(0),
		_requested_height(0),
		_width(0),
		_height(0),
		_render_width(0),
		_render_height(0),
		_vertex_buffer_position(0),
		_vsync(true),
		_hardware_rasterizer(false),
		_force_rgb(p_force_rgb),
		_reset_pending(false),
		_frame_active(false),
		_depth_active(false),
		_frame_readable(false),
		_window_rendered(false),
		_resetting(false),
		_color_source(COLOR_SOURCE_DIFFUSE),
		_vertex_buffer_unavailable(false) {
}

FixedFunctionGraphicsDeviceD3D7::~FixedFunctionGraphicsDeviceD3D7() {
	finalize();
}

#undef D3D7_FAIL_ON_ERROR
