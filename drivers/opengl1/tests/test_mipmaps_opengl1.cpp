#include "test_mipmaps_opengl1.h"

#include "drivers/opengl1/opengl1_headers.h"

#include <stdio.h>
#include <string.h>

#define FF_GL_MIP_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("OpenGL mipmaps FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

Ref<Image> TestFixedFunctionOpenGL1Mipmaps::_authored_image(Image::Format p_format) {
	const int width = 32, height = 8;
	Vector<uint8_t> data;
	data.resize(Image::get_image_data_size(width, height, p_format, true));
	{
		uint8_t *write = data.ptrw();
		memset(write, 0, data.size());
		int w = width, h = height;
		const int levels = Image::get_image_required_mipmaps(width, height, p_format) + 1;
		for (int level = 0; level < levels; level++) {
			const int64_t offset = Image::get_image_mipmap_offset(width, height, p_format, level);
			const int64_t size = Image::get_image_data_size(w, h, p_format, false);
			int channel = 1;
			if (level == 0) {
				channel = 0;
			} else if (w <= 2) {
				channel = 2;
			}
			if (p_format >= Image::FORMAT_DXT1 && p_format <= Image::FORMAT_DXT5) {
				const uint16_t colors[] = { 0xf800, 0x07e0, 0x001f };
				const int block_size = p_format == Image::FORMAT_DXT1 ? 8 : 16;
				for (int64_t i = 0; i < size; i += block_size) {
					uint8_t *block = write + offset + i;
					if (p_format == Image::FORMAT_DXT3) {
						memset(block, 255, 8);
					} else if (p_format == Image::FORMAT_DXT5) {
						block[0] = 255;
					}
					block[block_size - 8] = uint8_t(colors[channel]);
					block[block_size - 7] = uint8_t(colors[channel] >> 8);
				}
			} else {
				const int stride = p_format == Image::FORMAT_RGBA8 ? 4 : 3;
				for (int64_t i = 0; i < size; i += stride) {
					write[offset + i + channel] = 255;
					if (stride == 4) {
						write[offset + i + 3] = uint8_t((channel + 1) * 64);
					}
				}
			}
			w = MAX(w >> 1, 1);
			h = MAX(h >> 1, 1);
		}
	}
	return Image::create_from_data(width, height, true, p_format, data);
}

bool TestFixedFunctionOpenGL1Mipmaps::_draw(FixedFunctionGraphicsDeviceOpenGL1 &p_device, RID p_texture, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, float p_span, std::vector<uint32_t> &r_pixels) {
	const FixedFunctionVertex vertices[] = {
		{ -1, 1, 0, 0xffffffff, 0, 0 },
		{ 1, 1, 0, 0xffffffff, p_span, 0 },
		{ 1, -1, 0, 0xffffffff, p_span, p_span },
		{ -1, 1, 0, 0xffffffff, 0, 0 },
		{ 1, -1, 0, 0xffffffff, p_span, p_span },
		{ -1, -1, 0, 0xffffffff, 0, p_span },
	};
	FF_GL_MIP_CHECK(p_device.begin_frame(0xff0000ff) == OK);
	FF_GL_MIP_CHECK(p_device.draw_triangles(vertices, 6, Projection(), p_texture, p_state) == OK);
	FF_GL_MIP_CHECK(p_device.end_frame(false) == OK);
	r_pixels.resize(size_t(p_device.get_width()) * p_device.get_height());
	FF_GL_MIP_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	return true;
}

bool TestFixedFunctionOpenGL1Mipmaps::_pixel(const std::vector<uint32_t> &p_pixels, uint32_t p_expected) {
	const uint32_t actual = p_pixels[p_pixels.size() / 2];
	for (int shift = 0; shift < 32; shift += 8) {
		FF_GL_MIP_CHECK(Math::abs(int((actual >> shift) & 255) - int((p_expected >> shift) & 255)) <= 8);
	}
	return true;
}

