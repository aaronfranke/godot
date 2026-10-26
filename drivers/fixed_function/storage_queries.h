#ifndef FIXED_FUNCTION_STORAGE_QUERIES_H
#define FIXED_FUNCTION_STORAGE_QUERIES_H

#include "light_storage.h"
#include "material_storage.h"
#include "mesh_storage.h"
#include "texture_storage.h"

// Branch-native storage queries and counters, independent of the singleton facade.
class FixedFunctionStorageQueries {
public:
	enum FFResourceType {
		RESOURCE_NONE,
		RESOURCE_TEXTURE,
		RESOURCE_RENDER_TARGET,
		RESOURCE_SHADER,
		RESOURCE_MATERIAL,
		RESOURCE_MESH,
		RESOURCE_LIGHT,
		RESOURCE_IMMEDIATE,
	};

private:
	enum { FRAME_COUNTER_COUNT = VS::INFO_USAGE_VIDEO_MEM_TOTAL };
	uint64_t _current[FRAME_COUNTER_COUNT];
	uint64_t _previous[FRAME_COUNTER_COUNT];
	uint64_t _capture[FRAME_COUNTER_COUNT];
	FixedFunctionGraphicsDevice &_device;
	FixedFunctionTextureStorage &_textures;
	FixedFunctionMaterialStorage &_materials;
	FixedFunctionMeshStorage &_meshes;
	FixedFunctionLightStorage &_lights;
	bool _capturing;
	bool _keep_original_requested;
	static bool _is_frame_counter(VS::RenderInfo p_info);
	FixedFunctionStorageQueries(const FixedFunctionStorageQueries &);
	FixedFunctionStorageQueries &operator=(const FixedFunctionStorageQueries &);

public:
	FFResourceType get_resource_type(RID p_rid) const;
	VS::InstanceType get_base_type(RID p_rid) const;
	bool has_os_feature(const String &p_feature) const;
	bool supports_directional_shadows() const { return _device.supports_directional_shadows(); }
	Error set_debug_generate_wireframes(bool p_generate);
	Error texture_set_shrink_all_x2_on_set_data(bool p_enable);
	void textures_keep_original(bool p_enable);
	bool get_keep_original_requested() const { return _keep_original_requested; }
	Error texture_get_texid(RID p_texture, uint32_t &r_id) const;
	Error texture_bind(RID p_texture, uint32_t p_unit) const;
	Error texture_create_radiance_cubemap(RID p_source, int p_resolution) const;
	Error texture_debug_usage(List<VS::TextureInfo> *r_info) const;
	void begin_frame();
	void note_canvas_item();
	void note_canvas_draw();
	void note_scene_draw(uint32_t p_vertices, bool p_object, bool p_surface, uint32_t p_draw_calls = 1);
	Error render_info_begin_capture();
	Error render_info_end_capture();
	// Frame counters report the preceding logical frame, as in the 3.x GLES path.
	// Captures report this frame's interval. Memory queries are always current estimates.
	Error get_captured_render_info(VS::RenderInfo p_info, int &r_value) const;
	Error get_render_info(VS::RenderInfo p_info, uint64_t &r_value) const;
	String get_video_adapter_name() const { return _device.get_adapter_name(); }
	String get_video_adapter_vendor() const { return _device.get_adapter_vendor(); }
	void clear();
	FixedFunctionStorageQueries(FixedFunctionGraphicsDevice &p_device, FixedFunctionTextureStorage &p_textures, FixedFunctionMaterialStorage &p_materials, FixedFunctionMeshStorage &p_meshes, FixedFunctionLightStorage &p_lights);
};

#endif // FIXED_FUNCTION_STORAGE_QUERIES_H
