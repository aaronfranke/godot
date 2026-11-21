#pragma once

#include "core/templates/rb_map.h"
#include "drivers/fixed_function/material_storage.h"
#include "servers/rendering/storage/mesh_storage.h"

#include <string.h> // memcpy

// Own the branch-native packed buffers. Snapshots retain their COW source data;
// native draw limits never restrict the number of vertices in a logical surface.
class FixedFunctionMeshStorage {
public:
	struct FFDeformedVertex {
		Vector3 position;
		Vector3 normal;
	};
	struct FFDeformedTangentFrame {
		Vector3 tangent;
		Vector3 bitangent;
	};
	struct FFPositionBounds {
		Vector3 minimum;
		Vector3 maximum;
		bool valid = false;

		void expand_to(const Vector3 &p_position);
	};
	struct FFGeometryBounds {
		double minimum[3];
		double maximum[3];

		bool transform(const Transform3D &p_transform);
		double range_distance_squared(const Vector3 &p_position, real_t p_range) const;
	};
	struct FFSurfaceData {
		uint64_t identity;
		uint64_t version;
		RenderingServerTypes::SurfaceData source;
		Vector<FFPositionBounds> skinning_bounds;
		// Actual decoded positions may extend beyond the caller's AABB after a region edit.
		FFPositionBounds position_bounds;
		uint32_t offsets[RSE::ARRAY_MAX];
		uint32_t vertex_stride;
		uint32_t normal_stride;
		uint32_t attribute_stride;
		uint32_t skin_stride;
		FFSurfaceData();
	};
	enum FFBuffer {
		BUFFER_VERTEX,
		BUFFER_ATTRIBUTE,
		BUFFER_SKIN,
		BUFFER_INDEX,
	};

private:
	struct FFMesh {
		Dependency dependency;
		AABB custom_aabb;
		RID self;
		RID shadow_mesh;
		Vector<FFSurfaceData *> surfaces;
		String path;
		int blend_shape_count;
		RSE::BlendShapeMode blend_shape_mode;
		bool aabb_dirty;
		bool materials_dirty;
		bool free_pending;
		FFMesh();
	};
	struct FFMultiMesh {
		Dependency dependency;
		RendererMeshStorage::MultiMeshInterpolator interpolator;
		Vector<float> buffer;
		AABB custom_aabb;
		FFGeometryBounds geometry_bounds = {};
		RID mesh;
		uint64_t geometry_version = 1;
		uint64_t geometry_changed_frame = 0;
		uint64_t bounds_geometry_version = 0;
		uint64_t bounds_surface_identity = 0;
		uint64_t bounds_surface_version = 0;
		int count = 0;
		int visible = -1;
		int stride = 12;
		bool transform_2d = false;
		bool colors = false;
		bool custom_data = false;
		bool bounds_dirty = false;
		bool geometry_bounds_valid = false;
		bool mesh_dirty = false;
		bool freeing = false;
	};
	struct FFSkeleton {
		Dependency dependency;
		Transform2D base_transform_2d;
		Vector<Transform3D> bones;
		Vector<Transform2D> bones_2d;
		bool use_2d = false;
		bool dirty = false;
		bool freeing = false;
	};
	struct FFMeshInstance {
		Transform2D canvas_transform;
		RID self;
		RID base;
		RID skeleton;
		Vector<float> blend_weights;
	};
	struct FFDependencyEvent {
		RID mesh;
		bool aabb;
		bool materials;
	};
	uint64_t _next_surface_identity;
	uint64_t _frame_number = 1;
	FixedFunctionMaterialStorage &_materials;
	RBMap<uint64_t, FFSurfaceData *> _surfaces;
	// RID lookup lacks a const API; keep read-only queries until const lookup exists.
	mutable RID_PtrOwner<FFMesh> _mesh_owner;
	mutable RID_PtrOwner<FFMeshInstance> _instance_owner;
	mutable RID_Owner<FFSkeleton> _skeleton_owner;
	mutable RID_Owner<FFMultiMesh> _multimesh_owner;
	List<RID> _multimeshes;
	List<RID> _skeletons;
	List<FFMesh *> _meshes;
	List<FFMeshInstance *> _instances;
	bool _clearing;
	bool _notifying;
	bool _clear_pending;

