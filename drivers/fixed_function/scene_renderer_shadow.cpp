#include "scene_renderer.h"

#include "core/project_settings.h"

#include <string.h>

int FixedFunctionSceneRenderer::get_directional_shadow_size() const {
	const FixedFunctionGraphicsDevice &device = _context.get_device();
	if (!device.supports_directional_shadows() || !bool(GLOBAL_GET("rendering/quality/fixed_function/directional_shadows")) || !bool(GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting"))) {
		return 0;
	}
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = device.get_capabilities();
	const uint32_t maximum = MIN(caps.max_texture_width, caps.max_texture_height);
	// A square power-of-two map also works on the oldest optional SM3 adapters.
	const int requested = CLAMP(int(GLOBAL_GET("rendering/quality/directional_shadow/size")), 128, 4096);
	uint32_t size = next_power_of_2(uint32_t(requested));
	while (size > maximum) {
		size /= 2;
	}
	return size >= 64 ? int(size) : 0;
}

void FixedFunctionSceneRenderer::begin_directional_shadows() {
	_shadow_ready = false;
	_shadow_transform.light = RID();
	_pending_shadow.light = RID();
	_shadow_parameters.texture = RID();
}

Error FixedFunctionSceneRenderer::set_directional_shadow_transform(RID p_light, const CameraMatrix &p_projection, const Transform &p_transform, int p_pass, float p_bias_scale) {
	if (!_context.get_light_storage().owns_instance(p_light) || p_pass != 0 || !_finite_transform(p_transform) || p_transform.basis.determinant() == 0 || Math::is_nan(p_bias_scale) || Math::is_inf(p_bias_scale)) {
		return ERR_INVALID_PARAMETER;
	}
	for (int column = 0; column < 4; column++) {
		for (int row = 0; row < 4; row++) {
			const real_t value = p_projection.matrix[column][row];
			if (Math::is_nan(value) || Math::is_inf(value)) {
				return ERR_INVALID_PARAMETER;
			}
		}
	}
	// The first profile uses the culler's one orthographic directional pass.
	if (p_projection.matrix[3][3] != 1 || p_projection.matrix[2][3] != 0) {
		return ERR_UNAVAILABLE;
	}
	_pending_shadow.projection = p_projection;
	_pending_shadow.transform = p_transform;
	_pending_shadow.light = p_light;
	_pending_shadow.bias_scale = p_bias_scale;
	_pending_shadow.instance_id = p_light.get_id();
	return OK;
}

Error FixedFunctionSceneRenderer::render_directional_shadow(RID p_light, int p_pass, RasterizerScene::InstanceBase **p_instances, int p_count) {
	if (p_count < 0 || (p_count && !p_instances) || p_pass != 0 || p_light != _pending_shadow.light) {
		return ERR_INVALID_PARAMETER;
	}
	const int size = get_directional_shadow_size();
	if (!size || _shadow_ready) {
		return OK; // One map: later eligible lights keep their ordinary unshadowed contribution.
	}
	FixedFunctionLightStorage::FFLightInstanceData instance;
	FixedFunctionLightStorage::FFLightData light;
	Error error = _context.get_light_storage().light_instance_get_data(p_light, instance);
	if (error == OK) {
		error = _context.get_light_storage().light_get_data(instance.light, light);
	}
	if (error != OK || light.type != VS::LIGHT_DIRECTIONAL) {
		return error != OK ? error : ERR_INVALID_PARAMETER;
	}
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	if (!_shadow_target.is_valid()) {
		_shadow_target = textures.render_target_create();
		if (!_shadow_target.is_valid()) {
			return ERR_CANT_CREATE;
		}
	}
	error = textures.render_target_set_size(_shadow_target, size, size);
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
	error = _instances_drawn.resize(p_count);
	if (error == OK && p_count) {
		memset(_instances_drawn.ptrw(), 0, p_count);
	}
	if (error == OK) {
		error = _render_targets.clear_render_target(Color(1, 1, 1));
	}
	if (error == OK) {
		error = _render_targets.begin_draw(true);
	}
	if (error == OK) {
		error = _prepare(_pending_shadow.transform, _pending_shadow.projection, p_instances, p_count);
	}
	if (error == OK) {
		error = _submit(_pending_shadow.transform, _pending_shadow.projection, RID());
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
		// A temporarily unavailable caster texture must retry the real viewport,
		// including UPDATE_ONCE, rather than only the renderer's private shadow target.
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
	_shadow_transform = _pending_shadow;
	_shadow_frame = _context.get_frame_number();
	_shadow_parameters.color = light.shadow_color;
	// Godot 3.x GLES offsets light-view Z in world units. Convert that same bias
	// into [0,1] depth for the receiver comparison, retaining requested CPU values.
	const float bias_scale = Math::lerp(1.0f, _pending_shadow.bias_scale, light.parameters[VS::LIGHT_PARAM_SHADOW_BIAS_SPLIT_SCALE]);
	const real_t depth_scale = Math::abs(_pending_shadow.projection.matrix[2][2]) * real_t(0.5);
	_shadow_parameters.bias = float(light.parameters[VS::LIGHT_PARAM_SHADOW_BIAS] * bias_scale * depth_scale);
	_shadow_parameters.normal_bias = float(light.parameters[VS::LIGHT_PARAM_SHADOW_NORMAL_BIAS] * bias_scale * depth_scale);
	_shadow_ready = true;
	return OK;
}

FixedFunctionSceneRenderer::~FixedFunctionSceneRenderer() {
	if (_context.is_initialized() && _shadow_target.is_valid() && _context.get_texture_storage().owns_render_target(_shadow_target)) {
		_context.get_texture_storage().request_free(_shadow_target);
	}
}
