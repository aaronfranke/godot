#include "rasterizer_storage_fixed_function.h"

#include "core/project_settings.h"

RID RasterizerStorageFixedFunction::light_create(VS::LightType p_type) {
	const RID light = _light_storage.light_create(p_type);
	_report_error(light.is_valid() ? OK : ERR_INVALID_PARAMETER, "light_create");
	return light;
}

void RasterizerStorageFixedFunction::light_set_color(RID p_light, const Color &p_color) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.color = p_color;
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_color");
}

void RasterizerStorageFixedFunction::light_set_param(RID p_light, VS::LightParam p_param, float p_value) {
	if (p_param < 0 || p_param >= VS::LIGHT_PARAM_MAX) {
		_report_error(ERR_INVALID_PARAMETER, "light_set_param");
		return;
	}
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.parameters[p_param] = p_value;
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_param");
}

void RasterizerStorageFixedFunction::light_set_shadow_color(RID p_light, const Color &p_color) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.shadow_color = p_color;
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_shadow_color");
}

void RasterizerStorageFixedFunction::light_set_projector(RID p_light, RID p_texture) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.projector = p_texture;
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_projector");
}

void RasterizerStorageFixedFunction::light_set_cull_mask(RID p_light, uint32_t p_mask) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.cull_mask = p_mask;
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_cull_mask");
}

void RasterizerStorageFixedFunction::light_set_bake_mode(RID p_light, VS::LightBakeMode p_bake_mode) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.bake_mode = p_bake_mode;
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_bake_mode");
}

void RasterizerStorageFixedFunction::light_omni_set_shadow_mode(RID p_light, VS::LightOmniShadowMode p_mode) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.omni_shadow_mode = p_mode;
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_omni_set_shadow_mode");
}

void RasterizerStorageFixedFunction::light_omni_set_shadow_detail(RID p_light, VS::LightOmniShadowDetail p_detail) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.omni_shadow_detail = p_detail;
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_omni_set_shadow_detail");
}

void RasterizerStorageFixedFunction::light_directional_set_shadow_mode(RID p_light, VS::LightDirectionalShadowMode p_mode) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.directional_shadow_mode = p_mode;
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_directional_set_shadow_mode");
}

void RasterizerStorageFixedFunction::light_directional_set_shadow_depth_range_mode(RID p_light, VS::LightDirectionalShadowDepthRangeMode p_range_mode) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.directional_range_mode = p_range_mode;
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_directional_set_shadow_depth_range_mode");
}

void RasterizerStorageFixedFunction::light_set_shadow(RID p_light, bool p_enabled) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.flags = static_cast<uint8_t>(p_enabled ? data.flags | FixedFunctionLightStorage::LIGHT_SHADOW : data.flags & ~FixedFunctionLightStorage::LIGHT_SHADOW);
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_shadow");
}

void RasterizerStorageFixedFunction::light_set_negative(RID p_light, bool p_enabled) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.flags = static_cast<uint8_t>(p_enabled ? data.flags | FixedFunctionLightStorage::LIGHT_NEGATIVE : data.flags & ~FixedFunctionLightStorage::LIGHT_NEGATIVE);
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_negative");
}

void RasterizerStorageFixedFunction::light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.flags = static_cast<uint8_t>(p_enabled ? data.flags | FixedFunctionLightStorage::LIGHT_REVERSE_CULL : data.flags & ~FixedFunctionLightStorage::LIGHT_REVERSE_CULL);
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_reverse_cull_face_mode");
}

void RasterizerStorageFixedFunction::light_set_use_gi(RID p_light, bool p_enabled) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.flags = static_cast<uint8_t>(p_enabled ? data.flags | FixedFunctionLightStorage::LIGHT_USE_GI : data.flags & ~FixedFunctionLightStorage::LIGHT_USE_GI);
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_use_gi");
}

void RasterizerStorageFixedFunction::light_directional_set_blend_splits(RID p_light, bool p_enabled) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _light_storage.light_get_data(p_light, data);
	if (error == OK) {
		data.flags = static_cast<uint8_t>(p_enabled ? data.flags | FixedFunctionLightStorage::LIGHT_BLEND_SPLITS : data.flags & ~FixedFunctionLightStorage::LIGHT_BLEND_SPLITS);
		error = _light_storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_directional_set_blend_splits");
}

