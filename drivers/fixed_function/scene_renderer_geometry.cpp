#include "drivers/fixed_function/scene_renderer.h"

#include <float.h>

uint32_t FixedFunctionSceneRenderer::_select_mesh_lod(const FixedFunctionMeshStorage::FFSurfaceData &p_surface, float p_model_scale, float p_distance, float p_threshold) {
	const RenderingServerTypes::SurfaceData &source = p_surface.source;
	if (source.lods.is_empty() || !(p_threshold > 0 && p_distance > 0 && p_model_scale > 0) ||
			!Math::is_finite(p_threshold) || !Math::is_finite(p_distance) || !Math::is_finite(p_model_scale)) {
		return 0;
	}
	int minimum = 1;
	int alignment = 1;
	switch (source.primitive) {
		case RSE::PRIMITIVE_TRIANGLES:
			alignment = minimum = 3;
			break;
		case RSE::PRIMITIVE_TRIANGLE_STRIP:
			minimum = 3;
			break;
		case RSE::PRIMITIVE_LINES:
			alignment = minimum = 2;
			break;
		case RSE::PRIMITIVE_LINE_STRIP:
			minimum = 2;
			break;
		default:
			break;
	}
	uint32_t selected = 0;
	const int index_stride = source.vertex_count > 65536 ? 4 : 2;
	for (int64_t i = 0; i < source.lods.size(); i++) {
		const RenderingServerTypes::SurfaceData::LOD &lod = source.lods[i];
		if (lod.edge_length * p_model_scale / p_distance > p_threshold) {
			break;
		}
		const int64_t count = lod.index_data.size() / index_stride;
		if (count >= minimum && count <= INT32_MAX && count % alignment == 0) {
			selected = uint32_t(i + 1);
		}
	}
	return selected;
}

