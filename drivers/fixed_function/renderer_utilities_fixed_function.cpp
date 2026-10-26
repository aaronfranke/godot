#include "renderer_utilities_fixed_function.h"

#include "core/os/os.h"

RendererUtilitiesFixedFunction::FFVisibilityNotifier::FFVisibilityNotifier() :
		initialized(false), dirty(false), free_pending(false) {}

RendererUtilitiesFixedFunction::FFVisibilityNotifier *RendererUtilitiesFixedFunction::_get_visibility_notifier(RID p_notifier) const {
	FFVisibilityNotifier *notifier = _visibility_notifier_owner.get_or_null(p_notifier);
	return notifier && notifier->initialized && !notifier->free_pending ? notifier : nullptr;
}

void RendererUtilitiesFixedFunction::_free_visibility_notifier(FFVisibilityNotifier *p_notifier) {
	_visibility_notifier_owner.free(p_notifier->self);
	_visibility_notifiers.erase(p_notifier);
	const bool notifying = _notifying_visibility;
	_notifying_visibility = true;
	p_notifier->dependency.deleted_notify(p_notifier->self);
	_notifying_visibility = notifying;
	memdelete(p_notifier);
}

void RendererUtilitiesFixedFunction::_retire_visibility_notifiers() {
	// Deletion callbacks may retire another notifier that was visited earlier.
	while (true) {
		Vector<RID> retired;
		for (const FFVisibilityNotifier *notifier : _visibility_notifiers) {
			if (notifier->free_pending) {
				retired.push_back(notifier->self);
			}
		}
		if (retired.is_empty()) {
			return;
		}
		for (RID rid : retired) {
			FFVisibilityNotifier *notifier = _visibility_notifier_owner.get_or_null(rid);
			if (notifier) {
				_free_visibility_notifier(notifier);
			}
		}
	}
}

Error RendererUtilitiesFixedFunction::_update_visibility_dependencies() {
	if (_notifying_visibility || _clearing_visibility) {
		return ERR_BUSY;
	}
	Vector<RID> changed;
	for (FFVisibilityNotifier *notifier : _visibility_notifiers) {
		if (notifier->dirty && !notifier->free_pending) {
			changed.push_back(notifier->self);
			notifier->dirty = false;
		}
	}
	_notifying_visibility = true;
	for (RID rid : changed) {
		FFVisibilityNotifier *notifier = _get_visibility_notifier(rid);
		if (notifier) {
			notifier->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
		}
	}
	_notifying_visibility = false;
	_retire_visibility_notifiers();
	return OK;
}

void RendererUtilitiesFixedFunction::_report_error(Error p_error, const char *p_operation) {
	_last_error = p_error;
	if (p_error != OK && p_error != ERR_SKIP) {
		ERR_PRINT(String("Fixed Function RendererUtilities ") + p_operation + " failed (Error " + itos(p_error) + ").");
	}
}

void RendererUtilitiesFixedFunction::_unsupported(const char *p_operation) {
	ERR_PRINT(String("Fixed Function RendererUtilities ") + p_operation + " is unavailable.");
}

RSE::InstanceType RendererUtilitiesFixedFunction::get_base_type(RID p_rid) const {
	if (_visibility_notifier_owner.owns(p_rid)) {
		return RSE::INSTANCE_VISIBLITY_NOTIFIER;
	}
	if (_context.get_light_storage().owns_light(p_rid)) {
		return RSE::INSTANCE_LIGHT;
	}
	if (_lights.owns_lightmap(p_rid)) {
		return RSE::INSTANCE_LIGHTMAP;
	}
	if (_context.get_mesh_storage().owns_multimesh(p_rid)) {
		return RSE::INSTANCE_MULTIMESH;
	}
	return _context.get_mesh_storage().owns_mesh(p_rid) ? RSE::INSTANCE_MESH : RSE::INSTANCE_NONE;
}

