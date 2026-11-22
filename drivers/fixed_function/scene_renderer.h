#ifndef FIXED_FUNCTION_SCENE_RENDERER_H
#define FIXED_FUNCTION_SCENE_RENDERER_H

#include "render_target_manager.h"

class FixedFunctionSceneRenderer {
	friend class TestFixedFunctionSpatialLines;
	friend class TestFixedFunctionLambert;
	friend class TestFixedFunctionSceneCPU;
	friend class TestFixedFunctionRetainedScene;

	// Limit optional synchronous MikkTSpace work and recursive face grouping.
	enum { MAX_GENERATED_TANGENT_TRIANGLES = 16384 };
	enum {
		MAX_BATCH_PRIMITIVES = 4096,
		BATCH_INDEX_TABLE_SIZE = 32768,
		MAX_LIT_VERTEX_CACHE_ENTRIES = 2048,
		MAX_INDEXED_LIT_VERTICES = 32768,
		MAX_LINE_VERTICES = 30,
		SKY_COLUMNS = 32,
		SKY_ROWS = 16
	};
	struct FFSpatialVertex {
		Vector3 position;
		Vector2 uv;
		Color color;
	};
	struct FFLitVertexCacheEntry {
		FixedFunctionVertex vertex;
		uint32_t index;
	};
	struct FFDeformedVertex {
		Vector3 position;
		Vector3 normal;
	};
	struct FFDeformedTangentFrame {
		Vector3 tangent;
		Vector3 bitangent;
	};
	struct FFMorphAttributes {
		Vector2 uv[2];
		Color color;
	};
	struct FFDrawPacket {
		FixedFunctionMeshStorage::FFSurfaceData surface;
		FixedFunctionSpatialMaterial::FFMaterialParameters material;
		Transform view_from_model;
		Basis normal_from_model;
		Vector<FFDeformedVertex> deformed_vertices;
		Vector<FFDeformedTangentFrame> deformed_tangents;
		Vector<FFMorphAttributes> morph_attributes;
		Vector<int> local_light_indices;
		AABB multimesh_light_bounds; // Camera-relative world space.
		uint64_t multimesh = 0;
		uint64_t baked_occlusion_image_version = 0;
		float line_width;
		float depth;
		int instance_index;
		int submission_index;
		uint32_t layer_mask;
		int8_t priority;
		uint8_t baked_occlusion_sampler_flags = 0;
		bool flip_v;
		bool antialiased;
		bool late;
		bool retained_geometry;
		bool normal_mapping;
		bool occlusion;
		bool occlusion_secondary_uv;
		bool optional_material_pass;
		bool local_lights_filtered = false;
		bool multimesh_local_lighting = false;
	};
	struct FFOcclusionSample {
		Size2i size;
		const uint8_t *rgba;
		uint8_t sampler_flags;
		int8_t single_channel = -1;
	};
	struct FFDirectionalLight {
		// Finite float color*energy can exceed float range before signed-light cancellation.
		double radiance[3];
		Vector3 direction;
		uint32_t cull_mask;
		uint32_t instance_id;
	};
	struct FFLocalLight {
		double radiance[3];
		Vector3 position;
		Vector3 direction;
		real_t range;
		real_t cos_spot_angle;
		float attenuation;
		float spot_attenuation;
		uint32_t cull_mask;
		uint32_t instance_id;
		bool spot;
		bool direct_distance = false;
	};
	struct FFLocalLightSphere {
		Vector3 position;
		real_t range;
		uint32_t cull_mask;
	};
	struct FFMultimeshLights {
		const RID *sources;
		Vector<FFLocalLightSphere> spheres;
		int source_count;
		Error status = ERR_SKIP;
	};
	struct FFPixelLightCandidate {
		double score;
		uint32_t instance_id;
		int index;
		bool local;
	};
	struct FFDrawPacketSort {
		bool operator()(const FFDrawPacket &p_left, const FFDrawPacket &p_right) const;
	};
	struct FFDrawPacketOrder {
		const FFDrawPacket *packets = nullptr;
		bool operator()(uint32_t p_left, uint32_t p_right) const { return FFDrawPacketSort()(packets[p_left], packets[p_right]); }
	};
	struct FFSceneOptions {
		const Transform &camera;
		FFMultimeshLights &multimesh_lights;
		bool antialiased;
		bool emission;
		bool occlusion;
	};
	struct FFShadowTransform {
		CameraMatrix projection;
		Transform transform;
		RID light;
		float bias_scale;
		uint32_t instance_id;
		FFShadowTransform() :
				bias_scale(1), instance_id(0) {}
	};
	FixedFunctionRenderContext &_context;
	FixedFunctionRenderTargetManager &_render_targets;
	FFShadowTransform _pending_shadow;
	FFShadowTransform _shadow_transform;
	FixedFunctionGraphicsDevice::FFDirectionalShadowParameters _shadow_parameters;
	RID _shadow_target;
	uint64_t _shadow_frame;
	double _ambient[3];
	Vector<FFDirectionalLight> _directional_lights;
	Vector<FFLocalLight> _local_lights;
	Vector<FFDrawPacket> _packets;
	Vector<uint32_t> _packet_order;
	FixedFunctionGraphicsDevice::FFFog _fog;
	Vector<FixedFunctionVertex> _vertices;
	Vector<FFLitVertexCacheEntry> _lit_vertex_cache;
	Vector<FixedFunctionVertex> _indexed_lit_vertices;
	Vector<uint32_t> _batch_index_table;
	Vector<uint32_t> _batch_source_indices;
	Vector<uint16_t> _batch_draw_indices;
	uint16_t _batch_index_generation = 0;
	Vector<FixedFunctionMaskedVertex> _emission_vertices;
	Vector<FixedFunctionVertex> _sky_vertices;
	Vector<FixedFunctionLightingVertex> _lighting_vertices;
	Vector<FixedFunctionTransientOcclusionVertex> _occlusion_vertices;
	Vector<FixedFunctionTransientNormalMapVertex> _normal_vertices;
	Vector<uint8_t> _instances_drawn;
	Basis _camera_basis;
	uint32_t _sky_color;
	bool _redraw_requested;
	bool _sort_packets;
	bool _lighting_required;
	bool _shadow_pass;
	bool _shadow_reverse_cull;
	bool _shadow_ready;
	bool _debug_unshaded;
	// 0: CPU lighting only. 1: native lighting where it is exact. 2: also approximate local lights.
	int _native_lighting_mode = 0;
	bool _per_pixel_lighting = true;
	bool _normal_mapping_enabled = true;
	bool _retained_geometry_enabled = true;

