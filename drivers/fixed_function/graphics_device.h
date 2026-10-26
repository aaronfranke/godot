#pragma once

#include "core/error/error_list.h"
#include "core/io/image.h"
#include "core/math/projection.h"
#include "core/math/rect2i.h"
#include "core/templates/rid.h"
#include "drivers/fixed_function/geometry_data.h"

// The platform owns the native window. All calls and destruction occur on its thread.
class FixedFunctionGraphicsDevice {
protected:
	String _adapter_name;
	uint32_t _adapter_vendor_id;

	// Pack matrix precision at the device boundary; this does not change coordinate conventions.
	static _FORCE_INLINE_ void store_camera(const Projection &p_matrix, float *p_array) {
		for (int column = 0; column < 4; column++) {
			for (int row = 0; row < 4; row++) {
				p_array[column * 4 + row] = static_cast<float>(p_matrix.columns[column][row]);
			}
		}
	}

public:
	enum { MAX_PIXEL_LIGHTS = 8 };
	// Four native float constant registers per light; these are upload copies,
	// not replacements for the renderer's real_t/double light snapshots.
	struct FFPixelLight {
		float color_type[4];
		float position_range[4];
		float direction_attenuation[4];
		float cone[4];
	};
	struct FFDirectionalShadowParameters {
		// Receivers use camera-relative positions in world axes, like the light snapshots.
		Projection clip_from_world;
		Color color;
		RID texture;
		float bias;
		float normal_bias;
		uint32_t light_index;
		FFDirectionalShadowParameters();
	};
	struct FFPixelLighting {
		FFPixelLight lights[MAX_PIXEL_LIGHTS];
		FFDirectionalShadowParameters shadow;
		Basis camera_basis;
		Color ambient;
		uint32_t light_count;
		bool vertex_ambient_occlusion = false; // Native vertex lighting only.
	};
	struct FFOcclusionMapParameters {
		Vector2 uv_scale;
		Vector2 uv_offset;
		Color weights;
		RID texture;
		float light_affect;
		uint8_t sampler_flags;
		// The secondary UV occupies the normal UV slot or the lit vertex's RG slot.
		bool uses_secondary_uv;
	};
	struct FFTextureInfo {
		uint64_t bytes;
		Size2i size;
		FFTextureInfo();
	};

	enum FFBlendMode : uint8_t {
		BLEND_MODE_DISABLED,
		BLEND_MODE_MIX,
		BLEND_MODE_PREMULTIPLIED_ALPHA,
		BLEND_MODE_ADD,
		BLEND_MODE_MUL,
		BLEND_MODE_ADD_UNWEIGHTED, // RGB adds independently of the alpha test input.
		BLEND_MODE_MAX,
	};

	enum FFSamplerFlags : uint8_t {
		SAMPLER_FILTER_LINEAR = 1 << 0,
		SAMPLER_REPEAT_U = 1 << 1,
		SAMPLER_REPEAT_V = 1 << 2,
		SAMPLER_MIPMAPS = 1 << 3,
		SAMPLER_REPEAT_MIRRORED = 1 << 4, // Applies only to axes enabled for repeat.
		SAMPLER_FILTER_ANISOTROPIC = 1 << 5,
		SAMPLER_FLAGS_MASK = SAMPLER_FILTER_LINEAR | SAMPLER_REPEAT_U | SAMPLER_REPEAT_V | SAMPLER_MIPMAPS | SAMPLER_REPEAT_MIRRORED | SAMPLER_FILTER_ANISOTROPIC,
	};

	enum FFMipFilter : uint8_t {
		MIP_FILTER_NONE = 0,
		MIP_FILTER_POINT = 1 << 0,
		MIP_FILTER_LINEAR = 1 << 1,
	};
	enum FFCullMode : uint8_t {
		CULL_DISABLED,
		CULL_BACK,
		CULL_FRONT,
		CULL_MODE_MAX,
	};

	struct FFFog {
		Color color;
		float begin;
		float end;
		// Distance thresholds in D3D device Z, for table fog without eye-relative W.
		float projected_begin;
		float projected_end;
		bool orthogonal;

		Color get_blend_color(FFBlendMode p_blend_mode) const;
	};

