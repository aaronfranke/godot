#include "graphics_device_opengl1.h"

#include "opengl1_headers.h"

#include <string.h>

bool FixedFunctionGraphicsDeviceOpenGL1::supports_compressed_texture(Image::Format p_format) const {
	return _compressed_texture_upload && p_format >= Image::FORMAT_DXT1 && p_format <= Image::FORMAT_DXT5;
}

Error FixedFunctionGraphicsDeviceOpenGL1::_upload_compressed_texture_levels(const GL11Texture &p_texture, const PoolVector<uint8_t> &p_blocks, uint8_t p_levels, Image::Format p_format) {
#ifdef WINDOWS_ENABLED
	typedef void(APIENTRY * CompressedTexImage2D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
#else
	typedef void (*CompressedTexImage2D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
#endif
	// Win32's OpenGL 1.1 header has no S3TC tokens. DXT1 can carry one-bit alpha.
	const GLenum formats[] = { 0x83F1, 0x83F2, 0x83F3 }; // GL_COMPRESSED_RGBA_S3TC_DXT{1,3,5}_EXT.
	const GLenum format = formats[p_format - Image::FORMAT_DXT1];
	const CompressedTexImage2D upload = reinterpret_cast<CompressedTexImage2D>(_compressed_texture_upload);
	Size2i size = p_texture.upload_size;
	const PoolVector<uint8_t>::Read blocks = p_blocks.read();
	int offset = 0;
	for (uint8_t level = 0; level < p_levels; level++) {
		const int bytes = Image::get_image_data_size(size.x, size.y, p_format, false);
		if (bytes <= 0 || bytes > p_blocks.size() - offset) {
			return ERR_INVALID_DATA;
		}
		upload(GL_TEXTURE_2D, level, format, size.x, size.y, 0, GLsizei(bytes), blocks.ptr() + offset);
		const Error error = _check_gl_error();
		if (error != OK) {
			return error;
		}
		offset += bytes;
		size = Size2i(MAX(size.x >> 1, 1), MAX(size.y >> 1, 1));
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::_upload_texture_levels(const GL11Texture &p_texture, GL11TextureView p_view, const PoolVector<uint8_t> &p_pixels, uint8_t p_levels) {
	const PoolVector<uint8_t>::Read pixels = p_pixels.read();
	const int border = _clamp_to_edge ? 0 : 1;
	const GLint formats[] = { GL_RGBA8, GL_RGB8, GL_ALPHA8 };
	Size2i interior = p_texture.upload_size;
	size_t offset = 0;
	Vector<uint8_t> bordered;
	for (uint8_t level = 0; level < p_levels; level++) {
		const size_t level_bytes = size_t(interior.x) * interior.y * 4;
		if (offset > size_t(p_pixels.size()) || level_bytes > size_t(p_pixels.size()) - offset) {
			return ERR_INVALID_DATA;
		}
		const uint8_t *upload = pixels.ptr() + offset;
		const Size2i size = interior + Size2i(2 * border, 2 * border);
		if (border) {
			// Core 1.1 needs replicated GL_CLAMP borders on every mip level,
			// including the rectangular tail after one interior axis reaches one.
			const uint64_t bytes = uint64_t(size.x) * size.y * 4;
			if (bytes > INT32_MAX) {
				return ERR_OUT_OF_MEMORY;
			}
			const Error resized = bordered.resize(int(bytes));
			if (resized != OK) {
				return resized;
			}
			const size_t row_bytes = size_t(interior.x) * 4;
			const size_t stride = row_bytes + 8;
			uint8_t *row = bordered.ptrw();
			for (int y = 0; y < size.y; y++, row += stride) {
				const uint8_t *source = upload + size_t(CLAMP(y - 1, 0, interior.y - 1)) * row_bytes;
				memcpy(row, source, 4);
				memcpy(row + 4, source, row_bytes);
				memcpy(row + 4 + row_bytes, source + row_bytes - 4, 4);
			}
			upload = bordered.ptr();
		}
		glTexImage2D(GL_TEXTURE_2D, level, formats[p_view], size.x, size.y, border, GL_RGBA, GL_UNSIGNED_BYTE, upload);
		const Error error = _check_gl_error();
		if (error != OK) {
			return error;
		}
		offset += level_bytes;
		interior = Size2i(MAX(interior.x >> 1, 1), MAX(interior.y >> 1, 1));
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::_upload_texture_view(GL11Texture &p_texture, GL11TextureView p_view) {
	if (p_texture.framebuffer) {
		return _copy_framebuffer_view(p_texture, p_view);
	}
	if (p_texture.names[p_view]) {
		return OK;
	}

	// Core texture environments use the internal format to decide which channels
	// modulate the vertex. Prepare a fresh upload from the same accepted snapshot.
	Ref<Image> upload;
	Error error;
	if (can_upload_compressed_texture(p_texture.source)) {
		error = prepare_texture_upload(p_texture.source, upload);
	} else {
		Ref<Image> decoded;
		error = prepare_rgba8_upload(p_texture.source, decoded, true);
		if (error == OK) {
			error = prepare_texture_upload(decoded, upload);
		}
	}
	if (error != OK) {
		return error;
	}
	if (upload->is_compressed()) {
		if (p_view == TEXTURE_RGBA) {
			error = _upload_texture_view_data(p_texture, p_view, upload->get_data(), upload->get_mipmap_count() + 1, upload->get_format());
			if (error == OK) {
				return OK;
			}
			// Retry a refused compressed upload with the full decoded mip chain.
		}
		Ref<Image> decoded;
		error = prepare_rgba8_upload(upload, decoded, true);
		if (error != OK) {
			return error;
		}
		upload = decoded;
	}
	return _upload_texture_view_data(p_texture, p_view, upload->get_data(), upload->get_mipmap_count() + 1);
}

Error FixedFunctionGraphicsDeviceOpenGL1::_upload_texture_view_data(GL11Texture &p_texture, GL11TextureView p_view, const PoolVector<uint8_t> &p_pixels, uint8_t p_levels, Image::Format p_format) {
	GLuint name = 0;
	glGenTextures(1, &name);
	if (!name) {
		const Error error = _check_gl_error();
		return error == OK ? ERR_CANT_CREATE : error;
	}
	glBindTexture(GL_TEXTURE_2D, name);
	const bool compressed = p_format != Image::FORMAT_RGBA8;
	uint8_t levels = p_levels;
	Error error = compressed ? _upload_compressed_texture_levels(p_texture, p_pixels, levels, p_format) : _upload_texture_levels(p_texture, p_view, p_pixels, levels);
	if (!compressed && error != OK && error != ERR_INVALID_DATA && levels > 1) {
		// Delete incomplete sublevels before retrying a standalone base image.
		// Each lazy channel view tracks the levels its own native allocation accepted.
		glDeleteTextures(1, &name);
		glGenTextures(1, &name);
		if (!name) {
			const Error generated = _check_gl_error();
			return generated == OK ? ERR_CANT_CREATE : generated;
		}
		glBindTexture(GL_TEXTURE_2D, name);
		levels = 1;
		error = _upload_texture_levels(p_texture, p_view, p_pixels, levels);
	}
	if (error != OK) {
		glDeleteTextures(1, &name);
		return error;
	}
	p_texture.names[p_view] = name;
	p_texture.uploaded_levels[p_view] = levels;
	if (compressed) {
		p_texture.compressed_format = p_format;
	}
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (_frame_active && !p_allow_during_frame) {
		return ERR_BUSY;
	}
	if (p_image.is_null() || p_image->empty()) {
		return ERR_INVALID_PARAMETER;
	}
	const Size2i size(p_image->get_width(), p_image->get_height());
	Size2i upload_size;
	Error error = get_texture_upload_size(size.x, size.y, upload_size);
	if (error != OK) {
		return error;
	}
	GL11Texture *texture = memnew(GL11Texture);
	texture->size = size;
	texture->upload_size = upload_size;
	// Copy the Image metadata and share its bytes through copy-on-write. Later
	// source edits or replacements leave this device texture's lazy views intact.
	texture->source = p_image->duplicate();
	error = _upload_texture_view(*texture, TEXTURE_RGBA);
	if (error != OK) {
		memdelete(texture);
		return error;
	}
	texture->rid = _texture_owner.make_rid(texture);
	_textures.push_back(texture);
	r_texture = texture->rid;
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (!_capabilities.render_target_opaque || (p_transparent && !_capabilities.render_target_transparent)) {
		return ERR_UNAVAILABLE;
	}
	FFCapabilities limits = _capabilities;
	limits.max_texture_width = MIN(limits.max_texture_width, MIN(uint32_t(_max_viewport.x), _max_renderbuffer_size));
	limits.max_texture_height = MIN(limits.max_texture_height, MIN(uint32_t(_max_viewport.y), _max_renderbuffer_size));
	Size2i upload_size;
	Error error = get_texture_upload_size(limits, p_width, p_height, upload_size);
	if (error != OK) {
		return error;
	}
	GL11Texture *texture = memnew(GL11Texture);
	texture->size = Size2i(p_width, p_height);
	texture->upload_size = upload_size;
	texture->transparent = p_transparent;
	error = _allocate_framebuffer(*texture);
	if (error != OK) {
		memdelete(texture);
		return error;
	}
	texture->rid = _texture_owner.make_rid(texture);
	_textures.push_back(texture);
	r_texture = texture->rid;
	return OK;
}

bool FixedFunctionGraphicsDeviceOpenGL1::texture_is_render_target_ready(RID p_texture) const {
	if (!_initialized || Thread::get_caller_id() != _owner_thread || !_texture_owner.owns(p_texture)) {
		return false;
	}
	const GL11Texture *texture = _texture_owner.getptr(p_texture);
	return texture != _render_target && texture->framebuffer && texture->rendered;
}

uint64_t FixedFunctionGraphicsDeviceOpenGL1::_texture_bytes(const GL11Texture &p_texture) const {
	if (p_texture.framebuffer) {
		// RGBA8 color and an optional 16-bit depth attachment.
		unsigned int bytes_per_texel = p_texture.depth ? 6 : 4;
		if (p_texture.names[TEXTURE_RGB]) {
			bytes_per_texel += 3;
		}
		if (p_texture.names[TEXTURE_ALPHA]) {
			bytes_per_texel++;
		}
		return uint64_t(p_texture.upload_size.x) * p_texture.upload_size.y * bytes_per_texel;
	}
	const uint64_t border = _clamp_to_edge ? 0 : 2;
	const unsigned int bytes_per_view[] = { 4, 3, 1 };
	uint64_t bytes = 0;
	for (int view = 0; view < TEXTURE_VIEW_COUNT; view++) {
		Size2i interior = p_texture.upload_size;
		for (uint8_t level = 0; level < p_texture.uploaded_levels[view]; level++) {
			if (view == TEXTURE_RGBA && p_texture.compressed_format != Image::FORMAT_MAX) {
				bytes += Image::get_image_data_size(interior.x, interior.y, p_texture.compressed_format, false);
			} else {
				bytes += (uint64_t(interior.x) + border) * (uint64_t(interior.y) + border) * bytes_per_view[view];
			}
			interior = Size2i(MAX(interior.x >> 1, 1), MAX(interior.y >> 1, 1));
		}
	}
	return bytes;
}

Error FixedFunctionGraphicsDeviceOpenGL1::texture_get_info(RID p_texture, FFTextureInfo &r_info) const {
	if (!_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	const GL11Texture &texture = *_texture_owner.getptr(p_texture);
	r_info.size = texture.size;
	r_info.bytes = _texture_bytes(texture);
	return OK;
}

uint64_t FixedFunctionGraphicsDeviceOpenGL1::get_texture_memory_usage() const {
	uint64_t total = 0;
	for (const List<GL11Texture *>::Element *E = _textures.front(); E; E = E->next()) {
		total += MIN(_texture_bytes(*E->get()), UINT64_MAX - total);
	}
	return total;
}

Error FixedFunctionGraphicsDeviceOpenGL1::texture_free(RID p_texture) {
	const GL11CurrentContext current(*this);
	if (current.error != OK) {
		return current.error;
	}
	if (_frame_active) {
		return ERR_BUSY;
	}
	if (!_texture_owner.owns(p_texture)) {
		return ERR_INVALID_PARAMETER;
	}
	GL11Texture *texture = _texture_owner.getptr(p_texture);
	if (texture->framebuffer) {
		_delete_framebuffer(*texture);
	}
	glDeleteTextures(TEXTURE_VIEW_COUNT, texture->names);
	const Error deleted = _check_gl_error();
	if (deleted != OK) {
		return deleted;
	}
	_textures.erase(texture);
	_texture_owner.free(p_texture);
	memdelete(texture);
	return OK;
}

Error FixedFunctionGraphicsDeviceOpenGL1::_bind_texture(GL11Texture *p_texture, const FFDrawParameters &p_state) {
	// Only opaque targets contribute a constant one to alpha.
	const bool opaque_target = p_texture && p_texture->framebuffer && !p_texture->transparent;
	if (!p_texture || (!p_state.texture_rgb_enabled && (!p_state.texture_alpha_enabled || opaque_target))) {
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		return OK;
	}
	GL11TextureView view = TEXTURE_RGBA;
	if (!p_texture->framebuffer || p_texture->transparent) {
		if (!p_state.texture_rgb_enabled) {
			view = TEXTURE_ALPHA;
		} else if (!p_state.texture_alpha_enabled) {
			view = TEXTURE_RGB;
		}
		const Error uploaded = _upload_texture_view(*p_texture, view);
		if (uploaded != OK) {
			return uploaded;
		}
	}
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindTexture(GL_TEXTURE_2D, p_texture->names[view]);
	const GLint filter = p_state.sampler_flags & SAMPLER_FILTER_LINEAR ? GL_LINEAR : GL_NEAREST;
	const FFMipFilter mip = get_mip_filter(p_state.sampler_flags, p_texture->uploaded_levels[view]);
	GLint min_filter = filter;
	if (mip == MIP_FILTER_LINEAR) {
		min_filter = filter == GL_LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
	} else if (mip == MIP_FILTER_POINT) {
		min_filter = filter == GL_LINEAR ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	// GL_CLAMP_TO_EDGE entered core in 1.2 and is missing from the Win32 1.1 header.
	enum { CLAMP_TO_EDGE = 0x812F };
	const GLint clamp = _clamp_to_edge ? GLint(CLAMP_TO_EDGE) : GL_CLAMP;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, p_state.sampler_flags & SAMPLER_REPEAT_U ? GL_REPEAT : clamp);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, p_state.sampler_flags & SAMPLER_REPEAT_V ? GL_REPEAT : clamp);
	return OK;
}
