#include "texture_storage.h"

#include "scene_storage.h"

#include "core/math/math_funcs.h"

FixedFunctionTextureStorage::FFTextureUsage::FFTextureUsage() :
		cpu_bytes(0), native_bytes(0), format(Image::FORMAT_RGBA8) {}
FixedFunctionTextureStorage::FFTextureDetectionData::FFTextureDetectionData() :
		callback(nullptr), userdata(nullptr) {}

Error FixedFunctionTextureStorage::texture_set_detect_callback(RID p_texture, FFTextureDetection p_detection, FFTextureDetectCallback p_callback, void *p_userdata) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFTexture *texture = _get_texture(p_texture);
	if (!texture || uint32_t(p_detection) >= DETECT_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	texture->detection[p_detection].callback = p_callback;
	texture->detection[p_detection].userdata = p_callback ? p_userdata : nullptr;
	return OK;
}

Error FixedFunctionTextureStorage::texture_get_detect_callback(RID p_texture, FFTextureDetection p_detection, FFTextureDetectCallback &r_callback, void *&r_userdata) const {
	const FFTexture *texture = _get_texture(p_texture);
	if (!texture || uint32_t(p_detection) >= DETECT_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	r_callback = texture->detection[p_detection].callback;
	r_userdata = texture->detection[p_detection].userdata;
	return OK;
}

Error FixedFunctionTextureStorage::texture_debug_usage(List<FFTextureUsage> *r_info) const {
	if (!r_info) {
		return ERR_INVALID_PARAMETER;
	}
	List<FFTextureUsage> usage;
	for (const List<FFTexture *>::Element *E = _textures.front(); E; E = E->next()) {
		const FFTexture *texture = E->get();
		if (texture->free_pending) {
			continue;
		}
		FFTextureUsage info;
		info.texture = texture->self;
		info.path = texture->path;
		info.size = texture->image_size;
		info.format = texture->format;
		info.cpu_bytes = texture->image.is_valid() ? texture->image->get_data().size() : 0;
		if (texture->pending_decoded_image.is_valid()) {
			info.cpu_bytes += texture->pending_decoded_image->get_data().size();
		}
		info.cpu_bytes += texture->cpu_sample_rgba.size();
		if (texture->device_texture.is_valid()) {
			FixedFunctionGraphicsDevice::FFTextureInfo native;
			const Error error = _device.texture_get_info(texture->device_texture, native);
			if (error != OK && error != ERR_UNAVAILABLE) {
				return error;
			}
			if (error == OK) {
				info.size = native.size;
				info.native_bytes = native.bytes;
			}
		}
		const Map<const FFTexture *, RID>::Element *coverage = _distance_field_textures.find(texture);
		if (coverage && coverage->get().is_valid()) {
			FixedFunctionGraphicsDevice::FFTextureInfo native;
			if (_device.texture_get_info(coverage->get(), native) == OK) {
				info.native_bytes += native.bytes;
			}
		}
		usage.push_back(info);
	}
	for (const List<FFTextureUsage>::Element *E = usage.front(); E; E = E->next()) {
		r_info->push_back(E->get());
	}
	return OK;
}

FixedFunctionTextureStorage::FFUpdateScope::FFUpdateScope(bool &p_updating) :
		_updating(p_updating) {
	_updating = true;
}

FixedFunctionTextureStorage::FFUpdateScope::~FFUpdateScope() {
	_updating = false;
}

FixedFunctionTextureStorage::FFTexture::FFTexture() :
		proxy(nullptr),
		render_target(nullptr),
		flags(0),
		format(Image::FORMAT_L8),
		panorama_average_rgba(0),
		panorama_sky_rgba(0),
		panorama_ground_rgba(0),
		sampler_flags(0),
		allocated(false),
		size_overridden(false),
		upload_on_use(false),
		upload_pending(false),
		upload_failed(false),
		free_pending(false),
		redraw_if_visible(false) {
}

FixedFunctionTextureStorage::FFRenderTarget::FFRenderTarget() :
		flags(0),
		used(false),
		redraw_on_loss(false) {
}

FixedFunctionTextureStorage::FFTexture *FixedFunctionTextureStorage::_get_texture(RID p_texture) const {
	return _texture_owner.owns(p_texture) ? _texture_owner.getptr(p_texture) : nullptr;
}

FixedFunctionTextureStorage::FFRenderTarget *FixedFunctionTextureStorage::_get_render_target(RID p_render_target) const {
	return _render_target_owner.owns(p_render_target) ? _render_target_owner.getptr(p_render_target) : nullptr;
}

Error FixedFunctionTextureStorage::_validate_render_target_change(RID p_render_target) const {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	const FFRenderTarget *target = _get_render_target(p_render_target);
	if (!target) {
		return ERR_INVALID_PARAMETER;
	}
	return _get_texture(target->texture)->free_pending ? ERR_ALREADY_IN_USE : OK;
}

RID FixedFunctionTextureStorage::texture_create(bool p_upload_on_use) {
	if (_clearing) {
		return RID();
	}
	FFTexture *texture = memnew(FFTexture);
	texture->upload_on_use = p_upload_on_use;
	texture->self = _texture_owner.make_rid(texture);
	_textures.push_back(texture);
	return texture->self;
}

bool FixedFunctionTextureStorage::texture_can_be_referenced(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	return !_clearing && texture && !texture->free_pending;
}

Error FixedFunctionTextureStorage::_validate_allocation(const FFTexture *p_texture, const Size2i &p_size, Image::Format p_format, uint8_t p_sampler_flags) const {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	if (!p_texture || p_size.x <= 0 || p_size.y <= 0 || p_size.x > Image::MAX_WIDTH || p_size.y > Image::MAX_HEIGHT ||
			p_format < Image::FORMAT_L8 || p_format >= Image::FORMAT_MAX || (p_sampler_flags & ~FixedFunctionGraphicsDevice::SAMPLER_FLAGS_MASK)) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_texture->render_target || p_texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	return OK;
}

void FixedFunctionTextureStorage::_commit_allocation(FFTexture *p_texture, const Size2i &p_size, Image::Format p_format, uint32_t p_flags, uint8_t p_sampler_flags) {
	_invalidate_distance_field(p_texture);
	p_texture->upload_failed = false;
	p_texture->image.unref();
	p_texture->pending_decoded_image.unref();
	_discard_cpu_sample(p_texture);
	p_texture->panorama_average_rgba = 0;
	p_texture->size = p_texture->image_size = p_size;
	p_texture->format = p_format;
	p_texture->flags = p_flags;
	p_texture->sampler_flags = p_sampler_flags;
	p_texture->allocated = true;
	p_texture->size_overridden = false;
}

Error FixedFunctionTextureStorage::texture_allocate_2d(RID p_texture, const Size2i &p_size, Image::Format p_format, uint32_t p_flags, uint8_t p_sampler_flags) {
	FFTexture *texture = _get_texture(p_texture);
	Error error = _validate_allocation(texture, p_size, p_format, p_sampler_flags);
	if (error != OK) {
		return error;
	}
	if (_updating || texture->upload_pending) {
		return ERR_BUSY;
	}
	FFUpdateScope update_scope(_updating);
	error = _release_device_texture(texture);
	if (error != OK) {
		return error;
	}
	if (texture->upload_pending || texture->free_pending) {
		return ERR_SKIP;
	}
	_commit_allocation(texture, p_size, p_format, p_flags, p_sampler_flags);
	return OK;
}

Error FixedFunctionTextureStorage::texture_set_flags(RID p_texture, uint32_t p_flags, uint8_t p_sampler_flags) {
	FFTexture *texture = _get_texture(p_texture);
	if (!texture || (p_sampler_flags & ~FixedFunctionGraphicsDevice::SAMPLER_FLAGS_MASK)) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	texture->flags = p_flags;
	texture->sampler_flags = p_sampler_flags;
	return OK;
}

uint32_t FixedFunctionTextureStorage::texture_get_flags(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, 0);
	return texture->flags;
}