bool FixedFunctionSceneRenderer::_request_geometry(const FFDrawPacket &p_packet) {
	const FixedFunctionSpatialMaterial::FFMaterialParameters &material = p_packet.material;
	const bool lit = !(material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED);
	const bool native_only = lit && (!_context.get_device().supports_lit_geometry() || (material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_VERTEX) || !_per_pixel_lighting);
	if (_shadow_pass || !_context.is_initialized() || !_retained_geometry_enabled ||
			!FixedFunctionMeshStorage::surface_get_triangle_count(p_packet.surface) || !p_packet.surface.identity ||
			material.is_fallback || !p_packet.deformed.is_empty() ||
			(native_only && !_uses_vertex_lit_geometry(p_packet)) ||
			material.albedo_uses_vertex_color || material.albedo_uses_vertex_alpha ||
			(!lit && (p_packet.flip_v || material.albedo.uv_scale != Vector2(1, 1) || material.albedo.uv_offset != Vector2())) ||
			material.albedo.uv_set < 0 || material.albedo.uv_set > 1) {
		return false;
	}
	FixedFunctionGeometryCache &cache = _context.get_geometry_cache();
	RID geometry;
	FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
	Vector3 anchor, extent;
	const int normal_uv_set = p_packet.normal_mapping ? material.normal.uv_set : -1;
	const FixedFunctionGeometryCache::FFAmbientOcclusion baked_occlusion(material.occlusion, p_packet.baked_occlusion_image_version);
	const FixedFunctionGeometryCache::FFAmbientOcclusion *baked = baked_occlusion.image_version ? &baked_occlusion : nullptr;
	const Error cached = cache.get(p_packet.surface.identity, p_packet.surface.version, material.albedo.uv_set, geometry, info, anchor, extent, lit, normal_uv_set, p_packet.occlusion_secondary_uv, baked);
	if (cached != ERR_DOES_NOT_EXIST) {
		return cached == OK || cached == ERR_SKIP;
	}
	anchor = p_packet.surface.source.aabb.position + p_packet.surface.source.aabb.size * real_t(0.5);
	if (native_only || baked) {
		FixedFunctionGraphicsDevice::FFPixelLighting lighting;
		if ((!baked && _prepare_pixel_lighting(p_packet, lighting)) || !_prepare_vertex_lighting(p_packet, anchor, lighting)) {
			return false; // Check the current lights before building geometry used only by the native path.
		}
	}
	FFDrawPacket upload = p_packet;
	// Keep large model origins in real_t. Only the relative vertices and the final
	// camera-relative matrix cross the native float boundary.
	upload.view_from_model = Transform3D(Basis(), -anchor);
	upload.material.albedo.factor = Color(1, 1, 1, 1);
	upload.instance_opacity = 1;
	if (lit) {
		upload.material.albedo.uv_scale = Vector2(1, 1);
		upload.material.albedo.uv_offset = Vector2();
		upload.flip_v = false;
		if (p_packet.normal_mapping) {
			// Keep both raw UV sets. The normal UV is also used to generate tangents.
			upload.material.albedo.uv_set = 1 - normal_uv_set;
		}
	}
	Vector<FixedFunctionVertex> vertices;
	Vector<FixedFunctionLightingVertex> lighting_vertices;
	Vector<FixedFunctionNormalMapVertex> normal_vertices;
	Vector<uint32_t> indices;
	bool missing_normals = false;
	size_t vertex_stride = sizeof(FixedFunctionVertex);
	if (p_packet.normal_mapping) {
		vertex_stride = sizeof(FixedFunctionNormalMapVertex);
	} else if (lit) {
		vertex_stride = sizeof(FixedFunctionLightingVertex);
	}
	const bool expand = p_packet.surface.source.primitive == RSE::PRIMITIVE_TRIANGLE_STRIP;
	const uint32_t triangle_count = FixedFunctionMeshStorage::surface_get_triangle_count(p_packet.surface);
	const uint64_t index_count = expand ? uint64_t(triangle_count) * 3 : p_packet.surface.source.index_count;
	const bool fits = _geometry_upload_fits(p_packet.surface.source.vertex_count, vertex_stride) && _geometry_upload_fits(index_count, sizeof(uint32_t));
	Error error = fits ? OK : ERR_OUT_OF_MEMORY;
	Vector<uint8_t> occlusion_rgba;
	FFOcclusionSample occlusion_sample{};
	if (error == OK && baked) {
		if (_context.get_texture_storage().texture_get_cpu_rgba8(material.occlusion.texture, occlusion_sample.size, occlusion_rgba)) {
			occlusion_sample.rgba = occlusion_rgba.ptr();
			occlusion_sample.sampler_flags = material.occlusion.sampler_flags;
			occlusion_sample.single_channel = _get_occlusion_sample_channel(material.occlusion.weights);
		} else {
			error = ERR_UNAVAILABLE;
		}
	}
	if (error == OK) {
		if (p_packet.normal_mapping) {
			error = normal_vertices.resize(p_packet.surface.source.vertex_count);
		} else if (lit) {
			error = lighting_vertices.resize(p_packet.surface.source.vertex_count);
		} else {
			error = vertices.resize(p_packet.surface.source.vertex_count);
		}
	}
	if (error == OK) {
		error = indices.resize(index_count);
	}
	if (error == OK) {
		const uint8_t *source = p_packet.surface.source.vertex_data.ptr();
		FixedFunctionVertex *output = vertices.ptrw();
		FixedFunctionLightingVertex *lighting_output = lighting_vertices.ptrw();
		FixedFunctionNormalMapVertex *normal_output = normal_vertices.ptrw();
		const bool has_tangent = p_packet.surface.source.format & RSE::ARRAY_FORMAT_TANGENT;
		for (uint32_t i = 0; i < p_packet.surface.source.vertex_count; i++) {
			FixedFunctionVertex packed;
			error = _vertex(upload, source, i, packed);
			if (error != OK) {
				break;
			}
			if (lit) {
				Vector4 tangent;
				const Vector3 normal = _unit_normal(FixedFunctionMeshStorage::surface_get_normal(p_packet.surface, source, i, normal_output && has_tangent ? &tangent : nullptr));
				missing_normals |= normal == Vector3();
				FixedFunctionLightingVertex &vertex = normal_output ? normal_output[i].lighting : lighting_output[i];
				vertex.x = packed.x;
				vertex.y = packed.y;
				vertex.z = packed.z;
				vertex.u = packed.u;
				vertex.v = packed.v;
				vertex.nx = float(normal.x);
				vertex.ny = float(normal.y);
				vertex.nz = float(normal.z);
				vertex.r = vertex.g = vertex.b = vertex.a = 1;
				if (baked) {
					const Vector2 uv = FixedFunctionMeshStorage::surface_read_uv(p_packet.surface, i, material.occlusion.uv_set);
					vertex.r = vertex.g = vertex.b = _sample_occlusion(occlusion_sample, material.occlusion, uv);
				}
				if (p_packet.occlusion_secondary_uv) {
					// Retained scene geometry uses uniform color, leaving RG free for the second UV.
					const Vector2 uv = FixedFunctionMeshStorage::surface_read_uv(p_packet.surface, i, material.occlusion.uv_set);
					if (!(Math::abs(uv.x) <= real_t(1.0e10) && Math::abs(uv.y) <= real_t(1.0e10))) {
						error = ERR_UNAVAILABLE;
						break;
					}
					vertex.r = float(uv.x);
					vertex.g = float(uv.y);
				}
				if (normal_output) {
					const Vector2 uv = FixedFunctionMeshStorage::surface_read_uv(p_packet.surface, i, normal_uv_set);
					error = _pack_normal_geometry_vertex(tangent, uv, has_tangent, normal_output[i]);
					if (error != OK) {
						break;
					}
				}
			} else {
				output[i] = packed;
			}
			extent.x = MAX(extent.x, Math::abs(real_t(packed.x)));
			extent.y = MAX(extent.y, Math::abs(real_t(packed.y)));
			extent.z = MAX(extent.z, Math::abs(real_t(packed.z)));
		}
	}
	if (error == OK && indices.size()) {
		const uint8_t *source = p_packet.surface.source.index_data.ptr();
		uint32_t *output = indices.ptrw();
		if (expand) {
			FixedFunctionMeshStorage::surface_expand_triangle_strip(p_packet.surface, source, 0, triangle_count, output);
		} else {
			for (int i = 0; i < indices.size(); i++) {
				output[i] = FixedFunctionMeshStorage::surface_read_index(p_packet.surface, source, i);
			}
		}
	}
	if (error == OK && missing_normals && !p_packet.normal_mapping) {
		error = _generate_geometry_normals(upload, lighting_vertices, indices);
	}
	if (error == OK && p_packet.normal_mapping && !(p_packet.surface.source.format & RSE::ARRAY_FORMAT_TANGENT)) {
		error = _generate_geometry_tangents(normal_vertices, indices);
	}
	Error requested;
	if (p_packet.normal_mapping) {
		// Generated tangents and flat normals can duplicate or renumber vertices.
		requested = cache.request(p_packet.surface, material.albedo.uv_set, material.normal.uv_set, normal_vertices, indices, anchor, extent, error, p_packet.surface.source.format & RSE::ARRAY_FORMAT_TANGENT);
	} else if (lit) {
		requested = cache.request(p_packet.surface, material.albedo.uv_set, lighting_vertices, indices, anchor, extent, error, p_packet.occlusion_secondary_uv, !missing_normals, baked);
	} else {
		requested = cache.request(p_packet.surface, material.albedo.uv_set, vertices, indices, anchor, extent, error);
	}
	return requested == OK && error == OK;
}

