#include "graphics_device_d3d3_software.h"
#include "graphics_device_d3d3_software_factory.h"

#include "core/math/math_funcs.h"
#include "core/os/os.h"

#include <stddef.h>
#include <string.h>

FixedFunctionGraphicsDevice *FixedFunctionGraphicsDeviceD3D3SoftwareFactory::create() {
	return memnew(FixedFunctionGraphicsDeviceD3D3Software);
}

FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareRenderTarget::D3D3SoftwareRenderTarget() :
		color(nullptr),
		depth(nullptr),
		device(nullptr),
		viewport(nullptr),
		background(nullptr),
		execute(nullptr),
		device_id(0),
		identity_matrix(0),
		projection_matrix(0),
		projection_valid(false),
		readable(false),
		depth_initialized(false),
		rgb888_fill(false) {}

FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareRenderTarget::~D3D3SoftwareRenderTarget() {
	release();
}

bool FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareRenderTarget::is_lost() const {
	return color->IsLost() != DD_OK || depth->IsLost() != DD_OK;
}

FixedFunctionGraphicsDeviceD3D3Software::FixedFunctionGraphicsDeviceD3D3Software() :
		_texture_format(),
		_alpha_texture_format(),
		_pass(&_window_target),
		_next_device_id(0),
		_library(nullptr),
		_window(nullptr),
		_ddraw(nullptr),
		_d3d(nullptr),
		_clipper(nullptr),
		_front(nullptr),
		_glyph_cache(nullptr),
		_execute_bytes(0),
		_owner_thread(0),
		_frame_active(false),
		_depth_active(false) {}

FixedFunctionGraphicsDeviceD3D3Software::~FixedFunctionGraphicsDeviceD3D3Software() {
	finalize();
}

