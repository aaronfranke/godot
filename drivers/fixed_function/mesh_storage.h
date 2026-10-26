#ifndef FIXED_FUNCTION_MESH_STORAGE_H
#define FIXED_FUNCTION_MESH_STORAGE_H

#include "material_storage.h"

#include <string.h>

// CPU ownership of the existing 3.x packed surface format. Storage support does
// not imply drawing support; scene submission must still check each feature.
// Discard owned RIDs after free/clear, as with other 3.x resource owners.
class FixedFunctionMeshStorage {
public:
	struct FFImmediateVertex {
		Plane tangent;
		Vector3 position;
		Vector3 normal;
		Vector2 uv[2];
		Color color;

		FFImmediateVertex() :
				tangent(Vector3(), 1), color(1, 1, 1, 1) {}
	};
	struct FFMultiMeshInstance {
		Transform transform;
		Color color;

		FFMultiMeshInstance() :
				color(1, 1, 1, 1) {}
	};
	struct FFGeometryBounds {
		double minimum[3];
		double maximum[3];

		bool transform(const Transform &p_transform);
		double range_distance_squared(const Vector3 &p_position, real_t p_range) const;
	};

	struct FFSurfaceData {
		uint64_t identity;
		uint64_t version;
		AABB aabb;
		// Decoded position bounds can differ from the caller's culling AABB.
		Vector3 position_min;
		Vector3 position_max;
		PoolVector<uint8_t> array;
		PoolVector<uint8_t> index_array;
		Vector<PoolVector<uint8_t>> blend_shapes;
		Vector<AABB> bone_aabbs;
		// ImmediateGeometry owns branch-native real_t data instead of a packed mesh array.
		Vector<FFImmediateVertex> immediate_vertices;
		RID immediate_texture;
		uint32_t offsets[VS::ARRAY_MAX];
		uint32_t strides[VS::ARRAY_MAX];
		uint32_t format;
		int vertex_count;
		int index_count;
		VS::PrimitiveType primitive;

		FFSurfaceData();
	};

private:
	struct FFSurface {
		FFSurfaceData data;
		AABB morph_bounds[2];
		RID material;
		uint64_t first_immediate_frame;

		FFSurface() :
				first_immediate_frame(0) {}
	};
	struct FFMesh : RID_Data {
		// Map then RID avoids alignment holes after RID_Data in x86_32 handle builds.
		Map<RasterizerScene::InstanceBase *, uint64_t> instances;
		RID self;
		RID dependency_owner;
		AABB custom_aabb;
		Vector<FFSurface *> surfaces;
		PoolVector<float> blend_values;
		int blend_shape_count;
		VS::BlendShapeMode blend_shape_mode;
		bool aabb_dirty;
		bool materials_dirty;

		FFMesh();
	};
	struct FFImmediate : RID_Data {
		// Fill the gap before handle RIDs on x86_32, as with FFMesh.
		Vector<FFImmediateVertex> vertices;
		RID self;
		RID mesh;
		RID material;
		RID texture;
		FFImmediateVertex current;
		AABB bounds;
		uint32_t format;
		VS::PrimitiveType primitive;
		bool building;

		FFImmediate() :
				format(0), primitive(VS::PRIMITIVE_TRIANGLES), building(false) {}
	};
	struct FFSkeleton : RID_Data {
		Map<RasterizerScene::InstanceBase *, uint64_t> instances;
		RID self;
		Transform2D base_transform_2d;
		Vector<Transform> bones;
		Vector<Transform2D> bones_2d;
		Vector<RID> canvas_items;
		uint32_t revision;
		bool use_2d;
		bool dirty;

		FFSkeleton() :
				revision(0), use_2d(false), dirty(false) {}
	};
	struct FFMultiMesh : RID_Data {
		Map<RasterizerScene::InstanceBase *, uint64_t> instances;
		FFGeometryBounds geometry_bounds = {};
		uint64_t geometry_identity;
		uint64_t geometry_version = 1;
		uint64_t geometry_changed_frame = 0;
		uint64_t bounds_geometry_version = 0;
		uint64_t bounds_surface_identity = 0;
		uint64_t bounds_surface_version = 0;
		RID self;
		RID mesh;
		Vector<FFMultiMeshInstance> copies;
		Vector<Color> custom_data;
		Vector<RID> canvas_items;
		int visible;
		VS::MultimeshColorFormat color_format;
		VS::MultimeshCustomDataFormat custom_format;
		VS::MultimeshTransformFormat transform_format;
		bool aabb_dirty;
		bool materials_dirty;
		bool geometry_bounds_valid = false;

