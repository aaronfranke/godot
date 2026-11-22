#include "geometry_cache.h"

FixedFunctionGeometryCache::FFAmbientOcclusion::FFAmbientOcclusion(const FixedFunctionSpatialMaterial::FFOcclusionChannel &p_channel, uint64_t p_image_version, uint8_t p_sampler_flags) :
		uv_scale(p_channel.uv_scale),
		uv_offset(p_channel.uv_offset),
		weights(p_channel.weights),
		image_version(p_image_version),
		uv_set(p_channel.uv_set),
		sampler_flags(p_sampler_flags) {}

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

FixedFunctionGeometryCache::FFGeometryEntry::FFGeometryEntry() :
		version(0), status(ERR_SKIP) {}

bool FixedFunctionGeometryCache::_is_current(const FFGeometryKey &p_key, const FFGeometryEntry &p_entry) const {
	return _meshes.surface_is_current(p_key.surface, p_entry.version) &&
			(!p_key.multimesh || _meshes.multimesh_get_geometry_version(p_key.multimesh) == p_entry.multimesh_version);
}

Error FixedFunctionGeometryCache::get(uint64_t p_surface, uint64_t p_version, int p_uv_set, RID &r_geometry, FixedFunctionGraphicsDevice::FFGeometryDrawInfo &r_info, Vector3 &r_anchor, Vector3 &r_extent, bool p_lit, int p_normal_uv_set, bool p_secondary_uv, const FFAmbientOcclusion *p_occlusion, uint64_t p_multimesh) {
	const FFGeometryKey key = { p_surface, p_multimesh, p_occlusion ? *p_occlusion : FFAmbientOcclusion(), p_uv_set, p_normal_uv_set, p_lit, p_secondary_uv };
	Map<FFGeometryKey, FFGeometryEntry>::Element *entry = _entries.find(key);
	if (!entry || entry->get().version != p_version || !_is_current(key, entry->get())) {
		return ERR_DOES_NOT_EXIST;
	}
	if (key.occlusion.image_version || key.multimesh) {
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

Error FixedFunctionGeometryCache::_retire(Map<FFGeometryKey, FFGeometryEntry>::Element *p_entry) {
	if (p_entry->get().geometry.is_valid() && _retired.push_back(p_entry->get().geometry)) {
		return ERR_OUT_OF_MEMORY;
	}
	_entries.erase(p_entry);
	return OK;
}

template <class TVertex>
Error FixedFunctionGeometryCache::_request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const PoolVector<TVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, bool p_lit, int p_normal_uv_set, bool p_secondary_uv, const FFAmbientOcclusion *p_occlusion, uint64_t p_multimesh) {
	if (_updating) {
		return ERR_BUSY;
	}
	if (p_uv_set < 0 || !_meshes.surface_is_current(p_surface.identity, p_surface.version)) {
		return ERR_INVALID_PARAMETER;
	}
	const uint64_t multimesh_version = p_multimesh ? _meshes.multimesh_get_geometry_version(p_multimesh) : 0;
	if (p_multimesh && !multimesh_version) {
		return ERR_SKIP;
	}
	const FFGeometryKey key = { p_surface.identity, p_multimesh, p_occlusion ? *p_occlusion : FFAmbientOcclusion(), p_uv_set, p_normal_uv_set, p_lit, p_secondary_uv };
	Map<FFGeometryKey, FFGeometryEntry>::Element *previous = _entries.find(key);
	if (previous) {
		if (previous->get().version == p_surface.version && previous->get().multimesh_version == multimesh_version) {
			if (key.occlusion.image_version || key.multimesh) {
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
	entry.anchor = p_anchor;
	entry.extent = p_extent;
	entry.status = p_build_error == OK ? ERR_SKIP : ERR_UNAVAILABLE;
	if (p_build_error == OK) {
		_set_vertices(entry, p_vertices);
		entry.indices = p_indices;
	}
	_entries[key] = entry;
	return OK;
}

Error FixedFunctionGeometryCache::update_pending() {
	if (_updating) {
		return ERR_BUSY;
	}
	for (Map<FFGeometryKey, FFGeometryEntry>::Element *E = _entries.front(); E;) {
		Map<FFGeometryKey, FFGeometryEntry>::Element *next = E->next();
		const bool idle_variant = (E->key().occlusion.image_version || E->key().multimesh) && _frame_number - E->get().last_use_frame > VARIANT_IDLE_FRAMES;
		if (!_is_current(E->key(), E->get()) || idle_variant) {
			const Error error = _retire(E);
			if (error != OK) {
				return error;
			}
		}
		E = next;
	}
	_updating = true;
	while (!_retired.empty()) {
		const Error error = _device.geometry_free(_retired[_retired.size() - 1]);
		if (error != OK) {
			_updating = false;
			return error;
		}
		_retired.resize(_retired.size() - 1);
	}
	for (Map<FFGeometryKey, FFGeometryEntry>::Element *E = _entries.front(); E; E = E->next()) {
		FFGeometryEntry &entry = E->get();
		if (entry.status != ERR_SKIP) {
			continue;
		}
		Error error;
		if (E->key().normal_uv_set >= 0) {
			error = _device.geometry_create_normal_mapped(entry.normal_vertices, entry.indices, entry.geometry);
		} else if (E->key().lit) {
			error = _device.geometry_create_lit(entry.lighting_vertices, entry.indices, entry.geometry);
		} else {
			error = _device.geometry_create(entry.vertices, entry.indices, entry.geometry);
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
		entry.vertices = PoolVector<FixedFunctionVertex>();
		entry.lighting_vertices = PoolVector<FixedFunctionLightingVertex>();
		entry.normal_vertices = PoolVector<FixedFunctionNormalMapVertex>();
		entry.indices = PoolVector<uint32_t>();
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

void FixedFunctionGeometryCache::_set_vertices(FFGeometryEntry &p_entry, const PoolVector<FixedFunctionVertex> &p_vertices) { p_entry.vertices = p_vertices; }
Error FixedFunctionGeometryCache::request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const PoolVector<FixedFunctionVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, uint64_t p_multimesh) {
	return _request(p_surface, p_uv_set, p_vertices, p_indices, p_anchor, p_extent, p_build_error, false, -1, false, nullptr, p_multimesh);
}

void FixedFunctionGeometryCache::_set_vertices(FFGeometryEntry &p_entry, const PoolVector<FixedFunctionLightingVertex> &p_vertices) { p_entry.lighting_vertices = p_vertices; }
Error FixedFunctionGeometryCache::request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, const PoolVector<FixedFunctionLightingVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error, bool p_secondary_uv, const FFAmbientOcclusion *p_occlusion, uint64_t p_multimesh) {
	return _request(p_surface, p_uv_set, p_vertices, p_indices, p_anchor, p_extent, p_build_error, true, -1, p_secondary_uv, p_occlusion, p_multimesh);
}

void FixedFunctionGeometryCache::_set_vertices(FFGeometryEntry &p_entry, const PoolVector<FixedFunctionNormalMapVertex> &p_vertices) { p_entry.normal_vertices = p_vertices; }
Error FixedFunctionGeometryCache::request(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, int p_uv_set, int p_normal_uv_set, const PoolVector<FixedFunctionNormalMapVertex> &p_vertices, const PoolVector<uint32_t> &p_indices, const Vector3 &p_anchor, const Vector3 &p_extent, Error p_build_error) {
	if (p_normal_uv_set < 0) {
		return ERR_INVALID_PARAMETER;
	}
	return _request(p_surface, p_uv_set, p_vertices, p_indices, p_anchor, p_extent, p_build_error, true, p_normal_uv_set);
}
