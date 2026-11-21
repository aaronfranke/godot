#pragma once

#include "core/templates/rb_map.h"
#include "core/templates/rid_owner.h"
#include "core/variant/variant.h"
#include "drivers/fixed_function/graphics_device.h"
#include "drivers/fixed_function/shader_usage.h"
#include "drivers/fixed_function/spatial_material.h"
#include "servers/rendering/storage/material_storage.h"
#include "servers/rendering/storage/utilities.h"

class ShaderLanguage;

// CPU resource ownership behind the emitted shader-code/named-parameter boundary.
// All operations stay on the renderer's owning thread; no native device calls occur here.
// Discard owned RIDs after free/clear/destruction, as with other engine resource owners.
class FixedFunctionMaterialStorage {
	struct FFShader {
		RBMap<StringName, Variant> uniform_defaults;
		RBMap<Pair<StringName, int>, RID> default_textures;
		RID self;
		String code;
		Error canvas_error;
		Error parse_error;
		FixedFunctionSpatialMaterial::FFShaderRecipe spatial_recipe;
		List<RendererMaterialStorage::InstanceShaderParam> instance_parameters;
		Vector<String> custom_defines;
		FixedFunctionShaderUsage::FFFeatures features;
		Error spatial_error;
		FixedFunctionGraphicsDevice::FFBlendMode blend_mode;

		FFShader();
	};

	struct FFMaterial {
		Dependency dependency;
		RBMap<RID *, bool *> geometry_owners;
		RID self;
		RID shader;
		RID next_pass;
		// Place the 12-byte x86_32 map after the RIDs to avoid an internal alignment hole.
		RBMap<StringName, Variant> parameters;
		int8_t render_priority;
		bool free_pending;

		FFMaterial();
	};
	class FFNotifyScope {
		bool &_notifying;
		FFNotifyScope(const FFNotifyScope &);
		FFNotifyScope &operator=(const FFNotifyScope &);

	public:
		FFNotifyScope(bool &p_notifying);
		~FFNotifyScope();
	};

	// RID lookup lacks a const API; preserve read-only getters until const lookup is available.
	mutable RID_PtrOwner<FFShader> _shader_owner;
	mutable RID_PtrOwner<FFMaterial> _material_owner;
	List<FFShader *> _shaders;
	List<FFMaterial *> _materials;
	bool _clearing;
	bool _shader_async_hidden_forbidden;
	bool _dependencies_dirty;
	bool _notifying;
	bool _clear_pending;

	FFShader *_get_shader(RID p_shader) const;
	FFMaterial *_get_material(RID p_material) const;
	static Error _parse_shader(const String &p_code, ShaderLanguage &r_language);
	static Error _lower_canvas_shader(const String &p_code, FixedFunctionGraphicsDevice::FFBlendMode &r_blend_mode);
	static Error _analyze_shader(FFShader &r_shader);
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
	// Retain latest code even when it cannot be lowered. Strict queries retain diagnostic status.
	Error shader_set_code(RID p_shader, const String &p_code);
	String shader_get_code(RID p_shader) const;
	// Append CPU reflection in this branch's property order. Parse errors leave output unchanged.
	Variant shader_get_param_default(RID p_shader, const StringName &p_name) const;
	Error shader_get_param_list(RID p_shader, List<PropertyInfo> *r_parameters) const;
	// Texture RIDs are borrowed metadata; the facade validates the owning texture store.
	Error shader_set_default_texture_param(RID p_shader, const StringName &p_name, RID p_texture, int p_index = 0);
	RID shader_get_default_texture_param(RID p_shader, const StringName &p_name, int p_index = 0) const;
	Error shader_add_custom_define(RID p_shader, const String &p_define);
	Error shader_get_custom_defines(RID p_shader, Vector<String> *r_defines) const;
	Error shader_remove_custom_define(RID p_shader, const String &p_define);
	Error set_shader_async_hidden_forbidden(bool p_forbidden);
	bool is_shader_async_hidden_forbidden() const { return _shader_async_hidden_forbidden; }
	RID material_create();
	bool owns_material(RID p_material) const { return _material_owner.owns(p_material); }
	Error material_set_render_priority(RID p_material, int p_priority);
	int material_get_render_priority(RID p_material) const;
	// 4.x exposes this renderer option through named material parameters instead of a server line-width API.
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
	Error material_update_dependency(RID p_material, DependencyTracker *p_tracker);
	Error material_add_geometry_owner(RID *p_slot, bool *p_dirty);
	Error material_remove_geometry_owner(RID *p_slot);
	Error material_get_instance_shader_parameters(RID p_material, List<RendererMaterialStorage::InstanceShaderParam> *r_parameters) const;
	Error update_dependencies();
	// Resolve a supported parameterless canvas declaration; never execute user shader code.
	// Failure leaves the caller's complete draw state unchanged.
	Error material_get_canvas_parameters(RID p_material, FixedFunctionGraphicsDevice::FFDrawParameters &r_parameters) const;
	Error material_get_spatial_parameters(RID p_material, FixedFunctionSpatialMaterial::FFMaterialParameters &r_parameters) const;
	Error material_get_spatial_draw_parameters(RID p_material, FixedFunctionSpatialMaterial::FFMaterialParameters &r_parameters) const;
	Error free(RID p_rid);
	void clear();
	int get_shader_count() const { return _shaders.size(); }
	int get_material_count() const { return _materials.size(); }

	FixedFunctionMaterialStorage();
	~FixedFunctionMaterialStorage();
};