bool RendererUtilitiesFixedFunction::free(RID p_rid) {
	if (_visibility_notifier_owner.owns(p_rid)) {
		visibility_notifier_free(p_rid);
		return true;
	}
	FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	if (meshes.owns_multimesh(p_rid)) {
		meshes.multimesh_free(p_rid);
		return true;
	}
	if (meshes.owns_skeleton(p_rid)) {
		meshes.skeleton_free(p_rid);
		return true;
	}
	if (meshes.owns_mesh(p_rid) || meshes.owns_mesh_instance(p_rid)) {
		return meshes.free(p_rid) == OK;
	}
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	if (textures.owns_texture(p_rid)) {
		_texture_facade.texture_free(p_rid);
		return true;
	}
	if (textures.owns_render_target(p_rid)) {
		_texture_facade.render_target_free(p_rid);
		return true;
	}
	if (textures.owns_canvas_texture(p_rid)) {
		_texture_facade.canvas_texture_free(p_rid);
		return true;
	}
	FixedFunctionMaterialStorage &materials = _context.get_material_storage();
	if (materials.owns_material(p_rid)) {
		_material_facade.material_free(p_rid);
		return true;
	}
	if (materials.owns_shader(p_rid)) {
		_material_facade.shader_free(p_rid);
		return true;
	}
	return _lights.free(p_rid) || _scene.free(p_rid);
}

void RendererUtilitiesFixedFunction::base_update_dependency(RID p_base, DependencyTracker *p_instance) {
	FFVisibilityNotifier *notifier = _get_visibility_notifier(p_base);
	if (notifier) {
		if (p_instance) {
			p_instance->update_dependency(&notifier->dependency);
		}
		_report_error(p_instance ? OK : ERR_INVALID_PARAMETER, "base_update_dependency");
		return;
	}
	if (_context.get_light_storage().owns_light(p_base)) {
		_report_error(_context.get_light_storage().light_update_dependency(p_base, p_instance), "base_update_dependency");
		return;
	}
	if (Dependency *lightmap = _lights.lightmap_get_dependency(p_base)) {
		if (p_instance) {
			p_instance->update_dependency(lightmap);
		}
		_report_error(p_instance ? OK : ERR_INVALID_PARAMETER, "base_update_dependency");
		return;
	}
	if (_context.get_mesh_storage().owns_multimesh(p_base)) {
		_context.get_mesh_storage().multimesh_update_dependency(p_base, p_instance);
		return;
	}
	_report_error(_context.get_mesh_storage().mesh_update_dependency(p_base, p_instance), "base_update_dependency");
}

RID RendererUtilitiesFixedFunction::visibility_notifier_allocate() {
	if (_clearing_visibility) {
		_report_error(ERR_UNCONFIGURED, "visibility_notifier_allocate");
		return RID();
	}
	FFVisibilityNotifier *notifier = memnew(FFVisibilityNotifier);
	notifier->self = _visibility_notifier_owner.make_rid(notifier);
	_visibility_notifiers.push_back(notifier);
	_last_error = OK;
	return notifier->self;
}

void RendererUtilitiesFixedFunction::visibility_notifier_initialize(RID p_notifier) {
	FFVisibilityNotifier *notifier = _visibility_notifier_owner.get_or_null(p_notifier);
	if (!notifier || notifier->initialized || notifier->free_pending) {
		_report_error(ERR_INVALID_PARAMETER, "visibility_notifier_initialize");
		return;
	}
	notifier->initialized = true;
	_last_error = OK;
}

void RendererUtilitiesFixedFunction::visibility_notifier_free(RID p_notifier) {
	FFVisibilityNotifier *notifier = _visibility_notifier_owner.get_or_null(p_notifier);
	if (!notifier || notifier->free_pending) {
		_report_error(ERR_INVALID_PARAMETER, "visibility_notifier_free");
		return;
	}
	if (_notifying_visibility) {
		notifier->free_pending = true;
	} else {
		_free_visibility_notifier(notifier);
		_retire_visibility_notifiers();
	}
	_last_error = OK;
}

