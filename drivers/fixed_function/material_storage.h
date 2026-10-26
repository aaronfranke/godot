#ifndef FIXED_FUNCTION_MATERIAL_STORAGE_H
#define FIXED_FUNCTION_MATERIAL_STORAGE_H

#include "graphics_device.h"
#include "shader_usage.h"
#include "spatial_material.h"

#include "core/map.h"
#include "core/variant.h"
#include "servers/visual/rasterizer.h"

class ShaderLanguage;

// CPU resource ownership behind the emitted shader-code/named-parameter boundary.
// All operations stay on the renderer's owning thread; no native device calls occur here.
// Discard owned RIDs after free/clear/destruction, as with other 3.x resource owners.
class FixedFunctionMaterialStorage {
	struct FFShader : RID_Data {
		// Map/RID alignment and the following errors avoid holes in x86_32 RID/real_t variants.
		Map<StringName, Variant> uniform_defaults;
		Map<StringName, Variant> parameter_defaults;
		List<PropertyInfo> parameter_properties;
		RID self;
		Error canvas_error;
		Error features_error;
		Error reflection_error;
		Error spatial_error;
		FixedFunctionSpatialMaterial::FFShaderRecipe spatial_recipe;
		Map<StringName, RID> default_textures;
		Vector<String> custom_defines;
		String code;
		FixedFunctionShaderUsage::FFFeatures features;
		FixedFunctionGraphicsDevice::FFBlendMode blend_mode;

		FFShader();
	};

	struct FFMaterial : RID_Data {
		Map<StringName, Variant> parameters;
		Map<RasterizerScene::InstanceBase *, uint32_t> instance_owners;
		Map<RID *, bool *> geometry_owners;
		RID self;
		RID shader;
		RID next_pass;
		float line_width;
		int8_t render_priority;

		FFMaterial();
	};
	struct FFDependencyRegistration {
		uint64_t generation;
		uint32_t references;
		FFDependencyRegistration();
	};
	struct FFDependencyEvent {
		uint64_t generation;
		RasterizerScene::InstanceBase *instance;
	};
	class FFNotifyScope {
		bool &_notifying;
		FFNotifyScope(const FFNotifyScope &);
		FFNotifyScope &operator=(const FFNotifyScope &);

	public:
		FFNotifyScope(bool &p_notifying);
		~FFNotifyScope();
	};

	// Align the generation before pointer-sized containers to avoid an x86_32 hole.
	uint64_t _next_dependency_generation;
	Map<RasterizerScene::InstanceBase *, FFDependencyRegistration> _dependencies;
	// RID lookup lacks a const API; preserve read-only getters until const lookup is available.
	mutable RID_Owner<FFShader> _shader_owner;
	mutable RID_Owner<FFMaterial> _material_owner;
	List<FFShader *> _shaders;
	List<FFMaterial *> _materials;
	bool _clearing;
	bool _shader_async_hidden_forbidden;
	bool _dependencies_dirty;
	bool _notifying;

