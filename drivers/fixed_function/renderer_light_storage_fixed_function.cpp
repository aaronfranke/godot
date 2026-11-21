#include "renderer_light_storage_fixed_function.h"

#include "core/config/project_settings.h"

void RendererLightStorageFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP) {
		ERR_PRINT(String("Fixed Function RendererLightStorage ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

void RendererLightStorageFixedFunction::_unsupported(const char *p_operation) {
	ERR_PRINT(String("Fixed Function RendererLightStorage ") + p_operation + " is unavailable.");
}

RID RendererLightStorageFixedFunction::directional_light_allocate() {
	return _storage.light_allocate();
}

void RendererLightStorageFixedFunction::directional_light_initialize(RID p_rid) {
	_report_error(_storage.light_initialize(p_rid, RSE::LIGHT_DIRECTIONAL), "directional_light_initialize");
}

RID RendererLightStorageFixedFunction::omni_light_allocate() {
	return _storage.light_allocate();
}

void RendererLightStorageFixedFunction::omni_light_initialize(RID p_rid) {
	_report_error(_storage.light_initialize(p_rid, RSE::LIGHT_OMNI), "omni_light_initialize");
}

RID RendererLightStorageFixedFunction::spot_light_allocate() {
	return _storage.light_allocate();
}

void RendererLightStorageFixedFunction::spot_light_initialize(RID p_rid) {
	_report_error(_storage.light_initialize(p_rid, RSE::LIGHT_SPOT), "spot_light_initialize");
}

RID RendererLightStorageFixedFunction::area_light_allocate() {
	_report_error(ERR_UNAVAILABLE, "area_light_allocate");
	return RID();
}

void RendererLightStorageFixedFunction::area_light_initialize(RID p_rid) {
	_report_error(ERR_UNAVAILABLE, "area_light_initialize");
}

void RendererLightStorageFixedFunction::light_free(RID p_rid) {
	_report_error(_storage.free(p_rid), "light_free");
}

void RendererLightStorageFixedFunction::light_set_color(RID p_light, const Color &p_color) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.color = p_color;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_color");
}

void RendererLightStorageFixedFunction::light_set_param(RID p_light, RSE::LightParam p_param, float p_value) {
	if (p_param < 0 || p_param >= RSE::LIGHT_PARAM_MAX) {
		_report_error(ERR_INVALID_PARAMETER, "light_set_param");
		return;
	}
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.parameters[p_param] = p_value;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_param");
}

void RendererLightStorageFixedFunction::light_set_shadow(RID p_light, bool p_enabled) {
	_set_flag(p_light, FixedFunctionLightStorage::LIGHT_SHADOW, p_enabled);
}

void RendererLightStorageFixedFunction::light_set_projector(RID p_light, RID p_texture) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.projector = p_texture;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_projector");
}

void RendererLightStorageFixedFunction::light_set_negative(RID p_light, bool p_enable) {
	_set_flag(p_light, FixedFunctionLightStorage::LIGHT_NEGATIVE, p_enable);
}

void RendererLightStorageFixedFunction::light_set_cull_mask(RID p_light, uint32_t p_mask) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.cull_mask = p_mask;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_cull_mask");
}

void RendererLightStorageFixedFunction::light_set_distance_fade(RID p_light, bool p_enabled, float p_begin, float p_shadow, float p_length) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.distance_fade_enabled = p_enabled;
		data.distance_fade_begin = p_begin;
		data.distance_fade_shadow = p_shadow;
		data.distance_fade_length = p_length;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_distance_fade");
}

void RendererLightStorageFixedFunction::light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) {
	_set_flag(p_light, FixedFunctionLightStorage::LIGHT_REVERSE_CULL, p_enabled);
}

void RendererLightStorageFixedFunction::light_set_shadow_caster_mask(RID p_light, uint32_t p_caster_mask) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.shadow_caster_mask = p_caster_mask;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_shadow_caster_mask");
}

uint32_t RendererLightStorageFixedFunction::light_get_shadow_caster_mask(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK ? data.shadow_caster_mask : 0;
}

void RendererLightStorageFixedFunction::light_set_bake_mode(RID p_light, RSE::LightBakeMode p_bake_mode) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.bake_mode = p_bake_mode;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_bake_mode");
}

void RendererLightStorageFixedFunction::light_set_max_sdfgi_cascade(RID p_light, uint32_t p_cascade) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.max_sdfgi_cascade = p_cascade;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_max_sdfgi_cascade");
}