bool TestFixedFunctionOpenGL1Mipmaps::_levels(FixedFunctionGraphicsDeviceOpenGL1 &p_device, RID p_texture) {
	const FixedFunctionGraphicsDeviceOpenGL1::GL11CurrentContext current(p_device);
	FF_GL_MIP_CHECK(current.error == OK);
	const FixedFunctionGraphicsDeviceOpenGL1::GL11Texture *texture = p_device._texture_owner.get_or_null(p_texture);
	FF_GL_MIP_CHECK(texture && !texture->framebuffer);
	const int expected_levels = Image::get_image_required_mipmaps(texture->upload_size.x, texture->upload_size.y, Image::FORMAT_RGBA8) + 1;
	const int border = p_device._clamp_to_edge ? 0 : 1;
	const unsigned int bytes_per_view[] = { 4, 3, 1 };
	uint64_t bytes = 0;
	for (int view = 0; view < FixedFunctionGraphicsDeviceOpenGL1::TEXTURE_VIEW_COUNT; view++) {
		if (!texture->names[view]) {
			continue;
		}
		FF_GL_MIP_CHECK(texture->uploaded_levels[view] == expected_levels);
		glBindTexture(GL_TEXTURE_2D, texture->names[view]);
		Size2i interior = texture->upload_size;
		for (int level = 0; level < expected_levels; level++) {
			GLint w = 0, h = 0, native_border = -1;
			glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &w);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &h);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_BORDER, &native_border);
			FF_GL_MIP_CHECK(w == interior.x + 2 * border && h == interior.y + 2 * border && native_border == border);
			if (view == FixedFunctionGraphicsDeviceOpenGL1::TEXTURE_RGBA && texture->compressed_format != Image::FORMAT_MAX) {
				enum { TEXTURE_COMPRESSED = 0x86A1,
					TEXTURE_COMPRESSED_IMAGE_SIZE = 0x86A0 };
				GLint compressed = 0, image_bytes = 0;
				glGetTexLevelParameteriv(GL_TEXTURE_2D, level, TEXTURE_COMPRESSED, &compressed);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, level, TEXTURE_COMPRESSED_IMAGE_SIZE, &image_bytes);
				FF_GL_MIP_CHECK(compressed == GL_TRUE && image_bytes == Image::get_image_data_size(interior.x, interior.y, texture->compressed_format, false));
				bytes += image_bytes;
			} else {
				bytes += uint64_t(w) * h * bytes_per_view[view];
			}
			interior = Size2i(MAX(interior.x >> 1, 1), MAX(interior.y >> 1, 1));
		}
	}
	FF_GL_MIP_CHECK(glGetError() == GL_NO_ERROR);
	FixedFunctionGraphicsDevice::FFTextureInfo info;
	FF_GL_MIP_CHECK(p_device.texture_get_info(p_texture, info) == OK && info.bytes == bytes);
	return true;
}