Image::Format FixedFunctionTextureStorage::texture_get_format(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, Image::FORMAT_L8);
	return texture->format;
}

Size2i FixedFunctionTextureStorage::texture_get_image_size(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, Size2i());
	return texture->image_size;
}

Error FixedFunctionTextureStorage::texture_set_size_override(RID p_texture, const Size2i &p_size) {
	FFTexture *texture = _get_texture(p_texture);
	if (!texture || p_size.x <= 0 || p_size.y <= 0 || p_size.x > Image::MAX_WIDTH || p_size.y > Image::MAX_HEIGHT) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->render_target || texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	texture->size = p_size;
	texture->size_overridden = true;
	return OK;
}

Error FixedFunctionTextureStorage::texture_set_path(RID p_texture, const String &p_path) {
	FFTexture *texture = _get_texture(p_texture);
	if (!texture) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	texture->path = p_path;
	return OK;
}

String FixedFunctionTextureStorage::texture_get_path(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, String());
	return texture->path;
}

Error FixedFunctionTextureStorage::texture_set_size(RID p_texture, const Size2i &p_size) {
	FFTexture *texture = _get_texture(p_texture);
	if (!texture || p_size.x < 0 || p_size.y < 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->render_target || texture->image.is_valid() || texture->allocated || texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	texture->size = texture->image_size = p_size;
	return OK;
}

Size2i FixedFunctionTextureStorage::texture_get_size(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, Size2i());
	return texture->size;
}

Error FixedFunctionTextureStorage::texture_set_sampler_flags(RID p_texture, uint8_t p_flags) {
	FFTexture *texture = _get_texture(p_texture);
	if (!texture || (p_flags & ~FixedFunctionGraphicsDevice::SAMPLER_FLAGS_MASK)) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	texture->sampler_flags = p_flags;
	return OK;
}

uint8_t FixedFunctionTextureStorage::texture_get_sampler_flags(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, 0);
	return texture->proxy ? texture->proxy->sampler_flags : texture->sampler_flags;
}

uint64_t FixedFunctionTextureStorage::texture_get_cpu_image_version(RID p_texture, bool p_for_bake) const {
	const FFTexture *texture = _get_texture(p_texture);
	if (!texture || texture->free_pending) {
		return 0;
	}
	if (texture->proxy) {
		texture = texture->proxy;
	}
	if (texture->free_pending || texture->image.is_null() || (p_for_bake && _frame_number < texture->cpu_image_bake_after)) {
		return 0;
	}
	return texture->cpu_image_version;
}

bool FixedFunctionTextureStorage::texture_get_cpu_rgba8(RID p_texture, Size2i &r_size, PoolVector<uint8_t> &r_data) {
	FFTexture *texture = _get_texture(p_texture);
	if (!texture || texture->free_pending) {
		return false;
	}
	if (texture->proxy) {
		texture = texture->proxy;
	}
	if (texture->free_pending || texture->image.is_null()) {
		return false;
	}
	const Ref<Image> &source = texture->image;
	if (source->get_format() == Image::FORMAT_RGBA8) {
		r_data = source->get_data();
	} else {
		if (!texture->cpu_sample_rgba.size()) {
			Ref<Image> base = source;
			if (source->has_mipmaps()) {
				base = source->duplicate();
				base->clear_mipmaps();
			}
			Ref<Image> decoded;
			if (FixedFunctionGraphicsDevice::prepare_rgba8_upload(base, decoded) != OK) {
				return false;
			}
			texture->cpu_sample_rgba = decoded->get_data();
			_cpu_sample_textures.insert(texture);
		}
		texture->cpu_sample_frame = _frame_number;
		r_data = texture->cpu_sample_rgba;
	}
	r_size = texture->image_size;
	return true;
}

