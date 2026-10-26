#include "material_storage.h"

#include "core/math/math_funcs.h"
#include "servers/visual/shader_language.h"
#include "servers/visual/shader_types.h"

FixedFunctionMaterialStorage::FFShader::FFShader() :
		canvas_error(ERR_UNCONFIGURED),
		features_error(OK),
		reflection_error(OK),
		spatial_error(ERR_UNCONFIGURED),
		blend_mode(FixedFunctionGraphicsDevice::BLEND_MODE_MIX) {
}

FixedFunctionMaterialStorage::FFMaterial::FFMaterial() :
		line_width(1.0f),
		render_priority(0) {
}

FixedFunctionMaterialStorage::FFDependencyRegistration::FFDependencyRegistration() :
		generation(0), references(0) {}
FixedFunctionMaterialStorage::FFNotifyScope::FFNotifyScope(bool &p_notifying) :
		_notifying(p_notifying) { _notifying = true; }
FixedFunctionMaterialStorage::FFNotifyScope::~FFNotifyScope() { _notifying = false; }

FixedFunctionMaterialStorage::FFShader *FixedFunctionMaterialStorage::_get_shader(RID p_shader) const {
	return _shader_owner.owns(p_shader) ? _shader_owner.getptr(p_shader) : nullptr;
}

FixedFunctionMaterialStorage::FFMaterial *FixedFunctionMaterialStorage::_get_material(RID p_material) const {
	return _material_owner.owns(p_material) ? _material_owner.getptr(p_material) : nullptr;
}

RID FixedFunctionMaterialStorage::shader_create() {
	if (_clearing) {
		return RID();
	}
	FFShader *shader = memnew(FFShader);
	shader->self = _shader_owner.make_rid(shader);
	_shaders.push_back(shader);
	return shader->self;
}

Error FixedFunctionMaterialStorage::_parse_shader(const String &p_code, ShaderLanguage &r_language) {
	if (p_code.empty()) {
		return ERR_UNCONFIGURED;
	}
	const String type = ShaderLanguage::get_shader_type(p_code);
	VS::ShaderMode mode;
	if (type == "canvas_item") {
		mode = VS::SHADER_CANVAS_ITEM;
	} else if (type == "spatial") {
		mode = VS::SHADER_SPATIAL;
	} else if (type == "particles") {
		mode = VS::SHADER_PARTICLES;
	} else {
		return ERR_PARSE_ERROR;
	}
	ShaderTypes *types = ShaderTypes::get_singleton();
	if (!types) {
		return ERR_UNCONFIGURED;
	}
	// Use Godot's parser rather than matching source text or recognizing resource comments.
	return r_language.compile(p_code, types->get_functions(mode), types->get_modes(mode), types->get_types());
}