bool TestFixedFunctionOpenGL1Mipmaps::_authored(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDeviceOpenGL1 &p_device, Image::Format p_format, std::vector<uint32_t> &r_pixels) {
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	Ref<Image> image = _authored_image(p_format);
	const Vector<uint8_t> original = image->get_data();
	const RID texture = storage.texture_create();
	FF_GL_MIP_CHECK(storage.texture_set_data(texture, image) == OK);
	RID native;
	FF_GL_MIP_CHECK(storage.texture_get_device_texture(texture, native) == OK);
	const FixedFunctionGraphicsDeviceOpenGL1::GL11Texture *uploaded = p_device._texture_owner.get_or_null(native);
	FF_GL_MIP_CHECK(uploaded && uploaded->source->get_format() == p_format);
	if (image->is_compressed()) {
		FF_GL_MIP_CHECK(uploaded->compressed_format == (p_device.supports_compressed_texture(p_format) ? p_format : Image::FORMAT_MAX));
	} else {
		image->fill(Color(1, 0, 1)); // Lazy channel views must retain accepted mip data.
	}
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	const uint8_t repeat = FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V;
	const uint8_t mipmaps = repeat | FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
	state.sampler_flags = repeat;
	FF_GL_MIP_CHECK(_draw(p_device, native, state, 256, r_pixels) && _pixel(r_pixels, 0xffff0000));
	state.sampler_flags = mipmaps;
	FF_GL_MIP_CHECK(_draw(p_device, native, state, 256, r_pixels) && _pixel(r_pixels, 0xff0000ff));
	state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
	FF_GL_MIP_CHECK(_draw(p_device, native, state, 256, r_pixels) && _pixel(r_pixels, 0xff0000ff));
	// Choose a fractional LOD between authored red and green levels. A regenerated
	// chain or nearest-level selection cannot produce both channels here.
	const float span = float(p_device.get_width()) / 32 * Math::pow(2.0f, 0.75f);
	FF_GL_MIP_CHECK(_draw(p_device, native, state, span, r_pixels));
	const uint32_t blended = r_pixels[r_pixels.size() / 2];
	FF_GL_MIP_CHECK(((blended >> 16) & 255) > 16 && ((blended >> 8) & 255) > 16 && (blended & 255) < 8);
	state.sampler_flags = mipmaps;
	FF_GL_MIP_CHECK(_draw(p_device, native, state, span, r_pixels) && _pixel(r_pixels, 0xff00ff00));
	state.sampler_flags = repeat;
	FF_GL_MIP_CHECK(_draw(p_device, native, state, 256, r_pixels) && _pixel(r_pixels, 0xffff0000));
	state.sampler_flags = mipmaps;
	FF_GL_MIP_CHECK(_draw(p_device, native, state, 0.1f, r_pixels) && _pixel(r_pixels, 0xffff0000));
	state.sampler_flags = mipmaps;
	state.texture_rgb_enabled = false;
	state.alpha_cutoff_enabled = true;
	state.alpha_cutoff = 0.6f;
	FF_GL_MIP_CHECK(_draw(p_device, native, state, 256, r_pixels) && _pixel(r_pixels, 0xffffffff));
	if (p_format == Image::FORMAT_RGBA8) {
		state.sampler_flags = repeat;
		FF_GL_MIP_CHECK(_draw(p_device, native, state, 256, r_pixels) && _pixel(r_pixels, 0xff0000ff));
	}
	state.sampler_flags = mipmaps;
	state.texture_rgb_enabled = true;
	state.texture_alpha_enabled = false;
	state.alpha_cutoff = 0.9f;
	FF_GL_MIP_CHECK(_draw(p_device, native, state, 256, r_pixels) && _pixel(r_pixels, 0xff0000ff));
	FF_GL_MIP_CHECK(_levels(p_device, native));
	FF_GL_MIP_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK);
	FF_GL_MIP_CHECK(storage.texture_get_device_texture(texture, native) == OK);
	FF_GL_MIP_CHECK(_draw(p_device, native, state, 256, r_pixels) && _pixel(r_pixels, 0xff0000ff));
	FF_GL_MIP_CHECK(p_context.resize(128, 96) == OK && p_context.prepare_resources() == OK);
	const Ref<Image> snapshot = storage.texture_get_data(texture);
	FF_GL_MIP_CHECK(snapshot.is_valid() && snapshot->has_mipmaps() && snapshot->get_format() == p_format);
	const Vector<uint8_t> retained = snapshot->get_data();
	FF_GL_MIP_CHECK(retained.size() == original.size());
	FF_GL_MIP_CHECK(memcmp(retained.ptr(), original.ptr(), original.size()) == 0);
	FF_GL_MIP_CHECK(storage.free(texture) == OK);
	return true;
}

bool TestFixedFunctionOpenGL1Mipmaps::_adapted(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDeviceOpenGL1 &p_device, std::vector<uint32_t> &r_pixels) {
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	Ref<Image> image = Image::create_empty(13, 7, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 1));
	FF_GL_MIP_CHECK(image->generate_mipmaps() == OK);
	const RID texture = storage.texture_create();
	FF_GL_MIP_CHECK(storage.texture_set_data(texture, image) == OK);
	RID native;
	FF_GL_MIP_CHECK(storage.texture_get_device_texture(texture, native) == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR | FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
	FF_GL_MIP_CHECK(_draw(p_device, native, state, 256, r_pixels) && _pixel(r_pixels, 0xffff00ff));
	FF_GL_MIP_CHECK(_levels(p_device, native));
	FF_GL_MIP_CHECK(storage.texture_get_size(texture) == Size2i(13, 7) && image->get_width() == 13 && image->get_height() == 7 && image->has_mipmaps());
	FF_GL_MIP_CHECK(storage.free(texture) == OK);
	return true;
}

