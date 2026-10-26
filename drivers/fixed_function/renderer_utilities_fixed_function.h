#pragma once

#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "drivers/fixed_function/render_context.h"
#include "drivers/fixed_function/renderer_light_storage_fixed_function.h"
#include "drivers/fixed_function/renderer_material_storage_fixed_function.h"
#include "drivers/fixed_function/renderer_scene_render_fixed_function.h"
#include "drivers/fixed_function/renderer_texture_storage_fixed_function.h"
#include "servers/rendering/storage/utilities.h"

// Branch-native resource contracts and CPU visibility notifications.
class RendererUtilitiesFixedFunction : public RendererUtilities {
	struct FFVisibilityNotifier {
		AABB aabb;
		Dependency dependency;
		Callable enter_callback;
		Callable exit_callback;
		RID self;
		bool initialized;
		bool dirty;
		bool free_pending;

		FFVisibilityNotifier();
	};
	struct FFTimestamp {
		uint64_t cpu_usec;
		String name;
	};
	// RID lookup lacks a const API; preserve read-only queries until one exists.
	mutable RID_PtrOwner<FFVisibilityNotifier> _visibility_notifier_owner;
	List<FFVisibilityNotifier *> _visibility_notifiers;
	LocalVector<FFTimestamp> _timestamps;
	FixedFunctionRenderContext &_context;
	RendererTextureStorageFixedFunction &_texture_facade;
	RendererMaterialStorageFixedFunction &_material_facade;
	RendererLightStorageFixedFunction &_lights;
	RendererSceneRenderFixedFunction &_scene;
	Error _last_error;
	bool _notifying_visibility;
	bool _clearing_visibility;
	FFVisibilityNotifier *_get_visibility_notifier(RID p_notifier) const;
	void _free_visibility_notifier(FFVisibilityNotifier *p_notifier);
	void _retire_visibility_notifiers();
	Error _update_visibility_dependencies();
	void _report_error(Error p_error, const char *p_operation);
	static void _unsupported(const char *p_operation);

public:
	Error get_last_error() const { return _last_error; }
	RendererUtilitiesFixedFunction(FixedFunctionRenderContext &p_context, RendererTextureStorageFixedFunction &p_textures, RendererMaterialStorageFixedFunction &p_materials, RendererLightStorageFixedFunction &p_lights, RendererSceneRenderFixedFunction &p_scene);
	~RendererUtilitiesFixedFunction() override;
	RSE::InstanceType get_base_type(RID p_rid) const override;
	bool free(RID p_rid) override;
	void base_update_dependency(RID p_base, DependencyTracker *p_instance) override;
	RID visibility_notifier_allocate() override;
	void visibility_notifier_initialize(RID p_notifier) override;
	void visibility_notifier_free(RID p_notifier) override;
	void visibility_notifier_set_aabb(RID p_notifier, const AABB &p_aabb) override;
	void visibility_notifier_set_callbacks(RID p_notifier, const Callable &p_enter_callbable, const Callable &p_exit_callable) override;
	AABB visibility_notifier_get_aabb(RID p_notifier) const override;
	void visibility_notifier_call(RID p_notifier, bool p_enter, bool p_deferred) override;
	void begin_frame_timestamps();
	void end_frame_timestamps();
	void capture_timestamps_begin() override;
	void capture_timestamp(const String &p_name) override;
	uint32_t get_captured_timestamps_count() const override;
	uint64_t get_captured_timestamps_frame() const override;
	uint64_t get_captured_timestamp_gpu_time(uint32_t p_index) const override;
	uint64_t get_captured_timestamp_cpu_time(uint32_t p_index) const override;
	String get_captured_timestamp_name(uint32_t p_index) const override;
	void update_dirty_resources() override;
	void set_debug_generate_wireframes(bool p_generate) override;
	bool has_os_feature(const String &p_feature) const override;
	void update_memory_info() override;
	uint64_t get_rendering_info(RSE::RenderingInfo p_info) override;
	String get_video_adapter_name() const override;
	String get_video_adapter_vendor() const override;
#ifdef RD_ENABLED
	RenderingDeviceEnums::DeviceType get_video_adapter_type() const override;
#endif
	String get_video_adapter_api_version() const override;
	Size2i get_maximum_viewport_size() const override;
	uint32_t get_maximum_shader_varyings() const override;
	uint64_t get_maximum_uniform_buffer_size() const override;
};
