#include "drivers/fixed_function/spatial_material.h"

// Match the expressions emitted by BaseMaterial3D. Check their operands so
// other shader expressions still go through the ordinary fallback.
class FixedFunctionSpatialMaterial::FFShaderPattern {
	typedef ShaderLanguage SL;

	static const SL::OperatorNode *_operation(const SL::Node *p_node, SL::Operator p_op, int p_arguments) {
		if (!p_node || p_node->type != SL::Node::NODE_TYPE_OPERATOR) {
			return nullptr;
		}
		const SL::OperatorNode *op = static_cast<const SL::OperatorNode *>(p_node);
		return op->op == p_op && op->arguments.size() == p_arguments ? op : nullptr;
	}

	static bool _number(const SL::Node *p_node, float p_value) {
		if (!p_node || p_node->type != SL::Node::NODE_TYPE_CONSTANT) {
			return false;
		}
		const SL::ConstantNode *constant = static_cast<const SL::ConstantNode *>(p_node);
		if (constant->values.size() != 1) {
			return false;
		}
		if (constant->datatype == SL::TYPE_INT) {
			return constant->values[0].sint == p_value;
		}
		return constant->datatype == SL::TYPE_FLOAT && constant->values[0].real == p_value;
	}

	static const SL::Node *_index(const SL::Node *p_node, int p_index) {
		const SL::OperatorNode *op = _operation(p_node, SL::OP_INDEX, 2);
		return op && _number(op->arguments[1], p_index) ? op->arguments[0] : nullptr;
	}

	static const SL::Node *_member(const SL::Node *p_node, const StringName &p_name) {
		if (!p_node || p_node->type != SL::Node::NODE_TYPE_MEMBER) {
			return nullptr;
		}
		const SL::MemberNode *member = static_cast<const SL::MemberNode *>(p_node);
		return member->name == p_name && !member->index_expression && !member->assign_expression && !member->call_expression ? member->owner : nullptr;
	}

	static bool _vec3_number(const SL::Node *p_node, float p_value) {
		if (p_node->type != SL::Node::NODE_TYPE_CONSTANT) {
			return false;
		}
		const SL::ConstantNode *constant = static_cast<const SL::ConstantNode *>(p_node);
		return constant->datatype == SL::TYPE_VEC3 && constant->values.size() == 3 &&
				constant->values[0].real == p_value && constant->values[1].real == p_value && constant->values[2].real == p_value;
	}

	static bool _albedo_rgb(const SL::Node *p_node) {
		return _variable(_member(p_node, "rgb"), "albedo_tex");
	}

	static const SL::VariableDeclarationNode::Declaration *_float_declaration(const SL::Node *p_node, const StringName &p_name) {
		if (p_node->type != SL::Node::NODE_TYPE_VARIABLE_DECLARATION) {
			return nullptr;
		}
		const SL::VariableDeclarationNode *decl = static_cast<const SL::VariableDeclarationNode *>(p_node);
		return decl->datatype == SL::TYPE_FLOAT && decl->declarations.size() == 1 && decl->declarations[0].name == p_name && !decl->declarations[0].size ? &decl->declarations[0] : nullptr;
	}

	static const SL::Node *_initializer(const SL::Node *p_node, const StringName &p_name) {
		const SL::VariableDeclarationNode::Declaration *decl = _float_declaration(p_node, p_name);
		return decl && decl->initializer.size() == 1 ? decl->initializer[0] : nullptr;
	}

	static const SL::BlockNode *_branch_body(const SL::BlockNode &p_branch) {
		// The parser wraps the braced branch in an outer block.
		if (p_branch.statements.size() != 1 || p_branch.statements.front()->get()->type != SL::Node::NODE_TYPE_BLOCK) {
			return nullptr;
		}
		return static_cast<const SL::BlockNode *>(p_branch.statements.front()->get());
	}

	static const SL::ControlFlowNode *_if_else(const SL::Node *p_node) {
		if (p_node->type != SL::Node::NODE_TYPE_CONTROL_FLOW) {
			return nullptr;
		}
		const SL::ControlFlowNode *flow = static_cast<const SL::ControlFlowNode *>(p_node);
		return flow->flow_op == SL::FLOW_OP_IF && flow->expressions.size() == 1 && flow->blocks.size() == 2 ? flow : nullptr;
	}

	static const SL::Node *_scale_assignment(const SL::BlockNode &p_branch) {
		const SL::BlockNode *body = _branch_body(p_branch);
		if (!body || body->statements.size() != 1) {
			return nullptr;
		}
		const SL::OperatorNode *assign = _operation(body->statements.front()->get(), SL::OP_ASSIGN, 2);
		return assign && _variable(assign->arguments[0], "sc") ? assign->arguments[1] : nullptr;
	}