	struct FFFogTransforms {
		const Projection &clip_from_view;
		const Transform3D &view_from_model;
	};

	struct FFDrawParameters {
		const FFFog *fog;
		// Optional eye-space transforms for fog on retained model-space draws.
		const FFFogTransforms *fog_transforms;
		Rect2i clip_rect;
		float alpha_cutoff;
		FFBlendMode blend_mode;
		uint8_t sampler_flags;
		bool alpha_cutoff_enabled;
		bool clip_enabled;
		bool depth_test_enabled;
		bool depth_write_enabled;
		FFCullMode cull_mode;
		bool texture_alpha_enabled : 1;
		bool texture_rgb_enabled : 1;
		bool font_glyph : 1;
		bool alpha_write_enabled : 1;

		FFDrawParameters() :
				fog(nullptr),
				fog_transforms(nullptr),
				alpha_cutoff(0.5f),
				blend_mode(BLEND_MODE_DISABLED),
				sampler_flags(0),
				alpha_cutoff_enabled(false),
				clip_enabled(false),
				depth_test_enabled(false),
				depth_write_enabled(false),
				cull_mode(CULL_DISABLED),
				texture_alpha_enabled(true),
				texture_rgb_enabled(true),
				font_glyph(false),
				alpha_write_enabled(true) {}
	};

	struct FFCapabilities {
		uint32_t max_texture_width;
		uint32_t max_texture_height;
		uint32_t max_texture_stages;
		uint32_t max_primitives_per_draw;
		bool hardware_transform_and_light;
		bool texture_power_of_two;
		bool texture_square_only;
		bool blend_mix;
		bool blend_premultiplied_alpha;
		bool blend_add;
		bool blend_mul;
		bool alpha_cutoff;
		bool render_target_opaque;
		bool render_target_scaled;
		bool depth_buffer;
		bool blend_add_unweighted : 1;
		bool sky_solid_hemispheres : 1;
		bool sampler_filter_linear : 1;
		bool sampler_repeat : 1;
		bool sampler_repeat_mirrored : 1;
		bool sampler_address_independent : 1;
		bool render_target_transparent : 1;
		bool canvas_fill_blit : 1;
		uint8_t sampler_mip_filters;
		bool cull_back;
		bool cull_front;
		uint8_t sampler_max_anisotropy;

		FFCapabilities() :
				max_texture_width(0),
				max_texture_height(0),
				max_texture_stages(0),
				max_primitives_per_draw(0),
				hardware_transform_and_light(false),
				texture_power_of_two(false),
				texture_square_only(false),
				blend_mix(false),
				blend_premultiplied_alpha(false),
				blend_add(false),
				blend_mul(false),
				alpha_cutoff(false),
				render_target_opaque(false),
				render_target_scaled(false),
				depth_buffer(false),
				blend_add_unweighted(false),
				sky_solid_hemispheres(false),
				sampler_filter_linear(false),
				sampler_repeat(false),
				sampler_repeat_mirrored(false),
				sampler_address_independent(false),
				render_target_transparent(false),
				canvas_fill_blit(false),
				sampler_mip_filters(MIP_FILTER_NONE),
				cull_back(false),
				cull_front(false),
				sampler_max_anisotropy(1) {}
	};

