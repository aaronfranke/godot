#ifndef FIXED_FUNCTION_LIGHT_STORAGE_H
#define FIXED_FUNCTION_LIGHT_STORAGE_H

#include "servers/visual/rasterizer.h"

// CPU source data and native scene dependencies. Ownership does not imply that
// the renderer executes a light type, shadows, projectors or GI.
class FixedFunctionLightStorage {
public:
	enum FFLightFlag {
		LIGHT_SHADOW = 1,
		LIGHT_NEGATIVE = 2,
		LIGHT_REVERSE_CULL = 4,
		LIGHT_USE_GI = 8,
		LIGHT_BLEND_SPLITS = 16,
	};
	struct FFLightData {
		uint64_t version;
		RID projector;
		Color color;
		Color shadow_color;
		float parameters[VS::LIGHT_PARAM_MAX];
		uint32_t cull_mask;
		VS::LightType type;
		VS::LightBakeMode bake_mode;
		VS::LightOmniShadowMode omni_shadow_mode;
		VS::LightOmniShadowDetail omni_shadow_detail;
		VS::LightDirectionalShadowMode directional_shadow_mode;
		VS::LightDirectionalShadowDepthRangeMode directional_range_mode;
		uint8_t flags;
		FFLightData();
	};
	struct FFLightInstanceData {
		Transform transform;
		RID light;
	};

private:
	struct FFLight;
	struct FFLightInstance : RID_Data {
		FFLight *light;
		RID self;
		Transform transform;
		FFLightInstance();
	};
	struct FFLight : RID_Data {
		Map<RasterizerScene::InstanceBase *, uint64_t> dependencies;
		// The pointer-sized RID fills the gap before data in x86_32 debug builds.
		RID self;
		FFLightData data;
		Set<FFLightInstance *> instances;
		bool dirty;
		FFLight();
	};
	struct FFRegistration {
		uint64_t generation;
		RID light;
	};
	struct FFDependencyEvent {
		uint64_t generation;
		RID light;
		RasterizerScene::InstanceBase *instance;
	};
	uint64_t _next_generation;
	// RID lookup is read-only here; 3.x RID_Owner lacks a const lookup API.
	mutable RID_Owner<FFLight> _light_owner;
	mutable RID_Owner<FFLightInstance> _instance_owner;
	List<FFLight *> _lights;
	List<FFLightInstance *> _instances;
	Map<RasterizerScene::InstanceBase *, FFRegistration> _registrations;
	bool _clearing;
	bool _notifying;
	FFLight *_get_light(RID p_light) const;
	FFLightInstance *_get_instance(RID p_instance) const;
	static bool _finite_color(const Color &p_color);
	void _free_light(FFLight *p_light);
	void _free_instance(FFLightInstance *p_instance);
	FixedFunctionLightStorage(const FixedFunctionLightStorage &);
	FixedFunctionLightStorage &operator=(const FixedFunctionLightStorage &);

public:
	RID light_create(VS::LightType p_type);
	bool owns_light(RID p_light) const { return _light_owner.owns(p_light); }
	Error light_get_data(RID p_light, FFLightData &r_data) const;
	// Validate atomically and retain source precision/ranges; version is owner-managed.
	Error light_set_data(RID p_light, const FFLightData &p_data);
	Error light_get_aabb(RID p_light, AABB &r_aabb) const;
	RID light_instance_create(RID p_light);
	bool owns_instance(RID p_instance) const { return _instance_owner.owns(p_instance); }
	Error light_instance_set_transform(RID p_instance, const Transform &p_transform);
	Error light_instance_get_data(RID p_instance, FFLightInstanceData &r_data) const;
	Error validate_light_instances(const RID *p_instances, int p_count) const;
	Error instance_add_dependency(RID p_light, RasterizerScene::InstanceBase *p_instance);
	Error instance_remove_dependency(RID p_light, RasterizerScene::InstanceBase *p_instance);
	bool has_dependency(RID p_light, RasterizerScene::InstanceBase *p_instance) const;
	Error update_dependencies();
	Error free(RID p_rid);
	void clear();
	int get_light_count() const { return _lights.size(); }
	int get_instance_count() const { return _instances.size(); }
	FixedFunctionLightStorage();
	~FixedFunctionLightStorage();
};

#endif // FIXED_FUNCTION_LIGHT_STORAGE_H