VS::LightType RasterizerStorageFixedFunction::light_get_type(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	const Error error = _light_storage.light_get_data(p_light, data);
	ERR_FAIL_COND_V(error != OK, VS::LIGHT_DIRECTIONAL);
	return data.type;
}

Color RasterizerStorageFixedFunction::light_get_color(RID p_light) {
	FixedFunctionLightStorage::FFLightData data;
	const Error error = _light_storage.light_get_data(p_light, data);
	ERR_FAIL_COND_V(error != OK, Color());
	return data.color;
}

bool RasterizerStorageFixedFunction::light_get_use_gi(RID p_light) {
	FixedFunctionLightStorage::FFLightData data;
	const Error error = _light_storage.light_get_data(p_light, data);
	ERR_FAIL_COND_V(error != OK, false);
	return (data.flags & FixedFunctionLightStorage::LIGHT_USE_GI) != 0;
}

VS::LightBakeMode RasterizerStorageFixedFunction::light_get_bake_mode(RID p_light) {
	FixedFunctionLightStorage::FFLightData data;
	const Error error = _light_storage.light_get_data(p_light, data);
	ERR_FAIL_COND_V(error != OK, VS::LIGHT_BAKE_DISABLED);
	return data.bake_mode;
}

uint64_t RasterizerStorageFixedFunction::light_get_version(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	const Error error = _light_storage.light_get_data(p_light, data);
	ERR_FAIL_COND_V(error != OK, 0);
	return data.version;
}

bool RasterizerStorageFixedFunction::light_directional_get_blend_splits(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	const Error error = _light_storage.light_get_data(p_light, data);
	ERR_FAIL_COND_V(error != OK, false);
	return (data.flags & FixedFunctionLightStorage::LIGHT_BLEND_SPLITS) != 0;
}

VS::LightDirectionalShadowDepthRangeMode RasterizerStorageFixedFunction::light_directional_get_shadow_depth_range_mode(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	const Error error = _light_storage.light_get_data(p_light, data);
	ERR_FAIL_COND_V(error != OK, VS::LIGHT_DIRECTIONAL_SHADOW_DEPTH_RANGE_STABLE);
	return data.directional_range_mode;
}

VS::LightDirectionalShadowMode RasterizerStorageFixedFunction::light_directional_get_shadow_mode(RID p_light) {
	FixedFunctionLightStorage::FFLightData data;
	const Error error = _light_storage.light_get_data(p_light, data);
	ERR_FAIL_COND_V(error != OK, VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL);
	// Preserve requested cascades in CPU data; this profile executes one full-range map.
	return light_has_shadow(p_light) ? VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL : data.directional_shadow_mode;
}

VS::LightOmniShadowMode RasterizerStorageFixedFunction::light_omni_get_shadow_mode(RID p_light) {
	FixedFunctionLightStorage::FFLightData data;
	const Error error = _light_storage.light_get_data(p_light, data);
	ERR_FAIL_COND_V(error != OK, VS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID);
	return data.omni_shadow_mode;
}

AABB RasterizerStorageFixedFunction::light_get_aabb(RID p_light) const {
	AABB aabb;
	const Error error = _light_storage.light_get_aabb(p_light, aabb);
	ERR_FAIL_COND_V(error != OK, AABB());
	return aabb;
}

float RasterizerStorageFixedFunction::light_get_param(RID p_light, VS::LightParam p_param) {
	ERR_FAIL_INDEX_V(p_param, VS::LIGHT_PARAM_MAX, 0);
	FixedFunctionLightStorage::FFLightData data;
	const Error error = _light_storage.light_get_data(p_light, data);
	ERR_FAIL_COND_V(error != OK, 0);
	return data.parameters[p_param];
}

bool RasterizerStorageFixedFunction::light_has_shadow(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	ERR_FAIL_COND_V(_light_storage.light_get_data(p_light, data) != OK, false);
	return data.type == VS::LIGHT_DIRECTIONAL && (data.flags & FixedFunctionLightStorage::LIGHT_SHADOW) && _queries.supports_directional_shadows() &&
			bool(GLOBAL_GET("rendering/quality/fixed_function/directional_shadows")) && bool(GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting"));
}
