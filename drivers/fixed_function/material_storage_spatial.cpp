#include "drivers/fixed_function/material_storage.h"

Error FixedFunctionMaterialStorage::_lower_spatial_shader(const String &p_code, FFShader &r_shader) {
	r_shader.uniform_defaults.clear();
	FixedFunctionSpatialMaterial::lower_fallback(nullptr, r_shader.spatial_recipe);
	if (p_code.is_empty() || ShaderLanguage::get_shader_type(p_code) != "spatial") {
		return ERR_UNAVAILABLE;
	}
	ShaderLanguage language;
	const Error error = _parse_shader(p_code, language);
	if (error != OK) {
		return error;
	}
	const ShaderLanguage::ShaderNode *shader = language.get_shader();
	for (const KeyValue<StringName, ShaderLanguage::ShaderNode::Uniform> &E : shader->uniforms) {
		Variant value = ShaderLanguage::constant_value_to_variant(E.value.default_value, E.value.type, E.value.array_size, E.value.hint);
		if (value.get_type() == Variant::NIL) {
			if (E.value.type == ShaderLanguage::TYPE_VEC2) {
				value = Vector2();
			} else if (E.value.type == ShaderLanguage::TYPE_VEC3) {
				value = Vector3();
			} else if (E.value.type == ShaderLanguage::TYPE_VEC4) {
				value = Color(0, 0, 0, 0);
			}
		}
		r_shader.uniform_defaults[E.key] = value;
	}
	const Error lower_error = FixedFunctionSpatialMaterial::lower(*shader, r_shader.spatial_recipe);
	if (lower_error != OK) {
		FixedFunctionSpatialMaterial::lower_fallback(shader, r_shader.spatial_recipe);
	}
	return lower_error;
}

Error FixedFunctionMaterialStorage::material_get_spatial_parameters(RID p_material, FixedFunctionSpatialMaterial::FFMaterialParameters &r_parameters) const {
	return _material_get_spatial_parameters(p_material, r_parameters, false);
}

Error FixedFunctionMaterialStorage::material_get_spatial_draw_parameters(RID p_material, FixedFunctionSpatialMaterial::FFMaterialParameters &r_parameters) const {
	const Error error = material_get_spatial_parameters(p_material, r_parameters);
	if (error == OK || (p_material.is_valid() && !owns_material(p_material))) {
		return error;
	}
	return _material_get_spatial_parameters(p_material, r_parameters, true);
}

