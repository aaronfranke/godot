#include "light_storage.h"

FixedFunctionLightStorage::FFLightData::FFLightData() :
		version(0), color(1, 1, 1), shadow_color(0, 0, 0), cull_mask(0xffffffff), type(VS::LIGHT_DIRECTIONAL), bake_mode(VS::LIGHT_BAKE_INDIRECT), omni_shadow_mode(VS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID), omni_shadow_detail(VS::LIGHT_OMNI_SHADOW_DETAIL_VERTICAL), directional_shadow_mode(VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL), directional_range_mode(VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE), flags(0) {
	for (int i = 0; i < VS::LIGHT_PARAM_MAX; i++) {
		parameters[i] = 0;
	}
	parameters[VS::LIGHT_PARAM_ENERGY] = 1;
	parameters[VS::LIGHT_PARAM_INDIRECT_ENERGY] = 1;
	parameters[VS::LIGHT_PARAM_SPECULAR] = 0.5;
	parameters[VS::LIGHT_PARAM_RANGE] = 1;
	parameters[VS::LIGHT_PARAM_SPOT_ANGLE] = 45;
	parameters[VS::LIGHT_PARAM_CONTACT_SHADOW_SIZE] = 45;
	parameters[VS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET] = 0.1;
	parameters[VS::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET] = 0.3;
	parameters[VS::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET] = 0.6;
	parameters[VS::LIGHT_PARAM_SHADOW_NORMAL_BIAS] = 0.1;
	parameters[VS::LIGHT_PARAM_SHADOW_BIAS_SPLIT_SCALE] = 0.1;
	parameters[VS::LIGHT_PARAM_SHADOW_FADE_START] = 0.8;
}

FixedFunctionLightStorage::FFLightInstance::FFLightInstance() :
		light(nullptr) {}
FixedFunctionLightStorage::FFLight::FFLight() :
		dirty(false) {}
FixedFunctionLightStorage::FixedFunctionLightStorage() :
		_next_generation(0), _clearing(false), _notifying(false) {}
FixedFunctionLightStorage::~FixedFunctionLightStorage() { clear(); }

FixedFunctionLightStorage::FFLight *FixedFunctionLightStorage::_get_light(RID p_light) const {
	return _light_owner.owns(p_light) ? _light_owner.getptr(p_light) : nullptr;
}

FixedFunctionLightStorage::FFLightInstance *FixedFunctionLightStorage::_get_instance(RID p_instance) const {
	return _instance_owner.owns(p_instance) ? _instance_owner.getptr(p_instance) : nullptr;
}

bool FixedFunctionLightStorage::_finite_color(const Color &p_color) {
	for (int i = 0; i < 4; i++) {
		if (Math::is_nan(p_color[i]) || Math::is_inf(p_color[i])) {
			return false;
		}
	}
	return true;
}

