#include "spatial_material.h"

FixedFunctionSpatialMaterial::FFUVTransform::FFUVTransform() : scale(1, 1), set(0) {}
FixedFunctionSpatialMaterial::FFNormalMapRecipe::FFNormalMapRecipe() : strength(1), sampler_flags(0) {}
FixedFunctionSpatialMaterial::FFNormalMapChannel::FFNormalMapChannel() : uv_scale(1, 1), uv_set(0), strength(1), sampler_flags(0) {}
FixedFunctionSpatialMaterial::FFChannelRecipe::FFChannelRecipe() : factor(1, 1, 1, 1), sampler_flags(0), texture_black_default(false) {}
FixedFunctionSpatialMaterial::FFEmissionRecipe::FFEmissionRecipe() : energy(0), additive(false) {}
FixedFunctionSpatialMaterial::FFOcclusionRecipe::FFOcclusionRecipe() : weights(1, 0, 0, 0), light_affect(0), sampler_flags(0) {}
FixedFunctionSpatialMaterial::FFShaderRecipe::FFShaderRecipe() : point_size(1), albedo_uses_vertex_color(false), albedo_uses_vertex_alpha(false), lighting_flags(0), transform_flags(0) {
	state.depth_test_enabled = state.depth_write_enabled = true;
	state.cull_mode = FixedFunctionGraphicsDevice::CULL_BACK;
}
FixedFunctionSpatialMaterial::FFMaterialChannel::FFMaterialChannel() : uv_scale(1, 1), factor(1, 1, 1, 1), uv_set(0), sampler_flags(0) {}
FixedFunctionSpatialMaterial::FFEmissionChannel::FFEmissionChannel() : energy(0), additive(false) {}
FixedFunctionSpatialMaterial::FFOcclusionChannel::FFOcclusionChannel() : uv_scale(1, 1), weights(1, 0, 0, 0), uv_set(0), light_affect(0), sampler_flags(0) {}
FixedFunctionSpatialMaterial::FFMaterialParameters::FFMaterialParameters() : point_size(1), albedo_uses_vertex_color(false), albedo_uses_vertex_alpha(false), is_fallback(false), lighting_flags(0), transform_flags(0) {}
FixedFunctionSpatialMaterial::FFValue::FFValue() : vector(0, 0, 0, 0), type(VALUE_NONE), components(0), uses_vertex_color(false) {}

FixedFunctionSpatialMaterial::FixedFunctionSpatialMaterial(const ShaderLanguage::ShaderNode &p_shader) : _shader(p_shader), _alpha_written(false), _alpha_cutoff_written(false), _albedo_written(false), _transform_flags(0) {
	for (int i = 0; i < 2; i++) {
		_uv[i].type = VALUE_UV;
		_uv[i].uv.set = i;
	}
	_normal_depth.vector.r = 1;
	_point_size.vector.r = 1;
	_albedo.type = VALUE_COLOR;
	_alpha.type = VALUE_ALPHA; // ALPHA defaults to one independently of ALBEDO.
}

bool FixedFunctionSpatialMaterial::_same_texture_mapping(const FFChannelRecipe &p_left, const FFChannelRecipe &p_right) {
	if (p_left.texture_parameter != p_right.texture_parameter) {
		return false;
	}
	return p_left.texture_parameter == StringName() ||
			(p_left.sampler_flags == p_right.sampler_flags && p_left.uv.set == p_right.uv.set && p_left.uv.scale == p_right.uv.scale && p_left.uv.offset == p_right.uv.offset &&
					p_left.uv.scale_parameter == p_right.uv.scale_parameter && p_left.uv.offset_parameter == p_right.uv.offset_parameter);
}

bool FixedFunctionSpatialMaterial::_variable(const ShaderLanguage::Node *p_node, const StringName &p_name) {
	return p_node && p_node->type == ShaderLanguage::Node::NODE_TYPE_VARIABLE && static_cast<const ShaderLanguage::VariableNode *>(p_node)->name == p_name;
}

