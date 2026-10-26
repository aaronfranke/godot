#ifndef FIXED_FUNCTION_GRAPHICS_DEVICE_D3D3_SOFTWARE_H
#define FIXED_FUNCTION_GRAPHICS_DEVICE_D3D3_SOFTWARE_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <d3d.h>

#include "drivers/fixed_function/graphics_device.h"

class FixedFunctionGraphicsDeviceD3D3Software : public FixedFunctionGraphicsDevice {
	struct D3D3SoftwareTextureView {
		IDirectDrawSurface *surface;
		IDirect3DTexture *texture;
		uint64_t device_id;
		D3DTEXTUREHANDLE handle;

		D3D3SoftwareTextureView();
		~D3D3SoftwareTextureView();
		void release();
	};

	struct D3D3SoftwareRenderTarget {
		IDirectDrawSurface *color;
		IDirectDrawSurface *depth;
		IDirect3DDevice *device;
		IDirect3DViewport *viewport;
		IDirect3DMaterial *background;
		IDirect3DExecuteBuffer *execute;
		uint64_t device_id;
		CameraMatrix submitted_projection;
		Rect2i viewport_rect;
		Size2i size;
		Size2i native_size;
		D3DMATRIXHANDLE identity_matrix;
		D3DMATRIXHANDLE projection_matrix;
		bool projection_valid;
		bool readable;
		bool depth_initialized;
		bool rgb888_fill;

		D3D3SoftwareRenderTarget();
		~D3D3SoftwareRenderTarget();
		bool is_lost() const;
		void release();
	};

	struct D3D3SoftwareTexture : RID_Data {
		Size2i size;
		Size2i upload_size;
		D3D3SoftwareTextureView opaque;
		D3D3SoftwareTextureView alpha;
		D3D3SoftwareTextureView binary_glyph;
		D3D3SoftwareRenderTarget *target;
		IDirectDrawSurface *keyed_sprite;
		Vector2 uv_scale;
		uint32_t bytes;
		RID rid;
		bool sprite_blit_eligible;

		D3D3SoftwareTexture();
		~D3D3SoftwareTexture();
		uint64_t get_bytes() const;
	};

	struct D3D3SoftwareGlyph {
		Rect2i source;
		IDirectDrawSurface *surface;
		uint32_t texture_id;
		uint32_t color;

		D3D3SoftwareGlyph();
		~D3D3SoftwareGlyph();
		void release();
	};

	enum {
		GLYPH_CACHE_SIZE = 256,
		MAX_GLYPH_SIZE = 64,
		GLYPH_ALPHA_THRESHOLD = 128
	};
	// Bound the optional system-memory copy of a sprite or atlas to 256 KiB.
	enum { MAX_KEYED_SPRITE_PIXELS = 256 * 256 };
	// Leave room for state instructions and keep the stream below the SDK's 64 KiB recommendation.
	enum {
		EXECUTE_BYTES = 32 * 1024,
		EXECUTE_STATE_BYTES = 512
	};
	// RID lookup lacks a const API. Texture queries only read the records.
	mutable RID_Owner<D3D3SoftwareTexture> _texture_owner;
	List<D3D3SoftwareTexture *> _textures;
	D3D3SoftwareTextureView _blend_white;
	FFCapabilities _capabilities;
	DDPIXELFORMAT _texture_format;
	DDPIXELFORMAT _alpha_texture_format;
	D3D3SoftwareRenderTarget _window_target;
	D3D3SoftwareRenderTarget *_pass;
	Size2i _requested_size;
	uint64_t _next_device_id;
	HMODULE _library;
	HWND _window;
	IDirectDraw *_ddraw;
	IDirect3D *_d3d;
	IDirectDrawClipper *_clipper;
	IDirectDrawSurface *_front;
	D3D3SoftwareGlyph *_glyph_cache;
	DWORD _execute_bytes;
	DWORD _owner_thread;
	bool _frame_active;
	bool _depth_active;

