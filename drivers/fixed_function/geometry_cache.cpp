#include "geometry_cache.h"

#include <string.h>

FixedFunctionGeometryCache::FFAmbientOcclusion::FFAmbientOcclusion(const FixedFunctionSpatialMaterial::FFOcclusionChannel &p_channel, uint64_t p_image_version) :
		uv_scale(p_channel.uv_scale),
		uv_offset(p_channel.uv_offset),
		weights(p_channel.weights),
		image_version(p_image_version),
		uv_set(p_channel.uv_set),
		sampler_flags(p_channel.sampler_flags) {}

bool FixedFunctionGeometryCache::FFAmbientOcclusion::operator<(const FFAmbientOcclusion &p_other) const {
	if (image_version != p_other.image_version) {
		return image_version < p_other.image_version;
	}
	if (!image_version) {
		return false;
	}
	if (uv_set != p_other.uv_set) {
		return uv_set < p_other.uv_set;
	}
	if (sampler_flags != p_other.sampler_flags) {
		return sampler_flags < p_other.sampler_flags;
	}
	if (uv_scale != p_other.uv_scale) {
		return uv_scale < p_other.uv_scale;
	}
	if (uv_offset != p_other.uv_offset) {
		return uv_offset < p_other.uv_offset;
	}
	return weights < p_other.weights;
}

bool FixedFunctionGeometryCache::FFGeometryKey::operator<(const FFGeometryKey &p_other) const {
	if (surface != p_other.surface) {
		return surface < p_other.surface;
	}
	if (multimesh != p_other.multimesh) {
		return multimesh < p_other.multimesh;
	}
	if (multimesh_lod != p_other.multimesh_lod) {
		return multimesh_lod < p_other.multimesh_lod;
	}
	if (uv_set != p_other.uv_set) {
		return uv_set < p_other.uv_set;
	}
	if (normal_uv_set != p_other.normal_uv_set) {
		return normal_uv_set < p_other.normal_uv_set;
	}
	if (lit != p_other.lit) {
		return lit < p_other.lit;
	}
	if (secondary_uv != p_other.secondary_uv) {
		return secondary_uv < p_other.secondary_uv;
	}
	return occlusion < p_other.occlusion;
}

FixedFunctionGeometryCache::FFGeometryEntry::FFGeometryEntry() : version(0), status(ERR_SKIP) {}

bool FixedFunctionGeometryCache::_is_current(const FFGeometryKey &p_key, const FFGeometryEntry &p_entry) const {
	return _meshes.surface_is_current(p_key.surface, p_entry.version) &&
			(!p_key.multimesh.is_valid() || _meshes.multimesh_get_geometry_version(p_key.multimesh) == p_entry.multimesh_version);
}

Error FixedFunctionGeometryCache::get(uint64_t p_surface, uint64_t p_version, int p_uv_set, RID &r_geometry, FixedFunctionGraphicsDevice::FFGeometryDrawInfo &r_info, Vector3 &r_anchor, Vector3 &r_extent, bool p_lit, int p_normal_uv_set, bool p_secondary_uv, const FFAmbientOcclusion *p_occlusion, RID p_multimesh, uint32_t p_multimesh_lod) {
	const FFGeometryKey key = { p_surface, p_multimesh, p_occlusion ? *p_occlusion : FFAmbientOcclusion(), p_uv_set, p_normal_uv_set, p_multimesh_lod, p_lit, p_secondary_uv };
	RBMap<FFGeometryKey, FFGeometryEntry>::Element *entry = _entries.find(key);
	if (!entry || entry->get().version != p_version || !_is_current(key, entry->get())) {
		return ERR_DOES_NOT_EXIST;
	}
	if (key.occlusion.image_version || key.multimesh.is_valid()) {
		entry->get().last_use_frame = _frame_number;
	}
	if (entry->get().status == OK) {
		r_geometry = entry->get().geometry;
		r_info = entry->get().info;
		r_anchor = entry->get().anchor;
		r_extent = entry->get().extent;
	}
	return entry->get().status;
}

Error FixedFunctionGeometryCache::_retire(RBMap<FFGeometryKey, FFGeometryEntry>::Element *p_entry) {
	if (p_entry->get().geometry.is_valid() && _retired.push_back(p_entry->get().geometry)) {
		return ERR_OUT_OF_MEMORY;
	}
	_entries.erase(p_entry);
	return OK;
}

