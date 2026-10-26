#ifndef FIXED_FUNCTION_TEXTURE_STORAGE_H
#define FIXED_FUNCTION_TEXTURE_STORAGE_H

#include "core/map.h"
#include "graphics_device.h"

class FixedFunctionSceneStorage;

// The borrowed device outlives storage and remains initialized until clear succeeds.
// All calls/destruction stay on its thread; native resource mutations require an idle device.
// Discard logical RIDs after accepted request_free/free/clear/destruction, as with other 3.x owners.
class FixedFunctionTextureStorage {
public:
	typedef void (*FFTextureDetectCallback)(void *);
	enum FFTextureDetection {
		DETECT_3D,
		DETECT_SRGB,
		DETECT_NORMAL,
		DETECT_MAX,
	};
	struct FFTextureUsage {
		uint64_t cpu_bytes;
		uint64_t native_bytes;
		RID texture;
		Size2i size;
		String path;
		Image::Format format;
		FFTextureUsage();
	};

	enum FFRenderTargetFlag {
		RENDER_TARGET_VFLIP,
		RENDER_TARGET_TRANSPARENT,
		RENDER_TARGET_NO_3D_EFFECTS,
		RENDER_TARGET_NO_3D,
		RENDER_TARGET_NO_SAMPLING,
		RENDER_TARGET_HDR,
		RENDER_TARGET_KEEP_3D_LINEAR,
		RENDER_TARGET_DIRECT_TO_SCREEN,
		RENDER_TARGET_USE_32_BPC_DEPTH,
		RENDER_TARGET_FLAG_MAX,
	};

private:
	struct FFTextureDetectionData {
		FFTextureDetectCallback callback;
		void *userdata;
		FFTextureDetectionData();
	};
	// MSVC RID_Data size (x86_32/x86_64): 8/16 bytes with debug pointer RIDs;
	// 12/24 bytes with release pointer RIDs or RID_HANDLES_ENABLED.
	struct FFRenderTarget : RID_Data {
		enum {
			FLAG_DRAW_FAILED = 1 << RENDER_TARGET_FLAG_MAX,
		};
		Rect2i rect;
		Size2i allocated_size;
		uint16_t flags;
		// With RID_HANDLES_ENABLED on MSVC x86_32, these bools occupy two padding
		// bytes before the first 8-byte-aligned RID, reducing the record from 64 to 56 bytes.
		bool used;
		bool redraw_on_loss;
		RID self;
		RID texture;

		FFRenderTarget();
	};

	struct FFTexture : RID_Data {
		Set<FFTexture *> proxy_owners;
		RID self;
		RID device_texture;
		uint64_t cpu_image_version = 0;
		uint64_t cpu_image_bake_after = 0;
		uint64_t cpu_sample_frame = 0;
		Size2i size;
		Size2i image_size;
		String path;
		Ref<Image> image;
		Ref<Image> pending_decoded_image;
		PoolVector<uint8_t> cpu_sample_rgba;
		FFTexture *proxy;
		FFRenderTarget *render_target;
		FFTextureDetectionData detection[DETECT_MAX];
		uint32_t flags;
		Image::Format format;
		// A nonzero average validates all three cached panorama colors.
		uint32_t panorama_average_rgba;
		uint32_t panorama_sky_rgba;
		uint32_t panorama_ground_rgba;
		uint8_t sampler_flags;
		// An explicit declaration constrains later uploads, independently of native backing.
		bool allocated;
		bool size_overridden;
		bool upload_on_use;
		bool upload_pending;
		bool upload_failed;
		bool free_pending;
		bool redraw_if_visible;

		FFTexture();
	};

	class FFUpdateScope {
		bool &_updating;
		FFUpdateScope(const FFUpdateScope &);
		FFUpdateScope &operator=(const FFUpdateScope &);

	public:
		FFUpdateScope(bool &p_updating);
		~FFUpdateScope();
	};

	FixedFunctionGraphicsDevice &_device;
	FixedFunctionSceneStorage *_scene_storage;
	// RID lookup lacks a const API; preserve our read-only public getters.
	// Const lookup/record overloads would let us remove mutable.
	mutable RID_Owner<FFTexture> _texture_owner;
	mutable RID_Owner<FFRenderTarget> _render_target_owner;
	// Derived native coverage for atlases actually used by distance-field glyphs.
	Map<const FFTexture *, RID> _distance_field_textures;
	Set<FFTexture *> _pending_uploads;
	Set<FFTexture *> _pending_releases;
	Set<FFTexture *> _cpu_sample_textures;
	List<FFTexture *> _textures;
	List<FFRenderTarget *> _render_targets;
	List<RID> _retired_device_textures;
	uint64_t _cpu_image_version = 0;
	uint64_t _frame_number = 1;
	bool _updating;
	bool _clearing;

