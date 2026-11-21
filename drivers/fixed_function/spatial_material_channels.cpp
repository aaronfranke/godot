#include "drivers/fixed_function/spatial_material.h"

bool FixedFunctionSpatialMaterial::_is_ignored_pbr_value(const ShaderLanguage::Node *p_node) const {
	FFValue value;
	if (_expression(p_node, value) == OK) {
		return value.type == VALUE_SCALAR || value.type == VALUE_LIGHTING_SCALAR || value.type == VALUE_TEXTURE_SCALAR || value.type == VALUE_ALPHA;
	}
	if (p_node->type != ShaderLanguage::Node::NODE_TYPE_OPERATOR) {
		return false;
	}
	const ShaderLanguage::OperatorNode *op = static_cast<const ShaderLanguage::OperatorNode *>(p_node);
	if (op->op != ShaderLanguage::OP_MUL || op->arguments.size() != 2) {
		return false;
	}
	for (int i = 0; i < 2; i++) {
		if (_expression(op->arguments[i], value) != OK || (value.type != VALUE_SCALAR && value.type != VALUE_TEXTURE_SCALAR && value.type != VALUE_ALPHA)) {
			return false;
		}
	}
	return true;
}

bool FixedFunctionSpatialMaterial::_lower_emission(const ShaderLanguage::Node *p_node, FFEmissionRecipe &r_emission) const {
	FFEmissionRecipe emission;
	emission.energy = 1;
	if (p_node->type == ShaderLanguage::Node::NODE_TYPE_OPERATOR) {
		const ShaderLanguage::OperatorNode *op = static_cast<const ShaderLanguage::OperatorNode *>(p_node);
		if (op->op == ShaderLanguage::OP_MUL && op->arguments.size() == 2) {
			for (int i = 0; i < 2; i++) {
				FFValue energy;
				if (_expression(op->arguments[i], energy) == OK && energy.type == VALUE_SCALAR) {
					emission.energy = energy.vector.r;
					emission.energy_parameter = energy.parameter;
					p_node = op->arguments[1 - i];
					break;
				}
			}
		}
	}
	if (p_node->type == ShaderLanguage::Node::NODE_TYPE_OPERATOR) {
		const ShaderLanguage::OperatorNode *op = static_cast<const ShaderLanguage::OperatorNode *>(p_node);
		if (op->op == ShaderLanguage::OP_ADD && op->arguments.size() == 2) {
			FFValue factor, sample;
			if (_expression(op->arguments[0], factor) != OK || _expression(op->arguments[1], sample) != OK) {
				return false;
			}
			if (_is_texture_sample(factor)) {
				SWAP(factor, sample);
			}
			if (!_is_texture_sample(sample) || factor.type != VALUE_COLOR || factor.color.texture_parameter != StringName() || factor.uses_vertex_color) {
				return false;
			}
			emission.color = factor.color;
			emission.color.uv = sample.color.uv;
			emission.color.texture_parameter = sample.color.texture_parameter;
			emission.color.texture_black_default = sample.color.texture_black_default;
			emission.color.sampler_flags = sample.color.sampler_flags;
			emission.additive = true;
		}
	}
	if (!emission.additive) {
		FFValue value;
		if (_expression(p_node, value) != OK || value.type != VALUE_COLOR || value.uses_vertex_color) {
			return false;
		}
		emission.color = value.color;
	}
	r_emission = emission;
	return true;
}
