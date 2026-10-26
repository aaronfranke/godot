#ifndef TEST_FIXED_FUNCTION_OPENGL1_MIPMAPS_H
#define TEST_FIXED_FUNCTION_OPENGL1_MIPMAPS_H

#include "drivers/fixed_function/render_context.h"
#include "drivers/opengl1/graphics_device_opengl1.h"

#include <vector>

class TestFixedFunctionOpenGL1Mipmaps {
	static Ref<Image> _authored_image(Image::Format p_format);
	static bool _draw(FixedFunctionGraphicsDeviceOpenGL1 &p_device, RID p_texture, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, float p_span, std::vector<uint32_t> &r_pixels);
	static bool _pixel(const std::vector<uint32_t> &p_pixels, uint32_t p_expected);
	static bool _levels(FixedFunctionGraphicsDeviceOpenGL1 &p_device, RID p_texture);
	static bool _authored(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDeviceOpenGL1 &p_device, Image::Format p_format, std::vector<uint32_t> &r_pixels);
	static bool _adapted(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDeviceOpenGL1 &p_device, std::vector<uint32_t> &r_pixels);
	static bool _fallback(FixedFunctionGraphicsDeviceOpenGL1 &p_device);
	static bool _compressed_alpha(FixedFunctionGraphicsDeviceOpenGL1 &p_device, std::vector<uint32_t> &r_pixels);

public:
	static bool test(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDeviceOpenGL1 &p_device, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_OPENGL1_MIPMAPS_H