bool FixedFunctionSceneRenderer::_geometry_projection(const FFDrawPacket &p_packet, const Vector3 &p_anchor, const Vector3 &p_extent, const Projection &p_projection, const Size2i &p_target, Projection &r_projection) {
	if (p_target.x <= 0 || p_target.y <= 0) {
		return false;
	}
	Transform3D view_from_anchor = p_packet.view_from_model;
	view_from_anchor.origin = view_from_anchor.xform(p_anchor);
	const Projection projection = p_projection * Projection(view_from_anchor);
	// Estimate coefficient/vertex packing and four-term float dot-product roundoff.
	// Restrict error to 1/8 target pixel and 1/65536 in normalized depth.
	const double FLOAT_ROUNDOFF = double(FLT_EPSILON) * 0.5;
	const double DOT_ROUNDOFF = (4 * FLOAT_ROUNDOFF) / (1 - 4 * FLOAT_ROUNDOFF);
	double errors[4] = {};
	double magnitudes[4] = {};
	for (int row = 0; row < 4; row++) {
		double dot_magnitude = 0;
		for (int column = 0; column < 4; column++) {
			const double value = projection.columns[column][row];
			if (!(Math::abs(value) <= FLT_MAX)) {
				return false;
			}
			const double packed = float(value);
			const double extent = column < 3 ? double(p_extent[column]) : 1;
			const double vertex_error = column < 3 ? MAX(FLOAT_ROUNDOFF * extent, double(FLT_MIN)) : 0;
			errors[row] += Math::abs(value - packed) * extent + Math::abs(packed) * vertex_error;
			magnitudes[row] += Math::abs(value) * extent;
			dot_magnitude += Math::abs(packed) * (extent + vertex_error);
		}
		// The error estimate assumes a finite homogeneous transform.
		if (!(dot_magnitude <= FLT_MAX / (1 + DOT_ROUNDOFF))) {
			return false;
		}
		errors[row] += DOT_ROUNDOFF * dot_magnitude;
	}
	double minimum_w = projection.columns[3][3];
	double maximum_z_minus_w = double(projection.columns[3][2]) - projection.columns[3][3];
	for (int column = 0; column < 3; column++) {
		const double w = projection.columns[column][3];
		minimum_w -= Math::abs(w) * p_extent[column];
		maximum_z_minus_w += Math::abs(double(projection.columns[column][2]) - w) * p_extent[column];
	}
	// A visible point satisfies z + w = (z - w) + 2w >= 0. Bound w using
	// the near plane even when the whole mesh bounds reach behind the camera.
	// Allow roundoff on both clip coordinates so either path can be visible.
	minimum_w = MAX(minimum_w, -0.5 * (maximum_z_minus_w + errors[2] + errors[3]));
	if (!(minimum_w > errors[3])) {
		return false;
	}
	for (int row = 0; row < 3; row++) {
		double clip_extent = 1.0;
		double limit = 1.0 / 65536;
		if (row < 2) {
			// Reserve a native half pixel even if the logical target is stretched
			// from a native target as small as one pixel on this axis.
			clip_extent = 2.0;
			limit = 0.25 / p_target[row];
		}
		// Use the tighter of the box and clip bounds for the visible coordinate.
		const double ndc_magnitude = MIN(magnitudes[row] / minimum_w, clip_extent);
		const double ndc_error = (errors[row] + ndc_magnitude * errors[3]) / (minimum_w - errors[3]);
		if (!(ndc_error <= limit)) {
			return false;
		}
	}
	r_projection = projection;
	return true;
}

