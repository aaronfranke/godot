// Engine-provided Lambert shaders. CPU source data and scene ownership stay shared.
#ifndef LIGHT_COUNT
#define LIGHT_COUNT 8
#endif

struct VertexInput {
	float3 position : POSITION0;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
#if defined(OCCLUSION_MAP) && !defined(NORMAL_MAP) && defined(GEOMETRY_SHADER)
	float4 color_or_uv : TEXCOORD1;
#else
	float4 color : COLOR0;
#endif
#if defined(OCCLUSION_MAP) && !defined(NORMAL_MAP) && !defined(GEOMETRY_SHADER)
	float2 occlusion_uv : TEXCOORD1;
#endif
#ifdef NORMAL_MAP
#ifdef GEOMETRY_SHADER
	float4 tangent : TANGENT0;
#else
	float3 tangent : TANGENT0;
	float3 binormal : BINORMAL0;
#endif
	float2 normal_uv : TEXCOORD1;
#ifndef GEOMETRY_SHADER
	float2 occlusion_uv : TEXCOORD2;
#endif
#endif
};

struct VertexOutput {
	float4 position : POSITION0;
	float3 normal : TEXCOORD0;
	float4 world_position : TEXCOORD1; // Camera-relative position and unclamped fog amount.
	float2 uv : TEXCOORD2;
	float4 color : COLOR0;
#ifdef NORMAL_MAP
	float3 tangent : TEXCOORD3;
	float3 binormal : TEXCOORD4;
	float2 normal_uv : TEXCOORD5;
#endif
#ifdef OCCLUSION_MAP
	float2 occlusion_uv : TEXCOORD6;
#endif
};

#ifdef VERTEX_SHADER
float4 projection[4] : register(c0);
float4 fog_depth : register(c16); // Scale for clip Z (orthographic) or W (perspective), then bias.
#ifdef GEOMETRY_SHADER
float4 world_from_model[4] : register(c4);
float4 normal_from_model[3] : register(c8);
float4 material_color : register(c11);
float4 albedo_uv_transform : register(c12);
#ifdef NORMAL_MAP
float4 normal_uv_transform : register(c13);
#endif
#if defined(NORMAL_MAP) || defined(OCCLUSION_MAP)
float4 uv_sources : register(c14); // Select the secondary UV for albedo (x) or occlusion (y).
#endif
#ifdef OCCLUSION_MAP
float4 occlusion_uv_transform : register(c15);
#endif
#else
float4 camera_basis[3] : register(c4);
#endif