Error FixedFunctionTextureStorage::texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFTexture *texture = _get_texture(p_texture);
	if (!texture) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	texture->redraw_if_visible = p_enable;
	return OK;
}

bool FixedFunctionTextureStorage::texture_get_force_redraw_if_visible(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, false);
	return !texture->free_pending && texture->redraw_if_visible;
}

Error FixedFunctionTextureStorage::_validate_data(const FFTexture *p_texture, const Ref<Image> &p_image) const {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	if (!p_texture || p_image.is_null() || p_image->empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_texture->render_target || p_texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	if (p_texture->allocated && (p_texture->format != p_image->get_format() || p_texture->image_size.x != p_image->get_width() || p_texture->image_size.y != p_image->get_height())) {
		return ERR_INVALID_PARAMETER;
	}
	return OK;
}

// Mirrors Image::decompress() without allocating the decoded image.
static bool _can_decompress(Image::Format p_format) {
	if (p_format >= Image::FORMAT_DXT1 && p_format <= Image::FORMAT_RGTC_RG) {
		return Image::_image_decompress_bc != nullptr;
	}
	if (p_format >= Image::FORMAT_BPTC_RGBA && p_format <= Image::FORMAT_BPTC_RGBFU) {
		return Image::_image_decompress_bptc != nullptr;
	}
	if (p_format >= Image::FORMAT_PVRTC2 && p_format <= Image::FORMAT_PVRTC4A) {
		return Image::_image_decompress_pvrtc != nullptr;
	}
	if (p_format == Image::FORMAT_ETC) {
		return Image::_image_decompress_etc1 != nullptr;
	}
	if (p_format >= Image::FORMAT_ETC2_R11 && p_format <= Image::FORMAT_ETC2_RGB8A1) {
		return Image::_image_decompress_etc2 != nullptr;
	}
	return false;
}

Error FixedFunctionTextureStorage::_prepare_data(const Ref<Image> &p_image, Ref<Image> &r_snapshot, Ref<Image> *r_decoded) const {
	// Check whether a legal upload can be derived, without restricting canonical CPU dimensions.
	if (_device.get_capabilities().max_texture_stages) {
		Size2i upload_size;
		const Error size_error = _device.get_texture_upload_size(p_image->get_width(), p_image->get_height(), upload_size);
		if (size_error != OK) {
			return size_error;
		}
	}
	// Image::duplicate shares bytes with any outstanding Image/PoolVector write lock.
	// Copy explicitly so a caller's already-open writer cannot change accepted data.
	const PoolVector<uint8_t> &source = p_image->get_data();
	PoolVector<uint8_t> data;
	const Error error = data.resize(source.size());
	if (error != OK) {
		return error;
	}
	{
		PoolVector<uint8_t>::Read read = source.read();
		PoolVector<uint8_t>::Write write = data.write();
		memcpy(write.ptr(), read.ptr(), source.size());
	}
	r_snapshot.instance();
	r_snapshot->create(p_image->get_width(), p_image->get_height(), p_image->has_mipmaps(), p_image->get_format(), data);
	if (r_snapshot->is_compressed()) {
		if (_device.accepts_source_texture_images() || _device.can_upload_compressed_texture(r_snapshot)) {
			// Require the fallback decoder before accepting the original compressed image.
			return _can_decompress(r_snapshot->get_format()) ? OK : ERR_UNAVAILABLE;
		}
		if (r_decoded) {
			return FixedFunctionGraphicsDevice::prepare_rgba8_upload(r_snapshot, *r_decoded);
		}
		// Deferred requests decode one image at a time during upload. Decoding here would keep
		// every RGBA8 copy alive until the next idle point, which exhausts a 32-bit address
		// space while a scene loads many textures between frames.
		return _can_decompress(r_snapshot->get_format()) ? OK : ERR_UNAVAILABLE;
	}
	return OK;
}

void FixedFunctionTextureStorage::_commit_data(FFTexture *p_texture, const Ref<Image> &p_snapshot) {
	_invalidate_distance_field(p_texture);
	p_texture->upload_failed = false;
	p_texture->image = p_snapshot;
	// Allow a complete frame without edits before requesting a new AO bake.
	if (p_texture->cpu_image_version) {
		p_texture->cpu_image_bake_after = _frame_number + 2;
	}
	p_texture->cpu_image_version = ++_cpu_image_version;
	p_texture->pending_decoded_image.unref();
	_discard_cpu_sample(p_texture);
	p_texture->panorama_average_rgba = 0;
	p_texture->format = p_snapshot->get_format();
	p_texture->image_size.x = p_snapshot->get_width();
	p_texture->image_size.y = p_snapshot->get_height();
	if (!p_texture->size_overridden) {
		p_texture->size = p_texture->image_size;
	}
}

bool FixedFunctionTextureStorage::_is_upload_resource_error(Error p_error) {
	return p_error == ERR_INVALID_DATA || p_error == ERR_UNAVAILABLE || p_error == ERR_CANT_CREATE ||
			p_error == ERR_OUT_OF_MEMORY || p_error == ERR_INVALID_PARAMETER || p_error == FAILED;
}

Error FixedFunctionTextureStorage::_create_device_texture(FFTexture *p_texture, const Ref<Image> &p_image, RID &r_native, bool p_allow_during_frame) {
	if (_device.accepts_source_texture_images()) {
		return _device.texture_create(p_image, r_native, p_allow_during_frame);
	}
	Error error = ERR_UNAVAILABLE;
	if (p_image->is_compressed() && _device.can_upload_compressed_texture(p_image)) {
		error = _device.texture_create(p_image, r_native, p_allow_during_frame);
		if (error != OK && error != ERR_UNAVAILABLE && error != FAILED && error != ERR_OUT_OF_MEMORY) {
			return error;
		}
		if (error != OK && (p_texture->free_pending || ((p_allow_during_frame || p_texture->upload_pending) && p_texture->image != p_image))) {
			return ERR_SKIP;
		}
	}
	if (error != OK) {
		Ref<Image> upload;
		error = FixedFunctionGraphicsDevice::prepare_rgba8_upload(p_image, upload, true);
		if (error != OK) {
			return error;
		}
		if (p_image->is_compressed() && p_texture->upload_pending && !p_texture->free_pending && p_texture->image == p_image) {
			p_texture->pending_decoded_image = upload;
		}
		error = _device.texture_create(upload, r_native, p_allow_during_frame);
	}
	return error;
}

Error FixedFunctionTextureStorage::_upload_data(FFTexture *p_texture, const Ref<Image> &p_image, bool *r_creation_failed) {
	if (r_creation_failed) {
		*r_creation_failed = true;
	}
	RID replacement;
	Error error = _create_device_texture(p_texture, p_image, replacement, false);
	if (error != OK) {
		return error;
	}
	if (r_creation_failed) {
		*r_creation_failed = false;
	}
	error = _release_device_texture(p_texture);
	if (error != OK) {
		// Retain ownership even if cleanup fails, so clear can retry this native release.
		if (_device.texture_free(replacement) != OK) {
			_retired_device_textures.push_back(replacement);
		}
		return error;
	}
	p_texture->device_texture = replacement;
	return OK;
}

Error FixedFunctionTextureStorage::texture_set_data(RID p_texture, const Ref<Image> &p_image) {
	FFTexture *texture = _get_texture(p_texture);
	const Error error = _validate_data(texture, p_image);
	if (error != OK) {
		return error;
	}
	if (_updating || texture->upload_pending) {
		return ERR_BUSY;
	}
	Ref<Image> snapshot, decoded;
	const Error prepare_error = _prepare_data(p_image, snapshot, &decoded);
	if (prepare_error != OK) {
		return prepare_error;
	}
	FFUpdateScope update_scope(_updating);
	const Error upload_error = _upload_data(texture, decoded.is_valid() ? decoded : snapshot);
	if (upload_error == OK) {
		if (texture->upload_pending || texture->free_pending) {
			return ERR_SKIP;
		}
		_commit_data(texture, snapshot);
	}
	return upload_error;
}

Error FixedFunctionTextureStorage::_patch_data(const FFTexture *p_texture, const Ref<Image> &p_image, const Rect2i &p_source, const Point2i &p_destination, Ref<Image> &r_image) const {
	r_image.unref();
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	if (!p_texture || p_image.is_null() || p_image->empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_texture->render_target || p_texture->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	if (p_texture->image.is_null()) {
		return ERR_UNCONFIGURED;
	}
	if (p_image->is_compressed() || p_texture->image->is_compressed() || p_texture->image->has_mipmaps()) {
		return ERR_UNAVAILABLE;
	}
	if (p_image->get_format() != p_texture->image->get_format() || p_source.position.x < 0 || p_source.position.y < 0 ||
			p_source.size.x < 0 || p_source.size.y < 0 || p_destination.x < 0 || p_destination.y < 0) {
		return ERR_INVALID_PARAMETER;
	}
	// Widen before adding: invalid rectangle endpoints must not overflow or be clipped.
	if (int64_t(p_source.position.x) + p_source.size.x > p_image->get_width() ||
			int64_t(p_source.position.y) + p_source.size.y > p_image->get_height() ||
			int64_t(p_destination.x) + p_source.size.x > p_texture->image->get_width() ||
			int64_t(p_destination.y) + p_source.size.y > p_texture->image->get_height()) {
		return ERR_INVALID_PARAMETER;
	}
	if (!p_source.size.x || !p_source.size.y) {
		return OK;
	}
	r_image = p_texture->image->duplicate();
	// 3.x's public Image API uses real_t rectangles; its 16384-pixel size limit fits exactly.
	r_image->blit_rect(p_image, Rect2(p_source.position.x, p_source.position.y, p_source.size.x, p_source.size.y), Point2(p_destination.x, p_destination.y));
	return OK;
}

Error FixedFunctionTextureStorage::texture_set_data_partial(RID p_texture, const Ref<Image> &p_image, const Rect2i &p_source, const Point2i &p_destination) {
	FFTexture *texture = _get_texture(p_texture);
	if (_updating || (texture && texture->upload_pending)) {
		return ERR_BUSY;
	}
	Ref<Image> replacement;
	const Error error = _patch_data(texture, p_image, p_source, p_destination, replacement);
	return error != OK || replacement.is_null() ? error : texture_set_data(p_texture, replacement);
}

void FixedFunctionTextureStorage::_queue_upload(FFTexture *p_texture) {
	if (p_texture->upload_on_use && !p_texture->device_texture.is_valid()) {
		return;
	}
	if (!p_texture->upload_pending) {
		_pending_uploads.insert(p_texture);
		p_texture->upload_pending = true;
	}
}

void FixedFunctionTextureStorage::_cancel_upload(FFTexture *p_texture) {
	if (p_texture->upload_pending) {
		_pending_uploads.erase(p_texture);
		p_texture->upload_pending = false;
		p_texture->pending_decoded_image.unref();
	}
}

Error FixedFunctionTextureStorage::texture_request_allocate_2d(RID p_texture, const Size2i &p_size, Image::Format p_format, uint32_t p_flags, uint8_t p_sampler_flags) {
	FFTexture *texture = _get_texture(p_texture);
	const Error error = _validate_allocation(texture, p_size, p_format, p_sampler_flags);
	if (error != OK) {
		return error;
	}
	_commit_allocation(texture, p_size, p_format, p_flags, p_sampler_flags);
	_queue_upload(texture);
	return OK;
}

Error FixedFunctionTextureStorage::texture_request_data(RID p_texture, const Ref<Image> &p_image) {
	FFTexture *texture = _get_texture(p_texture);
	const Error error = _validate_data(texture, p_image);
	if (error != OK) {
		return error;
	}
	Ref<Image> snapshot;
	const Error prepare_error = _prepare_data(p_image, snapshot, nullptr);
	if (prepare_error != OK) {
		return prepare_error;
	}
	_commit_data(texture, snapshot);
	_queue_upload(texture);
	return OK;
}

Error FixedFunctionTextureStorage::texture_request_data_partial(RID p_texture, const Ref<Image> &p_image, const Rect2i &p_source, const Point2i &p_destination) {
	FFTexture *texture = _get_texture(p_texture);
	Ref<Image> replacement;
	const Error error = _patch_data(texture, p_image, p_source, p_destination, replacement);
	if (error == OK && replacement.is_valid()) {
		_commit_data(texture, replacement);
		_queue_upload(texture);
	}
	return error;
}

Error FixedFunctionTextureStorage::request_free(RID p_rid) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFRenderTarget *target = _get_render_target(p_rid);
	FFTexture *texture = _get_texture(target ? target->texture : p_rid);
	if (!texture) {
		return ERR_INVALID_PARAMETER;
	}
	if (!target && texture->render_target) {
		return ERR_ALREADY_IN_USE;
	}
	_cancel_upload(texture);
	_pending_releases.insert(texture);
	texture->free_pending = true;
	for (int i = 0; i < DETECT_MAX; i++) {
		texture->detection[i] = FFTextureDetectionData();
	}
	// A native call may still hold its own snapshot; queued CPU image data can go now.
	texture->image.unref();
	_discard_cpu_sample(texture);
	return OK;
}

Error FixedFunctionTextureStorage::_release_retired_textures() {
	while (_retired_device_textures.front()) {
		const Error error = _device.texture_free(_retired_device_textures.front()->get());
		if (error != OK) {
			return error;
		}
		_retired_device_textures.pop_front();
	}
	return OK;
}

Error FixedFunctionTextureStorage::_update_pending(bool p_release_targets) {
	Error error = _release_retired_textures();
	if (error != OK) {
		return error;
	}
	// Retain target records through the logical frame: context/manager may still hold them.
	int release_budget = _pending_releases.size();
	for (Set<FFTexture *>::Element *E = _pending_releases.front(); E && release_budget--;) {
		FFTexture *texture = E->get();
		E = E->next();
		if (texture->render_target && !p_release_targets) {
			continue;
		}
		error = _release_device_texture(texture);
		if (error != OK) {
			return error;
		}
		if (texture->render_target) {
			_free_render_target(texture->render_target);
		} else {
			_free_texture(texture);
		}
	}
	bool prepared = false;
	// Bound work admitted reentrantly by native window callbacks to this update's budget.
	int upload_budget = _pending_uploads.size();
	while (_pending_uploads.front() && upload_budget--) {
		FFTexture *texture = _pending_uploads.front()->get();
		const Ref<Image> snapshot = texture->image;
		const Ref<Image> upload = texture->pending_decoded_image.is_valid() ? texture->pending_decoded_image : snapshot;
		if (snapshot.is_valid() && !prepared) {
			error = _device.prepare_resources();
			if (error != OK) {
				return error;
			}
			prepared = true;
		}
		if (texture->free_pending) {
			continue;
		}
		if (texture->image != snapshot) {
			return ERR_SKIP;
		}
		bool creation_failed = false;
		error = upload.is_valid() ? _upload_data(texture, upload, &creation_failed) : _release_device_texture(texture);
		if (error != OK && (!creation_failed || !_is_upload_resource_error(error))) {
			return error;
		}
		if (texture->free_pending) {
			continue;
		}
		// A newer CPU request must not be cleared by completion of this older upload.
		if (texture->image != snapshot) {
			return ERR_SKIP;
		}
		// Keep the accepted CPU image, but prevent sampling the previous native revision.
		texture->upload_failed = creation_failed;
		_cancel_upload(texture);
		if (creation_failed) {
			ERR_PRINT(vformat("Fixed Function could not upload texture '%s' (Error %d).", texture->path, error));
			error = _release_device_texture(texture);
			if (error != OK) {
				return error;
			}
		}
	}
	return _pending_uploads.empty() ? OK : ERR_SKIP;
}

void FixedFunctionTextureStorage::_discard_cpu_sample(FFTexture *p_texture) {
	if (p_texture->cpu_sample_rgba.size()) {
		_cpu_sample_textures.erase(p_texture);
		p_texture->cpu_sample_rgba = PoolVector<uint8_t>();
	}
}

void FixedFunctionTextureStorage::advance_frame(uint64_t p_frame_number) {
	_frame_number = p_frame_number;
	for (Set<FFTexture *>::Element *E = _cpu_sample_textures.front(); E;) {
		FFTexture *texture = E->get();
		E = E->next();
		// Streamed AO can reuse the last frame's sample. Idle copies can be decoded again.
		if (_frame_number - texture->cpu_sample_frame > 1) {
			_discard_cpu_sample(texture);
		}
	}
}

Error FixedFunctionTextureStorage::update_pending(bool p_release_targets) {
	if (_updating) {
		return ERR_BUSY;
	}
	FFUpdateScope update_scope(_updating);
	return _update_pending(p_release_targets);
}

Ref<Image> FixedFunctionTextureStorage::texture_get_data(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, Ref<Image>());
	if (texture->render_target) {
		return _read_render_target(*texture);
	}
	return texture->image.is_valid() ? Ref<Image>(texture->image->duplicate()) : Ref<Image>();
}