bool FixedFunctionSpatialMaterial::_constant_srgb_condition(const ShaderLanguage::Node *p_node) {
	if (!p_node || p_node->type != ShaderLanguage::Node::NODE_TYPE_OPERATOR) {
		return false;
	}
	const ShaderLanguage::OperatorNode *op = static_cast<const ShaderLanguage::OperatorNode *>(p_node);
	return op->op == ShaderLanguage::OP_NOT && op->arguments.size() == 1 && _variable(op->arguments[0], "OUTPUT_IS_SRGB");
}

bool FixedFunctionSpatialMaterial::_is_texture_sample(const FFValue &p_value) {
	return p_value.type == VALUE_COLOR && p_value.color.texture_parameter != StringName() &&
			p_value.color.factor == Color(1, 1, 1, 1) && p_value.color.factor_parameter == StringName() && !p_value.uses_vertex_color;
}

Error FixedFunctionSpatialMaterial::_multiply(const FFValue &p_left, const FFValue &p_right, FFValue &r_value) {
	if ((p_left.type == VALUE_ALPHA && p_right.type == VALUE_SCALAR && p_right.parameter == StringName()) ||
			(p_right.type == VALUE_ALPHA && p_left.type == VALUE_SCALAR && p_left.parameter == StringName())) {
		r_value = p_left.type == VALUE_ALPHA ? p_left : p_right;
		r_value.color.factor.a *= p_left.type == VALUE_SCALAR ? p_left.vector.r : p_right.vector.r;
		return OK;
	}
	if ((p_left.type == VALUE_LIGHTING_SCALAR || p_left.type == VALUE_SCALAR) && (p_right.type == VALUE_LIGHTING_SCALAR || p_right.type == VALUE_SCALAR)) {
		r_value.type = VALUE_LIGHTING_SCALAR;
		return OK;
	}
	if (p_left.type == VALUE_UV && p_right.type == VALUE_VECTOR && p_right.components == 2 && p_left.uv.scale_parameter == StringName() && p_left.uv.offset_parameter == StringName() && p_left.uv.offset == Vector2()) {
		r_value = p_left;
		r_value.uv.scale *= Vector2(p_right.vector.r, p_right.vector.g);
		r_value.uv.scale_parameter = p_right.parameter;
		return OK;
	}
	if ((p_left.type != VALUE_COLOR && p_left.type != VALUE_ALPHA) || p_right.type != p_left.type ||
			(p_left.color.factor_parameter != StringName() && p_right.color.factor_parameter != StringName()) ||
			(p_left.color.texture_parameter != StringName() && p_right.color.texture_parameter != StringName()) ||
			(p_left.uses_vertex_color && p_right.uses_vertex_color)) {
		return ERR_UNAVAILABLE;
	}
	r_value = p_left;
	r_value.color.factor *= p_right.color.factor;
	if (p_right.color.factor_parameter != StringName()) {
		r_value.color.factor_parameter = p_right.color.factor_parameter;
	}
	if (p_right.color.texture_parameter != StringName()) {
		r_value.color.texture_parameter = p_right.color.texture_parameter;
		r_value.color.texture_black_default = p_right.color.texture_black_default;
		r_value.color.uv = p_right.color.uv;
		r_value.color.sampler_flags = p_right.color.sampler_flags;
	}
	r_value.uses_vertex_color |= p_right.uses_vertex_color;
	return OK;
}