	struct FFLitGeometryParameters {
		Vector2 albedo_uv_scale;
		Vector2 albedo_uv_offset;
		Transform3D view_from_anchor;
		Basis normal_from_model;
		Color color;
		FFLitGeometryParameters();
	};
	struct FFNormalMapParameters {
		Vector2 uv_scale;
		Vector2 uv_offset;
		RID texture;
		float strength;
		uint8_t sampler_flags;
		bool flip_v;
		bool albedo_uses_normal_uv;
		FFNormalMapParameters();
	};
	struct FFGeometryDrawInfo {
		uint32_t vertex_count;
		uint32_t primitive_count;
		uint32_t draw_calls;
		uint32_t lod_count;
		FFGeometryDrawInfo();
	};

protected:
	Rect2i get_draw_viewport(const FFDrawParameters &p_state) const;
	Projection get_viewport_projection(const Projection &p_projection, const Rect2i &p_viewport) const;
	static Projection get_viewport_projection(const Projection &p_projection, const Rect2i &p_viewport, const Size2i &p_target_size);
	Error validate_triangle_indices(uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count) const;
	static uint32_t get_alpha_reference(float p_cutoff);
	Error validate_draw_state(const FFDrawParameters &p_state) const;
	Error validate_texture_image(const Ref<Image> &p_image) const;
	// Returns a native upload copy; never modifies the source image or its authored mipmaps.
	Error prepare_texture_upload(const Ref<Image> &p_image, Ref<Image> &r_upload) const;
	static Error copy_texture_readback(const uint8_t *p_source, const Size2i &p_native_size, size_t p_pitch, const Size2i &p_logical_size, uint32_t *r_pixels, size_t p_pixel_count, bool p_preserve_alpha = true);
	static bool copy_compressed_texture_rows(void *p_destination, int p_pitch, const uint8_t *p_source, uint32_t p_row_bytes, uint32_t p_rows);
	FFMipFilter get_mip_filter(uint8_t p_flags, uint8_t p_levels) const;
	uint8_t get_sampler_anisotropy(uint8_t p_flags) const;

public:
	static Error pack_geometry_color(const Color &p_color, uint32_t &r_color);

	virtual Error initialize(void *p_native_window, const uint32_t p_width, const uint32_t p_height, const bool p_vsync) = 0;
	virtual Error finalize() = 0;

