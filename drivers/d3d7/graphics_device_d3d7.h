#ifndef FIXED_FUNCTION_GRAPHICS_DEVICE_D3D7_H
#define FIXED_FUNCTION_GRAPHICS_DEVICE_D3D7_H

#include "drivers/fixed_function/geometry_data.h"
#include "drivers/fixed_function/graphics_device.h"
#include "resolution_policy_d3d7.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <d3d.h>
#include <ddraw.h>

class FixedFunctionGraphicsDeviceD3D7 : public FixedFunctionGraphicsDevice {
	friend class TestFixedFunctionGraphicsDeviceD3D7;

	struct D3D7GeometryBatch {
		FixedFunctionGeometryData::FFGeometryBatch source;
		IDirect3DVertexBuffer7 *vertices;
		bool unavailable;
		bool lit_ambient_occlusion = false;
		D3D7GeometryBatch();
	};
	struct D3D7Geometry : RID_Data {
		Vector<D3D7GeometryBatch *> batches;
		RID rid;
	};
	// RID lookup lacks a const API; keep cached draw-info queries read-only.
	mutable RID_Owner<D3D7Geometry> _geometry_owner;
	List<D3D7Geometry *> _geometries;
	void _release_geometry_batch(D3D7GeometryBatch *p_batch);
	void _release_geometry_buffers();
	void _free_geometry(D3D7Geometry *p_geometry);
	HRESULT _upload_geometry_batch(D3D7GeometryBatch *p_batch);
	HRESULT _upload_vertex_lit_batch(D3D7GeometryBatch *p_batch, bool p_ambient_occlusion);
	Vector<uint8_t> _vertex_lit_data;
	Error _draw_vertex_lighting(const D3D7Geometry *p_geometry, const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const uint16_t *p_indices, uint32_t p_index_count, const CameraMatrix &p_projection, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state);

	struct D3D7Texture : RID_Data {
		PoolVector<uint8_t> data;
		RID rid;
		Size2i native_size;
		Size2i upload_size;
		IDirectDrawSurface7 *native;
		IDirectDrawSurface7 *depth;
		uint32_t width;
		uint32_t height;
		bool render_target;
		bool rendered;
		uint8_t depth_pixel_size;
		uint8_t source_mip_levels;
		uint8_t mip_levels;
		uint8_t format;
		D3D7Texture() :
				native(nullptr), depth(nullptr), width(0), height(0), render_target(false), rendered(false), depth_pixel_size(0), source_mip_levels(1), mip_levels(1), format(Image::FORMAT_RGBA8) {}
	};
	// RID lookup lacks a const API; preserve our read-only query API.
	// A const lookup returning const records would let us remove mutable.
	mutable RID_Owner<D3D7Texture> _texture_owner;
	List<D3D7Texture *> _textures;
	RID _render_target;
	DDPIXELFORMAT _texture_format;
	DDPIXELFORMAT _compressed_texture_formats[3];
	DDPIXELFORMAT _render_target_format;
	DDPIXELFORMAT _depth_formats[2];
	HMODULE _library;
	IDirectDraw7 *_ddraw;
	IDirect3D7 *_d3d;
	IDirect3DDevice7 *_device;
	IDirect3DVertexBuffer7 *_vertex_buffer;
	IDirectDrawSurface7 *_primary;
	IDirectDrawSurface7 *_target;
	IDirectDrawClipper *_clipper;
	IDirectDrawSurface7 *_window_depth;
	HWND _window;
	DWORD _owner_thread;
	HRESULT _last_result;
	DWORD _render_bit_depth;
	FFCapabilities _capabilities;
	D3D7ResolutionPolicy _resolution;
	Size2i _native_size;
	uint32_t _requested_width;
	uint32_t _requested_height;
	uint32_t _width;
	uint32_t _height;
	uint32_t _render_width;
	uint32_t _render_height;
	uint32_t _vertex_buffer_position;
	uint32_t _vertex_light_limit = 0;
	uint32_t _enabled_vertex_lights = 0;
	uint32_t _fog_raster_caps = 0;
	uint32_t _fog_color = 0;
	float _fog_begin = 0;
	float _fog_end = 0;
	bool _vsync;
	bool _hardware_rasterizer;
	bool _force_rgb;
	bool _rgb_device = false;
	bool _reset_pending;
	bool _frame_active;
	bool _fog_enabled = false;
	bool _depth_active;
	bool _frame_readable;
	bool _window_rendered;
	bool _resetting;
	enum D3D7ColorSource : uint8_t {
		COLOR_SOURCE_DIFFUSE,
		COLOR_SOURCE_CONSTANT,
		COLOR_SOURCE_DIRTY,
	};
	D3D7ColorSource _color_source;
	bool _vertex_buffer_unavailable;