void RendererUtilitiesFixedFunction::visibility_notifier_set_aabb(RID p_notifier, const AABB &p_aabb) {
	FFVisibilityNotifier *notifier = _get_visibility_notifier(p_notifier);
	if (!notifier || !p_aabb.position.is_finite() || !p_aabb.size.is_finite()) {
		_report_error(ERR_INVALID_PARAMETER, "visibility_notifier_set_aabb");
		return;
	}
	notifier->aabb = p_aabb;
	// Match other FF storage: coalesce dependency changes at the idle update boundary.
	notifier->dirty = true;
	_last_error = OK;
}

void RendererUtilitiesFixedFunction::visibility_notifier_set_callbacks(RID p_notifier, const Callable &p_enter_callbable, const Callable &p_exit_callable) {
	FFVisibilityNotifier *notifier = _get_visibility_notifier(p_notifier);
	if (!notifier) {
		_report_error(ERR_INVALID_PARAMETER, "visibility_notifier_set_callbacks");
		return;
	}
	notifier->enter_callback = p_enter_callbable;
	notifier->exit_callback = p_exit_callable;
	_last_error = OK;
}

AABB RendererUtilitiesFixedFunction::visibility_notifier_get_aabb(RID p_notifier) const {
	const FFVisibilityNotifier *notifier = _get_visibility_notifier(p_notifier);
	ERR_FAIL_NULL_V(notifier, AABB());
	return notifier->aabb;
}

void RendererUtilitiesFixedFunction::visibility_notifier_call(RID p_notifier, bool p_enter, bool p_deferred) {
	const FFVisibilityNotifier *notifier = _get_visibility_notifier(p_notifier);
	if (!notifier) {
		_report_error(ERR_INVALID_PARAMETER, "visibility_notifier_call");
		return;
	}
	// A callback may replace itself or free its notifier. Keep its payload alive.
	const Callable callback = p_enter ? notifier->enter_callback : notifier->exit_callback;
	_last_error = OK;
	if (callback.is_valid()) {
		if (p_deferred) {
			callback.call_deferred();
		} else {
			callback.call();
		}
	}
}

void RendererUtilitiesFixedFunction::begin_frame_timestamps() {
	_timestamps.clear();
	// Keep frame boundaries even with profiling disabled, like GLES. A nonempty result
	// lets RenderingServer clear the previous profile when capture is switched off.
	capture_timestamp("Internal Begin");
}

void RendererUtilitiesFixedFunction::end_frame_timestamps() {
	capture_timestamp("Internal End");
}

void RendererUtilitiesFixedFunction::capture_timestamps_begin() {
	capture_timestamp("Frame Begin");
}

void RendererUtilitiesFixedFunction::capture_timestamp(const String &p_name) {
	FFTimestamp timestamp;
	timestamp.name = p_name;
	timestamp.cpu_usec = OS::get_singleton()->get_ticks_usec();
	_timestamps.push_back(timestamp);
}

uint32_t RendererUtilitiesFixedFunction::get_captured_timestamps_count() const {
	return _timestamps.size();
}

uint64_t RendererUtilitiesFixedFunction::get_captured_timestamps_frame() const {
	return _context.get_frame_number();
}

uint64_t RendererUtilitiesFixedFunction::get_captured_timestamp_gpu_time(uint32_t p_index) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_index, _timestamps.size(), 0);
	// Native GPU queries are not implemented. Estimate rendering time using the render-thread
	// clock, in the nanoseconds required by this interface; do not flush or stall the device.
	return (_timestamps[p_index].cpu_usec - _timestamps[0].cpu_usec) * 1000;
}

uint64_t RendererUtilitiesFixedFunction::get_captured_timestamp_cpu_time(uint32_t p_index) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_index, _timestamps.size(), 0);
	return _timestamps[p_index].cpu_usec;
}

String RendererUtilitiesFixedFunction::get_captured_timestamp_name(uint32_t p_index) const {
	ERR_FAIL_UNSIGNED_INDEX_V(p_index, _timestamps.size(), String());
	return _timestamps[p_index].name;
}

