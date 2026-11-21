#include "material_storage.h"

#include "core/math/math_funcs.h"
#include "servers/rendering/shader_language.h"
#include "servers/rendering/shader_types.h"

#include <float.h>

FixedFunctionMaterialStorage::FFShader::FFShader() :
		canvas_error(ERR_UNCONFIGURED),
		parse_error(OK),
		spatial_error(ERR_UNCONFIGURED),
		blend_mode(FixedFunctionGraphicsDevice::BLEND_MODE_MIX) {
}

FixedFunctionMaterialStorage::FFMaterial::FFMaterial() :
		render_priority(0),
		free_pending(false) {
}

FixedFunctionMaterialStorage::FFNotifyScope::FFNotifyScope(bool &p_notifying) :
		_notifying(p_notifying) {
	_notifying = true;
}
FixedFunctionMaterialStorage::FFNotifyScope::~FFNotifyScope() {
	_notifying = false;
}

FixedFunctionMaterialStorage::FFShader *FixedFunctionMaterialStorage::_get_shader(RID p_shader) const {
	return _shader_owner.owns(p_shader) ? _shader_owner.get_or_null(p_shader) : nullptr;
}

FixedFunctionMaterialStorage::FFMaterial *FixedFunctionMaterialStorage::_get_material(RID p_material) const {
	return _material_owner.owns(p_material) ? _material_owner.get_or_null(p_material) : nullptr;
}

RID FixedFunctionMaterialStorage::shader_create() {
	if (_clearing || _clear_pending) {
		return RID();
	}
	FFShader *shader = memnew(FFShader);
	shader->self = _shader_owner.make_rid(shader);
	_shaders.push_back(shader);
	return shader->self;
}

Error FixedFunctionMaterialStorage::_parse_shader(const String &p_code, ShaderLanguage &r_language) {
	if (p_code.is_empty()) {
		return ERR_UNCONFIGURED;
	}
	const String type = ShaderLanguage::get_shader_type(p_code);
	RSE::ShaderMode mode;
	if (type == "canvas_item") {
		mode = RSE::SHADER_CANVAS_ITEM;
	} else if (type == "spatial") {
		mode = RSE::SHADER_SPATIAL;
	} else if (type == "particles") {
		mode = RSE::SHADER_PARTICLES;
	} else if (type == "sky") {
		mode = RSE::SHADER_SKY;
	} else if (type == "fog") {
		mode = RSE::SHADER_FOG;
	} else if (type == "texture_blit") {
		mode = RSE::SHADER_TEXTURE_BLIT;
	} else {
		return ERR_PARSE_ERROR;
	}
	ShaderTypes *types = ShaderTypes::get_singleton();
	if (!types) {
		return ERR_UNCONFIGURED;
	}
	// Use Godot's parser rather than matching source text or recognizing resource comments.
	ShaderLanguage::ShaderCompileInfo info;
	info.functions = types->get_functions(mode);
	info.render_modes = types->get_modes(mode);
	info.stencil_modes = types->get_stencil_modes(mode);
	info.shader_types = types->get_types();
	return r_language.compile(p_code, info);
}

Error FixedFunctionMaterialStorage::_lower_canvas_shader(const String &p_code, FixedFunctionGraphicsDevice::FFBlendMode &r_blend_mode) {
	if (!p_code.is_empty() && ShaderLanguage::get_shader_type(p_code) != "canvas_item") {
		return ERR_UNAVAILABLE;
	}
	ShaderLanguage language;
	const Error error = _parse_shader(p_code, language);
	if (error != OK) {
		return error;
	}
	const ShaderLanguage::ShaderNode *shader = language.get_shader();
	if (!shader->functions.is_empty() || !shader->uniforms.is_empty() || !shader->varyings.is_empty() || !shader->constants.is_empty() || !shader->structs.is_empty() || !shader->stencil_modes.is_empty() || shader->stencil_reference >= 0) {
		return ERR_UNAVAILABLE;
	}
	FixedFunctionGraphicsDevice::FFBlendMode blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
	for (int i = 0; i < shader->render_modes.size(); i++) {
		const String mode = shader->render_modes[i];
		if (mode == "blend_mix") {
			blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		} else if (mode == "blend_add") {
			blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_ADD;
		} else if (mode == "blend_mul") {
			blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MUL;
		} else if (mode == "blend_premul_alpha") {
			blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_PREMULTIPLIED_ALPHA;
		} else if (mode == "blend_disabled") {
			blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
		} else if (mode != "unshaded") {
			return ERR_UNAVAILABLE;
		}
	}
	r_blend_mode = blend_mode;
	return OK;
}

