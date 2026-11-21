#include "renderer_material_storage_fixed_function.h"

#include "core/config/project_settings.h"

void RendererMaterialStorageFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP) {
		ERR_PRINT(String("Fixed Function RendererMaterialStorage ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

void RendererMaterialStorageFixedFunction::_unsupported(const char *p_operation) {
	ERR_PRINT(String("Fixed Function RendererMaterialStorage ") + p_operation + " is unavailable.");
}

void RendererMaterialStorageFixedFunction::global_shader_parameter_add(const StringName &p_name, RSE::GlobalShaderParameterType p_type, const Variant &p_value) {
	_report_error(ERR_UNAVAILABLE, "global_shader_parameter_add");
}

void RendererMaterialStorageFixedFunction::global_shader_parameter_remove(const StringName &p_name) {
	_report_error(ERR_UNAVAILABLE, "global_shader_parameter_remove");
}

Vector<StringName> RendererMaterialStorageFixedFunction::global_shader_parameter_get_list() const {
	// No global can be added in this profile. Listing that empty set is a valid query;
	// declarations/settings and writes still report their unsupported status.
	return Vector<StringName>();
}

void RendererMaterialStorageFixedFunction::global_shader_parameter_set(const StringName &p_name, const Variant &p_value) {
	_report_error(ERR_UNAVAILABLE, "global_shader_parameter_set");
}

void RendererMaterialStorageFixedFunction::global_shader_parameter_set_override(const StringName &p_name, const Variant &p_value) {
	_report_error(ERR_UNAVAILABLE, "global_shader_parameter_set_override");
}

Variant RendererMaterialStorageFixedFunction::global_shader_parameter_get(const StringName &p_name) const {
	_unsupported("global_shader_parameter_get");
	return Variant();
}

RSE::GlobalShaderParameterType RendererMaterialStorageFixedFunction::global_shader_parameter_get_type(const StringName &p_name) const {
	return RSE::GLOBAL_VAR_TYPE_MAX;
}

void RendererMaterialStorageFixedFunction::global_shader_parameters_load_settings(bool p_load_textures) {
	List<PropertyInfo> properties;
	ProjectSettings::get_singleton()->get_property_list(&properties);
	for (const PropertyInfo &property : properties) {
		if (property.name.begins_with("shader_globals/")) {
			_report_error(ERR_UNAVAILABLE, "global shader uniforms");
			return;
		}
	}
	_last_error = OK;
}

void RendererMaterialStorageFixedFunction::global_shader_parameters_clear() {
	_last_error = OK;
}

int32_t RendererMaterialStorageFixedFunction::global_shader_parameters_instance_allocate(RID p_instance) {
	_report_error(ERR_UNAVAILABLE, "instance shader uniforms");
	return -1;
}

void RendererMaterialStorageFixedFunction::global_shader_parameters_instance_free(RID p_instance) {
	_last_error = OK; // No instance uniform allocation can succeed.
}

void RendererMaterialStorageFixedFunction::global_shader_parameters_instance_update(RID p_instance, int p_index, const Variant &p_value, int p_flags_count) {
	_report_error(ERR_UNAVAILABLE, "global_shader_parameters_instance_update");
}

RID RendererMaterialStorageFixedFunction::shader_allocate() {
	return _materials.shader_create();
}

void RendererMaterialStorageFixedFunction::shader_initialize(RID p_rid, bool p_embedded) {
	if (_materials.owns_shader(p_rid)) {
		if (p_embedded) {
			_embedded.insert(p_rid);
		}
		_last_error = OK;
	} else {
		_report_error(ERR_INVALID_PARAMETER, "shader_initialize");
	}
}

void RendererMaterialStorageFixedFunction::shader_free(RID p_rid) {
	_embedded.erase(p_rid);
	_paths.erase(p_rid);
	_report_error(_materials.free(p_rid), "shader_free");
}

void RendererMaterialStorageFixedFunction::shader_set_code(RID p_shader, const String &p_code) {
	_report_error(_materials.shader_set_code(p_shader, p_code), "shader_set_code");
}

void RendererMaterialStorageFixedFunction::shader_set_path_hint(RID p_shader, const String &p_path) {
	if (_materials.owns_shader(p_shader)) {
		_paths[p_shader] = p_path;
		_last_error = OK;
	} else {
		_report_error(ERR_INVALID_PARAMETER, "shader_set_path_hint");
	}
}

String RendererMaterialStorageFixedFunction::shader_get_code(RID p_shader) const {
	return _materials.shader_get_code(p_shader);
}

void RendererMaterialStorageFixedFunction::get_shader_parameter_list(RID p_shader, List<PropertyInfo> *p_param_list) const {
	const Error error = _materials.shader_get_param_list(p_shader, p_param_list);
	if (error != OK) {
		_unsupported("get_shader_parameter_list");
	}
}

void RendererMaterialStorageFixedFunction::shader_set_default_texture_parameter(RID p_shader, const StringName &p_name, RID p_texture, int p_index) {
	Error error = ERR_INVALID_PARAMETER;
	if (p_index >= 0 && (!p_texture.is_valid() || _textures.texture_can_be_referenced(p_texture))) {
		error = _materials.shader_set_default_texture_param(p_shader, p_name, p_texture, p_index);
	}
	_report_error(error, "shader_set_default_texture_parameter");
}

RID RendererMaterialStorageFixedFunction::shader_get_default_texture_parameter(RID p_shader, const StringName &p_name, int p_index) const {
	return p_index >= 0 ? _materials.shader_get_default_texture_param(p_shader, p_name, p_index) : RID();
}

Variant RendererMaterialStorageFixedFunction::shader_get_parameter_default(RID p_material, const StringName &p_param) const {
	return _materials.shader_get_param_default(p_material, p_param);
}

RenderingServerTypes::ShaderNativeSourceCode RendererMaterialStorageFixedFunction::shader_get_native_source_code(RID p_shader) const {
	_unsupported("shader_get_native_source_code");
	return RenderingServerTypes::ShaderNativeSourceCode();
}

void RendererMaterialStorageFixedFunction::shader_embedded_set_lock() {
	_last_error = OK; // All calls are confined to the owning render thread.
}

const HashSet<RID> &RendererMaterialStorageFixedFunction::shader_embedded_set_get() const {
	return _embedded;
}

void RendererMaterialStorageFixedFunction::shader_embedded_set_unlock() {
	_last_error = OK;
}

RID RendererMaterialStorageFixedFunction::material_allocate() {
	return _materials.material_create();
}

void RendererMaterialStorageFixedFunction::material_initialize(RID p_rid) {
	_report_error(_materials.owns_material(p_rid) ? OK : ERR_INVALID_PARAMETER, "material_initialize");
}

void RendererMaterialStorageFixedFunction::material_free(RID p_rid) {
	_report_error(_materials.free(p_rid), "material_free");
}

void RendererMaterialStorageFixedFunction::material_set_render_priority(RID p_material, int priority) {
	_report_error(_materials.material_set_render_priority(p_material, priority), "material_set_render_priority");
}

void RendererMaterialStorageFixedFunction::material_set_shader(RID p_shader_material, RID p_shader) {
	_report_error(_materials.material_set_shader(p_shader_material, p_shader), "material_set_shader");
}

void RendererMaterialStorageFixedFunction::material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) {
	_report_error(_materials.material_set_param(p_material, p_param, p_value), "material_set_param");
}

Variant RendererMaterialStorageFixedFunction::material_get_param(RID p_material, const StringName &p_param) const {
	return _materials.material_get_param(p_material, p_param);
}

void RendererMaterialStorageFixedFunction::material_set_next_pass(RID p_material, RID p_next_material) {
	_report_error(_materials.material_set_next_pass(p_material, p_next_material), "material_set_next_pass");
}

bool RendererMaterialStorageFixedFunction::material_is_animated(RID p_material) {
	FixedFunctionShaderUsage::FFFeatures features;
	return _materials.material_get_features(p_material, features) == OK && features.animated;
}

bool RendererMaterialStorageFixedFunction::material_casts_shadows(RID p_material) {
	FixedFunctionShaderUsage::FFFeatures features;
	return _materials.material_get_features(p_material, features) == OK && features.casts_shadows;
}

RSE::CullMode RendererMaterialStorageFixedFunction::material_get_cull_mode(RID p_material) const {
	FixedFunctionShaderUsage::FFFeatures features;
	return _materials.material_get_features(p_material, features, false) == OK ? features.cull_mode : RSE::CULL_MODE_DISABLED;
}

void RendererMaterialStorageFixedFunction::material_get_instance_shader_parameters(RID p_material, List<InstanceShaderParam> *r_parameters) {
	_report_error(_materials.material_get_instance_shader_parameters(p_material, r_parameters), "material_get_instance_shader_parameters");
}

void RendererMaterialStorageFixedFunction::material_update_dependency(RID p_material, DependencyTracker *p_instance) {
	_report_error(_materials.material_update_dependency(p_material, p_instance), "material_update_dependency");
}

RendererMaterialStorageFixedFunction::RendererMaterialStorageFixedFunction(FixedFunctionRenderContext &p_context) : _materials(p_context.get_material_storage()), _textures(p_context.get_texture_storage()), _last_error(OK) {}
