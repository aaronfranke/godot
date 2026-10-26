#include "renderer_fog_fixed_function.h"

void RendererFogFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP) {
		ERR_PRINT(String("Fixed Function RendererFog ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

void RendererFogFixedFunction::_unsupported(const char *p_operation) {
	ERR_PRINT(String("Fixed Function RendererFog ") + p_operation + " is unavailable.");
}

RID RendererFogFixedFunction::fog_volume_allocate() {
	_report_error(ERR_UNAVAILABLE, "fog_volume_allocate");
	return RID();
}

void RendererFogFixedFunction::fog_volume_initialize(RID p_rid) {
	_report_error(ERR_UNAVAILABLE, "fog_volume_initialize");
}

void RendererFogFixedFunction::fog_volume_free(RID p_rid) {
	_report_error(ERR_UNAVAILABLE, "fog_volume_free");
}

void RendererFogFixedFunction::fog_volume_set_shape(RID p_fog_volume, RSE::FogVolumeShape p_shape) {
	_report_error(ERR_UNAVAILABLE, "fog_volume_set_shape");
}

void RendererFogFixedFunction::fog_volume_set_size(RID p_fog_volume, const Vector3 &p_size) {
	_report_error(ERR_UNAVAILABLE, "fog_volume_set_size");
}

void RendererFogFixedFunction::fog_volume_set_material(RID p_fog_volume, RID p_material) {
	_report_error(ERR_UNAVAILABLE, "fog_volume_set_material");
}

AABB RendererFogFixedFunction::fog_volume_get_aabb(RID p_fog_volume) const {
	_unsupported("fog_volume_get_aabb");
	return AABB();
}

RSE::FogVolumeShape RendererFogFixedFunction::fog_volume_get_shape(RID p_fog_volume) const {
	_unsupported("fog_volume_get_shape");
	return static_cast<RSE::FogVolumeShape>(0);
}

RendererFogFixedFunction::RendererFogFixedFunction() : _last_error(OK) {}
