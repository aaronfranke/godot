// Depth is encoded into ordinary RGB8, avoiding vendor-specific depth textures.
struct ShadowVertex {
	float3 position : POSITION0;
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
};

struct ShadowFragment {
	float4 position : POSITION0;
	float2 depth : TEXCOORD0;
	float2 uv : TEXCOORD1;
	float4 color : COLOR0;
};

#ifdef VERTEX_SHADER
float4 projection[4] : register(c0);

ShadowFragment vertex_main(ShadowVertex input) {
	ShadowFragment output;
	output.position = projection[0] * input.position.x + projection[1] * input.position.y + projection[2] * input.position.z + projection[3];
	output.depth = output.position.zw;
	output.uv = input.uv;
	output.color = input.color;
	return output;
}
#else
sampler2D albedo_texture : register(s0);
float4 settings : register(c0); // Whether albedo texture alpha participates in coverage.

float4 pixel_main(ShadowFragment input) : COLOR0 {
	// Keep the far endpoint below one so frac cannot wrap it back to near depth.
	float depth = clamp(input.depth.x / input.depth.y, 0, 1 - 1.0 / 16777216);
	float3 encoded = frac(depth * float3(1, 255, 65025));
	encoded.xy -= encoded.yz / 255;
	float alpha = input.color.a;
#ifdef TEXTURED
	alpha *= lerp(1, tex2D(albedo_texture, input.uv).a, settings.x);
#endif
	// Native alpha testing still applies the material's cutoff to this output.
	return float4(encoded, alpha);
}
#endif