Error FixedFunctionTextureStorage::texture_get_device_texture(RID p_texture, RID &r_device_texture) const {
	r_device_texture = RID();
	const FFTexture *texture = _get_texture(p_texture);
	if (!texture) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->free_pending) {
		return ERR_SKIP;
	}
	if (texture->proxy) {
		texture = texture->proxy;
	}
	if (texture->upload_pending || texture->free_pending) {
		return ERR_SKIP;
	}
	if (texture->render_target) {
		if (!_device.get_capabilities().render_target_opaque) {
			return ERR_UNAVAILABLE;
		}
		if (texture->render_target->allocated_size != texture->size || !texture->size.x || !texture->size.y) {
			return ERR_UNCONFIGURED;
		}
	}
	if (texture->upload_failed || !texture->device_texture.is_valid()) {
		return ERR_UNCONFIGURED;
	}
	r_device_texture = texture->device_texture;
	return OK;
}

FixedFunctionTextureStorage::FFTexture *FixedFunctionTextureStorage::_get_first_upload(RID p_texture) const {
	FFTexture *texture = _get_texture(p_texture);
	if (!texture || texture->free_pending || _clearing) {
		return nullptr;
	}
	if (texture->proxy) {
		texture = texture->proxy;
	}
	if (!texture->upload_on_use || texture->render_target || texture->free_pending || texture->upload_pending ||
			texture->upload_failed || texture->device_texture.is_valid() || texture->image.is_null()) {
		return nullptr;
	}
	return texture;
}