VertexOutput vertex_main(VertexInput input) {
	VertexOutput output;
	output.position = projection[0] * input.position.x + projection[1] * input.position.y + projection[2] * input.position.z + projection[3];
	output.world_position.w = dot(output.position.zw, fog_depth.xy) + fog_depth.z;
#ifdef GEOMETRY_SHADER
	float3 normal = normal_from_model[0].xyz * input.normal.x + normal_from_model[1].xyz * input.normal.y + normal_from_model[2].xyz * input.normal.z;
	output.normal = normal * rsqrt(max(dot(normal, normal), 1.0e-30));
	output.world_position.xyz = world_from_model[0].xyz * input.position.x + world_from_model[1].xyz * input.position.y + world_from_model[2].xyz * input.position.z + world_from_model[3].xyz;
#if defined(OCCLUSION_MAP) && !defined(NORMAL_MAP)
	output.color = uv_sources.y != 0 ? material_color : input.color_or_uv * material_color;
#else
	output.color = input.color * material_color;
#endif
	float2 albedo_uv = input.uv;
#ifdef NORMAL_MAP
	if (uv_sources.x != 0) {
		albedo_uv = input.normal_uv;
	}
#endif
	output.uv = albedo_uv * albedo_uv_transform.xy + albedo_uv_transform.zw;
#else
	output.normal = input.normal;
	output.world_position.xyz = camera_basis[0].xyz * input.position.x + camera_basis[1].xyz * input.position.y + camera_basis[2].xyz * input.position.z;
	output.color = input.color;
	output.uv = input.uv;
#endif
#ifdef NORMAL_MAP
#ifdef GEOMETRY_SHADER
	// Match Godot's authored tangent/binormal convention, including reflected and
	// nonuniform model transforms. Transform tangents as vectors, not covectors.
	float3 binormal = cross(input.normal, input.tangent.xyz) * input.tangent.w;
	float3 tangent_world = world_from_model[0].xyz * input.tangent.x + world_from_model[1].xyz * input.tangent.y + world_from_model[2].xyz * input.tangent.z;
	float3 binormal_world = world_from_model[0].xyz * binormal.x + world_from_model[1].xyz * binormal.y + world_from_model[2].xyz * binormal.z;
	output.tangent = tangent_world * rsqrt(max(dot(tangent_world, tangent_world), 1.0e-30));
	output.binormal = binormal_world * rsqrt(max(dot(binormal_world, binormal_world), 1.0e-30));
	output.normal_uv = input.normal_uv * normal_uv_transform.xy + normal_uv_transform.zw;
#else
	// The CPU deforms the frame and each channel's UVs; lighting stays per pixel.
	output.tangent = input.tangent;
	output.binormal = input.binormal;
	output.normal_uv = input.normal_uv;
#endif
#endif
#ifdef OCCLUSION_MAP
#ifdef GEOMETRY_SHADER
#ifdef NORMAL_MAP
	float2 occlusion_uv = uv_sources.y != 0 ? input.normal_uv : input.uv;
#else
	float2 occlusion_uv = uv_sources.y != 0 ? input.color_or_uv.xy : input.uv;
#endif
	output.occlusion_uv = occlusion_uv * occlusion_uv_transform.xy + occlusion_uv_transform.zw;
#else
	output.occlusion_uv = input.occlusion_uv;
#endif
#endif
	return output;
}
#else
sampler2D albedo_texture : register(s0);
float4 ambient : register(c0);
float4 fog_color : register(c43);
#ifdef NORMAL_MAP
sampler2D normal_texture : register(s1);
float4 normal_settings : register(c34); // Strength; normal maps are linear data.
#endif
#ifdef OCCLUSION_MAP
sampler2D occlusion_texture : register(s2);
float4 occlusion_weights : register(c35);
#endif
float4 settings : register(c1); // Unused, texture alpha, active light count, AO direct-light affect.
struct Light {
	float4 color_type; // Signed radiance, directional=0 / omni=1 / spot=2.
	float4 position_range; // Camera-relative world position, inverse range.
	float4 direction_attenuation; // Toward directional light / emitted spot direction, decay.
	float4 cone; // Cosine cutoff, angular decay, unused, unused.
};
Light lights[LIGHT_COUNT] : register(c2);

#ifdef DIRECTIONAL_SHADOW
sampler2D shadow_texture : register(s3);
float4 shadow_projection[4] : register(c36);
float4 shadow_settings : register(c40); // Light index, normalized depth and normal-slope biases.
float4 shadow_color : register(c41);
float4 shadow_direction : register(c42);

float3 directional_shadow(VertexOutput input) {
	float3 normal = input.normal * rsqrt(max(dot(input.normal, input.normal), 1.0e-30));
	float3 position = input.world_position.xyz;
	float4 clip_position = shadow_projection[0] * position.x + shadow_projection[1] * position.y + shadow_projection[2] * position.z + shadow_projection[3];
	float3 projected = clip_position.xyz / clip_position.w;
	float2 uv = projected.xy * float2(0.5, -0.5) + 0.5;
	float depth = projected.z * 0.5 + 0.5;
	// Shadow targets have one level; explicit LOD avoids derivative flow-control restrictions.
	float stored = dot(tex2Dlod(shadow_texture, float4(uv, 0, 0)).rgb, float3(1, 1.0 / 255, 1.0 / 65025));
	bool inside = clip_position.w > 0 && all(uv >= 0) && all(uv <= 1) && depth >= 0 && depth <= 1;
	// Match 3.x's light-view depth bias: more normal bias on grazing surfaces.
	float bias = shadow_settings.y + (1 - abs(dot(normal, shadow_direction.xyz))) * shadow_settings.z;
	return inside && depth - bias > stored ? shadow_color.rgb : float3(1, 1, 1);
}
#endif