void RendererLightStorageFixedFunction::light_set_allow_contact_shadows(RID p_light, bool p_enable) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.allow_contact_shadows = p_enable;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_allow_contact_shadows");
}

bool RendererLightStorageFixedFunction::light_get_allow_contact_shadows(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK ? data.allow_contact_shadows : false;
}

void RendererLightStorageFixedFunction::light_omni_set_shadow_mode(RID p_light, RSE::LightOmniShadowMode p_mode) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.omni_shadow_mode = p_mode;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_omni_set_shadow_mode");
}

void RendererLightStorageFixedFunction::light_directional_set_shadow_mode(RID p_light, RSE::LightDirectionalShadowMode p_mode) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.directional_shadow_mode = p_mode;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_directional_set_shadow_mode");
}

void RendererLightStorageFixedFunction::light_directional_set_blend_splits(RID p_light, bool p_enable) {
	_set_flag(p_light, FixedFunctionLightStorage::LIGHT_BLEND_SPLITS, p_enable);
}

bool RendererLightStorageFixedFunction::light_directional_get_blend_splits(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK && (data.flags & FixedFunctionLightStorage::LIGHT_BLEND_SPLITS);
}

void RendererLightStorageFixedFunction::light_directional_set_sky_mode(RID p_light, RSE::LightDirectionalSkyMode p_mode) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.sky_mode = p_mode;
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_directional_set_sky_mode");
}

RSE::LightDirectionalSkyMode RendererLightStorageFixedFunction::light_directional_get_sky_mode(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK ? data.sky_mode : RSE::LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_AND_SKY;
}

RSE::LightDirectionalShadowMode RendererLightStorageFixedFunction::light_directional_get_shadow_mode(RID p_light) {
	FixedFunctionLightStorage::FFLightData data;
	if (_storage.light_get_data(p_light, data) != OK) {
		return RSE::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL;
	}
	// Retain requested cascades in CPU data; this profile executes one full-range map.
	return light_has_shadow(p_light) ? RSE::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL : data.directional_shadow_mode;
}

RSE::LightOmniShadowMode RendererLightStorageFixedFunction::light_omni_get_shadow_mode(RID p_light) {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK ? data.omni_shadow_mode : RSE::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID;
}

void RendererLightStorageFixedFunction::light_area_set_size(RID p_light, const Vector2 &p_size) {
	_report_error(ERR_UNAVAILABLE, "light_area_set_size");
}

Vector2 RendererLightStorageFixedFunction::light_area_get_size(RID p_light) const {
	_unsupported("light_area_get_size");
	return Vector2();
}

void RendererLightStorageFixedFunction::light_area_set_normalize_energy(RID p_light, bool p_enabled) {
	_report_error(ERR_UNAVAILABLE, "light_area_set_normalize_energy");
}

bool RendererLightStorageFixedFunction::light_area_get_normalize_energy(RID p_light) const {
	_unsupported("light_area_get_normalize_energy");
	return false;
}

void RendererLightStorageFixedFunction::light_area_set_texture(RID p_light, RID p_texture) {
	_report_error(ERR_UNAVAILABLE, "light_area_set_texture");
}

RID RendererLightStorageFixedFunction::light_area_get_texture(RID p_light) const {
	_unsupported("light_area_get_texture");
	return RID();
}

bool RendererLightStorageFixedFunction::light_has_shadow(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK && data.type == RSE::LIGHT_DIRECTIONAL &&
			(data.flags & FixedFunctionLightStorage::LIGHT_SHADOW) && _get_directional_shadow_size() > 0;
}

bool RendererLightStorageFixedFunction::light_has_projector(RID p_light) const {
	// Retain requested settings in CPU data; these passes are not executed yet.
	return false;
}

RSE::LightType RendererLightStorageFixedFunction::light_get_type(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK ? data.type : RSE::LIGHT_DIRECTIONAL;
}

AABB RendererLightStorageFixedFunction::light_get_aabb(RID p_light) const {
	AABB bounds;
	_storage.light_get_aabb(p_light, bounds);
	return bounds;
}

float RendererLightStorageFixedFunction::light_get_param(RID p_light, RSE::LightParam p_param) {
	FixedFunctionLightStorage::FFLightData data;
	return p_param >= 0 && p_param < RSE::LIGHT_PARAM_MAX && _storage.light_get_data(p_light, data) == OK ? data.parameters[p_param] : 0;
}