	static bool _perspective_scale(const SL::Node *p_node) {
		const SL::ControlFlowNode *flow = _if_else(p_node);
		if (!flow || !_variable(flow->expressions[0], "IS_MULTIVIEW")) {
			return false;
		}
		const SL::OperatorNode *length = _operation(_scale_assignment(*flow->blocks[0]), SL::OP_CALL, 2);
		const SL::OperatorNode *negate = _operation(_scale_assignment(*flow->blocks[1]), SL::OP_NEGATE, 1);
		return length && _variable(length->arguments[0], "length") && _variable(_index(_member(length->arguments[1], "xyz"), 3), "MODELVIEW_MATRIX") &&
				negate && _variable(_index(_member(negate->arguments[0], "z"), 3), "MODELVIEW_MATRIX");
	}

	static bool _fixed_size_branch(const SL::BlockNode &p_branch, bool p_orthogonal) {
		const SL::BlockNode *block = _branch_body(p_branch);
		if (!block || block->statements.size() != 5) {
			return false;
		}
		const List<SL::Node *>::Element *statement = block->statements.front();
		if (p_orthogonal) {
			// float h = abs(1.0 / (2.0 * PROJECTION_MATRIX[1][1]));
			const SL::OperatorNode *abs_call = _operation(_initializer(statement->get(), "h"), SL::OP_CALL, 2);
			const SL::OperatorNode *divide = abs_call && _variable(abs_call->arguments[0], "abs") ? _operation(abs_call->arguments[1], SL::OP_DIV, 2) : nullptr;
			const SL::OperatorNode *multiply = divide && _number(divide->arguments[0], 1) ? _operation(divide->arguments[1], SL::OP_MUL, 2) : nullptr;
			if (!multiply || !_number(multiply->arguments[0], 2) || !_variable(_index(_index(multiply->arguments[1], 1), 1), "PROJECTION_MATRIX")) {
				return false;
			}
			statement = statement->next();
		}
		if (p_orthogonal) {
			const SL::OperatorNode *multiply = _operation(_initializer(statement->get(), "sc"), SL::OP_MUL, 2);
			if (!multiply || !_variable(multiply->arguments[0], "h") || !_number(multiply->arguments[1], 2)) {
				return false;
			}
		} else {
			const SL::VariableDeclarationNode::Declaration *scale = _float_declaration(statement->get(), "sc");
			if (!scale || !scale->initializer.is_empty()) {
				return false;
			}
			statement = statement->next();
			if (!_perspective_scale(statement->get())) {
				return false;
			}
		}
		for (int column = 0; column < 3; column++) {
			statement = statement->next();
			const SL::OperatorNode *assign = _operation(statement->get(), SL::OP_ASSIGN_MUL, 2);
			if (!assign || !_variable(_index(assign->arguments[0], column), "MODELVIEW_MATRIX") || !_variable(assign->arguments[1], "sc")) {
				return false;
			}
		}
		return true;
	}

	static const SL::OperatorNode *_modelview_assignment(const SL::Node *p_node, const StringName &p_left) {
		const SL::OperatorNode *assign = _operation(p_node, SL::OP_ASSIGN, 2);
		if (!assign || !_variable(assign->arguments[0], "MODELVIEW_MATRIX")) {
			return nullptr;
		}
		const SL::OperatorNode *multiply = _operation(assign->arguments[1], SL::OP_MUL, 2);
		if (!multiply || !_variable(multiply->arguments[0], p_left)) {
			return nullptr;
		}
		const SL::OperatorNode *matrix = _operation(multiply->arguments[1], SL::OP_CONSTRUCT, 5);
		return matrix && _variable(matrix->arguments[0], "mat4") ? matrix : nullptr;
	}

public:
	static bool albedo_srgb_conversion(const SL::Node *p_node) {
		const SL::OperatorNode *assignment = _operation(p_node, SL::OP_ASSIGN, 2);
		if (!assignment || !_albedo_rgb(assignment->arguments[0])) {
			return false;
		}
		const SL::OperatorNode *mix = _operation(assignment->arguments[1], SL::OP_CALL, 4);
		if (!mix || !_variable(mix->arguments[0], "mix")) {
			return false;
		}
		const SL::OperatorNode *power = _operation(mix->arguments[1], SL::OP_CALL, 3);
		const SL::OperatorNode *linear = _operation(mix->arguments[2], SL::OP_MUL, 2);
		const SL::OperatorNode *condition = _operation(mix->arguments[3], SL::OP_CALL, 3);
		if (!power || !_variable(power->arguments[0], "pow") || !_vec3_number(power->arguments[2], 2.4f) ||
				!linear || !_albedo_rgb(_member(linear->arguments[0], "rgb")) ||
				!condition || !_variable(condition->arguments[0], "lessThan") || !_albedo_rgb(condition->arguments[1]) ||
				!_vec3_number(condition->arguments[2], 0.04045f)) {
			return false;
		}
		const SL::OperatorNode *divide_linear = _operation(linear->arguments[1], SL::OP_DIV, 2);
		const SL::OperatorNode *scale = _operation(power->arguments[1], SL::OP_MUL, 2);
		if (!divide_linear || !_number(divide_linear->arguments[0], 1) || !_number(divide_linear->arguments[1], 12.92f) || !scale) {
			return false;
		}
		const SL::OperatorNode *offset = _operation(scale->arguments[0], SL::OP_ADD, 2);
		const SL::OperatorNode *divide_power = _operation(scale->arguments[1], SL::OP_DIV, 2);
		if (!offset || !_albedo_rgb(offset->arguments[0]) || !_vec3_number(offset->arguments[1], 0.055f) ||
				!divide_power || !_number(divide_power->arguments[0], 1)) {
			return false;
		}
		const SL::OperatorNode *denominator = _operation(divide_power->arguments[1], SL::OP_ADD, 2);
		return denominator && _number(denominator->arguments[0], 1) && _number(denominator->arguments[1], 0.055f);
	}

