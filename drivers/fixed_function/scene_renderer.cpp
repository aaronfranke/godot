#include "scene_renderer.h"

#include "core/project_settings.h"

#include <float.h>
#include <string.h>
#include <limits>
#include <utility>

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

bool FixedFunctionSceneRenderer::_finite_transform(const Transform &p_transform) {
	for (int column = 0; column < 4; column++) {
		const Vector3 vector = column < 3 ? p_transform.basis.get_axis(column) : p_transform.origin;
		for (int row = 0; row < 3; row++) {
			if (Math::is_nan(vector[row]) || Math::is_inf(vector[row])) {
				return false;
			}
		}
	}
	return true;
}

Error FixedFunctionSceneRenderer::_prepare(const Transform &p_camera, const CameraMatrix &p_projection, RasterizerScene::InstanceBase **p_instances, int p_count, const RID *p_lights, int p_light_count) {
	const Transform view = p_camera.affine_inverse();
	FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	FFMultimeshLights multimesh_lights;
	multimesh_lights.sources = p_lights;
	multimesh_lights.source_count = p_light_count;
	const FFSceneOptions options = {
		p_camera,
		multimesh_lights,
		bool(GLOBAL_GET("rendering/quality/fixed_function/antialiased_lines")),
		bool(GLOBAL_GET("rendering/quality/fixed_function/emission")),
		bool(GLOBAL_GET("rendering/quality/fixed_function/texture_occlusion")),
	};
	for (int i = 0; i < p_count; i++) {
		const RasterizerScene::InstanceBase *instance = p_instances[i];
		if (!instance) {
			return ERR_INVALID_PARAMETER;
		}
		if (!instance->visible || (_shadow_pass ? instance->cast_shadows == VS::SHADOW_CASTING_SETTING_OFF : instance->cast_shadows == VS::SHADOW_CASTING_SETTING_SHADOWS_ONLY)) {
			continue;
		}
		if (instance->base_type == VS::INSTANCE_MESH && meshes.owns_mesh(instance->base)) {
			const Error error = _prepare_mesh(view, p_projection, *instance, instance->base, i, options);
			if (error != OK) {
				return error;
			}
		} else if (instance->base_type == VS::INSTANCE_IMMEDIATE) {
			RID mesh;
			const Error error = meshes.immediate_get_mesh(instance->base, mesh);
			if (error != OK) {
				return error;
			}
			if (mesh.is_valid()) {
				const Error draw_error = _prepare_mesh(view, p_projection, *instance, mesh, i, options);
				if (draw_error != OK) {
					return draw_error;
				}
			}
		} else if (instance->base_type == VS::INSTANCE_MULTIMESH) {
			RID mesh;
			Vector<FixedFunctionMeshStorage::FFMultiMeshInstance> copies;
			int visible;
			const Error error = meshes.multimesh_get_draw_data(instance->base, mesh, copies, visible);
			if (error != OK) {
				return error;
			}
			if (!mesh.is_valid()) {
				continue;
			}
			const bool is_2d = meshes.multimesh_get_transform_format(instance->base) == VS::MULTIMESH_TRANSFORM_2D;
			if (!_shadow_pass && _retained_geometry_enabled && !is_2d && visible >= 8 &&
					_context.get_device().supports_indexed_triangles() && meshes.mesh_get_surface_count(mesh) == 1) {
				const uint64_t multimesh = meshes.multimesh_get_geometry_identity(instance->base);
				if (meshes.multimesh_get_geometry_version(multimesh)) {
					const bool sort_packets = _sort_packets;
					const bool lighting_required = _lighting_required;
					const bool redraw_requested = _redraw_requested;
					if (_prepare_mesh(view, p_projection, *instance, mesh, i, options, nullptr, multimesh) == OK) {
						continue;
					}
					_sort_packets = sort_packets;
					_lighting_required = lighting_required;
					_redraw_requested = redraw_requested;
				}
			}
			for (int copy = 0; copy < visible; copy++) {
				if (FixedFunctionMeshStorage::multimesh_is_collapsed(copies[copy].transform, is_2d)) {
					continue;
				}
				const Error draw_error = _prepare_mesh(view, p_projection, *instance, mesh, i, options, &copies[copy]);
				if (draw_error != OK) {
					return draw_error;
				}
			}
		} else {
			return ERR_INVALID_PARAMETER;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_prepare_mesh(const Transform &p_view, const CameraMatrix &p_projection, const RasterizerScene::InstanceBase &p_instance, RID p_mesh, int p_index, const FFSceneOptions &p_options, const FixedFunctionMeshStorage::FFMultiMeshInstance *p_copy, uint64_t p_multimesh) {
	FixedFunctionMeshStorage &meshes = _context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	FixedFunctionMaterialStorage &materials = _context.get_material_storage();
	if (!_shadow_pass && p_instance.lightmap.is_valid()) {
		return ERR_UNAVAILABLE;
	}
	Vector<Transform> bones;
	const bool single_mesh = !p_copy && !p_multimesh;
	if (single_mesh && p_instance.skeleton.is_valid()) {
		const Error error = meshes.skeleton_get_pose(p_instance.skeleton, bones);
		if (error != OK) {
			return error;
		}
	}
	const bool has_weights = single_mesh && !p_instance.blend_values.empty();
	const VS::BlendShapeMode blend_mode = has_weights ? meshes.mesh_get_blend_shape_mode(p_mesh) : VS::BLEND_SHAPE_MODE_NORMALIZED;
	const Transform model = p_copy ? p_instance.transform * p_copy->transform : p_instance.transform;
	if (!_finite_transform(model)) {
		return ERR_INVALID_PARAMETER;
	}
	const Transform view_from_model = p_view * model;
	if (!_finite_transform(view_from_model)) {
		return ERR_INVALID_PARAMETER;
	}
	_redraw_requested |= p_instance.redraw_if_visible;
	for (int surface = 0; surface < meshes.mesh_get_surface_count(p_mesh); surface++) {
		FixedFunctionMeshStorage::FFSurfaceData surface_data;
		Error error = meshes.mesh_surface_get_data(p_mesh, surface, surface_data);
		if (error != OK) {
			return error;
		}
		const bool lines = surface_data.primitive == VS::PRIMITIVE_LINES || surface_data.primitive == VS::PRIMITIVE_LINE_STRIP || surface_data.primitive == VS::PRIMITIVE_LINE_LOOP;
		const bool points = surface_data.primitive == VS::PRIMITIVE_POINTS;
		if (_shadow_pass && (points || lines)) {
			continue;
		}
		const uint32_t count = surface_data.index_count ? surface_data.index_count : surface_data.vertex_count;
		if ((surface_data.primitive == VS::PRIMITIVE_TRIANGLES && count % 3) || (surface_data.primitive == VS::PRIMITIVE_LINES && count % 2)) {
			return ERR_INVALID_PARAMETER;
		}
		if (!points && !lines && !FixedFunctionMeshStorage::surface_get_triangle_count(surface_data)) {
			continue;
		}
		RID base_material = p_instance.material_override;
		if (!base_material.is_valid() && surface < p_instance.materials.size()) {
			base_material = p_instance.materials[surface];
		}
		if (!base_material.is_valid()) {
			base_material = p_instance.base_type == VS::INSTANCE_IMMEDIATE ? meshes.immediate_get_material(p_instance.base) : meshes.mesh_surface_get_material(p_mesh, surface);
		}
		RID overlay = p_instance.material_overlay;
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
			if (p_multimesh && next.is_valid()) {
				return ERR_UNAVAILABLE;
			}
			FFDrawPacket packet;
			if (next.is_valid()) {
				packet.surface = surface_data;
			} else {
				packet.surface = std::move(surface_data);
			}
			packet.view_from_model = view_from_model;
			packet.instance_index = p_index;
			packet.submission_index = _packets.size();
			packet.layer_mask = p_instance.layer_mask;
			packet.depth = 0;
			packet.priority = 0;
			packet.line_width = 1;
			packet.flip_v = false;
			packet.antialiased = false;
			packet.retained_geometry = false;
			packet.optional_material_pass = !base;
			if (base) {
				error = materials.material_get_spatial_draw_parameters(material, packet.material);
				if (error != OK) {
					return error;
				}
			} else if (materials.material_get_spatial_parameters(material, packet.material) != OK) {
				// An unsupported optional material must never replace the base.
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
				if (p_instance.cast_shadows == VS::SHADOW_CASTING_SETTING_DOUBLE_SIDED) {
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
						packet.view_from_model.basis[axis][axis] = model.basis.get_axis(axis).length();
					}
				}
			}
			if (transform_flags & FixedFunctionSpatialMaterial::TRANSFORM_FIXED_SIZE) {
				// Match Material3D's scale relative to the vertical viewport span.
				real_t scale = -packet.view_from_model.origin.z;
				if (p_projection.matrix[3][3] != 0) {
					if (p_projection.matrix[1][1] == 0) {
						return ERR_INVALID_PARAMETER;
					}
					scale = Math::abs(real_t(1) / p_projection.matrix[1][1]);
				}
				packet.view_from_model.basis = packet.view_from_model.basis * scale;
			}
			if (transform_flags && !_finite_transform(packet.view_from_model)) {
				return ERR_INVALID_PARAMETER;
			}
			if (p_copy) {
				// Instance color is part of COLOR, so only shaders that consume
				// vertex RGB or alpha apply the corresponding components.
				Color &albedo = packet.material.albedo.factor;
				if (packet.material.albedo_uses_vertex_color) {
					for (int channel = 0; channel < 3; channel++) {
						albedo[channel] *= p_copy->color[channel];
					}
				}
				if (packet.material.albedo_uses_vertex_alpha) {
					albedo.a *= p_copy->color.a;
				}
			}
			if (_debug_unshaded) {
				packet.material.lighting_flags |= FixedFunctionSpatialMaterial::LIGHTING_UNSHADED;
			}
			if (!(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED)) {
				const Basis world_basis = transform_flags ? p_view.basis.inverse() * packet.view_from_model.basis : model.basis;
				packet.normal_from_model = _normal_matrix(world_basis);
				_lighting_required = true;
			}
			if (lines && material.is_valid()) {
				packet.line_width = MAX(materials.material_get_line_width(material), 1.0f);
			}
			if (material.is_valid()) {
				packet.priority = static_cast<int8_t>(materials.material_get_render_priority(material));
			}
			const FixedFunctionGraphicsDevice::FFCapabilities &caps = _context.get_device().get_capabilities();
			const bool blend_mix = caps.blend_mix;
			if (packet.material.state.alpha_cutoff_enabled && !caps.alpha_cutoff) {
				if (_shadow_pass) {
					return ERR_UNAVAILABLE; // Unsupported coverage cannot become an opaque shadow.
				}
				if (!base) {
					continue; // Extra passes cannot preserve the base surface's cutout coverage.
				}
				// Keep cutout materials visible without alpha testing, using alpha blending when available.
				packet.material.state.alpha_cutoff_enabled = false;
				if (blend_mix) {
					packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
					packet.material.state.depth_write_enabled = false;
				}
			}
			packet.late = packet.material.state.blend_mode != FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED || !packet.material.state.depth_test_enabled;
			if ((packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD && !caps.blend_add) ||
					(packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MUL && !caps.blend_mul)) {
				packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
			}
			if (packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MIX && !blend_mix) {
				if (!base) {
					continue;
				}
				// Keep base geometry visible on devices without optional alpha blending.
				packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
			}
			// The coverage fringe needs a blend mode that weights RGB by alpha.
			packet.antialiased = lines && p_options.antialiased && !packet.material.state.alpha_cutoff_enabled &&
					((blend_mix && packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED) ||
							packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MIX || packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD);
			if (packet.antialiased) {
				packet.late = true;
				// Coverage must not accidentally enable an opaque texture's unused alpha.
				packet.material.state.texture_alpha_enabled &= packet.material.state.blend_mode != FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
				if (packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED) {
					packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
				}
				packet.material.state.depth_write_enabled = false;
			}
			if (packet.late) {
				_sort_packets = true;
				// VisualServer supplies the same depth (including sorting offset) used by GLES.
				packet.depth = p_instance.depth;
				if (p_copy) {
					packet.depth += p_view.xform(p_instance.transform.origin).z - view_from_model.origin.z;
				}
				if (Math::is_nan(packet.depth) || Math::is_inf(packet.depth)) {
					return ERR_INVALID_PARAMETER;
				}
			}
			// ImmediateGeometry::begin supplies a per-batch albedo binding, including
			// for material overrides/extra passes. A freed binding retains ordinary material data.
			if (packet.surface.immediate_texture.is_valid() && textures.texture_can_be_referenced(packet.surface.immediate_texture)) {
				packet.material.albedo.texture = packet.surface.immediate_texture;
			}
			if (packet.material.is_fallback && packet.material.albedo.texture.is_valid() && !textures.owns_texture(packet.material.albedo.texture)) {
				packet.material.albedo.texture = RID();
			}
			if (!base && packet.material.albedo.texture.is_valid() && !textures.owns_texture(packet.material.albedo.texture)) {
				continue;
			}
			if (packet.material.albedo.texture.is_valid()) {
				const RID target = textures.texture_get_render_target(packet.material.albedo.texture);
				if (p_multimesh && target.is_valid()) {
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
					packet.flip_v = !textures.render_target_get_flag(target, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP);
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
					if (packet.material.albedo.texture.is_valid()) {
						packet.material.state.sampler_flags = textures.texture_get_sampler_flags(packet.material.albedo.texture);
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
			const bool shaded_triangles = !points && !lines && !(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED);
			packet.occlusion = false;
			FixedFunctionSpatialMaterial::FFEmissionChannel &emission = packet.material.emission;
			const bool cutout = packet.material.state.alpha_cutoff_enabled;
			const FixedFunctionGraphicsDevice::FFBlendMode blend = packet.material.state.blend_mode;
			const bool weighted_emission = blend == FixedFunctionGraphicsDevice::BLEND_MODE_MIX || blend == FixedFunctionGraphicsDevice::BLEND_MODE_ADD;
			if (!shaded_triangles || !p_options.emission ||
					(!weighted_emission && (blend != FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED || !packet.material.state.depth_write_enabled)) ||
					!((cutout && !weighted_emission) ? caps.blend_add_unweighted : caps.blend_add)) {
				emission.energy = 0;
			}
			if (emission.energy > 0 && emission.color.texture.is_valid() && textures.owns_texture(emission.color.texture)) {
				const RID target = textures.texture_get_render_target(emission.color.texture);
				if (target.is_valid() && target != _render_targets.get_current_render_target() && !textures.render_target_has_failed(target)) {
					textures.render_target_mark_used(target);
				}
			}
			packet.normal_mapping = _prepare_normal_map(packet);
			if (!bones.empty() || (has_weights && !packet.surface.blend_shapes.empty())) {
				const bool tangents = packet.normal_mapping && _context.get_device().supports_transient_normal_mapping();
				error = _prepare_deformation(packet, bones, p_instance.blend_values, blend_mode, tangents);
				if (error != OK) {
					return error;
				}
			}
			const bool static_geometry = packet.deformed_vertices.empty();
			if (!static_geometry && packet.deformed_tangents.empty()) {
				// Dynamic meshes need authored frames; do not run MikkTSpace every frame.
				packet.normal_mapping = false;
			}
			packet.occlusion_secondary_uv = false;
			const FixedFunctionSpatialMaterial::FFOcclusionChannel &occlusion = packet.material.occlusion;
			const bool wants_occlusion = shaded_triangles && p_options.occlusion && occlusion.texture.is_valid() &&
					(occlusion.light_affect > 0 || !(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_AMBIENT_DISABLED));
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
						packet.baked_occlusion_sampler_flags = textures.texture_get_sampler_flags(occlusion.texture);
						packet.occlusion = true;
					} else {
						Size2i size;
						PoolVector<uint8_t> rgba;
						packet.occlusion = textures.texture_get_cpu_rgba8(occlusion.texture, size, rgba);
					}
				}
			}
			const bool retain = static_geometry && (!packet.occlusion || pixel_occlusion || packet.baked_occlusion_image_version);
			if (packet.occlusion && !pixel_occlusion) {
				packet.normal_mapping = false; // Preserve the complete per-vertex AO fallback.
			}
			packet.retained_geometry = retain && !_shadow_pass;
			if (p_multimesh) {
				packet.multimesh = p_multimesh;
				if (!_request_multimesh_geometry(packet, model, p_view, p_options)) {
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
	const Vector3 position = p_packet.deformed_vertices.empty() ? FixedFunctionMeshStorage::surface_read_position(surface, p_data, p_index) : p_packet.deformed_vertices[p_index].position;
	if (r_model_position) {
		*r_model_position = position;
	}
	// Camera-relative math stays real_t; only the final native vertex becomes float.
	r_position = p_packet.view_from_model.xform(position);
	const int uv_set = p_packet.material.albedo.uv_set;
	// Point-sprite coordinates are filled per corner during expansion.
	r_uv = uv_set == FixedFunctionSpatialMaterial::UV_POINT_COORD ? Vector2() : _vertex_uv(p_packet, p_data, p_index, uv_set);
	r_uv = r_uv * p_packet.material.albedo.uv_scale + p_packet.material.albedo.uv_offset;
	if (p_packet.flip_v) {
		r_uv.y = 1 - r_uv.y;
	}
}

Color FixedFunctionSceneRenderer::_apply_vertex_alpha_state(const FFDrawPacket &p_packet, Color p_color) {
	if (!p_packet.material.state.alpha_cutoff_enabled && (p_packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED || !p_packet.material.state.texture_alpha_enabled)) {
		p_color.a = 1;
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
	if ((p_packet.material.albedo_uses_vertex_color || p_packet.material.albedo_uses_vertex_alpha) && (surface.format & VS::ARRAY_FORMAT_COLOR)) {
		const Color vertex_color = p_packet.morph_attributes.empty() ? FixedFunctionMeshStorage::surface_read_color(surface, p_data, p_index) : p_packet.morph_attributes[p_index].color;
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
	const bool vertex_color = (p_packet.surface.format & VS::ARRAY_FORMAT_COLOR) && (p_packet.material.albedo_uses_vertex_color || p_packet.material.albedo_uses_vertex_alpha);
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

Error FixedFunctionSceneRenderer::_submit_batch(const FFDrawPacket &p_packet, const CameraMatrix &p_projection, RID p_texture, const FixedFunctionGraphicsDevice::FFDrawParameters &p_state, uint32_t p_count, bool &r_first_surface, uint32_t p_indexed_vertices) {
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
		_context.get_storage_queries().note_scene_draw(p_count, !_instances_drawn[p_packet.instance_index], r_first_surface);
		_instances_drawn.write[p_packet.instance_index] = 1;
		r_first_surface = false;
	}
	return error;
}

Error FixedFunctionSceneRenderer::_submit_triangles(const FFDrawPacket &p_packet, const CameraMatrix &p_projection, RID p_texture, uint32_t p_batch_primitives, bool p_first_surface) {
	FixedFunctionGraphicsDevice &device = _context.get_device();
	Error error = OK;
	const PoolVector<uint8_t>::Read vertices = p_packet.surface.array.read();
	const PoolVector<uint8_t>::Read indices = p_packet.surface.index_array.read();
	const uint32_t count = FixedFunctionMeshStorage::surface_get_triangle_count(p_packet.surface);
	Vector<uint32_t> expanded;
	uint32_t *batch_indices = nullptr;
	if (p_packet.surface.primitive != VS::PRIMITIVE_TRIANGLES) {
		error = expanded.resize(MIN(count, p_batch_primitives) * 3);
		if (error != OK) {
			return error;
		}
		batch_indices = expanded.ptrw();
	}
	FixedFunctionGraphicsDevice::FFPixelLighting lighting;
	PoolVector<uint8_t> occlusion_rgba;
	FFOcclusionSample occlusion;
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	const RID occlusion_texture = p_packet.material.occlusion.texture;
	PoolVector<uint8_t>::Read occlusion_read;
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
	bool initialize_lit_cache = count > 1 && p_packet.surface.vertex_count > 0 && (p_packet.surface.index_count || batch_indices) && (p_packet.surface.format & VS::ARRAY_FORMAT_NORMAL);
	// Dense staging shades every source vertex, so reserve it for meshes with
	// enough index reuse. Larger source meshes keep the bounded corner cache.
	const bool indexed_lighting = initialize_lit_cache && !batch_indices && !_shadow_pass && device.supports_indexed_triangles() &&
			p_packet.surface.vertex_count <= MAX_INDEXED_LIT_VERTICES &&
			p_packet.surface.index_count >= MAX(96, p_packet.surface.vertex_count * 2);
	FFLitVertexCacheEntry *lit_cache = nullptr;
	uint32_t lit_cache_mask = 0;
	bool first_surface = p_first_surface;
	const bool unshaded = p_packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED;
	const bool authored_normals = p_packet.surface.format & VS::ARRAY_FORMAT_NORMAL;
	// Indexed lighting uploads vertices and indices. Require roughly 25% fewer
	// vertices than corners, first by surface and then by batch.
	const uint64_t corners = uint64_t(count) * 3;
	const bool reuse_lighting_vertices = authored_normals && p_packet.surface.index_count && uint64_t(p_packet.surface.vertex_count) <= corners - corners / 4;
	for (uint32_t offset = 0; offset < count;) {
		const uint32_t batch_triangles = MIN(count - offset, p_batch_primitives);
		const uint32_t amount = batch_triangles * 3;
		const uint32_t first_index = batch_indices ? 0 : offset * 3;
		if (batch_indices) {
			FixedFunctionMeshStorage::surface_expand_triangles(p_packet.surface, indices.ptr(), offset, batch_triangles, batch_indices);
		}
		bool remapped = false;
		uint32_t remapped_count = 0;
		const auto get_remapped_count = [&]() {
			if (!remapped) {
				remapped_count = _remap_batch(p_packet, indices.ptr(), batch_indices, first_index, amount);
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
				Vector3 model_normal;
				if (_lighting_vertex(p_packet, vertices.ptr(), sources[v], lit_vertex, &model_normal) != OK ||
						_pack_normal_vertex(p_packet, vertices.ptr(), sources[v], world_from_model, model_normal, lit_vertex, output[v], normal_map.flip_v, normal_occlusion) != OK) {
					// Preserve face-dependent normals and the original batch rejection rules.
					unique_count = 0;
				}
			}
			error = OK;
			for (uint32_t v = 0; !unique_count && v < amount; v += 3) {
				uint32_t triangle[3];
				for (int corner = 0; corner < 3; corner++) {
					triangle[corner] = batch_indices ? batch_indices[v + corner] : FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices.ptr(), first_index + v + corner);
				}
				error = _normal_triangle(p_packet, vertices.ptr(), triangle, world_from_model, output + v, normal_map.flip_v, normal_occlusion);
				if (error != OK) {
					break;
				}
			}
			if (error == OK) {
				error = device.draw_normal_mapped_triangles(output, unique_count ? unique_count : amount, p_projection, lighting, p_texture, p_packet.material.state, normal_map, normal_occlusion ? &occlusion_map : nullptr, unique_count ? _batch_draw_indices.ptr() : nullptr, unique_count ? amount : 0);
			}
			if (error == OK) {
				_context.get_storage_queries().note_scene_draw(amount, !_instances_drawn[p_packet.instance_index], first_surface);
				_instances_drawn.write[p_packet.instance_index] = 1;
				first_surface = false;
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
					if (_lighting_vertex(p_packet, vertices.ptr(), sources[v], lit_vertex) != OK ||
							_pack_occlusion_vertex(p_packet, vertices.ptr(), sources[v], lit_vertex, output[v]) != OK) {
						unique_count = 0;
					}
				}
				error = OK;
				for (uint32_t v = 0; !unique_count && v < amount; v += 3) {
					uint32_t triangle[3];
					for (int corner = 0; corner < 3; corner++) {
						triangle[corner] = batch_indices ? batch_indices[v + corner] : FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices.ptr(), first_index + v + corner);
					}
					error = _occlusion_triangle(p_packet, vertices.ptr(), triangle, output + v);
					if (error != OK) {
						break;
					}
				}
				if (error == OK) {
					error = device.draw_occlusion_mapped_triangles(output, unique_count ? unique_count : amount, p_projection, lighting, p_texture, p_packet.material.state, occlusion_map, unique_count ? _batch_draw_indices.ptr() : nullptr, unique_count ? amount : 0);
				}
				if (error == OK) {
					_context.get_storage_queries().note_scene_draw(amount, !_instances_drawn[p_packet.instance_index], first_surface);
					_instances_drawn.write[p_packet.instance_index] = 1;
					first_surface = false;
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
			occlusion_read = occlusion_rgba.read();
			if (use_occlusion) {
				occlusion.rgba = occlusion_read.ptr();
				occlusion.sampler_flags = textures.texture_get_sampler_flags(occlusion_texture);
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
				if (_lighting_vertex(p_packet, vertices.ptr(), sources[v], output[v], nullptr, normal_from_model) != OK) {
					unique_count = 0;
				}
			}
			error = OK;
			for (uint32_t v = 0; !unique_count && v < amount; v += 3) {
				uint32_t triangle[3];
				for (int corner = 0; corner < 3; corner++) {
					triangle[corner] = batch_indices ? batch_indices[v + corner] : FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices.ptr(), first_index + v + corner);
				}
				error = _lighting_triangle(p_packet, vertices.ptr(), triangle, output + v, nullptr, normal_from_model);
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
							source = FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices.ptr(), first_index + v);
						}
						output[v].r = output[v].g = output[v].b = _sample_occlusion(occlusion, channel, _vertex_uv(p_packet, vertices.ptr(), source, channel.uv_set));
					}
				}
				Color color = _apply_vertex_alpha_state(p_packet, p_packet.material.albedo.factor);
				color.a = Math::round(CLAMP(color.a, 0.0f, 1.0f) * 255.0f) / 255.0f;
				error = device.draw_vertex_lit_triangles(output, vertex_count, p_projection, lighting, color, p_texture, p_packet.material.state, draw_indices, index_count);
			} else if (error == OK) {
				error = device.draw_lit_triangles(output, vertex_count, p_projection, lighting, p_texture, p_packet.material.state, draw_indices, index_count);
			}
			if (error == OK) {
				_context.get_storage_queries().note_scene_draw(amount, !_instances_drawn[p_packet.instance_index], first_surface);
				_instances_drawn.write[p_packet.instance_index] = 1;
				first_surface = false;
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
			error = _submit_indexed_lit_triangles(p_packet, vertices.ptr(), indices.ptr(), p_projection, p_texture, first_surface, occlusion_sample);
			if (error != ERR_UNAVAILABLE) {
				return error;
			}
		}
		if (!_shadow_pass && device.supports_indexed_triangles() && (unshaded || authored_normals)) {
			const uint32_t unique_count = get_remapped_count();
			if (unique_count) {
				error = unshaded ? _pack_unshaded_batch(p_packet, vertices.ptr(), nullptr, _batch_source_indices.ptr(), 0, unique_count) : _pack_indexed_batch(p_packet, vertices.ptr(), unique_count, occlusion_sample);
			}
			if (unique_count && error == OK) {
				error = _submit_batch(p_packet, p_projection, p_texture, p_packet.material.state, amount, first_surface, unique_count);
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
			error = _pack_unshaded_batch(p_packet, vertices.ptr(), indices.ptr(), batch_indices, first_index, amount);
			if (error != OK) {
				return error;
			}
		} else {
			FixedFunctionVertex *output = _vertices.ptrw();
			if (initialize_lit_cache) {
				initialize_lit_cache = false;
				const int capacity = next_power_of_2(MIN(p_packet.surface.vertex_count, int(MAX_LIT_VERTEX_CACHE_ENTRIES)));
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
					triangle[corner] = batch_indices ? batch_indices[v + corner] : FixedFunctionMeshStorage::surface_read_index(p_packet.surface, indices.ptr(), first_index + v + corner);
				}
				if (lit_cache) {
					error = _cached_lit_triangle(p_packet, vertices.ptr(), triangle, output + v, occlusion_sample, lit_cache, lit_cache_mask);
				} else {
					error = _lit_triangle(p_packet, vertices.ptr(), triangle, output + v, occlusion_sample);
				}
				if (error != OK) {
					return error;
				}
			}
		}
		error = _submit_batch(p_packet, p_projection, p_texture, p_packet.material.state, amount, first_surface);
		if (error != OK) {
			return error;
		}
		offset += batch_triangles;
	}
	return OK;
}

bool FixedFunctionSceneRenderer::_prepare_depth_fog(const CameraMatrix &p_projection, RID p_environment) {
	if (!_context.get_scene_storage().environment_get_depth_fog(p_environment, _fog.color, _fog.begin, _fog.end)) {
		return false;
	}
	if (_fog.end <= 0) {
		_fog.end = p_projection.get_z_far();
	}
	if (!(_fog.end > _fog.begin) || Math::is_inf(_fog.end)) {
		return false;
	}
	// All Fixed Function targets blend in LDR. Fog changes RGB only.
	_fog.color = Color(CLAMP(_fog.color.r, 0.0f, 1.0f), CLAMP(_fog.color.g, 0.0f, 1.0f), CLAMP(_fog.color.b, 0.0f, 1.0f));
	_fog.orthogonal = p_projection.is_orthogonal();
	// Keep the original near/far mapping for native Z table fog. Clip perspective
	// distances in front of the near plane before dividing by W.
	const real_t near_depth = p_projection.get_z_near();
	const real_t begin = _fog.orthogonal ? real_t(_fog.begin) : MAX(real_t(_fog.begin), near_depth);
	const real_t end = _fog.orthogonal ? real_t(_fog.end) : MAX(real_t(_fog.end), near_depth);
	const real_t z = p_projection.matrix[2][2];
	const real_t offset = p_projection.matrix[3][2];
	const real_t begin_w = _fog.orthogonal ? 1 : begin;
	const real_t end_w = _fog.orthogonal ? 1 : end;
	_fog.projected_begin = float(((-begin * z + offset) / begin_w + 1) * real_t(0.5));
	_fog.projected_end = float(((-end * z + offset) / end_w + 1) * real_t(0.5));
	return true;
}

Error FixedFunctionSceneRenderer::_submit(const Transform &p_camera, const CameraMatrix &p_projection, RID p_environment) {
	if (_packets.empty()) {
		return _submit_sky(p_camera, p_projection, p_environment);
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
	error = _render_targets.begin_draw(true);
	if (error != OK) {
		return error;
	}
	error = device.clear_depth();
	if (error != OK) {
		return error;
	}
	error = _submit_sky(p_camera, p_projection, p_environment);
	if (error != OK) {
		return error;
	}
	const bool use_fog = !_shadow_pass && _prepare_depth_fog(p_projection, p_environment);
	const uint32_t *order = _sort_packets ? _packet_order.ptr() : nullptr;
	const int packet_count = _packets.size();
	FFDrawPacket *packets = _packets.ptrw();
	for (int i = 0; i < packet_count; i++) {
		FFDrawPacket &packet = packets[order ? order[i] : i];
		if (!(packet.material.lighting_flags & FixedFunctionSpatialMaterial::LIGHTING_UNSHADED)) {
			_filter_local_lights(packet);
		}
		// Native begin may recover/reset resources. Resolve again after that boundary.
		RID native_texture;
		if (packet.material.albedo.texture.is_valid()) {
			FixedFunctionTextureStorage &textures = _context.get_texture_storage();
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
				error = OK; // The albedo factor remains drawable without the native texture.
			}
			if (error != OK) {
				return error;
			}
		}
		if (!FixedFunctionMeshStorage::surface_get_triangle_count(packet.surface)) {
			if (packet.surface.primitive == VS::PRIMITIVE_POINTS) {
				error = _submit_points(packet, p_projection, native_texture);
			} else {
				error = _submit_lines(packet, p_projection, native_texture);
			}
			if (error != OK) {
				return error;
			}
			continue;
		}
		packet.material.state.fog = use_fog ? &_fog : nullptr;
		error = !_shadow_pass && packet.retained_geometry ? _submit_geometry(packet, p_projection, native_texture) : ERR_UNAVAILABLE;
		const bool retained = error == OK;
		if (error == ERR_SKIP) {
			_redraw_requested = true;
			return _render_targets.defer_render_target();
		}
		if (error == ERR_UNAVAILABLE) {
			if (packet.multimesh) {
				error = _submit_multimesh_copies(packet, p_projection, native_texture, batch_primitives);
			} else {
				error = _submit_triangles(packet, p_projection, native_texture, batch_primitives, true);
			}
		}
		if (error != OK) {
			return error;
		}
		if (packet.material.emission.energy > 0) {
			error = _submit_emission(packet, p_projection, native_texture, batch_primitives, retained);
			if (error != OK) {
				return error;
			}
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::render(const Transform &p_camera, const CameraMatrix &p_projection, RasterizerScene::InstanceBase **p_instances, int p_count, RID p_environment, const RID *p_lights, int p_light_count) {
	_shadow_parameters.texture = RID();
	if (_shadow_ready && _shadow_frame == _context.get_frame_number()) {
		const Basis inverse = _shadow_transform.transform.basis.inverse();
		const Transform relative(inverse, inverse.xform(p_camera.origin - _shadow_transform.transform.origin));
		_shadow_parameters.clip_from_world = _shadow_transform.projection * CameraMatrix(relative);
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
	_per_pixel_lighting = bool(GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting"));
	_normal_mapping_enabled = bool(GLOBAL_GET("rendering/quality/fixed_function/normal_mapping"));
	_retained_geometry_enabled = bool(GLOBAL_GET("rendering/quality/fixed_function/retained_geometry"));
	_directional_lights.clear();
	_local_lights.clear();
	for (int i = 0; i < 3; i++) {
		_ambient[i] = 0;
	}
	_packets.clear();
	if (p_count < 0 || (p_count && !p_instances) || !_finite_transform(p_camera) || p_camera.basis.determinant() == 0) {
		return ERR_INVALID_PARAMETER;
	}
	for (int column = 0; column < 4; column++) {
		for (int row = 0; row < 4; row++) {
			const real_t value = p_projection.matrix[column][row];
			if (Math::is_nan(value) || Math::is_inf(value) || value < -FLT_MAX || value > FLT_MAX) {
				return ERR_INVALID_PARAMETER;
			}
		}
	}
	Error error = _context.get_light_storage().validate_light_instances(p_lights, p_light_count);
	if (error == OK && p_environment.is_valid()) {
		error = _context.get_scene_storage().environment_check(p_environment);
	}
	if (error == OK) {
		error = _instances_drawn.resize(p_count);
	}
	if (error == OK && p_count) {
		memset(_instances_drawn.ptrw(), 0, p_count);
	}
	if (error == OK) {
		error = _prepare(p_camera, p_projection, p_instances, p_count, p_lights, p_light_count);
	}
	if (error == OK && _lighting_required) {
		error = _prepare_lighting(p_camera, p_environment, p_lights, p_light_count);
	}
	if (error == OK) {
		for (int i = 0; i < _packets.size(); i++) {
			FFDrawPacket &packet = _packets.write[i];
			if (packet.retained_geometry && !packet.multimesh) {
				packet.retained_geometry = _request_geometry(packet);
			}
		}
	}
	if (error == OK && _sort_packets) {
		error = _packet_order.resize(_packets.size());
		if (error == OK) {
			uint32_t *order = _packet_order.ptrw();
			for (int i = 0; i < _packets.size(); i++) {
				order[i] = i;
			}
			SortArray<uint32_t, FFDrawPacketOrder> sorter;
			sorter.compare.packets = _packets.ptr();
			sorter.sort(order, _packets.size());
		}
	}
	if (error == OK) {
		error = _submit(p_camera, p_projection, p_environment);
	}
	if (error != OK) {
		_render_targets.report_draw_error(error);
	}
	_packets.clear();
	_directional_lights.clear();
	_local_lights.clear();
	return error;
}

FixedFunctionSceneRenderer::FixedFunctionSceneRenderer(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_render_targets) :
		_context(p_context), _render_targets(p_render_targets), _shadow_frame(0), _sky_color(0), _redraw_requested(false), _sort_packets(false), _lighting_required(false), _shadow_pass(false), _shadow_reverse_cull(false), _shadow_ready(false), _debug_unshaded(false) {
	for (int i = 0; i < 3; i++) {
		_ambient[i] = 0;
	}
	GLOBAL_DEF("rendering/quality/fixed_function/antialiased_lines", true);
	GLOBAL_DEF("rendering/quality/fixed_function/per_pixel_lighting", true);
	GLOBAL_DEF("rendering/quality/fixed_function/normal_mapping", true);
	GLOBAL_DEF("rendering/quality/fixed_function/emission", true);
	GLOBAL_DEF("rendering/quality/fixed_function/texture_occlusion", true);
	GLOBAL_DEF("rendering/quality/fixed_function/retained_geometry", true);
	GLOBAL_DEF("rendering/quality/fixed_function/native_lighting", 2);
	ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/fixed_function/native_lighting", PropertyInfo(Variant::INT, "rendering/quality/fixed_function/native_lighting", PROPERTY_HINT_ENUM, "Disabled,Exact (Directional and Ambient),All Lights (Approximate Falloff)"));
	GLOBAL_DEF("rendering/quality/fixed_function/directional_shadows", true);
	GLOBAL_DEF("rendering/quality/directional_shadow/size", 1024);
}