Error FixedFunctionSpatialMaterial::_expression(const ShaderLanguage::Node *p_node, FFValue &r_value, int p_depth) const {
	if (!p_node || p_depth > MAX_EXPRESSION_DEPTH) {
		return ERR_UNAVAILABLE;
	}
	FFValue value;
	switch (p_node->type) {
		case ShaderLanguage::Node::NODE_TYPE_VARIABLE: {
			const ShaderLanguage::VariableNode *variable = static_cast<const ShaderLanguage::VariableNode *>(p_node);
			if (variable->is_local) {
				const RBMap<StringName, FFValue>::Element *local = _locals.find(variable->name);
				if (!local) {
					return ERR_UNAVAILABLE;
				}
				value = local->get();
			} else if (variable->name == StringName("UV") || variable->name == StringName("UV2")) {
				value = _uv[variable->name == StringName("UV2") ? 1 : 0];
			} else if (variable->name == StringName("POINT_COORD")) {
				value.type = VALUE_UV;
				value.uv.set = UV_POINT_COORD;
			} else if (variable->name == StringName("COLOR")) {
				value.type = VALUE_COLOR;
				value.uses_vertex_color = true;
			} else {
				const ShaderLanguage::ShaderNode::Uniform *uniform = _shader.uniforms.getptr(variable->name);
				if (!uniform || uniform->scope != ShaderLanguage::ShaderNode::Uniform::SCOPE_LOCAL || uniform->array_size) {
					return ERR_UNAVAILABLE;
				}
				if (uniform->type == ShaderLanguage::TYPE_SAMPLER2D) {
					if (uniform->hint != ShaderLanguage::ShaderNode::Uniform::HINT_NONE && uniform->hint != ShaderLanguage::ShaderNode::Uniform::HINT_DEFAULT_WHITE && uniform->hint != ShaderLanguage::ShaderNode::Uniform::HINT_DEFAULT_BLACK && uniform->hint != ShaderLanguage::ShaderNode::Uniform::HINT_SOURCE_COLOR && uniform->hint != ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL && uniform->hint != ShaderLanguage::ShaderNode::Uniform::HINT_ROUGHNESS_NORMAL && !(uniform->hint >= ShaderLanguage::ShaderNode::Uniform::HINT_ROUGHNESS_R && uniform->hint <= ShaderLanguage::ShaderNode::Uniform::HINT_ROUGHNESS_GRAY)) {
						return ERR_UNAVAILABLE;
					}
					value.type = VALUE_SAMPLER;
					value.parameter = variable->name;
					value.color.texture_black_default = uniform->hint == ShaderLanguage::ShaderNode::Uniform::HINT_DEFAULT_BLACK;
				} else if (uniform->type == ShaderLanguage::TYPE_VEC4) {
					value.type = VALUE_COLOR;
					value.color.factor_parameter = variable->name;
				} else if (uniform->type == ShaderLanguage::TYPE_VEC2 || uniform->type == ShaderLanguage::TYPE_VEC3) {
					value.type = VALUE_VECTOR;
					value.parameter = variable->name;
					value.components = uniform->type == ShaderLanguage::TYPE_VEC2 ? 2 : 3;
					// Runtime values replace these placeholders; constants combine separately.
					value.vector = Color(1, 1, 1, 1);
				} else if (uniform->type == ShaderLanguage::TYPE_FLOAT) {
					value.type = VALUE_SCALAR;
					value.parameter = variable->name;
					value.vector.r = 1;
				} else {
					return ERR_UNAVAILABLE;
				}
			}
		} break;
		case ShaderLanguage::Node::NODE_TYPE_CONSTANT: {
			const ShaderLanguage::ConstantNode *constant = static_cast<const ShaderLanguage::ConstantNode *>(p_node);
			if (constant->array_size || constant->datatype < ShaderLanguage::TYPE_FLOAT || constant->datatype > ShaderLanguage::TYPE_VEC4 || constant->values.size() < 1 || constant->values.size() > 4) {
				return ERR_UNAVAILABLE;
			}
			value.components = constant->values.size();
			value.type = value.components >= 3 ? VALUE_COLOR : VALUE_VECTOR;
			if (value.components == 1) {
				value.type = VALUE_SCALAR;
			}
			for (int i = 0; i < value.components; i++) {
				value.vector[i] = constant->values[i].real;
			}
			if (value.components == 3) {
				value.vector.a = 1;
			}
			value.color.factor = value.vector;
		} break;
		case ShaderLanguage::Node::NODE_TYPE_MEMBER: {
			const ShaderLanguage::MemberNode *member = static_cast<const ShaderLanguage::MemberNode *>(p_node);
			if (member->index_expression || member->assign_expression || member->call_expression || _expression(member->owner, value, p_depth + 1) != OK) {
				return ERR_UNAVAILABLE;
			}
			if (member->name == StringName("rgb") && value.type == VALUE_COLOR) {
				value.color.factor.a = 1; // Alpha contributes only when ALPHA requests it.
			} else if (member->name == StringName("a") && value.type == VALUE_COLOR) {
				value.type = VALUE_ALPHA;
			} else if (_is_texture_sample(value) && (member->name == StringName("r") || member->name == StringName("g") || member->name == StringName("b"))) {
				value.type = VALUE_TEXTURE_SCALAR;
				value.vector = Color(0, 0, 0, 0);
				int channel = 0;
				if (member->name == StringName("g")) {
					channel = 1;
				} else if (member->name == StringName("b")) {
					channel = 2;
				}
				value.vector[channel] = 1;
			} else if (member->name == StringName("xy") && value.type == VALUE_VECTOR && value.components >= 2) {
				value.components = 2;
			} else {
				return ERR_UNAVAILABLE;
			}
		} break;
		case ShaderLanguage::Node::NODE_TYPE_OPERATOR: {
			const ShaderLanguage::OperatorNode *op = static_cast<const ShaderLanguage::OperatorNode *>(p_node);
			if (op->op == ShaderLanguage::OP_CALL && op->arguments.size() == 3 && _variable(op->arguments[0], "texture")) {
				FFValue sampler, uv;
				if (_expression(op->arguments[1], sampler, p_depth + 1) != OK || _expression(op->arguments[2], uv, p_depth + 1) != OK || sampler.type != VALUE_SAMPLER || uv.type != VALUE_UV) {
					return ERR_UNAVAILABLE;
				}
				value.type = VALUE_COLOR;
				value.color.texture_parameter = sampler.parameter;
				value.color.texture_black_default = sampler.color.texture_black_default;
				value.color.uv = uv.uv;
				const ShaderLanguage::ShaderNode::Uniform *uniform = _shader.uniforms.getptr(sampler.parameter);
				const ShaderLanguage::TextureFilter filter = uniform->filter;
				if (filter != ShaderLanguage::FILTER_NEAREST && filter != ShaderLanguage::FILTER_NEAREST_MIPMAP && filter != ShaderLanguage::FILTER_NEAREST_MIPMAP_ANISOTROPIC) {
					value.color.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
				}
				if (filter != ShaderLanguage::FILTER_NEAREST && filter != ShaderLanguage::FILTER_LINEAR) {
					value.color.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
				}
				if (filter == ShaderLanguage::FILTER_NEAREST_MIPMAP_ANISOTROPIC || filter == ShaderLanguage::FILTER_LINEAR_MIPMAP_ANISOTROPIC) {
					value.color.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_ANISOTROPIC;
				}
				if (uniform->repeat != ShaderLanguage::REPEAT_DISABLE) {
					value.color.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V;
				}
			} else if (op->op == ShaderLanguage::OP_CALL && op->arguments.size() == 3 && _variable(op->arguments[0], "dot")) {
				FFValue weights;
				if (_expression(op->arguments[1], value, p_depth + 1) != OK ||
						_expression(op->arguments[2], weights, p_depth + 1) != OK || weights.type != VALUE_COLOR ||
						weights.color.texture_parameter != StringName() || weights.uses_vertex_color) {
					return ERR_UNAVAILABLE;
				}
				if (!_is_texture_sample(value)) {
					if (value.type != VALUE_COLOR) {
						return ERR_UNAVAILABLE;
					}
					value.type = VALUE_LIGHTING_SCALAR;
					break;
				}
				value.type = VALUE_TEXTURE_SCALAR;
				value.vector = weights.color.factor;
				value.parameter = weights.color.factor_parameter;
			} else if ((op->op == ShaderLanguage::OP_MUL || op->op == ShaderLanguage::OP_ADD) && op->arguments.size() == 2) {
				FFValue left, right;
				if (_expression(op->arguments[0], left, p_depth + 1) != OK || _expression(op->arguments[1], right, p_depth + 1) != OK) {
					return ERR_UNAVAILABLE;
				}
				if (op->op == ShaderLanguage::OP_MUL) {
					if (_multiply(left, right, value) != OK) {
						return ERR_UNAVAILABLE;
					}
				} else if (left.type == VALUE_UV && right.type == VALUE_VECTOR && right.components == 2 && left.uv.offset_parameter == StringName()) {
					value = left;
					if (right.parameter == StringName()) {
						value.uv.offset += Vector2(right.vector.r, right.vector.g);
					}
					value.uv.offset_parameter = right.parameter;
				} else {
					return ERR_UNAVAILABLE;
				}
			} else {
				return ERR_UNAVAILABLE;
			}
		} break;
		default:
			return ERR_UNAVAILABLE;
	}
	r_value = value;
	return OK;
}