	Ref<Image> _read_render_target(const FFTexture &p_texture) const;
	FFTexture *_get_texture(RID p_texture) const;
	FFTexture *_get_first_upload(RID p_texture) const;
	FFTexture *_get_panorama_averages(RID p_texture);
	FFRenderTarget *_get_render_target(RID p_render_target) const;
	Error _validate_render_target_change(RID p_render_target) const;
	Error _validate_allocation(const FFTexture *p_texture, const Size2i &p_size, Image::Format p_format, uint8_t p_sampler_flags) const;
	void _commit_allocation(FFTexture *p_texture, const Size2i &p_size, Image::Format p_format, uint32_t p_flags, uint8_t p_sampler_flags);
	Error _validate_data(const FFTexture *p_texture, const Ref<Image> &p_image) const;
	// A null r_decoded validates decoder availability and leaves decoding to the upload.
	Error _prepare_data(const Ref<Image> &p_image, Ref<Image> &r_snapshot, Ref<Image> *r_decoded) const;
	void _commit_data(FFTexture *p_texture, const Ref<Image> &p_snapshot);
	void _discard_cpu_sample(FFTexture *p_texture);
	static bool _is_upload_resource_error(Error p_error);
	Error _create_device_texture(FFTexture *p_texture, const Ref<Image> &p_image, RID &r_native, bool p_allow_during_frame);
	Error _upload_data(FFTexture *p_texture, const Ref<Image> &p_image, bool *r_creation_failed = nullptr);
	Error _patch_data(const FFTexture *p_texture, const Ref<Image> &p_image, const Rect2i &p_source, const Point2i &p_destination, Ref<Image> &r_image) const;
	void _queue_upload(FFTexture *p_texture);
	void _cancel_upload(FFTexture *p_texture);
	Error _release_retired_textures();
	Error _update_pending(bool p_release_targets);
	void _invalidate_distance_field(const FFTexture *p_texture);
	Error _release_device_texture(FFTexture *p_texture);
	void _clear_records();
	void _free_texture(FFTexture *p_texture);
	void _free_render_target(FFRenderTarget *p_render_target);

	FixedFunctionTextureStorage(const FixedFunctionTextureStorage &);
	FixedFunctionTextureStorage &operator=(const FixedFunctionTextureStorage &);

public:
	RID texture_create(bool p_upload_on_use = false);
	bool owns_texture(RID p_texture) const { return _texture_owner.owns(p_texture); }
	// New resource bindings must not attach to records retained only for deferred release.
	bool texture_can_be_referenced(RID p_texture) const;
	// Declare a 2D image without allocating native backing until data arrives. Reallocation
	// releases old backing transactionally; flags are opaque metadata interpreted by the adapter.
	Error texture_allocate_2d(RID p_texture, const Size2i &p_size, Image::Format p_format, uint32_t p_flags, uint8_t p_sampler_flags);
	Error texture_set_flags(RID p_texture, uint32_t p_flags, uint8_t p_sampler_flags);
	uint32_t texture_get_flags(RID p_texture) const;
	Image::Format texture_get_format(RID p_texture) const;
	Size2i texture_get_image_size(RID p_texture) const;
	Error texture_set_size_override(RID p_texture, const Size2i &p_size);
	Error texture_set_path(RID p_texture, const String &p_path);
	String texture_get_path(RID p_texture) const;
	// Borrow callbacks without invoking them until the corresponding rendering use exists.
	Error texture_set_detect_callback(RID p_texture, FFTextureDetection p_detection, FFTextureDetectCallback p_callback, void *p_userdata);
	Error texture_get_detect_callback(RID p_texture, FFTextureDetection p_detection, FFTextureDetectCallback &r_callback, void *&r_userdata) const;
	// Append snapshots of live records; proxies do not duplicate their source's allocation.
	Error texture_debug_usage(List<FFTextureUsage> *r_info) const;
	Error texture_set_force_redraw_if_visible(RID p_texture, bool p_enable);
	// Match GLES: visibility scheduling belongs to the referenced wrapper, before proxy lookup.
	bool texture_get_force_redraw_if_visible(RID p_texture) const;
	const FixedFunctionGraphicsDevice::FFCapabilities &get_device_capabilities() const { return _device.get_capabilities(); }
	// Metadata sizing is for undeclared, unbacked textures; allocation resets a declaration.
	Error texture_set_size(RID p_texture, const Size2i &p_size);
	Size2i texture_get_size(RID p_texture) const;
	Error texture_set_sampler_flags(RID p_texture, uint8_t p_flags);
	// Resolve the same single proxy hop as image size and native texture lookup.
	uint8_t texture_get_sampler_flags(RID p_texture) const;
	// Read-only CPU sampling, following the same proxy hop as native sampling.
	// Identifies the current CPU image, including when a proxy changes its source.
	// Baking waits for edits to settle; CPU sampling still sees the latest image.
	uint64_t texture_get_cpu_image_version(RID p_texture, bool p_for_bake = false) const;
	bool texture_get_cpu_rgba8(RID p_texture, Size2i &r_size, PoolVector<uint8_t> &r_data);
	bool texture_get_panorama_average(RID p_texture, Color &r_color);
	bool texture_get_panorama_hemispheres(RID p_texture, Color &r_sky, Color &r_ground);
	// Snapshot the source image; upload its RGBA8 base level with nearest/clamp sampling.
	// Compressed images are unavailable. Failed replacements preserve the old data/backing.
	Error texture_set_data(RID p_texture, const Ref<Image> &p_image);
	// Patch an uncompressed, matching-format base level and replace its native backing.
	// Bounds are strict; the destination must have no mipmaps. Failure preserves old data.
	Error texture_set_data_partial(RID p_texture, const Ref<Image> &p_image, const Rect2i &p_source, const Point2i &p_destination);
	// Void server setters publish CPU snapshots immediately. Unused textures can wait
	// for their first draw; updates to resident textures are queued for idle upload.
	Error texture_request_allocate_2d(RID p_texture, const Size2i &p_size, Image::Format p_format, uint32_t p_flags, uint8_t p_sampler_flags);
	Error texture_request_data(RID p_texture, const Ref<Image> &p_image);
	Error texture_request_data_partial(RID p_texture, const Ref<Image> &p_image, const Rect2i &p_source, const Point2i &p_destination);
	// Acceptance transfers release responsibility here; callers must discard the RID immediately.
	Error request_free(RID p_rid);
	// Prepares before uploads. Target records are released only at a safe frame boundary;
	// ordinary releases may run at any idle point, including while the window is suspended.
	void advance_frame(uint64_t p_frame_number);
	Error update_pending(bool p_release_targets = false);
	int get_pending_upload_count() const { return _pending_uploads.size(); }
	int get_pending_release_count() const { return _pending_releases.size() + _retired_device_textures.size(); }
	Ref<Image> texture_get_data(RID p_texture) const;
	// Resolve one proxy hop. The output is borrowed for drawing, not for caching or freeing.
	// Failure clears the output; unbacked textures never mean vertex color.
	Error texture_get_device_texture(RID p_texture, RID &r_device_texture) const;
	// Preflight can recognize a drawable CPU snapshot without allocating it.
	bool texture_can_upload_on_use(RID p_texture) const;
	Error texture_get_or_upload_device_texture(RID p_texture, RID &r_device_texture);
	// An absent optional atlas means to omit only the distance-field glyph.
	RID texture_get_distance_field(RID p_texture);
	Error texture_set_proxy(RID p_texture, RID p_proxy);
	RID texture_get_proxy(RID p_texture) const;
	// Match GLES2's single level of proxy lookup, including viewport texture wrappers.
	Size2i texture_size_with_proxy(RID p_texture) const;
	RID texture_get_render_target(RID p_texture) const;

