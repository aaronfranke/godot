#pragma once

#include "drivers/fixed_function/render_context.h"
#include "servers/rendering/environment/renderer_fog.h"

// Explicit restricted-profile contracts; unsupported resources have no owners or handles.
class RendererFogFixedFunction : public RendererFog {
	Error _last_error;
	void _report_error(Error p_error, const char *p_operation);
	static void _unsupported(const char *p_operation);

public:
	RendererFogFixedFunction();
	Error get_last_error() const { return _last_error; }

	RID fog_volume_allocate() override;
	void fog_volume_initialize(RID p_rid) override;
	void fog_volume_free(RID p_rid) override;
	void fog_volume_set_shape(RID p_fog_volume, RSE::FogVolumeShape p_shape) override;
	void fog_volume_set_size(RID p_fog_volume, const Vector3 &p_size) override;
	void fog_volume_set_material(RID p_fog_volume, RID p_material) override;
	AABB fog_volume_get_aabb(RID p_fog_volume) const override;
	RSE::FogVolumeShape fog_volume_get_shape(RID p_fog_volume) const override;
};