Error FixedFunctionSceneRenderer::_submit_geometry(const FFDrawPacket &p_packet, const Projection &p_projection, RID p_texture, RenderingServerTypes::RenderInfo *r_info) {
	const bool lit = !(p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED);
	RID geometry;
	FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
	Vector3 anchor, extent;
	const int normal_uv_set = p_packet.normal_mapping ? p_packet.material.normal.uv_set : -1;
	const FixedFunctionGeometryCache::FFAmbientOcclusion baked_occlusion(p_packet.material.occlusion, p_packet.baked_occlusion_image_version);
	const FixedFunctionGeometryCache::FFAmbientOcclusion *baked = baked_occlusion.image_version ? &baked_occlusion : nullptr;
	// Each merged MultiMesh holds just the selected level, in its first index range.
	const bool multimesh = p_packet.multimesh.is_valid();
	const uint32_t multimesh_lod = multimesh ? p_packet.lod : 0;
	const uint32_t lod = multimesh ? 0 : p_packet.lod;
	if (_context.get_geometry_cache().get(p_packet.surface.identity, p_packet.surface.version, p_packet.material.albedo.uv_set, geometry, info, anchor, extent, lit, normal_uv_set, p_packet.occlusion_secondary_uv, baked, p_packet.multimesh, multimesh_lod) != OK) {
		return ERR_UNAVAILABLE;
	}
	if (lod > info.lod_count) {
		return ERR_UNAVAILABLE;
	}
	FixedFunctionGraphicsDevice &device = _context.get_device();
	Projection projection;
	if (!_geometry_projection(p_packet, anchor, extent, p_projection, Size2i(device.get_target_width(), device.get_target_height()), projection)) {
		return ERR_UNAVAILABLE;
	}
	const Color color = _apply_vertex_alpha_state(p_packet, p_packet.material.albedo.factor);
	// Retained vertices are anchor-relative model coordinates, not view positions.
	const Transform3D view_from_anchor(p_packet.view_from_model.basis, p_packet.view_from_model.xform(anchor));
	const FixedFunctionGraphicsDevice::FFFogTransforms fog_transforms = { p_projection, view_from_anchor };
	FixedFunctionGraphicsDevice::FFDrawParameters state = p_packet.material.state;
	if (state.fog) {
		state.fog_transforms = &fog_transforms;
	}
	Error error;
	if (lit) {
		FixedFunctionGraphicsDevice::FFPixelLighting lighting;
		if (baked || !_prepare_pixel_lighting(p_packet, lighting)) {
			if (p_packet.multimesh_local_lighting || !_prepare_vertex_lighting(p_packet, anchor, lighting)) {
				return ERR_UNAVAILABLE;
			}
			Vector2 uv_scale = p_packet.material.albedo.uv_scale;
			Vector2 uv_offset = p_packet.material.albedo.uv_offset;
			if (p_packet.flip_v) {
				uv_scale.y = -uv_scale.y;
				uv_offset.y = 1 - uv_offset.y;
			}
			error = device.draw_vertex_lit_geometry(geometry, projection, lighting, color, uv_scale, uv_offset, p_texture, state, lod);
			if (error == OK) {
				bool first_surface = true;
				_note_draw(p_packet, lod ? p_packet.surface.source.index_count : info.vertex_count, first_surface, r_info, info.draw_calls);
			}
			return error;
		}
		FixedFunctionGraphicsDevice::FFLitGeometryParameters parameters;
		parameters.view_from_anchor = p_packet.view_from_model;
		parameters.view_from_anchor.origin = p_packet.view_from_model.xform(anchor);
		parameters.normal_from_model = p_packet.normal_from_model;
		parameters.color = color;
		parameters.albedo_uv_scale = p_packet.material.albedo.uv_scale;
		parameters.albedo_uv_offset = p_packet.material.albedo.uv_offset;
		if (p_packet.flip_v) {
			parameters.albedo_uv_scale.y = -parameters.albedo_uv_scale.y;
			parameters.albedo_uv_offset.y = 1 - parameters.albedo_uv_offset.y;
		}
		FixedFunctionGraphicsDevice::FFOcclusionMapParameters map;
		const FixedFunctionGraphicsDevice::FFOcclusionMapParameters *occlusion_map = nullptr;
		if (p_packet.occlusion) {
			const FixedFunctionSpatialMaterial::FFOcclusionChannel &occlusion = p_packet.material.occlusion;
			if (!_get_occlusion_texture(occlusion.texture, map.texture)) {
				return ERR_UNAVAILABLE;
			}
			map.uv_scale = occlusion.uv_scale;
			map.uv_offset = occlusion.uv_offset;
			map.weights = occlusion.weights;
			map.light_affect = occlusion.light_affect;
			map.sampler_flags = occlusion.sampler_flags;
			map.uses_secondary_uv = p_packet.normal_mapping ? occlusion.uv_set == normal_uv_set : p_packet.occlusion_secondary_uv;
			occlusion_map = &map;
		}
		if (p_packet.normal_mapping) {
			FixedFunctionGraphicsDevice::FFNormalMapParameters normal;
			const FixedFunctionSpatialMaterial::FFNormalMapChannel &channel = p_packet.material.normal;
			FixedFunctionTextureStorage &textures = _context.get_texture_storage();
			const RID target = textures.texture_get_render_target(channel.texture);
			error = textures.texture_get_or_upload_device_texture(channel.texture, normal.texture);
			if (error == ERR_SKIP || (target.is_valid() && (error == ERR_UNCONFIGURED || (error == OK && !textures.render_target_is_ready(target))))) {
				return ERR_SKIP;
			}
			if (error == ERR_UNCONFIGURED) {
				return ERR_UNAVAILABLE;
			}
			if (error != OK) {
				return error;
			}
			normal.uv_scale = channel.uv_scale;
			normal.uv_offset = channel.uv_offset;
			normal.strength = channel.strength;
			normal.sampler_flags = channel.sampler_flags;
			normal.flip_v = target.is_valid() && textures.render_target_get_flag(target, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP);
			normal.albedo_uses_normal_uv = p_packet.material.albedo.uv_set == channel.uv_set;
			error = device.draw_normal_mapped_geometry(geometry, projection, parameters, lighting, p_texture, state, normal, occlusion_map, lod);
		} else {
			error = device.draw_lit_geometry(geometry, projection, parameters, lighting, p_texture, state, occlusion_map, lod);
		}
	} else {
		error = device.draw_geometry(geometry, projection, p_texture, state, &color, lod);
	}
	if (error == OK) {
		bool first_surface = true;
		_note_draw(p_packet, lod ? p_packet.surface.source.index_count : info.vertex_count, first_surface, r_info, info.draw_calls);
	}
	return error;
}