Error FixedFunctionMaterialStorage::_lower_canvas_shader(const String &p_code, FixedFunctionGraphicsDevice::FFBlendMode &r_blend_mode) {
	if (!p_code.empty() && ShaderLanguage::get_shader_type(p_code) != "canvas_item") {
		return ERR_UNAVAILABLE;
	}
	ShaderLanguage language;
	const Error error = _parse_shader(p_code, language);
	if (error != OK) {
		return error;
	}
	const ShaderLanguage::ShaderNode *shader = language.get_shader();
	if (!shader->functions.empty() || !shader->uniforms.empty() || !shader->varyings.empty() || !shader->constants.empty() || !shader->structs.empty()) {
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
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFShader *shader = _get_shader(p_shader);
	if (!shader) {
		return ERR_INVALID_PARAMETER;
	}
	shader->code = p_code;
	shader->canvas_error = _lower_canvas_shader(p_code, shader->blend_mode);
	shader->features_error = _analyze_shader(*shader);
	shader->spatial_error = _lower_spatial_shader(p_code, *shader);
	_dependencies_dirty = true;
	return OK;
}

Error FixedFunctionMaterialStorage::_analyze_shader(FFShader &r_shader) {
	r_shader.parameter_properties.clear();
	r_shader.parameter_defaults.clear();
	r_shader.reflection_error = OK;
	FixedFunctionShaderUsage::FFFeatures features;
	if (!r_shader.code.empty()) {
		ShaderLanguage language;
		const Error error = _parse_shader(r_shader.code, language);
		if (error != OK) {
			r_shader.reflection_error = error;
			return error;
		}
		const ShaderLanguage::ShaderNode &shader = *language.get_shader();
		if (ShaderLanguage::get_shader_type(r_shader.code) == "spatial") {
			features = FixedFunctionShaderUsage::analyze_spatial(shader);
		}
		// Store CPU reflection with the source revision, even when lowering is unavailable.
		for (OrderedHashMap<StringName, ShaderLanguage::ShaderNode::Uniform>::ConstElement E = shader.uniforms.front(); E; E = E.next()) {
			const ShaderLanguage::ShaderNode::Uniform &uniform = E.get();
			const Variant value = ShaderLanguage::constant_value_to_variant(uniform.default_value, uniform.type, uniform.hint);
			if (value.get_type() != Variant::NIL) {
				r_shader.parameter_defaults[E.key()] = value;
			}
		}
		r_shader.reflection_error = _collect_shader_parameters(language, r_shader.parameter_properties);
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
	if (shader->reflection_error != OK) {
		return shader->reflection_error;
	}
	for (const List<PropertyInfo>::Element *E = shader->parameter_properties.front(); E; E = E->next()) {
		r_parameters->push_back(E->get());
	}
	return OK;
}

Error FixedFunctionMaterialStorage::_collect_shader_parameters(ShaderLanguage &p_language, List<PropertyInfo> &r_parameters) {
	const ShaderLanguage::ShaderNode *parsed = p_language.get_shader();
	List<PropertyInfo> parameters;
	// Parser insertion order preserves declaration order within each of these two groups.
	for (int textures = 0; textures < 2; textures++) {
		for (OrderedHashMap<StringName, ShaderLanguage::ShaderNode::Uniform>::ConstElement E = parsed->uniforms.front(); E; E = E.next()) {
			const ShaderLanguage::ShaderNode::Uniform &uniform = E.get();
			if ((uniform.texture_order >= 0) != bool(textures)) {
				continue;
			}
			PropertyInfo info;
			info.name = E.key();
			switch (uniform.type) {
				case ShaderLanguage::TYPE_STRUCT:
					info.type = Variant::ARRAY;
					break;
				case ShaderLanguage::TYPE_VOID:
					info.type = Variant::NIL;
					break;
				case ShaderLanguage::TYPE_BOOL:
					info.type = Variant::BOOL;
					break;
				case ShaderLanguage::TYPE_BVEC2:
					info.type = Variant::INT;
					info.hint = PROPERTY_HINT_FLAGS;
					info.hint_string = "x,y";
					break;
				case ShaderLanguage::TYPE_BVEC3:
					info.type = Variant::INT;
					info.hint = PROPERTY_HINT_FLAGS;
					info.hint_string = "x,y,z";
					break;
				case ShaderLanguage::TYPE_BVEC4:
					info.type = Variant::INT;
					info.hint = PROPERTY_HINT_FLAGS;
					info.hint_string = "x,y,z,w";
					break;
				case ShaderLanguage::TYPE_INT:
				case ShaderLanguage::TYPE_UINT:
					info.type = Variant::INT;
					break;
				case ShaderLanguage::TYPE_IVEC2:
				case ShaderLanguage::TYPE_IVEC3:
				case ShaderLanguage::TYPE_IVEC4:
				case ShaderLanguage::TYPE_UVEC2:
				case ShaderLanguage::TYPE_UVEC3:
				case ShaderLanguage::TYPE_UVEC4:
					info.type = Variant::POOL_INT_ARRAY;
					break;
				case ShaderLanguage::TYPE_FLOAT:
					info.type = Variant::REAL;
					break;
				case ShaderLanguage::TYPE_VEC2:
					info.type = Variant::VECTOR2;
					break;
				case ShaderLanguage::TYPE_VEC3:
					info.type = Variant::VECTOR3;
					break;
				case ShaderLanguage::TYPE_VEC4:
					info.type = uniform.hint == ShaderLanguage::ShaderNode::Uniform::HINT_COLOR ? Variant::COLOR : Variant::PLANE;
					break;
				case ShaderLanguage::TYPE_MAT2:
					info.type = Variant::TRANSFORM2D;
					break;
				case ShaderLanguage::TYPE_MAT3:
					info.type = Variant::BASIS;
					break;
				case ShaderLanguage::TYPE_MAT4:
					info.type = Variant::TRANSFORM;
					break;
				case ShaderLanguage::TYPE_SAMPLER2D:
				case ShaderLanguage::TYPE_SAMPLEREXT:
				case ShaderLanguage::TYPE_ISAMPLER2D:
				case ShaderLanguage::TYPE_USAMPLER2D:
				case ShaderLanguage::TYPE_SAMPLER2DARRAY:
				case ShaderLanguage::TYPE_ISAMPLER2DARRAY:
				case ShaderLanguage::TYPE_USAMPLER2DARRAY:
				case ShaderLanguage::TYPE_SAMPLER3D:
				case ShaderLanguage::TYPE_ISAMPLER3D:
				case ShaderLanguage::TYPE_USAMPLER3D:
				case ShaderLanguage::TYPE_SAMPLERCUBE:
					info.type = Variant::OBJECT;
					info.hint = PROPERTY_HINT_RESOURCE_TYPE;
					if (uniform.type == ShaderLanguage::TYPE_SAMPLERCUBE) {
						info.hint_string = "CubeMap";
					} else if (uniform.type >= ShaderLanguage::TYPE_SAMPLER2DARRAY && uniform.type <= ShaderLanguage::TYPE_USAMPLER2DARRAY) {
						info.hint_string = "TextureArray";
					} else if (uniform.type >= ShaderLanguage::TYPE_SAMPLER3D && uniform.type <= ShaderLanguage::TYPE_USAMPLER3D) {
						info.hint_string = "Texture3D";
					} else {
						info.hint_string = "Texture";
					}
					break;
				default:
					return ERR_UNAVAILABLE;
			}
			if ((uniform.type == ShaderLanguage::TYPE_INT || uniform.type == ShaderLanguage::TYPE_UINT || uniform.type == ShaderLanguage::TYPE_FLOAT) && uniform.hint == ShaderLanguage::ShaderNode::Uniform::HINT_RANGE) {
				info.hint = PROPERTY_HINT_RANGE;
				info.hint_string = rtos(uniform.hint_range[0]) + "," + rtos(uniform.hint_range[1]) + "," + rtos(uniform.hint_range[2]);
			}
			parameters.push_back(info);
		}
	}
	r_parameters = parameters;
	return OK;
}

Error FixedFunctionMaterialStorage::shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFShader *shader = _get_shader(p_shader);
	if (!shader) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_texture.is_valid()) {
		shader->default_textures[p_name] = p_texture;
	} else {
		shader->default_textures.erase(p_name);
	}
	_dependencies_dirty = true;
	return OK;
}

