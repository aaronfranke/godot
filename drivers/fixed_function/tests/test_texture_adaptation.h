#ifndef TEST_FIXED_FUNCTION_TEXTURE_ADAPTATION_H
#define TEST_FIXED_FUNCTION_TEXTURE_ADAPTATION_H

#include "drivers/fixed_function/render_context.h"

#include <vector>

class TestFixedFunctionTextureAdaptation {
	friend class TestFixedFunctionSceneCPU;
	class FFTextureCapsScope {
		FixedFunctionGraphicsDevice::FFCapabilities &_caps;
		FixedFunctionGraphicsDevice::FFCapabilities _saved;

	public:
		explicit FFTextureCapsScope(FixedFunctionGraphicsDevice &p_device);
		~FFTextureCapsScope();
	};
	static bool _draw(FixedFunctionGraphicsDevice &p_device, RID p_texture, bool p_repeat, std::vector<uint32_t> &r_pixels);
	static bool _targets(FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels);
	static bool _compressed(FixedFunctionTextureStorage &p_storage, FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels);
	static bool _matches_data(const Ref<Image> &p_image, const PoolVector<uint8_t> &p_data);

public:
	static bool test(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels);
};

#endif // TEST_FIXED_FUNCTION_TEXTURE_ADAPTATION_H