Error FixedFunctionSpatialMaterial::_block(const ShaderLanguage::BlockNode &p_block, bool p_vertex) {
	for (const ShaderLanguage::Node *node : p_block.statements) {
		if (p_vertex && _lower_transform(node)) {
			continue;
		}
		if (!p_vertex && _is_albedo_srgb_conversion(node)) {
			continue;
		}
		if (node->type == ShaderLanguage::Node::NODE_TYPE_CONTROL_FLOW) {
			const ShaderLanguage::ControlFlowNode *flow = static_cast<const ShaderLanguage::ControlFlowNode *>(node);
			// OUTPUT_IS_SRGB is true for this GLES2-like profile: the emitted conversion is dead.
			if (!p_vertex || flow->flow_op != ShaderLanguage::FLOW_OP_IF || flow->expressions.size() != 1 || flow->blocks.size() != 1 || !_constant_srgb_condition(flow->expressions[0])) {
				return ERR_UNAVAILABLE;
			}
			continue;
		}
		if (node->type == ShaderLanguage::Node::NODE_TYPE_VARIABLE_DECLARATION) {
			const ShaderLanguage::VariableDeclarationNode *declaration = static_cast<const ShaderLanguage::VariableDeclarationNode *>(node);
			for (int i = 0; i < declaration->declarations.size(); i++) {
				FFValue value;
				if (declaration->declarations[i].initializer.size() != 1 || _expression(declaration->declarations[i].initializer[0], value) != OK) {
					return ERR_UNAVAILABLE;
				}
				_locals[declaration->declarations[i].name] = value;
			}
			continue;
		}
		if (node->type != ShaderLanguage::Node::NODE_TYPE_OPERATOR) {
			return ERR_UNAVAILABLE;
		}
		const ShaderLanguage::OperatorNode *op = static_cast<const ShaderLanguage::OperatorNode *>(node);
		if ((op->op != ShaderLanguage::OP_ASSIGN && op->op != ShaderLanguage::OP_ASSIGN_MUL) || op->arguments.size() != 2 || op->arguments[0]->type != ShaderLanguage::Node::NODE_TYPE_VARIABLE) {
			return ERR_UNAVAILABLE;
		}
		const ShaderLanguage::VariableNode *target = static_cast<const ShaderLanguage::VariableNode *>(op->arguments[0]);
		if (!target->is_local && (target->name == StringName("METALLIC") || target->name == StringName("ROUGHNESS") || target->name == StringName("SPECULAR"))) {
			// The bounded Lambert approximation does not execute PBR/specular outputs.
			if (op->op != ShaderLanguage::OP_ASSIGN || !_is_ignored_pbr_value(op->arguments[1])) {
				return ERR_UNAVAILABLE;
			}
			continue;
		}
		if (!target->is_local && !p_vertex && target->name == StringName("EMISSION") && op->op == ShaderLanguage::OP_ASSIGN) {
			_emission = FFEmissionRecipe();
			_lower_emission(op->arguments[1], _emission);
			continue;
		}
		const bool optional_occlusion = !target->is_local && !p_vertex && op->op == ShaderLanguage::OP_ASSIGN &&
				(target->name == StringName("AO") || target->name == StringName("AO_LIGHT_AFFECT"));
		FFValue value;
		if (_expression(op->arguments[1], value) != OK) {
			if (optional_occlusion) {
				_occlusion = FFValue();
				continue;
			}
			return ERR_UNAVAILABLE;
		}
		if (target->is_local) {
			if (op->op == ShaderLanguage::OP_ASSIGN_MUL) {
				const RBMap<StringName, FFValue>::Element *old = _locals.find(target->name);
				FFValue product;
				if (!old || _multiply(old->get(), value, product) != OK) {
					return ERR_UNAVAILABLE;
				}
				value = product;
			}
			_locals[target->name] = value;
		} else if (p_vertex && (target->name == StringName("UV") || target->name == StringName("UV2")) && value.type == VALUE_UV && op->op == ShaderLanguage::OP_ASSIGN) {
			_uv[target->name == StringName("UV2") ? 1 : 0] = value;
		} else if (p_vertex && target->name == StringName("POINT_SIZE") && value.type == VALUE_SCALAR && op->op == ShaderLanguage::OP_ASSIGN) {
			_point_size = value;
		} else if (!p_vertex && target->name == StringName("ALBEDO") && value.type == VALUE_COLOR && op->op == ShaderLanguage::OP_ASSIGN) {
			_albedo = value;
			_albedo_written = true;
		} else if (!p_vertex && target->name == StringName("NORMAL_MAP") && value.type == VALUE_COLOR && op->op == ShaderLanguage::OP_ASSIGN) {
			// Tangent-space data is sampled independently, never multiplied as sRGB color.
			if (value.color.texture_parameter == StringName() || value.color.factor != Color(1, 1, 1, 1) || value.color.factor_parameter != StringName() || value.uses_vertex_color) {
				return ERR_UNAVAILABLE;
			}
			_normal = value;
		} else if (!p_vertex && target->name == StringName("NORMAL_MAP_DEPTH") && value.type == VALUE_SCALAR && op->op == ShaderLanguage::OP_ASSIGN) {
			_normal_depth = value;
		} else if (optional_occlusion) {
			if (target->name == StringName("AO")) {
				if (value.type == VALUE_ALPHA && value.color.texture_parameter != StringName() &&
						value.color.factor == Color(1, 1, 1, 1) && value.color.factor_parameter == StringName() && !value.uses_vertex_color) {
					value.type = VALUE_TEXTURE_SCALAR;
					value.vector = Color(0, 0, 0, 1);
				}
				_occlusion = value;
			} else if (value.type == VALUE_SCALAR) {
				_occlusion_light_affect = value;
			}
		} else if (!p_vertex && target->name == StringName("ALPHA")) {
			if (value.type == VALUE_SCALAR && value.parameter == StringName()) {
				value.type = VALUE_ALPHA;
				value.color.factor.a = value.vector.r;
			}
			if (value.type != VALUE_ALPHA) {
				return ERR_UNAVAILABLE;
			}
			if (op->op == ShaderLanguage::OP_ASSIGN_MUL) {
				FFValue product;
				if (_multiply(_alpha, value, product) != OK) {
					return ERR_UNAVAILABLE;
				}
				value = product;
			}
			_alpha = value;
			_alpha_written = true;
		} else if (!p_vertex && target->name == StringName("ALPHA_SCISSOR_THRESHOLD") && value.type == VALUE_SCALAR && op->op == ShaderLanguage::OP_ASSIGN) {
			_alpha_cutoff = value;
			_alpha_cutoff_written = true;
		} else {
			return ERR_UNAVAILABLE;
		}
	}
	return OK;
}