		FFMultiMesh();
	};
	struct FFRegistration {
		uint64_t generation;
		RID owner;
	};
	struct FFDependencyEvent {
		uint64_t generation;
		RID owner;
		RasterizerScene::InstanceBase *instance;
		bool aabb;
		bool materials;
		bool skeleton;
	};

	uint64_t _next_generation;
	uint64_t _next_surface_identity;
	uint64_t _next_multimesh_identity = 0;
	uint64_t _frame_number = 1;
	FixedFunctionMaterialStorage &_materials;
	// RID lookup lacks a const API; retain const getters until that API is available.
	mutable RID_Owner<FFMesh> _mesh_owner;
	mutable RID_Owner<FFSkeleton> _skeleton_owner;
	mutable RID_Owner<FFMultiMesh> _multimesh_owner;
	mutable RID_Owner<FFImmediate> _immediate_owner;
	List<FFMesh *> _meshes;
	List<FFSkeleton *> _skeletons;
	List<FFMultiMesh *> _multimeshes;
	List<FFImmediate *> _immediates;
	Map<uint64_t, FFSurface *> _surfaces;
	// Geometry cache entries may outlive a pointer-backed RID.
	Map<uint64_t, FFMultiMesh *> _multimesh_identities;
	Map<RasterizerScene::InstanceBase *, FFRegistration> _registrations;
	Map<RasterizerScene::InstanceBase *, FFRegistration> _skeleton_registrations;
	bool _clearing;
	bool _notifying;

