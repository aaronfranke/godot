#pragma once

#include "core/templates/rb_map.h"
#include "core/templates/rb_set.h"
#include "servers/rendering/rendering_server_enums.h"
#include "servers/rendering/shader_language.h"

class FixedFunctionShaderUsage {
public:
	struct FFFeatures {
		RSE::CullMode cull_mode;
		bool animated;
		bool casts_shadows;
		bool uses_tangents;
		bool ensure_correct_normals;

		FFFeatures();
	};

private:
	enum FFUsageFlags : uint8_t {
		USES_TIME = 1,
		USES_ALPHA = 2,
		USES_SCISSOR = 4,
		WRITES_VERTEX = 8,
		USES_TANGENTS = 16,
		USES_DISCARD = 32,
	};
	struct FFVisit {
		const ShaderLanguage::Node *node;
		bool assigning;
	};
	static void _push(Vector<FFVisit> &r_pending, const ShaderLanguage::Node *p_node, bool p_assigning = false);
	static uint32_t _variable(const StringName &p_name, bool p_local, bool p_assigning);
	static const ShaderLanguage::ShaderNode::Function *_find_function(const ShaderLanguage::ShaderNode &p_shader, const StringName &p_name);
	static uint32_t _function_usage(const ShaderLanguage::ShaderNode &p_shader, const ShaderLanguage::FunctionNode &p_function);
	static uint32_t _stage_usage(const ShaderLanguage::ShaderNode &p_shader, const RBMap<StringName, uint32_t> &p_usage, const StringName &p_stage);

public:
	// CPU metadata only. Reachable helper calls and out/inout vertex writes are included.
	static FFFeatures analyze_spatial(const ShaderLanguage::ShaderNode &p_shader);
};