void RendererUtilitiesFixedFunction::update_dirty_resources() {
	const Error visibility = _update_visibility_dependencies();
	const Error dependencies = _context.get_material_storage().update_dependencies();
	const Error meshes = _context.get_mesh_storage().update_dependencies();
	const Error textures = _context.is_frame_active() ? OK : _context.get_texture_storage().update_pending();
	Error error = visibility == OK ? dependencies : visibility;
	if (textures != OK) {
		error = textures;
	} else if (meshes != OK) {
		error = meshes;
	}
	_report_error(error, "update_dirty_resources");
}

void RendererUtilitiesFixedFunction::set_debug_generate_wireframes(bool p_generate) {
	// The editor supplies this global generation hint; wireframe passes remain unavailable.
	_last_error = OK;
}

bool RendererUtilitiesFixedFunction::has_os_feature(const String &p_feature) const {
	return p_feature == "fixed_function" || ((p_feature == "s3tc" || p_feature == "bptc") && Image::can_decompress(p_feature));
}

void RendererUtilitiesFixedFunction::update_memory_info() {
	// Payload estimates are queried from the current CPU resource records.
}

uint64_t RendererUtilitiesFixedFunction::get_rendering_info(RSE::RenderingInfo p_info) {
	// Include native allocations still owned by deferred-release queues.
	if (p_info == RSE::RENDERING_INFO_TEXTURE_MEM_USED) {
		return _context.get_device().get_texture_memory_usage();
	}
	if (p_info == RSE::RENDERING_INFO_BUFFER_MEM_USED) {
		return _context.get_device().get_geometry_memory_usage();
	}
	if (p_info == RSE::RENDERING_INFO_VIDEO_MEM_USED) {
		const uint64_t textures = _context.get_device().get_texture_memory_usage();
		return textures + MIN(_context.get_device().get_geometry_memory_usage(), UINT64_MAX - textures);
	}
	return 0; // Frame counts belong to viewports.
}

String RendererUtilitiesFixedFunction::get_video_adapter_name() const {
	return _context.get_device().get_adapter_name();
}

String RendererUtilitiesFixedFunction::get_video_adapter_vendor() const {
	return _context.get_device().get_adapter_vendor();
}

#ifdef RD_ENABLED
RenderingDeviceEnums::DeviceType RendererUtilitiesFixedFunction::get_video_adapter_type() const {
	return RenderingDeviceEnums::DEVICE_TYPE_OTHER;
}
#endif

String RendererUtilitiesFixedFunction::get_video_adapter_api_version() const {
	return "Fixed Function";
}

Size2i RendererUtilitiesFixedFunction::get_maximum_viewport_size() const {
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = _context.get_device().get_capabilities();
	return Size2i(caps.max_texture_width, caps.max_texture_height);
}

uint32_t RendererUtilitiesFixedFunction::get_maximum_shader_varyings() const {
	// ShaderLanguage validates source against these limits. Shaders are only parsed here to
	// recognize their inputs, so a zero limit would reject every shader that declares a varying.
	return 31;
}

uint64_t RendererUtilitiesFixedFunction::get_maximum_uniform_buffer_size() const {
	return 65536;
}

RendererUtilitiesFixedFunction::RendererUtilitiesFixedFunction(FixedFunctionRenderContext &p_context, RendererTextureStorageFixedFunction &p_textures, RendererMaterialStorageFixedFunction &p_materials, RendererLightStorageFixedFunction &p_lights, RendererSceneRenderFixedFunction &p_scene) :
		_context(p_context), _texture_facade(p_textures), _material_facade(p_materials), _lights(p_lights), _scene(p_scene), _last_error(OK), _notifying_visibility(false), _clearing_visibility(false) {}

RendererUtilitiesFixedFunction::~RendererUtilitiesFixedFunction() {
	_clearing_visibility = true;
	while (!_visibility_notifiers.is_empty()) {
		_free_visibility_notifier(_visibility_notifiers.front()->get());
	}
}
