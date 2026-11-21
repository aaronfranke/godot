#pragma once

#include "core/math/projection.h"
#include "core/templates/hash_set.h"
#include "core/templates/rid_owner.h"
#include "servers/rendering/rendering_server_types.h"
#include "servers/rendering/storage/utilities.h"

// CPU snapshots retain native Godot properties independently of executed features.
class FixedFunctionLightStorage {
public:
	enum FFLightFlag {
		LIGHT_SHADOW = 1,
		LIGHT_NEGATIVE = 2,
		LIGHT_REVERSE_CULL = 4,
		LIGHT_BLEND_SPLITS = 16,
	};
	struct FFLightData {
		uint64_t version;
		RID projector;
		Color color;
		float parameters[RSE::LIGHT_PARAM_MAX];
		float distance_fade_begin;
		float distance_fade_shadow;
		float distance_fade_length;
		uint32_t cull_mask;
		uint32_t shadow_caster_mask;
		uint32_t max_sdfgi_cascade;
		RSE::LightType type;
		RSE::LightBakeMode bake_mode;
		RSE::LightOmniShadowMode omni_shadow_mode;
		RSE::LightDirectionalShadowMode directional_shadow_mode;
		RSE::LightDirectionalSkyMode sky_mode;
		uint8_t flags;
		bool distance_fade_enabled;
		bool allow_contact_shadows;
		FFLightData();
	};
	struct FFShadowTransform {
		Projection projection;
		Transform3D transform;
		float texel_size;
		float bias_scale;
		bool valid;
		FFShadowTransform() : texel_size(0), bias_scale(1), valid(false) {}
	};
	struct FFLightInstanceData {
		Transform3D transform;
		RID light;
	};

private:
	struct FFLight;
	struct FFLightInstance {
		FFShadowTransform shadow;
		Transform3D transform;
		RID self;
		FFLight *light;
		FFLightInstance();
	};
	struct FFLight {
		FFLightData data;
		Dependency dependency;
		HashSet<FFLightInstance *> instances;
		RID self;
		bool initialized;
		bool dirty;
		bool free_pending;
		FFLight();
	};
	// RID lookup lacks a const API; preserve read-only queries until one exists.
	mutable RID_PtrOwner<FFLight> _light_owner;
	mutable RID_PtrOwner<FFLightInstance> _instance_owner;
	List<FFLight *> _lights;
	List<FFLightInstance *> _instances;
	bool _clearing;
	bool _notifying;
	bool _clear_pending;
	FFLight *_get_light(RID p_light) const;
	FFLightInstance *_get_instance(RID p_instance) const;
	static bool _finite_color(const Color &p_color);
	void _free_light(FFLight *p_light);
	void _free_instance(FFLightInstance *p_instance);
	void _retire_pending();
	FixedFunctionLightStorage(const FixedFunctionLightStorage &);
	FixedFunctionLightStorage &operator=(const FixedFunctionLightStorage &);

public:
	RID light_allocate();
	Error light_initialize(RID p_light, RSE::LightType p_type);
	RID light_create(RSE::LightType p_type);
	bool owns_light(RID p_light) const { return _light_owner.owns(p_light); }
	Error light_get_data(RID p_light, FFLightData &r_data) const;
	Error light_set_data(RID p_light, const FFLightData &p_data);
	Error light_get_aabb(RID p_light, AABB &r_aabb) const;
	Error light_update_dependency(RID p_light, DependencyTracker *p_tracker);
	Error update_dependencies();
	RID light_instance_create(RID p_light);
	bool owns_instance(RID p_instance) const { return _instance_owner.owns(p_instance); }
	Error light_instance_set_transform(RID p_instance, const Transform3D &p_transform);
	Error light_instance_get_data(RID p_instance, FFLightInstanceData &r_data) const;
	Error light_instance_set_shadow_transform(RID p_instance, const Projection &p_projection, const Transform3D &p_transform, int p_pass, float p_texel_size, float p_bias_scale);
	Error light_instance_get_shadow_transform(RID p_instance, FFShadowTransform &r_shadow) const;
	Error free(RID p_rid);
	void clear();
	int get_light_count() const { return _lights.size(); }
	int get_instance_count() const { return _instances.size(); }
	FixedFunctionLightStorage();
	~FixedFunctionLightStorage();
};