	static Transform3D _multimesh_transform(const float *p_data);
	const float *_multimesh_instance_data(RID p_multimesh, int p_index) const;
	FFMesh *_get_mesh(RID p_mesh) const;
	FFSurfaceData *_get_surface(RID p_mesh, int p_surface) const;
	FFMeshInstance *_get_instance(RID p_instance) const;
	static bool _valid_aabb(const AABB &p_aabb, bool p_allow_unused = false);
	static Error _make_layout(FFSurfaceData &r_data, int p_blend_shape_count);
	static Error _validate_indices(const FFSurfaceData &p_data, const Vector<uint8_t> &p_indices);
	static Error _validate_positions(const FFSurfaceData &p_data, const Vector<uint8_t> &p_vertices, uint64_t p_offset = 0, FFPositionBounds *r_bounds = nullptr);
	static Basis _decode_surface_rotation(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, float *r_sign);
	static void _build_skinning_bounds(FFSurfaceData &r_surface);
	static bool _merge_skinning_bounds(const FFSurfaceData &p_surface, const FFSkeleton &p_skeleton, AABB &r_bounds);
	void _delete_surface(FFSurfaceData *p_surface);
	void _free_mesh(FFMesh *p_mesh);
	void _retire_pending();
	FixedFunctionMeshStorage(const FixedFunctionMeshStorage &);
	FixedFunctionMeshStorage &operator=(const FixedFunctionMeshStorage &);

public:
	// Stable identities survive surface-index shifts; revisions follow accepted buffer writes.
	bool surface_is_current(uint64_t p_identity, uint64_t p_version) const;
	RID mesh_create();
	bool owns_mesh(RID p_mesh) const { return _mesh_owner.owns(p_mesh); }
	Error mesh_add_surface(RID p_mesh, const RenderingServerTypes::SurfaceData &p_surface);
	Error mesh_set_blend_shape_count(RID p_mesh, int p_count);
	int mesh_get_blend_shape_count(RID p_mesh) const;
	Error mesh_set_blend_shape_mode(RID p_mesh, RSE::BlendShapeMode p_mode);
	RSE::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const;
	Error mesh_surface_update_region(RID p_mesh, int p_surface, FFBuffer p_buffer, int p_offset, const Vector<uint8_t> &p_data);
	Error mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material);
	RID mesh_surface_get_material(RID p_mesh, int p_surface) const;
	Error mesh_surface_get_data(RID p_mesh, int p_surface, FFSurfaceData &r_data) const;
	// Optional tangent xyz and bitangent sign w share the compressed rotation decode.
	static Vector3 surface_get_normal(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index, Vector4 *r_tangent = nullptr);
	static Vector4 surface_get_tangent(const FFSurfaceData &p_surface, const uint8_t *p_data, uint32_t p_index);
	static Vector3 surface_get_position(const FFSurfaceData &p_surface, const uint8_t *p_vertices, uint32_t p_index);
	static Vector2 surface_read_uv(const FFSurfaceData &p_surface, uint32_t p_index, int p_uv_set);
	static Color surface_read_color(const FFSurfaceData &p_surface, uint32_t p_index);
	// Keep the index read inline for the remap and vertex packing loops.
	static uint32_t surface_read_index(const FFSurfaceData &p_surface, const uint8_t *p_indices, uint32_t p_index) {
		if (!p_surface.source.index_count) {
			return p_index;
		}
		if (p_surface.source.vertex_count > 65536) {
			uint32_t index;
			memcpy(&index, p_indices + p_index * 4, 4);
			return index;
		}
		uint16_t index;
		memcpy(&index, p_indices + p_index * 2, 2);
		return index;
	}
	static uint32_t surface_get_triangle_count(const FFSurfaceData &p_surface);
	static void surface_expand_triangle_strip(const FFSurfaceData &p_surface, const uint8_t *p_indices, uint32_t p_first_triangle, uint32_t p_count, uint32_t *r_indices);
	Error mesh_remove_surface(RID p_mesh, int p_surface);
	int mesh_get_surface_count(RID p_mesh) const;
	Error mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb);
	AABB mesh_get_custom_aabb(RID p_mesh) const;
	Error mesh_get_aabb(RID p_mesh, RID p_skeleton, AABB &r_aabb) const;
	Error mesh_clear(RID p_mesh);
	Error mesh_set_path(RID p_mesh, const String &p_path);
	String mesh_get_path(RID p_mesh) const;
	Error mesh_set_shadow_mesh(RID p_mesh, RID p_shadow);
	void mesh_debug_usage(List<RenderingServerTypes::MeshInfo> *r_info) const;
	Error mesh_update_dependency(RID p_mesh, DependencyTracker *p_tracker);
	Error update_dependencies();
	RID mesh_instance_create(RID p_mesh);
	bool owns_mesh_instance(RID p_instance) const { return _instance_owner.owns(p_instance); }
	Error mesh_instance_set_skeleton(RID p_instance, RID p_skeleton);
	Error mesh_instance_set_blend_shape_weight(RID p_instance, int p_shape, float p_weight);
	Error mesh_instance_set_canvas_transform(RID p_instance, const Transform2D &p_transform);
	bool owns_multimesh(RID p_multimesh) const { return _multimesh_owner.owns(p_multimesh); }
	RID multimesh_allocate();
	void multimesh_initialize(RID p_multimesh);
	void multimesh_free(RID p_multimesh);
	Error multimesh_allocate_data(RID p_multimesh, int p_count, bool p_colors, bool p_custom_data, bool p_transform_2d = false);
	Error multimesh_set_mesh(RID p_multimesh, RID p_mesh);
	RID multimesh_get_mesh(RID p_multimesh) const;
	int multimesh_get_instance_count(RID p_multimesh) const;
	int multimesh_get_visible_instances(RID p_multimesh) const;
	int multimesh_get_visible_count(RID p_multimesh) const;
	// Zero while 3D instance data is changing or settling.
	uint64_t multimesh_get_geometry_version(RID p_multimesh) const;
	void advance_frame(uint64_t p_frame_number) { _frame_number = p_frame_number; }
	Error multimesh_set_visible_instances(RID p_multimesh, int p_visible);
	Error multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer);
	Vector<float> multimesh_get_buffer(RID p_multimesh) const;
	Error multimesh_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform);
	Transform3D multimesh_get_transform(RID p_multimesh, int p_index) const;
	Error multimesh_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform);
	Transform2D multimesh_get_transform_2d(RID p_multimesh, int p_index) const;
	Error multimesh_set_channel(RID p_multimesh, int p_index, const Color &p_value, bool p_custom_data);
	Color multimesh_get_channel(RID p_multimesh, int p_index, bool p_custom_data) const;
	Error multimesh_set_custom_aabb(RID p_multimesh, const AABB &p_aabb);
	AABB multimesh_get_custom_aabb(RID p_multimesh) const;
	AABB multimesh_get_aabb(RID p_multimesh) const;
	bool multimesh_get_geometry_bounds(RID p_multimesh, const FFSurfaceData &p_surface, FFGeometryBounds &r_bounds);
	void multimesh_update_dependency(RID p_multimesh, DependencyTracker *p_tracker);
	RendererMeshStorage::MultiMeshInterpolator *multimesh_get_interpolator(RID p_multimesh) const;
	bool owns_skeleton(RID p_skeleton) const { return _skeleton_owner.owns(p_skeleton); }
	RID skeleton_allocate();
	void skeleton_initialize(RID p_skeleton);
	Error skeleton_allocate_data(RID p_skeleton, int p_count, bool p_2d = false);
	int skeleton_get_bone_count(RID p_skeleton) const;
	Error skeleton_bone_set_transform(RID p_skeleton, int p_bone, const Transform3D &p_transform);
	Transform3D skeleton_bone_get_transform(RID p_skeleton, int p_bone) const;
	Error skeleton_bone_set_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform);
	Transform2D skeleton_bone_get_transform_2d(RID p_skeleton, int p_bone) const;
	Error skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_transform);
	Error skeleton_get_pose_2d(RID p_skeleton, Vector<Transform2D> &r_bones, Transform2D &r_base) const;
	void skeleton_update_dependency(RID p_skeleton, DependencyTracker *p_tracker);
	void skeleton_free(RID p_skeleton);
	Error mesh_instance_deform_2d(RID p_instance, const FFSurfaceData &p_surface, Vector<FFDeformedVertex> &r_vertices, AABB &r_bounds, const Transform2D *p_item_transform = nullptr) const;
	Error mesh_instance_deform(RID p_instance, const FFSurfaceData &p_surface, Vector<FFDeformedVertex> &r_vertices, AABB &r_bounds, Vector<FFDeformedTangentFrame> *r_tangents = nullptr, FFPositionBounds *r_position_bounds = nullptr, bool p_positions_only = false) const;
	Error mesh_instance_get_draw_status(RID p_instance, RID p_mesh) const;
	Error free(RID p_rid);
	void clear();
	int get_mesh_count() const { return _meshes.size(); }

	FixedFunctionMeshStorage(FixedFunctionMaterialStorage &p_materials);
	~FixedFunctionMeshStorage();
};
