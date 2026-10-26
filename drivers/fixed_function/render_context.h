#ifndef FIXED_FUNCTION_RENDER_CONTEXT_H
#define FIXED_FUNCTION_RENDER_CONTEXT_H

#include "geometry_cache.h"
#include "graphics_device.h"
#include "light_storage.h"
#include "material_storage.h"
#include "mesh_storage.h"
#include "scene_storage.h"
#include "storage_queries.h"
#include "texture_storage.h"

#include "core/os/thread.h"

// Exclusive use of a borrowed device. The platform's device/window outlive this context.
// All calls, including destruction, stay on the window/device thread.
class FixedFunctionRenderContext {
	FixedFunctionGraphicsDevice &_device;
	FixedFunctionSceneStorage _scene_storage;
	FixedFunctionTextureStorage _texture_storage;
	FixedFunctionMaterialStorage _material_storage;
	FixedFunctionMeshStorage _mesh_storage;
	FixedFunctionGeometryCache _geometry_cache;
	FixedFunctionLightStorage _light_storage;
	FixedFunctionStorageQueries _storage_queries;
	RID _frame_target;
	void *_native_window;
	Thread::ID _owner_thread;
	uint64_t _frame_number;
	double _time_total;
	double _frame_delta;
	uint32_t _requested_width;
	uint32_t _requested_height;
	float _time_scale;
	Error _last_error;
	Error _frame_error;
	bool _use_vsync;
	bool _initialized;
	bool _frame_open;
	bool _frame_active;
	bool _frame_depth;

	Error _check_ready() const;
	Error _prepare_resources(bool p_release_targets);
	Error _open_frame();
	Error _begin_device_frame(uint32_t p_clear_color, RID p_render_target, bool p_clear, bool p_use_depth);
	Error _draw_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_present, bool p_use_filter);

	FixedFunctionRenderContext(const FixedFunctionRenderContext &);
	FixedFunctionRenderContext &operator=(const FixedFunctionRenderContext &);

public:
	Error initialize();
	Error finalize();
	Error resize(uint32_t p_width, uint32_t p_height);
	Error set_use_vsync(bool p_enabled);
	Error set_shader_time_scale(float p_scale);
	// No frame-clock or draw changes. Resource producers call this before idle uploads.
	Error prepare_resources();
	// Prepare before resource updates, recording recovery errors for the deferred native begin.
	// A valid logical frame still advances its clock while the device is suspended.
	Error begin_frame(double p_frame_step);
	// Defer native begin until the clear/load operation is known. Repeated begins on an
	// already active window pass do not clear it again.
	Error begin_device_frame(uint32_t p_clear_color, bool p_clear = true, bool p_use_depth = false);
	// Offscreen passes use logical storage target RIDs and stay within one engine frame.
	Error begin_render_target(RID p_render_target, uint32_t p_clear_color, bool p_clear = true, bool p_use_depth = false);
	Error end_render_target();
	// Close a native pass while keeping the logical engine frame open for another target.
	// Discard keeps incomplete target contents unavailable until a successful redraw.
	Error end_device_frame(bool p_discard = false);
	// Close without opening another pass or presenting incomplete output.
	Error cancel_frame();
	Error get_frame_status() const;
	Error end_frame(bool p_present);
	Error draw_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_present = true, bool p_use_filter = false);

	Error get_last_error() const { return _last_error; }
	FixedFunctionTextureStorage &get_texture_storage() { return _texture_storage; }
	FixedFunctionMaterialStorage &get_material_storage() { return _material_storage; }
	FixedFunctionMeshStorage &get_mesh_storage() { return _mesh_storage; }
	FixedFunctionGeometryCache &get_geometry_cache() { return _geometry_cache; }
	FixedFunctionLightStorage &get_light_storage() { return _light_storage; }
	FixedFunctionSceneStorage &get_scene_storage() { return _scene_storage; }
	FixedFunctionStorageQueries &get_storage_queries() { return _storage_queries; }
	FixedFunctionGraphicsDevice &get_device() { return _device; }
	const FixedFunctionGraphicsDevice &get_device() const { return _device; }
	uint64_t get_frame_number() const { return _frame_number; }
	double get_total_time() const { return _time_total; }
	double get_frame_delta_time() const { return _frame_delta; }
	bool is_initialized() const { return _initialized; }
	bool is_frame_active() const { return _frame_active; }
	bool is_frame_depth_enabled() const { return _frame_depth; }

	FixedFunctionRenderContext(FixedFunctionGraphicsDevice &p_device, void *p_native_window, uint32_t p_width, uint32_t p_height, bool p_vsync);
	~FixedFunctionRenderContext();
};

#endif // FIXED_FUNCTION_RENDER_CONTEXT_H