	static bool _finite_transform(const Transform &p_transform);
	static Error _read_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, FFSpatialVertex &r_vertex, Vector3 *r_model_position = nullptr);
	static void _read_vertex_coordinates(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, Vector3 &r_position, Vector2 &r_uv, Vector3 *r_model_position = nullptr);
	static Color _apply_vertex_alpha_state(const FFDrawPacket &p_packet, Color p_color);
	static Vector2 _vertex_uv(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, int p_uv_set);
	static Vector3 _unit_normal(const Vector3 &p_normal);
	static Basis _normal_matrix(const Basis &p_basis);
	static real_t _uniform_basis_scale_squared(const Basis &p_basis);
	static Vector3 _read_normal(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, bool p_normalize = true);
	static Vector3 _vertex_normal(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index);
	static Error _prepare_deformation(FFDrawPacket &r_packet, const Vector<Transform> &p_bones, const PoolVector<float> &p_weights, VS::BlendShapeMode p_mode, bool p_tangents = false);
	static void _read_skin_weights(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, real_t *r_weights);
	static void _skin_vertex(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, const Vector<Transform> &p_bones, const real_t *p_weights, FFDeformedVertex &r_vertex, FFDeformedTangentFrame *r_tangent = nullptr);
	Error _prepare_lighting(const Transform &p_camera, RID p_environment, const RID *p_lights, int p_light_count);
	static real_t _local_light_diffuse(const FFLocalLight &p_light, const Vector3 &p_position, const Vector3 &p_normal);
	void _filter_local_lights(FFDrawPacket &r_packet) const;
	bool _has_direct_lights(const FFDrawPacket &p_packet) const;
	void _shade_vertex(const FFDrawPacket &p_packet, const Vector3 &p_normal, FFSpatialVertex &r_vertex, float p_occlusion = 1) const;
	Error _pack_ambient_indexed_vertices(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_sources, uint32_t p_count, FixedFunctionVertex *r_vertices, const FFOcclusionSample *p_occlusion) const;
	Error _shade_and_pack_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, const Vector3 &p_normal, FFSpatialVertex &r_vertex, FixedFunctionVertex &r_packed, const FFOcclusionSample *p_occlusion) const;
	Error _lit_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, FixedFunctionVertex *r_vertices, const FFOcclusionSample *p_occlusion = nullptr) const;
	Error _cached_lit_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, FixedFunctionVertex *r_vertices, const FFOcclusionSample *p_occlusion, FFLitVertexCacheEntry *p_cache, uint32_t p_cache_mask) const;
	static int8_t _get_occlusion_sample_channel(const Color &p_weights);
	static float _sample_occlusion(const FFOcclusionSample &p_sample, const FixedFunctionSpatialMaterial::FFOcclusionChannel &p_channel, Vector2 p_uv);
	bool _get_occlusion_texture(RID p_texture, RID &r_native_texture, bool p_preflight = false);
	static Error _read_lighting_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, FFSpatialVertex *r_vertices, Vector3 *r_normals, Vector3 *r_model_normals = nullptr, const Basis *p_normal_from_model = nullptr);
	bool _prepare_pixel_lighting(const FFDrawPacket &p_packet, FixedFunctionGraphicsDevice::FFPixelLighting &r_lighting) const;
	// Strongest lights in camera-relative world axes. r_candidates counts every light that can reach the bounds.
	bool _select_lights(const FFDrawPacket &p_packet, uint32_t p_limit, FixedFunctionGraphicsDevice::FFPixelLighting &r_lighting, uint32_t *r_candidates = nullptr) const;
	bool _uses_vertex_lit_geometry(const FFDrawPacket &p_packet) const;
	// Lights in the retained geometry's anchor-relative model space, for native transform and lighting.
	bool _prepare_vertex_lighting(const FFDrawPacket &p_packet, const Vector3 &p_anchor, FixedFunctionGraphicsDevice::FFPixelLighting &r_lighting) const;
	bool _prepare_streamed_vertex_lighting(const FFDrawPacket &p_packet, bool p_occlusion, FixedFunctionGraphicsDevice::FFPixelLighting &r_lighting, Basis &r_normal_from_model) const;
	bool _prepare_native_lights(const FFDrawPacket &p_packet, uint32_t p_limit, const Basis &p_world_from_input, const Vector3 &p_origin, bool p_occlusion, FixedFunctionGraphicsDevice::FFPixelLighting &r_lighting) const;
	static double _local_light_score(const FFLocalLight &p_light, const AABB &p_bounds);
	static void _select_pixel_light(FFPixelLightCandidate *p_selected, uint32_t &r_count, uint32_t p_limit, const FFPixelLightCandidate &p_candidate);
	static Error _lighting_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, FixedFunctionLightingVertex *r_vertices, Vector3 *r_model_normals = nullptr, const Basis *p_normal_from_model = nullptr);
	static Error _pack_lighting_vertex(const FFSpatialVertex &p_vertex, const Vector3 &p_normal, FixedFunctionLightingVertex &r_vertex);
	static Error _lighting_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, FixedFunctionLightingVertex &r_vertex, Vector3 *r_model_normal = nullptr, const Basis *p_normal_from_model = nullptr);
	static Error _pack_vertex(const FFSpatialVertex &p_vertex, FixedFunctionVertex &r_vertex);
	static Error _pack_vertex_coordinates(const Vector3 &p_position, const Vector2 &p_uv, FixedFunctionVertex &r_vertex);
	static Error _vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, FixedFunctionVertex &r_vertex);
	static Error _line_quad(const FFSpatialVertex *p_endpoints, const CameraMatrix &p_projection, const Size2i &p_size, float p_width, FixedFunctionVertex *r_vertices, int &r_vertex_count, bool p_antialiased = false);
	static Error _offset_projected_vertex(const Vector3 &p_position, const Plane &p_clip, const CameraMatrix &p_projection, const Vector2 &p_offset, Vector3 &r_position);
	Error _submit_batch(const FFDrawPacket &p_packet, const CameraMatrix &p_projection, RID p_texture, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, uint32_t p_count, bool &r_first_surface, uint32_t p_indexed_vertices = 0);
	Error _submit_lines(const FFDrawPacket &p_packet, const CameraMatrix &p_projection, RID p_texture);
	Error _submit_points(const FFDrawPacket &p_packet, const CameraMatrix &p_projection, RID p_texture);
	Error _prepare(const Transform &p_camera, const CameraMatrix &p_projection, RasterizerScene::InstanceBase **p_instances, int p_count, const RID *p_lights = nullptr, int p_light_count = 0);
	Error _prepare_mesh(const Transform &p_view, const CameraMatrix &p_projection, const RasterizerScene::InstanceBase &p_instance, RID p_mesh, int p_index, const FFSceneOptions &p_options, const FixedFunctionMeshStorage::FFMultiMeshInstance *p_copy = nullptr, uint64_t p_multimesh = 0);
	bool _prepare_normal_map(FFDrawPacket &p_packet);
	static Error _pack_normal_geometry_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, const Vector3 &p_normal, FixedFunctionNormalMapVertex &r_vertex);
	static Error _generate_geometry_tangents(PoolVector<FixedFunctionNormalMapVertex> &r_vertices, PoolVector<uint32_t> &r_indices);
	static bool _read_tangent(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, Vector3 &r_tangent, float &r_sign);
	static void _read_tangent_data(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, Vector3 &r_tangent, float &r_sign);
	Error _occlusion_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, FixedFunctionTransientOcclusionVertex *r_vertices);
	static Error _pack_occlusion_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, const FixedFunctionLightingVertex &p_lighting, FixedFunctionTransientOcclusionVertex &r_vertex);
	bool _prepare_transient_occlusion(const FFDrawPacket &p_packet, FixedFunctionGraphicsDevice::FFOcclusionMapParameters &r_occlusion);
	Error _normal_triangle(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint32_t *p_indices, const Basis &p_world_from_model, FixedFunctionTransientNormalMapVertex *r_vertices, bool p_flip_v, bool p_occlusion);
	static Error _pack_normal_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, const Basis &p_world_from_model, const Vector3 &p_model_normal, const FixedFunctionLightingVertex &p_lighting, FixedFunctionTransientNormalMapVertex &r_vertex, bool p_flip_v, bool p_occlusion);
	bool _prepare_transient_normal_map(const FFDrawPacket &p_packet, FixedFunctionGraphicsDevice::FFNormalMapParameters &r_normal, FixedFunctionGraphicsDevice::FFOcclusionMapParameters &r_occlusion, bool &r_use_occlusion);
	bool _request_geometry(const FFDrawPacket &p_packet);
	static bool _geometry_upload_fits(uint64_t p_count, size_t p_stride);
	static Error _generate_geometry_normals(const FFDrawPacket &p_packet, PoolVector<FixedFunctionLightingVertex> &r_vertices, PoolVector<uint32_t> &r_indices);
	static bool _geometry_projection(const FFDrawPacket &p_packet, const Vector3 &p_anchor, const Vector3 &p_extent, const CameraMatrix &p_projection, const Size2i &p_target, CameraMatrix &r_projection);
	Error _submit_geometry(const FFDrawPacket &p_packet, const CameraMatrix &p_projection, RID p_texture);
	Error _prepare_multimesh_light_spheres(FFMultimeshLights &r_lights, const Vector3 &p_camera_position) const;
	bool _prepare_multimesh_lighting(FFDrawPacket &r_packet, const Transform &p_world, const Transform &p_view, const FFSceneOptions &p_options, bool p_pixel) const;
	bool _request_multimesh_geometry(FFDrawPacket &r_packet, const Transform &p_world, const Transform &p_view, const FFSceneOptions &p_options);
	Error _build_multimesh_geometry(const FFDrawPacket &p_packet, PoolVector<FixedFunctionVertex> &r_vertices, PoolVector<FixedFunctionLightingVertex> &r_lit_vertices, PoolVector<uint32_t> &r_indices, Vector3 &r_anchor, Vector3 &r_extent);
	Error _submit_multimesh_copies(const FFDrawPacket &p_packet, const CameraMatrix &p_projection, RID p_texture, uint32_t p_batch_primitives);
	Error _submit_triangles(const FFDrawPacket &p_packet, const CameraMatrix &p_projection, RID p_texture, uint32_t p_batch_primitives, bool p_first_surface);
	uint32_t _remap_batch(const FFDrawPacket &p_packet, const uint8_t *p_indices, const uint32_t *p_expanded, uint32_t p_first_index, uint32_t p_count);
	Error _pack_unshaded_batch(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint8_t *p_indices, const uint32_t *p_expanded, uint32_t p_first_index, uint32_t p_count);
	Error _pack_indexed_batch(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_vertex_count, const FFOcclusionSample *p_occlusion);
	Error _submit_indexed_lit_triangles(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint8_t *p_indices, const CameraMatrix &p_projection, RID p_texture, bool p_first_surface, const FFOcclusionSample *p_occlusion);
	Error _submit_emission(const FFDrawPacket &p_packet, const CameraMatrix &p_projection, RID p_albedo_texture, uint32_t p_batch_primitives, bool p_retained);
	Error _submit_retained_emission(const FFDrawPacket &p_packet, const FFDrawPacket &p_pass, const CameraMatrix &p_projection, RID p_texture);
	Error _submit_emission_triangles(const FFDrawPacket &p_color, const FFDrawPacket &p_mask, const CameraMatrix &p_projection, RID p_color_texture, RID p_mask_texture, uint32_t p_batch_primitives);
	Error _submit_sky(const Transform &p_camera, const CameraMatrix &p_projection, RID p_environment);
	Error _prepare_sky_vertices(uint32_t p_color);
	static FixedFunctionVertex _sky_vertex(int p_column, int p_row, uint32_t p_color);
	Error _submit(const Transform &p_camera, const CameraMatrix &p_projection, RID p_environment);
	bool _prepare_depth_fog(const CameraMatrix &p_projection, RID p_environment);
	FixedFunctionSceneRenderer(const FixedFunctionSceneRenderer &);
	FixedFunctionSceneRenderer &operator=(const FixedFunctionSceneRenderer &);

public:
	int get_directional_shadow_size() const;
	void begin_directional_shadows();
	Error set_directional_shadow_transform(RID p_light, const CameraMatrix &p_projection, const Transform &p_transform, int p_pass, float p_bias_scale);
	Error render_directional_shadow(RID p_light, int p_pass, RasterizerScene::InstanceBase **p_instances, int p_count);
	Error render(const Transform &p_camera, const CameraMatrix &p_projection, RasterizerScene::InstanceBase **p_instances, int p_count, RID p_environment = RID(), const RID *p_lights = nullptr, int p_light_count = 0);
	void set_debug_unshaded(bool p_enabled) { _debug_unshaded = p_enabled; }
	bool is_redraw_requested() const { return _redraw_requested; }
	FixedFunctionSceneRenderer(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets);
	~FixedFunctionSceneRenderer();
};

#endif // FIXED_FUNCTION_SCENE_RENDERER_H
