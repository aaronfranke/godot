#include "scene_storage.h"

#include "core/math/math_funcs.h"

FixedFunctionSceneStorage::FFShadowAtlas::FFShadowAtlas() :
		size(0) {
	for (int i = 0; i < 4; i++) {
		subdivisions[i] = 0;
	}
}
FixedFunctionSceneStorage::FFReflectionAtlas::FFReflectionAtlas() :
		size(0), subdivision(0) {}
FixedFunctionSceneStorage::FFEnvironment::FFEnvironment() :
		background(VS::ENV_BG_CLEAR_COLOR), canvas_max_layer(0), camera_feed_id(1), background_energy(1), ambient_energy(1), sky_contribution(1), sky_custom_fov(0) {}
FixedFunctionSceneStorage::FixedFunctionSceneStorage() :
		_clearing(false) {}
FixedFunctionSceneStorage::~FixedFunctionSceneStorage() { clear(); }

FixedFunctionSceneStorage::FFEnvironment *FixedFunctionSceneStorage::_get_environment(RID p_env) const { return _environment_owner.owns(p_env) ? _environment_owner.getptr(p_env) : nullptr; }
bool FixedFunctionSceneStorage::_finite_color(const Color &p_color) {
	for (int i = 0; i < 4; i++) {
		if (Math::is_nan(p_color[i]) || Math::is_inf(p_color[i])) {
			return false;
		}
	}
	return true;
}

RID FixedFunctionSceneStorage::shadow_atlas_create() {
	if (_clearing) {
		return RID();
	}
	FFShadowAtlas *atlas = memnew(FFShadowAtlas);
	atlas->self = _shadow_owner.make_rid(atlas);
	_shadows.push_back(atlas);
	return atlas->self;
}
Error FixedFunctionSceneStorage::shadow_atlas_set_size(RID p_atlas, int p_size) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	if (!_shadow_owner.owns(p_atlas) || p_size < 0) {
		return ERR_INVALID_PARAMETER;
	}
	_shadow_owner.getptr(p_atlas)->size = p_size;
	return OK;
}
int FixedFunctionSceneStorage::shadow_atlas_get_size(RID p_atlas) const {
	ERR_FAIL_COND_V(!_shadow_owner.owns(p_atlas), 0);
	return _shadow_owner.getptr(p_atlas)->size;
}
Error FixedFunctionSceneStorage::shadow_atlas_set_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	if (!_shadow_owner.owns(p_atlas) || p_quadrant < 0 || p_quadrant >= 4 || p_subdivision < 0 || p_subdivision >= 16384) {
		return ERR_INVALID_PARAMETER;
	}
	_shadow_owner.getptr(p_atlas)->subdivisions[p_quadrant] = p_subdivision;
	return OK;
}
int FixedFunctionSceneStorage::shadow_atlas_get_subdivision(RID p_atlas, int p_quadrant) const {
	ERR_FAIL_COND_V(!_shadow_owner.owns(p_atlas) || p_quadrant < 0 || p_quadrant >= 4, 0);
	return _shadow_owner.getptr(p_atlas)->subdivisions[p_quadrant];
}
RID FixedFunctionSceneStorage::reflection_atlas_create() {
	if (_clearing) {
		return RID();
	}
	FFReflectionAtlas *atlas = memnew(FFReflectionAtlas);
	atlas->self = _reflection_owner.make_rid(atlas);
	_reflections.push_back(atlas);
	return atlas->self;
}
Error FixedFunctionSceneStorage::reflection_atlas_set_size(RID p_atlas, int p_size) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	if (!_reflection_owner.owns(p_atlas) || p_size < 0) {
		return ERR_INVALID_PARAMETER;
	}
	_reflection_owner.getptr(p_atlas)->size = p_size;
	return OK;
}
Error FixedFunctionSceneStorage::reflection_atlas_set_subdivision(RID p_atlas, int p_subdivision) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	if (!_reflection_owner.owns(p_atlas) || p_subdivision < 0) {
		return ERR_INVALID_PARAMETER;
	}
	_reflection_owner.getptr(p_atlas)->subdivision = p_subdivision;
	return OK;
}
int FixedFunctionSceneStorage::reflection_atlas_get_size(RID p_atlas) const {
	ERR_FAIL_COND_V(!_reflection_owner.owns(p_atlas), 0);
	return _reflection_owner.getptr(p_atlas)->size;
}
int FixedFunctionSceneStorage::reflection_atlas_get_subdivision(RID p_atlas) const {
	ERR_FAIL_COND_V(!_reflection_owner.owns(p_atlas), 0);
	return _reflection_owner.getptr(p_atlas)->subdivision;
}

RID FixedFunctionSceneStorage::sky_create() {
	if (_clearing) {
		return RID();
	}
	FFSky *sky = memnew(FFSky);
	sky->self = _sky_owner.make_rid(sky);
	_skies.push_back(sky);
	return sky->self;
}