float safe_power(float base, float exponent) {
	float value = pow(max(base, 1.0e-10), exponent);
	float zero_value = exponent == 0 ? 1 : 0;
	return base > 0 ? value : zero_value;
}

float4 pixel_main(VertexOutput input) : COLOR0 {
	float3 normal = input.normal * rsqrt(max(dot(input.normal, input.normal), 1.0e-30));
#ifdef DIRECTIONAL_SHADOW
	// One sample per fragment, independent of the number of active light slots.
	float3 shadow = directional_shadow(input);
#endif
#ifdef NORMAL_MAP
	// Godot reconstructs positive Z from RG; the stored blue channel is ignored.
	float2 xy = tex2D(normal_texture, input.normal_uv).rg * 2 - 1;
	float z = sqrt(max(0, 1 - dot(xy, xy)));
	float3 mapped = input.tangent * xy.x + input.binormal * xy.y + input.normal * z;
	normal = lerp(input.normal, mapped, normal_settings.x);
	normal *= rsqrt(max(dot(normal, normal), 1.0e-30));
#endif
#ifdef OCCLUSION_MAP
	float3 illumination = 0;
#else
	float3 illumination = ambient.rgb;
#endif
#ifdef DIRECTIONAL_SHADOW
	// Keep even normal+AO+shadow below SM3's minimum static instruction budget.
	// Loop only over active slots; the unchanged unshadowed variants stay unrolled.
	[loop]
	for (int i = 0; i < settings.z; i++) {
#else
	[unroll]
	for (int i = 0; i < LIGHT_COUNT; i++) {
#endif
		// The condition is constant for the draw, so unused slots do not evaluate lighting.
		[branch]
		if (i < settings.z) {
			float3 delta = lights[i].position_range.xyz - input.world_position.xyz;
			float distance_squared = dot(delta, delta);
			float distance = sqrt(max(distance_squared, 1.0e-30));
			float3 toward = delta / distance;
			float normalized_distance = distance * lights[i].position_range.w;
			float weight = safe_power(saturate(1 - normalized_distance), lights[i].direction_attenuation.w);
			weight *= normalized_distance < 1 && distance_squared > 0 ? 1 : 0;
			float angle = dot(-toward, lights[i].direction_attenuation.xyz);
			float rim = saturate((1 - angle) / max(1 - lights[i].cone.x, 1.0e-6));
			float cone_weight = 1 - safe_power(rim, lights[i].cone.y);
			cone_weight *= angle > lights[i].cone.x ? 1 : 0;
			weight *= lights[i].color_type.w > 1.5 ? cone_weight : 1;
			if (lights[i].color_type.w < 0.5) {
				toward = lights[i].direction_attenuation.xyz;
				weight = 1;
			}
			float3 contribution = lights[i].color_type.rgb * (max(dot(normal, toward), 0) * weight);
#ifdef DIRECTIONAL_SHADOW
			contribution *= i == shadow_settings.x ? shadow : float3(1, 1, 1);
#endif
			illumination += contribution;
		}
	}
#ifdef OCCLUSION_MAP
	float ao = saturate(dot(tex2D(occlusion_texture, input.occlusion_uv), occlusion_weights));
	illumination = ambient.rgb * ao + illumination * lerp(1, ao, settings.w);
#endif
	float4 color = float4(saturate(input.color.rgb * illumination), saturate(input.color.a));
#ifdef TEXTURED
	float4 texel = tex2D(albedo_texture, input.uv);
	color.rgb *= texel.rgb;
	color.a *= lerp(1, texel.a, settings.y);
#endif
	color.rgb = lerp(color.rgb, fog_color.rgb, saturate(input.world_position.w));
	return color;
}
#endif