	FixedFunctionGraphicsDeviceD3D3Software(const FixedFunctionGraphicsDeviceD3D3Software &);
	FixedFunctionGraphicsDeviceD3D3Software &operator=(const FixedFunctionGraphicsDeviceD3D3Software &);
	Error _check_thread() const;
	Error _create_window_resources();
	Error _create_pass(D3D3SoftwareRenderTarget &r_pass, const Size2i &p_backing_size, D3DDEVICEDESC *r_software = nullptr);
	Error _create_depth(D3D3SoftwareRenderTarget &r_pass, const Size2i &p_backing_size);
	void _release_window_resources();
	Error _set_viewport(const Rect2i &p_rect);
	Rect2i _get_native_draw_viewport(const FFDrawParameters &p_state) const;
	Rect2 _get_native_canvas_bounds(const Rect2 &p_bounds) const;
	Error _check_canvas_blt(const FFDrawParameters &p_state) const;
	Error _fill_canvas_rect(RECT &p_rect, uint32_t p_color);
	static bool _get_clipped_blt_rect(const Rect2 &p_bounds, const Rect2i &p_clip, RECT &r_rect);
	static bool _get_canvas_rectangle(const FixedFunctionVertex *const *p_triangles, Rect2 &r_bounds);
	static bool _get_canvas_border(const FixedFunctionVertex *const *p_triangles, int p_count, const Rect2 &p_inner, Rect2 &r_outer, uint32_t &r_color);
	Error _create_keyed_glyph(D3D3SoftwareGlyph &r_glyph, IDirectDrawSurface *p_atlas, const Rect2i &p_source, uint32_t p_color);
	Error _prepare_binary_glyph_view(D3D3SoftwareTexture &r_texture);
	Error _prepare_keyed_sprite(D3D3SoftwareTexture &r_texture);
	Error _blit_canvas_sprite(D3D3SoftwareTexture &r_texture, const FixedFunctionVertex *p_vertices, const CameraMatrix &p_clip_from_model, const Rect2i &p_viewport);
	static bool _projection_matches(const CameraMatrix &p_first, const CameraMatrix &p_second);
	Error _clear(DWORD p_flags);
	Error _present();
	Error _draw_batch(const FixedFunctionVertex *p_vertices, uint32_t p_count, D3DTEXTUREHANDLE p_texture, const FFDrawParameters &p_state, const Vector2 &p_uv_scale = Vector2(1, 1));
	Error _create_texture_view(D3D3SoftwareTextureView &r_view, const Size2i &p_size, const DDPIXELFORMAT &p_format);
	Error _upload_texture_view(D3D3SoftwareTextureView &r_view, const Size2i &p_size, const DDPIXELFORMAT &p_format, const uint8_t *p_rgba, bool p_use_alpha = true);
	bool _probe_alpha_blend();
	uint32_t _probe_texture_extent(bool p_horizontal, const DDPIXELFORMAT &p_format);
	static uint32_t _unpack_channel(uint32_t p_pixel, uint32_t p_mask);
	static uint32_t _pack_surface_color(uint32_t p_color, const DDPIXELFORMAT &p_format);
	static Size2i _fit_render_size(const Size2i &p_size, const Size2i &p_limit);
	static Error _check_result(HRESULT p_result);
	static HRESULT CALLBACK _select_texture_format(DDSURFACEDESC *p_surface, void *p_context);

	Error _read_surface(const D3D3SoftwareRenderTarget &p_target, uint32_t *r_pixels);

public:
	enum { MAX_RENDER_TARGET_LIMIT = 2048 };

	FixedFunctionGraphicsDeviceD3D3Software();
	~FixedFunctionGraphicsDeviceD3D3Software() override;

	Error initialize(void *p_native_window, uint32_t p_width, uint32_t p_height, bool p_vsync) override;
	Error finalize() override;
	Error resize(uint32_t p_width, uint32_t p_height) override;
	Error set_use_vsync(bool p_enabled) override;
	Error prepare_resources() override;
	const FFCapabilities &get_capabilities() const override { return _capabilities; }
	uint32_t get_width() const override { return _frame_active ? _window_target.size.x : _requested_size.x; }
	uint32_t get_height() const override { return _frame_active ? _window_target.size.y : _requested_size.y; }
	Size2i get_max_window_size() const override;
	Size2i get_window_size(const Size2i &p_window_size) const override;
	uint32_t get_target_width() const override { return _frame_active ? _pass->size.x : get_width(); }
	uint32_t get_target_height() const override { return _frame_active ? _pass->size.y : get_height(); }

	Error texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame = false) override;
	Error texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent = false) override;
	Error texture_get_info(RID p_texture, FFTextureInfo &r_info) const override;
	uint64_t get_texture_memory_usage() const override;
	bool texture_is_render_target_ready(RID p_texture) const override;
	Error texture_free(RID p_texture) override;

	Error begin_frame(uint32_t p_clear_color, RID p_target = RID(), bool p_clear = true, bool p_use_depth = false) override;
	Error clear_color(uint32_t p_color) override;
	Error clear_depth(float p_depth = 1.0f) override;
	Error copy_render_target_to_window(RID p_texture) override;
	Error fill_canvas_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const FFDrawParameters &p_state) override;
	Error copy_canvas_render_target(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, RID p_texture, const FFDrawParameters &p_state) override;
	bool prefers_canvas_glyph_blits() const override { return true; }
	Error blit_canvas_glyph(RID p_texture, const Rect2 &p_uv, const Rect2 &p_bounds, uint32_t p_color, const FFDrawParameters &p_state) override;
	Error draw_sky_hemispheres(real_t p_horizon_y, uint32_t p_sky_color, uint32_t p_ground_color, uint32_t &r_draw_calls) override;
	Error draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_model, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters()) override;
	Error end_frame(bool p_present = true, bool p_discard = false) override;
	Error texture_read_render_target(RID p_texture, uint32_t *r_pixels, size_t p_pixel_count) override;
	Error read_frame(uint32_t *p_pixels, size_t p_pixel_count) override;
};

#endif // FIXED_FUNCTION_GRAPHICS_DEVICE_D3D3_SOFTWARE_H