template <class TVertex>
Error FixedFunctionGeometryCache::_request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const Vector<TVertex> &p_vertices, const Vector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, bool p_share_lods, bool p_lit, int p_normal_uv_set, bool p_secondary_uv, const FFAmbientOcclusion *p_occlusion, RID p_multimesh, uint32_t p_multimesh_lod) {
	if (_updating) {
		return ERR_BUSY;
	}
	if (p_uv_set < 0 || !_meshes.surface_is_current(p_surface.identity, p_surface.version)) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t multimesh_version = p_multimesh.is_valid() ? _meshes.multimesh_get_geometry_version(p_multimesh) : 0;
	if (p_multimesh.is_valid() && !multimesh_version) {
		return ERR_SKIP;
	}
	const FFGeometryKey key = { p_surface.identity, p_multimesh, p_occlusion ? *p_occlusion : FFAmbientOcclusion(), p_uv_set, p_normal_uv_set, p_multimesh_lod, p_lit, p_secondary_uv };
	RBMap<FFGeometryKey, FFGeometryEntry>::Element *previous = _entries.find(key);
	if (previous) {
		if (previous->get().version == p_surface.version && previous->get().multimesh_version == multimesh_version) {
			if (key.occlusion.image_version || key.multimesh.is_valid()) {
				previous->get().last_use_frame = _frame_number;
			}
			return OK;
		}
		const Error error = _retire(previous);
		if (error != OK) {
			return error;
		}
	}
	FFGeometryEntry entry;
	entry.version = p_surface.version;
	entry.multimesh_version = multimesh_version;
	entry.last_use_frame = _frame_number;
	entry.multimesh_has_lods = p_multimesh.is_valid() && !p_surface.source.lods.is_empty();
	entry.anchor = p_anchor;
	entry.extent = p_extent;
	entry.status = p_build_error == OK ? ERR_SKIP : ERR_UNAVAILABLE;
	if (p_build_error == OK) {
		_set_vertices(entry, p_vertices);
		entry.indices = p_indices;
		if (p_share_lods && p_surface.source.primitive == RSE::PRIMITIVE_TRIANGLES && p_surface.source.index_count &&
				p_vertices.size() == p_surface.source.vertex_count && p_vertices.size() <= FixedFunctionGeometryData::MAX_BATCH_VERTICES) {
			entry.lods = p_surface.source.lods;
		}
	}
	_entries[key] = entry;
	return OK;
}