Color RendererLightStorageFixedFunction::light_get_color(RID p_light) {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK ? data.color : Color();
}

bool RendererLightStorageFixedFunction::light_get_reverse_cull_face_mode(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK && (data.flags & FixedFunctionLightStorage::LIGHT_REVERSE_CULL);
}

RSE::LightBakeMode RendererLightStorageFixedFunction::light_get_bake_mode(RID p_light) {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK ? data.bake_mode : RSE::LIGHT_BAKE_DISABLED;
}

uint32_t RendererLightStorageFixedFunction::light_get_max_sdfgi_cascade(RID p_light) {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK ? data.max_sdfgi_cascade : 0;
}

uint64_t RendererLightStorageFixedFunction::light_get_version(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK ? data.version : 0;
}

uint32_t RendererLightStorageFixedFunction::light_get_cull_mask(RID p_light) const {
	FixedFunctionLightStorage::FFLightData data;
	return _storage.light_get_data(p_light, data) == OK ? data.cull_mask : 0;
}

RID RendererLightStorageFixedFunction::light_instance_create(RID p_light) {
	return _storage.light_instance_create(p_light);
}

void RendererLightStorageFixedFunction::light_instance_free(RID p_light_instance) {
	_report_error(_storage.free(p_light_instance), "light_instance_free");
}

void RendererLightStorageFixedFunction::light_instance_set_transform(RID p_light_instance, const Transform3D &p_transform) {
	_report_error(_storage.light_instance_set_transform(p_light_instance, p_transform), "light_instance_set_transform");
}

void RendererLightStorageFixedFunction::light_instance_set_aabb(RID p_light_instance, const AABB &p_aabb) {
	// The scene culler owns world bounds; shading uses source range and transform.
	_report_error(_storage.owns_instance(p_light_instance) && p_aabb.position.is_finite() && p_aabb.size.is_finite() ? OK : ERR_INVALID_PARAMETER, "light_instance_set_aabb");
}

void RendererLightStorageFixedFunction::light_instance_set_shadow_transform(RID p_light_instance, const Projection &p_projection, const Transform3D &p_transform, float p_far, float p_split, int p_pass, float p_shadow_texel_size, float p_bias_scale, float p_range_begin, const Vector2 &p_uv_scale) {
	_report_error(_storage.light_instance_set_shadow_transform(p_light_instance, p_projection, p_transform, p_pass, p_shadow_texel_size, p_bias_scale), "light_instance_set_shadow_transform");
}

void RendererLightStorageFixedFunction::light_instance_mark_visible(RID p_light_instance) {
	_report_error(_storage.owns_instance(p_light_instance) ? OK : ERR_INVALID_PARAMETER, "light_instance_mark_visible");
}

bool RendererLightStorageFixedFunction::light_instance_is_shadow_visible_at_position(RID p_light, const Vector3 &p_position) const {
	return false;
}

RID RendererLightStorageFixedFunction::reflection_probe_allocate() {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_allocate");
	return RID();
}

void RendererLightStorageFixedFunction::reflection_probe_initialize(RID p_rid) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_initialize");
}

void RendererLightStorageFixedFunction::reflection_probe_free(RID p_rid) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_free");
}

void RendererLightStorageFixedFunction::reflection_probe_set_update_mode(RID p_probe, RSE::ReflectionProbeUpdateMode p_mode) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_update_mode");
}

void RendererLightStorageFixedFunction::reflection_probe_set_resolution(RID p_probe, int p_resolution) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_resolution");
}

void RendererLightStorageFixedFunction::reflection_probe_set_intensity(RID p_probe, float p_intensity) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_intensity");
}

void RendererLightStorageFixedFunction::reflection_probe_set_blend_distance(RID p_probe, float p_blend_distance) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_blend_distance");
}

void RendererLightStorageFixedFunction::reflection_probe_set_ambient_mode(RID p_probe, RSE::ReflectionProbeAmbientMode p_mode) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_ambient_mode");
}

void RendererLightStorageFixedFunction::reflection_probe_set_ambient_color(RID p_probe, const Color &p_color) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_ambient_color");
}

void RendererLightStorageFixedFunction::reflection_probe_set_ambient_energy(RID p_probe, float p_energy) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_ambient_energy");
}