bool TestFixedFunctionOpenGL1Mipmaps::_fallback(FixedFunctionGraphicsDeviceOpenGL1 &p_device) {
	const FixedFunctionGraphicsDeviceOpenGL1::GL11CurrentContext current(p_device);
	FF_GL_MIP_CHECK(current.error == OK);
	FixedFunctionGraphicsDeviceOpenGL1::GL11Texture texture;
	texture.size = texture.upload_size = Size2i(1, 1);
	// Safely force a real GL level-limit error using tiny images. This intentionally
	// bypasses Image validation; ordinary complete Image chains never have 32 levels.
	const uint8_t levels = 32;
	Vector<uint8_t> pixels;
	FF_GL_MIP_CHECK(pixels.resize(levels * 4) == OK);
	memset(pixels.ptrw(), 255, pixels.size());
	for (int view = 0; view < FixedFunctionGraphicsDeviceOpenGL1::TEXTURE_VIEW_COUNT; view++) {
		const FixedFunctionGraphicsDeviceOpenGL1::GL11TextureView channel = static_cast<FixedFunctionGraphicsDeviceOpenGL1::GL11TextureView>(view);
		FF_GL_MIP_CHECK(p_device._upload_texture_view_data(texture, channel, pixels, levels) == OK && texture.uploaded_levels[view] == 1);
		GLint unused_level = -1;
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &unused_level);
		FF_GL_MIP_CHECK(unused_level == 0 && glGetError() == GL_NO_ERROR);
		FF_GL_MIP_CHECK(p_device.get_mip_filter(FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS, texture.uploaded_levels[view]) == FixedFunctionGraphicsDevice::MIP_FILTER_NONE);
	}
	const uint64_t width = p_device._clamp_to_edge ? 1 : 3;
	FF_GL_MIP_CHECK(p_device._texture_bytes(texture) == width * width * 8);
	glDeleteTextures(FixedFunctionGraphicsDeviceOpenGL1::TEXTURE_VIEW_COUNT, texture.names);
	FF_GL_MIP_CHECK(glGetError() == GL_NO_ERROR);
	printf("PASS: rejected GL mip allocations retry clean base-only channel views without incomplete sublevels or stale GL errors.\n");
	return true;
}

bool TestFixedFunctionOpenGL1Mipmaps::_compressed_alpha(FixedFunctionGraphicsDeviceOpenGL1 &p_device, std::vector<uint32_t> &r_pixels) {
	Vector<uint8_t> blocks;
	FF_GL_MIP_CHECK(blocks.resize(8) == OK);
	memset(blocks.ptrw(), 255, blocks.size());
	blocks.write[0] = blocks.write[1] = 0; // Endpoint 0 < endpoint 1, selector 3 is transparent.
	Ref<Image> image = Image::create_from_data(4, 4, false, Image::FORMAT_DXT1, blocks);
	RID texture;
	FF_GL_MIP_CHECK(p_device.texture_create(image, texture) == OK);
	const FixedFunctionGraphicsDeviceOpenGL1::GL11Texture *uploaded = p_device._texture_owner.get_or_null(texture);
	FF_GL_MIP_CHECK(uploaded && uploaded->source->get_format() == Image::FORMAT_DXT1);
	image->set_as_black(); // The caller edits its blocks before either lazy view exists.
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.alpha_cutoff_enabled = true;
	state.alpha_cutoff = 0.5f;
	FF_GL_MIP_CHECK(_draw(p_device, texture, state, 1, r_pixels) && _pixel(r_pixels, 0xff0000ff));
	state.texture_rgb_enabled = false;
	FF_GL_MIP_CHECK(_draw(p_device, texture, state, 1, r_pixels) && _pixel(r_pixels, 0xff0000ff));
	state.texture_rgb_enabled = true;
	state.texture_alpha_enabled = false;
	FF_GL_MIP_CHECK(_draw(p_device, texture, state, 1, r_pixels) && _pixel(r_pixels, 0xff000000));
	FF_GL_MIP_CHECK(p_device.texture_free(texture) == OK);
#ifdef WINDOWS_ENABLED
	const FixedFunctionOpenGL1Context::ProcAddress native_upload = p_device._compressed_texture_upload;
	if (native_upload) {
		struct RefusedS3TCUpload {
			static void APIENTRY upload(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *) {
				glEnable(GL_TRIANGLES); // Generate GL_INVALID_ENUM.
			}
		};
		p_device._compressed_texture_upload = reinterpret_cast<FixedFunctionOpenGL1Context::ProcAddress>(&RefusedS3TCUpload::upload);
		texture = RID();
		const Error error = p_device.texture_create(_authored_image(Image::FORMAT_DXT5), texture);
		p_device._compressed_texture_upload = native_upload;
		FF_GL_MIP_CHECK(error == OK);
		const FixedFunctionGraphicsDeviceOpenGL1::GL11Texture *decoded = p_device._texture_owner.get_or_null(texture);
		FF_GL_MIP_CHECK(decoded && decoded->compressed_format == Image::FORMAT_MAX && decoded->source->get_format() == Image::FORMAT_DXT5);
		state = FixedFunctionGraphicsDevice::FFDrawParameters();
		state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
		FF_GL_MIP_CHECK(_draw(p_device, texture, state, 256, r_pixels) && _pixel(r_pixels, 0xff0000ff));
		FF_GL_MIP_CHECK(_levels(p_device, texture) && p_device.texture_free(texture) == OK);
	}
#endif
	printf("PASS: OpenGL %s BC1-3, immutable compressed snapshots, one-bit alpha, independent views and decoded fallback.\n", p_device.supports_compressed_texture(Image::FORMAT_DXT1) ? "native S3TC" : "decoded");
	return true;
}

