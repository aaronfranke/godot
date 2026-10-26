#include "drivers/fixed_function/scene_renderer.h"

#include <float.h>

#include <limits>

Error FixedFunctionSceneRenderer::_submit_emission(const FFDrawPacket &p_packet, const Projection &p_projection, RID p_albedo_texture, uint32_t p_batch_primitives, RenderingServerTypes::RenderInfo *r_info, bool p_retained) {
	const FixedFunctionSpatialMaterial::FFEmissionChannel &emission = p_packet.material.emission;
	const bool cutout = p_packet.material.state.alpha_cutoff_enabled;
	const bool weighted = p_packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MIX || p_packet.material.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD;
	const bool coverage = cutout || weighted;
	const bool masked = coverage && p_packet.material.state.texture_alpha_enabled && p_albedo_texture.is_valid();
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	RID native_texture;
	RID target;
	if (emission.color.texture.is_valid() && textures.owns_texture(emission.color.texture)) {
		target = textures.texture_get_render_target(emission.color.texture);
		if (!target.is_valid() || (target != _render_targets.get_current_render_target() && !textures.render_target_has_failed(target))) {
			if (target.is_valid() && !textures.render_target_is_ready(target)) {
				_redraw_requested = true;
			} else if (textures.texture_get_or_upload_device_texture(emission.color.texture, native_texture) == ERR_SKIP) {
				_redraw_requested = true;
			}
		}
	}
	if (emission.color.texture.is_valid() && !native_texture.is_valid() && !emission.additive) {
		return OK; // A missing optional multiply input contributes no emission.
	}

	FFDrawPacket pass = p_packet;
	Projection projection = p_projection;
	Transform3D view_from_anchor;
	const FixedFunctionGraphicsDevice::FFFogTransforms fog_transforms = { p_projection, view_from_anchor };
	if (p_retained) {
		// Replay the base draw's object-relative packing. Transforming vertices into
		// view space first can round the same fragment to a different depth.
		const AABB &bounds = p_packet.surface.source.aabb;
		const Vector3 anchor = bounds.position + bounds.size * real_t(0.5);
		view_from_anchor = Transform3D(p_packet.view_from_model.basis, p_packet.view_from_model.xform(anchor));
		projection = p_projection * Projection(view_from_anchor);
		pass.view_from_model = Transform3D(Basis(), -anchor);
		if (pass.material.state.fog) {
			// Match the base draw's fog transforms as well as its positions.
			pass.material.state.fog_transforms = &fog_transforms;
		}
	}
	pass.material.lighting_flags = FixedFunctionSpatialMaterial::LIGHTING_UNSHADED;
	pass.material.albedo_uses_vertex_color = false;
	pass.material.albedo_uses_vertex_alpha = coverage && p_packet.material.albedo_uses_vertex_alpha;
	pass.material.state.blend_mode = cutout && !weighted ? FixedFunctionGraphicsDevice::BLEND_MODE_ADD_UNWEIGHTED : FixedFunctionGraphicsDevice::BLEND_MODE_ADD;
	pass.material.state.depth_write_enabled = false;
	// Emission contributes light, not a second layer of surface coverage.
	pass.material.state.alpha_write_enabled = false;
	// Preserve CPU vertex/factor alpha independently of whether a texture supplies coverage.
	pass.material.state.texture_alpha_enabled = coverage;
	pass.occlusion = false;
	pass.material.albedo = emission.color;
	pass.material.state.sampler_flags = native_texture.is_valid() ? emission.color.sampler_flags : 0;
	pass.flip_v = false; // Native top-left UVs match this branch's viewport textures.
	if (!_context.get_device().get_capabilities().sampler_filter_linear) {
		pass.material.state.sampler_flags &= ~FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
	}

	// ADD is a constant plus a separately sampled color. MULTIPLY only needs the
	// texture pass. Each contribution is bounded to the native LDR modulation range.
	const int pass_count = emission.additive && native_texture.is_valid() ? 2 : 1;
	for (int i = 0; i < pass_count; i++) {
		const bool textured = native_texture.is_valid() && (!emission.additive || i == 1);
		Color color = emission.color.factor;
		if (textured && emission.additive) {
			color = Color(1, 1, 1);
		}
		for (int channel = 0; channel < 3; channel++) {
			color[channel] = float(CLAMP(double(color[channel]) * emission.energy, 0.0, 1.0));
		}
		if (color.r == 0 && color.g == 0 && color.b == 0) {
			continue;
		}
		color.a = coverage ? p_packet.material.albedo.factor.a : 1;
		pass.material.albedo.factor = color;
		const Error error = _submit_emission_triangles(pass, p_packet, projection, textured ? native_texture : RID(), masked ? p_albedo_texture : RID(), p_batch_primitives, r_info);
		if (error != OK && error != ERR_UNAVAILABLE) {
			return error;
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::_submit_emission_triangles(const FFDrawPacket &p_color, const FFDrawPacket &p_mask, const Projection &p_projection, RID p_color_texture, RID p_mask_texture, uint32_t p_batch_primitives, RenderingServerTypes::RenderInfo *r_info) {
	FixedFunctionGraphicsDevice &device = _context.get_device();
	const FixedFunctionSpatialMaterial::FFMaterialChannel &color = p_color.material.albedo;
	const FixedFunctionSpatialMaterial::FFMaterialChannel &mask = p_mask.material.albedo;
	const bool separate = p_color_texture.is_valid() && p_mask_texture.is_valid() &&
			(p_color_texture != p_mask_texture || color.uv_set != mask.uv_set || color.uv_scale != mask.uv_scale || color.uv_offset != mask.uv_offset ||
					p_color.flip_v != p_mask.flip_v || p_color.material.state.sampler_flags != p_mask.material.state.sampler_flags);
	if (separate && !device.supports_texture_mask()) {
		return ERR_UNAVAILABLE; // Keep the base and any constant Add contribution on single-texture devices.
	}
	const uint32_t triangles = FixedFunctionMeshStorage::surface_get_triangle_count(p_color.surface);
	const uint32_t capacity = MIN(triangles, p_batch_primitives) * 3;
	Vector<uint32_t> expanded;
	Error error = expanded.resize(capacity);
	if (error == OK && separate) {
		error = _emission_vertices.resize(capacity);
	}
	if (error != OK) {
		return error;
	}
	const uint8_t *vertices = p_color.surface.source.vertex_data.ptr();
	const uint8_t *indices = p_color.surface.source.index_data.ptr();
	FixedFunctionGraphicsDevice::FFDrawParameters state = p_color.material.state;
	state.texture_rgb_enabled = p_color_texture.is_valid();
	state.texture_alpha_enabled = p_mask_texture.is_valid();
	if (!p_color_texture.is_valid() && p_mask_texture.is_valid()) {
		state.sampler_flags = p_mask.material.state.sampler_flags;
	}
	for (uint32_t first = 0; first < triangles;) {
		const uint32_t count = MIN(triangles - first, p_batch_primitives) * 3;
		if (p_color.surface.source.primitive == RSE::PRIMITIVE_TRIANGLE_STRIP) {
			FixedFunctionMeshStorage::surface_expand_triangle_strip(p_color.surface, indices, first, count / 3, expanded.ptrw());
		} else {
			for (uint32_t i = 0; i < count; i++) {
				expanded.write[i] = FixedFunctionMeshStorage::surface_read_index(p_color.surface, indices, first * 3 + i);
			}
		}
		for (uint32_t i = 0; i < count; i++) {
			const uint32_t index = expanded[i];
			FixedFunctionVertex &vertex = separate ? _emission_vertices.write[i].vertex : _vertices.write[i];
			error = _vertex(p_color, vertices, index, vertex);
			if (error != OK) {
				return error;
			}
			if (p_mask_texture.is_valid()) {
				Vector2 uv = FixedFunctionMeshStorage::surface_read_uv(p_mask.surface, index, mask.uv_set) * mask.uv_scale + mask.uv_offset;
				if (p_mask.flip_v) {
					uv.y = 1 - uv.y;
				}
				for (int axis = 0; axis < 2; axis++) {
					if (Math::is_nan(uv[axis]) || Math::is_inf(uv[axis]) || uv[axis] < -FLT_MAX || uv[axis] > FLT_MAX) {
						return ERR_INVALID_PARAMETER;
					}
				}
				if (separate) {
					_emission_vertices.write[i].mask_u = float(uv.x);
					_emission_vertices.write[i].mask_v = float(uv.y);
				} else {
					vertex.u = float(uv.x);
					vertex.v = float(uv.y);
				}
			}
		}
		if (separate) {
			error = device.draw_masked_triangles(_emission_vertices.ptr(), count, p_projection, p_color_texture, p_mask_texture, p_mask.material.state.sampler_flags, state);
		} else {
			error = device.draw_triangles(_vertices.ptr(), count, p_projection, p_color_texture.is_valid() ? p_color_texture : p_mask_texture, state);
		}
		if (error != OK) {
			return error; // Optional refusal never retries contributions already submitted by this pass.
		}
		bool first_surface = false;
		_note_draw(p_color, count, first_surface, r_info);
		first += count / 3;
	}
	return OK;
}

bool FixedFunctionSceneRenderer::_get_occlusion_texture(RID p_texture, RID &r_native_texture, bool p_preflight) {
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	if (!textures.owns_texture(p_texture) || textures.texture_get_render_target(p_texture).is_valid()) {
		return false;
	}
	if (p_preflight) {
		return textures.texture_get_device_texture(p_texture, r_native_texture) == OK || textures.texture_can_upload_on_use(p_texture);
	}
	return textures.texture_get_or_upload_device_texture(p_texture, r_native_texture) == OK;
}

int8_t FixedFunctionSceneRenderer::_get_occlusion_sample_channel(const Color &p_weights) {
	int8_t channel = -1;
	for (int8_t i = 0; i < 4; i++) {
		if (p_weights[i] == 0) {
			continue;
		}
		if (channel >= 0 || p_weights[i] != 1) {
			return -1;
		}
		channel = i;
	}
	return channel;
}

float FixedFunctionSceneRenderer::_sample_occlusion(const FFOcclusionSample &p_sample, const FixedFunctionSpatialMaterial::FFOcclusionChannel &p_channel, Vector2 p_uv) {
	p_uv = p_uv * p_channel.uv_scale + p_channel.uv_offset;
	const bool linear = p_sample.sampler_flags & FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
	const int taps = linear ? 2 : 1;
	int texels[2][2];
	real_t fraction[2];
	for (int axis = 0; axis < 2; axis++) {
		const real_t coordinate = p_uv[axis];
		if (!(Math::abs(coordinate) <= std::numeric_limits<real_t>::max())) {
			return 1;
		}
		const bool repeat = p_sample.sampler_flags & (axis ? FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V : FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U);
		const bool mirror = repeat && (p_sample.sampler_flags & FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED);
		real_t uv;
		if (mirror) {
			const real_t period = coordinate >= 0 && coordinate < 2 ? coordinate : Math::fposmod(coordinate, real_t(2));
			uv = MIN(period, 2 - period);
		} else if (repeat) {
			uv = coordinate >= 0 && coordinate < 1 ? coordinate : Math::fposmod(coordinate, real_t(1));
		} else {
			uv = CLAMP(coordinate, real_t(0), real_t(1));
		}
		const int size = p_sample.size[axis];
		const real_t pixel = uv * size - (linear ? real_t(0.5) : real_t(0));
		// Addressed UVs are in [0, 1], so the filter can only reach the
		// immediate neighbors outside the image.
		const int first = pixel < 0 ? -1 : int(pixel);
		fraction[axis] = pixel - first;
		for (int tap = 0; tap < taps; tap++) {
			int index = first + tap;
			if (repeat && !mirror) {
				if (index < 0) {
					index += size;
				} else if (index >= size) {
					index -= size;
				}
			} else {
				// A mirrored seam repeats the edge texel on both sides.
				index = CLAMP(index, 0, size - 1);
			}
			texels[axis][tap] = index;
		}
	}
	if (p_sample.single_channel >= 0) {
		const size_t stride = size_t(p_sample.size.x) * 4;
		const size_t x0 = size_t(texels[0][0]) * 4;
		const uint8_t *row0 = p_sample.rgba + size_t(texels[1][0]) * stride + p_sample.single_channel;
		if (!linear) {
			return float(row0[x0] / 255.0);
		}
		const size_t x1 = size_t(texels[0][1]) * 4;
		const uint8_t *row1 = p_sample.rgba + size_t(texels[1][1]) * stride + p_sample.single_channel;
		const real_t left = 1 - fraction[0];
		const real_t top = 1 - fraction[1];
		// Preserve the weighted sampler's tap order and real_t filter weights.
		double result = double(row0[x0]) * (left * top);
		result += double(row0[x1]) * (fraction[0] * top);
		result += double(row1[x0]) * (left * fraction[1]);
		result += double(row1[x1]) * (fraction[0] * fraction[1]);
		return float(CLAMP(result / 255.0, 0.0, 1.0));
	}
	double result = 0;
	for (int y = 0; y < taps; y++) {
		const real_t wy = y ? fraction[1] : 1 - fraction[1];
		for (int x = 0; x < taps; x++) {
			const uint8_t *rgba = p_sample.rgba + (size_t(texels[1][y]) * p_sample.size.x + texels[0][x]) * 4;
			double sample = 0;
			for (int channel = 0; channel < 4; channel++) {
				sample += double(p_channel.weights[channel]) * rgba[channel];
			}
			if (linear) {
				const real_t wx = x ? fraction[0] : 1 - fraction[0];
				sample *= wx * wy;
			}
			result += sample;
		}
	}
	return float(CLAMP(result / 255.0, 0.0, 1.0));
}