bool FixedFunctionTextureStorage::texture_can_upload_on_use(RID p_texture) const {
	return _get_first_upload(p_texture) != nullptr;
}

Error FixedFunctionTextureStorage::texture_get_or_upload_device_texture(RID p_texture, RID &r_device_texture) {
	const Error lookup = texture_get_device_texture(p_texture, r_device_texture);
	if (lookup != ERR_UNCONFIGURED) {
		return lookup;
	}
	FFTexture *texture = _get_first_upload(p_texture);
	if (!texture) {
		return lookup;
	}
	if (_updating) {
		return ERR_SKIP;
	}
	FFUpdateScope update_scope(_updating);
	const Ref<Image> snapshot = texture->image;
	RID native;
	const Error error = _create_device_texture(texture, snapshot, native, true);
	if (texture->free_pending || texture->image != snapshot) {
		// A native callback replaced or freed this image. Retire the obsolete upload at idle.
		if (native.is_valid()) {
			_retired_device_textures.push_back(native);
		}
		return ERR_SKIP;
	}
	if (error != OK) {
		if (!_is_upload_resource_error(error)) {
			return error == ERR_BUSY ? ERR_SKIP : error;
		}
		texture->upload_failed = true;
		ERR_PRINT(vformat("Fixed Function could not upload texture '%s' (Error %d).", texture->path, error));
		return ERR_UNCONFIGURED;
	}
	texture->device_texture = native;
	// Re-resolve the original RID in case a proxy changed during the native call.
	const Error resolved = texture_get_device_texture(p_texture, r_device_texture);
	return resolved == ERR_UNCONFIGURED && texture_can_upload_on_use(p_texture) ? ERR_SKIP : resolved;
}