Error FixedFunctionSceneStorage::sky_set_panorama(RID p_sky, RID p_panorama) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	if (!_sky_owner.owns(p_sky)) {
		return ERR_INVALID_PARAMETER;
	}
	_sky_owner.getptr(p_sky)->panorama = p_panorama;
	return OK;
}

void FixedFunctionSceneStorage::sky_detach_panorama(RID p_panorama) {
	for (List<FFSky *>::Element *E = _skies.front(); E; E = E->next()) {
		if (E->get()->panorama == p_panorama) {
			E->get()->panorama = RID();
		}
	}
}

RID FixedFunctionSceneStorage::environment_create() {
	if (_clearing) {
		return RID();
	}
	FFEnvironment *environment = memnew(FFEnvironment);
	environment->self = _environment_owner.make_rid(environment);
	_environments.push_back(environment);
	return environment->self;
}
Error FixedFunctionSceneStorage::environment_check(RID p_env, bool p_unsupported) const {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	if (!_get_environment(p_env)) {
		return ERR_INVALID_PARAMETER;
	}
	return p_unsupported ? ERR_UNAVAILABLE : OK;
}
Error FixedFunctionSceneStorage::environment_set_background(RID p_env, VS::EnvironmentBG p_background) {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	if (p_background < VS::ENV_BG_CLEAR_COLOR || p_background >= VS::ENV_BG_MAX) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_background != VS::ENV_BG_CLEAR_COLOR && p_background != VS::ENV_BG_COLOR && p_background != VS::ENV_BG_CANVAS && p_background != VS::ENV_BG_SKY && p_background != VS::ENV_BG_COLOR_SKY) {
		return ERR_UNAVAILABLE;
	}
	_get_environment(p_env)->background = p_background;
	return OK;
}
VS::EnvironmentBG FixedFunctionSceneStorage::environment_get_background(RID p_env) const {
	const FFEnvironment *env = _get_environment(p_env);
	ERR_FAIL_COND_V(!env, VS::ENV_BG_CLEAR_COLOR);
	return env->background;
}

Error FixedFunctionSceneStorage::environment_set_sky(RID p_env, RID p_sky) {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	if (p_sky.is_valid() && !_sky_owner.owns(p_sky)) {
		return ERR_INVALID_PARAMETER;
	}
	_get_environment(p_env)->sky = p_sky;
	return OK;
}

Error FixedFunctionSceneStorage::environment_set_sky_custom_fov(RID p_env, float p_fov) {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	if (!(p_fov >= 0 && p_fov <= 180)) {
		return ERR_INVALID_PARAMETER;
	}
	_get_environment(p_env)->sky_custom_fov = p_fov;
	return OK;
}

Error FixedFunctionSceneStorage::environment_set_sky_orientation(RID p_env, const Basis &p_orientation) {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	for (int row = 0; row < 3; row++) {
		for (int col = 0; col < 3; col++) {
			if (Math::is_nan(p_orientation[row][col]) || Math::is_inf(p_orientation[row][col])) {
				return ERR_INVALID_PARAMETER;
			}
		}
	}
	const real_t determinant = p_orientation.determinant();
	if (determinant == 0 || Math::is_nan(determinant) || Math::is_inf(determinant)) {
		return ERR_INVALID_PARAMETER;
	}
	_get_environment(p_env)->sky_orientation = p_orientation;
	return OK;
}

