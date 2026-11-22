#include "storage_queries.h"

FixedFunctionStorageQueries::FFResourceType FixedFunctionStorageQueries::get_resource_type(RID p_rid) const {
	if (_textures.owns_texture(p_rid)) {
		return RESOURCE_TEXTURE;
	}
	if (_textures.owns_render_target(p_rid)) {
		return RESOURCE_RENDER_TARGET;
	}
	if (_materials.owns_shader(p_rid)) {
		return RESOURCE_SHADER;
	}
	if (_materials.owns_material(p_rid)) {
		return RESOURCE_MATERIAL;
	}
	if (_lights.owns_light(p_rid)) {
		return RESOURCE_LIGHT;
	}
	if (_meshes.owns_immediate(p_rid)) {
		return RESOURCE_IMMEDIATE;
	}
	return _meshes.owns_mesh(p_rid) ? RESOURCE_MESH : RESOURCE_NONE;
}

VS::InstanceType FixedFunctionStorageQueries::get_base_type(RID p_rid) const {
	if (_lights.owns_light(p_rid)) {
		return VS::INSTANCE_LIGHT;
	}
	if (_meshes.owns_multimesh(p_rid)) {
		return VS::INSTANCE_MULTIMESH;
	}
	if (_meshes.owns_immediate(p_rid)) {
		return VS::INSTANCE_IMMEDIATE;
	}
	return _meshes.owns_mesh(p_rid) ? VS::INSTANCE_MESH : VS::INSTANCE_NONE;
}

bool FixedFunctionStorageQueries::has_os_feature(const String &p_feature) const {
	// The importer uses these tags to select data the renderer can decode.
	if (p_feature == "s3tc") {
		return Image::_image_decompress_bc != nullptr;
	}
	if (p_feature == "bptc") {
		return Image::_image_decompress_bptc != nullptr;
	}
	return false;
}

Error FixedFunctionStorageQueries::set_debug_generate_wireframes(bool p_generate) {
	return p_generate ? ERR_UNAVAILABLE : OK;
}

Error FixedFunctionStorageQueries::texture_set_shrink_all_x2_on_set_data(bool p_enable) {
	return p_enable ? ERR_UNAVAILABLE : OK;
}

void FixedFunctionStorageQueries::textures_keep_original(bool p_enable) {
	_keep_original_requested = p_enable;
	// Both settings retain the CPU snapshot required by readback, deferred writes and recovery.
}

Error FixedFunctionStorageQueries::texture_get_texid(RID p_texture, uint32_t &r_id) const {
	return _textures.texture_can_be_referenced(p_texture) ? ERR_UNAVAILABLE : ERR_INVALID_PARAMETER;
}

Error FixedFunctionStorageQueries::texture_bind(RID p_texture, uint32_t p_unit) const {
	return _textures.texture_can_be_referenced(p_texture) ? ERR_UNAVAILABLE : ERR_INVALID_PARAMETER;
}

Error FixedFunctionStorageQueries::texture_create_radiance_cubemap(RID p_source, int p_resolution) const {
	if (!_textures.texture_can_be_referenced(p_source) || (p_resolution != -1 && p_resolution <= 0)) {
		return ERR_INVALID_PARAMETER;
	}
	return ERR_UNAVAILABLE;
}

Error FixedFunctionStorageQueries::texture_debug_usage(List<VS::TextureInfo> *r_info) const {
	if (!r_info) {
		return ERR_INVALID_PARAMETER;
	}
	List<FixedFunctionTextureStorage::FFTextureUsage> usage;
	const Error error = _textures.texture_debug_usage(&usage);
	if (error != OK) {
		return error;
	}
	for (const List<FixedFunctionTextureStorage::FFTextureUsage>::Element *E = usage.front(); E; E = E->next()) {
		const FixedFunctionTextureStorage::FFTextureUsage &source = E->get();
		VS::TextureInfo info;
		info.texture = source.texture;
		info.path = source.path;
		info.width = source.size.x;
		info.height = source.size.y;
		info.depth = 0;
		info.format = source.format;
		info.bytes = int(MIN(source.native_bytes, uint64_t(INT32_MAX)));
		r_info->push_back(info);
	}
	return OK;
}

bool FixedFunctionStorageQueries::_is_frame_counter(VS::RenderInfo p_info) {
	const int index = p_info;
	return index >= 0 && index < FRAME_COUNTER_COUNT;
}

