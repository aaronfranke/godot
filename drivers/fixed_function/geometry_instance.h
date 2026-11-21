#pragma once

#include "drivers/fixed_function/render_context.h"
#include "servers/rendering/renderer_geometry_instance.h"

class FixedFunctionGeometryInstance : public RenderGeometryInstanceBase {
	enum {
		PAIRED_LIGHT = 1 << 0,
		PAIRED_REFLECTION = 1 << 1,
		PAIRED_DECAL = 1 << 2,
		PAIRED_VOXEL_GI = 1 << 3,
		PAIRED_SHADOW_OR_PROJECTOR = 1 << 4,
	};
	FixedFunctionRenderContext &_context;
	uint8_t _paired_features;
	static void _dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker);
	static void _dependency_deleted(const RID &p_dependency, DependencyTracker *p_tracker);

public:
	void _mark_dirty() override;
	Error update_dependencies();
	bool has_unsupported_lighting() const { return _paired_features & (PAIRED_REFLECTION | PAIRED_DECAL | PAIRED_VOXEL_GI); }
	void set_use_lightmap(RID p_lightmap_instance, const Rect2 &p_lightmap_uv_scale, int p_lightmap_slice_index) override;
	void set_lightmap_capture(const Color *p_sh9) override;
	void clear_light_instances() override;
	void pair_light_instance(const RID p_light_instance, RSE::LightType p_type, uint32_t p_placement) override;
	void pair_reflection_probe_instances(const RID *p_instances, uint32_t p_count) override;
	void pair_decal_instances(const RID *p_instances, uint32_t p_count) override;
	void pair_voxel_gi_instances(const RID *p_instances, uint32_t p_count) override;
	void set_softshadow_projector_pairing(bool p_softshadow, bool p_projector) override;
	FixedFunctionGeometryInstance(FixedFunctionRenderContext &p_context, RID p_base);
	~FixedFunctionGeometryInstance() override;
};
