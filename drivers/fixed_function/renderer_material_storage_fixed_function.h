#pragma once

#include "drivers/fixed_function/render_context.h"
#include "servers/rendering/storage/material_storage.h"

// Explicit restricted-profile contracts; unsupported resources have no owners or handles.
class RendererMaterialStorageFixedFunction : public RendererMaterialStorage {
	FixedFunctionMaterialStorage &_materials;
	FixedFunctionTextureStorage &_textures;
	HashSet<RID> _embedded;
	HashMap<RID, String> _paths;
	Error _last_error;
	void _report_error(Error p_error, const char *p_operation);
	static void _unsupported(const char *p_operation);

public:
	Error get_last_error() const { return _last_error; }
	RendererMaterialStorageFixedFunction(FixedFunctionRenderContext &p_context);
	void global_shader_parameter_add(const StringName &p_name, RSE::GlobalShaderParameterType p_type, const Variant &p_value) override;
	void global_shader_parameter_remove(const StringName &p_name) override;
	Vector<StringName> global_shader_parameter_get_list() const override;
	void global_shader_parameter_set(const StringName &p_name, const Variant &p_value) override;
	void global_shader_parameter_set_override(const StringName &p_name, const Variant &p_value) override;
	Variant global_shader_parameter_get(const StringName &p_name) const override;
	RSE::GlobalShaderParameterType global_shader_parameter_get_type(const StringName &p_name) const override;
	void global_shader_parameters_load_settings(bool p_load_textures = true) override;
	void global_shader_parameters_clear() override;
	int32_t global_shader_parameters_instance_allocate(RID p_instance) override;
	void global_shader_parameters_instance_free(RID p_instance) override;
	void global_shader_parameters_instance_update(RID p_instance, int p_index, const Variant &p_value, int p_flags_count = 0) override;
	RID shader_allocate() override;
	void shader_initialize(RID p_rid, bool p_embedded = true) override;
	void shader_free(RID p_rid) override;
	void shader_set_code(RID p_shader, const String &p_code) override;
	void shader_set_path_hint(RID p_shader, const String &p_path) override;
	String shader_get_code(RID p_shader) const override;
	void get_shader_parameter_list(RID p_shader, List<PropertyInfo> *p_param_list) const override;
	void shader_set_default_texture_parameter(RID p_shader, const StringName &p_name, RID p_texture, int p_index) override;
	RID shader_get_default_texture_parameter(RID p_shader, const StringName &p_name, int p_index) const override;
	Variant shader_get_parameter_default(RID p_material, const StringName &p_param) const override;
	RenderingServerTypes::ShaderNativeSourceCode shader_get_native_source_code(RID p_shader) const override;
	void shader_embedded_set_lock() override;
	const HashSet<RID> &shader_embedded_set_get() const override;
	void shader_embedded_set_unlock() override;
	RID material_allocate() override;
	void material_initialize(RID p_rid) override;
	void material_free(RID p_rid) override;
	void material_set_render_priority(RID p_material, int priority) override;
	void material_set_shader(RID p_shader_material, RID p_shader) override;
	void material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) override;
	Variant material_get_param(RID p_material, const StringName &p_param) const override;
	void material_set_next_pass(RID p_material, RID p_next_material) override;
	bool material_is_animated(RID p_material) override;
	bool material_casts_shadows(RID p_material) override;
	RSE::CullMode material_get_cull_mode(RID p_material) const override;
	void material_get_instance_shader_parameters(RID p_material, List<InstanceShaderParam> *r_parameters) override;
	void material_update_dependency(RID p_material, DependencyTracker *p_instance) override;
};