void FixedFunctionStorageQueries::begin_frame() {
	for (int i = 0; i < FRAME_COUNTER_COUNT; i++) {
		_previous[i] = _current[i];
		_current[i] = _capture[i] = 0;
	}
	_capturing = false;
}

void FixedFunctionStorageQueries::note_canvas_item() {
	if (_current[VS::INFO_2D_ITEMS_IN_FRAME] != UINT64_MAX) {
		_current[VS::INFO_2D_ITEMS_IN_FRAME]++;
	}
}

void FixedFunctionStorageQueries::note_canvas_draw() {
	if (_current[VS::INFO_2D_DRAW_CALLS_IN_FRAME] != UINT64_MAX) {
		_current[VS::INFO_2D_DRAW_CALLS_IN_FRAME]++;
	}
}

void FixedFunctionStorageQueries::note_scene_draw(uint32_t p_vertices, bool p_object, bool p_surface, uint32_t p_draw_calls) {
	const VS::RenderInfo counters[] = { VS::INFO_OBJECTS_IN_FRAME, VS::INFO_VERTICES_IN_FRAME, VS::INFO_SURFACE_CHANGES_IN_FRAME, VS::INFO_MATERIAL_CHANGES_IN_FRAME, VS::INFO_DRAW_CALLS_IN_FRAME };
	const uint64_t amounts[] = { p_object ? 1U : 0U, p_vertices, p_surface ? 1U : 0U, p_surface ? 1U : 0U, p_draw_calls };
	for (int i = 0; i < 5; i++) {
		uint64_t &value = _current[counters[i]];
		value += MIN(amounts[i], UINT64_MAX - value);
	}
}

Error FixedFunctionStorageQueries::render_info_begin_capture() {
	if (_capturing) {
		return ERR_BUSY;
	}
	for (int i = 0; i < FRAME_COUNTER_COUNT; i++) {
		_capture[i] = _current[i];
	}
	_capturing = true;
	return OK;
}

Error FixedFunctionStorageQueries::render_info_end_capture() {
	if (!_capturing) {
		return ERR_UNCONFIGURED;
	}
	for (int i = 0; i < FRAME_COUNTER_COUNT; i++) {
		_capture[i] = _current[i] - _capture[i];
	}
	_capturing = false;
	return OK;
}

Error FixedFunctionStorageQueries::get_captured_render_info(VS::RenderInfo p_info, int &r_value) const {
	if (_is_frame_counter(p_info)) {
		if (_capturing) {
			return ERR_BUSY;
		}
		r_value = int(MIN(_capture[p_info], uint64_t(INT32_MAX)));
		return OK;
	}
	uint64_t value = 0;
	const Error error = get_render_info(p_info, value);
	if (error == OK) {
		r_value = int(MIN(value, uint64_t(INT32_MAX)));
	}
	return error;
}

Error FixedFunctionStorageQueries::get_render_info(VS::RenderInfo p_info, uint64_t &r_value) const {
	if (_is_frame_counter(p_info)) {
		r_value = _previous[p_info];
		return OK;
	}
	switch (p_info) {
		case VS::INFO_VIDEO_MEM_USED:
			r_value = _device.get_texture_memory_usage();
			r_value += MIN(_device.get_geometry_memory_usage(), UINT64_MAX - r_value);
			break;
		case VS::INFO_TEXTURE_MEM_USED:
			r_value = _device.get_texture_memory_usage();
			break;
		case VS::INFO_VERTEX_MEM_USED: // Native geometry payload; transient upload streams are excluded.
			r_value = _device.get_geometry_memory_usage();
			break;
		case VS::INFO_USAGE_VIDEO_MEM_TOTAL: // Available physical VRAM is unknown.
			r_value = 0;
			break;
		default:
			return ERR_INVALID_PARAMETER;
	}
	return OK;
}

void FixedFunctionStorageQueries::clear() {
	for (int i = 0; i < FRAME_COUNTER_COUNT; i++) {
		_current[i] = _previous[i] = _capture[i] = 0;
	}
	_capturing = _keep_original_requested = false;
}

FixedFunctionStorageQueries::FixedFunctionStorageQueries(FixedFunctionGraphicsDevice &p_device, FixedFunctionTextureStorage &p_textures, FixedFunctionMaterialStorage &p_materials, FixedFunctionMeshStorage &p_meshes, FixedFunctionLightStorage &p_lights) :
		_device(p_device), _textures(p_textures), _materials(p_materials), _meshes(p_meshes), _lights(p_lights) {
	clear();
}
