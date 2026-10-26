#pragma once

#include "core/os/thread.h"
#include "core/templates/list.h"
#include "core/templates/rid_owner.h"
#include "drivers/fixed_function/graphics_device.h"
#include "drivers/opengl1/opengl1_context.h"

class FixedFunctionGraphicsDeviceOpenGL1 : public FixedFunctionGraphicsDevice {
	friend class TestFixedFunctionOpenGL1Mipmaps;
	friend class TestFixedFunctionOpenGL1Probe;
	Error _read_pixels(const Size2i &p_size, uint32_t *r_pixels, size_t p_pixel_count, bool p_flip_y, bool p_preserve_alpha = false);

public:
	enum GL11OptionalFeatures : uint8_t {
		FEATURE_FRAMEBUFFER = 1 << 0,
		FEATURE_NPOT_TEXTURE = 1 << 1,
		FEATURE_SEPARATE_ALPHA = 1 << 2,
		FEATURE_S3TC = 1 << 3,
		FEATURE_ALL = FEATURE_FRAMEBUFFER | FEATURE_NPOT_TEXTURE | FEATURE_SEPARATE_ALPHA | FEATURE_S3TC,
	};

private:
	struct GL11FramebufferAPI;
	struct GL11BlendAPI;
	struct GL11GeometryBufferAPI;
	enum GL11TextureView {
		TEXTURE_RGBA,
		TEXTURE_RGB,
		TEXTURE_ALPHA,
		TEXTURE_VIEW_COUNT,
	};
	struct GL11Texture {
		Size2i size;
		Size2i upload_size;
		Ref<Image> source;
		RID rid;
		unsigned int names[TEXTURE_VIEW_COUNT];
		unsigned int framebuffer;
		unsigned int depth;
		Image::Format compressed_format = Image::FORMAT_MAX;
		bool rendered;
		bool depth_initialized;
		bool transparent;
		uint8_t dirty_views;
		uint8_t uploaded_levels[TEXTURE_VIEW_COUNT];

		GL11Texture() :
				names(), framebuffer(0), depth(0), rendered(false), depth_initialized(false), transparent(false), dirty_views(0), uploaded_levels() {}
	};

	struct GL11GeometryBatch {
		Vector<FixedFunctionVertex> vertices;
		Vector<FixedFunctionLightingVertex> lit_vertices;
		Vector<uint16_t> indices;
		Vector<uint8_t> colors;
		Vector<uint32_t> lod_offsets;
		unsigned int buffers[2] = {};
		uint32_t index_count = 0;
	};
	struct GL11Geometry {
		Vector<GL11GeometryBatch> batches;
		FFGeometryDrawInfo info;
		bool lit = false;
	};

	FixedFunctionOpenGL1Context &_context;
	// RID lookup lacks a const API. Readiness, memory, and draw-info queries only read the records.
	mutable RID_PtrOwner<GL11Texture> _texture_owner;
	mutable RID_PtrOwner<GL11Geometry> _geometry_owner;
	List<GL11Texture *> _textures;
	Vector<uint8_t> _draw_colors;
	GL11FramebufferAPI *_framebuffers;
	GL11BlendAPI *_blending;
	GL11GeometryBufferAPI *_geometry_buffers = nullptr;
	FixedFunctionOpenGL1Context::ProcAddress _compressed_texture_upload = nullptr;
	GL11Texture *_render_target;
	FFCapabilities _capabilities;
	Thread::ID _owner_thread;
	Size2i _size;
	Size2i _max_viewport;
	Color _fog_color;
	uint32_t _max_renderbuffer_size;
	float _fog_begin = 0;
	float _fog_end = 0;
	uint32_t _vertex_light_limit = 0;
	uint32_t _enabled_vertex_lights = 0;
	float _model_view_scale = 1;
	uint8_t _optional_features;
	bool _initialized;
	bool _frame_active;
	bool _frame_readable;
	bool _depth_active;
	bool _window_has_depth;
	bool _window_depth_initialized;
	bool _clamp_to_edge;
	bool _owns_context;
	bool _fog_enabled = false;
	bool _model_view_loaded = false;

	FixedFunctionGraphicsDeviceOpenGL1(const FixedFunctionGraphicsDeviceOpenGL1 &);
	FixedFunctionGraphicsDeviceOpenGL1 &operator=(const FixedFunctionGraphicsDeviceOpenGL1 &);

