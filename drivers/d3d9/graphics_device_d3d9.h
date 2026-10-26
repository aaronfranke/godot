#pragma once

#include "core/templates/rid_owner.h"
#include "drivers/fixed_function/geometry_data.h"
#include "drivers/fixed_function/graphics_device.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <d3d9.h>

class FixedFunctionGraphicsDeviceD3D9 : public FixedFunctionGraphicsDevice {
	friend class TestFixedFunctionGraphicsDeviceD3D9;

	struct D3D9GeometryBatch {
		FixedFunctionGeometryData::FFGeometryBatch source;
		IDirect3DVertexBuffer9 *vertices;
		IDirect3DVertexBuffer9 *lit_vertices = nullptr;
		IDirect3DIndexBuffer9 *indices;
		bool unavailable;
		bool lit_unavailable = false;
		bool lit_ambient_occlusion = false;
		D3D9GeometryBatch();
	};
	struct D3D9Geometry {
		Vector3 extent;
		Vector<D3D9GeometryBatch *> batches;
		RID rid;
	};
	// RID lookup lacks a const API; keep cached draw-info queries read-only.
	mutable RID_PtrOwner<D3D9Geometry> _geometry_owner;
	List<D3D9Geometry *> _geometries;
	void _release_geometry_batch(D3D9GeometryBatch *p_batch);
	void _release_geometry_buffers();
	void _free_geometry(D3D9Geometry *p_geometry);
	Error _create_geometry(const Vector<FixedFunctionGeometryData::FFGeometryBatch> &p_batches, RID &r_geometry);
	static Error _validate_lit_vertices(const Vector<FixedFunctionLightingVertex> &p_vertices);
	static Error _validate_normal_vertices(const Vector<FixedFunctionNormalMapVertex> &p_vertices);
	HRESULT _draw_geometry_batches(D3D9Geometry *p_geometry, uint32_t p_lod);
	Error _validate_pixel_lighting(const FFPixelLighting &p_lighting) const;
	Error _create_normal_map_shaders();
	Error _create_transient_normal_map_shaders(bool p_occlusion);
	struct D3D9OcclusionShaders {
		IDirect3DVertexDeclaration9 *declaration;
		IDirect3DVertexShader9 *vertex;
		IDirect3DPixelShader9 *pixel;
		IDirect3DPixelShader9 *textured;
		bool unavailable;