Error FixedFunctionGraphicsDeviceD3D3Software::_check_thread() const {
	if (!_owner_thread) {
		return ERR_UNCONFIGURED;
	}
	return _owner_thread == GetCurrentThreadId() ? OK : ERR_BUG;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_check_result(HRESULT p_result) {
	if (SUCCEEDED(p_result)) {
		return OK;
	}
	switch (p_result) {
		case DDERR_SURFACELOST:
		case DDERR_WRONGMODE:
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

#define D3D3_SOFTWARE_CHECK(m_call)                     \
	do {                                                \
		const Error call_error = _check_result(m_call); \
		if (call_error != OK) {                         \
			return call_error;                          \
		}                                               \
	} while (false)

Error FixedFunctionGraphicsDeviceD3D3Software::initialize(void *p_native_window, uint32_t p_width, uint32_t p_height, bool p_vsync) {
	if (_owner_thread) {
		return ERR_ALREADY_IN_USE;
	}
	HWND window = static_cast<HWND>(p_native_window);
	if (!IsWindow(window) || !p_width || !p_height || p_width > INT32_MAX || p_height > INT32_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	if (GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId()) {
		return ERR_BUG;
	}
	_library = LoadLibraryA("ddraw.dll");
	if (!_library) {
		return ERR_UNAVAILABLE;
	}
	_window = window;
	_owner_thread = GetCurrentThreadId();
	typedef HRESULT(WINAPI * CreateDirectDraw)(GUID *, IDirectDraw **, IUnknown *);
	const CreateDirectDraw create = reinterpret_cast<CreateDirectDraw>(reinterpret_cast<void *>(GetProcAddress(_library, "DirectDrawCreate")));
	HRESULT result = create ? create(nullptr, &_ddraw, nullptr) : E_NOINTERFACE;
	if (SUCCEEDED(result)) {
		result = _ddraw->SetCooperativeLevel(_window, DDSCL_NORMAL);
	}
	if (SUCCEEDED(result)) {
		result = _ddraw->QueryInterface(IID_IDirect3D, reinterpret_cast<void **>(&_d3d));
	}
	if (SUCCEEDED(result)) {
		result = _ddraw->CreateClipper(0, &_clipper, nullptr);
	}
	if (SUCCEEDED(result)) {
		result = _clipper->SetHWnd(0, _window);
	}
	Error error = _check_result(result);
	if (error == OK) {
		_requested_size = get_window_size(Size2i(p_width, p_height));
		error = _create_window_resources();
	}
	if (error == OK) {
		_capabilities.blend_mix = _alpha_texture_format.dwSize && _probe_alpha_blend();
		if (_capabilities.blend_mix) {
			uint8_t white[8 * 8 * 4];
			memset(white, 255, sizeof(white));
			_capabilities.blend_mix = _upload_texture_view(_blend_white, Size2i(8, 8), _alpha_texture_format, white) == OK;
			if (!_capabilities.blend_mix) {
				_blend_white.release();
			}
		}
		if (!_capabilities.texture_square_only) {
			const DDPIXELFORMAT &format = _capabilities.blend_mix ? _alpha_texture_format : _texture_format;
			_capabilities.max_texture_width = _probe_texture_extent(true, format);
			_capabilities.max_texture_height = _probe_texture_extent(false, format);
		}
		RID probe;
		_capabilities.render_target_opaque = true;
		const bool created = texture_create_render_target(8, 8, probe) == OK;
		if (created) {
			D3D3SoftwareTextureView &view = _texture_owner.getptr(probe)->opaque;
			_capabilities.render_target_opaque = SUCCEEDED(view.texture->GetHandle(_window_target.device, &view.handle)) && view.handle;
			texture_free(probe);
		} else {
			_capabilities.render_target_opaque = false;
		}
		_capabilities.render_target_scaled = _capabilities.render_target_opaque;
		_adapter_name = "Direct3D RGB software device";
		// Inform the OS singleton of D3D3's max window size.
		OS *os = OS::get_singleton();
		os->set_max_window_size(get_max_window_size());
		// Shrink non-fullscreen window size if it's too big.
		if (!os->is_window_fullscreen()) {
			const Size2 window_size = os->get_window_size();
			const Size2 limited_size = get_window_size(window_size);
			if (window_size != limited_size) {
				os->set_window_size(limited_size);
			}
		}
	} else {
		finalize();
	}
	return error;
}

Size2i FixedFunctionGraphicsDeviceD3D3Software::_fit_render_size(const Size2i &p_size, const Size2i &p_limit) {
	if (p_size.x <= p_limit.x && p_size.y <= p_limit.y) {
		return p_size;
	}
	// Compare the scale factors exactly and round down to fit both native limits.
	if (int64_t(p_limit.x) * p_size.y <= int64_t(p_limit.y) * p_size.x) {
		return Size2i(p_limit.x, MAX(int64_t(1), int64_t(p_size.y) * p_limit.x / p_size.x));
	}
	return Size2i(MAX(int64_t(1), int64_t(p_size.x) * p_limit.y / p_size.y), p_limit.y);
}

Size2i FixedFunctionGraphicsDeviceD3D3Software::get_max_window_size() const {
	const Size2i os_max_window_size = Size2i(OS::get_singleton()->get_max_window_size());
	// Zero means unlimited. Preserve any stricter existing window limits.
	return Size2i(
			os_max_window_size.x > 0 ? MIN(os_max_window_size.x, int(MAX_RENDER_TARGET_LIMIT)) : int(MAX_RENDER_TARGET_LIMIT),
			os_max_window_size.y > 0 ? MIN(os_max_window_size.y, int(MAX_RENDER_TARGET_LIMIT)) : int(MAX_RENDER_TARGET_LIMIT));
}

Size2i FixedFunctionGraphicsDeviceD3D3Software::get_window_size(const Size2i &p_window_size) const {
	const Size2i max_window_size = get_max_window_size();
	return Size2i(MIN(p_window_size.x, max_window_size.x), MIN(p_window_size.y, max_window_size.y));
}

Error FixedFunctionGraphicsDeviceD3D3Software::_create_window_resources() {
	DDSURFACEDESC description = {};
	description.dwSize = sizeof(description);
	description.dwFlags = DDSD_CAPS;
	description.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	D3D3_SOFTWARE_CHECK(_ddraw->CreateSurface(&description, &_front, nullptr));
	D3D3_SOFTWARE_CHECK(_front->SetClipper(_clipper));
	description.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	description.dwWidth = _requested_size.x;
	description.dwHeight = _requested_size.y;
	description.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE | DDSCAPS_SYSTEMMEMORY;
	D3D3_SOFTWARE_CHECK(_ddraw->CreateSurface(&description, &_window_target.color, nullptr));
	_window_target.size = _requested_size;
	_window_target.native_size = _requested_size;
	D3DDEVICEDESC software = {};
	const Error window_pass = _create_pass(_window_target, _requested_size, &software);
	if (window_pass != OK) {
		return window_pass;
	}

	const D3DPRIMCAPS &caps = software.dpcTriCaps;

	_texture_format = DDPIXELFORMAT();
	_alpha_texture_format = DDPIXELFORMAT();
	D3D3_SOFTWARE_CHECK(_window_target.device->EnumTextureFormats(_select_texture_format, this));
	if (!_texture_format.dwSize || !(caps.dwTextureBlendCaps & D3DPTBLENDCAPS_MODULATE)) {
		return ERR_UNAVAILABLE;
	}
	_capabilities.max_primitives_per_draw = (_execute_bytes - EXECUTE_STATE_BYTES) / (3 * sizeof(D3DLVERTEX) + sizeof(D3DTRIANGLE));
	if (software.dwMaxVertexCount) {
		_capabilities.max_primitives_per_draw = MIN(_capabilities.max_primitives_per_draw, software.dwMaxVertexCount / 3);
		if (!_capabilities.max_primitives_per_draw) {
			return ERR_UNAVAILABLE;
		}
	}
	if (!_alpha_texture_format.dwSize) {
		_capabilities.blend_mix = false;
	}
	// The original caps structure has no maximum texture dimensions. Keep the
	// initial limits across a window resize, where the same RGB device is recreated.
	if (!_capabilities.max_texture_width) {
		_capabilities.max_texture_width = _capabilities.max_texture_height = 256;
	}
	_capabilities.max_texture_stages = 1;
	_capabilities.texture_power_of_two = (caps.dwTextureCaps & D3DPTEXTURECAPS_POW2) != 0;
	_capabilities.texture_square_only = (caps.dwTextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0;
	_capabilities.sampler_filter_linear = (caps.dwTextureFilterCaps & D3DPTFILTERCAPS_LINEAR) != 0;
	_capabilities.sampler_repeat = (caps.dwTextureAddressCaps & D3DPTADDRESSCAPS_WRAP) != 0;
	_capabilities.cull_front = (caps.dwMiscCaps & D3DPMISCCAPS_CULLCW) != 0;
	_capabilities.cull_back = (caps.dwMiscCaps & D3DPMISCCAPS_CULLCCW) != 0;
	_capabilities.depth_buffer = (software.dwDeviceZBufferBitDepth & DDBD_16) && (caps.dwZCmpCaps & D3DPCMPCAPS_LESSEQUAL);
	_capabilities.sky_solid_hemispheres = true;
	_capabilities.canvas_fill_blit = true;
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_create_pass(D3D3SoftwareRenderTarget &r_pass, const Size2i &p_backing_size, D3DDEVICEDESC *r_software) {
	// The original RGB device takes its depth attachment at creation.
	const Error depth = _create_depth(r_pass, p_backing_size);
	if (depth != OK) {
		return depth;
	}
	D3D3_SOFTWARE_CHECK(r_pass.color->QueryInterface(IID_IDirect3DRGBDevice, reinterpret_cast<void **>(&r_pass.device)));
	if (r_software) {
		D3DDEVICEDESC hardware = {};
		// DirectX 3's descriptor ends before the texture-size fields added in DirectX 5.
		hardware.dwSize = r_software->dwSize = offsetof(D3DDEVICEDESC, dwMinTextureWidth);
		D3D3_SOFTWARE_CHECK(r_pass.device->GetCaps(&hardware, r_software));
		_execute_bytes = r_software->dwMaxBufferSize ? MIN(DWORD(EXECUTE_BYTES), r_software->dwMaxBufferSize) : DWORD(EXECUTE_BYTES);
		if (_execute_bytes < EXECUTE_STATE_BYTES + 3 * sizeof(D3DLVERTEX) + sizeof(D3DTRIANGLE)) {
			return ERR_UNAVAILABLE;
		}
	}
	D3D3_SOFTWARE_CHECK(_d3d->CreateViewport(&r_pass.viewport, nullptr));
	D3D3_SOFTWARE_CHECK(r_pass.device->AddViewport(r_pass.viewport));
	D3D3_SOFTWARE_CHECK(_d3d->CreateMaterial(&r_pass.background, nullptr));
	D3DMATERIALHANDLE background_handle = 0;
	D3D3_SOFTWARE_CHECK(r_pass.background->GetHandle(r_pass.device, &background_handle));
	D3D3_SOFTWARE_CHECK(r_pass.viewport->SetBackground(background_handle));
	D3D3_SOFTWARE_CHECK(r_pass.device->CreateMatrix(&r_pass.identity_matrix));
	D3D3_SOFTWARE_CHECK(r_pass.device->CreateMatrix(&r_pass.projection_matrix));
	D3DMATRIX identity = {};
	identity._11 = identity._22 = identity._33 = identity._44 = 1;
	D3D3_SOFTWARE_CHECK(r_pass.device->SetMatrix(r_pass.identity_matrix, &identity));

	D3DEXECUTEBUFFERDESC buffer = {};
	buffer.dwSize = sizeof(buffer);
	buffer.dwFlags = D3DDEB_BUFSIZE;
	buffer.dwBufferSize = _execute_bytes;
	D3D3_SOFTWARE_CHECK(r_pass.device->CreateExecuteBuffer(&buffer, &r_pass.execute, nullptr));
	DDPIXELFORMAT format = {};
	format.dwSize = sizeof(format);
	r_pass.rgb888_fill = SUCCEEDED(r_pass.color->GetPixelFormat(&format)) && (format.dwFlags & DDPF_RGB) &&
			format.dwRGBBitCount == 32 && format.dwRBitMask == 0xff0000 && format.dwGBitMask == 0xff00 && format.dwBBitMask == 0xff;
	r_pass.device_id = ++_next_device_id;
	return OK;
}

void FixedFunctionGraphicsDeviceD3D3Software::D3D3SoftwareRenderTarget::release() {
	if (execute) {
		execute->Release();
		execute = nullptr;
	}
	if (device) {
		if (projection_matrix) {
			device->DeleteMatrix(projection_matrix);
		}
		if (identity_matrix) {
			device->DeleteMatrix(identity_matrix);
		}
		if (viewport) {
			device->DeleteViewport(viewport);
		}
	}
	if (viewport) {
		viewport->Release();
		viewport = nullptr;
	}
	if (device) {
		device->Release();
		device = nullptr;
	}
	if (color) {
		color->Release();
		color = nullptr;
	}
	// Keep the attachment and background alive through destruction of the
	// color surface, which owns the underlying Direct3D device.
	if (depth) {
		depth->Release();
		depth = nullptr;
	}
	if (background) {
		background->Release();
		background = nullptr;
	}
	identity_matrix = projection_matrix = 0;
	device_id = 0;
	projection_valid = readable = depth_initialized = false;
	viewport_rect = Rect2i();
	size = Size2i();
	native_size = Size2i();
}

void FixedFunctionGraphicsDeviceD3D3Software::_release_window_resources() {
	_window_target.release();
	if (_front) {
		_front->Release();
		_front = nullptr;
	}
}

Error FixedFunctionGraphicsDeviceD3D3Software::finalize() {
	if (!_owner_thread) {
		return OK;
	}
	const Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active) {
		end_frame(false, true);
	}
	while (!_textures.empty()) {
		D3D3SoftwareTexture *texture = _textures.front()->get();
		_textures.pop_front();
		_texture_owner.free(texture->rid);
		memdelete(texture);
	}
	_release_window_resources();
	_blend_white.release();
	if (_glyph_cache) {
		memdelete_arr(_glyph_cache);
		_glyph_cache = nullptr;
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
	FreeLibrary(_library);
	_library = nullptr;
	_window = nullptr;
	_owner_thread = 0;
	_requested_size = Size2i();
	_execute_bytes = 0;
	_next_device_id = 0;
	_capabilities = FFCapabilities();
	_adapter_name = String();
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D3Software::resize(uint32_t p_width, uint32_t p_height) {
	const Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (p_width > INT32_MAX || p_height > INT32_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	// Window callbacks only queue a size. The scene owns the active native buffers.
	_requested_size = get_window_size(Size2i(p_width, p_height));
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D3Software::set_use_vsync(bool p_enabled) {
	// Windowed DirectDraw blits use the runtime's presentation timing.
	return _check_thread();
}

Error FixedFunctionGraphicsDeviceD3D3Software::prepare_resources() {
	const Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (!_requested_size.x || !_requested_size.y) {
		return ERR_SKIP;
	}
	if (_window_target.size != _requested_size || _front->IsLost() != DD_OK || _window_target.is_lost()) {
		_release_window_resources();
		const Error recreate = _create_window_resources();
		if (recreate != OK) {
			_release_window_resources();
			return recreate;
		}
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_create_depth(D3D3SoftwareRenderTarget &r_pass, const Size2i &p_backing_size) {
	DDSURFACEDESC description = {};
	description.dwSize = sizeof(description);
	description.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_ZBUFFERBITDEPTH;
	description.dwWidth = p_backing_size.x;
	description.dwHeight = p_backing_size.y;
	description.dwZBufferBitDepth = 16;
	description.ddsCaps.dwCaps = DDSCAPS_ZBUFFER | DDSCAPS_SYSTEMMEMORY;
	IDirectDrawSurface *depth = nullptr;
	D3D3_SOFTWARE_CHECK(_ddraw->CreateSurface(&description, &depth, nullptr));
	const Error attached = _check_result(r_pass.color->AddAttachedSurface(depth));
	if (attached != OK) {
		depth->Release();
		return attached;
	}
	r_pass.depth = depth;
	return OK;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_set_viewport(const Rect2i &p_rect) {
	if (_pass->viewport_rect == p_rect) {
		return OK;
	}
	// The packed projection includes the viewport correction.
	_pass->projection_valid = false;
	D3DVIEWPORT viewport = {};
	viewport.dwSize = sizeof(viewport);
	viewport.dwX = p_rect.position.x;
	viewport.dwY = p_rect.position.y;
	viewport.dwWidth = p_rect.size.x;
	viewport.dwHeight = p_rect.size.y;
	viewport.dvScaleX = p_rect.size.x * 0.5f;
	viewport.dvScaleY = p_rect.size.y * 0.5f;
	viewport.dvMaxX = viewport.dvMaxY = viewport.dvMaxZ = 1;
	const Error error = _check_result(_pass->viewport->SetViewport(&viewport));
	_pass->viewport_rect = error == OK ? p_rect : Rect2i();
	return error;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_clear(DWORD p_flags) {
	const Error error = _set_viewport(Rect2i(Point2i(), _pass->native_size));
	if (error != OK) {
		return error;
	}
	D3DRECT rect = {};
	rect.x2 = _pass->native_size.x;
	rect.y2 = _pass->native_size.y;
	return _check_result(_pass->viewport->Clear(1, &rect, p_flags));
}

Error FixedFunctionGraphicsDeviceD3D3Software::begin_frame(uint32_t p_clear_color, RID p_target, bool p_clear, bool p_use_depth) {
	const Error error = prepare_resources();
	if (error != OK) {
		return error;
	}
	if (p_use_depth && !_capabilities.depth_buffer) {
		return ERR_UNAVAILABLE;
	}
	D3D3SoftwareRenderTarget *pass = &_window_target;
	if (p_target.is_valid()) {
		if (!_texture_owner.owns(p_target)) {
			return ERR_INVALID_PARAMETER;
		}
		pass = _texture_owner.getptr(p_target)->target;
		if (!pass) {
			return ERR_INVALID_PARAMETER;
		}
		if (pass->is_lost()) {
			pass->readable = pass->depth_initialized = false;
			D3D3_SOFTWARE_CHECK(pass->color->Restore());
			D3D3_SOFTWARE_CHECK(pass->depth->Restore());
			p_clear = true;
		}
	}
	if (!p_clear && !pass->readable) {
		return ERR_UNCONFIGURED;
	}
	pass->readable = false;
	D3D3_SOFTWARE_CHECK(pass->device->BeginScene());
	_pass = pass;
	_pass->viewport_rect = Rect2i();
	_pass->projection_valid = false;
	_frame_active = true;
	_depth_active = p_use_depth;
	Error clear_error = p_clear ? clear_color(p_clear_color) : OK;
	if (clear_error == OK && p_use_depth && (p_clear || !_pass->depth_initialized)) {
		clear_error = clear_depth();
	}
	if (clear_error != OK) {
		end_frame(false, true);
	}
	return clear_error;
}

Error FixedFunctionGraphicsDeviceD3D3Software::clear_color(uint32_t p_color) {
	const Error error = _check_thread();
	if (error != OK || !_frame_active) {
		return error == OK ? ERR_UNCONFIGURED : error;
	}
	D3DMATERIAL material = {};
	material.dwSize = sizeof(material);
	material.diffuse.r = float((p_color >> 16) & 255) / 255.0f;
	material.diffuse.g = float((p_color >> 8) & 255) / 255.0f;
	material.diffuse.b = float(p_color & 255) / 255.0f;
	material.diffuse.a = 1;
	D3D3_SOFTWARE_CHECK(_pass->background->SetMaterial(&material));
	return _clear(D3DCLEAR_TARGET);
}

Error FixedFunctionGraphicsDeviceD3D3Software::clear_depth(float p_depth) {
	const Error error = _check_thread();
	if (error != OK || !_depth_active) {
		return error == OK ? ERR_UNCONFIGURED : error;
	}
	if (Math::is_nan(p_depth) || p_depth < 0 || p_depth > 1) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_depth != 1) {
		return ERR_UNAVAILABLE;
	}
	const Error cleared = _clear(D3DCLEAR_ZBUFFER);
	_pass->depth_initialized = cleared == OK;
	return cleared;
}

Error FixedFunctionGraphicsDeviceD3D3Software::copy_render_target_to_window(RID p_texture) {
	const Error error = _check_thread();
	if (error != OK) {
		return error;
	}
	if (!_frame_active || _pass != &_window_target || !_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	const D3D3SoftwareRenderTarget *source = _texture_owner.getptr(p_texture)->target;
	if (!source || source->native_size != _pass->native_size || !source->readable || source->is_lost()) {
		return ERR_UNAVAILABLE;
	}
	RECT src = { 0, 0, source->native_size.x, source->native_size.y };
	RECT dst = { 0, 0, _pass->native_size.x, _pass->native_size.y };
	// Some runtimes reject blitting while a scene is active. The compositor can draw its quad then.
	return SUCCEEDED(_pass->color->Blt(&dst, source->color, &src, DDBLT_WAIT, nullptr)) ? OK : ERR_UNAVAILABLE;
}

Error FixedFunctionGraphicsDeviceD3D3Software::_present() {
	RECT destination;
	POINT origin = {};
	if (!GetClientRect(_window, &destination) || !ClientToScreen(_window, &origin)) {
		return FAILED;
	}
	if (destination.right <= 0 || destination.bottom <= 0) {
		return ERR_SKIP;
	}
	const Size2i &size = _window_target.native_size;
	destination.right = MIN(destination.right, LONG(size.x));
	destination.bottom = MIN(destination.bottom, LONG(size.y));
	RECT source = destination;
	OffsetRect(&destination, origin.x, origin.y);
	return _check_result(_front->Blt(&destination, _window_target.color, &source, DDBLT_WAIT, nullptr));
}

Error FixedFunctionGraphicsDeviceD3D3Software::end_frame(bool p_present, bool p_discard) {
	const Error error = _check_thread();
	if (error != OK || !_frame_active) {
		return error == OK ? ERR_UNCONFIGURED : error;
	}
	D3D3SoftwareRenderTarget &pass = *_pass;
	_pass = &_window_target;
	_frame_active = _depth_active = false;
	const Error ended = _check_result(pass.device->EndScene());
	if (ended != OK || p_discard) {
		pass.depth_initialized = false;
		return ended;
	}
	if (&pass != &_window_target) {
		pass.readable = true;
		return p_present ? ERR_UNAVAILABLE : OK;
	}
	if (p_present) {
		return _present();
	}
	pass.readable = true;
	return OK;
}
