#include "drivers/opengl1/graphics_device_opengl1.h"
#include "drivers/opengl1/opengl1_headers.h"

Error FixedFunctionGraphicsDeviceOpenGL1::_read_pixels(const Size2i &p_size, uint32_t *r_pixels, size_t p_pixel_count, bool p_flip_y, bool p_preserve_alpha) {
	const uint64_t count = uint64_t(p_size.x) * p_size.y;
	if (!r_pixels || count > p_pixel_count || count > INT32_MAX / 4) {
		return ERR_INVALID_PARAMETER;
	}
	Vector<uint8_t> rgba;
	const Error resized = rgba.resize(int(count) * 4);
	if (resized != OK) {
		return resized;
	}
	uint8_t *source = rgba.ptrw();
	glReadPixels(0, 0, p_size.x, p_size.y, GL_RGBA, GL_UNSIGNED_BYTE, source);
	const Error read = _check_gl_error();
	if (read != OK) {
		return read;
	}
	// FBO drawing already flips the projection; only window rows need inversion.
	for (int y = 0; y < p_size.y; y++) {
		const int source_y = p_flip_y ? p_size.y - 1 - y : y;
		const uint8_t *row = source + size_t(source_y) * p_size.x * 4;
		for (int x = 0; x < p_size.x; x++, row += 4) {
			const uint32_t alpha = p_preserve_alpha ? row[3] : 255;
			*r_pixels++ = (alpha << 24) | (uint32_t(row[0]) << 16) | (uint32_t(row[1]) << 8) | row[2];
		}
	}
	return OK;
}
