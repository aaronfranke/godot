#pragma once

#include <stdint.h>

class FixedFunctionShadersD3D9 {
public:
	static const uint32_t VERTEX[];
	static const uint32_t VERTEX_NORMAL_MAP[];
	static const uint32_t VERTEX_TRANSIENT_NORMAL_MAP[];
	static const uint32_t VERTEX_TRANSIENT_NORMAL_OCCLUSION[];
	static const uint32_t PIXEL_NORMAL_MAP[];
	static const uint32_t PIXEL_TEXTURED_NORMAL_MAP[];
	static const uint32_t VERTEX_GEOMETRY[];
	static const uint32_t PIXEL[];
	static const uint32_t PIXEL_TEXTURED[];
	static const uint32_t VERTEX_NORMAL_OCCLUSION[];
	static const uint32_t PIXEL_NORMAL_OCCLUSION[];
	static const uint32_t PIXEL_TEXTURED_NORMAL_OCCLUSION[];
	static const uint32_t VERTEX_TRANSIENT_OCCLUSION[];
	static const uint32_t VERTEX_OCCLUSION[];
	static const uint32_t PIXEL_OCCLUSION[];
	static const uint32_t PIXEL_TEXTURED_OCCLUSION[];
	static const uint32_t SHADOW_VERTEX[];
	static const uint32_t SHADOW_PIXEL[];
	static const uint32_t SHADOW_PIXEL_TEXTURED[];
	static const uint32_t PIXEL_SHADOW[];
	static const uint32_t PIXEL_TEXTURED_SHADOW[];
	static const uint32_t PIXEL_NORMAL_SHADOW[];
	static const uint32_t PIXEL_TEXTURED_NORMAL_SHADOW[];
	static const uint32_t PIXEL_OCCLUSION_SHADOW[];
	static const uint32_t PIXEL_TEXTURED_OCCLUSION_SHADOW[];
	static const uint32_t PIXEL_NORMAL_OCCLUSION_SHADOW[];
	static const uint32_t PIXEL_TEXTURED_NORMAL_OCCLUSION_SHADOW[];
};