	FFMesh *_get_mesh(RID p_mesh) const;
	FFMesh *_get_dependency_mesh(RID p_base) const;
	FFImmediate *_get_immediate(RID p_immediate) const;
	FFImmediate *_get_building_immediate(RID p_immediate) const;
	void _free_immediate(FFImmediate *p_immediate);
	FFSkeleton *_get_skeleton(RID p_skeleton) const;
	static bool _valid_transform_2d(const Transform2D &p_transform);
	void _invalidate_skeleton_canvas_items(FFSkeleton *p_skeleton, bool p_detach = false);
	FFMultiMesh *_get_multimesh(RID p_multimesh) const;
	void _detach_base_instances(Map<RasterizerScene::InstanceBase *, uint64_t> &r_instances);
	void _free_multimesh(FFMultiMesh *p_multimesh);
	static bool _multimesh_finite(const float *p_values, int p_count);
	static Color _multimesh_read_color(const float *p_data, int p_size);
	static Color _multimesh_quantize_color(const Color &p_color);
	FFSurface *_get_surface(RID p_mesh, int p_surface) const;
	static bool _valid_aabb(const AABB &p_aabb, bool p_allow_unused = false);
	static Error _make_layout(FFSurfaceData &r_data);
	static Error _validate_positions(const FFSurfaceData &p_data, const PoolVector<uint8_t> &p_array, PoolVector<Vector3> *r_positions = nullptr, Vector3 *r_min = nullptr, Vector3 *r_max = nullptr);
	static Error _validate_morphs(const FFSurfaceData &p_data, const PoolVector<uint8_t> &p_base, AABB *r_bounds);
	static Error _validate_indices(const FFSurfaceData &p_data);
	void _delete_surface(FFSurface *p_surface);
	void _free_mesh(FFMesh *p_mesh);
	void _free_skeleton(FFSkeleton *p_skeleton);
	static AABB _skinned_surface_aabb(const FFSurfaceData &p_surface, const FFSkeleton &p_skeleton, const AABB &p_bounds);
	FixedFunctionMeshStorage(const FixedFunctionMeshStorage &);
	FixedFunctionMeshStorage &operator=(const FixedFunctionMeshStorage &);

public:
	RID immediate_create();
	bool owns_immediate(RID p_immediate) const { return _immediate_owner.owns(p_immediate); }
	Error immediate_begin(RID p_immediate, VS::PrimitiveType p_primitive, RID p_texture);
	Error immediate_vertex(RID p_immediate, const Vector3 &p_vertex);
	Error immediate_normal(RID p_immediate, const Vector3 &p_normal);
	Error immediate_tangent(RID p_immediate, const Plane &p_tangent);
	Error immediate_color(RID p_immediate, const Color &p_color);
	Error immediate_uv(RID p_immediate, const Vector2 &p_uv, bool p_uv2 = false);
	Error immediate_end(RID p_immediate);
	Error immediate_clear(RID p_immediate);
	Error immediate_set_material(RID p_immediate, RID p_material);
	RID immediate_get_material(RID p_immediate) const;
	Error immediate_get_aabb(RID p_immediate, AABB &r_aabb) const;
	// Borrowed, read-only backing geometry is absent while begin/end is open, like GLES.
	Error immediate_get_mesh(RID p_immediate, RID &r_mesh) const;
	bool immediate_surface_can_retain(uint64_t p_identity, uint64_t p_frame);
	RID mesh_create();
	bool owns_mesh(RID p_mesh) const { return _mesh_owner.owns(p_mesh); }
	bool owns_multimesh(RID p_multimesh) const { return _multimesh_owner.owns(p_multimesh); }
	RID multimesh_create();
	Error multimesh_allocate(RID p_multimesh, int p_count, VS::MultimeshColorFormat p_color, VS::MultimeshCustomDataFormat p_custom, VS::MultimeshTransformFormat p_format = VS::MULTIMESH_TRANSFORM_3D);
	VS::MultimeshTransformFormat multimesh_get_transform_format(RID p_multimesh) const;
	Error multimesh_attach_canvas_item(RID p_multimesh, RID p_canvas_item, bool p_attach);
	Error multimesh_set_mesh(RID p_multimesh, RID p_mesh);
	RID multimesh_get_mesh(RID p_multimesh) const;
	int multimesh_get_instance_count(RID p_multimesh) const;
	Error multimesh_set_visible(RID p_multimesh, int p_visible);
	int multimesh_get_visible(RID p_multimesh) const;
	Error multimesh_set_transform(RID p_multimesh, int p_index, const Transform &p_transform);
	Error multimesh_set_color(RID p_multimesh, int p_index, const Color &p_color, bool p_custom);
	Transform multimesh_get_transform(RID p_multimesh, int p_index) const;
	Color multimesh_get_color(RID p_multimesh, int p_index, bool p_custom) const;
	Error multimesh_set_bulk(RID p_multimesh, const PoolVector<float> &p_data);
	Error multimesh_get_draw_data(RID p_multimesh, RID &r_mesh, Vector<FFMultiMeshInstance> &r_copies, int &r_visible) const;
	Error multimesh_get_draw_data(uint64_t p_identity, RID &r_mesh, Vector<FFMultiMeshInstance> &r_copies, int &r_visible) const;
	uint64_t multimesh_get_geometry_identity(RID p_multimesh) const;
	// Zero while 3D instance data is changing or settling.
	uint64_t multimesh_get_geometry_version(uint64_t p_identity) const;
	bool multimesh_get_geometry_bounds(uint64_t p_identity, const FFSurfaceData &p_surface, FFGeometryBounds &r_bounds);
	void advance_frame(uint64_t p_frame_number) { _frame_number = p_frame_number; }
	Error multimesh_get_aabb(RID p_multimesh, AABB &r_bounds) const;
	static bool multimesh_is_collapsed(const Transform &p_transform, bool p_2d = false);
	static Transform2D multimesh_to_transform_2d(const Transform &p_transform);
	// Decode a vertex from an already validated packed surface or blend shape.
	static Vector3 surface_read_position(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index);
	Error mesh_add_surface(RID p_mesh, uint32_t p_format, VS::PrimitiveType p_primitive, const PoolVector<uint8_t> &p_array, int p_vertex_count, const PoolVector<uint8_t> &p_index_array, int p_index_count, const AABB &p_aabb, const Vector<PoolVector<uint8_t>> &p_blend_shapes, const Vector<AABB> &p_bone_aabbs);
	Error mesh_set_blend_shape_count(RID p_mesh, int p_count);
	int mesh_get_blend_shape_count(RID p_mesh) const;
	Error mesh_set_blend_shape_mode(RID p_mesh, VS::BlendShapeMode p_mode);
	VS::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const;
	Error mesh_set_blend_shape_values(RID p_mesh, const PoolVector<float> &p_values);
	PoolVector<float> mesh_get_blend_shape_values(RID p_mesh) const;
	Error mesh_surface_update_region(RID p_mesh, int p_surface, int p_offset, const PoolVector<uint8_t> &p_data);
	Error mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material);
	RID mesh_surface_get_material(RID p_mesh, int p_surface) const;
	// Snapshots own their COW arrays; mutations never invalidate a previous snapshot.
	static Vector2 surface_read_uv(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, int p_uv_set);
	static Color surface_read_color(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index);
	// Keep the index read inline for the remap and vertex packing loops.
	static uint32_t surface_read_index(const FFSurfaceData &p_surface, const uint8_t *p_indices, uint32_t p_index) {
		if (!p_surface.index_count) {
			return p_index;
		}
		if (p_surface.vertex_count >= 65536) {
			uint32_t index;
			memcpy(&index, p_indices + p_index * 4, 4);
			return index;
		}
		uint16_t index;
		memcpy(&index, p_indices + p_index * 2, 2);
		return index;
	}
	static uint32_t surface_get_triangle_count(const FFSurfaceData &p_surface);
	static void surface_expand_triangles(const FFSurfaceData &p_surface, const uint8_t *p_indices, uint32_t p_first_triangle, uint32_t p_count, uint32_t *r_indices);
	Error mesh_surface_get_data(RID p_mesh, int p_surface, FFSurfaceData &r_data) const;
	// Stable across array-index changes; identities are never reused by this owner.
	bool surface_is_current(uint64_t p_identity, uint64_t p_version) const;
	Error mesh_surface_get_positions(RID p_mesh, int p_surface, PoolVector<Vector3> &r_positions) const;
	Error mesh_remove_surface(RID p_mesh, int p_surface);
	int mesh_get_surface_count(RID p_mesh) const;
	Error mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb);
	AABB mesh_get_custom_aabb(RID p_mesh) const;
	Error mesh_get_aabb(RID p_mesh, RID p_skeleton, AABB &r_aabb) const;
	Error mesh_clear(RID p_mesh);
	Error instance_add_dependency(RID p_mesh, RasterizerScene::InstanceBase *p_instance);
	Error instance_remove_dependency(RID p_mesh, RasterizerScene::InstanceBase *p_instance);
	RID skeleton_create();
	bool owns_skeleton(RID p_skeleton) const { return _skeleton_owner.owns(p_skeleton); }
	Error skeleton_allocate(RID p_skeleton, int p_bones, bool p_2d = false);
	int skeleton_get_bone_count(RID p_skeleton) const;
	Error skeleton_set_bone_transform(RID p_skeleton, int p_bone, const Transform &p_transform);
	Transform skeleton_get_bone_transform(RID p_skeleton, int p_bone) const;
	uint32_t skeleton_get_revision(RID p_skeleton) const;
	Error skeleton_get_pose(RID p_skeleton, Vector<Transform> &r_bones) const;
	Error skeleton_set_bone_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform);
	Transform2D skeleton_get_bone_transform_2d(RID p_skeleton, int p_bone) const;
	Error skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_transform);
	Error skeleton_get_pose_2d(RID p_skeleton, Vector<Transform2D> &r_bones, Transform2D &r_base) const;
	Error skeleton_attach_canvas_item(RID p_skeleton, RID p_canvas_item, bool p_attach);
	Error instance_add_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance);
	Error instance_remove_skeleton(RID p_skeleton, RasterizerScene::InstanceBase *p_instance);
	Error update_dependencies();
	Error free(RID p_mesh);
	void clear();
	int get_mesh_count() const { return _meshes.size(); }

	FixedFunctionMeshStorage(FixedFunctionMaterialStorage &p_materials);
	~FixedFunctionMeshStorage();
};

#endif // FIXED_FUNCTION_MESH_STORAGE_H