bool TestFixedFunctionOpenGL1Mipmaps::test(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDeviceOpenGL1 &p_device, std::vector<uint32_t> &r_pixels) {
	const uint64_t before = p_device.get_texture_memory_usage();
	FF_GL_MIP_CHECK(p_device.get_capabilities().sampler_mip_filters == (FixedFunctionGraphicsDevice::MIP_FILTER_POINT | FixedFunctionGraphicsDevice::MIP_FILTER_LINEAR));
	FF_GL_MIP_CHECK(_authored(p_context, p_device, Image::FORMAT_RGBA8, r_pixels));
	FF_GL_MIP_CHECK(_authored(p_context, p_device, Image::FORMAT_RGB8, r_pixels));
	if (Image::_image_decompress_bc) {
		FF_GL_MIP_CHECK(_authored(p_context, p_device, Image::FORMAT_DXT1, r_pixels));
		FF_GL_MIP_CHECK(_authored(p_context, p_device, Image::FORMAT_DXT3, r_pixels));
		FF_GL_MIP_CHECK(_authored(p_context, p_device, Image::FORMAT_DXT5, r_pixels));
		FF_GL_MIP_CHECK(_compressed_alpha(p_device, r_pixels));
	}
	FF_GL_MIP_CHECK(_adapted(p_context, p_device, r_pixels));
	FF_GL_MIP_CHECK(_fallback(p_device));
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 1, 0));
	RID single;
	FF_GL_MIP_CHECK(p_device.texture_create(image, single) == OK);
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
	FF_GL_MIP_CHECK(_draw(p_device, single, state, 256, r_pixels) && _pixel(r_pixels, 0xffffff00));
	FF_GL_MIP_CHECK(p_device.texture_free(single) == OK);
	if (p_device.get_capabilities().render_target_opaque) {
		RID target;
		FF_GL_MIP_CHECK(p_device.texture_create_render_target(4, 4, target) == OK);
		FF_GL_MIP_CHECK(p_device.begin_frame(0xff00ffff, target) == OK && p_device.end_frame(false) == OK);
		FF_GL_MIP_CHECK(_draw(p_device, target, state, 256, r_pixels) && _pixel(r_pixels, 0xff00ffff));
		FF_GL_MIP_CHECK(p_device.texture_free(target) == OK);
	}
	FF_GL_MIP_CHECK(p_device.get_texture_memory_usage() == before);
	printf("PASS: OpenGL authored mip chains, RGB/BC decode, nearest/trilinear selection, channel views/alpha, borders/accounting, NPOT adaptation, base-only targets and resize.\n");
	return true;
}

#undef FF_GL_MIP_CHECK