	FFShader *_get_shader(RID p_shader) const;
	FFMaterial *_get_material(RID p_material) const;
	static Error _parse_shader(const String &p_code, ShaderLanguage &r_language);
	static Error _lower_canvas_shader(const String &p_code, FixedFunctionGraphicsDevice::FFBlendMode &r_blend_mode);
	static Error _analyze_shader(FFShader &r_shader);
	static Error _collect_shader_parameters(ShaderLanguage &p_language, List<PropertyInfo> &r_parameters);
	static Error _lower_spatial_shader(const String &p_code, FFShader &r_shader);
	Error _material_get_spatial_parameters(RID p_material, FixedFunctionSpatialMaterial::FFMaterialParameters &r_parameters, bool p_fallback) const;
	static Variant _spatial_parameter(const FFMaterial &p_material, const FFShader &p_shader, const StringName &p_name);
	static bool _resolve_spatial_channel(const FFMaterial &p_material, const FFShader &p_shader, const FixedFunctionSpatialMaterial::FFChannelRecipe &p_recipe, FixedFunctionSpatialMaterial::FFMaterialChannel &r_channel, bool p_fallback = false);
	static void _resolve_optional_channels(const FFMaterial &p_material, const FFShader &p_shader, FixedFunctionSpatialMaterial::FFMaterialParameters &r_parameters);
	void _free_shader(FFShader *p_shader);
	void _free_material(FFMaterial *p_material);
	FixedFunctionMaterialStorage(const FixedFunctionMaterialStorage &);
	FixedFunctionMaterialStorage &operator=(const FixedFunctionMaterialStorage &);

public:
	RID shader_create();
	bool owns_shader(RID p_shader) const { return _shader_owner.owns(p_shader); }
	// Retain latest code even when it cannot be lowered. Draw queries return its status.
	Error shader_set_code(RID p_shader, const String &p_code);
	String shader_get_code(RID p_shader) const;
	// Append CPU reflection in Godot's scalar-then-texture order. Errors leave output unchanged.
	Error shader_get_param_list(RID p_shader, List<PropertyInfo> *r_parameters) const;
	// Texture RIDs are borrowed metadata; the facade validates the owning texture store.
	Error shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture);
	RID shader_get_default_texture_param(RID p_shader, const StringName &p_name) const;
	Error shader_add_custom_define(RID p_shader, const String &p_define);
	Error shader_get_custom_defines(RID p_shader, Vector<String> *r_defines) const;
	Error shader_remove_custom_define(RID p_shader, const String &p_define);
	Error set_shader_async_hidden_forbidden(bool p_forbidden);
	bool is_shader_async_hidden_forbidden() const { return _shader_async_hidden_forbidden; }
	RID material_create();
	bool owns_material(RID p_material) const { return _material_owner.owns(p_material); }
	Error material_set_render_priority(RID p_material, int p_priority);
	int material_get_render_priority(RID p_material) const;
	Error material_set_line_width(RID p_material, float p_width);
	float material_get_line_width(RID p_material) const;
	Error material_set_next_pass(RID p_material, RID p_next_material);
	RID material_get_next_pass(RID p_material) const;
	Error material_set_shader(RID p_material, RID p_shader);
	RID material_get_shader(RID p_material) const;
	Error material_set_param(RID p_material, const StringName &p_name, const Variant &p_value);
	Variant material_get_param(RID p_material, const StringName &p_name) const;
	Variant material_get_param_default(RID p_material, const StringName &p_name) const;
	Error material_get_features(RID p_material, FixedFunctionShaderUsage::FFFeatures &r_features, bool p_include_next_pass = true) const;
	Error material_add_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance);
	Error material_remove_instance_owner(RID p_material, RasterizerScene::InstanceBase *p_instance);
	// Geometry owns stable material slots and dirty flags, unregistering before destruction.
	// Free clears slots immediately; updates coalesce flags without invoking geometry callbacks.
	Error material_add_geometry_owner(RID *p_slot, bool *p_dirty);
	Error material_remove_geometry_owner(RID *p_slot);
	// Coalesced material-only invalidation. Registration generations protect callback snapshots.
	Error update_dependencies();
	// Resolve a supported parameterless canvas declaration; never execute user shader code.
	// Failure leaves the caller's complete draw state unchanged.
	Error material_get_canvas_parameters(RID p_material, FixedFunctionGraphicsDevice::FFDrawParameters &r_parameters) const;
	Error material_get_spatial_parameters(RID p_material, FixedFunctionSpatialMaterial::FFMaterialParameters &r_parameters) const;
	// Drawing always gets an opaque fallback for unsupported programs; strict queries stay diagnostic.
	Error material_get_spatial_draw_parameters(RID p_material, FixedFunctionSpatialMaterial::FFMaterialParameters &r_parameters) const;
	Error free(RID p_rid);
	void clear();
	int get_shader_count() const { return _shaders.size(); }
	int get_material_count() const { return _materials.size(); }

	FixedFunctionMaterialStorage();
	~FixedFunctionMaterialStorage();
};

#endif // FIXED_FUNCTION_MATERIAL_STORAGE_H