		void release();
	};
	Error _create_occlusion_shaders(bool p_normal_mapped);
	Error _create_transient_occlusion_shader();
	struct D3D9ShadowShaders {
		IDirect3DVertexShader9 *vertex;
		IDirect3DPixelShader9 *caster[2];
		IDirect3DPixelShader9 *receiver[4][2]; // Plain, normal, AO, normal+AO; untextured/textured.
		bool unavailable;
		D3D9ShadowShaders();
		void release();
	};
	D3D9ShadowShaders _shadow_shaders;
	Error _create_shadow_shaders();
	Error _unbind_directional_shadow();
	Error _bind_directional_shadow(const FFPixelLighting &p_lighting, bool p_normal, bool p_occlusion, bool p_textured, IDirect3DPixelShader9 *&r_shader);
	Error _draw_lit_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters *p_normal, const FFOcclusionMapParameters *p_occlusion, uint32_t p_lod);
	Error _bind_lighting(const float *p_vertex_constants, uint32_t p_register_count, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, bool p_retained, const FFNormalMapParameters *p_normal = nullptr, const FFOcclusionMapParameters *p_occlusion = nullptr);
	HRESULT _upload_geometry_batch(D3D9GeometryBatch *p_batch);
	HRESULT _upload_vertex_lit_batch(D3D9GeometryBatch *p_batch, bool p_ambient_occlusion);
	Error _draw_vertex_lighting(const D3D9Geometry *p_geometry, const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const uint16_t *p_indices, uint32_t p_index_count, const Projection &p_projection, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state, uint32_t p_lod);

	struct D3D9VertexStream {
		IDirect3DVertexBuffer9 *native;
		uint32_t position;
		bool unavailable;

		D3D9VertexStream();
	};
	D3D9VertexStream _vertex_stream;
	D3D9VertexStream _lighting_vertex_stream;
	D3D9VertexStream _vertex_lit_stream;
	D3D9VertexStream _vertex_lit_ao_stream;
	Vector<uint8_t> _vertex_lit_data;
	D3D9VertexStream _masked_vertex_stream;
	D3D9VertexStream _normal_vertex_stream;
	D3D9VertexStream _occlusion_vertex_stream;
	IDirect3DIndexBuffer9 *_stream_indices = nullptr;
	uint32_t _stream_index_position = 0;
	bool _stream_indices_unavailable = false;

	struct D3D9Texture {
		Size2i upload_size;
		IDirect3DTexture9 *native;
		IDirect3DSurface9 *depth;
		// Fill the alignment gap before 64-bit RIDs in x86_32 builds.
		bool render_target;
		bool rendered;
		bool transparent;
		uint8_t depth_pixel_size;
		uint8_t mip_levels;
		uint8_t format;
		RID rid;
		uint32_t width;
		uint32_t height;
		D3D9Texture() :
				native(nullptr), depth(nullptr), render_target(false), rendered(false), transparent(false), depth_pixel_size(0), mip_levels(1), format(Image::FORMAT_RGBA8), width(0), height(0) {}
	};
	// RID lookup lacks a const API; preserve our read-only query API.
	// A const lookup returning const records would let us remove mutable.
	mutable RID_PtrOwner<D3D9Texture> _texture_owner;
	List<D3D9Texture *> _textures;
	RID _render_target;
	HMODULE _library;
	IDirect3D9 *_d3d;
	IDirect3DDevice9 *_device;
	IDirect3DVertexDeclaration9 *_lighting_declaration;
	IDirect3DVertexShader9 *_lighting_vertex_shader;
	IDirect3DVertexShader9 *_lighting_geometry_shader;
	IDirect3DPixelShader9 *_lighting_pixel_shader;
	IDirect3DPixelShader9 *_lighting_textured_shader;
	IDirect3DVertexDeclaration9 *_normal_map_declaration;
	IDirect3DVertexShader9 *_normal_map_vertex_shader;
	IDirect3DPixelShader9 *_normal_map_pixel_shader;
	IDirect3DPixelShader9 *_normal_map_textured_shader;
	IDirect3DVertexDeclaration9 *_transient_normal_declaration;
	IDirect3DVertexShader9 *_transient_normal_vertex_shader;
	IDirect3DVertexShader9 *_transient_normal_occlusion_shader;
	IDirect3DVertexDeclaration9 *_transient_occlusion_declaration;
	IDirect3DVertexShader9 *_transient_occlusion_shader;
	D3D9OcclusionShaders _occlusion_shaders;
	D3D9OcclusionShaders _normal_occlusion_shaders;
	IDirect3DSurface9 *_window_target;
	IDirect3DSurface9 *_window_depth;
	HWND _window;
	DWORD _owner_thread;
	HRESULT _last_result;
	FFCapabilities _capabilities;
	D3DFORMAT _adapter_format;
	uint32_t _requested_width;
	uint32_t _requested_height;
	uint32_t _width;
	uint32_t _height;
	uint32_t _render_width;
	uint32_t _render_height;
	uint32_t _pixel_light_limit;
	uint32_t _geometry_vertex_limit;
	uint32_t _vertex_light_limit = 0;
	uint32_t _enabled_vertex_lights = 0;
	uint32_t _fog_raster_caps = 0;
	uint32_t _fog_color = 0;
	float _fog_begin = 0;
	float _fog_end = 0;
	uint8_t _compressed_texture_formats;
	bool _normal_map_unavailable;
	bool _transient_normal_unavailable;
	bool _transient_normal_occlusion_unavailable;
	bool _transient_occlusion_unavailable;
	bool _normal_map_bound;
	bool _shadow_bound;
	bool _vsync;
	bool _reset_pending;
	bool _frame_active;
	bool _fog_enabled = false;
	bool _depth_active;
	bool _frame_readable;
	bool _window_rendered;
	bool _resetting;
	enum D3D9ColorSource : uint8_t {
		COLOR_SOURCE_DIFFUSE,
		COLOR_SOURCE_CONSTANT,
		COLOR_SOURCE_DIRTY,
	};
	D3D9ColorSource _color_source;

	FixedFunctionGraphicsDeviceD3D9(const FixedFunctionGraphicsDeviceD3D9 &);
	FixedFunctionGraphicsDeviceD3D9 &operator=(const FixedFunctionGraphicsDeviceD3D9 &);

	Error _check_thread() const;
	Error _check_result(HRESULT p_result);
	D3DPRESENT_PARAMETERS _make_present_parameters() const;
	Error _reset();
	static const D3DFORMAT DEPTH_FORMATS[3];
	bool _supports_depth_format(D3DFORMAT p_color, D3DFORMAT p_depth) const;
	void _update_depth_support();
	void _update_compressed_texture_support();
	Error _allocate_depth(IDirect3DSurface9 *p_color, IDirect3DSurface9 *&r_depth, uint8_t &r_pixel_size);
	Error _allocate_render_target(D3D9Texture *p_texture);
	Error _restore_window_target();
	Error _begin_frame(uint32_t p_clear_color, RID p_render_target, bool p_clear, bool p_use_depth);
	Error _set_sampler_state(uint8_t p_flags, uint32_t p_sampler = 0, uint8_t p_levels = 1);
	Error _prepare_draw(const void *p_vertices, uint32_t p_vertex_count, RID p_texture, const FFDrawParameters &p_state, Rect2i &r_viewport, bool p_constant_color = false);
	Error _draw_masked_triangles(const FixedFunctionMaskedVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_projection, RID p_color_texture, RID p_mask_texture, uint8_t p_mask_sampler_flags, const FFDrawParameters &p_state);
	Projection _get_draw_projection(const Projection &p_projection, const Rect2i &p_viewport) const;
	Error _set_table_fog(const FFFog *p_fog, FFBlendMode p_blend_mode = BLEND_MODE_DISABLED);
	Size2i _get_native_target_size() const;
	Error _create_lighting_shaders(const D3DCAPS9 &p_caps);
	void _release_lighting_shaders();
	enum { VERTEX_STREAM_CAPACITY = 12288 };
	void _release_vertex_streams();
	HRESULT _draw_vertex_stream(const void *p_vertices, uint32_t p_vertex_count, uint32_t p_stride, D3D9VertexStream &p_stream, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0, DWORD p_fvf = 0);
	HRESULT _upload_stream_indices(const uint16_t *p_indices, uint32_t p_count, uint32_t &r_start);
	Error _draw_fixed_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint16_t *p_indices, uint32_t p_element_count, const Projection &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state);
	Error _prepare_lighting_draw(const void *p_vertices, uint32_t p_vertex_count, const uint16_t *p_indices, uint32_t p_index_count, RID p_texture, const FFDrawParameters &p_state, Rect2i &r_viewport);

	Error _read_surface(IDirect3DSurface9 *p_surface, uint32_t p_width, uint32_t p_height, uint32_t *r_pixels, bool p_preserve_alpha = false);

