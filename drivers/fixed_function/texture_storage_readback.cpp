#include "drivers/fixed_function/texture_storage.h"

Ref<Image> FixedFunctionTextureStorage::_read_render_target(const FFTexture &p_texture) const {
	RID native;
	if (p_texture.free_pending || !render_target_is_ready(p_texture.render_target->self) || texture_get_device_texture(p_texture.self, native) != OK) {
		return Ref<Image>();
	}
	const Size2i size = p_texture.size;
	if (size.x <= 0 || size.y <= 0) {
		return Ref<Image>();
	}
	const uint64_t count = uint64_t(size.x) * size.y;
	if (count > INT32_MAX / 4) {
		return Ref<Image>();
	}
	Vector<uint32_t> pixels;
	if (pixels.resize(int(count)) != OK || _device.texture_read_render_target(native, pixels.ptrw(), size_t(count)) != OK) {
		return Ref<Image>();
	}
	Vector<uint8_t> rgba;
	if (rgba.resize(int(count) * 4) != OK) {
		return Ref<Image>();
	}
	{
		uint8_t *write = rgba.ptrw();
		const uint32_t *source = pixels.ptr();
		uint8_t *destination = write;
		for (uint64_t i = 0; i < count; i++) {
			const uint32_t pixel = source[i];
			destination[i * 4] = uint8_t(pixel >> 16);
			destination[i * 4 + 1] = uint8_t(pixel >> 8);
			destination[i * 4 + 2] = uint8_t(pixel);
			destination[i * 4 + 3] = uint8_t(pixel >> 24);
		}
	}
	return Image::create_from_data(size.x, size.y, false, Image::FORMAT_RGBA8, rgba);
}