Error FixedFunctionMaterialStorage::shader_set_code(RID p_shader, const String &p_code) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFShader *shader = _get_shader(p_shader);
	if (!shader) {
		return ERR_INVALID_PARAMETER;
	}
	shader->code = p_code;
	shader->canvas_error = _lower_canvas_shader(p_code, shader->blend_mode);
	shader->parse_error = _analyze_shader(*shader);
	shader->spatial_error = _lower_spatial_shader(p_code, *shader);
	_dependencies_dirty = true;
	return OK;
}

Error FixedFunctionMaterialStorage::_analyze_shader(FFShader &r_shader) {
	r_shader.instance_parameters.clear();
	FixedFunctionShaderUsage::FFFeatures features;
	if (!r_shader.code.is_empty()) {
		ShaderLanguage language;
		const Error error = _parse_shader(r_shader.code, language);
		if (error != OK) {
			return error;
		}
		const ShaderLanguage::ShaderNode &shader = *language.get_shader();
		if (ShaderLanguage::get_shader_type(r_shader.code) == "spatial") {
			features = FixedFunctionShaderUsage::analyze_spatial(shader);
		}
		// Dirty scene instances query reflection. Save the metadata while the source is already parsed.
		for (const KeyValue<StringName, ShaderLanguage::ShaderNode::Uniform> &E : shader.uniforms) {
			if (E.value.scope != ShaderLanguage::ShaderNode::Uniform::SCOPE_INSTANCE) {
				continue;
			}
			RendererMaterialStorage::InstanceShaderParam parameter;
			parameter.info = ShaderLanguage::uniform_to_property_info(E.value);
			parameter.info.name = E.key;
			parameter.index = E.value.instance_index;
			parameter.default_value = ShaderLanguage::constant_value_to_variant(E.value.default_value, E.value.type, E.value.array_size, E.value.hint);
			r_shader.instance_parameters.push_back(parameter);
		}
	}
	r_shader.features = features;
	return OK;
}

String FixedFunctionMaterialStorage::shader_get_code(RID p_shader) const {
	const FFShader *shader = _get_shader(p_shader);
	ERR_FAIL_COND_V(!shader, String());
	return shader->code;
}

Error FixedFunctionMaterialStorage::shader_get_param_list(RID p_shader, List<PropertyInfo> *r_parameters) const {
	const FFShader *shader = _get_shader(p_shader);
	if (!shader || !r_parameters) {
		return ERR_INVALID_PARAMETER;
	}
	if (shader->code.is_empty()) {
		return OK;
	}
	ShaderLanguage language;
	const Error error = _parse_shader(shader->code, language);
	if (error != OK) {
		return error;
	}
	const ShaderLanguage::ShaderNode *parsed = language.get_shader();
	RBMap<int, Pair<StringName, ShaderLanguage::ShaderNode::Uniform>> ordered;
	for (const KeyValue<StringName, ShaderLanguage::ShaderNode::Uniform> &E : parsed->uniforms) {
		if (E.value.scope == ShaderLanguage::ShaderNode::Uniform::SCOPE_LOCAL) {
			ordered[E.value.prop_order] = Pair<StringName, ShaderLanguage::ShaderNode::Uniform>(E.key, E.value);
		}
	}
	String group;
	for (const RBMap<int, Pair<StringName, ShaderLanguage::ShaderNode::Uniform>>::Element *E = ordered.front(); E; E = E->next()) {
		const ShaderLanguage::ShaderNode::Uniform &uniform = E->get().second;
		if (uniform.group != group) {
			PropertyInfo header;
			header.usage = PROPERTY_USAGE_GROUP;
			header.name = uniform.group;
			r_parameters->push_back(header);
			group = uniform.group;
		}
		PropertyInfo info = ShaderLanguage::uniform_to_property_info(uniform);
		info.name = E->get().first;
		r_parameters->push_back(info);
	}
	return OK;
}