	// A zero dimension suspends drawing until a nonzero size is supplied.
	virtual Error resize(const uint32_t p_width, const uint32_t p_height) = 0;
	virtual Error set_use_vsync(bool p_enabled) = 0;
	// Settle queued reset/recovery while idle, before resource updates. Never opens a scene,
	// clears, presents, or advances clocks. ERR_SKIP means retry later; an active pass is busy.
	virtual Error prepare_resources() = 0;
	virtual const FFCapabilities &get_capabilities() const = 0;
	// Strict native allocation validation. Source images use get_texture_upload_size instead.
	virtual Error validate_texture_size(uint32_t p_width, uint32_t p_height) const;
	// Logical source dimensions are independent of legal native allocation dimensions.
	static Error get_texture_upload_size(const FFCapabilities &p_caps, uint32_t p_width, uint32_t p_height, Size2i &r_size);
	Error get_texture_upload_size(uint32_t p_width, uint32_t p_height, Size2i &r_size) const;
	// Sizes must be positive; p_clip is already clamped to the logical target.
	static Rect2i get_texture_native_clip(const Rect2i &p_clip, const Size2i &p_logical, const Size2i &p_native);
	// Zero keeps the per-vertex path. This query reports successfully created
	// optional pipelines and never initializes or polls a native device.
	virtual uint32_t get_max_per_pixel_lights() const { return 0; }
	virtual bool supports_lit_geometry() const { return false; }
	virtual bool supports_normal_mapping() const { return false; }
	virtual bool supports_transient_normal_mapping() const { return false; }
	virtual bool supports_transient_normal_mapped_occlusion() const { return false; }
	virtual bool supports_texture_mask() const { return false; }
	virtual bool supports_normal_mapped_occlusion() const { return false; }
	virtual bool supports_occlusion_mapping() const { return false; }
	virtual bool supports_directional_shadows() const { return false; }
	// Optional encoded-depth pass into an ordinary opaque target. Projection and CPU
	// vertices retain Godot conventions; only the native device converts clip space.
	virtual Error draw_shadow_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_projection, RID p_texture, const FFDrawParameters &p_state) { return ERR_UNAVAILABLE; }
	virtual bool supports_transient_occlusion_mapping() const { return false; }
	virtual uint32_t get_width() const = 0;
	virtual uint32_t get_height() const = 0;
	// Optional platform sizing constraint; zero leaves the window unconstrained.
	virtual Size2i get_max_window_size() const { return Size2i(); }
	virtual Size2i get_window_size(const Size2i &p_window_size) const { return p_window_size; }
	// Window dimensions remain stable across passes; these query the current drawing target.
	virtual uint32_t get_target_width() const = 0;
	virtual uint32_t get_target_height() const = 0;
	// Cached diagnostics only; these queries never poll or reset the device.
	String get_adapter_name() const;
	String get_adapter_vendor() const;
	uint32_t get_adapter_vendor_id() const { return _adapter_vendor_id; }
	// Native target payload estimates include allocated depth and exclude pitch, driver overhead, window buffers
	// and CPU recovery copies. Zero means absent or unavailable, not physical VRAM residency.
	virtual Error texture_get_info(RID p_texture, FFTextureInfo &r_info) const;
	virtual uint64_t get_texture_memory_usage() const { return 0; }
	// Source-aware devices choose their upload format while keeping an accepted image
	// snapshot available for views that may be requested later.
	virtual bool accepts_source_texture_images() const { return false; }
	virtual bool supports_compressed_texture(Image::Format p_format) const { return false; }
	bool can_upload_compressed_texture(const Ref<Image> &p_image) const;
	static Error prepare_rgba8_upload(const Ref<Image> &p_image, Ref<Image> &r_upload, bool p_rgbe_to_srgb = false);

	// Upload RGBA8 or supported compressed blocks, with supplied mip levels where supported.
	// The device owns a snapshot.
	// Create and free ordinary textures outside a frame. Discard RIDs after free/finalize.
	// Failed creation leaves the output unchanged; failed free retains ownership.
	// A newly allocated canvas coverage texture may be uploaded without closing the current draw pass.
	virtual Error texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame = false) = 0;
	// render_target_scaled permits proportional backing while retaining logical dimensions.
	// Single-level 2D color target. Its RID is also a sampleable device texture.
	// Transparent targets require render_target_transparent and store premultiplied RGBA,
	// matching Godot's Compatibility viewports. Opaque targets read back alpha one.
	// Contents are unavailable until a successful render pass, and become unavailable after reset.
	virtual Error texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent = false) = 0;
	// Last observed readiness only: prepare before scheduling viewport updates. Invalid,
	// unrendered, active, or pending-recovery targets return false; this query does not poll/reset.
	virtual bool texture_is_render_target_ready(RID p_texture) const = 0;
	virtual Error texture_free(RID p_texture) = 0;

	// Immutable, indexed native upload copies. CPU model geometry remains owned by
	// mesh storage. Creation/free stay outside passes; creation does not poll/reset
	// or upload. An empty index array means a nonindexed triangle list. Backends
	// partition arbitrary source indices to their own draw/vertex limits.
	// Successful creation retains a snapshot and leaves native upload lazy. Reset
	// preserves the RID/source, releases buffers and restores on the next draw.
	// Failed creation leaves r_geometry unchanged. Discard RIDs after free/finalize.
	virtual Error geometry_create(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry);
	virtual Error geometry_create_lod_batch(const FixedFunctionGeometryData::FFGeometryBatch &p_batch, RID &r_geometry);
	virtual Error geometry_create_lit(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry);
	virtual Error geometry_create_normal_mapped(const Vector<FixedFunctionNormalMapVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry);
	virtual Error draw_normal_mapped_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters &p_normal, const FFOcclusionMapParameters *p_occlusion = nullptr, uint32_t p_lod = 0);
	// Streamed view positions, world-space normals/frame and final per-channel UVs.
	// Optional rejection submits nothing; preserve ordinary lighting as the fallback.
	virtual Error draw_occlusion_mapped_triangles(const FixedFunctionTransientOcclusionVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFOcclusionMapParameters &p_occlusion, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0);
	virtual Error draw_normal_mapped_triangles(const FixedFunctionTransientNormalMapVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters &p_normal, const FFOcclusionMapParameters *p_occlusion = nullptr, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0);
	virtual Error draw_lit_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFOcclusionMapParameters *p_occlusion = nullptr, uint32_t p_lod = 0);
	virtual Error geometry_free(RID p_geometry);
	// Cached indexed corner/primitive/native-batch counts; no device polling.
	virtual Error geometry_get_draw_info(RID p_geometry, FFGeometryDrawInfo &r_info) const;
	// Native buffer payload only, excluding CPU copies and transient upload streams;
	// neither physical VRAM residency nor driver overhead (D3D7 may use system RAM).
	virtual uint64_t get_geometry_memory_usage() const { return 0; }

	// ERR_SKIP means the device cannot render now; retry on a later frame.
	// Depth is allocated only when requested. Clear passes reset it to far (1); load passes
	// preserve it, initializing a newly allocated depth buffer to far. Color-only passes
	// neither allocate nor clear depth. Reset discards depth together with target contents.
	// One target per native pass. Empty selects the window; target switches stay outside scenes.
	// Loading requires completed contents, retained only until reset or window presentation.
	virtual Error begin_frame(const uint32_t p_clear_color, RID p_render_target = RID(), bool p_clear = true, bool p_use_depth = false) = 0;
	// Clear the whole active color target, independently of the previous draw's clipping.
	virtual Error clear_color(uint32_t p_color) = 0;
	// Requires a depth-enabled pass. Clears the entire depth buffer, independent of clipping.
	virtual Error clear_depth(float p_depth = 1.0f);
	// Optional native copy of a completed opaque target over the active window pass.
	virtual Error copy_render_target_to_window(RID p_texture) { return ERR_UNAVAILABLE; }
	// Optionally fill opaque canvas geometry in logical target coordinates.
	virtual Error fill_canvas_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const FFDrawParameters &p_state) { return ERR_UNAVAILABLE; }
	// Optionally copy a completed opaque render target into a canvas rectangle.
	virtual Error copy_canvas_render_target(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, RID p_texture, const FFDrawParameters &p_state) { return ERR_UNAVAILABLE; }
	virtual bool prefers_canvas_glyph_blits() const { return false; }
	virtual Error blit_canvas_glyph(RID p_texture, const Rect2 &p_uv, const Rect2 &p_bounds, uint32_t p_color, const FFDrawParameters &p_state) { return ERR_UNAVAILABLE; }
	// Godot column-vector projections: -w <= z <= w, with conventional near-to-far depth.
	// NDC (-1, +1) is the target's top-left edge; pixel centers are at (x + 0.5, y + 0.5).
	// Native depth, pixel-center and matrix-storage conversions belong to the backend.
	// UV origin is top-left. Textures modulate vertex color; sampling defaults to nearest/clamp.
	// Optional bilinear min/mag filtering and U/V repeat are capability-checked per draw.
	// Mip sampling uses uploaded levels when enabled and supported. Anisotropy is separate.
	// An empty texture RID selects vertex color alone.
	// MIX: RGB = source * source_alpha + destination * (1 - source_alpha).
	// PREMULTIPLIED_ALPHA: RGB = source + destination * (1 - source_alpha).
	// Premultiplied inputs must already have RGB multiplied by alpha, including modulation.
	// ADD: RGB = source * source_alpha + destination. MUL: RGB = source * destination.
	// Optional cutoff rejects modulated alpha below the threshold, without changing surviving alpha.
	// It uses an 8-bit native reference; finite thresholds <= 0 pass all, and > 1 discard all.
	// Transparent targets accumulate coverage for MIX/PREMULTIPLIED_ALPHA; ADD/MUL
	// follow Godot's canvas alpha factors. alpha_write_enabled=false preserves destination
	// coverage for auxiliary color passes. Opaque targets do not expose accumulated alpha.
	// Optional clipping uses target pixels, top-left origin, and exclusive right/bottom bounds.
	// It intersects the target without changing the projection's screen mapping. Empty clips draw nothing.
	// Enabled negative sizes are invalid; disabled clipping ignores its rectangle.
	// Optional depth tests use conventional near-to-far LESS_EQUAL; writes are independent.
	// Either flag requires a depth-enabled pass. Both default off for canvas drawing.
	// Sampling the active target is invalid feedback.
	// Solid background with horizon Y normalized from top to bottom. Reports completed fills.
	// ERR_UNAVAILABLE leaves the target's default background available for scene drawing.
	virtual Error draw_sky_hemispheres(real_t p_horizon_y, uint32_t p_sky_color, uint32_t p_ground_color, uint32_t &r_draw_calls) { return ERR_UNAVAILABLE; }
	virtual Error draw_triangles(const FixedFunctionVertex *p_vertices, const uint32_t p_vertex_count, const Projection &p_clip_from_model, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters()) = 0;
	virtual bool supports_indexed_triangles() const { return false; }
	virtual Error draw_indexed_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_vertex_count, const uint8_t *p_indices, uint32_t p_index_count, const Projection &p_projection, RID p_texture, const FFDrawParameters &p_state);
	// Optional independent RGB/alpha textures. RGB = vertex * color texture; alpha = vertex * mask.
	// p_state supplies the color sampler; the mask has its own UVs/sampler. ERR_UNAVAILABLE submits nothing.
	virtual Error draw_masked_triangles(const FixedFunctionMaskedVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_model, RID p_color_texture, RID p_mask_texture, uint8_t p_mask_sampler_flags, const FFDrawParameters &p_state);
	// Same state/coordinate contract, using retained indexed geometry. A backend
	// may issue several native draws. ERR_UNAVAILABLE never follows submission.
	// A color override replaces vertex RGBA before optional texture modulation.
	virtual Error draw_geometry(RID p_geometry, const Projection &p_clip_from_model, RID p_texture = RID(), const FFDrawParameters &p_state = FFDrawParameters(), const Color *p_color_override = nullptr, uint32_t p_lod = 0);
	// Optional native transform-and-light path for geometry from geometry_create_lit.
	// Zero lights means unavailable. p_lighting reuses the per-pixel light layout, but
	// every position, direction and reciprocal range is in the geometry's own model
	// space and camera_basis is unused. The draw therefore uses the same clip_from_model
	// matrix as every other retained pass, and produces identical depth. Directional
	// lights and ambient are exact per-vertex Lambert. Omni and spot falloff use the
	// nearest native attenuation, so the renderer decides whether to submit them.
	// The albedo UV transform is applied as uv * scale + offset.
	// ERR_UNAVAILABLE guarantees no geometry was submitted.
	virtual uint32_t get_max_vertex_lights() const { return 0; }
	// Native lighting can read the retained vertices' RGB as an ambient-only factor.
	virtual bool supports_vertex_ambient_occlusion(const Color &p_ambient = Color()) const { return false; }
	// Packed native ambient states cannot represent signed or HDR terms.
	static bool can_pack_vertex_ambient_color(const Color &p_ambient);
	static bool pack_vertex_ambient_color(const Color &p_ambient, uint32_t &r_color);
	virtual Error draw_vertex_lit_geometry(RID p_geometry, const Projection &p_clip_from_model, const FFPixelLighting &p_lighting, const Color &p_color, const Vector2 &p_uv_scale, const Vector2 &p_uv_offset, RID p_texture, const FFDrawParameters &p_state, uint32_t p_lod = 0);
	// Streamed native lighting takes positions, normals and lights in view space.
	// UVs are already transformed. RGB carries ambient-only occlusion when enabled.
	// ERR_UNAVAILABLE guarantees no geometry was submitted.
	virtual uint32_t get_max_streamed_vertex_lights() const { return 0; }
	virtual Error draw_vertex_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, const Color &p_color, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0);
	// Coefficient of (distance / range)^2 in 1 / (1 + k * x^2), fitted to Godot's
	// (1 - x)^attenuation falloff. Zero attenuation is constant inside the range.
	static float get_vertex_light_quadratic_attenuation(float p_attenuation);
	// Exponent of (1 - rim) fitted to Godot's 1 - rim^spot_attenuation at the cone's midpoint.
	static float get_vertex_light_spot_falloff(float p_spot_attenuation);
	// Optional SM3 path; ERR_UNAVAILABLE guarantees that no geometry was submitted.
	virtual Error draw_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_vertex_count, const Projection &p_clip_from_view, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices = nullptr, uint32_t p_index_count = 0);
	// Offscreen passes must end without presentation. An attempted presentation still closes
	// the pass but returns ERR_UNAVAILABLE.
	// Discard closes without making partial output readable/sampleable or presenting it.
	virtual Error end_frame(const bool p_present = true, bool p_discard = false) = 0;
	// Optional window readback after a window pass's end_frame(false), before presentation.
	// Rows are top to bottom, tightly packed, with opaque alpha for the window target.
	// Optional readback of a completed target, with the same top-left
	// 0xAARRGGBB pixels as read_frame. No pass may be active; output is caller-owned.
	virtual Error texture_read_render_target(RID p_texture, uint32_t *r_pixels, size_t p_pixel_count);
	virtual Error read_frame(uint32_t *p_pixels, const size_t p_pixel_count) = 0;

	FixedFunctionGraphicsDevice();
	virtual ~FixedFunctionGraphicsDevice() {}
};