Error FixedFunctionGeometryCache::_create_lod_batch(FFGeometryEntry &r_entry) {
	if (r_entry.lods.is_empty()) {
		return ERR_UNAVAILABLE;
	}
	const int64_t index_limit = INT32_MAX / (2 * sizeof(uint16_t)); // Leave room for vector growth on x86_32.
	int64_t total = r_entry.indices.size();
	for (const RenderingServerTypes::SurfaceData::LOD &lod : r_entry.lods) {
		const int64_t bytes = lod.index_data.size();
		if (bytes == 0 || bytes % (3 * sizeof(uint16_t)) || bytes / int(sizeof(uint16_t)) > index_limit - total) {
			return ERR_UNAVAILABLE;
		}
		total += bytes / sizeof(uint16_t);
	}
	FixedFunctionGeometryData::FFGeometryBatch batch;
	if (batch.indices.resize(total) != OK || batch.lod_offsets.resize(r_entry.lods.size()) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	batch.vertices = r_entry.vertices;
	batch.lighting_vertices = r_entry.lighting_vertices;
	batch.normal_vertices = r_entry.normal_vertices;
	uint16_t *indices = batch.indices.ptrw();
	const uint32_t vertex_count = batch.get_vertex_count();
	uint32_t offset = 0;
	for (uint32_t index : r_entry.indices) {
		if (index >= vertex_count) {
			return ERR_INVALID_DATA;
		}
		indices[offset++] = uint16_t(index);
	}
	uint32_t *lod_offsets = batch.lod_offsets.ptrw();
	for (int64_t i = 0; i < r_entry.lods.size(); i++) {
		const Vector<uint8_t> &data = r_entry.lods[i].index_data;
		lod_offsets[i] = offset;
		memcpy(indices + offset, data.ptr(), data.size());
		offset += uint32_t(data.size() / sizeof(uint16_t));
	}
	return _device.geometry_create_lod_batch(batch, r_entry.geometry);
}

Error FixedFunctionGeometryCache::update_pending() {
	if (_updating) {
		return ERR_BUSY;
	}
	for (RBMap<FFGeometryKey, FFGeometryEntry>::Element *E = _entries.front(); E;) {
		RBMap<FFGeometryKey, FFGeometryEntry>::Element *next = E->next();
		const uint64_t idle_limit = E->get().multimesh_has_lods ? MULTIMESH_LOD_IDLE_FRAMES : VARIANT_IDLE_FRAMES;
		const bool idle_variant = (E->key().occlusion.image_version || E->key().multimesh.is_valid()) && _frame_number - E->get().last_use_frame > idle_limit;
		if (!_is_current(E->key(), E->get()) || idle_variant) {
			const Error error = _retire(E);
			if (error != OK) {
				return error;
			}
		}
		E = next;
	}
	_updating = true;
	while (!_retired.is_empty()) {
		const Error error = _device.geometry_free(_retired[_retired.size() - 1]);
		if (error != OK) {
			_updating = false;
			return error;
		}
		_retired.resize(_retired.size() - 1);
	}
	for (RBMap<FFGeometryKey, FFGeometryEntry>::Element *E = _entries.front(); E; E = E->next()) {
		FFGeometryEntry &entry = E->get();
		if (entry.status != ERR_SKIP) {
			continue;
		}
		Error error = _create_lod_batch(entry);
		if (error == ERR_UNAVAILABLE || error == ERR_INVALID_DATA || error == ERR_INVALID_PARAMETER || error == ERR_OUT_OF_MEMORY) {
			if (E->key().normal_uv_set >= 0) {
				error = _device.geometry_create_normal_mapped(entry.normal_vertices, entry.indices, entry.geometry);
			} else if (E->key().lit) {
				error = _device.geometry_create_lit(entry.lighting_vertices, entry.indices, entry.geometry);
			} else {
				error = _device.geometry_create(entry.vertices, entry.indices, entry.geometry);
			}
		}
		if (error == ERR_SKIP || error == ERR_BUSY || error == ERR_UNCONFIGURED) {
			continue;
		}
		if (error == OK) {
			entry.status = _device.geometry_get_draw_info(entry.geometry, entry.info) == OK ? OK : ERR_UNAVAILABLE;
		} else {
			// Optional cache construction failure retains the existing CPU draw path.
			entry.status = ERR_UNAVAILABLE;
		}
		entry.vertices = Vector<FixedFunctionVertex>();
		entry.lighting_vertices = Vector<FixedFunctionLightingVertex>();
		entry.normal_vertices = Vector<FixedFunctionNormalMapVertex>();
		entry.indices = Vector<uint32_t>();
		entry.lods.clear();
	}
	_updating = false;
	return OK;
}

Error FixedFunctionGeometryCache::clear() {
	if (_updating) {
		return ERR_BUSY;
	}
	while (_entries.front()) {
		const Error error = _retire(_entries.front());
		if (error != OK) {
			return error;
		}
	}
	return update_pending();
}

FixedFunctionGeometryCache::FixedFunctionGeometryCache(FixedFunctionGraphicsDevice &p_device, FixedFunctionMeshStorage &p_meshes) :
		_device(p_device), _meshes(p_meshes), _updating(false) {}

FixedFunctionGeometryCache::~FixedFunctionGeometryCache() {
	if (clear() != OK) {
		ERR_PRINT("Fixed Function geometry cache cleanup failed; remaining native resources stay device-owned until finalization.");
	}
}

void FixedFunctionGeometryCache::_set_vertices(FFGeometryEntry &p_entry, const Vector<FixedFunctionVertex> &p_vertices) {
	p_entry.vertices = p_vertices;
}
Error FixedFunctionGeometryCache::request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, RID p_multimesh, uint32_t p_multimesh_lod) {
	return _request(p_surface, p_uv_set, p_vertices, p_indices, p_anchor, p_extent, p_build_error, !p_multimesh.is_valid(), false, -1, false, nullptr, p_multimesh, p_multimesh_lod);
}

void FixedFunctionGeometryCache::_set_vertices(FFGeometryEntry &p_entry, const Vector<FixedFunctionLightingVertex> &p_vertices) {
	p_entry.lighting_vertices = p_vertices;
}
Error FixedFunctionGeometryCache::request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, bool p_secondary_uv, bool p_share_lods, const FFAmbientOcclusion *p_occlusion, RID p_multimesh, uint32_t p_multimesh_lod) {
	return _request(p_surface, p_uv_set, p_vertices, p_indices, p_anchor, p_extent, p_build_error, p_share_lods, true, -1, p_secondary_uv, p_occlusion, p_multimesh, p_multimesh_lod);
}

void FixedFunctionGeometryCache::_set_vertices(FFGeometryEntry &p_entry, const Vector<FixedFunctionNormalMapVertex> &p_vertices) {
	p_entry.normal_vertices = p_vertices;
}
Error FixedFunctionGeometryCache::request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, int p_normal_uv_set, const Vector<FixedFunctionNormalMapVertex> &p_vertices, const Vector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, bool p_share_lods) {
	if (p_normal_uv_set < 0) {
		return ERR_INVALID_PARAMETER;
	}
	return _request(p_surface, p_uv_set, p_vertices, p_indices, p_anchor, p_extent, p_build_error, p_share_lods, true, p_normal_uv_set);
}
