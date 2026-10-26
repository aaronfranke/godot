#ifndef FIXED_FUNCTION_SCENE_STORAGE_H
#define FIXED_FUNCTION_SCENE_STORAGE_H

#include "servers/visual/rasterizer.h"

// Real CPU owners for the atlas/environment records allocated by viewport/world startup.
// These records do not allocate native shadow/reflection resources. All calls use the owner thread.
class FixedFunctionSceneStorage {
	struct FFShadowAtlas : RID_Data {
		uint32_t subdivisions[4];
		int size;
		RID self;
		FFShadowAtlas();
	};
	struct FFReflectionAtlas : RID_Data {
		RID self;
		int size;
		int subdivision;
		FFReflectionAtlas();
	};
	struct FFSky : RID_Data {
		RID self;
		RID panorama;
	};
	struct FFEnvironment : RID_Data {
		Basis sky_orientation;
		Color background_color;
		Color ambient_color;
		Map<StringName, Vector<Variant>> optional_effects;
		RID self;
		RID sky;
		VS::EnvironmentBG background;
		int canvas_max_layer;
		int camera_feed_id;
		float background_energy;
		float ambient_energy;
		float sky_contribution;
		float sky_custom_fov;
		FFEnvironment();
	};
	// Const record lookup is conceptually read-only; 3.x RID_Owner lacks a const API.
	mutable RID_Owner<FFShadowAtlas> _shadow_owner;
	mutable RID_Owner<FFReflectionAtlas> _reflection_owner;
	mutable RID_Owner<FFEnvironment> _environment_owner;
	mutable RID_Owner<FFSky> _sky_owner;
	List<FFShadowAtlas *> _shadows;
	List<FFReflectionAtlas *> _reflections;
	List<FFEnvironment *> _environments;
	List<FFSky *> _skies;
	bool _clearing;
	FFEnvironment *_get_environment(RID p_env) const;
	static bool _finite_color(const Color &p_color);
	Error _free(RID p_rid);
	FixedFunctionSceneStorage(const FixedFunctionSceneStorage &);
	FixedFunctionSceneStorage &operator=(const FixedFunctionSceneStorage &);

public:
	RID shadow_atlas_create();
	Error shadow_atlas_set_size(RID p_atlas, int p_size);
	int shadow_atlas_get_size(RID p_atlas) const;
	Error shadow_atlas_set_subdivision(RID p_atlas, int p_quadrant, int p_subdivision);
	int shadow_atlas_get_subdivision(RID p_atlas, int p_quadrant) const;
	RID reflection_atlas_create();
	Error reflection_atlas_set_size(RID p_atlas, int p_size);
	Error reflection_atlas_set_subdivision(RID p_atlas, int p_subdivision);
	int reflection_atlas_get_size(RID p_atlas) const;
	int reflection_atlas_get_subdivision(RID p_atlas) const;
	RID sky_create();
	bool owns_sky(RID p_sky) const { return _sky_owner.owns(p_sky); }
	Error sky_set_panorama(RID p_sky, RID p_panorama);
	void sky_detach_panorama(RID p_panorama);
	RID environment_create();
	bool is_environment(RID p_env) const { return _environment_owner.owns(p_env); }
	Error environment_check(RID p_env, bool p_unsupported = false) const;
	// Retain native setter arguments for effects omitted by the current LDR profile.
	Error environment_set_effect(RID p_env, const StringName &p_name, const Variant *p_values, int p_count);
	Error environment_get_effect(RID p_env, const StringName &p_name, Vector<Variant> &r_values) const;
	bool environment_get_depth_fog(RID p_env, Color &r_color, float &r_begin, float &r_end) const;
	Error environment_set_background(RID p_env, VS::EnvironmentBG p_background);
	VS::EnvironmentBG environment_get_background(RID p_env) const;
	Error environment_set_sky(RID p_env, RID p_sky);
	Error environment_set_sky_custom_fov(RID p_env, float p_fov);
	Error environment_set_sky_orientation(RID p_env, const Basis &p_orientation);
	RID environment_get_panorama(RID p_env, Basis &r_orientation, float &r_fov, float &r_energy, bool p_require_visible = true) const;
	Error environment_set_color(RID p_env, const Color &p_color);
	Color environment_get_color(RID p_env) const;
	Error environment_set_energy(RID p_env, float p_energy);
	float environment_get_energy(RID p_env) const;
	Error environment_set_canvas_max_layer(RID p_env, int p_layer);
	int environment_get_canvas_max_layer(RID p_env) const;
	Error environment_set_camera_feed(RID p_env, int p_feed);
	Error environment_set_ambient(RID p_env, const Color &p_color, float p_energy, float p_sky_contribution);
	Error environment_get_ambient(RID p_env, Color &r_color, float &r_energy, float &r_sky_contribution) const;
	Error validate_empty_scene(RID p_environment, RID p_shadow_atlas, RID p_reflection_atlas, int p_geometry_count, int p_light_count, int p_probe_count) const;
	bool owns(RID p_rid) const;
	Error free(RID p_rid);
	void clear();
	int get_shadow_atlas_count() const { return _shadows.size(); }
	int get_reflection_atlas_count() const { return _reflections.size(); }
	int get_environment_count() const { return _environments.size(); }
	FixedFunctionSceneStorage();
	~FixedFunctionSceneStorage();
};

#endif // FIXED_FUNCTION_SCENE_STORAGE_H