Error FixedFunctionMaterialStorage::shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture, int p_index) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFShader *shader = _get_shader(p_shader);
	if (!shader) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_texture.is_valid()) {
		shader->default_textures[Pair<StringName, int>(p_name, p_index)] = p_texture;
	} else {
		shader->default_textures.erase(Pair<StringName, int>(p_name, p_index));
	}
	_dependencies_dirty = true;
	return OK;
}

RID FixedFunctionMaterialStorage::shader_get_default_texture_param(RID p_shader, const StringName &p_name, int p_index) const {
	const FFShader *shader = _get_shader(p_shader);
	ERR_FAIL_COND_V(!shader, RID());
	const RBMap<Pair<StringName, int>, RID>::Element *texture = shader->default_textures.find(Pair<StringName, int>(p_name, p_index));
	return texture ? texture->get() : RID();
}

Error FixedFunctionMaterialStorage::shader_add_custom_define(RID p_shader, const String &p_define) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFShader *shader = _get_shader(p_shader);
	if (!shader) {
		return ERR_INVALID_PARAMETER;
	}
	shader->custom_defines.push_back(p_define);
	_dependencies_dirty = true;
	return OK;
}

Error FixedFunctionMaterialStorage::shader_get_custom_defines(RID p_shader, Vector<String> *r_defines) const {
	const FFShader *shader = _get_shader(p_shader);
	if (!shader || !r_defines) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < shader->custom_defines.size(); i++) {
		r_defines->push_back(shader->custom_defines[i]);
	}
	return OK;
}

Error FixedFunctionMaterialStorage::shader_remove_custom_define(RID p_shader, const String &p_define) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFShader *shader = _get_shader(p_shader);
	if (!shader) {
		return ERR_INVALID_PARAMETER;
	}
	shader->custom_defines.erase(p_define);
	_dependencies_dirty = true;
	return OK;
}

Error FixedFunctionMaterialStorage::set_shader_async_hidden_forbidden(bool p_forbidden) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	_shader_async_hidden_forbidden = p_forbidden;
	return OK;
}

RID FixedFunctionMaterialStorage::material_create() {
	if (_clearing || _clear_pending) {
		return RID();
	}
	FFMaterial *material = memnew(FFMaterial);
	material->self = _material_owner.make_rid(material);
	_materials.push_back(material);
	return material->self;
}

Error FixedFunctionMaterialStorage::material_set_render_priority(RID p_material, int p_priority) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = _get_material(p_material);
	if (!material || material->free_pending || p_priority < RSE::MATERIAL_RENDER_PRIORITY_MIN || p_priority > RSE::MATERIAL_RENDER_PRIORITY_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	material->render_priority = static_cast<int8_t>(p_priority);
	_dependencies_dirty = true;
	return OK;
}

int FixedFunctionMaterialStorage::material_get_render_priority(RID p_material) const {
	const FFMaterial *material = _get_material(p_material);
	ERR_FAIL_COND_V(!material, 0);
	return material->render_priority;
}

Error FixedFunctionMaterialStorage::material_set_line_width(RID p_material, float p_width) {
	if (Math::is_nan(p_width) || Math::is_inf(p_width)) {
		return ERR_INVALID_PARAMETER;
	}
	return material_set_param(p_material, "fixed_function_line_width", p_width);
}

float FixedFunctionMaterialStorage::material_get_line_width(RID p_material) const {
	const FFMaterial *material = _get_material(p_material);
	if (!material) {
		return 1.0f;
	}
	const RBMap<StringName, Variant>::Element *parameter = material->parameters.find("fixed_function_line_width");
	if (!parameter) {
		const FFShader *shader = _get_shader(material->shader);
		if (shader) {
			parameter = shader->uniform_defaults.find("fixed_function_line_width");
		}
	}
	if (!parameter || (parameter->get().get_type() != Variant::FLOAT && parameter->get().get_type() != Variant::INT)) {
		return 1.0f;
	}
	const double width = parameter->get();
	// Invalid optional metadata must not hide otherwise drawable geometry; preserve its CPU value.
	return Math::is_nan(width) || Math::is_inf(width) ? 1.0f : float(CLAMP(width, -double(FLT_MAX), double(FLT_MAX)));
}