RID FixedFunctionMaterialStorage::shader_get_default_texture_param(RID p_shader, const StringName &p_name) const {
	const FFShader *shader = _get_shader(p_shader);
	ERR_FAIL_COND_V(!shader, RID());
	const Map<StringName, RID>::Element *texture = shader->default_textures.find(p_name);
	return texture ? texture->get() : RID();
}

Error FixedFunctionMaterialStorage::shader_add_custom_define(RID p_shader, const String &p_define) {
	if (_clearing) {
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
	if (_clearing) {
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
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	_shader_async_hidden_forbidden = p_forbidden;
	return OK;
}

RID FixedFunctionMaterialStorage::material_create() {
	if (_clearing) {
		return RID();
	}
	FFMaterial *material = memnew(FFMaterial);
	material->self = _material_owner.make_rid(material);
	_materials.push_back(material);
	return material->self;
}

Error FixedFunctionMaterialStorage::material_set_render_priority(RID p_material, int p_priority) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = _get_material(p_material);
	if (!material || p_priority < VS::MATERIAL_RENDER_PRIORITY_MIN || p_priority > VS::MATERIAL_RENDER_PRIORITY_MAX) {
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
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = _get_material(p_material);
	if (!material || Math::is_nan(p_width) || Math::is_inf(p_width)) {
		return ERR_INVALID_PARAMETER;
	}
	// Retain finite API values; the spatial draw path applies its minimum/coverage bound.
	material->line_width = p_width;
	_dependencies_dirty = true;
	return OK;
}

float FixedFunctionMaterialStorage::material_get_line_width(RID p_material) const {
	const FFMaterial *material = _get_material(p_material);
	ERR_FAIL_COND_V(!material, 1.0f);
	return material->line_width;
}

Error FixedFunctionMaterialStorage::material_set_next_pass(RID p_material, RID p_next_material) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = _get_material(p_material);
	if (!material) {
		return ERR_INVALID_PARAMETER;
	}
	RID cursor = p_next_material;
	int budget = _materials.size();
	while (cursor.is_valid()) {
		if (cursor == p_material || budget-- <= 0) {
			return ERR_CYCLIC_LINK;
		}
		const FFMaterial *pass = _get_material(cursor);
		if (!pass) {
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
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = _get_material(p_material);
	if (!material || (p_shader.is_valid() && !_shader_owner.owns(p_shader))) {
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
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = _get_material(p_material);
	if (!material) {
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
	const Map<StringName, Variant>::Element *parameter = material->parameters.find(p_name);
	return parameter ? parameter->get() : material_get_param_default(p_material, p_name);
}

Variant FixedFunctionMaterialStorage::material_get_param_default(RID p_material, const StringName &p_name) const {
	const FFMaterial *material = _get_material(p_material);
	ERR_FAIL_COND_V(!material, Variant());
	const FFShader *shader = _get_shader(material->shader);
	if (!shader) {
		return Variant();
	}
	const Map<StringName, Variant>::Element *value = shader->parameter_defaults.find(p_name);
	return value ? value->get() : Variant();
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
			if (!shader->custom_defines.empty()) {
				return ERR_UNAVAILABLE;
			}
			if (shader->features_error != OK) {
				return shader->features_error;
			}
			result.animated |= shader->features.animated;
			result.casts_shadows |= shader->features.casts_shadows;
			if (first) {
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

Error FixedFunctionMaterialStorage::material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = _get_material(p_material);
	if (!material || !p_instance) {
		return ERR_INVALID_PARAMETER;
	}
	Map<RasterizerScene::InstanceBase *, FFDependencyRegistration>::Element *registration = _dependencies.find(p_instance);
	const Map<RasterizerScene::InstanceBase *, uint32_t>::Element *existing = material->instance_owners.find(p_instance);
	if ((existing && existing->get() == UINT32_MAX) || (registration && registration->get().references == UINT32_MAX)) {
		return ERR_CANT_CREATE;
	}
	if (!registration) {
		if (_next_dependency_generation == UINT64_MAX) {
			return ERR_CANT_CREATE;
		}
		FFDependencyRegistration fresh;
		fresh.generation = ++_next_dependency_generation;
		_dependencies[p_instance] = fresh;
		registration = _dependencies.find(p_instance);
	}
	uint32_t &references = material->instance_owners[p_instance];
	references++;
	registration->get().references++;
	_dependencies_dirty = true;
	return OK;
}

Error FixedFunctionMaterialStorage::material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance) {
	// Unregistration stays available during clear so destruction can retire other owners safely.
	FFMaterial *material = _get_material(p_material);
	if (!material || !p_instance) {
		return ERR_INVALID_PARAMETER;
	}
	Map<RasterizerScene::InstanceBase *, uint32_t>::Element *owner = material->instance_owners.find(p_instance);
	Map<RasterizerScene::InstanceBase *, FFDependencyRegistration>::Element *registration = _dependencies.find(p_instance);
	if (!owner || !registration) {
		return ERR_INVALID_PARAMETER;
	}
	if (--owner->get() == 0) {
		material->instance_owners.erase(owner);
	}
	if (--registration->get().references == 0) {
		_dependencies.erase(registration);
	}
	return OK;
}

Error FixedFunctionMaterialStorage::material_add_geometry_owner(RID *p_slot, bool *p_dirty) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	FFMaterial *material = p_slot ? _get_material(*p_slot) : nullptr;
	if (!material || !p_dirty) {
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
	FFNotifyScope scope(_notifying);
	_dependencies_dirty = false;
	for (List<FFMaterial *>::Element *E = _materials.front(); E; E = E->next()) {
		for (Map<RID *, bool *>::Element *G = E->get()->geometry_owners.front(); G; G = G->next()) {
			*G->get() = true;
		}
	}
	Vector<FFDependencyEvent> pending;
	for (Map<RasterizerScene::InstanceBase *, FFDependencyRegistration>::Element *E = _dependencies.front(); E; E = E->next()) {
		FFDependencyEvent event = { E->get().generation, E->key() };
		pending.push_back(event);
	}
	for (int i = 0; i < pending.size(); i++) {
		const FFDependencyEvent event = pending[i];
		const Map<RasterizerScene::InstanceBase *, FFDependencyRegistration>::Element *current = _dependencies.find(event.instance);
		if (current && current->get().generation == event.generation) {
			event.instance->base_changed(false, true);
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
		if (!shader->custom_defines.empty()) {
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
	while (p_material->geometry_owners.front()) {
		Map<RID *, bool *>::Element *owner = p_material->geometry_owners.front();
		*owner->key() = RID();
		*owner->get() = true;
		p_material->geometry_owners.erase(owner);
	}
	// Match the 3.x boundary: clear material slots, never remove the instance's geometry base.
	while (p_material->instance_owners.front()) {
		RasterizerScene::InstanceBase *instance = p_material->instance_owners.front()->key();
		if (instance->material_override == p_material->self) {
			instance->material_override = RID();
		}
		if (instance->material_overlay == p_material->self) {
			instance->material_overlay = RID();
		}
		for (int i = 0; i < instance->materials.size(); i++) {
			if (instance->materials[i] == p_material->self) {
				instance->materials.write[i] = RID();
			}
		}
		const uint32_t references = p_material->instance_owners.front()->get();
		FFDependencyRegistration &registration = _dependencies[instance];
		registration.references -= references;
		if (!registration.references) {
			_dependencies.erase(instance);
		}
		p_material->instance_owners.erase(instance);
	}
	for (List<FFMaterial *>::Element *E = _materials.front(); E; E = E->next()) {
		if (E->get()->next_pass == p_material->self) {
			E->get()->next_pass = RID();
		}
	}
	_material_owner.free(p_material->self);
	_dependencies_dirty = true;
	_materials.erase(p_material);
	memdelete(p_material);
}

Error FixedFunctionMaterialStorage::free(RID p_rid) {
	if (_clearing) {
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
	_free_material(material);
	return OK;
}

void FixedFunctionMaterialStorage::clear() {
	if (_clearing) {
		return;
	}
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
		_next_dependency_generation(0),
		_clearing(false),
		_shader_async_hidden_forbidden(false),
		_dependencies_dirty(false),
		_notifying(false) {
}

FixedFunctionMaterialStorage::~FixedFunctionMaterialStorage() {
	clear();
}
