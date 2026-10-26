#ifndef FIXED_FUNCTION_GEOMETRY_CACHE_H
#define FIXED_FUNCTION_GEOMETRY_CACHE_H

#include "mesh_storage.h"

// Object-relative upload copies. Ambient AO variants also depend on the CPU image and sampler.
class FixedFunctionGeometryCache {
public:
	struct FFAmbientOcclusion {
		Vector2 uv_scale;
		Vector2 uv_offset;
		Color weights;
		uint64_t image_version = 0;
		int uv_set = 0;
		uint8_t sampler_flags = 0;

		FFAmbientOcclusion() = default;
		FFAmbientOcclusion(const FixedFunctionSpatialMaterial::FFOcclusionChannel &p_channel, uint64_t p_image_version, uint8_t p_sampler_flags);
		bool operator<(const FFAmbientOcclusion &p_other) const;
	};

private:
	enum { VARIANT_IDLE_FRAMES = 8 };
	friend class TestFixedFunctionRetainedScene;
	struct FFGeometryKey {
		uint64_t surface;
		uint64_t multimesh;
		FFAmbientOcclusion occlusion;
		int uv_set;
		int normal_uv_set;
		bool lit;
		bool secondary_uv;
		bool operator<(const FFGeometryKey &p_other) const;
	};
	struct FFGeometryEntry {
		uint64_t version;
		uint64_t multimesh_version = 0;
		uint64_t last_use_frame = 0;
		Vector3 anchor;
		Vector3 extent;
		RID geometry;
		PoolVector<FixedFunctionVertex> vertices;
		PoolVector<FixedFunctionLightingVertex> lighting_vertices;
		PoolVector<FixedFunctionNormalMapVertex> normal_vertices;
		PoolVector<uint32_t> indices;
		FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
		Error status;
		FFGeometryEntry();
	};
	FixedFunctionGraphicsDevice &_device;
	FixedFunctionMeshStorage &_meshes;
	Map<FFGeometryKey, FFGeometryEntry> _entries;
	Vector<RID> _retired;
	uint64_t _frame_number = 1;
	bool _updating;
	template <class TVertex>
	Error _request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const PoolVector<TVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, bool p_lit, int p_normal_uv_set = -1, bool p_secondary_uv = false, const FFAmbientOcclusion *p_occlusion = nullptr, uint64_t p_multimesh = 0);
	bool _is_current(const FFGeometryKey &p_key, const FFGeometryEntry &p_entry) const;
	static void _set_vertices(FFGeometryEntry &p_entry, const PoolVector<FixedFunctionVertex> &p_vertices);
	static void _set_vertices(FFGeometryEntry &p_entry, const PoolVector<FixedFunctionLightingVertex> &p_vertices);
	static void _set_vertices(FFGeometryEntry &p_entry, const PoolVector<FixedFunctionNormalMapVertex> &p_vertices);
	Error _retire(Map<FFGeometryKey, FFGeometryEntry>::Element *p_entry);
	FixedFunctionGeometryCache(const FixedFunctionGeometryCache &);
	FixedFunctionGeometryCache &operator=(const FixedFunctionGeometryCache &);

public:
	// Pending/blocked entries prevent rebuilding a CPU copy on every draw.
	Error get(uint64_t p_surface, uint64_t p_version, int p_uv_set, RID &r_geometry, FixedFunctionGraphicsDevice::FFGeometryDrawInfo &r_info, Vector3 &r_anchor, Vector3 &r_extent, bool p_lit = false, int p_normal_uv_set = -1, bool p_secondary_uv = false, const FFAmbientOcclusion *p_occlusion = nullptr, uint64_t p_multimesh = 0);
	// CPU-only request; safe during a pass. A rejected build is remembered until
	// source mutation. Newer versions never reuse the preceding upload.
	Error request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const PoolVector<FixedFunctionVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error = OK, uint64_t p_multimesh = 0);
	Error request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const PoolVector<FixedFunctionLightingVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error = OK, bool p_secondary_uv = false, const FFAmbientOcclusion *p_occlusion = nullptr, uint64_t p_multimesh = 0);
	Error request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, int p_normal_uv_set, const PoolVector<FixedFunctionNormalMapVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error = OK);
	// Idle-only native creation/release. Deletions and stale revisions are pruned
	// here, including resources belonging to surfaces no longer visible.
	void advance_frame(uint64_t p_frame_number) { _frame_number = p_frame_number; }
	Error update_pending();
	Error clear();
	int get_entry_count() const { return _entries.size(); }
	int get_retired_count() const { return _retired.size(); }
	FixedFunctionGeometryCache(FixedFunctionGraphicsDevice &p_device, FixedFunctionMeshStorage &p_meshes);
	~FixedFunctionGeometryCache();
};

#endif // FIXED_FUNCTION_GEOMETRY_CACHE_H