Error FixedFunctionMaterialStorage::_material_get_spatial_parameters(RID p_material, FixedFunctionSpatialMaterial::FFMaterialParameters &r_parameters, bool p_fallback) const {
	const FFMaterial *material = _get_material(p_material);
	if (p_fallback && (!p_material.is_valid() || (material && !material->shader.is_valid()))) {
		FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
		parameters.state = FixedFunctionSpatialMaterial::FFShaderRecipe().state;
		r_parameters = parameters; // Ordinary missing/default materials are white.
		return OK;
	}
	if (!material) {
		return ERR_INVALID_PARAMETER;
	}
	const FFShader *shader = _get_shader(material->shader);
	if (!shader) {
		return ERR_UNAVAILABLE;
	}
	if (p_fallback && shader->code.is_empty()) {
		FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
		parameters.state = FixedFunctionSpatialMaterial::FFShaderRecipe().state;
		r_parameters = parameters;
		return OK;
	}
	if (!p_fallback && shader->spatial_error != OK) {
		return shader->spatial_error;
	}
	if (!p_fallback && (!shader->custom_defines.is_empty() || material->next_pass.is_valid())) {
		return ERR_UNAVAILABLE;
	}
	const FixedFunctionSpatialMaterial::FFShaderRecipe &recipe = shader->spatial_recipe;
	FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
	parameters.state = recipe.state;
	parameters.lighting_flags = recipe.lighting_flags;
	parameters.albedo_uses_vertex_color = recipe.albedo_uses_vertex_color;
	parameters.albedo_uses_vertex_alpha = recipe.albedo_uses_vertex_alpha;
	parameters.transform_flags = recipe.transform_flags;
	if (!Math::is_nan(recipe.point_size) && !Math::is_inf(recipe.point_size)) {
		parameters.point_size = recipe.point_size;
	}
	if (recipe.point_size_parameter != StringName()) {
		const Variant value = _spatial_parameter(*material, *shader, recipe.point_size_parameter);
		if (value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT) {
			const float size = value;
			if (!Math::is_nan(size) && !Math::is_inf(size)) {
				parameters.point_size = size;
			}
		}
	}
	parameters.is_fallback = p_fallback;
	if (p_fallback) {
		parameters.lighting_flags |= FixedFunctionSpatialMaterial::LIGHTING_UNSHADED;
		parameters.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
		parameters.state.alpha_cutoff_enabled = false;
		parameters.albedo_uses_vertex_alpha = false;
	}
	if (!_resolve_spatial_channel(*material, *shader, recipe.albedo, parameters.albedo, p_fallback)) {
		return ERR_INVALID_PARAMETER;
	}
	if (recipe.albedo.texture_black_default && !parameters.albedo.texture.is_valid()) {
		parameters.albedo.factor.r = parameters.albedo.factor.g = parameters.albedo.factor.b = 0;
	}
	if (!p_fallback && recipe.alpha_cutoff_parameter != StringName()) {
		const RBMap<StringName, Variant>::Element *parameter = material->parameters.find(recipe.alpha_cutoff_parameter);
		if (!parameter) {
			parameter = shader->uniform_defaults.find(recipe.alpha_cutoff_parameter);
		}
		if (!parameter || (parameter->get().get_type() != Variant::FLOAT && parameter->get().get_type() != Variant::INT)) {
			return ERR_INVALID_PARAMETER;
		}
		parameters.state.alpha_cutoff = parameter->get();
	}
	if (parameters.state.alpha_cutoff_enabled && (Math::is_nan(parameters.state.alpha_cutoff) || Math::is_inf(parameters.state.alpha_cutoff))) {
		return ERR_INVALID_PARAMETER;
	}
	if (!p_fallback && recipe.normal.texture_parameter != StringName()) {
		const FixedFunctionSpatialMaterial::FFNormalMapRecipe &normal = recipe.normal;
		parameters.normal.uv_scale = normal.uv.scale;
		parameters.normal.uv_offset = normal.uv.offset;
		parameters.normal.uv_set = normal.uv.set;
		parameters.normal.strength = normal.strength;
		parameters.normal.sampler_flags = normal.sampler_flags;
		const StringName normal_names[] = { normal.texture_parameter, normal.strength_parameter, normal.uv.scale_parameter, normal.uv.offset_parameter };
		bool valid = true;
		for (int i = 0; i < 4; i++) {
			if (normal_names[i] == StringName()) {
				continue;
			}
			const RBMap<StringName, Variant>::Element *parameter = material->parameters.find(normal_names[i]);
			if (!parameter) {
				parameter = shader->uniform_defaults.find(normal_names[i]);
			}
			const Variant value = parameter ? parameter->get() : Variant();
			if (i == 0) {
				if (value.get_type() == Variant::RID) {
					parameters.normal.texture = value;
				} else if (value.get_type() != Variant::NIL) {
					valid = false;
				}
				if (!parameters.normal.texture.is_valid()) {
					const RBMap<Pair<StringName, int>, RID>::Element *texture = shader->default_textures.find(Pair<StringName, int>(normal_names[i], 0));
					if (texture) {
						parameters.normal.texture = texture->get();
					}
				}
			} else if (i == 1) {
				if (value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT) {
					parameters.normal.strength = value;
				} else {
					valid = false;
				}
			} else {
				Vector2 xy;
				if (value.get_type() == Variant::VECTOR3) {
					const Vector3 xyz = value;
					xy = Vector2(xyz.x, xyz.y);
				} else if (value.get_type() == Variant::VECTOR2) {
					xy = value;
				} else {
					valid = false;
				}
				if (i == 2) {
					parameters.normal.uv_scale *= xy;
				} else {
					parameters.normal.uv_offset += xy;
				}
			}
		}
		valid &= !Math::is_nan(parameters.normal.strength) && !Math::is_inf(parameters.normal.strength);
		for (int axis = 0; axis < 2; axis++) {
			valid &= !Math::is_nan(parameters.normal.uv_scale[axis]) && !Math::is_inf(parameters.normal.uv_scale[axis]) && !Math::is_nan(parameters.normal.uv_offset[axis]) && !Math::is_inf(parameters.normal.uv_offset[axis]);
		}
		if (!valid) {
			// Bad optional inputs never discard the supported lit albedo material.
			parameters.normal = FixedFunctionSpatialMaterial::FFNormalMapChannel();
		}
	}
	if (!p_fallback) {
		_resolve_optional_channels(*material, *shader, parameters);
	}
	r_parameters = parameters;
	return OK;
}

