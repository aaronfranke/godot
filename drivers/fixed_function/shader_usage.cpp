#include "shader_usage.h"

FixedFunctionShaderUsage::FFFeatures::FFFeatures() :
		animated(false), casts_shadows(false), uses_tangents(false), ensure_correct_normals(false) {
}

void FixedFunctionShaderUsage::_push(Vector<FFVisit> &r_pending, const ShaderLanguage::Node *p_node, bool p_assigning) {
	if (p_node) {
		FFVisit visit = { p_node, p_assigning };
		r_pending.push_back(visit);
	}
}

uint32_t FixedFunctionShaderUsage::_variable(const StringName &p_name, bool p_local, bool p_assigning) {
	if (p_local) {
		return 0;
	}
	const String name = p_name;
	if (name == "TIME") {
		return USES_TIME;
	}
	if (name == "ALPHA") {
		return USES_ALPHA;
	}
	if (name == "ALPHA_SCISSOR") {
		return USES_SCISSOR;
	}
	if (name == "VERTEX" && p_assigning) {
		return WRITES_VERTEX;
	}
	if (name == "TANGENT" || name == "NORMALMAP") {
		return USES_TANGENTS;
	}
	return 0;
}

const ShaderLanguage::ShaderNode::Function *FixedFunctionShaderUsage::_find_function(const ShaderLanguage::ShaderNode &p_shader, const StringName &p_name) {
	for (int i = 0; i < p_shader.functions.size(); i++) {
		if (p_shader.functions[i].name == p_name) {
			return &p_shader.functions[i];
		}
	}
	return nullptr;
}

uint32_t FixedFunctionShaderUsage::_function_usage(const ShaderLanguage::ShaderNode &p_shader, const ShaderLanguage::FunctionNode &p_function) {
	typedef ShaderLanguage SL;
	Vector<FFVisit> pending;
	Map<const SL::Node *, uint8_t> visited;
	_push(pending, p_function.body);
	uint32_t flags = 0;
	while (!pending.empty()) {
		const FFVisit visit = pending[pending.size() - 1];
		pending.resize(pending.size() - 1);
		const uint8_t bit = visit.assigning ? 2 : 1;
		if (visited[visit.node] & bit) {
			continue;
		}
		visited[visit.node] |= bit;
		switch (visit.node->type) {
			case SL::Node::TYPE_VARIABLE: {
				const SL::VariableNode *node = static_cast<const SL::VariableNode *>(visit.node);
				flags |= _variable(node->name, node->is_local, visit.assigning);
			} break;
			case SL::Node::TYPE_BLOCK: {
				const SL::BlockNode *node = static_cast<const SL::BlockNode *>(visit.node);
				for (const List<SL::Node *>::Element *E = node->statements.front(); E; E = E->next()) {
					_push(pending, E->get());
				}
			} break;
			case SL::Node::TYPE_CONTROL_FLOW: {
				const SL::ControlFlowNode *node = static_cast<const SL::ControlFlowNode *>(visit.node);
				if (node->flow_op == SL::FLOW_OP_DISCARD) {
					flags |= USES_DISCARD;
				}
				for (int i = 0; i < node->expressions.size(); i++) {
					_push(pending, node->expressions[i]);
				}
				for (int i = 0; i < node->blocks.size(); i++) {
					_push(pending, node->blocks[i]);
				}
			} break;
			case SL::Node::TYPE_OPERATOR: {
				const SL::OperatorNode *node = static_cast<const SL::OperatorNode *>(visit.node);
				const bool writes_first = (node->op >= SL::OP_ASSIGN && node->op <= SL::OP_ASSIGN_BIT_XOR) || node->op == SL::OP_INCREMENT || node->op == SL::OP_DECREMENT || node->op == SL::OP_POST_INCREMENT || node->op == SL::OP_POST_DECREMENT;
				const SL::ShaderNode::Function *callee = nullptr;
				String call_name;
				if (node->op == SL::OP_CALL && !node->arguments.empty() && node->arguments[0]->type == SL::Node::TYPE_VARIABLE) {
					const StringName name = static_cast<const SL::VariableNode *>(node->arguments[0])->name;
					callee = _find_function(p_shader, name);
					call_name = name;
				}
				for (int i = 0; i < node->arguments.size(); i++) {
					bool assigning = (writes_first && i == 0) || (visit.assigning && node->op == SL::OP_INDEX && i == 0);
					if (callee && i > 0 && i <= callee->function->arguments.size()) {
						assigning |= callee->function->arguments[i - 1].qualifier != SL::ARGUMENT_QUALIFIER_IN;
					}
					// modf's second parameter is the sole builtin out argument in 3.x.
					assigning |= call_name == "modf" && i == 2;
					_push(pending, node->arguments[i], assigning);
				}
			} break;
			case SL::Node::TYPE_MEMBER: {
				const SL::MemberNode *node = static_cast<const SL::MemberNode *>(visit.node);
				_push(pending, node->owner, visit.assigning || node->assign_expression != nullptr);
				_push(pending, node->index_expression);
				_push(pending, node->assign_expression);
			} break;
			case SL::Node::TYPE_ARRAY: {
				const SL::ArrayNode *node = static_cast<const SL::ArrayNode *>(visit.node);
				flags |= _variable(node->name, node->is_local, visit.assigning || node->assign_expression != nullptr);
				_push(pending, node->index_expression);
				_push(pending, node->call_expression, visit.assigning);
				_push(pending, node->assign_expression);
			} break;
			case SL::Node::TYPE_VARIABLE_DECLARATION: {
				const SL::VariableDeclarationNode *node = static_cast<const SL::VariableDeclarationNode *>(visit.node);
				for (int i = 0; i < node->declarations.size(); i++) {
					_push(pending, node->declarations[i].initializer);
				}
			} break;
			case SL::Node::TYPE_ARRAY_DECLARATION: {
				const SL::ArrayDeclarationNode *node = static_cast<const SL::ArrayDeclarationNode *>(visit.node);
				for (int i = 0; i < node->declarations.size(); i++) {
					for (int j = 0; j < node->declarations[i].initializer.size(); j++) {
						_push(pending, node->declarations[i].initializer[j]);
					}
				}
			} break;
			case SL::Node::TYPE_ARRAY_CONSTRUCT: {
				const SL::ArrayConstructNode *node = static_cast<const SL::ArrayConstructNode *>(visit.node);
				for (int i = 0; i < node->initializer.size(); i++) {
					_push(pending, node->initializer[i]);
				}
			} break;
			default:
				break;
		}
	}
	return flags;
}

