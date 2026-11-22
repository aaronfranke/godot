#include "test_texture_adaptation.h"

#include "test_scene_cpu.h"

#include <stdio.h>
#include <string.h>

#define FF_ADAPT_CHECK(m_condition)                                                \
	do {                                                                           \
		if (!(m_condition)) {                                                      \
			printf("Texture adaptation FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                          \
		}                                                                          \
	} while (false)

bool TestFixedFunctionSceneCPU::_texture_adaptation() {
	FixedFunctionGraphicsDevice::FFCapabilities caps;
	caps.max_texture_width = 33;
	caps.max_texture_height = 25;
	caps.max_texture_stages = 1;
	caps.texture_power_of_two = true;
	caps.sampler_mip_filters = FixedFunctionGraphicsDevice::MIP_FILTER_POINT;
	Size2i size(7, 9);
	FF_ADAPT_CHECK(FixedFunctionGraphicsDevice::get_texture_upload_size(caps, 13, 7, size) == OK && size == Size2i(16, 8));
	FF_ADAPT_CHECK(FixedFunctionGraphicsDevice::get_texture_upload_size(caps, 300, 150, size) == OK && size == Size2i(32, 16));
	FF_ADAPT_CHECK(FixedFunctionGraphicsDevice::get_texture_upload_size(caps, 0, 1, size) == ERR_INVALID_PARAMETER && size == Size2i(32, 16));
	FF_ADAPT_CHECK(FixedFunctionGraphicsDevice::get_texture_upload_size(caps, UINT32_MAX, 1, size) == ERR_INVALID_PARAMETER);
	caps.texture_square_only = true;
	FF_ADAPT_CHECK(FixedFunctionGraphicsDevice::get_texture_upload_size(caps, 13, 7, size) == OK && size == Size2i(16, 16));
	caps.texture_square_only = caps.texture_power_of_two = false;
	FF_ADAPT_CHECK(FixedFunctionGraphicsDevice::get_texture_upload_size(caps, 66, 50, size) == OK && size == Size2i(33, 25));
	caps.max_texture_width = 0;
	FF_ADAPT_CHECK(FixedFunctionGraphicsDevice::get_texture_upload_size(caps, 1, 1, size) == ERR_UNAVAILABLE);
	caps.max_texture_width = 32;
	caps.max_texture_height = 16;
	caps.texture_power_of_two = true;
	FFUnusedDevice device;
	device.set_capabilities(caps);
	device.set_compressed(true);
	for (int compressed = 0; compressed < 2; compressed++) {
		const Image::Format format = compressed ? Image::FORMAT_DXT1 : Image::FORMAT_RGBA8;
		PoolVector<uint8_t> bytes;
		FF_ADAPT_CHECK(bytes.resize(Image::get_image_data_size(64, 32, format, true)) == OK);
		{
			PoolVector<uint8_t>::Write write = bytes.write();
			for (int i = 0; i < bytes.size(); i++) {
				write[i] = uint8_t(i * 17);
			}
		}
		Ref<Image> image = memnew(Image(64, 32, true, format, bytes));
		Ref<Image> upload;
		FF_ADAPT_CHECK(device.prepare_texture_upload(image, upload) == OK);
		FF_ADAPT_CHECK(upload->get_width() == 32 && upload->get_height() == 16 && upload->get_format() == format && upload->has_mipmaps());
		const PoolVector<uint8_t> tail = upload->get_data();
		const int offset = image->get_mipmap_offset(1);
		FF_ADAPT_CHECK(tail.size() == bytes.size() - offset && memcmp(tail.read().ptr(), bytes.read().ptr() + offset, tail.size()) == 0);
		FF_ADAPT_CHECK(image->get_width() == 64 && image->get_height() == 32 && TestFixedFunctionTextureAdaptation::_matches_data(image, bytes));
	}
	Ref<Image> image = memnew(Image(13, 7, false, Image::FORMAT_RGBA8));
	image->fill(Color(1, 0.5, 0, 0.25));
	const PoolVector<uint8_t> original = image->get_data();
	Ref<Image> upload;
	FF_ADAPT_CHECK(device.prepare_texture_upload(image, upload) == OK && upload->get_width() == 16 && upload->get_height() == 8);
	FF_ADAPT_CHECK(image->get_width() == 13 && image->get_height() == 7 && TestFixedFunctionTextureAdaptation::_matches_data(image, original));
	FF_ADAPT_CHECK(image->generate_mipmaps() == OK);
	const PoolVector<uint8_t> authored = image->get_data();
	FF_ADAPT_CHECK(device.prepare_texture_upload(image, upload) == OK && upload->get_width() == 16 && upload->get_height() == 8 && upload->has_mipmaps());
	FF_ADAPT_CHECK(image->has_mipmaps() && TestFixedFunctionTextureAdaptation::_matches_data(image, authored));
	int previous = 0;
	for (int x = 0; x < 7; x++) {
		const Rect2i clip = FixedFunctionGraphicsDevice::get_texture_native_clip(Rect2i(x, 0, 1, 3), Size2i(7, 3), Size2i(8, 4));
		FF_ADAPT_CHECK(clip.position.x == previous);
		previous += clip.size.x;
	}
	FF_ADAPT_CHECK(previous == 8);
	const uint32_t padded[] = { 0x00112233, 0x80445566, 0xdeadbeef, 0xff778899, 0xffaabbcc, 0xdeadbeef };
	uint32_t output[16] = {};
	FF_ADAPT_CHECK(FFUnusedDevice::copy_texture_readback(reinterpret_cast<const uint8_t *>(padded), Size2i(2, 2), 12, Size2i(4, 4), output, 15) == ERR_INVALID_PARAMETER && output[0] == 0);
	FF_ADAPT_CHECK(FFUnusedDevice::copy_texture_readback(reinterpret_cast<const uint8_t *>(padded), Size2i(2, 2), 12, Size2i(4, 4), output, 16) == OK);
	FF_ADAPT_CHECK(output[0] == 0x00112233 && output[3] == 0x80445566 && output[12] == 0xff778899 && output[15] == 0xffaabbcc);
	FF_ADAPT_CHECK(FFUnusedDevice::copy_texture_readback(reinterpret_cast<const uint8_t *>(padded), Size2i(2, 2), 12, Size2i(4, 4), output, 16, false) == OK && output[0] == 0xff112233);
	printf("PASS: CPU upload limits/POT/square conversion, immutable source images, authored RGBA/BC mip tails, shared clip edges and pitched logical readback.\n");
	return true;
}

TestFixedFunctionTextureAdaptation::FFTextureCapsScope::FFTextureCapsScope(FixedFunctionGraphicsDevice &p_device) :
		_caps(const_cast<FixedFunctionGraphicsDevice::FFCapabilities &>(p_device.get_capabilities())), _saved(_caps) {
	// Exercise old-device restrictions on the real backend without changing its API.
	_caps.max_texture_width = MIN(_caps.max_texture_width, uint32_t(33));
	_caps.max_texture_height = MIN(_caps.max_texture_height, uint32_t(25));
	_caps.texture_power_of_two = true;
}

TestFixedFunctionTextureAdaptation::FFTextureCapsScope::~FFTextureCapsScope() {
	_caps = _saved;
}

bool TestFixedFunctionTextureAdaptation::_draw(FixedFunctionGraphicsDevice &p_device, RID p_texture, bool p_repeat, std::vector<uint32_t> &r_pixels) {
	const float uv = p_repeat ? 2.0f : 1.0f;
	const FixedFunctionVertex vertices[] = {
		{ -1, 1, 0, 0xffffffff, 0, 0 },
		{ 1, 1, 0, 0xffffffff, uv, 0 },
		{ 1, -1, 0, 0xffffffff, uv, uv },
		{ -1, 1, 0, 0xffffffff, 0, 0 },
		{ 1, -1, 0, 0xffffffff, uv, uv },
		{ -1, -1, 0, 0xffffffff, 0, uv },
	};
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	if (p_repeat) {
		state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V;
	}
	FF_ADAPT_CHECK(p_device.begin_frame(0xff000000) == OK);
	FF_ADAPT_CHECK(p_device.draw_triangles(vertices, 6, CameraMatrix(), p_texture, state) == OK);
	FF_ADAPT_CHECK(p_device.end_frame(false) == OK);
	r_pixels.resize(size_t(p_device.get_width()) * p_device.get_height());
	FF_ADAPT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	return true;
}

bool TestFixedFunctionTextureAdaptation::test(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
	FF_ADAPT_CHECK(TestFixedFunctionSceneCPU::_texture_adaptation());
	FFTextureCapsScope limits(p_device);
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	const Size2i sizes[] = { Size2i(13, 7), Size2i(63, 27) };
	const bool repeat = p_device.get_capabilities().sampler_repeat;
	for (int i = 0; i < 2; i++) {
		const Size2i size = sizes[i];
		Ref<Image> image = memnew(Image(size.x, size.y, false, Image::FORMAT_RGBA8));
		image->fill(Color(1, 1, 1));
		image->fill_rect(Rect2(0, 0, size.x / 2, size.y / 2), Color(1, 0, 0));
		image->fill_rect(Rect2(size.x / 2, 0, size.x - size.x / 2, size.y / 2), Color(0, 1, 0));
		image->fill_rect(Rect2(0, size.y / 2, size.x / 2, size.y - size.y / 2), Color(0, 0, 1));
		const PoolVector<uint8_t> original = image->get_data();
		const RID texture = storage.texture_create();
		FF_ADAPT_CHECK(storage.texture_set_data(texture, image) == OK);
		RID native;
		FF_ADAPT_CHECK(storage.texture_get_device_texture(texture, native) == OK);
		FixedFunctionGraphicsDevice::FFTextureInfo info;
		FF_ADAPT_CHECK(p_device.texture_get_info(native, info) == OK && info.size == size);
		Ref<Image> snapshot = storage.texture_get_data(texture);
		FF_ADAPT_CHECK(snapshot->get_width() == size.x && snapshot->get_height() == size.y && TestFixedFunctionTextureAdaptation::_matches_data(snapshot, original));
		FF_ADAPT_CHECK(_draw(p_device, native, repeat, r_pixels));
		const int divisor = repeat ? 8 : 4;
		const int x = p_device.get_width() / divisor, y = p_device.get_height() / divisor;
		const uint32_t samples[] = { r_pixels[size_t(y) * p_device.get_width() + x], r_pixels[size_t(y) * p_device.get_width() + x * 3], r_pixels[size_t(y * 3) * p_device.get_width() + x], r_pixels[size_t(y * 3) * p_device.get_width() + x * 3] };
		const uint32_t expected[] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };
		for (int color = 0; color < 4; color++) {
			FF_ADAPT_CHECK((samples[color] & 0xfff8f8f8) == (expected[color] & 0xfff8f8f8));
		}
		if (repeat) {
			FF_ADAPT_CHECK(r_pixels[size_t(y) * p_device.get_width() + x * 5] == samples[0]);
		}
		Ref<Image> patch = memnew(Image(1, 1, false, Image::FORMAT_RGBA8));
		patch->fill(Color(1, 1, 0));
		FF_ADAPT_CHECK(storage.texture_request_data_partial(texture, patch, Rect2i(0, 0, 1, 1), Point2i(size.x - 1, size.y - 1)) == OK);
		snapshot = storage.texture_get_data(texture);
		snapshot->lock();
		const Color patched = snapshot->get_pixel(size.x - 1, size.y - 1);
		snapshot->unlock();
		FF_ADAPT_CHECK(patched == Color(1, 1, 0) && snapshot->get_width() == size.x && snapshot->get_height() == size.y);
		FF_ADAPT_CHECK(storage.update_pending() == OK);
		FF_ADAPT_CHECK(storage.texture_get_device_texture(texture, native) == OK && _draw(p_device, native, repeat, r_pixels));
		FF_ADAPT_CHECK(image->get_width() == size.x && image->get_height() == size.y && TestFixedFunctionTextureAdaptation::_matches_data(image, original));
		FF_ADAPT_CHECK(storage.free(texture) == OK);
	}
	FF_ADAPT_CHECK(_compressed(storage, p_device, r_pixels));
	FF_ADAPT_CHECK(_targets(p_device, r_pixels));
	printf("PASS: native POT/size-limited uploads, source dimensions/readback, normalized regions/repeat, logical partial writes and immutable CPU images.\n");
	return true;
}

bool TestFixedFunctionTextureAdaptation::_compressed(FixedFunctionTextureStorage &p_storage, FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
	if (!Image::_image_decompress_bc) {
		return true;
	}
	// A 12x4 BC1 image has no authored 16x4 level: POT adaptation must decode it.
	PoolVector<uint8_t> bytes;
	FF_ADAPT_CHECK(bytes.resize(24) == OK);
	{
		PoolVector<uint8_t>::Write write = bytes.write();
		memset(write.ptr(), 0, bytes.size());
		for (int block = 0; block < 3; block++) {
			write[block * 8 + 1] = 0xf8;
		}
	}
	Ref<Image> image = memnew(Image(12, 4, false, Image::FORMAT_DXT1, bytes));
	const RID texture = p_storage.texture_create();
	FF_ADAPT_CHECK(p_storage.texture_set_data(texture, image) == OK);
	RID native;
	FF_ADAPT_CHECK(p_storage.texture_get_device_texture(texture, native) == OK && _draw(p_device, native, false, r_pixels));
	FF_ADAPT_CHECK((r_pixels[r_pixels.size() / 2] & 0xfff8f8f8) == 0xfff80000);
	Ref<Image> snapshot = p_storage.texture_get_data(texture);
	FF_ADAPT_CHECK(snapshot->get_width() == 12 && snapshot->get_height() == 4 && snapshot->get_format() == Image::FORMAT_DXT1);
	FF_ADAPT_CHECK(TestFixedFunctionTextureAdaptation::_matches_data(snapshot, bytes));
	FF_ADAPT_CHECK(image->get_width() == 12 && image->get_format() == Image::FORMAT_DXT1 && TestFixedFunctionTextureAdaptation::_matches_data(image, bytes));
	FF_ADAPT_CHECK(p_storage.free(texture) == OK);
	printf("PASS: compressed source decode/rescale with unchanged logical BC1 image.\n");
	return true;
}

bool TestFixedFunctionTextureAdaptation::_targets(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
	if (!p_device.get_capabilities().render_target_opaque) {
		return true;
	}
	const Size2i size(19, 11);
	RID target;
	FF_ADAPT_CHECK(p_device.texture_create_render_target(size.x, size.y, target) == OK);
	FixedFunctionGraphicsDevice::FFTextureInfo info;
	FF_ADAPT_CHECK(p_device.texture_get_info(target, info) == OK && info.size == size);
	// Cover the target with one triangle: this checks shared scissor edges without
	// depending on legacy software rasterizers' shared triangle-edge fill rules.
	const FixedFunctionVertex vertices[] = {
		{ -1, 1, 0, 0x80ff0000, 0, 0 },
		{ 3, 1, 0, 0x80ff0000, 2, 0 },
		{ -1, -3, 0, 0x80ff0000, 0, 2 },
	};
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.clip_enabled = true;
	state.clip_rect = Rect2i(0, 0, 9, size.y);
	const bool blend = p_device.get_capabilities().blend_mix;
	state.blend_mode = blend ? FixedFunctionGraphicsDevice::BLEND_MODE_MIX : FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
	const bool depth = p_device.get_capabilities().depth_buffer;
	FF_ADAPT_CHECK(p_device.begin_frame(0xffffffff, target, true, depth) == OK);
	FF_ADAPT_CHECK(p_device.clear_color(0xff000000) == OK);
	if (depth) {
		FF_ADAPT_CHECK(p_device.clear_depth(1) == OK);
	}
	FF_ADAPT_CHECK(p_device.draw_triangles(vertices, 3, CameraMatrix(), RID(), state) == OK);
	state.clip_rect = Rect2i(9, 0, size.x - 9, size.y);
	FF_ADAPT_CHECK(p_device.draw_triangles(vertices, 3, CameraMatrix(), RID(), state) == OK);
	FF_ADAPT_CHECK(p_device.end_frame(false) == OK);
	std::vector<uint32_t> pixels(size.x * size.y);
	FF_ADAPT_CHECK(p_device.texture_read_render_target(target, pixels.data(), pixels.size()) == OK);
	const uint32_t expected = blend ? 0xff800000 : 0xffff0000;
	printf("Adapted target blended sample: %08x (expected %08x).\n", pixels[0], expected);
	for (size_t i = 0; i < pixels.size(); i++) {
		for (int shift = 0; shift < 32; shift += 8) {
			// Allow native 16-bit quantization without hiding gaps or double blending.
			if (ABS(int((pixels[i] >> shift) & 255) - int((expected >> shift) & 255)) > 8) {
				printf("Adapted target mismatch at %u,%u: %08x.\n", unsigned(i % size.x), unsigned(i / size.x), pixels[i]);
			}
			FF_ADAPT_CHECK(ABS(int((pixels[i] >> shift) & 255) - int((expected >> shift) & 255)) <= 8);
		}
	}
	// D3D3's existing padded render targets support clamped sampling only.
	FF_ADAPT_CHECK(_draw(p_device, target, false, r_pixels));
	const uint32_t sampled = r_pixels[size_t(p_device.get_height() / 4) * p_device.get_width() + p_device.get_width() / 4];
	for (int shift = 0; shift < 32; shift += 8) {
		FF_ADAPT_CHECK(ABS(int((sampled >> shift) & 255) - int((expected >> shift) & 255)) <= 8);
	}
	FF_ADAPT_CHECK(p_device.texture_free(target) == OK);
	printf("PASS: native NPOT logical targets, constrained backing, adjacent alpha clips, normalized sampling and logical-size readback.\n");
	return true;
}

bool TestFixedFunctionTextureAdaptation::_matches_data(const Ref<Image> &p_image, const PoolVector<uint8_t> &p_data) {
	// Obtain the COW reference before locking shared bytes for comparison.
	const PoolVector<uint8_t> data = p_image->get_data();
	if (data.size() != p_data.size()) {
		return false;
	}
	const PoolVector<uint8_t>::Read actual = data.read();
	const PoolVector<uint8_t>::Read expected = p_data.read();
	return memcmp(actual.ptr(), expected.ptr(), data.size()) == 0;
}

#undef FF_ADAPT_CHECK