Variant FixedFunctionMaterialStorage::_spatial_parameter(const FFMaterial &p_material, const FFShader &p_shader, const StringName &p_name) {
	const RBMap<StringName, Variant>::Element *parameter = p_material.parameters.find(p_name);
	if (!parameter) {
		parameter = p_shader.uniform_defaults.find(p_name);
	}
	return parameter ? parameter->get() : Variant();
}

bool FixedFunctionMaterialStorage::_resolve_spatial_channel(const FFMaterial &p_material, const FFShader &p_shader, const FixedFunctionSpatialMaterial::FFChannelRecipe &p_recipe, FixedFunctionSpatialMaterial::FFMaterialChannel &r_channel, bool p_fallback) {
	r_channel.factor = p_recipe.factor;
	r_channel.uv_scale = p_recipe.uv.scale;
	r_channel.uv_offset = p_recipe.uv.offset;
	r_channel.uv_set = p_recipe.uv.set;
	r_channel.sampler_flags = p_recipe.sampler_flags;
	const StringName names[] = { p_recipe.factor_parameter, p_recipe.uv.scale_parameter, p_recipe.uv.offset_parameter, p_recipe.texture_parameter };
	for (int i = 0; i < 4; i++) {
		if (names[i] == StringName()) {
			continue;
		}
		const Variant value = _spatial_parameter(p_material, p_shader, names[i]);
		if (i == 0) {
			Color color;
			if (value.get_type() == Variant::COLOR) {
				color = value;
			} else if (value.get_type() == Variant::VECTOR4) {
				const Vector4 vector = value;
				color = Color(vector.x, vector.y, vector.z, vector.w);
			} else if (value.get_type() == Variant::PLANE) {
				const Plane plane = value;
				color = Color(plane.normal.x, plane.normal.y, plane.normal.z, plane.d);
			} else if (value.get_type() == Variant::QUATERNION) {
				const Quaternion quat = value;
				color = Color(quat.x, quat.y, quat.z, quat.w);
			} else {
				if (p_fallback) {
					continue;
				}
				return false;
			}
			r_channel.factor *= color;
		} else if (i == 1 || i == 2) {
			Vector2 xy;
			if (value.get_type() == Variant::VECTOR3) {
				const Vector3 vector = value;
				xy = Vector2(vector.x, vector.y);
			} else if (value.get_type() == Variant::VECTOR2) {
				xy = value;
			} else {
				if (p_fallback) {
					continue;
				}
				return false;
			}
			if (i == 1) {
				r_channel.uv_scale *= xy;
			} else {
				r_channel.uv_offset += xy;
			}
		} else {
			if (value.get_type() == Variant::RID) {
				r_channel.texture = value;
			} else if (value.get_type() != Variant::NIL) {
				if (p_fallback) {
					continue;
				}
				return false;
			}
			if (!r_channel.texture.is_valid()) {
				const RBMap<Pair<StringName, int>, RID>::Element *texture = p_shader.default_textures.find(Pair<StringName, int>(names[i], 0));
				if (texture) {
					r_channel.texture = texture->get();
				}
			}
		}
	}
	for (int i = 0; i < 4; i++) {
		if (Math::is_nan(r_channel.factor[i]) || Math::is_inf(r_channel.factor[i])) {
			if (p_fallback) {
				r_channel.factor = Color(1, 0, 1);
				break;
			}
			return false;
		}
	}
	for (int i = 0; i < 2; i++) {
		if (Math::is_nan(r_channel.uv_scale[i]) || Math::is_inf(r_channel.uv_scale[i]) || Math::is_nan(r_channel.uv_offset[i]) || Math::is_inf(r_channel.uv_offset[i])) {
			if (p_fallback) {
				r_channel.uv_scale = Vector2(1, 1);
				r_channel.uv_offset = Vector2();
				break;
			}
			return false;
		}
	}
	return true;
}