	FixedFunctionGraphicsDeviceD3D7(const FixedFunctionGraphicsDeviceD3D7 &);
	FixedFunctionGraphicsDeviceD3D7 &operator=(const FixedFunctionGraphicsDeviceD3D7 &);

	Error _check_thread() const;
	Error _check_result(HRESULT p_result);
	Size2i _get_native_target_size() const;
	void _release_surfaces();
	Error _reset();
	enum { VERTEX_STREAM_CAPACITY = 12288 };
	void _release_vertex_stream();
	HRESULT _draw_vertex_stream(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count);
	Error _upload_texture(D3D7Texture *p_texture);
	HRESULT _upload_texture_levels(const D3D7Texture *p_texture, uint8_t p_levels);
	Error _restore_texture(D3D7Texture *p_texture);
	Error _restore_window_target();
	Error _begin_frame(uint32_t p_clear_color, RID p_render_target, bool p_clear, bool p_use_depth);
	Error _set_sampler_state(uint8_t p_flags, uint8_t p_levels = 1, uint32_t p_stage = 0);
	Error _draw_masked_triangles(const FixedFunctionMaskedVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_projection, RID p_color_texture, RID p_mask_texture, uint8_t p_mask_sampler_flags, const FFDrawParameters &p_state);
	Error _prepare_draw(const void *p_vertices, uint32_t p_vertex_count, RID p_texture, const FFDrawParameters &p_state, Rect2i &r_viewport, bool p_constant_color = false);
	CameraMatrix _get_draw_projection(const CameraMatrix &p_projection, const Rect2i &p_viewport) const;
	Error _set_table_fog(const FFFog *p_fog, FFBlendMode p_blend_mode = BLEND_MODE_DISABLED);
	Error _bind_depth_target(IDirectDrawSurface7 *p_color, IDirectDrawSurface7 *&r_depth, uint8_t &r_pixel_size);
	static void _release_depth(IDirectDrawSurface7 *p_color, IDirectDrawSurface7 *&r_depth);
	static HRESULT CALLBACK _select_depth_format(DDPIXELFORMAT *p_format, void *p_context);
	static int _texture_format_rank(const DDPIXELFORMAT &p_format);
	static int _render_target_format_rank(const DDPIXELFORMAT &p_format);
	static HRESULT CALLBACK _select_texture_format(DDPIXELFORMAT *p_format, void *p_context);
	static HRESULT CALLBACK _select_render_target_format(DDPIXELFORMAT *p_format, void *p_context);
	static uint32_t _unpack_channel(uint32_t p_pixel, uint32_t p_mask);