Error FixedFunctionTextureStorage::texture_set_proxy(RID p_texture, RID p_proxy) {
	FFTexture *texture = _get_texture(p_texture);
	FFTexture *proxy = p_proxy.is_valid() ? _get_texture(p_proxy) : nullptr;
	if (!texture || (p_proxy.is_valid() && !proxy)) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->render_target || texture->free_pending || (proxy && proxy->free_pending)) {
		return ERR_ALREADY_IN_USE;
	}
	// Validate before detaching the old proxy. Rejected changes leave it intact.
	for (FFTexture *current = proxy; current; current = current->proxy) {
		if (current == texture) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (texture->proxy) {
		texture->proxy->proxy_owners.erase(texture);
	}
	texture->proxy = proxy;
	if (proxy) {
		proxy->proxy_owners.insert(texture);
	}
	return OK;
}

RID FixedFunctionTextureStorage::texture_get_proxy(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, RID());
	return texture->proxy ? texture->proxy->self : RID();
}

Size2i FixedFunctionTextureStorage::texture_size_with_proxy(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, Size2i());
	return texture->proxy ? texture->proxy->size : texture->size;
}

RID FixedFunctionTextureStorage::texture_get_render_target(RID p_texture) const {
	const FFTexture *texture = _get_texture(p_texture);
	ERR_FAIL_COND_V(!texture, RID());
	if (texture->proxy) {
		texture = texture->proxy;
	}
	return texture->render_target ? texture->render_target->self : RID();
}

RID FixedFunctionTextureStorage::render_target_create() {
	if (_clearing) {
		return RID();
	}
	FFRenderTarget *target = memnew(FFRenderTarget);
	target->self = _render_target_owner.make_rid(target);
	target->texture = texture_create();
	_get_texture(target->texture)->render_target = target;
	_get_texture(target->texture)->format = Image::FORMAT_RGB8;
	_render_targets.push_back(target);
	return target->self;
}

