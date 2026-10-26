#ifndef FIXED_FUNCTION_SPATIAL_MATERIAL_H
#define FIXED_FUNCTION_SPATIAL_MATERIAL_H

#include "graphics_device.h"
#include "servers/visual/shader_language.h"

// A deliberately bounded symbolic lowering, not a shader interpreter. Recipes
// contain constants and server parameter names, never scene Resource references.
class FixedFunctionSpatialMaterial {
public:
	enum { UV_POINT_COORD = 2 };
	enum FFLightingFlag {
		LIGHTING_UNSHADED = 1,
		LIGHTING_AMBIENT_DISABLED = 2,
		LIGHTING_VERTEX = 4,
		LIGHTING_SHADOWS_DISABLED = 8,
	};
	enum FFTransformFlag {
		TRANSFORM_BILLBOARD = 1,
		TRANSFORM_KEEP_SCALE = 2,
		TRANSFORM_FIXED_SIZE = 4,
	};
	struct FFUVTransform {
		Vector2 scale;
		Vector2 offset;
		StringName scale_parameter;
		StringName offset_parameter;
		int set;
		FFUVTransform();
	};
	// Shader-shared channel instructions, resolved against each material's parameters.
	struct FFChannelRecipe {
		FFUVTransform uv;
		Color factor;
		StringName factor_parameter;
		StringName texture_parameter;
		bool texture_black_default;
		FFChannelRecipe();
	};
	struct FFNormalMapRecipe {
		FFUVTransform uv;
		StringName texture_parameter;
		StringName strength_parameter;
		float strength;
		FFNormalMapRecipe();
	};
	struct FFEmissionRecipe {
		FFChannelRecipe color;
		StringName energy_parameter;
		float energy;
		bool additive;
		FFEmissionRecipe();
	};
	struct FFOcclusionRecipe {
		FFUVTransform uv;
		Color weights;
		StringName texture_parameter;
		StringName weights_parameter;
		StringName light_affect_parameter;
		float light_affect;
		FFOcclusionRecipe();
	};
	struct FFShaderRecipe {
		FFChannelRecipe albedo;
		FFNormalMapRecipe normal;
		FFEmissionRecipe emission;
		FFOcclusionRecipe occlusion;
		StringName alpha_cutoff_parameter;
		StringName point_size_parameter;
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		float point_size;
		bool albedo_uses_vertex_color;
		bool albedo_uses_vertex_alpha;
		uint8_t lighting_flags;
		uint8_t transform_flags;
		FFShaderRecipe();
	};
	// A resolved logical channel, independent of native texture stages and passes.
	struct FFMaterialChannel {
		Vector2 uv_scale;
		Vector2 uv_offset;
		Color factor;
		RID texture;
		int uv_set;
		FFMaterialChannel();
	};
	struct FFNormalMapChannel {
		Vector2 uv_scale;
		Vector2 uv_offset;
		RID texture;
		int uv_set;
		float strength;
		FFNormalMapChannel();
	};
	struct FFEmissionChannel {
		FFMaterialChannel color;
		float energy;
		bool additive;
		FFEmissionChannel();
	};
	struct FFOcclusionChannel {
		Vector2 uv_scale;
		Vector2 uv_offset;
		Color weights;
		RID texture;
		int uv_set;
		float light_affect;
		FFOcclusionChannel();
	};
	struct FFMaterialParameters {
		FFMaterialChannel albedo;
		FFNormalMapChannel normal;
		FFEmissionChannel emission;
		FFOcclusionChannel occlusion;
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		float point_size;
		bool albedo_uses_vertex_color;
		bool albedo_uses_vertex_alpha;
		bool is_fallback;
		uint8_t lighting_flags;
		uint8_t transform_flags;
		FFMaterialParameters();
	};

private:
	class FFShaderPattern;
	enum { MAX_EXPRESSION_DEPTH = 64 };
	enum FFValueType : uint8_t {
		VALUE_NONE,
		VALUE_UV,
		VALUE_COLOR,
		VALUE_ALPHA,
		VALUE_SCALAR,
		VALUE_VECTOR,
		VALUE_SAMPLER,
		VALUE_TEXTURE_SCALAR,
	};
	struct FFValue {
		FFChannelRecipe color;
		FFUVTransform uv;
		Color vector;
		StringName parameter;
		FFValueType type;
		uint8_t components;
		bool uses_vertex_color;
		FFValue();
	};
	const ShaderLanguage::ShaderNode &_shader;
	Map<StringName, FFValue> _locals;
	FFValue _uv[2];
	FFValue _albedo;
	FFValue _alpha;
	FFValue _alpha_cutoff;
	FFValue _point_size;
	FFValue _normal;
	FFValue _normal_depth;
	FFEmissionRecipe _emission;
	FFValue _occlusion;
	FFValue _occlusion_light_affect;
	bool _alpha_written;
	bool _alpha_cutoff_written;
	bool _albedo_written;
	uint8_t _transform_flags;
	bool _lower_transform(const ShaderLanguage::Node *p_node);
	bool _is_albedo_srgb_conversion(const ShaderLanguage::Node *p_node) const;
	Error _expression(const ShaderLanguage::Node *p_node, FFValue &r_value, int p_depth = 0) const;
	bool _lower_emission(const ShaderLanguage::Node *p_node, FFEmissionRecipe &r_emission) const;
	bool _is_ignored_pbr_value(const ShaderLanguage::Node *p_node) const;
	static bool _is_texture_sample(const FFValue &p_value);
	Error _block(const ShaderLanguage::BlockNode &p_block, bool p_vertex);
	static bool _variable(const ShaderLanguage::Node *p_node, const StringName &p_name);
	static bool _constant_srgb_condition(const ShaderLanguage::Node *p_node);
	static Error _multiply(const FFValue &p_left, const FFValue &p_right, FFValue &r_value);
	static bool _same_texture_mapping(const FFChannelRecipe &p_left, const FFChannelRecipe &p_right);
	FixedFunctionSpatialMaterial(const ShaderLanguage::ShaderNode &p_shader);

public:
	// The initial native profile follows GLES2's OUTPUT_IS_SRGB=true LDR semantics.
	static Error lower(const ShaderLanguage::ShaderNode &p_shader, FFShaderRecipe &r_recipe);
	// Recover a recognizable albedo prefix; unknown shader effects become opaque unlit color.
	static void lower_fallback(const ShaderLanguage::ShaderNode *p_shader, FFShaderRecipe &r_recipe);
};

#endif // FIXED_FUNCTION_SPATIAL_MATERIAL_H
