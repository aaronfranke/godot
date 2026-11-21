#include "render_scene_buffers_fixed_function.h"

#include "core/math/math_funcs.h"
#include "drivers/fixed_function/texture_storage.h"

void RenderSceneBuffersFixedFunction::configure(const RenderSceneBuffersConfiguration *p_config) {
	_error = p_config && p_config->get_render_target().is_valid() ? OK : ERR_INVALID_PARAMETER;
	if (_error != OK) {
		return;
	}
	_render_target = p_config->get_render_target();
	if (p_config->get_view_count() != 1) {
		_error = ERR_UNAVAILABLE;
		return;
	}
	if (p_config->get_internal_size() != p_config->get_target_size()) {
		WARN_PRINT_ONCE("Fixed Function draws 3D at the viewport's native size.");
	}
	if (p_config->get_msaa_3d() != RSE::VIEWPORT_MSAA_DISABLED) {
		FixedFunctionTextureStorage::warn_unsupported_msaa();
	}
	if (p_config->get_screen_space_aa() != RSE::VIEWPORT_SCREEN_SPACE_AA_DISABLED || p_config->get_use_taa()) {
		WARN_PRINT_ONCE("Fixed Function draws the viewport without screen-space or temporal antialiasing.");
	}
	set_use_debanding(p_config->get_use_debanding());
}

void RenderSceneBuffersFixedFunction::set_fsr_sharpness(float p_sharpness) {
	// Quality metadata has no effect without the unavailable scaling pass.
	if (!Math::is_finite(p_sharpness)) {
		_error = ERR_INVALID_PARAMETER;
	}
}

void RenderSceneBuffersFixedFunction::set_texture_mipmap_bias(float p_bias) {
	if (!Math::is_finite(p_bias)) {
		_error = ERR_INVALID_PARAMETER;
	}
}

void RenderSceneBuffersFixedFunction::set_anisotropic_filtering_level(RSE::ViewportAnisotropicFiltering p_level) {
	if (p_level < 0 || p_level >= RSE::VIEWPORT_ANISOTROPY_MAX) {
		_error = ERR_INVALID_PARAMETER;
	}
}

void RenderSceneBuffersFixedFunction::set_use_debanding(bool p_enabled) {
	if (p_enabled) {
		WARN_PRINT_ONCE("Fixed Function draws the viewport without debanding.");
	}
}

RenderSceneBuffersFixedFunction::RenderSceneBuffersFixedFunction() : _error(ERR_UNCONFIGURED) {}
