#include "drivers/fixed_function/scene_renderer.h"

#include <string.h>

void FixedFunctionSceneRenderer::begin_directional_shadows() {
	_shadow_ready = false;
	_shadow_light = RID();
	_shadow_parameters.texture = RID();
}

Error FixedFunctionSceneRenderer::render_directional_shadow(RID p_light, int p_pass, const PagedArray<RenderGeometryInstance *> &p_instances, int p_size, RenderingServerTypes::RenderInfo *r_info, const FFMeshLOD *p_lod) {
	if (p_pass != 0 || p_size < 64) {
		return ERR_INVALID_PARAMETER;
	}
	if (_shadow_ready) {
		return OK; // One map: later eligible lights keep their ordinary unshadowed contribution.
	}
	FixedFunctionLightStorage::FFLightInstanceData instance;
	FixedFunctionLightStorage::FFLightData light;
	FixedFunctionLightStorage::FFShadowTransform shadow;
	Error error = _context.get_light_storage().light_instance_get_data(p_light, instance);
	if (error == OK) {
		error = _context.get_light_storage().light_get_data(instance.light, light);
	}
	if (error == OK) {
		error = _context.get_light_storage().light_instance_get_shadow_transform(p_light, shadow);
	}
	if (error != OK || light.type != RSE::LIGHT_DIRECTIONAL || !shadow.valid) {
		return error != OK ? error : ERR_INVALID_PARAMETER;
	}
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	if (!_shadow_target.is_valid()) {
		_shadow_target = textures.render_target_create();
		if (!_shadow_target.is_valid()) {
			return ERR_CANT_CREATE;
		}
	}
	error = textures.render_target_set_size(_shadow_target, p_size, p_size);
	const RID previous = _render_targets.get_current_render_target();
	if (error == OK) {
		error = _render_targets.select_render_target(_shadow_target);
	}
	if (error != OK) {
		return error;
	}
	_shadow_pass = true;
	_shadow_reverse_cull = (light.flags & FixedFunctionLightStorage::LIGHT_REVERSE_CULL) != 0;
	_redraw_requested = _sort_packets = _lighting_required = false;
	_packets.clear();
	error = _instances_drawn.resize(p_instances.size());
	if (error == OK && p_instances.size()) {
		memset(_instances_drawn.ptrw(), 0, p_instances.size());
	}
	if (error == OK) {
		error = _render_targets.clear_render_target(Color(1, 1, 1));
	}
	if (error == OK) {
		error = _render_targets.begin_draw(true);
	}
	if (error == OK) {
		error = _prepare(shadow.transform, shadow.projection, p_instances, p_lod);
	}
	if (error == OK) {
		error = _submit(shadow.projection, r_info);
	}
	_shadow_pass = false;
	_packets.clear();
	if (error == ERR_UNAVAILABLE) {
		// A refused optional shadow draw must not discard otherwise supported scene geometry.
		_render_targets.report_draw_error(error);
		error = OK;
	} else if (error != OK && error != ERR_SKIP) {
		_render_targets.report_draw_error(error);
	}
	const Error restored = _render_targets.select_render_target(previous);
	if (error == ERR_SKIP && restored == OK && _context.get_frame_status() == OK) {
		// Retry the real viewport, including UPDATE_ONCE, rather than only the private map.
		return _render_targets.defer_render_target();
	}
	if (error != OK) {
		return error;
	}
	if (restored != OK) {
		return restored;
	}
	if (!textures.render_target_is_ready(_shadow_target) || textures.render_target_has_failed(_shadow_target)) {
		return OK;
	}
	_shadow_transform = shadow;
	_shadow_light = p_light; // 4.x RID stores the full uint64_t identity without dereferencing an owner.
	_shadow_frame = _context.get_frame_number();
	const float transmission = 1 - CLAMP(light.parameters[RSE::LIGHT_PARAM_SHADOW_OPACITY], 0.0f, 1.0f);
	_shadow_parameters.color = Color(transmission, transmission, transmission);
	// 4.x directional bias scales with depth range; normal bias is in world texels.
	// The common receiver approximates the latter as a geometry-normal slope depth bias.
	const real_t depth_scale = Math::abs(shadow.projection.columns[2][2]) * real_t(0.5);
	_shadow_parameters.bias = float(light.parameters[RSE::LIGHT_PARAM_SHADOW_BIAS] / 100.0f * shadow.bias_scale * depth_scale);
	_shadow_parameters.normal_bias = float(light.parameters[RSE::LIGHT_PARAM_SHADOW_NORMAL_BIAS] * shadow.texel_size * depth_scale);
	_shadow_ready = true;
	return OK;
}

FixedFunctionSceneRenderer::~FixedFunctionSceneRenderer() {
	if (_context.is_initialized() && _shadow_target.is_valid() && _context.get_texture_storage().owns_render_target(_shadow_target)) {
		_context.get_texture_storage().request_free(_shadow_target);
	}
}