public:
	Error initialize(void *p_native_window, const uint32_t p_width, const uint32_t p_height, const bool p_vsync) override;
	Error finalize() override;
	Error resize(const uint32_t p_width, const uint32_t p_height) override;
	Error set_use_vsync(const bool p_enabled) override;
	Error prepare_resources() override;
	const FFCapabilities &get_capabilities() const override { return _capabilities; }
	bool supports_transient_occlusion_mapping() const override { return supports_occlusion_mapping() && !_transient_occlusion_unavailable; }
	bool supports_directional_shadows() const override { return _pixel_light_limit && _capabilities.depth_buffer && _capabilities.render_target_opaque && _capabilities.max_texture_width >= 64 && _capabilities.max_texture_height >= 64 && !_shadow_shaders.unavailable; }
	Error draw_shadow_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_projection, RID p_texture, const FFDrawParameters &p_state) override;
	bool supports_texture_mask() const override { return _capabilities.max_texture_stages >= 2; }
	uint32_t get_max_per_pixel_lights() const override { return _pixel_light_limit; }
	bool supports_lit_geometry() const override { return _lighting_geometry_shader != nullptr; }
	bool supports_normal_mapping() const override { return supports_lit_geometry() && !_normal_map_unavailable && _capabilities.max_texture_stages >= 2; }
	bool supports_transient_normal_mapping() const override { return supports_normal_mapping() && !_transient_normal_unavailable; }
	bool supports_transient_normal_mapped_occlusion() const override { return supports_transient_normal_mapping() && supports_normal_mapped_occlusion() && !_transient_normal_occlusion_unavailable && _capabilities.max_texture_stages >= 3; }
	bool supports_normal_mapped_occlusion() const override { return supports_normal_mapping() && !_normal_occlusion_shaders.unavailable; }
	bool supports_occlusion_mapping() const override { return supports_lit_geometry() && !_occlusion_shaders.unavailable; }
	Error geometry_create_normal_mapped(const Vector<FixedFunctionNormalMapVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) override;
	Error draw_normal_mapped_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters &p_normal, const FFOcclusionMapParameters *p_occlusion = nullptr, uint32_t p_lod = 0) override;
	Error draw_occlusion_mapped_triangles(const FixedFunctionTransientOcclusionVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFOcclusionMapParameters &p_occlusion, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0) override;
	Error draw_normal_mapped_triangles(const FixedFunctionTransientNormalMapVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters &p_normal, const FFOcclusionMapParameters *p_occlusion = nullptr, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0) override;
	uint32_t get_width() const override { return _width; }
	uint32_t get_height() const override { return _height; }
	uint32_t get_target_width() const override { return _render_target.is_valid() ? _render_width : _width; }
	uint32_t get_target_height() const override { return _render_target.is_valid() ? _render_height : _height; }
	int32_t get_last_result() const { return static_cast<int32_t>(_last_result); }
	Error texture_get_info(RID p_texture, FFTextureInfo &r_info) const override;
	bool supports_compressed_texture(Image::Format p_format) const override;
	uint64_t get_texture_memory_usage() const override;

	Error texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame = false) override;
	Error texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent = false) override;
	bool texture_is_render_target_ready(RID p_texture) const override;
	Error texture_free(RID p_texture) override;

	Error geometry_create(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) override;
	Error geometry_create_lod_batch(const FixedFunctionGeometryData::FFGeometryBatch &p_batch, RID &r_geometry) override;
	Error geometry_create_lit(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) override;
	uint32_t get_max_vertex_lights() const override { return _vertex_light_limit; }
	bool supports_vertex_ambient_occlusion(const Color &p_ambient = Color()) const override { return _vertex_light_limit && can_pack_vertex_ambient_color(p_ambient); }
	Error draw_vertex_lit_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state, uint32_t p_lod = 0) override;
	uint32_t get_max_streamed_vertex_lights() const override { return _vertex_light_limit; }
	Error draw_vertex_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, const Color &p_color, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0) override;
	Error geometry_free(RID p_geometry) override;
	Error geometry_get_draw_info(RID p_geometry, FFGeometryDrawInfo &r_info) const override;
	uint64_t get_geometry_memory_usage() const override;
	Error draw_geometry(RID p_geometry, const Projection &p_clip_from_model, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters(), const Color *p_color_override = nullptr, uint32_t p_lod = 0) override;

	Error begin_frame(uint32_t p_clear_color, RID p_render_target = RID(), bool p_clear = true, bool p_use_depth = false) override;
	Error clear_color(uint32_t p_color) override;
	Error clear_depth(float p_depth = 1.0f) override;
	Error draw_triangles(const FixedFunctionVertex *p_vertices, const uint32_t p_vertex_count, const Projection &p_clip_from_model, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters()) override;
	bool supports_indexed_triangles() const override { return true; }
	Error draw_indexed_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count, const Projection &p_projection, RID p_texture, const FFDrawParameters &p_state) override;
	Error draw_masked_triangles(const FixedFunctionMaskedVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_model, RID p_color_texture, RID p_mask_texture, uint8_t p_mask_sampler_flags, const FFDrawParameters &p_state) override;
	Error draw_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0) override;
	Error draw_lit_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFOcclusionMapParameters *p_occlusion = nullptr, uint32_t p_lod = 0) override;
	Error end_frame(const bool p_present = true, bool p_discard = false) override;
	Error texture_read_render_target(RID p_texture, uint32_t *r_pixels, size_t p_pixel_count) override;
	Error read_frame(uint32_t *r_pixels, const size_t p_pixel_count) override;

	FixedFunctionGraphicsDeviceD3D9();
	~FixedFunctionGraphicsDeviceD3D9() override;
};