RID FixedFunctionLightStorage::light_create(VS::LightType p_type) {
	if (_clearing || p_type < VS::LIGHT_DIRECTIONAL || p_type > VS::LIGHT_SPOT) {
		return RID();
	}
	FFLight *light = memnew(FFLight);
	light->data.type = p_type;
	light->self = _light_owner.make_rid(light);
	_lights.push_back(light);
	return light->self;
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
	if (_clearing || !light || p_data.type != light->data.type || !_finite_color(p_data.color) || !_finite_color(p_data.shadow_color) ||
			p_data.bake_mode < VS::LIGHT_BAKE_DISABLED || p_data.bake_mode > VS::LIGHT_BAKE_ALL ||
			p_data.omni_shadow_mode < VS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID || p_data.omni_shadow_mode > VS::LIGHT_OMNI_SHADOW_CUBE ||
			p_data.omni_shadow_detail < VS::LIGHT_OMNI_SHADOW_DETAIL_VERTICAL || p_data.omni_shadow_detail > VS::LIGHT_OMNI_SHADOW_DETAIL_HORIZONTAL ||
			p_data.directional_shadow_mode < VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL || p_data.directional_shadow_mode > VS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS ||
			p_data.directional_range_mode < VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE || p_data.directional_range_mode > VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_OPTIMIZED ||
			(p_data.flags & ~(LIGHT_SHADOW | LIGHT_NEGATIVE | LIGHT_REVERSE_CULL | LIGHT_USE_GI | LIGHT_BLEND_SPLITS))) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < VS::LIGHT_PARAM_MAX; i++) {
		if (Math::is_nan(p_data.parameters[i]) || Math::is_inf(p_data.parameters[i])) {
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
	AABB aabb;
	if (light->data.type != VS::LIGHT_DIRECTIONAL) {
		const real_t range = MAX(light->data.parameters[VS::LIGHT_PARAM_RANGE], 0.0f);
		if (light->data.type == VS::LIGHT_OMNI) {
			aabb = AABB(Vector3(-range, -range, -range), Vector3(2 * range, 2 * range, 2 * range));
		} else {
			// Source values remain intact; clamp only the spatial-partition bounds copy.
			const real_t angle = CLAMP(light->data.parameters[VS::LIGHT_PARAM_SPOT_ANGLE], 0.0f, 89.0f);
			const real_t radius = Math::tan(Math::deg2rad(angle)) * range;
			aabb = AABB(Vector3(-radius, -radius, -range), Vector3(2 * radius, 2 * radius, range));
		}
	}
	for (int i = 0; i < 3; i++) {
		if (Math::is_nan(aabb.position[i]) || Math::is_inf(aabb.position[i]) || Math::is_nan(aabb.size[i]) || Math::is_inf(aabb.size[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	r_aabb = aabb;
	return OK;
}

RID FixedFunctionLightStorage::light_instance_create(RID p_light) {
	FFLight *light = _get_light(p_light);
	if (_clearing || !light) {
		return RID();
	}
	FFLightInstance *instance = memnew(FFLightInstance);
	instance->light = light;
	instance->self = _instance_owner.make_rid(instance);
	light->instances.insert(instance);
	_instances.push_back(instance);
	return instance->self;
}

Error FixedFunctionLightStorage::light_instance_set_transform(RID p_instance, const Transform &p_transform) {
	FFLightInstance *instance = _get_instance(p_instance);
	if (_clearing || !instance) {
		return ERR_INVALID_PARAMETER;
	}
	for (int column = 0; column < 4; column++) {
		const Vector3 value = column < 3 ? p_transform.basis.get_axis(column) : p_transform.origin;
		for (int row = 0; row < 3; row++) {
			if (Math::is_nan(value[row]) || Math::is_inf(value[row])) {
				return ERR_INVALID_PARAMETER;
			}
		}
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
	r_data.light = instance->light ? instance->light->self : RID();
	return OK;
}

Error FixedFunctionLightStorage::validate_light_instances(const RID *p_instances, int p_count) const {
	if (p_count < 0 || (p_count && !p_instances)) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < p_count; i++) {
		if (!_get_instance(p_instances[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	return OK;
}

void FixedFunctionLightStorage::_free_instance(FFLightInstance *p_instance) {
	_instances.erase(p_instance);
	_instance_owner.free(p_instance->self);
	if (p_instance->light) {
		p_instance->light->instances.erase(p_instance);
	}
	memdelete(p_instance);
}

Error FixedFunctionLightStorage::free(RID p_rid) {
	if (_light_owner.owns(p_rid)) {
		_free_light(_get_light(p_rid));
		return OK;
	}
	if (_instance_owner.owns(p_rid)) {
		_free_instance(_get_instance(p_rid));
		return OK;
	}
	return ERR_INVALID_PARAMETER;
}

void FixedFunctionLightStorage::clear() {
	if (_clearing) {
		return;
	}
	_clearing = true;
	while (_lights.front()) {
		_free_light(_lights.front()->get());
	}
	while (_instances.front()) {
		_free_instance(_instances.front()->get());
	}
	_clearing = false;
}