	// Targets own a distinct texture RID, preserved across position/size changes.
	// Size changes declare desired metadata; allocate commits matching native backing while idle.
	RID render_target_create();
	bool owns_render_target(RID p_render_target) const { return _render_target_owner.owns(p_render_target); }
	Error render_target_set_position(RID p_render_target, int p_x, int p_y);
	Error render_target_set_size(RID p_render_target, int p_width, int p_height);
	Error render_target_allocate(RID p_render_target);
	Rect2i render_target_get_rect(RID p_render_target) const;
	RID render_target_get_texture(RID p_render_target) const;
	// Requested hints do not change the opaque LDR backing in the current profile.
	// Explicit flip/transparency/direct/depth requests remain unavailable when enabled.
	Error render_target_set_flag(RID p_render_target, FFRenderTargetFlag p_flag, bool p_enabled);
	bool render_target_get_flag(RID p_render_target, FFRenderTargetFlag p_flag) const;
	Error render_target_set_msaa(RID p_render_target, uint32_t p_samples);
	Error render_target_set_fxaa(RID p_render_target, bool p_enabled);
	Error render_target_set_debanding(RID p_render_target, bool p_enabled);
	Error render_target_set_sharpen_intensity(RID p_render_target, float p_intensity);
	// Call device preparation before using this snapshot to schedule a viewport redraw.
	bool render_target_is_ready(RID p_render_target) const;
	// Failed viewport contents stay unavailable until a later successful pass.
	Error render_target_set_failed(RID p_render_target, bool p_failed);
	bool render_target_has_failed(RID p_render_target) const;
	// Retain recovery intent independently of volatile backing and per-frame usage.
	// A ready target needs no recovery redraw, even if the viewport was disabled explicitly.
	Error render_target_mark_for_recovery(RID p_render_target);
	bool render_target_needs_redraw(RID p_render_target) const;
	Error render_target_mark_used(RID p_render_target);
	bool render_target_was_used(RID p_render_target) const;
	Error render_target_clear_used(RID p_render_target);

	// A target's texture is borrowed: free the target, not its texture.
	Error free(RID p_rid);
	// On a release error, logical RIDs remain owned, but some native backing may be gone.
	// Retry clear before device shutdown. Explicit free preserves its record on release failure.
	Error clear();
	int get_texture_count() const { return _textures.size(); }
	int get_render_target_count() const { return _render_targets.size(); }

	FixedFunctionTextureStorage(FixedFunctionGraphicsDevice &p_device, FixedFunctionSceneStorage *p_scene_storage = nullptr);
	~FixedFunctionTextureStorage();
};

#endif // FIXED_FUNCTION_TEXTURE_STORAGE_H