void FixedFunctionSpatialMaterial::lower_fallback(const ShaderLanguage::ShaderNode *p_shader, FFShaderRecipe &r_recipe) {
	FFShaderRecipe recipe;
	recipe.lighting_flags = LIGHTING_UNSHADED;
	recipe.albedo.factor = Color(1, 0, 1); // Visible diagnostic when no useful albedo is recognizable.
	if (p_shader) {
		for (int i = 0; i < p_shader->render_modes.size(); i++) {
			const String mode = p_shader->render_modes[i];
			if (mode == "cull_disabled") {
				recipe.state.cull_mode = FixedFunctionGraphicsDevice::CULL_DISABLED;
			} else if (mode == "cull_back") {
				recipe.state.cull_mode = FixedFunctionGraphicsDevice::CULL_BACK;
			} else if (mode == "cull_front") {
				recipe.state.cull_mode = FixedFunctionGraphicsDevice::CULL_FRONT;
			} else if (mode == "depth_test_disabled") {
				recipe.state.depth_test_enabled = recipe.state.depth_write_enabled = false;
			} else if (mode == "depth_draw_never") {
				recipe.state.depth_write_enabled = false;
			} else if (mode == "fog_disabled") {
				recipe.lighting_flags |= LIGHTING_FOG_DISABLED;
			}
		}
		FixedFunctionSpatialMaterial lowering(*p_shader);
		for (int stage = 0; stage < 2; stage++) {
			lowering._locals.clear();
			for (int i = 0; i < p_shader->vfunctions.size(); i++) {
				const ShaderLanguage::FunctionNode *function = p_shader->vfunctions[i].function;
				if (function->name == StringName(stage == 0 ? "vertex" : "fragment") && function->body) {
					// Keep only the recognizable prefix before the first unsupported statement.
					// Its albedo is an approximation; helpers, lighting and remaining effects do not run.
					lowering._block(*function->body, stage == 0);
				}
			}
		}
		if (lowering._albedo_written) {
			recipe.albedo = lowering._albedo.color;
			recipe.albedo_uses_vertex_color = lowering._albedo.uses_vertex_color;
		}
	}
	r_recipe = recipe;
}