Error FixedFunctionMaterialStorage::material_set_next_pass(RID p_material, RID p_next_material) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = _get_material(p_material);
	if (!material || material->free_pending) {
		return ERR_INVALID_PARAMETER;
	}
	RID cursor = p_next_material;
	int budget = _materials.size();
	while (cursor.is_valid()) {
		if (cursor == p_material || budget-- <= 0) {
			return ERR_CYCLIC_LINK;
		}
		const FFMaterial *pass = _get_material(cursor);
		if (!pass || pass->free_pending) {
			return ERR_INVALID_PARAMETER;
		}
		cursor = pass->next_pass;
	}
	material->next_pass = p_next_material;
	_dependencies_dirty = true;
	return OK;
}

RID FixedFunctionMaterialStorage::material_get_next_pass(RID p_material) const {
	const FFMaterial *material = _get_material(p_material);
	ERR_FAIL_COND_V(!material, RID());
	return material->next_pass;
}

Error FixedFunctionMaterialStorage::material_set_shader(RID p_material, RID p_shader) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = _get_material(p_material);
	if (!material || material->free_pending || (p_shader.is_valid() && !_shader_owner.owns(p_shader))) {
		return ERR_INVALID_PARAMETER;
	}
	material->shader = p_shader;
	_dependencies_dirty = true;
	return OK;
}

RID FixedFunctionMaterialStorage::material_get_shader(RID p_material) const {
	const FFMaterial *material = _get_material(p_material);
	ERR_FAIL_COND_V(!material, RID());
	return material->shader;
}

Error FixedFunctionMaterialStorage::material_set_param(RID p_material, const StringName &p_name, const Variant &p_value) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = _get_material(p_material);
	if (!material || material->free_pending) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_value.get_type() == Variant::NIL) {
		material->parameters.erase(p_name);
	} else {
		material->parameters[p_name] = p_value;
	}
	_dependencies_dirty = true;
	return OK;
}

Variant FixedFunctionMaterialStorage::material_get_param(RID p_material, const StringName &p_name) const {
	const FFMaterial *material = _get_material(p_material);
	ERR_FAIL_COND_V(!material, Variant());
	const RBMap<StringName, Variant>::Element *parameter = material->parameters.find(p_name);
	return parameter ? parameter->get() : material_get_param_default(p_material, p_name);
}

Variant FixedFunctionMaterialStorage::material_get_param_default(RID p_material, const StringName &p_name) const {
	const FFMaterial *material = _get_material(p_material);
	ERR_FAIL_NULL_V(material, Variant());
	return material->shader.is_valid() ? shader_get_param_default(material->shader, p_name) : Variant();
}

Variant FixedFunctionMaterialStorage::shader_get_param_default(RID p_shader, const StringName &p_name) const {
	const FFShader *shader = _get_shader(p_shader);
	ERR_FAIL_NULL_V(shader, Variant());
	if (shader->code.is_empty()) {
		return Variant();
	}
	ShaderLanguage language;
	if (_parse_shader(shader->code, language) != OK) {
		return Variant();
	}
	const ShaderLanguage::ShaderNode::Uniform *uniform = language.get_shader()->uniforms.getptr(p_name);
	return uniform ? ShaderLanguage::constant_value_to_variant(uniform->default_value, uniform->type, uniform->array_size, uniform->hint) : Variant();
}

