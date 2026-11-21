#include "light_storage.h"

FixedFunctionLightStorage::FFLightData::FFLightData() :
		version(0), color(1, 1, 1), distance_fade_begin(40), distance_fade_shadow(50), distance_fade_length(10), cull_mask(0xffffffff), shadow_caster_mask(0xffffffff), max_sdfgi_cascade(2), type(RSE::LIGHT_DIRECTIONAL), bake_mode(RSE::LIGHT_BAKE_DYNAMIC), omni_shadow_mode(RSE::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID), directional_shadow_mode(RSE::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL), sky_mode(RSE::LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_AND_SKY), flags(0), distance_fade_enabled(false), allow_contact_shadows(true) {
	for (int i = 0; i < RSE::LIGHT_PARAM_MAX; i++) {
		parameters[i] = 0;
	}
	parameters[RSE::LIGHT_PARAM_ENERGY] = 1;
	parameters[RSE::LIGHT_PARAM_INDIRECT_ENERGY] = 1;
	parameters[RSE::LIGHT_PARAM_VOLUMETRIC_FOG_ENERGY] = 1;
	parameters[RSE::LIGHT_PARAM_SPECULAR] = 0.5;
	parameters[RSE::LIGHT_PARAM_RANGE] = 1;
	parameters[RSE::LIGHT_PARAM_ATTENUATION] = 1;
	parameters[RSE::LIGHT_PARAM_SPOT_ANGLE] = 45;
	parameters[RSE::LIGHT_PARAM_SPOT_ATTENUATION] = 1;
	parameters[RSE::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET] = 0.1;
	parameters[RSE::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET] = 0.3;
	parameters[RSE::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET] = 0.6;
	parameters[RSE::LIGHT_PARAM_SHADOW_FADE_START] = 0.8;
	parameters[RSE::LIGHT_PARAM_SHADOW_NORMAL_BIAS] = 1;
	parameters[RSE::LIGHT_PARAM_SHADOW_OPACITY] = 1;
	parameters[RSE::LIGHT_PARAM_SHADOW_BIAS] = 0.02;
	parameters[RSE::LIGHT_PARAM_SHADOW_PANCAKE_SIZE] = 20;
	parameters[RSE::LIGHT_PARAM_TRANSMITTANCE_BIAS] = 0.05;
	parameters[RSE::LIGHT_PARAM_INTENSITY] = 100000;
	parameters[RSE::LIGHT_PARAM_CONTACT_SHADOW_BLUR] = 1;
}

FixedFunctionLightStorage::FFLightInstance::FFLightInstance() : light(nullptr) {}
FixedFunctionLightStorage::FFLight::FFLight() : initialized(false), dirty(false), free_pending(false) {}
FixedFunctionLightStorage::FixedFunctionLightStorage() : _clearing(false), _notifying(false), _clear_pending(false) {}
FixedFunctionLightStorage::~FixedFunctionLightStorage() {
	clear();
}

bool FixedFunctionLightStorage::_finite_color(const Color &p_color) {
	for (int i = 0; i < 4; i++) {
		if (!Math::is_finite(p_color[i])) {
			return false;
		}
	}
	return true;
}

FixedFunctionLightStorage::FFLight *FixedFunctionLightStorage::_get_light(RID p_light) const {
	FFLight *light = _light_owner.owns(p_light) ? _light_owner.get_or_null(p_light) : nullptr;
	return light && light->initialized && !light->free_pending ? light : nullptr;
}

FixedFunctionLightStorage::FFLightInstance *FixedFunctionLightStorage::_get_instance(RID p_instance) const {
	return _instance_owner.owns(p_instance) ? _instance_owner.get_or_null(p_instance) : nullptr;
}

RID FixedFunctionLightStorage::light_allocate() {
	if (_clearing || _clear_pending) {
		return RID();
	}
	FFLight *light = memnew(FFLight);
	light->self = _light_owner.make_rid(light);
	_lights.push_back(light);
	return light->self;
}

Error FixedFunctionLightStorage::light_initialize(RID p_light, RSE::LightType p_type) {
	FFLight *light = _light_owner.owns(p_light) ? _light_owner.get_or_null(p_light) : nullptr;
	if (_clearing || _clear_pending || !light || p_type < RSE::LIGHT_DIRECTIONAL || p_type > RSE::LIGHT_SPOT) {
		return ERR_INVALID_PARAMETER;
	}
	if (light->initialized) {
		return ERR_ALREADY_IN_USE;
	}
	light->data.type = p_type;
	light->data.parameters[RSE::LIGHT_PARAM_INTENSITY] = p_type == RSE::LIGHT_DIRECTIONAL ? 100000 : 1000;
	light->initialized = true;
	return OK;
}

