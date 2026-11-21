#pragma once

#include "servers/rendering/storage/render_scene_buffers.h"

// CPU configuration for the empty-scene bridge; no 3D buffers are allocated.
class RenderSceneBuffersFixedFunction : public RenderSceneBuffers {
	GDCLASS(RenderSceneBuffersFixedFunction, RenderSceneBuffers);

	RID _render_target;
	Error _error;

protected:
	static void _bind_methods() {}

public:
	RenderSceneBuffersFixedFunction();
	void configure(const RenderSceneBuffersConfiguration *p_config) override;
	void set_fsr_sharpness(float p_sharpness) override;
	void set_texture_mipmap_bias(float p_bias) override;
	void set_anisotropic_filtering_level(RSE::ViewportAnisotropicFiltering p_level) override;
	void set_use_debanding(bool p_enabled) override;
	RID get_render_target() const { return _render_target; }
	Error get_error() const { return _error; }
};
