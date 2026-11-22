#ifndef FIXED_FUNCTION_DEFINES_H
#define FIXED_FUNCTION_DEFINES_H

#include <stddef.h>
#include <stdint.h>

// GPU upload format. CPU geometry retains Godot's real_t precision until packing here.
struct FixedFunctionVertex {
	float x;
	float y;
	float z;
	// Colors use 0xAARRGGBB in both vertices and readback buffers.
	uint32_t color;
	float u;
	float v;
};

// Optional two-texture upload: RGB uses vertex.u/v, alpha uses mask_u/v.
// The color texture's alpha and the mask texture's RGB are ignored.
struct FixedFunctionMaskedVertex {
	FixedFunctionVertex vertex;
	float mask_u;
	float mask_v;
};

// Similar to FixedFunctionLightingVertex but without r/g/b/a for baked lighting or unlit meshes.
struct FixedFunctionLitVertex {
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float u;
	float v;
};

// Native fixed-function lighting with packed ambient RGB before the texture coordinates.
struct FixedFunctionLitColorVertex {
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	uint32_t color;
	float u;
	float v;
};

// Unlit float upload data for optional per-pixel lighting. Source geometry,
// normals and colors retain their CPU representation until this boundary.
struct FixedFunctionLightingVertex {
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float u;
	float v;
	float r;
	float g;
	float b;
	float a;
};

// Independent AO coordinates for streamed draws, retaining vertex color and alpha.
struct FixedFunctionTransientOcclusionVertex {
	FixedFunctionLightingVertex lighting;
	float occlusion_u;
	float occlusion_v;
};

// Allocated only for normal-mapped draws. Authored tangents retain their sign;
// the second UV pair belongs to the independent normal channel.
struct FixedFunctionNormalMapVertex {
	FixedFunctionLightingVertex lighting;
	float tx;
	float ty;
	float tz;
	float tw; // Bitangent sign (-1 or +1): bitangent = cross(normal, tangent.xyz) * tw.
	float normal_u;
	float normal_v;
};

// Streamed animated geometry. The complete world-space tangent frame preserves
// nonuniform skin/model transforms; each texture has independently transformed UVs.
struct FixedFunctionTransientNormalMapVertex {
	FixedFunctionLightingVertex lighting;
	float tx;
	float ty;
	float tz;
	float bx;
	float by;
	float bz;
	float normal_u;
	float normal_v;
	float occlusion_u;
	float occlusion_v;
};

#endif // FIXED_FUNCTION_DEFINES_H