void FixedFunctionMaterialStorage::_resolve_optional_channels(const FFMaterial &p_material, const FFShader &p_shader, FixedFunctionSpatialMaterial::FFMaterialParameters &r_parameters) {
	const FixedFunctionSpatialMaterial::FFEmissionRecipe &emission = p_shader.spatial_recipe.emission;
	float energy = emission.energy;
	if (emission.energy_parameter != StringName()) {
		const Variant value = _spatial_parameter(p_material, p_shader, emission.energy_parameter);
		energy = 0;
		if (value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT) {
			energy = value;
		}
	}
	if (energy > 0 && !Math::is_inf(energy)) {
		FixedFunctionSpatialMaterial::FFMaterialChannel color;
		if (_resolve_spatial_channel(p_material, p_shader, emission.color, color)) {
			const bool missing_texture = emission.color.texture_parameter != StringName() && !color.texture.is_valid();
			if (!missing_texture || emission.additive || !emission.color.texture_black_default) {
				if (missing_texture && emission.additive && !emission.color.texture_black_default) {
					color.factor += Color(1, 1, 1, 0);
				}
				r_parameters.emission.color = color;
				r_parameters.emission.energy = energy;
				r_parameters.emission.additive = emission.additive;
			}
		}
	}

	const FixedFunctionSpatialMaterial::FFOcclusionRecipe &occlusion = p_shader.spatial_recipe.occlusion;
	if (occlusion.texture_parameter == StringName()) {
		return;
	}
	FixedFunctionSpatialMaterial::FFChannelRecipe sample;
	sample.uv = occlusion.uv;
	sample.texture_parameter = occlusion.texture_parameter;
	sample.factor = occlusion.weights;
	sample.factor_parameter = occlusion.weights_parameter;
	FixedFunctionSpatialMaterial::FFMaterialChannel channel;
	if (!_resolve_spatial_channel(p_material, p_shader, sample, channel) || !channel.texture.is_valid()) {
		return;
	}
	float light_affect = occlusion.light_affect;
	if (occlusion.light_affect_parameter != StringName()) {
		const Variant value = _spatial_parameter(p_material, p_shader, occlusion.light_affect_parameter);
		if (value.get_type() != Variant::FLOAT && value.get_type() != Variant::INT) {
			return;
		}
		light_affect = value;
	}
	if (Math::is_inf(light_affect) || Math::is_nan(light_affect)) {
		return;
	}
	r_parameters.occlusion.uv_scale = channel.uv_scale;
	r_parameters.occlusion.uv_offset = channel.uv_offset;
	r_parameters.occlusion.uv_set = channel.uv_set;
	r_parameters.occlusion.weights = channel.factor;
	r_parameters.occlusion.texture = channel.texture;
	r_parameters.occlusion.sampler_flags = occlusion.sampler_flags;
	r_parameters.occlusion.light_affect = CLAMP(light_affect, 0.0f, 1.0f);
}