RID FixedFunctionSceneStorage::environment_get_panorama(RID p_env, Basis &r_orientation, float &r_fov, float &r_energy, bool p_require_visible) const {
	const FFEnvironment *env = _get_environment(p_env);
	if (!env || !env->sky.is_valid()) {
		return RID();
	}
	if (env->background != VS::ENV_BG_SKY && (p_require_visible || env->background != VS::ENV_BG_COLOR_SKY)) {
		return RID();
	}
	r_orientation = env->sky_orientation;
	r_fov = env->sky_custom_fov;
	r_energy = env->background_energy;
	return _sky_owner.getptr(env->sky)->panorama;
}
Error FixedFunctionSceneStorage::environment_set_color(RID p_env, const Color &p_color) {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	if (!_finite_color(p_color)) {
		return ERR_INVALID_PARAMETER;
	}
	_get_environment(p_env)->background_color = p_color;
	return OK;
}
Color FixedFunctionSceneStorage::environment_get_color(RID p_env) const {
	const FFEnvironment *env = _get_environment(p_env);
	ERR_FAIL_COND_V(!env, Color());
	return env->background_color;
}
Error FixedFunctionSceneStorage::environment_set_energy(RID p_env, float p_energy) {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	if (Math::is_nan(p_energy) || Math::is_inf(p_energy)) {
		return ERR_INVALID_PARAMETER;
	}
	_get_environment(p_env)->background_energy = p_energy;
	return OK;
}
float FixedFunctionSceneStorage::environment_get_energy(RID p_env) const {
	const FFEnvironment *env = _get_environment(p_env);
	ERR_FAIL_COND_V(!env, 1);
	return env->background_energy;
}
Error FixedFunctionSceneStorage::environment_set_canvas_max_layer(RID p_env, int p_layer) {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	_get_environment(p_env)->canvas_max_layer = p_layer;
	return OK;
}
int FixedFunctionSceneStorage::environment_get_canvas_max_layer(RID p_env) const {
	const FFEnvironment *env = _get_environment(p_env);
	ERR_FAIL_COND_V(!env, 0);
	return env->canvas_max_layer;
}
Error FixedFunctionSceneStorage::environment_set_camera_feed(RID p_env, int p_feed) {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	if (p_feed < 0) {
		return ERR_INVALID_PARAMETER;
	}
	_get_environment(p_env)->camera_feed_id = p_feed;
	return OK;
}
Error FixedFunctionSceneStorage::environment_set_ambient(RID p_env, const Color &p_color, float p_energy, float p_sky_contribution) {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	if (!_finite_color(p_color) || Math::is_nan(p_energy) || Math::is_inf(p_energy) || Math::is_nan(p_sky_contribution) || Math::is_inf(p_sky_contribution)) {
		return ERR_INVALID_PARAMETER;
	}
	FFEnvironment *env = _get_environment(p_env);
	env->ambient_color = p_color;
	env->ambient_energy = p_energy;
	env->sky_contribution = p_sky_contribution;
	return OK;
}
Error FixedFunctionSceneStorage::environment_get_ambient(RID p_env, Color &r_color, float &r_energy, float &r_sky_contribution) const {
	const FFEnvironment *env = _get_environment(p_env);
	if (!env) {
		return ERR_INVALID_PARAMETER;
	}
	r_color = env->ambient_color;
	r_energy = env->ambient_energy;
	r_sky_contribution = env->sky_contribution;
	return OK;
}

Error FixedFunctionSceneStorage::validate_empty_scene(RID p_environment, RID p_shadow_atlas, RID p_reflection_atlas, int p_geometry_count, int p_light_count, int p_probe_count) const {
	if (p_geometry_count < 0 || p_light_count < 0 || p_probe_count < 0 || (p_environment.is_valid() && !_environment_owner.owns(p_environment)) || (p_shadow_atlas.is_valid() && !_shadow_owner.owns(p_shadow_atlas)) || (p_reflection_atlas.is_valid() && !_reflection_owner.owns(p_reflection_atlas))) {
		return ERR_INVALID_PARAMETER;
	}
	return p_geometry_count || p_light_count || p_probe_count ? ERR_UNAVAILABLE : OK;
}
bool FixedFunctionSceneStorage::owns(RID p_rid) const { return _shadow_owner.owns(p_rid) || _reflection_owner.owns(p_rid) || _environment_owner.owns(p_rid) || _sky_owner.owns(p_rid); }
Error FixedFunctionSceneStorage::free(RID p_rid) {
	if (_clearing) {
		return ERR_UNCONFIGURED;
	}
	return _free(p_rid);
}

Error FixedFunctionSceneStorage::_free(RID p_rid) {
	if (_sky_owner.owns(p_rid)) {
		FFSky *sky = _sky_owner.getptr(p_rid);
		for (List<FFEnvironment *>::Element *E = _environments.front(); E; E = E->next()) {
			if (E->get()->sky == p_rid) {
				E->get()->sky = RID();
			}
		}
		_sky_owner.free(p_rid);
		_skies.erase(sky);
		memdelete(sky);
		return OK;
	}
	if (_shadow_owner.owns(p_rid)) {
		FFShadowAtlas *atlas = _shadow_owner.getptr(p_rid);
		_shadow_owner.free(p_rid);
		_shadows.erase(atlas);
		memdelete(atlas);
		return OK;
	}
	if (_reflection_owner.owns(p_rid)) {
		FFReflectionAtlas *atlas = _reflection_owner.getptr(p_rid);
		_reflection_owner.free(p_rid);
		_reflections.erase(atlas);
		memdelete(atlas);
		return OK;
	}
	if (_environment_owner.owns(p_rid)) {
		FFEnvironment *env = _environment_owner.getptr(p_rid);
		_environment_owner.free(p_rid);
		_environments.erase(env);
		memdelete(env);
		return OK;
	}
	return ERR_INVALID_PARAMETER;
}
void FixedFunctionSceneStorage::clear() {
	if (_clearing) {
		return;
	}
	// No native calls or user-owned Variant payloads occur while destroying these records.
	_clearing = true;
	while (_shadows.front()) {
		_free(_shadows.front()->get()->self);
	}
	while (_reflections.front()) {
		_free(_reflections.front()->get()->self);
	}
	while (_environments.front()) {
		_free(_environments.front()->get()->self);
	}
	while (_skies.front()) {
		_free(_skies.front()->get()->self);
	}
	_clearing = false;
}