Error FixedFunctionMaterialStorage::material_get_features(RID p_material, FixedFunctionShaderUsage::FFFeatures &r_features, bool p_include_next_pass) const {
	RID cursor = p_material;
	int budget = _materials.size();
	FixedFunctionShaderUsage::FFFeatures result;
	bool first = true;
	do {
		if (budget-- <= 0) {
			return ERR_CYCLIC_LINK;
		}
		const FFMaterial *material = _get_material(cursor);
		if (!material) {
			return ERR_INVALID_PARAMETER;
		}
		if (material->shader.is_valid()) {
			const FFShader *shader = _get_shader(material->shader);
			if (!shader) {
				return ERR_BUG;
			}
			if (!shader->custom_defines.is_empty()) {
				return ERR_UNAVAILABLE;
			}
			if (shader->parse_error != OK) {
				return shader->parse_error;
			}
			result.animated |= shader->features.animated;
			result.casts_shadows |= shader->features.casts_shadows;
			if (first) {
				result.cull_mode = shader->features.cull_mode;
				result.uses_tangents = shader->features.uses_tangents;
				result.ensure_correct_normals = shader->features.ensure_correct_normals;
			}
		}
		first = false;
		cursor = p_include_next_pass ? material->next_pass : RID();
	} while (cursor.is_valid());
	r_features = result;
	return OK;
}

Error FixedFunctionMaterialStorage::material_update_dependency(RID p_material, DependencyTracker *p_tracker) {
	FFMaterial *material = _get_material(p_material);
	if (_clearing || !material || material->free_pending || !p_tracker) {
		return ERR_INVALID_PARAMETER;
	}
	p_tracker->update_dependency(&material->dependency);
	return OK;
}

Error FixedFunctionMaterialStorage::material_add_geometry_owner(RID *p_slot, bool *p_dirty) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = p_slot ? _get_material(*p_slot) : nullptr;
	if (!material || material->free_pending || !p_dirty) {
		return ERR_INVALID_PARAMETER;
	}
	if (material->geometry_owners.has(p_slot)) {
		return ERR_ALREADY_IN_USE;
	}
	material->geometry_owners[p_slot] = p_dirty;
	*p_dirty = true;
	return OK;
}

Error FixedFunctionMaterialStorage::material_remove_geometry_owner(RID *p_slot) {
	if (!p_slot) {
		return ERR_INVALID_PARAMETER;
	}
	if (!p_slot->is_valid()) {
		return OK; // Material release already detached this slot.
	}
	FFMaterial *material = _get_material(*p_slot);
	if (!material || !material->geometry_owners.has(p_slot)) {
		return ERR_INVALID_PARAMETER;
	}
	material->geometry_owners.erase(p_slot);
	return OK;
}

Error FixedFunctionMaterialStorage::update_dependencies() {
	if (_notifying || _clearing) {
		return ERR_BUSY;
	}
	if (!_dependencies_dirty) {
		return OK;
	}
	{
		FFNotifyScope scope(_notifying);
		_dependencies_dirty = false;
		Vector<RID> pending;
		for (const List<FFMaterial *>::Element *E = _materials.front(); E; E = E->next()) {
			pending.push_back(E->get()->self);
		}
		for (int i = 0; i < pending.size(); i++) {
			FFMaterial *material = _get_material(pending[i]);
			if (material && !material->free_pending) {
				for (RBMap<RID *, bool *>::Element *G = material->geometry_owners.front(); G; G = G->next()) {
					*G->get() = true;
				}
				// Use the native tracker contract: callbacks schedule dependency rebuilding.
				// Defer record deletion until native notification iteration has returned.
				material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
			}
		}
	}
	if (_clear_pending) {
		clear();
		return OK;
	}
	Vector<RID> retired;
	for (const List<FFMaterial *>::Element *E = _materials.front(); E; E = E->next()) {
		if (E->get()->free_pending) {
			retired.push_back(E->get()->self);
		}
	}
	for (int i = 0; i < retired.size(); i++) {
		FFMaterial *material = _get_material(retired[i]);
		if (material) {
			_free_material(material);
		}
	}
	return _dependencies_dirty ? ERR_SKIP : OK;
}

Error FixedFunctionMaterialStorage::material_get_canvas_parameters(RID p_material, FixedFunctionGraphicsDevice::FFDrawParameters &r_parameters) const {
	const FFMaterial *material = _get_material(p_material);
	if (!material) {
		return ERR_INVALID_PARAMETER;
	}
	FixedFunctionGraphicsDevice::FFBlendMode blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
	if (material->shader.is_valid()) {
		const FFShader *shader = _get_shader(material->shader);
		if (!shader) {
			return ERR_BUG;
		}
		if (shader->canvas_error != OK) {
			return shader->canvas_error;
		}
		if (!shader->custom_defines.is_empty()) {
			return ERR_UNAVAILABLE;
		}
		blend_mode = shader->blend_mode;
	}
	r_parameters.blend_mode = blend_mode;
	return OK;
}