void RendererLightStorageFixedFunction::reflection_probe_set_max_distance(RID p_probe, float p_distance) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_max_distance");
}

void RendererLightStorageFixedFunction::reflection_probe_set_size(RID p_probe, const Vector3 &p_size) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_size");
}

void RendererLightStorageFixedFunction::reflection_probe_set_origin_offset(RID p_probe, const Vector3 &p_offset) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_origin_offset");
}

void RendererLightStorageFixedFunction::reflection_probe_set_as_interior(RID p_probe, bool p_enable) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_as_interior");
}

void RendererLightStorageFixedFunction::reflection_probe_set_enable_box_projection(RID p_probe, bool p_enable) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_enable_box_projection");
}

void RendererLightStorageFixedFunction::reflection_probe_set_enable_shadows(RID p_probe, bool p_enable) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_enable_shadows");
}

void RendererLightStorageFixedFunction::reflection_probe_set_cull_mask(RID p_probe, uint32_t p_layers) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_cull_mask");
}

void RendererLightStorageFixedFunction::reflection_probe_set_reflection_mask(RID p_probe, uint32_t p_layers) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_reflection_mask");
}

void RendererLightStorageFixedFunction::reflection_probe_set_mesh_lod_threshold(RID p_probe, float p_ratio) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_set_mesh_lod_threshold");
}

AABB RendererLightStorageFixedFunction::reflection_probe_get_aabb(RID p_probe) const {
	_unsupported("reflection_probe_get_aabb");
	return AABB();
}

RSE::ReflectionProbeUpdateMode RendererLightStorageFixedFunction::reflection_probe_get_update_mode(RID p_probe) const {
	_unsupported("reflection_probe_get_update_mode");
	return static_cast<RSE::ReflectionProbeUpdateMode>(0);
}

uint32_t RendererLightStorageFixedFunction::reflection_probe_get_cull_mask(RID p_probe) const {
	_unsupported("reflection_probe_get_cull_mask");
	return 0;
}

uint32_t RendererLightStorageFixedFunction::reflection_probe_get_reflection_mask(RID p_probe) const {
	_unsupported("reflection_probe_get_reflection_mask");
	return 0;
}

Vector3 RendererLightStorageFixedFunction::reflection_probe_get_size(RID p_probe) const {
	_unsupported("reflection_probe_get_size");
	return Vector3();
}

Vector3 RendererLightStorageFixedFunction::reflection_probe_get_origin_offset(RID p_probe) const {
	_unsupported("reflection_probe_get_origin_offset");
	return Vector3();
}

float RendererLightStorageFixedFunction::reflection_probe_get_origin_max_distance(RID p_probe) const {
	_unsupported("reflection_probe_get_origin_max_distance");
	return 0;
}

bool RendererLightStorageFixedFunction::reflection_probe_renders_shadows(RID p_probe) const {
	_unsupported("reflection_probe_renders_shadows");
	return false;
}

float RendererLightStorageFixedFunction::reflection_probe_get_mesh_lod_threshold(RID p_probe) const {
	_unsupported("reflection_probe_get_mesh_lod_threshold");
	return 0;
}

RID RendererLightStorageFixedFunction::reflection_atlas_create() {
	return _reflections.make_rid(FFAtlas());
}

void RendererLightStorageFixedFunction::reflection_atlas_free(RID p_ref_atlas) {
	if (_reflections.owns(p_ref_atlas)) {
		_reflections.free(p_ref_atlas);
		_last_error = OK;
	} else {
		_report_error(ERR_INVALID_PARAMETER, "atlas_free");
	}
}

void RendererLightStorageFixedFunction::reflection_atlas_set_size(RID p_ref_atlas, int p_reflection_size, int p_reflection_count) {
	FFAtlas *atlas = _reflections.get_or_null(p_ref_atlas);
	if (!atlas || p_reflection_size < 0 || p_reflection_count < 0) {
		_report_error(ERR_INVALID_PARAMETER, "reflection_atlas_set_size");
		return;
	}
	atlas->size = p_reflection_size;
	atlas->count = p_reflection_count;
	_last_error = OK;
}

int RendererLightStorageFixedFunction::reflection_atlas_get_size(RID p_ref_atlas) const {
	const FFAtlas *atlas = _reflections.get_or_null(p_ref_atlas);
	return atlas ? atlas->size : 0;
}

RID RendererLightStorageFixedFunction::reflection_probe_instance_create(RID p_probe) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_create");
	return RID();
}