uint32_t FixedFunctionShaderUsage::_stage_usage(const ShaderLanguage::ShaderNode &p_shader, const Map<StringName, uint32_t> &p_usage, const StringName &p_stage) {
	Vector<StringName> pending;
	Set<StringName> visited;
	pending.push_back(p_stage);
	uint32_t flags = 0;
	while (!pending.empty()) {
		const StringName name = pending[pending.size() - 1];
		pending.resize(pending.size() - 1);
		if (visited.has(name)) {
			continue;
		}
		visited.insert(name);
		const ShaderLanguage::ShaderNode::Function *function = _find_function(p_shader, name);
		if (!function) {
			continue;
		}
		flags |= p_usage[name];
		for (int i = 0; i < function->uses_function.size(); i++) {
			pending.push_back(function->uses_function[i]);
		}
	}
	return flags;
}

FixedFunctionShaderUsage::FFFeatures FixedFunctionShaderUsage::analyze_spatial(const ShaderLanguage::ShaderNode &p_shader) {
	Map<StringName, uint32_t> usage;
	for (int i = 0; i < p_shader.functions.size(); i++) {
		usage[p_shader.functions[i].name] = _function_usage(p_shader, *p_shader.functions[i].function);
	}
	const uint32_t vertex = _stage_usage(p_shader, usage, "vertex");
	const uint32_t fragment = _stage_usage(p_shader, usage, "fragment") | _stage_usage(p_shader, usage, "light");
	const uint32_t combined = vertex | fragment;
	bool mix = true;
	bool alpha_prepass = false;
	FFFeatures features;
	for (int i = 0; i < p_shader.render_modes.size(); i++) {
		const String mode = p_shader.render_modes[i];
		if (mode.begins_with("blend_")) {
			mix = mode == "blend_mix";
		}
		if (mode.begins_with("depth_draw_")) {
			alpha_prepass = mode == "depth_draw_alpha_prepass";
		}
		if (mode == "ensure_correct_normals") {
			features.ensure_correct_normals = true;
		}
	}
	features.animated = ((vertex & WRITES_VERTEX) && (vertex & USES_TIME)) || ((fragment & USES_DISCARD) && (fragment & USES_TIME));
	features.casts_shadows = mix && (!(combined & USES_ALPHA) || (combined & USES_SCISSOR) || alpha_prepass);
	features.uses_tangents = (combined & USES_TANGENTS) != 0;
	return features;
}