Error FixedFunctionSpatialMaterial::lower(const ShaderLanguage::ShaderNode &p_shader, FFShaderRecipe &r_recipe) {
	if (!p_shader.varyings.is_empty() || !p_shader.constants.is_empty() || !p_shader.structs.is_empty() || !p_shader.stencil_modes.is_empty()) {
		return ERR_UNAVAILABLE;
	}
	FFShaderRecipe recipe;
	bool depth_draw_always = false;
	for (int i = 0; i < p_shader.render_modes.size(); i++) {
		const String mode = p_shader.render_modes[i];
		if (mode == "unshaded") {
			recipe.lighting_flags |= LIGHTING_UNSHADED;
		} else if (mode == "ambient_light_disabled") {
			recipe.lighting_flags |= LIGHTING_AMBIENT_DISABLED;
		} else if (mode == "vertex_lighting") {
			recipe.lighting_flags |= LIGHTING_VERTEX;
		} else if (mode == "shadows_disabled") {
			recipe.lighting_flags |= LIGHTING_SHADOWS_DISABLED;
		} else if (mode == "fog_disabled") {
			recipe.lighting_flags |= LIGHTING_FOG_DISABLED;
		} else if (mode == "cull_back") {
			recipe.state.cull_mode = FixedFunctionGraphicsDevice::CULL_BACK;
		} else if (mode == "cull_front") {
			recipe.state.cull_mode = FixedFunctionGraphicsDevice::CULL_FRONT;
		} else if (mode == "cull_disabled") {
			recipe.state.cull_mode = FixedFunctionGraphicsDevice::CULL_DISABLED;
		} else if (mode == "depth_draw_never") {
			recipe.state.depth_write_enabled = false;
		} else if (mode == "blend_add") {
			recipe.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_ADD;
		} else if (mode == "blend_mul") {
			recipe.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MUL;
		} else if (mode == "depth_draw_always") {
			depth_draw_always = true;
		} else if (mode == "depth_test_disabled") {
			recipe.state.depth_test_enabled = recipe.state.depth_write_enabled = false;
		} else if (mode == "async_visible" || mode == "async_hidden" || mode == "blend_mix" || mode == "depth_draw_opaque" || mode.begins_with("diffuse_") || mode.begins_with("specular_") || mode == "ensure_correct_normals") { /* Shared Lambert approximation. */
		} else {
			return ERR_UNAVAILABLE;
		}
	}
	FixedFunctionSpatialMaterial lowering(p_shader);
	// Vertex state must be established before interpreting fragment UV/COLOR reads.
	for (int stage = 0; stage < 2; stage++) {
		lowering._locals.clear();
		for (int i = 0; i < p_shader.vfunctions.size(); i++) {
			const ShaderLanguage::FunctionNode *function = p_shader.vfunctions[i].function;
			if (function->name != StringName("vertex") && function->name != StringName("fragment")) {
				return ERR_UNAVAILABLE;
			}
			if (function->return_type != ShaderLanguage::TYPE_VOID || !function->arguments.is_empty() || !function->body) {
				return ERR_UNAVAILABLE;
			}
			if ((function->name == StringName("vertex")) == (stage == 0)) {
				const Error error = lowering._block(*function->body, stage == 0);
				if (error != OK) {
					return error;
				}
			}
		}
	}
	recipe.transform_flags = lowering._transform_flags;
	recipe.point_size = lowering._point_size.vector.r;
	recipe.point_size_parameter = lowering._point_size.parameter;
	recipe.normal.uv = lowering._normal.color.uv;
	recipe.normal.texture_parameter = lowering._normal.color.texture_parameter;
	recipe.normal.strength_parameter = lowering._normal_depth.parameter;
	recipe.normal.strength = lowering._normal_depth.vector.r;
	recipe.normal.sampler_flags = lowering._normal.color.sampler_flags;
	recipe.emission = lowering._emission;
	if (lowering._occlusion.type == VALUE_TEXTURE_SCALAR) {
		recipe.occlusion.uv = lowering._occlusion.color.uv;
		recipe.occlusion.sampler_flags = lowering._occlusion.color.sampler_flags;
		recipe.occlusion.texture_parameter = lowering._occlusion.color.texture_parameter;
		recipe.occlusion.weights = lowering._occlusion.vector;
		recipe.occlusion.weights_parameter = lowering._occlusion.parameter;
		recipe.occlusion.light_affect = lowering._occlusion_light_affect.vector.r;
		recipe.occlusion.light_affect_parameter = lowering._occlusion_light_affect.parameter;
	}
	recipe.albedo = lowering._albedo.color;
	recipe.albedo_uses_vertex_color = lowering._albedo.uses_vertex_color;
	if (lowering._alpha_written || lowering._alpha_cutoff_written) {
		// One native texture supplies RGB and alpha. Reject independently sampled alpha
		// rather than quietly substituting the albedo map's alpha or UV transform.
		if (!_same_texture_mapping(lowering._albedo.color, lowering._alpha.color) ||
				lowering._albedo.color.factor_parameter != lowering._alpha.color.factor_parameter) {
			return ERR_UNAVAILABLE;
		}
		recipe.albedo.factor.a = lowering._alpha.color.factor.a;
		recipe.albedo_uses_vertex_alpha = lowering._alpha.uses_vertex_color;
	}
	if (lowering._alpha_cutoff_written) {
		recipe.alpha_cutoff_parameter = lowering._alpha_cutoff.parameter;
		recipe.state.alpha_cutoff = lowering._alpha_cutoff.vector.r;
		recipe.state.alpha_cutoff_enabled = true;
	} else if (lowering._alpha_written && recipe.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED) {
		recipe.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
	}
	if (recipe.state.blend_mode != FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED) {
		recipe.state.depth_write_enabled = recipe.state.depth_write_enabled && depth_draw_always;
		recipe.state.texture_alpha_enabled = lowering._alpha_written;
	}
	r_recipe = recipe;
	return OK;
}