void RendererLightStorageFixedFunction::reflection_probe_instance_free(RID p_instance) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_free");
}

void RendererLightStorageFixedFunction::reflection_probe_instance_set_transform(RID p_instance, const Transform3D &p_transform) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_set_transform");
}

bool RendererLightStorageFixedFunction::reflection_probe_has_atlas_index(RID p_instance) {
	_unsupported("reflection_probe_has_atlas_index");
	return false;
}

void RendererLightStorageFixedFunction::reflection_probe_release_atlas_index(RID p_instance) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_release_atlas_index");
}

bool RendererLightStorageFixedFunction::reflection_probe_instance_needs_redraw(RID p_instance) {
	_unsupported("reflection_probe_instance_needs_redraw");
	return false;
}

bool RendererLightStorageFixedFunction::reflection_probe_instance_has_reflection(RID p_instance) {
	_unsupported("reflection_probe_instance_has_reflection");
	return false;
}

bool RendererLightStorageFixedFunction::reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_begin_render");
	return false;
}

bool RendererLightStorageFixedFunction::reflection_probe_instance_end_render(RID p_instance, RID p_reflection_atlas) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_end_render");
	return false;
}

Ref<RenderSceneBuffers> RendererLightStorageFixedFunction::reflection_probe_atlas_get_render_buffers(RID p_reflection_atlas) {
	_unsupported("reflection_probe_atlas_get_render_buffers");
	return Ref<RenderSceneBuffers>();
}

bool RendererLightStorageFixedFunction::reflection_probe_instance_postprocess_step(RID p_instance) {
	_report_error(ERR_UNAVAILABLE, "reflection_probe_instance_postprocess_step");
	return false;
}

Dependency *RendererLightStorageFixedFunction::lightmap_get_dependency(RID p_lightmap) {
	FFLightmap *lightmap = _lightmaps.get_or_null(p_lightmap);
	return lightmap ? &lightmap->dependency : nullptr;
}

RID RendererLightStorageFixedFunction::lightmap_allocate() {
	return _lightmaps.allocate_rid();
}

void RendererLightStorageFixedFunction::lightmap_initialize(RID p_rid) {
	_lightmaps.initialize_rid(p_rid);
}

void RendererLightStorageFixedFunction::lightmap_free(RID p_rid) {
	FFLightmap *lightmap = _lightmaps.get_or_null(p_rid);
	ERR_FAIL_NULL(lightmap);
	lightmap->dependency.deleted_notify(p_rid);
	_lightmaps.free(p_rid);
}

void RendererLightStorageFixedFunction::lightmap_set_textures(RID p_lightmap, RID p_light, bool p_uses_spherical_haromics) {}

void RendererLightStorageFixedFunction::lightmap_set_probe_bounds(RID p_lightmap, const AABB &p_bounds) {}

void RendererLightStorageFixedFunction::lightmap_set_probe_interior(RID p_lightmap, bool p_interior) {}

void RendererLightStorageFixedFunction::lightmap_set_probe_capture_data(RID p_lightmap, const PackedVector3Array &p_points, const PackedColorArray &p_point_sh, const PackedInt32Array &p_tetrahedra, const PackedInt32Array &p_bsp_tree) {
	FFLightmap *lightmap = _lightmaps.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lightmap);
	// LightmapGIData serializes these arrays through the renderer.
	lightmap->points = p_points;
	lightmap->spherical_harmonics = p_point_sh;
	lightmap->tetrahedra = p_tetrahedra;
	lightmap->bsp_tree = p_bsp_tree;
}

void RendererLightStorageFixedFunction::lightmap_set_baked_exposure_normalization(RID p_lightmap, float p_exposure) {}

PackedVector3Array RendererLightStorageFixedFunction::lightmap_get_probe_capture_points(RID p_lightmap) const {
	const FFLightmap *lightmap = _lightmaps.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lightmap, PackedVector3Array());
	return lightmap->points;
}

PackedColorArray RendererLightStorageFixedFunction::lightmap_get_probe_capture_sh(RID p_lightmap) const {
	const FFLightmap *lightmap = _lightmaps.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lightmap, PackedColorArray());
	return lightmap->spherical_harmonics;
}

PackedInt32Array RendererLightStorageFixedFunction::lightmap_get_probe_capture_tetrahedra(RID p_lightmap) const {
	const FFLightmap *lightmap = _lightmaps.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lightmap, PackedInt32Array());
	return lightmap->tetrahedra;
}