	Error _read_surface(IDirectDrawSurface7 *p_surface, const Size2i &p_size, const Size2i &p_native_size, uint32_t *r_pixels);

public:
	Error initialize(void *p_native_window, const uint32_t p_width, const uint32_t p_height, const bool p_vsync) override;
	Error finalize() override;
	Error resize(const uint32_t p_width, const uint32_t p_height) override;
	Error set_use_vsync(const bool p_enabled) override;
	Error prepare_resources() override;
	const FFCapabilities &get_capabilities() const override { return _capabilities; }
	// The RGB device rejects the independent two-stage mask despite reporting multiple stages.
	bool supports_texture_mask() const override { return !_rgb_device && _capabilities.max_texture_stages >= 2; }
	uint32_t get_width() const override { return _width; }
	uint32_t get_height() const override { return _height; }
	uint32_t get_target_width() const override { return _render_target.is_valid() ? _render_width : _width; }
	uint32_t get_target_height() const override { return _render_target.is_valid() ? _render_height : _height; }
	Size2i get_max_window_size() const override { return _resolution.get_max_window_size(); }
	int32_t get_last_result() const { return static_cast<int32_t>(_last_result); }
	uint32_t get_render_target_limit() const { return _resolution.get_limit(); }
	Size2i get_native_window_size() const { return _native_size; }
	Error texture_get_info(RID p_texture, FFTextureInfo &r_info) const override;
	bool supports_compressed_texture(Image::Format p_format) const override;
	uint64_t get_texture_memory_usage() const override;

	Error texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame = false) override;
	Error texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent = false) override;
	bool texture_is_render_target_ready(RID p_texture) const override;
	Error texture_free(RID p_texture) override;

	Error geometry_create(const PoolVector<FixedFunctionVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, RID &r_geometry) override;
	uint32_t get_max_streamed_vertex_lights() const override { return _vertex_light_limit; }
	Error draw_vertex_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_view, const FFPixelLighting &p_lighting, const Color &p_color, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0) override;
	Error geometry_free(RID p_geometry) override;
	Error geometry_get_draw_info(RID p_geometry, FFGeometryDrawInfo &r_info) const override;
	uint64_t get_geometry_memory_usage() const override;
	Error draw_geometry(RID p_geometry, const CameraMatrix &p_clip_from_model, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters(), const Color *p_color_override = nullptr) override;

	Error begin_frame(const uint32_t p_clear_color, RID p_render_target = RID(), bool p_clear = true, bool p_use_depth = false) override;
	Error clear_color(uint32_t p_color) override;
	Error clear_depth(float p_depth = 1.0f) override;
	Error draw_triangles(const FixedFunctionVertex *p_vertices, const uint32_t p_vertex_count, const CameraMatrix &p_clip_from_model, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters()) override;
	bool supports_indexed_triangles() const override { return true; }
	uint32_t get_max_vertex_lights() const override { return _vertex_light_limit; }
	bool supports_vertex_ambient_occlusion(const Color &p_ambient = Color()) const override { return _vertex_light_limit && can_pack_vertex_ambient_color(p_ambient); }
	Error geometry_create_lit(const PoolVector<FixedFunctionLightingVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, RID &r_geometry) override;
	Error draw_vertex_lit_geometry(RID p_geometry, const CameraMatrix &p_clip_from_model, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state) override;
	Error draw_indexed_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count, const CameraMatrix &p_projection, RID p_texture, const FFDrawParameters &p_state) override;
	Error draw_masked_triangles(const FixedFunctionMaskedVertex *p_vertices, uint32_t p_vertex_count, const CameraMatrix &p_clip_from_model, RID p_color_texture, RID p_mask_texture, uint8_t p_mask_sampler_flags, const FFDrawParameters &p_state) override;
	Error end_frame(const bool p_present = true, bool p_discard = false) override;
	Error texture_read_render_target(RID p_texture, uint32_t *r_pixels, size_t p_pixel_count) override;
	Error read_frame(uint32_t *r_pixels, const size_t p_pixel_count) override;

	FixedFunctionGraphicsDeviceD3D7(D3D7ResolutionPolicy::D3D7OversizeMode p_oversize_mode = D3D7ResolutionPolicy::D3D7_OVERSIZE_RESTRICTED, bool p_force_rgb = false);
	~FixedFunctionGraphicsDeviceD3D7() override;
};

#endif // FIXED_FUNCTION_GRAPHICS_DEVICE_D3D7_H