	static bool billboard(const SL::Node *p_node) {
		const SL::OperatorNode *matrix = _modelview_assignment(p_node, "VIEW_MATRIX");
		if (!matrix) {
			return false;
		}
		for (int column = 0; column < 3; column++) {
			if (!_variable(_index(matrix->arguments[column + 1], column), "MAIN_CAM_INV_VIEW_MATRIX")) {
				return false;
			}
		}
		return _variable(_index(matrix->arguments[4], 3), "MODEL_MATRIX");
	}

	static bool keep_scale(const SL::Node *p_node) {
		const SL::OperatorNode *matrix = _modelview_assignment(p_node, "MODELVIEW_MATRIX");
		if (!matrix) {
			return false;
		}
		for (int axis = 0; axis < 3; axis++) {
			const SL::OperatorNode *vector = _operation(matrix->arguments[axis + 1], SL::OP_CONSTRUCT, 5);
			if (!vector || !_variable(vector->arguments[0], "vec4")) {
				return false;
			}
			const SL::OperatorNode *length = _operation(vector->arguments[axis + 1], SL::OP_CALL, 2);
			if (!length || !_variable(length->arguments[0], "length") || !_variable(_index(_member(length->arguments[1], "xyz"), axis), "MODEL_MATRIX")) {
				return false;
			}
			for (int component = 0; component < 4; component++) {
				if (component != axis && !_number(vector->arguments[component + 1], 0)) {
					return false;
				}
			}
		}
		const SL::Node *last = matrix->arguments[4];
		if (last->type != SL::Node::NODE_TYPE_CONSTANT) {
			return false;
		}
		const SL::ConstantNode *column = static_cast<const SL::ConstantNode *>(last);
		return column->datatype == SL::TYPE_VEC4 && column->values.size() == 4 && column->values[0].real == 0 && column->values[1].real == 0 && column->values[2].real == 0 && column->values[3].real == 1;
	}

	static bool billboard_normal(const SL::Node *p_node) {
		const SL::OperatorNode *assign = _operation(p_node, SL::OP_ASSIGN, 2);
		if (!assign || !_variable(assign->arguments[0], "MODELVIEW_NORMAL_MATRIX")) {
			return false;
		}
		const SL::OperatorNode *matrix = _operation(assign->arguments[1], SL::OP_CONSTRUCT, 2);
		return matrix && _variable(matrix->arguments[0], "mat3") && _variable(matrix->arguments[1], "MODELVIEW_MATRIX");
	}

	static bool fixed_size(const SL::Node *p_node) {
		const SL::ControlFlowNode *flow = _if_else(p_node);
		if (!flow) {
			return false;
		}
		const SL::OperatorNode *condition = _operation(flow->expressions[0], SL::OP_NOT_EQUAL, 2);
		return condition && _variable(_index(_index(condition->arguments[0], 3), 3), "PROJECTION_MATRIX") && _number(condition->arguments[1], 0) &&
				_fixed_size_branch(*flow->blocks[0], true) && _fixed_size_branch(*flow->blocks[1], false);
	}
};

bool FixedFunctionSpatialMaterial::_lower_transform(const ShaderLanguage::Node *p_node) {
	if (!_transform_flags && FFShaderPattern::billboard(p_node)) {
		_transform_flags |= TRANSFORM_BILLBOARD;
		return true;
	}
	if (_transform_flags == TRANSFORM_BILLBOARD && FFShaderPattern::keep_scale(p_node)) {
		_transform_flags |= TRANSFORM_KEEP_SCALE;
		return true;
	}
	if ((_transform_flags & TRANSFORM_BILLBOARD) && FFShaderPattern::billboard_normal(p_node)) {
		return true;
	}
	if (!(_transform_flags & TRANSFORM_FIXED_SIZE) && FFShaderPattern::fixed_size(p_node)) {
		_transform_flags |= TRANSFORM_FIXED_SIZE;
		return true;
	}
	return false;
}

bool FixedFunctionSpatialMaterial::_is_albedo_srgb_conversion(const ShaderLanguage::Node *p_node) const {
	const RBMap<StringName, FFValue>::Element *albedo = _locals.find("albedo_tex");
	// The LDR profile samples and displays sRGB color directly. Preserve the
	// original sample and alpha when BaseMaterial3D requests linear RGB.
	return albedo && _is_texture_sample(albedo->get()) && FFShaderPattern::albedo_srgb_conversion(p_node);
}