PackedInt32Array RendererLightStorageFixedFunction::lightmap_get_probe_capture_bsp_tree(RID p_lightmap) const {
	const FFLightmap *lightmap = _lightmaps.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lightmap, PackedInt32Array());
	return lightmap->bsp_tree;
}

AABB RendererLightStorageFixedFunction::lightmap_get_aabb(RID p_lightmap) const {
	// Keep inert lightmaps out of probe culling and mesh pairing.
	return AABB();
}

void RendererLightStorageFixedFunction::lightmap_tap_sh_light(RID p_lightmap, const Vector3 &p_point, Color *r_sh) {
	for (int i = 0; i < 9; i++) {
		r_sh[i] = Color(0, 0, 0, 0);
	}
}

bool RendererLightStorageFixedFunction::lightmap_is_interior(RID p_lightmap) const {
	return false;
}

void RendererLightStorageFixedFunction::lightmap_set_probe_capture_update_speed(float p_speed) {
	_report_error(Math::is_finite(p_speed) && p_speed >= 0 ? OK : ERR_INVALID_PARAMETER, "lightmap_set_probe_capture_update_speed");
}

float RendererLightStorageFixedFunction::lightmap_get_probe_capture_update_speed() const {
	// The scene server polls this even when no lightmap resources exist.
	return 0;
}

void RendererLightStorageFixedFunction::lightmap_set_shadowmask_textures(RID p_lightmap, RID p_shadow) {}

RSE::ShadowmaskMode RendererLightStorageFixedFunction::lightmap_get_shadowmask_mode(RID p_lightmap) {
	const FFLightmap *lightmap = _lightmaps.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lightmap, RSE::SHADOWMASK_MODE_NONE);
	return lightmap->shadowmask_mode;
}

void RendererLightStorageFixedFunction::lightmap_set_shadowmask_mode(RID p_lightmap, RSE::ShadowmaskMode p_mode) {
	FFLightmap *lightmap = _lightmaps.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lightmap);
	lightmap->shadowmask_mode = p_mode;
}

float RendererLightStorageFixedFunction::lightmap_get_specular_intensity(RID p_lightmap) {
	const FFLightmap *lightmap = _lightmaps.get_or_null(p_lightmap);
	ERR_FAIL_NULL_V(lightmap, 0.0f);
	return lightmap->specular_intensity;
}

void RendererLightStorageFixedFunction::lightmap_set_specular_intensity(RID p_lightmap, float p_intensity) {
	FFLightmap *lightmap = _lightmaps.get_or_null(p_lightmap);
	ERR_FAIL_NULL(lightmap);
	lightmap->specular_intensity = p_intensity;
}

RID RendererLightStorageFixedFunction::lightmap_instance_create(RID p_lightmap) {
	ERR_FAIL_COND_V(!owns_lightmap(p_lightmap), RID());
	return _lightmap_instances.make_rid(p_lightmap);
}

void RendererLightStorageFixedFunction::lightmap_instance_free(RID p_lightmap) {
	_lightmap_instances.free(p_lightmap);
}

void RendererLightStorageFixedFunction::lightmap_instance_set_transform(RID p_lightmap, const Transform3D &p_transform) {}

RID RendererLightStorageFixedFunction::shadow_atlas_create() {
	return _shadows.make_rid(FFAtlas());
}

void RendererLightStorageFixedFunction::shadow_atlas_free(RID p_atlas) {
	if (_shadows.owns(p_atlas)) {
		_shadows.free(p_atlas);
		_last_error = OK;
	} else {
		_report_error(ERR_INVALID_PARAMETER, "atlas_free");
	}
}

void RendererLightStorageFixedFunction::shadow_atlas_set_size(RID p_atlas, int p_size, bool p_use_16_bits) {
	FFAtlas *atlas = _shadows.get_or_null(p_atlas);
	if (!atlas || p_size < 0) {
		_report_error(ERR_INVALID_PARAMETER, "shadow_atlas_set_size");
		return;
	}
	atlas->size = p_size;
	_last_error = OK;
}

void RendererLightStorageFixedFunction::shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) {
	FFAtlas *atlas = _shadows.get_or_null(p_atlas);
	if (!atlas || p_quadrant < 0 || p_quadrant >= 4 || p_subdivision < 0) {
		_report_error(ERR_INVALID_PARAMETER, "shadow_atlas_set_quadrant_subdivision");
		return;
	}
	atlas->subdivisions[p_quadrant] = p_subdivision;
	_last_error = OK;
}