Error FixedFunctionTextureStorage::render_target_set_position(RID p_render_target, int p_x, int p_y) {
	FFRenderTarget *target = _get_render_target(p_render_target);
	if (!target) {
		return ERR_INVALID_PARAMETER;
	}
	if (_get_texture(target->texture)->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	// Assign integer fields directly; 3.x's Rect2i constructor passes through real_t.
	target->rect.position.x = p_x;
	target->rect.position.y = p_y;
	return OK;
}

Error FixedFunctionTextureStorage::render_target_set_size(RID p_render_target, int p_width, int p_height) {
	FFRenderTarget *target = _get_render_target(p_render_target);
	if (!target || p_width < 0 || p_height < 0) {
		return ERR_INVALID_PARAMETER;
	}
	if (_get_texture(target->texture)->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	target->rect.size.x = p_width;
	target->rect.size.y = p_height;
	FFTexture *texture = _get_texture(target->texture);
	texture->size = texture->image_size = target->rect.size;
	return OK;
}

Error FixedFunctionTextureStorage::render_target_set_flag(RID p_render_target, FFRenderTargetFlag p_flag, bool p_enabled) {
	const Error error = _validate_render_target_change(p_render_target);
	if (error != OK) {
		return error;
	}
	if (uint32_t(p_flag) >= RENDER_TARGET_FLAG_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_enabled && ((p_flag == RENDER_TARGET_TRANSPARENT && !_device.get_capabilities().render_target_transparent) || p_flag == RENDER_TARGET_DIRECT_TO_SCREEN || p_flag == RENDER_TARGET_USE_32_BPC_DEPTH)) {
		return ERR_UNAVAILABLE;
	}
	// Like GLES2, HDR requests fall back to LDR. VFLIP is applied when consuming the
	// texture; other accepted flags are 3D/effect/allocation hints for this 2D profile.
	FFRenderTarget *target = _get_render_target(p_render_target);
	const uint16_t mask = uint16_t(1u << p_flag);
	if (p_flag == RENDER_TARGET_TRANSPARENT && bool(target->flags & mask) != p_enabled) {
		// Like a size change, invalidate old backing until allocation commits the new format.
		target->allocated_size = Size2i();
		_get_texture(target->texture)->format = p_enabled ? Image::FORMAT_RGBA8 : Image::FORMAT_RGB8;
	}
	if (p_enabled) {
		target->flags |= mask;
	} else {
		target->flags &= ~mask;
	}
	return OK;
}

bool FixedFunctionTextureStorage::render_target_get_flag(RID p_render_target, FFRenderTargetFlag p_flag) const {
	const FFRenderTarget *target = _get_render_target(p_render_target);
	ERR_FAIL_COND_V(!target || uint32_t(p_flag) >= RENDER_TARGET_FLAG_MAX, false);
	return (target->flags & (1u << p_flag)) != 0;
}

Error FixedFunctionTextureStorage::render_target_set_msaa(RID p_render_target, uint32_t p_samples) {
	const Error error = _validate_render_target_change(p_render_target);
	if (error != OK) {
		return error;
	}
	return p_samples ? ERR_UNAVAILABLE : OK;
}

Error FixedFunctionTextureStorage::render_target_set_fxaa(RID p_render_target, bool p_enabled) {
	const Error error = _validate_render_target_change(p_render_target);
	if (error != OK) {
		return error;
	}
	return p_enabled ? ERR_UNAVAILABLE : OK;
}

Error FixedFunctionTextureStorage::render_target_set_debanding(RID p_render_target, bool p_enabled) {
	const Error error = _validate_render_target_change(p_render_target);
	if (error != OK) {
		return error;
	}
	return p_enabled ? ERR_UNAVAILABLE : OK;
}

Error FixedFunctionTextureStorage::render_target_set_sharpen_intensity(RID p_render_target, float p_intensity) {
	const Error error = _validate_render_target_change(p_render_target);
	if (error != OK) {
		return error;
	}
	if (Math::is_nan(p_intensity) || Math::is_inf(p_intensity) || p_intensity < 0) {
		return ERR_INVALID_PARAMETER;
	}
	return p_intensity > 0 ? ERR_UNAVAILABLE : OK;
}

Error FixedFunctionTextureStorage::render_target_allocate(RID p_render_target) {
	FFRenderTarget *target = _get_render_target(p_render_target);
	if (!target) {
		return ERR_INVALID_PARAMETER;
	}
	FFTexture *texture = _get_texture(target->texture);
	if (texture->free_pending) {
		return ERR_SKIP;
	}
	if (_updating) {
		return ERR_BUSY;
	}
	FFUpdateScope update_scope(_updating);
	if (!target->rect.size.x || !target->rect.size.y) {
		const Error error = _release_device_texture(texture);
		if (error == OK) {
			target->allocated_size = Size2i();
		}
		return error;
	}
	if (texture->device_texture.is_valid() && target->allocated_size == target->rect.size) {
		return OK;
	}
	RID replacement;
	const Size2i requested_size = target->rect.size;
	Error error = _device.texture_create_render_target(requested_size.x, requested_size.y, replacement, (target->flags & (1 << RENDER_TARGET_TRANSPARENT)) != 0);
	if (error != OK) {
		return error;
	}
	error = _release_device_texture(texture);
	if (error != OK) {
		if (_device.texture_free(replacement) != OK) {
			_retired_device_textures.push_back(replacement);
		}
		return error;
	}
	texture->device_texture = replacement;
	target->allocated_size = requested_size;
	return OK;
}

Rect2i FixedFunctionTextureStorage::render_target_get_rect(RID p_render_target) const {
	const FFRenderTarget *target = _get_render_target(p_render_target);
	ERR_FAIL_COND_V(!target, Rect2i());
	return target->rect;
}

RID FixedFunctionTextureStorage::render_target_get_texture(RID p_render_target) const {
	const FFRenderTarget *target = _get_render_target(p_render_target);
	ERR_FAIL_COND_V(!target, RID());
	return target->texture;
}

bool FixedFunctionTextureStorage::render_target_is_ready(RID p_render_target) const {
	const FFRenderTarget *target = _get_render_target(p_render_target);
	ERR_FAIL_COND_V(!target, false);
	RID native;
	return !(target->flags & FFRenderTarget::FLAG_DRAW_FAILED) && texture_get_device_texture(target->texture, native) == OK && _device.texture_is_render_target_ready(native);
}

Error FixedFunctionTextureStorage::render_target_set_failed(RID p_render_target, bool p_failed) {
	FFRenderTarget *target = _get_render_target(p_render_target);
	if (!target) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_failed) {
		target->flags |= FFRenderTarget::FLAG_DRAW_FAILED;
	} else {
		target->flags &= ~FFRenderTarget::FLAG_DRAW_FAILED;
	}
	return OK;
}

bool FixedFunctionTextureStorage::render_target_has_failed(RID p_render_target) const {
	const FFRenderTarget *target = _get_render_target(p_render_target);
	return target && (target->flags & FFRenderTarget::FLAG_DRAW_FAILED);
}

Error FixedFunctionTextureStorage::render_target_mark_for_recovery(RID p_render_target) {
	FFRenderTarget *target = _get_render_target(p_render_target);
	if (!target) {
		return ERR_INVALID_PARAMETER;
	}
	target->redraw_on_loss = true;
	return OK;
}

bool FixedFunctionTextureStorage::render_target_needs_redraw(RID p_render_target) const {
	const FFRenderTarget *target = _get_render_target(p_render_target);
	ERR_FAIL_COND_V(!target, false);
	return target->rect.size.x > 0 && target->rect.size.y > 0 &&
			!_get_texture(target->texture)->free_pending && target->redraw_on_loss && !render_target_has_failed(p_render_target) && !render_target_is_ready(p_render_target);
}

Error FixedFunctionTextureStorage::render_target_mark_used(RID p_render_target) {
	FFRenderTarget *target = _get_render_target(p_render_target);
	if (!target) {
		return ERR_INVALID_PARAMETER;
	}
	target->used = true;
	return OK;
}

bool FixedFunctionTextureStorage::render_target_was_used(RID p_render_target) const {
	const FFRenderTarget *target = _get_render_target(p_render_target);
	ERR_FAIL_COND_V(!target, false);
	return target->used;
}

Error FixedFunctionTextureStorage::render_target_clear_used(RID p_render_target) {
	FFRenderTarget *target = _get_render_target(p_render_target);
	if (!target) {
		return ERR_INVALID_PARAMETER;
	}
	target->used = false;
	return OK;
}

Error FixedFunctionTextureStorage::_release_device_texture(FFTexture *p_texture) {
	Map<const FFTexture *, RID>::Element *coverage = _distance_field_textures.find(p_texture);
	if (coverage) {
		if (coverage->get().is_valid()) {
			const Error error = _device.texture_free(coverage->get());
			if (error != OK) {
				return error;
			}
		}
		_distance_field_textures.erase(coverage);
	}
	if (p_texture->device_texture.is_valid()) {
		const Error error = _device.texture_free(p_texture->device_texture);
		if (error != OK) {
			return error;
		}
		p_texture->device_texture = RID();
	}
	return OK;
}

void FixedFunctionTextureStorage::_free_texture(FFTexture *p_texture) {
	_discard_cpu_sample(p_texture);
	_distance_field_textures.erase(p_texture);
	if (_scene_storage) {
		_scene_storage->sky_detach_panorama(p_texture->self);
	}
	_cancel_upload(p_texture);
	_pending_releases.erase(p_texture);
	if (p_texture->proxy) {
		p_texture->proxy->proxy_owners.erase(p_texture);
	}
	for (Set<FFTexture *>::Element *E = p_texture->proxy_owners.front(); E; E = E->next()) {
		E->get()->proxy = nullptr;
	}
	_texture_owner.free(p_texture->self);
	_textures.erase(p_texture);
	memdelete(p_texture);
}

void FixedFunctionTextureStorage::_free_render_target(FFRenderTarget *p_render_target) {
	_free_texture(_get_texture(p_render_target->texture));
	_render_target_owner.free(p_render_target->self);
	_render_targets.erase(p_render_target);
	memdelete(p_render_target);
}

Error FixedFunctionTextureStorage::free(RID p_rid) {
	if (_updating) {
		return ERR_BUSY;
	}
	FFUpdateScope update_scope(_updating);
	FFRenderTarget *target = _get_render_target(p_rid);
	if (target) {
		FFTexture *texture = _get_texture(target->texture);
		const bool was_pending = texture->free_pending;
		texture->free_pending = true;
		const Error error = _release_device_texture(texture);
		if (error != OK) {
			texture->free_pending = was_pending || _pending_releases.has(texture);
			return error;
		}
		_free_render_target(target);
		return OK;
	}
	FFTexture *texture = _get_texture(p_rid);
	if (!texture) {
		return ERR_INVALID_PARAMETER;
	}
	if (texture->render_target) {
		return ERR_ALREADY_IN_USE;
	}
	const bool was_pending = texture->free_pending;
	texture->free_pending = true;
	const Error error = _release_device_texture(texture);
	if (error != OK) {
		texture->free_pending = was_pending || _pending_releases.has(texture);
		return error;
	}
	_free_texture(texture);
	return OK;
}

Error FixedFunctionTextureStorage::clear() {
	if (_updating) {
		return ERR_BUSY;
	}
	FFUpdateScope update_scope(_updating);
	_clearing = true;
	const Error retired_error = _release_retired_textures();
	if (retired_error != OK) {
		_clearing = false;
		return retired_error;
	}
	// Keep logical records/links intact until all native releases succeed.
	for (List<FFTexture *>::Element *E = _textures.front(); E; E = E->next()) {
		const Error error = _release_device_texture(E->get());
		if (error != OK) {
			_clearing = false;
			return error;
		}
	}
	_clear_records();
	_clearing = false;
	return OK;
}

void FixedFunctionTextureStorage::_clear_records() {
	while (_render_targets.front()) {
		_free_render_target(_render_targets.front()->get());
	}
	while (_textures.front()) {
		_free_texture(_textures.front()->get());
	}
}

FixedFunctionTextureStorage::FixedFunctionTextureStorage(FixedFunctionGraphicsDevice &p_device, FixedFunctionSceneStorage *p_scene_storage) :
		_device(p_device),
		_scene_storage(p_scene_storage),
		_updating(false),
		_clearing(false) {
}

FixedFunctionTextureStorage::~FixedFunctionTextureStorage() {
	const Error error = clear();
	if (error != OK) {
		ERR_PRINT(String("Fixed Function storage cleanup failed (Error ") + itos(error) + "); native textures remain device-owned until device finalization.");
		// Contract violation: still release CPU records. The device owns the native allocations.
		_clear_records();
	}
}