	struct GL11CurrentContext {
		FixedFunctionOpenGL1Context &context;
		const Error error;
		explicit GL11CurrentContext(FixedFunctionGraphicsDeviceOpenGL1 &p_device);
		~GL11CurrentContext();
	};
	Error _initialize_gl(uint32_t p_width, uint32_t p_height);
	void _initialize_framebuffers(bool p_core, bool p_ext);
	void _clear_framebuffer_api();
	void _initialize_blending(bool p_core, bool p_ext);
	void _clear_blend_api();
	void _initialize_geometry_buffers(bool p_core, bool p_arb);
	void _clear_geometry_buffer_api();
	bool _upload_geometry_batch(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint16_t> &p_indices, GL11GeometryBatch &r_batch);
	bool _upload_geometry_buffers(const void *p_vertices, ptrdiff_t p_vertex_bytes, const Vector<uint16_t> &p_indices, GL11GeometryBatch &r_batch);
	Error _create_geometry(const Vector<FixedFunctionGeometryData::FFGeometryBatch> &p_batches, RID &r_geometry);
	void _release_geometry_buffers(const GL11Geometry &p_geometry);
	Error _draw_buffered_geometry_batch(const GL11GeometryBatch &p_batch, const FixedFunctionGeometryData::FFIndexRange &p_range, bool p_vertex_colors);
	Error _allocate_framebuffer(GL11Texture &r_texture);
	Error _allocate_framebuffer_depth(GL11Texture &r_texture);
	void _bind_framebuffer(const GL11Texture *p_texture);
	void _delete_framebuffer(GL11Texture &p_texture);
	Error _copy_framebuffer_view(GL11Texture &p_texture, GL11TextureView p_view);
	uint64_t _texture_bytes(const GL11Texture &p_texture) const;
	Error _make_current();
	static bool _has_extension(const char *p_extensions, const char *p_name);
	static Error _check_gl_error();
	static void _set_enabled(unsigned int p_capability, bool p_enabled);
	Error _upload_texture_view(GL11Texture &p_texture, GL11TextureView p_view);
	Error _upload_texture_view_data(GL11Texture &p_texture, GL11TextureView p_view, const Vector<uint8_t> &p_pixels, uint8_t p_levels, Image::Format p_format = Image::FORMAT_RGBA8);
	Error _upload_compressed_texture_levels(const GL11Texture &p_texture, const Vector<uint8_t> &p_blocks, uint8_t p_levels, Image::Format p_format);
	Error _upload_texture_levels(const GL11Texture &p_texture, GL11TextureView p_view, const Vector<uint8_t> &p_pixels, uint8_t p_levels);
	Error _bind_texture(GL11Texture *p_texture, const FFDrawParameters &p_state);
	void _apply_draw_state(const FFDrawParameters &p_state, const Rect2i &p_clip);
	Error _draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count, const Projection &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state);
	Error _prepare_draw(const Projection &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state);
	Error _draw_vertex_lighting(const GL11Geometry *p_geometry, const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const uint16_t *p_indices, uint32_t p_index_count, const Projection &p_projection, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state, uint32_t p_lod);
	static void _store_color(uint32_t p_argb, uint8_t *r_rgba) {
		r_rgba[0] = uint8_t(p_argb >> 16);
		r_rgba[1] = uint8_t(p_argb >> 8);
		r_rgba[2] = uint8_t(p_argb);
		r_rgba[3] = uint8_t(p_argb >> 24);
	}
	static void _store_colors(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, uint8_t *r_colors);
	Error _draw_arrays(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_colors, const void *p_indices, uint32_t p_index_count);
	void _apply_blend_state(const FFDrawParameters &p_state);

protected:
	// Takes ownership of a non-null context created with memnew.
	explicit FixedFunctionGraphicsDeviceOpenGL1(FixedFunctionOpenGL1Context *p_owned_context, uint8_t p_optional_features = FEATURE_ALL);

public:
	explicit FixedFunctionGraphicsDeviceOpenGL1(FixedFunctionOpenGL1Context &p_context, uint8_t p_optional_features = FEATURE_ALL);
	~FixedFunctionGraphicsDeviceOpenGL1() override;

	Error initialize(void *p_native_window, uint32_t p_width, uint32_t p_height, bool p_vsync) override;
	Error finalize() override;
	Error resize(uint32_t p_width, uint32_t p_height) override;
	Error set_use_vsync(bool p_enabled) override;
	Error prepare_resources() override;
	const FFCapabilities &get_capabilities() const override { return _capabilities; }
	uint32_t get_width() const override { return _size.x; }
	uint32_t get_height() const override { return _size.y; }
	uint32_t get_target_width() const override { return _render_target ? _render_target->size.x : get_width(); }
	uint32_t get_target_height() const override { return _render_target ? _render_target->size.y : get_height(); }

	bool accepts_source_texture_images() const override { return true; }
	bool supports_compressed_texture(Image::Format p_format) const override;
	Error texture_get_info(RID p_texture, FFTextureInfo &r_info) const override;
	uint64_t get_texture_memory_usage() const override;
	Error texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame = false) override;
	Error texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent = false) override;
	bool texture_is_render_target_ready(RID p_texture) const override;
	Error texture_free(RID p_texture) override;

	Error begin_frame(uint32_t p_clear_color, RID p_render_target = RID(), bool p_clear = true, bool p_use_depth = false) override;
	Error clear_color(uint32_t p_color) override;
	Error clear_depth(float p_depth = 1.0f) override;
	Error draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_model, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters()) override;
	Error geometry_create(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) override;
	Error geometry_create_lod_batch(const FixedFunctionGeometryData::FFGeometryBatch &p_batch, RID &r_geometry) override;
	uint32_t get_max_vertex_lights() const override { return _vertex_light_limit; }
	bool supports_vertex_ambient_occlusion(const Color &p_ambient = Color()) const override { return true; }
	Error geometry_create_lit(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) override;
	Error draw_vertex_lit_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state, uint32_t p_lod = 0) override;
	uint32_t get_max_streamed_vertex_lights() const override { return _vertex_light_limit; }
	Error draw_vertex_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, const Color &p_color, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0) override;
	Error geometry_free(RID p_geometry) override;
	Error geometry_get_draw_info(RID p_geometry, FFGeometryDrawInfo &r_info) const override;
	Error draw_geometry(RID p_geometry, const Projection &p_clip_from_model, RID p_texture, const FFDrawParameters &p_state, const Color *p_color_override, uint32_t p_lod = 0) override;
	bool supports_indexed_triangles() const override { return true; }
	Error draw_indexed_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count, const Projection &p_projection, RID p_texture, const FFDrawParameters &p_state) override;
	Error end_frame(bool p_present = true, bool p_discard = false) override;
	Error texture_read_render_target(RID p_texture, uint32_t *r_pixels, size_t p_pixel_count) override;
	Error read_frame(uint32_t *p_pixels, size_t p_pixel_count) override;
};