void FixedFunctionMaterialStorage::_free_shader(FFShader *p_shader) {
	// No cached native state or record pointers escape. Detach dependents before deleting a RID.
	for (List<FFMaterial *>::Element *E = _materials.front(); E; E = E->next()) {
		if (E->get()->shader == p_shader->self) {
			E->get()->shader = RID();
		}
	}
	_shader_owner.free(p_shader->self);
	_dependencies_dirty = true;
	_shaders.erase(p_shader);
	memdelete(p_shader);
}

void FixedFunctionMaterialStorage::_free_material(FFMaterial *p_material) {
	for (RBMap<RID *, bool *>::Element *E = p_material->geometry_owners.front(); E; E = E->next()) {
		*E->key() = RID();
		*E->get() = true;
	}
	p_material->geometry_owners.clear();
	for (List<FFMaterial *>::Element *E = _materials.front(); E; E = E->next()) {
		if (E->get()->next_pass == p_material->self) {
			E->get()->next_pass = RID();
		}
	}
	_material_owner.free(p_material->self);
	_dependencies_dirty = true;
	_materials.erase(p_material);
	p_material->dependency.deleted_notify(p_material->self);
	memdelete(p_material);
}

Error FixedFunctionMaterialStorage::free(RID p_rid) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFShader *shader = _get_shader(p_rid);
	if (shader) {
		_free_shader(shader);
		return OK;
	}
	FFMaterial *material = _get_material(p_rid);
	if (!material) {
		return ERR_INVALID_PARAMETER;
	}
	if (material->free_pending) {
		return ERR_ALREADY_IN_USE;
	}
	if (_notifying) {
		material->free_pending = true;
	} else {
		_free_material(material);
	}
	return OK;
}

void FixedFunctionMaterialStorage::clear() {
	if (_clearing) {
		return;
	}
	if (_notifying) {
		_clear_pending = true;
		for (List<FFMaterial *>::Element *E = _materials.front(); E; E = E->next()) {
			E->get()->free_pending = true;
		}
		return;
	}
	_clear_pending = false;
	_clearing = true;
	while (_materials.front()) {
		_free_material(_materials.front()->get());
	}
	while (_shaders.front()) {
		_free_shader(_shaders.front()->get());
	}
	_dependencies_dirty = false;
	_clearing = false;
}

FixedFunctionMaterialStorage::FixedFunctionMaterialStorage() :
		_clearing(false),
		_shader_async_hidden_forbidden(false),
		_dependencies_dirty(false),
		_notifying(false),
		_clear_pending(false) {
}

FixedFunctionMaterialStorage::~FixedFunctionMaterialStorage() {
	clear();
}

Error FixedFunctionMaterialStorage::material_get_instance_shader_parameters(RID p_material, List<RendererMaterialStorage::InstanceShaderParam> *r_parameters) const {
	if (!r_parameters) {
		return ERR_INVALID_PARAMETER;
	}
	List<RendererMaterialStorage::InstanceShaderParam> parameters;
	RID cursor = p_material;
	int budget = _materials.size();
	do {
		if (budget-- <= 0) {
			return ERR_CYCLIC_LINK;
		}
		const FFMaterial *material = _get_material(cursor);
		if (!material) {
			return ERR_INVALID_PARAMETER;
		}
		if (material->shader.is_valid()) {
			const FFShader *shader = _get_shader(material->shader);
			if (!shader) {
				return ERR_BUG;
			}
			if (shader->parse_error != OK) {
				return shader->parse_error;
			}
			for (const RendererMaterialStorage::InstanceShaderParam &parameter : shader->instance_parameters) {
				parameters.push_back(parameter);
			}
		}
		cursor = material->next_pass;
	} while (cursor.is_valid());
	for (const RendererMaterialStorage::InstanceShaderParam &parameter : parameters) {
		r_parameters->push_back(parameter);
	}
	return OK;
}
