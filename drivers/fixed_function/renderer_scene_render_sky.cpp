#include "drivers/fixed_function/renderer_scene_render_fixed_function.h"

bool RendererSceneRenderFixedFunction::_get_sky_background(RID p_environment, FixedFunctionSceneRenderer::FFSkyBackground &r_background) {
	const FFSky *sky = _skies.get_or_null(environment_get_sky(p_environment));
	const FixedFunctionMaterialStorage &materials = _context.get_material_storage();
	if (!sky || !materials.owns_material(sky->material)) {
		return false;
	}
	FixedFunctionSceneRenderer::FFSkyBackground &background = r_background;
	background.orientation = environment_get_sky_orientation(p_environment);
	background.fov = environment_get_sky_custom_fov(p_environment);
	background.energy = environment_get_bg_energy_multiplier(p_environment);
	const Variant panorama = materials.material_get_param(sky->material, "source_panorama");
	const Variant top = materials.material_get_param(sky->material, "sky_top_color");
	if (panorama.get_type() == Variant::RID) {
		background.panorama = panorama;
	} else if (top.get_type() == Variant::COLOR) {
		// The standard procedural material supplies these LDR gradient parameters.
		background.procedural = true;
		background.top = top;
		background.horizon = materials.material_get_param(sky->material, "sky_horizon_color");
		background.ground = materials.material_get_param(sky->material, "ground_bottom_color");
		background.ground_horizon = materials.material_get_param(sky->material, "ground_horizon_color");
		background.sky_curve = materials.material_get_param(sky->material, "inv_sky_curve");
		background.ground_curve = materials.material_get_param(sky->material, "inv_ground_curve");
	}
	const Variant exposure = materials.material_get_param(sky->material, "exposure");
	if (exposure.get_type() == Variant::FLOAT || exposure.get_type() == Variant::INT) {
		background.energy *= float(exposure);
	}
	if (!Math::is_finite(background.energy) || !Math::is_finite(background.sky_curve) || !Math::is_finite(background.ground_curve)) {
		return false;
	}
	return true;
}

Error RendererSceneRenderFixedFunction::_render_sky(const CameraData &p_camera, RID p_environment, RenderingServerTypes::RenderInfo *r_info) {
	FixedFunctionSceneRenderer::FFSkyBackground background;
	if (!_get_sky_background(p_environment, background)) {
		return OK;
	}
	return _renderer.render_sky(p_camera.main_transform, p_camera.main_projection, background, r_info);
}