bool RendererLightStorageFixedFunction::shadow_atlas_update_light(RID p_atlas, RID p_light_instance, float p_coverage, uint64_t p_light_version) {
	_report_error(ERR_UNAVAILABLE, "shadow_atlas_update_light");
	return false;
}

void RendererLightStorageFixedFunction::shadow_atlas_update(RID p_atlas) {
	_report_error(_shadows.owns(p_atlas) ? OK : ERR_INVALID_PARAMETER, "shadow_atlas_update");
}

void RendererLightStorageFixedFunction::directional_shadow_atlas_set_size(int p_size, bool p_16_bits) {
	if (p_size >= 0) {
		_directional_shadow_size = p_size;
	}
	_report_error(p_size >= 0 ? OK : ERR_INVALID_PARAMETER, "directional_shadow_atlas_set_size");
}

int RendererLightStorageFixedFunction::get_directional_light_shadow_size(RID p_light_instance) {
	if (!_storage.owns_instance(p_light_instance)) {
		_report_error(ERR_INVALID_PARAMETER, "get_directional_light_shadow_size");
		return 0;
	}
	return _get_directional_shadow_size();
}

int RendererLightStorageFixedFunction::_get_directional_shadow_size() const {
	const FixedFunctionGraphicsDevice &device = _context.get_device();
	if (!_directional_shadow_size || !device.supports_directional_shadows() || !bool(GLOBAL_GET("rendering/quality/fixed_function/directional_shadows")) || !bool(GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting"))) {
		return 0;
	}
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = device.get_capabilities();
	const uint32_t maximum = MIN(caps.max_texture_width, caps.max_texture_height);
	uint32_t size = Math::nearest_power_of_2_templated(uint32_t(CLAMP(_directional_shadow_size, 128, 4096)));
	while (size > maximum) {
		size /= 2;
	}
	return size >= 64 ? int(size) : 0;
}

void RendererLightStorageFixedFunction::set_directional_shadow_count(int p_count) {
	_report_error(p_count >= 0 ? OK : ERR_INVALID_PARAMETER, "set_directional_shadow_count");
}

RendererLightStorageFixedFunction::RendererLightStorageFixedFunction(FixedFunctionRenderContext &p_context) : _context(p_context), _storage(p_context.get_light_storage()), _directional_shadow_size(1024), _last_error(OK) {}

bool RendererLightStorageFixedFunction::free(RID p_rid) {
	if (_storage.owns_light(p_rid) || _storage.owns_instance(p_rid)) {
		return _storage.free(p_rid) == OK;
	}
	if (owns_lightmap(p_rid)) {
		lightmap_free(p_rid);
		return true;
	}
	if (_lightmap_instances.owns(p_rid)) {
		lightmap_instance_free(p_rid);
		return true;
	}
	if (_shadows.owns(p_rid)) {
		_shadows.free(p_rid);
		return true;
	}
	if (_reflections.owns(p_rid)) {
		_reflections.free(p_rid);
		return true;
	}
	return false;
}

RendererLightStorageFixedFunction::~RendererLightStorageFixedFunction() {
	for (RID rid : _lightmaps.get_owned_list()) {
		lightmap_free(rid);
	}
	for (RID rid : _lightmap_instances.get_owned_list()) {
		lightmap_instance_free(rid);
	}
	for (RID rid : _shadows.get_owned_list()) {
		_shadows.free(rid);
	}
	for (RID rid : _reflections.get_owned_list()) {
		_reflections.free(rid);
	}
}

RendererLightStorageFixedFunction::FFAtlas::FFAtlas() : size(0), count(0) {
	for (int i = 0; i < 4; i++) {
		subdivisions[i] = 0;
	}
}

void RendererLightStorageFixedFunction::_set_flag(RID p_light, FixedFunctionLightStorage::FFLightFlag p_flag, bool p_enabled) {
	FixedFunctionLightStorage::FFLightData data;
	Error error = _storage.light_get_data(p_light, data);
	if (error == OK) {
		data.flags = p_enabled ? (data.flags | p_flag) : (data.flags & ~p_flag);
		error = _storage.light_set_data(p_light, data);
	}
	_report_error(error, "light_set_flag");
}
