#include "scene_renderer.h"

#include "core/config/project_settings.h"
#include "core/math/math_funcs_binary.h"
#include "core/templates/sort_array.h"

#include <float.h>
#include <string.h>

#include <limits>

bool FixedFunctionSceneRenderer::FFDrawPacketSort::operator()(const FFDrawPacket &p_left, const FFDrawPacket &p_right) const {
	if (p_left.late != p_right.late) {
		return !p_left.late;
	}
	if (p_left.late) {
		if (p_left.priority != p_right.priority) {
			return p_left.priority < p_right.priority;
		}
		if (p_left.depth != p_right.depth) {
			return p_left.depth > p_right.depth;
		}
	}
	return p_left.submission_index < p_right.submission_index;
}

bool FixedFunctionSceneRenderer::_finite_transform(const Transform3D &p_transform) {
	for (int column = 0; column < 4; column++) {
		const Vector3 vector = column < 3 ? p_transform.basis.get_column(column) : p_transform.origin;
		for (int row = 0; row < 3; row++) {
			if (Math::is_nan(vector[row]) || Math::is_inf(vector[row])) {
				return false;
			}
		}
	}
	return true;
}

Error FixedFunctionSceneRenderer::_prepare(const Transform3D &p_camera, const Projection &p_projection, const PagedArray<RenderGeometryInstance *> &p_instances, const FFMeshLOD *p_lod, const RID *p_lights, int p_light_count) {
	const Transform3D p_view = p_camera.affine_inverse();
	Plane near_plane(-p_camera.basis.get_column(Vector3::AXIS_Z), p_camera.origin);
	near_plane.d += p_projection.get_z_near();
	const bool orthogonal = p_projection.is_orthogonal();
	const bool antialiased = GLOBAL_GET("rendering/quality/fixed_function/antialiased_lines");
	FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	FFMultimeshLights multimesh_lights;
	multimesh_lights.sources = Span<RID>(p_lights, p_light_count);
	for (uint64_t i = 0; i < p_instances.size(); i++) {
		FixedFunctionGeometryInstance *instance = static_cast<FixedFunctionGeometryInstance *>(p_instances[i]);
		if (instance && !instance->data->base.is_valid()) {
			continue;
		}
		if (!instance) {
			return ERR_INVALID_PARAMETER;
		}
		if (!Math::is_finite(instance->force_alpha)) {
			return ERR_INVALID_PARAMETER;
		}
		if (instance->force_alpha <= 0 || (_shadow_pass && instance->force_alpha < 1)) {
			continue;
		}
		instance->update_dependencies();
		if ((!_shadow_pass && instance->has_unsupported_lighting()) || instance->parent_fade_alpha != 1 || instance->fade_near || instance->fade_far || instance->shader_uniforms_offset >= 0) {
			return ERR_UNAVAILABLE;
		}
		float lod_distance = 0;
		const float lod_threshold = p_lod ? p_lod->screen_threshold : 0;
		if (lod_threshold > 0) {
			lod_distance = 1;
			if (!p_lod->orthogonal) {
				const AABB &bounds = instance->transformed_aabb;
				lod_distance = Vector3().max(bounds.position - p_lod->camera_position).max(p_lod->camera_position - bounds.get_end()).length();
			}
			lod_distance *= p_lod->distance_multiplier;
		}
		if (instance->data->base_type == RSE::INSTANCE_MESH) {
			if (!meshes.owns_mesh(instance->data->base)) {
				return ERR_INVALID_PARAMETER;
			}
			const Error error = _prepare_mesh(*instance, instance->data->base, i, instance->transform, p_camera, p_view, p_projection, near_plane, orthogonal, antialiased, Color(1, 1, 1, 1), lod_distance, lod_threshold);
			if (error != OK) {
				return error;
			}
		} else if (instance->data->base_type == RSE::INSTANCE_MULTIMESH) {
			const RID multimesh = instance->data->base;
			const RID mesh = meshes.multimesh_get_mesh(multimesh);
			if (!mesh.is_valid()) {
				continue;
			}
			const int visible = meshes.multimesh_get_visible_count(multimesh);
			if (!_shadow_pass && _retained_geometry_enabled && visible >= 8 && instance->force_alpha == 1 &&
					_context.get_device().supports_indexed_triangles() && meshes.mesh_get_surface_count(mesh) == 1 &&
					meshes.multimesh_get_geometry_version(multimesh)) {
				const bool sort_packets = _sort_packets;
				const bool lighting_required = _lighting_required;
				const Error error = _prepare_mesh(*instance, mesh, i, instance->transform, p_camera, p_view, p_projection, near_plane, orthogonal, antialiased, Color(1, 1, 1, 1), lod_distance, lod_threshold, multimesh, &multimesh_lights);
				if (error == OK) {
					continue;
				}
				_sort_packets = sort_packets;
				_lighting_required = lighting_required;
			}
			for (int copy = 0; copy < visible; copy++) {
				const Transform3D transform = meshes.multimesh_get_transform(multimesh, copy);
				// CPU particles collapse unused instances in the bulk upload.
				if (transform.basis == Basis(0, 0, 0, 0, 0, 0, 0, 0, 0)) {
					continue;
				}
				const Color color = meshes.multimesh_get_channel(multimesh, copy, false);
				const Error error = _prepare_mesh(*instance, mesh, i, instance->transform * transform, p_camera, p_view, p_projection, near_plane, orthogonal, antialiased, color, lod_distance, lod_threshold);
				if (error != OK) {
					return error;
				}
			}
		} else {
			return ERR_INVALID_PARAMETER;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_prepare_mesh(const FixedFunctionGeometryInstance &p_instance, RID p_mesh, uint64_t p_instance_index, const Transform3D &p_world, const Transform3D &p_camera, const Transform3D &p_view, const Projection &p_projection, const Plane &p_near_plane, bool p_orthogonal, bool p_antialiased, const Color &p_color, float p_lod_distance, float p_lod_threshold, RID p_multimesh, FFMultimeshLights *r_multimesh_lights) {
	FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	if (!_finite_transform(p_world)) {
		return ERR_INVALID_PARAMETER;
	}
	const Transform3D view_from_model = p_view * p_world;
	if (!_finite_transform(view_from_model)) {
		return ERR_INVALID_PARAMETER;
	}
	for (int surface = 0; surface < meshes.mesh_get_surface_count(p_mesh); surface++) {
		FFDrawPacket surface_packet = {};
		surface_packet.view_from_model = view_from_model;
		surface_packet.layer_mask = p_instance.layer_mask;
		surface_packet.instance_index = p_instance_index;
		surface_packet.depth = 0;
		surface_packet.priority = 0;
		surface_packet.line_width = 1;
		surface_packet.flip_v = false;
		surface_packet.antialiased = false;
		Error error = meshes.mesh_surface_get_data(p_mesh, surface, surface_packet.surface);
		if (error != OK) {
			return error;
		}
		surface_packet.draw_bounds = surface_packet.surface.source.aabb;
		const uint32_t lod = _select_mesh_lod(surface_packet.surface, p_instance.lod_model_scale * p_instance.lod_bias, p_lod_distance, p_lod_threshold);
		const bool has_mesh_instance = p_instance.data->base_type == RSE::INSTANCE_MESH && p_instance.mesh_instance.is_valid();
		bool pose_ready = false;
		bool normals_ready = false;
		bool frames_ready = false;
		const bool lines = surface_packet.surface.source.primitive == RSE::PRIMITIVE_LINES || surface_packet.surface.source.primitive == RSE::PRIMITIVE_LINE_STRIP;
		const bool points = surface_packet.surface.source.primitive == RSE::PRIMITIVE_POINTS;
		if (_shadow_pass && (points || lines)) {
			continue;
		}
		const uint32_t triangles = FixedFunctionMeshStorage::surface_get_triangle_count(surface_packet.surface);
		const uint32_t count = surface_packet.surface.source.index_count ? surface_packet.surface.source.index_count : surface_packet.surface.source.vertex_count;
		if ((surface_packet.surface.source.primitive == RSE::PRIMITIVE_TRIANGLES && count % 3) || (surface_packet.surface.source.primitive == RSE::PRIMITIVE_LINES && count % 2)) {
			return ERR_INVALID_PARAMETER;
		}
		if (!points && !lines && !triangles) {
			continue;
		}
		RID base_material = p_instance.data->material_override;
		if (!base_material.is_valid() && surface < p_instance.data->surface_materials.size()) {
			base_material = p_instance.data->surface_materials[surface];
		}
		if (!base_material.is_valid()) {
			base_material = meshes.mesh_surface_get_material(p_mesh, surface);
		}
		FixedFunctionMaterialStorage &materials = _context.get_material_storage();
		RID overlay = p_instance.data->material_overlay;
		RID next;
		bool base = true;
		for (RID material = base_material; base || material.is_valid(); material = next, base = false) {
			if (_shadow_pass && !base) {
				break; // Only the base material defines caster coverage, not overlays/next passes.
			}
			next = materials.owns_material(material) ? materials.material_get_next_pass(material) : RID();
			if (!next.is_valid()) {
				next = overlay;
				overlay = RID();
			}
			if (p_multimesh.is_valid() && next.is_valid()) {
				return ERR_UNAVAILABLE;
			}
			FFDrawPacket packet = surface_packet;
			packet.submission_index = _packets.size();
			packet.optional_material_pass = !base;
			if (base) {
				error = materials.material_get_spatial_draw_parameters(material, packet.material);
				if (error != OK) {
					return error;
				}
			} else if (materials.material_get_spatial_parameters(material, packet.material) != OK) {
				continue;
			}
			const uint8_t transform_flags = packet.material.transform_flags;
			if (_shadow_pass) {
				if (packet.material.state.blend_mode != FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED && !packet.material.state.alpha_cutoff_enabled) {
					continue; // Blended surfaces do not provide opaque shadow coverage.
				}
				packet.material.lighting_flags |= FixedFunctionSpatialMaterial::LIGHTING_UNSHADED;
				packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
				packet.material.state.depth_test_enabled = packet.material.state.depth_write_enabled = true;
				if (p_instance.data->cast_double_sided_shadows) {
					packet.material.state.cull_mode = FixedFunctionGraphicsDevice::CULL_DISABLED;
				} else if (_shadow_reverse_cull) {
					if (packet.material.state.cull_mode == FixedFunctionGraphicsDevice::CULL_BACK) {
						packet.material.state.cull_mode = FixedFunctionGraphicsDevice::CULL_FRONT;
					} else if (packet.material.state.cull_mode == FixedFunctionGraphicsDevice::CULL_FRONT) {
						packet.material.state.cull_mode = FixedFunctionGraphicsDevice::CULL_BACK;
					}
				}
			}
			if (transform_flags & FixedFunctionSpatialMaterial::TRANSFORM_BILLBOARD) {
				// A camera-aligned world basis becomes identity in view space.
				packet.view_from_model.basis = Basis();
				if (transform_flags & FixedFunctionSpatialMaterial::TRANSFORM_KEEP_SCALE) {
					for (int axis = 0; axis < 3; axis++) {
						packet.view_from_model.basis[axis][axis] = p_world.basis.get_column(axis).length();
					}
				}
			}
			if (transform_flags & FixedFunctionSpatialMaterial::TRANSFORM_FIXED_SIZE) {
				real_t scale = -packet.view_from_model.origin.z;
				if (p_orthogonal) {
					if (p_projection.columns[1][1] == 0) {
						return ERR_INVALID_PARAMETER;
					}
					scale = Math::abs(real_t(1) / p_projection.columns[1][1]);
				}
				packet.view_from_model.basis = packet.view_from_model.basis * scale;
			}
			if (transform_flags && !_finite_transform(packet.view_from_model)) {
				return ERR_INVALID_PARAMETER;
			}
			if (packet.material.albedo_uses_vertex_color) {
				packet.material.albedo.factor.r *= p_color.r;
				packet.material.albedo.factor.g *= p_color.g;
				packet.material.albedo.factor.b *= p_color.b;
			}
			if (packet.material.albedo_uses_vertex_alpha) {
				packet.material.albedo.factor.a *= p_color.a;
			}
			if (_debug_unshaded) {
				packet.material.lighting_flags |= FixedFunctionSpatialMaterial::LIGHTING_UNSHADED;
			}
			if (!(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED)) {
				_lighting_required = true;
				const Basis world_basis = transform_flags ? p_camera.basis * packet.view_from_model.basis : p_world.basis;
				packet.normal_from_model = _normal_matrix(world_basis);
			}
			if (lines && material.is_valid()) {
				packet.line_width = MAX(_context.get_material_storage().material_get_line_width(material), 1.0f);
			}
			if (material.is_valid()) {
				packet.priority = static_cast<int8_t>(_context.get_material_storage().material_get_render_priority(material));
			}
			const FixedFunctionGraphicsDevice::FFCapabilities &caps = _context.get_device().get_capabilities();
			const bool blend_mix = caps.blend_mix;
			if (packet.material.state.alpha_cutoff_enabled && !caps.alpha_cutoff) {
				if (_shadow_pass) {
					return ERR_UNAVAILABLE; // Unsupported coverage cannot become an opaque shadow.
				}
				if (!base) {
					continue; // An extra pass cannot preserve the base surface's cutout coverage.
				}
				// Keep cutout materials and editor gizmos visible on devices without alpha testing.
				packet.material.state.alpha_cutoff_enabled = false;
				if (blend_mix) {
					packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
					packet.material.state.depth_write_enabled = false;
				}
			}
			if (p_instance.force_alpha < 1 && blend_mix) {
				packet.instance_opacity = p_instance.force_alpha;
				if (packet.material.state.alpha_cutoff_enabled) {
					// The native test sees the modulated alpha. Scale the reference too to preserve the cutout.
					if (packet.material.state.alpha_cutoff <= 1) {
						packet.material.state.alpha_cutoff *= packet.instance_opacity;
					}
				} else if (packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED) {
					packet.material.state.texture_alpha_enabled = false;
					packet.material.albedo.factor.a = 1;
					packet.material.albedo_uses_vertex_alpha = false;
				}
				// Multiply has no source-alpha weight. Use the ordinary alpha pass for partial opacity.
				if (packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED || packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MUL) {
					packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
				}
				packet.material.state.depth_write_enabled = false;
			}
			const FixedFunctionGraphicsDevice::FFBlendMode blend = packet.material.state.blend_mode;
			packet.late = blend != FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED || !packet.material.state.depth_test_enabled;
			if ((blend == FixedFunctionGraphicsDevice::BLEND_MODE_ADD && !caps.blend_add) ||
					(blend == FixedFunctionGraphicsDevice::BLEND_MODE_MUL && !caps.blend_mul)) {
				packet.material.state.blend_mode = blend_mix ? FixedFunctionGraphicsDevice::BLEND_MODE_MIX : FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
			} else if (blend == FixedFunctionGraphicsDevice::BLEND_MODE_MIX && !blend_mix) {
				packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
			}
			packet.antialiased = lines && p_antialiased && blend_mix && !packet.material.state.alpha_cutoff_enabled;
			if (packet.antialiased) {
				packet.late = true;
				// Coverage must not accidentally enable an opaque texture's unused alpha.
				packet.material.state.texture_alpha_enabled &= packet.material.state.blend_mode != FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
				packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
				packet.material.state.depth_write_enabled = false;
			}
			if (packet.late) {
				_sort_packets = true;
				// Match the native GLES sorting pivot and camera-distance conventions.
				Vector3 center = p_world.origin;
				if (p_instance.use_aabb_center && p_instance.data->base_type == RSE::INSTANCE_MESH) {
					center = p_orthogonal ? p_instance.transformed_aabb.get_support(-p_near_plane.normal) : p_instance.transformed_aabb.get_center();
				}
				packet.depth = (p_orthogonal ? p_near_plane.distance_to(center) : p_camera.origin.distance_to(center)) - p_instance.sorting_offset;
				if (Math::is_nan(packet.depth) || Math::is_inf(packet.depth)) {
					return ERR_INVALID_PARAMETER;
				}
			}
			if (packet.material.is_fallback && packet.material.albedo.texture.is_valid() && !textures.owns_texture(packet.material.albedo.texture)) {
				packet.material.albedo.texture = RID();
			}
			if (packet.material.albedo.texture.is_valid()) {
				if (!base && !textures.owns_texture(packet.material.albedo.texture)) {
					continue;
				}
				const RID target = textures.texture_get_render_target(packet.material.albedo.texture);
				if (p_multimesh.is_valid() && target.is_valid()) {
					return ERR_UNAVAILABLE;
				}
				if (target.is_valid() && !caps.render_target_opaque) {
					if (!base) {
						continue;
					}
					packet.material.albedo.texture = RID();
				} else if (target.is_valid()) {
					if (target == _render_targets.get_current_render_target()) {
						if (!base) {
							continue;
						}
						return ERR_INVALID_PARAMETER;
					}
					error = textures.render_target_mark_used(target);
					if (error != OK) {
						return error;
					}
					if (textures.render_target_has_failed(target)) {
						if (!packet.material.is_fallback) {
							continue;
						}
						packet.material.albedo.texture = RID(); // Keep fallback geometry visible without failed image contents.
					}
					packet.flip_v = false; // Native top-left UVs match this branch's viewport textures.
				}
				if (packet.material.albedo.texture.is_valid()) {
					RID native_texture;
					error = textures.texture_get_device_texture(packet.material.albedo.texture, native_texture);
					if (error == ERR_SKIP || (target.is_valid() && (error == ERR_UNCONFIGURED || (error == OK && !textures.render_target_is_ready(target))))) {
						_redraw_requested = true;
						if (!base) {
							continue;
						}
						return _render_targets.defer_render_target();
					}
					if (!base && error == ERR_UNAVAILABLE) {
						continue;
					}
					if (error == ERR_UNCONFIGURED && !target.is_valid()) {
						if (!textures.texture_can_upload_on_use(packet.material.albedo.texture)) {
							if (!base || (_shadow_pass && packet.material.state.alpha_cutoff_enabled && packet.material.state.texture_alpha_enabled)) {
								continue;
							}
							packet.material.albedo.texture = RID(); // Keep the surface visible with its albedo color.
						}
						error = OK;
					}
					if (error != OK) {
						return error;
					}
					packet.material.state.sampler_flags = packet.material.albedo.sampler_flags;
					if (!_context.get_device().get_capabilities().sampler_filter_linear) {
						packet.material.state.sampler_flags &= ~FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
					}
				}
			}
			if (packet.view_from_model.basis.determinant() < 0) {
				if (packet.material.state.cull_mode == FixedFunctionGraphicsDevice::CULL_BACK) {
					packet.material.state.cull_mode = FixedFunctionGraphicsDevice::CULL_FRONT;
				} else if (packet.material.state.cull_mode == FixedFunctionGraphicsDevice::CULL_FRONT) {
					packet.material.state.cull_mode = FixedFunctionGraphicsDevice::CULL_BACK;
				}
			}
			const bool shaded = triangles && !(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED);
			const bool cutout = packet.material.state.alpha_cutoff_enabled;
			FixedFunctionSpatialMaterial::FFEmissionChannel &emission = packet.material.emission;
			const FixedFunctionGraphicsDevice::FFBlendMode emission_blend = packet.material.state.blend_mode;
			const bool weighted_emission = emission_blend == FixedFunctionGraphicsDevice::BLEND_MODE_MIX || emission_blend == FixedFunctionGraphicsDevice::BLEND_MODE_ADD;
			packet.emission = shaded && _emission_enabled && emission.energy > 0 &&
					(weighted_emission || (emission_blend == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED && packet.material.state.depth_write_enabled)) &&
					((cutout && !weighted_emission) ? caps.blend_add_unweighted : caps.blend_add);
			if (packet.emission && emission.color.texture.is_valid() && textures.owns_texture(emission.color.texture)) {
				const RID target = textures.texture_get_render_target(emission.color.texture);
				if (target.is_valid() && target != _render_targets.get_current_render_target() && !textures.render_target_has_failed(target)) {
					textures.render_target_mark_used(target);
				}
			}
			const FixedFunctionSpatialMaterial::FFOcclusionChannel &occlusion = packet.material.occlusion;
			const bool wants_occlusion = shaded && occlusion.texture.is_valid() && _texture_occlusion_enabled &&
					(occlusion.light_affect > 0 || !(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_AMBIENT_DISABLED));
			packet.occlusion = false;
			packet.normal_mapping = _prepare_normal_map(packet);
			const bool want_normals = !(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED);
			const bool want_frames = packet.normal_mapping && _context.get_device().supports_transient_normal_mapping() &&
					(packet.surface.source.format & RSE::ARRAY_FORMAT_TANGENT);
			if (has_mesh_instance && (!pose_ready || (want_normals && !normals_ready) || (want_frames && !frames_ready))) {
				// Later passes can request normals or tangent frames as needed.
				error = meshes.mesh_instance_deform(p_instance.mesh_instance, surface_packet.surface, surface_packet.deformed, surface_packet.draw_bounds,
						want_frames ? &surface_packet.deformed_tangents : nullptr, &surface_packet.deformed_position_bounds, !want_normals);
				if (error != OK) {
					return error;
				}
				pose_ready = true;
				normals_ready = want_normals;
				frames_ready = want_frames;
			}
			packet.deformed = surface_packet.deformed;
			packet.deformed_tangents = surface_packet.deformed_tangents;
			packet.draw_bounds = surface_packet.draw_bounds;
			packet.deformed_position_bounds = surface_packet.deformed_position_bounds;
			packet.occlusion_secondary_uv = false;
			const bool static_geometry = packet.deformed.is_empty();
			if (!static_geometry && packet.deformed_tangents.is_empty()) {
				packet.normal_mapping = false; // Animated meshes require authored frames, not per-frame MikkTSpace.
			}
			bool pixel_occlusion = false;
			if (wants_occlusion) {
				const FixedFunctionGraphicsDevice &device = _context.get_device();
				const bool combined = static_geometry ? device.supports_normal_mapped_occlusion() : device.supports_transient_normal_mapped_occlusion();
				const bool standalone = static_geometry ? device.supports_occlusion_mapping() : device.supports_transient_occlusion_mapping();
				RID native_occlusion;
				const bool pixel_lighting = _per_pixel_lighting && !(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_VERTEX);
				if (pixel_lighting && ((packet.normal_mapping && combined) || standalone) && _get_occlusion_texture(occlusion.texture, native_occlusion, true)) {
					packet.normal_mapping &= combined;
					pixel_occlusion = true;
				}
				if (pixel_occlusion) {
					packet.occlusion = true;
					packet.occlusion_secondary_uv = !packet.normal_mapping && occlusion.uv_set != packet.material.albedo.uv_set;
				} else {
					if (static_geometry && !_shadow_pass && _retained_geometry_enabled && _native_lighting_mode &&
							!packet.normal_mapping && occlusion.light_affect == 0 && device.supports_vertex_ambient_occlusion()) {
						packet.baked_occlusion_image_version = textures.texture_get_cpu_image_version(occlusion.texture, true);
					}
					if (packet.baked_occlusion_image_version) {
						packet.occlusion = true;
					} else {
						Size2i size;
						Vector<uint8_t> rgba;
						packet.occlusion = textures.texture_get_cpu_rgba8(occlusion.texture, size, rgba);
					}
				}
			}
			const bool retain = static_geometry && (!packet.occlusion || pixel_occlusion || packet.baked_occlusion_image_version);
			if (packet.occlusion && !pixel_occlusion) {
				packet.normal_mapping = false;
			}
			packet.retained_geometry = retain && !_shadow_pass;
			if (lod) {
				// Streamed draws and MultiMesh batches use the selected indices.
				// Single meshes retain the original surface to share vertices across LODs.
				RenderingServerTypes::SurfaceData &source = packet.surface.source;
				source.index_data = source.lods[lod - 1].index_data;
				source.index_count = uint32_t(source.index_data.size() / (source.vertex_count > 65536 ? 4 : 2));
				packet.lod = lod;
			}
			if (p_multimesh.is_valid()) {
				ERR_FAIL_NULL_V(r_multimesh_lights, ERR_INVALID_PARAMETER);
				packet.multimesh = p_multimesh;
				if (!_request_multimesh_geometry(packet, *r_multimesh_lights, p_world, p_view, p_camera)) {
					return ERR_UNAVAILABLE;
				}
			}
			if (_packets.push_back(packet)) {
				return ERR_OUT_OF_MEMORY;
			}
		}
	}
	return OK;
}

void FixedFunctionSceneRenderer::_read_vertex_coordinates(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, Vector3 &r_position, Vector2 &r_uv, Vector3 *r_model_position) {
	const FixedFunctionMeshStorage::FFSurfaceData &surface = p_packet.surface;
	const Vector3 model_position = p_packet.deformed.is_empty() ? FixedFunctionMeshStorage::surface_get_position(surface, p_data, p_index) : p_packet.deformed[p_index].position;
	if (r_model_position) {
		*r_model_position = model_position;
	}
	r_position = p_packet.view_from_model.xform(model_position);
	const int uv_set = p_packet.material.albedo.uv_set;
	// Point-sprite coordinates are filled per corner during expansion.
	r_uv = uv_set == FixedFunctionSpatialMaterial::UV_POINT_COORD ? Vector2() : FixedFunctionMeshStorage::surface_read_uv(surface, p_index, uv_set);
	r_uv = r_uv * p_packet.material.albedo.uv_scale + p_packet.material.albedo.uv_offset;
	if (p_packet.flip_v) {
		r_uv.y = 1 - r_uv.y;
	}
}

Color FixedFunctionSceneRenderer::_apply_vertex_alpha_state(const FFDrawPacket &p_packet, Color p_color) {
	if (!p_packet.material.state.alpha_cutoff_enabled && (p_packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED || !p_packet.material.state.texture_alpha_enabled)) {
		p_color.a = p_packet.instance_opacity;
	} else if (p_packet.instance_opacity != 1) {
		p_color.a = CLAMP(p_color.a, 0.0f, 1.0f) * p_packet.instance_opacity;
	}
	return p_color;
}

Error FixedFunctionSceneRenderer::_read_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, FFSpatialVertex &r_vertex, Vector3 *r_model_position) {
	const int uv_set = p_packet.material.albedo.uv_set;
	if (uv_set < 0 || uv_set > FixedFunctionSpatialMaterial::UV_POINT_COORD) {
		return ERR_UNAVAILABLE;
	}
	const FixedFunctionMeshStorage::FFSurfaceData &surface = p_packet.surface;
	Vector3 position;
	Vector2 uv;
	_read_vertex_coordinates(p_packet, p_data, p_index, position, uv, r_model_position);
	Color color = p_packet.material.albedo.factor;
	if ((p_packet.material.albedo_uses_vertex_color || p_packet.material.albedo_uses_vertex_alpha) && (surface.source.format & RSE::ARRAY_FORMAT_COLOR)) {
		const Color vertex_color = FixedFunctionMeshStorage::surface_read_color(surface, p_index);
		if (p_packet.material.albedo_uses_vertex_color) {
			color.r *= vertex_color.r;
			color.g *= vertex_color.g;
			color.b *= vertex_color.b;
		}
		if (p_packet.material.albedo_uses_vertex_alpha) {
			color.a *= vertex_color.a;
		}
	}
	if (p_packet.material.is_fallback) {
		for (int i = 0; i < 2; i++) {
			if (!(uv[i] >= -real_t(FLT_MAX) && uv[i] <= real_t(FLT_MAX))) {
				uv = Vector2();
				break;
			}
		}
		for (int i = 0; i < 4; i++) {
			if (!(color[i] >= -FLT_MAX && color[i] <= FLT_MAX)) {
				color = Color(1, 0, 1);
				break;
			}
		}
	}
	const real_t values[] = { position.x, position.y, position.z, uv.x, uv.y };
	const real_t real_limit = std::numeric_limits<real_t>::max();
	for (const real_t value : values) {
		if (!(value >= -real_limit && value <= real_limit)) {
			return ERR_INVALID_PARAMETER;
		}
	}
	for (int i = 0; i < 4; i++) {
		if (!(color[i] >= -FLT_MAX && color[i] <= FLT_MAX)) {
			return ERR_INVALID_PARAMETER;
		}
	}
	// The native LDR approximation clamps only this draw copy, never CPU source values.
	for (int i = 0; i < 4; i++) {
		if (i == 3 || (p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED)) {
			color[i] = CLAMP(color[i], 0.0f, 1.0f);
		}
	}
	r_vertex.position = position;
	r_vertex.uv = uv;
	r_vertex.color = _apply_vertex_alpha_state(p_packet, color);
	return OK;
}

Error FixedFunctionSceneRenderer::_pack_vertex_coordinates(const Vector3 &p_position, const Vector2 &p_uv, FixedFunctionVertex &r_vertex) {
	const real_t values[] = { p_position.x, p_position.y, p_position.z, p_uv.x, p_uv.y };
	for (const real_t value : values) {
		if (!(value >= -real_t(FLT_MAX) && value <= real_t(FLT_MAX))) {
			return ERR_INVALID_PARAMETER;
		}
	}
	r_vertex.x = float(p_position.x);
	r_vertex.y = float(p_position.y);
	r_vertex.z = float(p_position.z);
	r_vertex.u = float(p_uv.x);
	r_vertex.v = float(p_uv.y);
	return OK;
}

Error FixedFunctionSceneRenderer::_pack_vertex(const FFSpatialVertex &p_vertex, FixedFunctionVertex &r_vertex) {
	const Error error = _pack_vertex_coordinates(p_vertex.position, p_vertex.uv, r_vertex);
	return error == OK ? FixedFunctionGraphicsDevice::pack_geometry_color(p_vertex.color, r_vertex.color) : error;
}

Error FixedFunctionSceneRenderer::_vertex(const FFDrawPacket &p_packet, const uint8_t *p_data, uint32_t p_index, FixedFunctionVertex &r_vertex) {
	FFSpatialVertex vertex;
	const Error error = _read_vertex(p_packet, p_data, p_index, vertex);
	return error == OK ? _pack_vertex(vertex, r_vertex) : error;
}

Error FixedFunctionSceneRenderer::_pack_unshaded_batch(const FFDrawPacket &p_packet, const uint8_t *p_data, const uint8_t *p_indices, const uint32_t *p_expanded, uint32_t p_first_index, uint32_t p_count) {
	const int uv_set = p_packet.material.albedo.uv_set;
	if (uv_set < 0 || uv_set > FixedFunctionSpatialMaterial::UV_POINT_COORD) {
		return ERR_UNAVAILABLE;
	}
	const Color &albedo = p_packet.material.albedo.factor;
	const bool vertex_color = (p_packet.surface.source.format & RSE::ARRAY_FORMAT_COLOR) && (p_packet.material.albedo_uses_vertex_color || p_packet.material.albedo_uses_vertex_alpha);
	const bool constant_color = !p_packet.material.is_fallback && !vertex_color &&
			!Math::is_nan(albedo.a) && !Math::is_inf(albedo.a);
	uint32_t color = 0;
	if (constant_color) {
		const Error error = FixedFunctionGraphicsDevice::pack_geometry_color(_apply_vertex_alpha_state(p_packet, albedo), color);
		if (error != OK) {
			return error;
		}
	}
	FixedFunctionVertex *output = _vertices.ptrw();
	for (uint32_t v = 0; v < p_count; v++) {
		const uint32_t index = p_expanded ? p_expanded[v] : FixedFunctionMeshStorage::surface_read_index(p_packet.surface, p_indices, p_first_index + v);
		if (constant_color) {
			Vector3 position;
			Vector2 uv;
			_read_vertex_coordinates(p_packet, p_data, index, position, uv);
			const Error error = _pack_vertex_coordinates(position, uv, output[v]);
			if (error != OK) {
				return error;
			}
			output[v].color = color;
		} else {
			const Error error = _vertex(p_packet, p_data, index, output[v]);
			if (error != OK) {
				return error;
			}
		}
	}
	return OK;
}

void FixedFunctionSceneRenderer::_note_draw(const FFDrawPacket &p_packet, uint32_t p_count, bool &r_first_surface, RenderingServerTypes::RenderInfo *r_info, uint32_t p_draw_calls) {
	if (r_info) {
		int *info = r_info->info[_shadow_pass ? RSE::VIEWPORT_RENDER_INFO_TYPE_SHADOW : RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE];
		info[RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] += !_instances_drawn[p_packet.instance_index];
		info[RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] = int(MIN(uint64_t(INT32_MAX), uint64_t(info[RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME]) + p_count / 3));
		info[RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] = int(MIN(uint64_t(INT32_MAX), uint64_t(info[RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]) + p_draw_calls));
	}
	_instances_drawn.write[p_packet.instance_index] = 1;
	r_first_surface = false;
}

Error FixedFunctionSceneRenderer::_submit_batch(const FFDrawPacket &p_packet, const Projection &p_projection, RID p_texture, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, uint32_t p_count, bool &r_first_surface, RenderingServerTypes::RenderInfo *r_info, uint32_t p_indexed_vertices) {
	FixedFunctionGraphicsDevice &device = _context.get_device();
	Error error;
	if (p_indexed_vertices) {
		error = device.draw_indexed_triangles(_vertices.ptr(), p_indexed_vertices, reinterpret_cast<const uint8_t *>(_batch_draw_indices.ptr()), p_count, p_projection, p_texture, p_state);
	} else if (_shadow_pass) {
		error = device.draw_shadow_triangles(_vertices.ptr(), p_count, p_projection, p_texture, p_state);
	} else {
		error = device.draw_triangles(_vertices.ptr(), p_count, p_projection, p_texture, p_state);
	}
	if (error == OK) {
		_note_draw(p_packet, p_count, r_first_surface, r_info);
	}
	return error;
}

bool FixedFunctionSceneRenderer::_prepare_depth_fog(const Projection &p_projection, const FFLightingEnvironment &p_environment) {
	if (!(p_environment.fog_end > p_environment.fog_begin)) {
		return false;
	}
	_fog.color = p_environment.fog_color;
	_fog.begin = p_environment.fog_begin;
	_fog.end = p_environment.fog_end;
	_fog.orthogonal = p_projection.is_orthogonal();
	// Keep the original near/far mapping for native Z table fog. Clip perspective
	// distances in front of the near plane before dividing by W.
	const real_t near_depth = p_projection.get_z_near();
	const real_t begin = _fog.orthogonal ? real_t(_fog.begin) : MAX(real_t(_fog.begin), near_depth);
	const real_t end = _fog.orthogonal ? real_t(_fog.end) : MAX(real_t(_fog.end), near_depth);
	const real_t z = p_projection.columns[2][2];
	const real_t offset = p_projection.columns[3][2];
	const real_t begin_w = _fog.orthogonal ? 1 : begin;
	const real_t end_w = _fog.orthogonal ? 1 : end;
	_fog.projected_begin = float(((-begin * z + offset) / begin_w + 1) * real_t(0.5));
	_fog.projected_end = float(((-end * z + offset) / end_w + 1) * real_t(0.5));
	return true;
}

Error FixedFunctionSceneRenderer::_submit(const Projection &p_projection, RenderingServerTypes::RenderInfo *r_info, bool p_use_fog) {
	if (_packets.is_empty()) {
		return OK;
	}
	FixedFunctionGraphicsDevice &device = _context.get_device();
	const uint32_t batch_primitives = MIN(device.get_capabilities().max_primitives_per_draw, uint32_t(MAX_BATCH_PRIMITIVES));
	if (!batch_primitives) {
		return ERR_UNAVAILABLE;
	}
	Error error = _vertices.resize(batch_primitives * 3);
	if (error != OK) {
		return error;
	}
	FFDrawPacket *packets = _packets.ptrw();
	const int64_t packet_count = _packets.size();
	const uint32_t *order = nullptr;
	if (!_shadow_pass && _sort_packets && packet_count > 1) {
		// Order indices instead of moving the reference-counted arrays in each packet.
		error = _packet_order.resize(packet_count);
		if (error != OK) {
			return error;
		}
		uint32_t *indices = _packet_order.ptrw();
		for (int64_t i = 0; i < packet_count; i++) {
			indices[i] = uint32_t(i);
		}
		SortArray<uint32_t, FFDrawPacketOrder> sorter{ { packets } };
		sorter.sort(indices, packet_count);
		order = indices;
	}
	error = _render_targets.begin_draw(true);
	if (error != OK) {
		return error;
	}
	error = device.clear_depth();
	if (error != OK) {
		return error;
	}
	for (int64_t i = 0; i < packet_count; i++) {
		FFDrawPacket &packet = packets[order ? order[i] : i];
		if (!(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED)) {
			_filter_local_lights(packet);
		}
		// Native begin may recover/reset resources. Resolve again after that boundary.
		RID native_texture;
		if (packet.material.albedo.texture.is_valid()) {
			FixedFunctionTextureStorage &textures = _context.get_texture_storage();
			if (packet.optional_material_pass && !textures.owns_texture(packet.material.albedo.texture)) {
				continue;
			}
			const RID target = textures.texture_get_render_target(packet.material.albedo.texture);
			error = textures.texture_get_or_upload_device_texture(packet.material.albedo.texture, native_texture);
			if (error == ERR_SKIP || (target.is_valid() && (error == ERR_UNCONFIGURED || (error == OK && !textures.render_target_is_ready(target))))) {
				_redraw_requested = true;
				if (packet.optional_material_pass) {
					continue;
				}
				return _render_targets.defer_render_target();
			}
			if (error == ERR_UNCONFIGURED && !target.is_valid()) {
				if (packet.optional_material_pass || (_shadow_pass && packet.material.state.alpha_cutoff_enabled && packet.material.state.texture_alpha_enabled)) {
					continue;
				}
				error = OK; // An ordinary texture without backing leaves the albedo factor drawable.
			}
			if (error != OK) {
				if (packet.optional_material_pass && error == ERR_UNAVAILABLE) {
					continue;
				}
				return error;
			}
		}
		if (!FixedFunctionMeshStorage::surface_get_triangle_count(packet.surface)) {
			if (packet.surface.source.primitive == RSE::PRIMITIVE_POINTS) {
				error = _submit_points(packet, p_projection, native_texture, r_info);
			} else {
				error = _submit_lines(packet, p_projection, native_texture, r_info);
			}
			if (error != OK) {
				return error;
			}
			continue;
		}
		if (p_use_fog && !(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_FOG_DISABLED)) {
			packet.material.state.fog = &_fog;
		}
		if (!_shadow_pass && packet.retained_geometry) {
			error = _submit_geometry(packet, p_projection, native_texture, r_info);
			if (error == OK) {
				if (packet.emission) {
					error = _submit_emission(packet, p_projection, native_texture, batch_primitives, r_info, true);
					if (error != OK) {
						return error;
					}
				}
				continue;
			}
			if (error == ERR_SKIP) {
				_redraw_requested = true;
				if (packet.optional_material_pass) {
					continue;
				}
				return _render_targets.defer_render_target();
			}
			if (error != ERR_UNAVAILABLE) {
				return error;
			}
		}
		if (packet.multimesh.is_valid()) {
			error = _submit_multimesh_copies(packet, p_projection, native_texture, batch_primitives, r_info);
		} else {
			error = _submit_triangles(packet, p_projection, native_texture, batch_primitives, true, r_info);
		}
		if (error == OK && packet.emission) {
			error = _submit_emission(packet, p_projection, native_texture, batch_primitives, r_info);
		}
		if (error != OK) {
			return error;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::render(const Transform3D &p_camera, const Projection &p_projection, const PagedArray<RenderGeometryInstance *> &p_instances, RenderingServerTypes::RenderInfo *r_info, const FFLightingEnvironment &p_environment, const RID *p_lights, int p_light_count, const FFMeshLOD *p_lod) {
	_shadow_parameters.texture = RID();
	if (_shadow_ready && _shadow_frame == _context.get_frame_number()) {
		const Basis inverse = _shadow_transform.transform.basis.inverse();
		const Transform3D relative(inverse, inverse.xform(p_camera.origin - _shadow_transform.transform.origin));
		_shadow_parameters.clip_from_world = _shadow_transform.projection * Projection(relative);
		FixedFunctionTextureStorage &textures = _context.get_texture_storage();
		const RID texture = textures.render_target_get_texture(_shadow_target);
		if (textures.render_target_is_ready(_shadow_target)) {
			textures.texture_get_device_texture(texture, _shadow_parameters.texture);
		}
	}
	_redraw_requested = false;
	_sort_packets = false;
	_lighting_required = false;
	_native_lighting_mode = int(GLOBAL_GET("rendering/quality/fixed_function/native_lighting"));
	_per_pixel_lighting = GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting");
	_emission_enabled = GLOBAL_GET("rendering/quality/fixed_function/emission");
	_texture_occlusion_enabled = GLOBAL_GET("rendering/quality/fixed_function/texture_occlusion");
	_normal_mapping_enabled = GLOBAL_GET("rendering/quality/fixed_function/normal_mapping");
	_retained_geometry_enabled = GLOBAL_GET("rendering/quality/fixed_function/retained_geometry");
	_directional_lights.clear();
	_local_lights.clear();
	for (int channel = 0; channel < 3; channel++) {
		_ambient[channel] = 0;
	}
	_packets.clear();
	if (p_light_count < 0 || (p_light_count && !p_lights)) {
		return ERR_INVALID_PARAMETER;
	}
	if (!_finite_transform(p_camera) || p_camera.basis.determinant() == 0) {
		return ERR_INVALID_PARAMETER;
	}
	for (int column = 0; column < 4; column++) {
		for (int row = 0; row < 4; row++) {
			const real_t value = p_projection.columns[column][row];
			if (Math::is_nan(value) || Math::is_inf(value) || value < -FLT_MAX || value > FLT_MAX) {
				return ERR_INVALID_PARAMETER;
			}
		}
	}
	Error error = _instances_drawn.resize(p_instances.size());
	if (error == OK && p_instances.size()) {
		memset(_instances_drawn.ptrw(), 0, p_instances.size());
	}
	if (error == OK) {
		error = _prepare(p_camera, p_projection, p_instances, p_lod, p_lights, p_light_count);
	}
	if (error == OK && _lighting_required) {
		error = _prepare_lighting(p_camera, p_environment, p_lights, p_light_count);
	}
	if (error == OK) {
		for (int i = 0; i < _packets.size(); i++) {
			FFDrawPacket &packet = _packets.write[i];
			if (packet.retained_geometry && !packet.multimesh.is_valid()) {
				packet.retained_geometry = _request_geometry(packet);
			}
		}
		error = _submit(p_projection, r_info, !_packets.is_empty() && _prepare_depth_fog(p_projection, p_environment));
	}
	if (error != OK) {
		_render_targets.report_draw_error(error);
	}
	_packets.clear();
	return error;
}

FixedFunctionSceneRenderer::FixedFunctionSceneRenderer(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets) :
		_context(p_context), _render_targets(p_render_targets), _shadow_frame(0), _redraw_requested(false), _sort_packets(false), _lighting_required(false), _shadow_pass(false), _shadow_reverse_cull(false), _shadow_ready(false), _debug_unshaded(false) {
	GLOBAL_DEF("rendering/quality/fixed_function/antialiased_lines", true);
	GLOBAL_DEF("rendering/quality/fixed_function/per_pixel_lighting", true);
	GLOBAL_DEF("rendering/quality/fixed_function/normal_mapping", true);
	GLOBAL_DEF("rendering/quality/fixed_function/emission", true);
	GLOBAL_DEF("rendering/quality/fixed_function/texture_occlusion", true);
	GLOBAL_DEF("rendering/quality/fixed_function/retained_geometry", true);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "rendering/quality/fixed_function/native_lighting", PROPERTY_HINT_ENUM, "Disabled,Exact (Directional and Ambient),All Lights (Approximate Falloff)"), 1);
	GLOBAL_DEF("rendering/quality/fixed_function/directional_shadows", true);
	GLOBAL_DEF("rendering/lights_and_shadows/use_physical_light_units", false);
}

Error FixedFunctionSceneRenderer::_submit_triangles(const FFDrawPacket &p_packet, const Projection &p_projection, RID p_texture, uint32_t p_batch_primitives, bool p_first_surface, RenderingServerTypes::RenderInfo *r_info) {
	FixedFunctionGraphicsDevice &device = _context.get_device();
	Error error = OK;
	const uint8_t *vertices = p_packet.surface.source.vertex_data.ptr();
	const uint8_t *indices = p_packet.surface.source.index_data.ptr();
	const uint32_t count = FixedFunctionMeshStorage::surface_get_triangle_count(p_packet.surface);
	Vector<uint32_t> expanded;
	uint32_t *batch_indices = nullptr;
	if (p_packet.surface.source.primitive == RSE::PRIMITIVE_TRIANGLE_STRIP) {
		error = expanded.resize(MIN(count, p_batch_primitives) * 3);
		if (error != OK) {
			return error;
		}
		batch_indices = expanded.ptrw();
	}
	FixedFunctionGraphicsDevice::FFPixelLighting lighting;
	Vector<uint8_t> occlusion_rgba;
	FFOcclusionSample occlusion;
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	const RID occlusion_texture = p_packet.material.occlusion.texture;
	const bool pixel_lighting = (!p_packet.occlusion || p_packet.normal_mapping || device.supports_transient_occlusion_mapping()) && _prepare_pixel_lighting(p_packet, lighting);
	bool per_pixel = false;
	bool vertex_lighting_ready = false;
	bool per_vertex = false;
	Basis view_normal_from_model;
	bool use_occlusion = false;
	bool fallback_ready = false;
	FixedFunctionGraphicsDevice::FFNormalMapParameters normal_map;
	FixedFunctionGraphicsDevice::FFOcclusionMapParameters occlusion_map;
	bool normal_occlusion = false;
	bool normal_mapping = pixel_lighting && p_packet.normal_mapping && _prepare_transient_normal_map(p_packet, normal_map, occlusion_map, normal_occlusion);
	if (normal_mapping && _normal_vertices.resize(p_batch_primitives * 3) != OK) {
		normal_mapping = false;
	}
	Basis world_from_model;
	if (normal_mapping) {
		world_from_model = _camera_basis * p_packet.view_from_model.basis;
	}
	bool occlusion_mapping = pixel_lighting && p_packet.occlusion && device.supports_transient_occlusion_mapping();
	bool occlusion_ready = false;
	bool initialize_lit_cache = count > 1 && p_packet.surface.source.vertex_count > 0 && (p_packet.surface.source.index_count || batch_indices) && (p_packet.surface.source.format & RSE::ARRAY_FORMAT_NORMAL);
	// Dense staging shades every source vertex, so reserve it for meshes with
	// enough index reuse. Larger source meshes keep the bounded corner cache.
	const bool indexed_lighting = initialize_lit_cache && !batch_indices && !_shadow_pass && device.supports_indexed_triangles() &&
			p_packet.surface.source.vertex_count <= MAX_INDEXED_LIT_VERTICES &&
			p_packet.surface.source.index_count >= MAX(96u, p_packet.surface.source.vertex_count * 2);
	FFLitVertexCacheEntry *lit_cache = nullptr;
	uint32_t lit_cache_mask = 0;
	bool first_surface = p_first_surface;
	const bool unshaded = p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED;
	const bool authored_normals = p_packet.surface.source.format & RSE::ARRAY_FORMAT_NORMAL;
	// Indexed lighting uploads vertices and indices. Require roughly 25% fewer
	// vertices than corners, first by surface and then by batch.
	const uint64_t corners = uint64_t(count) * 3;
	const bool reuse_lighting_vertices = authored_normals && p_packet.surface.source.index_count && uint64_t(p_packet.surface.source.vertex_count) <= corners - corners / 4;
	for (uint32_t offset = 0; offset < count;) {
		const uint32_t batch_triangles = MIN(count - offset, p_batch_primitives);
		const uint32_t amount = batch_triangles * 3;
		const uint32_t first_index = batch_indices ? 0 : offset * 3;
		if (batch_indices) {
			FixedFunctionMeshStorage::surface_expand_triangle_strip(p_packet.surface, indices, offset, batch_triangles, batch_indices);
		}
		bool remapped = false;
		uint32_t remapped_count = 0;
		const auto get_remapped_count = [&]() {
			if (!remapped) {
				remapped_count = _remap_batch(p_packet, indices, batch_indices, first_index, amount);
				remapped = true;
			}
			return remapped_count;
		};
		const auto get_lighting_vertex_count = [&]() -> uint32_t {
			if (!reuse_lighting_vertices) {
				return 0;
			}
			const uint32_t unique_count = get_remapped_count();
			return unique_count <= amount - amount / 4 ? unique_count : 0;
		};
		if (normal_mapping) {
			FixedFunctionTransientNormalMapVertex *output = _normal_vertices.ptrw();
			uint32_t unique_count = get_lighting_vertex_count();
			const uint32_t *sources = _batch_source_indices.ptr();
			for (uint32_t v = 0; v < unique_count; v++) {
				FixedFunctionLightingVertex lit_vertex;
				FFModelTangentFrame frame;
				if (_lighting_vertex(p_packet, vertices, sources[v], lit_vertex, &frame) != OK ||
						_pack_normal_vertex(p_packet, sources[v], world_from_model, frame, lit_vertex, output[v], normal_map.flip_v, normal_occlusion) != OK) {
					// Preserve face-dependent normals and the original batch rejection rules.
					unique_count = 0;
				}
			}
			error = OK;
			for (uint32_t v = 0; !unique_count && v < amount; v += 3) {
				uint32_t triangle[3];
				for (int corner = 0; corner < 3; corner++) {
					triangle[corner] = batch_indices ? batch_indices[v + corner] : FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices, first_index + v + corner);
				}
				error = _normal_triangle(p_packet, vertices, triangle, world_from_model, output + v, normal_map.flip_v, normal_occlusion);
				if (error != OK) {
					break;
				}
			}
			if (error == OK) {
				error = device.draw_normal_mapped_triangles(output, unique_count ? unique_count : amount, p_projection, lighting, p_texture, p_packet.material.state, normal_map, normal_occlusion ? &occlusion_map : nullptr, unique_count ? _batch_draw_indices.ptr() : nullptr, unique_count ? amount : 0);
			}
			if (error == OK) {
				_note_draw(p_packet, amount, first_surface, r_info);
				offset += batch_triangles;
				continue;
			}
			if (error != ERR_UNAVAILABLE) {
				return error;
			}
			// Refusal submitted nothing. Draw this and the remaining batches with the
			// existing geometry-normal lighting, without replaying earlier batches.
			normal_mapping = false;
		}
		if (occlusion_mapping) {
			if (!occlusion_ready) {
				// Normal/AO success never allocates a separate AO stream. Initialize it
				// only when this draw actually needs the standalone pixel pipeline.
				occlusion_mapping = _prepare_transient_occlusion(p_packet, occlusion_map) && _occlusion_vertices.resize(p_batch_primitives * 3) == OK;
				occlusion_ready = true;
			}
			if (occlusion_mapping) {
				FixedFunctionTransientOcclusionVertex *output = _occlusion_vertices.ptrw();
				uint32_t unique_count = get_lighting_vertex_count();
				const uint32_t *sources = _batch_source_indices.ptr();
				for (uint32_t v = 0; v < unique_count; v++) {
					FixedFunctionLightingVertex lit_vertex;
					if (_lighting_vertex(p_packet, vertices, sources[v], lit_vertex) != OK ||
							_pack_occlusion_vertex(p_packet, sources[v], lit_vertex, output[v]) != OK) {
						unique_count = 0;
					}
				}
				error = OK;
				for (uint32_t v = 0; !unique_count && v < amount; v += 3) {
					uint32_t triangle[3];
					for (int corner = 0; corner < 3; corner++) {
						triangle[corner] = batch_indices ? batch_indices[v + corner] : FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices, first_index + v + corner);
					}
					error = _occlusion_triangle(p_packet, vertices, triangle, output + v);
					if (error != OK) {
						break;
					}
				}
				if (error == OK) {
					error = device.draw_occlusion_mapped_triangles(output, unique_count ? unique_count : amount, p_projection, lighting, p_texture, p_packet.material.state, occlusion_map, unique_count ? _batch_draw_indices.ptr() : nullptr, unique_count ? amount : 0);
				}
				if (error == OK) {
					_note_draw(p_packet, amount, first_surface, r_info);
					offset += batch_triangles;
					continue;
				}
				if (error != ERR_UNAVAILABLE) {
					return error;
				}
				// Refusal submitted nothing; fall back for the current and remaining
				// batches without repeating contributions already drawn by either path.
				occlusion_mapping = false;
			}
		}
		if (!fallback_ready) {
			// Native normal/AO draws need no CPU texture decode or fallback upload.
			use_occlusion = p_packet.occlusion && textures.texture_get_cpu_rgba8(occlusion_texture, occlusion.size, occlusion_rgba);
			if (use_occlusion) {
				occlusion.rgba = occlusion_rgba.ptr();
				occlusion.sampler_flags = p_packet.material.occlusion.sampler_flags;
				occlusion.single_channel = _get_occlusion_sample_channel(p_packet.material.occlusion.weights);
			}
			per_pixel = !use_occlusion && (pixel_lighting || _prepare_pixel_lighting(p_packet, lighting));
			if (per_pixel) {
				error = _lighting_vertices.resize(p_batch_primitives * 3);
				if (error != OK) {
					return error;
				}
			}
			fallback_ready = true;
		}
		bool lighting_drawn = false;
		while (true) {
			if (!per_pixel && !vertex_lighting_ready) {
				per_vertex = _prepare_streamed_vertex_lighting(p_packet, use_occlusion, lighting, view_normal_from_model) && _lighting_vertices.resize(p_batch_primitives * 3) == OK;
				vertex_lighting_ready = true;
			}
			if (!per_pixel && !per_vertex) {
				break;
			}
			const Basis *normal_from_model = per_vertex ? &view_normal_from_model : nullptr;
			FixedFunctionLightingVertex *output = _lighting_vertices.ptrw();
			uint32_t unique_count = get_lighting_vertex_count();
			const uint32_t *sources = _batch_source_indices.ptr();
			for (uint32_t v = 0; v < unique_count; v++) {
				if (_lighting_vertex(p_packet, vertices, sources[v], output[v], nullptr, normal_from_model) != OK) {
					unique_count = 0;
				}
			}
			error = OK;
			for (uint32_t v = 0; !unique_count && v < amount; v += 3) {
				uint32_t triangle[3];
				for (int corner = 0; corner < 3; corner++) {
					triangle[corner] = batch_indices ? batch_indices[v + corner] : FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices, first_index + v + corner);
				}
				error = _lighting_triangle(p_packet, vertices, triangle, output + v, nullptr, normal_from_model);
				if (error != OK) {
					break;
				}
			}
			const uint16_t *draw_indices = unique_count ? _batch_draw_indices.ptr() : nullptr;
			const uint32_t vertex_count = unique_count ? unique_count : amount;
			const uint32_t index_count = unique_count ? amount : 0;
			if (error == OK && per_vertex) {
				if (use_occlusion) {
					const FixedFunctionSpatialMaterial::FFOcclusionChannel &channel = p_packet.material.occlusion;
					for (uint32_t v = 0; v < vertex_count; v++) {
						uint32_t source = v;
						if (unique_count) {
							source = sources[v];
						} else if (batch_indices) {
							source = batch_indices[v];
						} else {
							source = FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices, first_index + v);
						}
						output[v].r = output[v].g = output[v].b = _sample_occlusion(occlusion, channel, FixedFunctionMeshStorage::surface_read_uv(p_packet.surface, source, channel.uv_set));
					}
				}
				Color color = _apply_vertex_alpha_state(p_packet, p_packet.material.albedo.factor);
				color.a = Math::round(CLAMP(color.a, 0.0f, 1.0f) * 255.0f) / 255.0f;
				error = device.draw_vertex_lit_triangles(output, vertex_count, p_projection, lighting, color, p_texture, p_packet.material.state, draw_indices, index_count);
			} else if (error == OK) {
				error = device.draw_lit_triangles(output, vertex_count, p_projection, lighting, p_texture, p_packet.material.state, draw_indices, index_count);
			}
			if (error == OK) {
				_note_draw(p_packet, amount, first_surface, r_info);
				offset += batch_triangles;
				lighting_drawn = true;
				break;
			}
			if (error != ERR_UNAVAILABLE) {
				return error;
			}
			// Refusal submitted nothing. Try the next pipeline for this and later batches.
			per_pixel = false;
			per_vertex = false;
		}
		if (lighting_drawn) {
			continue;
		}
		const FFOcclusionSample *occlusion_sample = use_occlusion ? &occlusion : nullptr;
		if (offset == 0 && !unshaded && indexed_lighting) {
			error = _submit_indexed_lit_triangles(p_packet, vertices, indices, p_projection, p_texture, first_surface, r_info, occlusion_sample);
			if (error != ERR_UNAVAILABLE) {
				return error;
			}
		}
		if (!_shadow_pass && device.supports_indexed_triangles() && (unshaded || authored_normals)) {
			const uint32_t unique_count = get_remapped_count();
			if (unique_count) {
				error = unshaded ? _pack_unshaded_batch(p_packet, vertices, nullptr, _batch_source_indices.ptr(), 0, unique_count) : _pack_indexed_batch(p_packet, vertices, unique_count, occlusion_sample);
			}
			if (unique_count && error == OK) {
				error = _submit_batch(p_packet, p_projection, p_texture, p_packet.material.state, amount, first_surface, r_info, unique_count);
				if (error == OK) {
					offset += batch_triangles;
					continue;
				}
				if (error != ERR_UNAVAILABLE) {
					return error;
				}
				// A native index limit can refuse the batch before any triangles are drawn.
			}
		}
		if (unshaded) {
			error = _pack_unshaded_batch(p_packet, vertices, indices, batch_indices, first_index, amount);
			if (error != OK) {
				return error;
			}
		} else {
			FixedFunctionVertex *output = _vertices.ptrw();
			if (initialize_lit_cache) {
				initialize_lit_cache = false;
				const int capacity = Math::next_power_of_2(MIN(p_packet.surface.source.vertex_count, uint32_t(MAX_LIT_VERTEX_CACHE_ENTRIES)));
				// Cache across native batches of this packet with bounded scratch memory.
				if (_lit_vertex_cache.size() >= capacity || _lit_vertex_cache.resize(capacity) == OK) {
					lit_cache = _lit_vertex_cache.ptrw();
					lit_cache_mask = capacity - 1;
					for (int i = 0; i < capacity; i++) {
						lit_cache[i].index = UINT32_MAX;
					}
				}
			}
			for (uint32_t v = 0; v < amount; v += 3) {
				uint32_t triangle[3];
				for (int corner = 0; corner < 3; corner++) {
					triangle[corner] = batch_indices ? batch_indices[v + corner] : FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices, first_index + v + corner);
				}
				if (lit_cache) {
					error = _cached_lit_triangle(p_packet, vertices, triangle, output + v, occlusion_sample, lit_cache, lit_cache_mask);
				} else {
					error = _lit_triangle(p_packet, vertices, triangle, output + v, occlusion_sample);
				}
				if (error != OK) {
					return error;
				}
			}
		}
		error = _submit_batch(p_packet, p_projection, p_texture, p_packet.material.state, amount, first_surface, r_info);
		if (error != OK) {
			return error;
		}
		offset += batch_triangles;
	}
	return OK;
}
