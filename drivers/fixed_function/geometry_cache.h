#pragma once

#include "drivers/fixed_function/mesh_storage.h"

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
		FFAmbientOcclusion(const FixedFunctionSpatialMaterial::FFOcclusionChannel &p_channel, uint64_t p_image_version);
		bool operator<(const FFAmbientOcclusion &p_other) const;
	};

private:
	enum {
		VARIANT_IDLE_FRAMES = 8,
		// Returning across an LOD threshold can otherwise rebuild and draw every copy.
		MULTIMESH_LOD_IDLE_FRAMES = 30,
	};
	friend class TestFixedFunctionRetainedScene;
	struct FFGeometryKey {
		uint64_t surface;
		RID multimesh;
		FFAmbientOcclusion occlusion;
		int uv_set;
		int normal_uv_set;
		uint32_t multimesh_lod;
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
		Vector<FixedFunctionVertex> vertices;
		Vector<FixedFunctionLightingVertex> lighting_vertices;
		Vector<FixedFunctionNormalMapVertex> normal_vertices;
		Vector<uint32_t> indices;
		Vector<RenderingServerTypes::SurfaceData::LOD> lods;
		FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
		Error status;
		bool multimesh_has_lods = false;
		FFGeometryEntry();
	};
	FixedFunctionGraphicsDevice &_device;
	FixedFunctionMeshStorage &_meshes;
	RBMap<FFGeometryKey, FFGeometryEntry> _entries;
	Vector<RID> _retired;
	uint64_t _frame_number = 1;
	bool _updating;
	template <class TVertex>
	Error _request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const Vector<TVertex> &p_vertices, const Vector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, bool p_share_lods, bool p_lit, int p_normal_uv_set = -1, bool p_secondary_uv = false, const FFAmbientOcclusion *p_occlusion = nullptr, RID p_multimesh = RID(), uint32_t p_multimesh_lod = 0);
	bool _is_current(const FFGeometryKey &p_key, const FFGeometryEntry &p_entry) const;
	Error _create_lod_batch(FFGeometryEntry &r_entry);
	static void _set_vertices(FFGeometryEntry &p_entry, const Vector<FixedFunctionVertex> &p_vertices);
	static void _set_vertices(FFGeometryEntry &p_entry, const Vector<FixedFunctionLightingVertex> &p_vertices);
	static void _set_vertices(FFGeometryEntry &p_entry, const Vector<FixedFunctionNormalMapVertex> &p_vertices);
	Error _retire(RBMap<FFGeometryKey, FFGeometryEntry>::Element *p_entry);
	FixedFunctionGeometryCache(const FixedFunctionGeometryCache &);
	FixedFunctionGeometryCache &operator=(const FixedFunctionGeometryCache &);

public:
	// Pending/blocked entries prevent rebuilding a CPU copy on every draw.
	Error get(uint64_t p_surface, uint64_t p_version, int p_uv_set, RID &r_geometry, FixedFunctionGraphicsDevice::FFGeometryDrawInfo &r_info, Vector3 &r_anchor, Vector3 &r_extent, bool p_lit = false, int p_normal_uv_set = -1, bool p_secondary_uv = false, const FFAmbientOcclusion *p_occlusion = nullptr, RID p_multimesh = RID(), uint32_t p_multimesh_lod = 0);
	// CPU-only request; safe during a pass. A rejected build is remembered until
	// source mutation. Newer versions never reuse the preceding upload.
	Error request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error = OK, RID p_multimesh = RID(), uint32_t p_multimesh_lod = 0);
	Error request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, bool p_secondary_uv, bool p_share_lods, const FFAmbientOcclusion *p_occlusion = nullptr, RID p_multimesh = RID(), uint32_t p_multimesh_lod = 0);
	Error request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, int p_normal_uv_set, const Vector<FixedFunctionNormalMapVertex> &p_vertices, const Vector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, bool p_share_lods);
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