RID FixedFunctionLightStorage::light_create(RSE::LightType p_type) {
	if (p_type < RSE::LIGHT_DIRECTIONAL || p_type > RSE::LIGHT_SPOT) {
		return RID();
	}
	const RID light = light_allocate();
	if (light.is_valid()) {
		light_initialize(light, p_type);
	}
	return light;
}

Error FixedFunctionLightStorage::light_get_data(RID p_light, FFLightData &r_data) const {
	const FFLight *light = _get_light(p_light);
	if (!light) {
		return ERR_INVALID_PARAMETER;
	}
	r_data = light->data;
	return OK;
}

Error FixedFunctionLightStorage::light_set_data(RID p_light, const FFLightData &p_data) {
	FFLight *light = _get_light(p_light);
	if (_clearing || _clear_pending || !light || p_data.type != light->data.type || !_finite_color(p_data.color) ||
			p_data.bake_mode < RSE::LIGHT_BAKE_DISABLED || p_data.bake_mode > RSE::LIGHT_BAKE_DYNAMIC ||
			p_data.omni_shadow_mode < RSE::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID || p_data.omni_shadow_mode > RSE::LIGHT_OMNI_SHADOW_CUBE ||
			p_data.directional_shadow_mode < RSE::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL || p_data.directional_shadow_mode > RSE::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS ||
			p_data.sky_mode < RSE::LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_AND_SKY || p_data.sky_mode > RSE::LIGHT_DIRECTIONAL_SKY_MODE_SKY_ONLY ||
			(p_data.flags & ~(LIGHT_SHADOW | LIGHT_NEGATIVE | LIGHT_REVERSE_CULL | LIGHT_BLEND_SPLITS)) ||
			!Math::is_finite(p_data.distance_fade_begin) || !Math::is_finite(p_data.distance_fade_shadow) || !Math::is_finite(p_data.distance_fade_length)) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < RSE::LIGHT_PARAM_MAX; i++) {
		if (!Math::is_finite(p_data.parameters[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (light->data.version == UINT64_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	const uint64_t version = light->data.version + 1;
	light->data = p_data;
	light->data.version = version;
	light->dirty = true;
	return OK;
}

Error FixedFunctionLightStorage::light_get_aabb(RID p_light, AABB &r_aabb) const {
	const FFLight *light = _get_light(p_light);
	if (!light) {
		return ERR_INVALID_PARAMETER;
	}
	AABB bounds;
	if (light->data.type != RSE::LIGHT_DIRECTIONAL) {
		const real_t range = MAX(light->data.parameters[RSE::LIGHT_PARAM_RANGE], 0.0f);
		if (light->data.type == RSE::LIGHT_OMNI) {
			bounds = AABB(Vector3(-range, -range, -range), Vector3(2 * range, 2 * range, 2 * range));
		} else {
			// Bound only the spatial-partition copy, retaining the finite source values.
			const real_t angle = CLAMP(light->data.parameters[RSE::LIGHT_PARAM_SPOT_ANGLE], 0.0f, 89.0f);
			const real_t radius = Math::tan(Math::deg_to_rad(angle)) * range;
			bounds = AABB(Vector3(-radius, -radius, -range), Vector3(2 * radius, 2 * radius, range));
		}
	}
	if (!bounds.position.is_finite() || !bounds.size.is_finite()) {
		return ERR_INVALID_PARAMETER;
	}
	r_aabb = bounds;
	return OK;
}

RID FixedFunctionLightStorage::light_instance_create(RID p_light) {
	FFLight *light = _get_light(p_light);
	if (_clearing || _clear_pending || !light) {
		return RID();
	}
	FFLightInstance *instance = memnew(FFLightInstance);
	instance->light = light;
	instance->self = _instance_owner.make_rid(instance);
	light->instances.insert(instance);
	_instances.push_back(instance);
	return instance->self;
}

Error FixedFunctionLightStorage::light_instance_set_transform(RID p_instance, const Transform3D &p_transform) {
	FFLightInstance *instance = _get_instance(p_instance);
	if (_clearing || _clear_pending || !instance || !p_transform.is_finite()) {
		return ERR_INVALID_PARAMETER;
	}
	instance->transform = p_transform;
	return OK;
}

Error FixedFunctionLightStorage::light_instance_get_data(RID p_instance, FFLightInstanceData &r_data) const {
	const FFLightInstance *instance = _get_instance(p_instance);
	if (!instance) {
		return ERR_INVALID_PARAMETER;
	}
	r_data.transform = instance->transform;
	r_data.light = instance->light && !instance->light->free_pending ? instance->light->self : RID();
	return OK;
}

Error FixedFunctionLightStorage::light_instance_set_shadow_transform(RID p_instance, const Projection &p_projection, const Transform3D &p_transform, int p_pass, float p_texel_size, float p_bias_scale) {
	FFLightInstance *instance = _get_instance(p_instance);
	if (_clearing || _clear_pending || !instance || !p_transform.is_finite() || p_transform.basis.determinant() == 0 || !Math::is_finite(p_texel_size) || p_texel_size < 0 || !Math::is_finite(p_bias_scale)) {
		return ERR_INVALID_PARAMETER;
	}
	for (int column = 0; column < 4; column++) {
		if (!p_projection.columns[column].is_finite()) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (p_pass != 0 || !p_projection.is_orthogonal()) {
		return ERR_UNAVAILABLE; // This profile executes one full-range orthographic directional pass.
	}
	instance->shadow.projection = p_projection;
	instance->shadow.transform = p_transform;
	instance->shadow.texel_size = p_texel_size;
	instance->shadow.bias_scale = p_bias_scale;
	instance->shadow.valid = true;
	return OK;
}

Error FixedFunctionLightStorage::light_instance_get_shadow_transform(RID p_instance, FFShadowTransform &r_shadow) const {
	const FFLightInstance *instance = _get_instance(p_instance);
	if (!instance) {
		return ERR_INVALID_PARAMETER;
	}
	r_shadow = instance->shadow;
	return OK;
}

Error FixedFunctionLightStorage::light_update_dependency(RID p_light, DependencyTracker *p_tracker) {
	FFLight *light = _get_light(p_light);
	if (!light || !p_tracker) {
		return ERR_INVALID_PARAMETER;
	}
	p_tracker->update_dependency(&light->dependency);
	return OK;
}

Error FixedFunctionLightStorage::update_dependencies() {
	if (_notifying || _clearing) {
		return ERR_BUSY;
	}
	Vector<RID> events;
	for (FFLight *light : _lights) {
		if (light->dirty && !light->free_pending) {
			events.push_back(light->self);
			light->dirty = false;
		}
	}
	_notifying = true;
	for (RID rid : events) {
		FFLight *light = _get_light(rid);
		if (light) {
			light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_LIGHT);
			if (!light->free_pending) {
				light->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
			}
		}
	}
	_notifying = false;
	_retire_pending();
	for (const FFLight *light : _lights) {
		if (light->dirty || light->free_pending) {
			return ERR_SKIP;
		}
	}
	return OK;
}

void FixedFunctionLightStorage::_free_light(FFLight *p_light) {
	for (FFLightInstance *instance : p_light->instances) {
		instance->light = nullptr;
	}
	_light_owner.free(p_light->self);
	_lights.erase(p_light);
	const bool notifying = _notifying;
	_notifying = true;
	p_light->dependency.deleted_notify(p_light->self);
	_notifying = notifying;
	memdelete(p_light);
}

void FixedFunctionLightStorage::_free_instance(FFLightInstance *p_instance) {
	if (p_instance->light) {
		p_instance->light->instances.erase(p_instance);
	}
	_instance_owner.free(p_instance->self);
	_instances.erase(p_instance);
	memdelete(p_instance);
}

void FixedFunctionLightStorage::_retire_pending() {
	if (_clear_pending) {
		clear();
		return;
	}
	Vector<RID> retired;
	for (const FFLight *light : _lights) {
		if (light->free_pending) {
			retired.push_back(light->self);
		}
	}
	for (RID rid : retired) {
		FFLight *light = _light_owner.get_or_null(rid);
		if (light) {
			_free_light(light);
		}
	}
}

Error FixedFunctionLightStorage::free(RID p_rid) {
	if (_clearing || _clear_pending) {
		return ERR_UNCONFIGURED;
	}
	FFLightInstance *instance = _get_instance(p_rid);
	if (instance) {
		_free_instance(instance);
		return OK;
	}
	FFLight *light = _light_owner.owns(p_rid) ? _light_owner.get_or_null(p_rid) : nullptr;
	if (!light || light->free_pending) {
		return ERR_INVALID_PARAMETER;
	}
	if (_notifying) {
		light->free_pending = true;
	} else {
		_free_light(light);
		_retire_pending();
	}
	return OK;
}

void FixedFunctionLightStorage::clear() {
	if (_clearing) {
		return;
	}
	if (_notifying) {
		_clear_pending = true;
		return;
	}
	_clearing = true;
	_clear_pending = false;
	while (!_lights.is_empty()) {
		_free_light(_lights.front()->get());
	}
	while (!_instances.is_empty()) {
		_free_instance(_instances.front()->get());
	}
	_clearing = false;
}
