#include "drivers/fixed_function/scene_renderer.h"

#include <float.h>
#include <string.h>

FixedFunctionVertex FixedFunctionSceneRenderer::_sky_vertex(int p_column, int p_row, uint32_t p_color) {
	const real_t u = real_t(p_column) / int(SKY_COLUMNS);
	const real_t v = real_t(p_row) / int(SKY_ROWS);
	// U=1 and U=0 share positions but keep their respective UVs at the seam.
	const real_t longitude = p_column == SKY_COLUMNS ? 0 : u * real_t(Math::TAU);
	const real_t latitude = v * real_t(Math::PI);
	Vector3 position;
	if (p_row == 0 || p_row == SKY_ROWS) {
		position.y = p_row == 0 ? 1 : -1;
	} else {
		const real_t ring = Math::sin(latitude);
		position = Vector3(ring * Math::sin(longitude), Math::cos(latitude), -ring * Math::cos(longitude));
	}
	const FixedFunctionVertex vertex = { float(position.x), float(position.y), float(position.z), p_color, float(u), float(v) };
	return vertex;
}

Color FixedFunctionSceneRenderer::_sample_sky_gradient(const FFSkyBackground &p_sky, real_t p_height) {
	const bool above = p_height >= 0;
	const float curve = above ? p_sky.sky_curve : p_sky.ground_curve;
	const float t = Math::pow(CLAMP(1.0f - float(Math::abs(p_height)), 0.0f, 1.0f), MAX(curve, 0.001f));
	return above ? p_sky.top.lerp(p_sky.horizon, t) : p_sky.ground.lerp(p_sky.ground_horizon, t);
}

bool FixedFunctionSceneRenderer::get_sky_ambient(const FFSkyBackground &p_sky, Color &r_color) {
	if (p_sky.panorama.is_valid()) {
		return _context.get_texture_storage().texture_get_panorama_average(p_sky.panorama, r_color);
	}
	if (!p_sky.procedural) {
		return false;
	}
	// Equal steps in the unit sphere's height cover equal solid angles.
	r_color = Color(0, 0, 0);
	for (int row = 0; row < SKY_ROWS; row++) {
		const Color sample = _sample_sky_gradient(p_sky, (real_t(2) * row + 1) / int(SKY_ROWS) - 1);
		for (int channel = 0; channel < 3; channel++) {
			if (Math::is_finite(sample[channel])) {
				r_color[channel] += CLAMP(sample[channel], 0.0f, 1.0f) / int(SKY_ROWS);
			}
		}
	}
	return true;
}

Error FixedFunctionSceneRenderer::_prepare_sky_vertices(const FFSkyBackground &p_sky) {
	const bool build_vertices = _sky_vertices.is_empty();
	if (build_vertices) {
		const Error error = _sky_vertices.resize(SKY_COLUMNS * (SKY_ROWS - 1) * 6);
		if (error != OK) {
			return error;
		}
	}
	uint32_t colors[SKY_ROWS + 1];
	for (int row = 0; row <= SKY_ROWS; row++) {
		Color color(1, 1, 1);
		if (p_sky.procedural) {
			const real_t y = row == SKY_ROWS / 2 ? 0 : Math::cos(real_t(row) * Math::PI / int(SKY_ROWS));
			color = _sample_sky_gradient(p_sky, y);
		}
		for (int channel = 0; channel < 3; channel++) {
			color[channel] = Math::is_finite(color[channel]) ? CLAMP(color[channel] * p_sky.energy, 0.0f, 1.0f) : 0;
		}
		color.a = 1;
		colors[row] = color.to_argb32();
	}
	if (!build_vertices && memcmp(_sky_row_colors, colors, sizeof(colors)) == 0) {
		return OK;
	}
	memcpy(_sky_row_colors, colors, sizeof(colors));
	FixedFunctionVertex *vertices = _sky_vertices.ptrw();
	if (!build_vertices) {
		for (int i = 0; i < _sky_vertices.size(); i++) {
			const int row = int(Math::round(vertices[i].v * int(SKY_ROWS)));
			vertices[i].color = colors[row];
		}
		return OK;
	}

	for (int row = 0; row < SKY_ROWS; row++) {
		for (int column = 0; column < SKY_COLUMNS; column++) {
			const FixedFunctionVertex top_left = _sky_vertex(column, row, colors[row]);
			FixedFunctionVertex top_right = _sky_vertex(column + 1, row, colors[row]);
			FixedFunctionVertex bottom_left = _sky_vertex(column, row + 1, colors[row + 1]);
			const FixedFunctionVertex bottom_right = _sky_vertex(column + 1, row + 1, colors[row + 1]);
			// Skip degenerate triangles at the poles and center the pole UV in its wedge.
			if (row == 0) {
				top_right.u = (top_left.u + top_right.u) * 0.5f;
			}
			if (row == SKY_ROWS - 1) {
				bottom_left.u = (bottom_left.u + bottom_right.u) * 0.5f;
			}
			if (row != 0) {
				*vertices++ = top_left;
				*vertices++ = bottom_left;
				*vertices++ = top_right;
			}
			if (row != SKY_ROWS - 1) {
				*vertices++ = top_right;
				*vertices++ = bottom_left;
				*vertices++ = bottom_right;
			}
		}
	}
	return OK;
}

Error FixedFunctionSceneRenderer::render_sky(const Transform3D &p_camera, const Projection &p_projection, const FFSkyBackground &p_sky, RenderingServerTypes::RenderInfo *r_info) {
	_redraw_requested = false;
	// Transparent targets keep environment lighting without drawing a background.
	if (_render_targets.is_render_target_transparent()) {
		return OK;
	}
	if (!p_sky.procedural && !p_sky.panorama.is_valid()) {
		return OK;
	}
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	FixedFunctionGraphicsDevice &device = _context.get_device();
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = device.get_capabilities();
	if (p_sky.panorama.is_valid() && (!textures.owns_texture(p_sky.panorama) || textures.texture_get_render_target(p_sky.panorama).is_valid())) {
		return OK;
	}
	Error error = _render_targets.begin_draw();
	if (error != OK) {
		return error;
	}
	// Use the same near-plane view as GLES for orthogonal cameras. The identity
	// projection used by the server's camera-less pass has no valid near plane.
	Projection projection = p_projection;
	if (p_projection.is_orthogonal() || p_sky.fov > 0) {
		const real_t near_plane = p_projection.get_z_near();
		const Vector2 half_extents = p_projection.get_viewport_half_extents();
		if (!(near_plane > 0 && half_extents.x > 0 && half_extents.y > 0)) {
			return OK;
		}
		const real_t view_fov = p_sky.fov > 0 ? real_t(p_sky.fov) : Math::rad_to_deg(real_t(2) * Math::atan2(half_extents.y, near_plane));
		// Sky clipping only needs the X, Y and W rows.
		projection.set_perspective(MIN(view_fov, real_t(179.9)), half_extents.x / half_extents.y, 0.1, 2);
	}
	const float brightness = MAX(p_sky.energy, 0.0f);
	if (caps.sky_solid_hemispheres) {
		Color sky, ground;
		if (p_sky.procedural) {
			// Integral of (1 - height)^curve over a hemisphere with equal solid-angle heights.
			sky = p_sky.top.lerp(p_sky.horizon, 1.0f / (MAX(p_sky.sky_curve, 0.001f) + 1.0f));
			ground = p_sky.ground.lerp(p_sky.ground_horizon, 1.0f / (MAX(p_sky.ground_curve, 0.001f) + 1.0f));
		} else if (!textures.texture_get_panorama_hemispheres(p_sky.panorama, sky, ground)) {
			return OK;
		}
		// Camera Z points backward, so its normalized Y component is -sin(pitch).
		// Perspective horizon displacement uses tan(pitch); roll and sky orientation are ignored.
		const real_t pitch_sine = CLAMP(-p_camera.basis.get_column(2).normalized().y, real_t(-1), real_t(1));
		real_t horizon_y = 1;
		if (pitch_sine <= -1) {
			horizon_y = 0;
		} else if (pitch_sine < 1) {
			const real_t pitch_tangent = pitch_sine / Math::sqrt(1 - pitch_sine * pitch_sine);
			horizon_y = (1 + projection.columns[2][1] + projection.columns[1][1] * pitch_tangent) * real_t(0.5);
		}
		for (int channel = 0; channel < 3; channel++) {
			sky[channel] = Math::is_finite(sky[channel]) ? CLAMP(sky[channel] * brightness, 0.0f, 1.0f) : 0;
			ground[channel] = Math::is_finite(ground[channel]) ? CLAMP(ground[channel] * brightness, 0.0f, 1.0f) : 0;
		}
		sky.a = ground.a = 1;
		uint32_t draw_calls = 0;
		error = device.draw_sky_hemispheres(horizon_y, sky.to_argb32(), ground.to_argb32(), draw_calls);
		if (error == OK && r_info) {
			int &count = r_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME];
			count = int(MIN(int64_t(INT32_MAX), int64_t(count) + draw_calls));
		}
		// An unavailable background still permits the scene and canvas to draw.
		return error == ERR_UNAVAILABLE ? OK : error;
	}
	// Project directions around the camera origin. All sky fragments use the same
	// clip depth, so neither camera translation nor scene near/far planes matter.
	Projection clip_from_sky = projection * Projection(Transform3D(p_camera.basis.inverse() * p_sky.orientation, Vector3()));
	for (int column = 0; column < 4; column++) {
		clip_from_sky.columns[column][2] = 0;
		for (int row = 0; row < 4; row++) {
			const real_t value = clip_from_sky.columns[column][row];
			if (!(value >= -FLT_MAX && value <= FLT_MAX)) {
				return OK;
			}
		}
	}
	error = _prepare_sky_vertices(p_sky);
	if (error != OK) {
		return error;
	}
	const uint32_t batch = MIN(caps.max_primitives_per_draw, uint32_t(MAX_BATCH_PRIMITIVES)) * 3;
	if (!batch) {
		return OK;
	}
	RID native_texture;
	if (p_sky.panorama.is_valid()) {
		error = textures.texture_get_or_upload_device_texture(p_sky.panorama, native_texture);
		if (error != OK) {
			_redraw_requested = error == ERR_SKIP;
			return OK;
		}
	}
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.texture_alpha_enabled = false;
	state.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
	if (caps.sampler_filter_linear) {
		state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
	}
	if (caps.sampler_repeat && caps.sampler_address_independent) {
		state.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U;
	}
	for (uint32_t offset = 0; offset < uint32_t(_sky_vertices.size()); offset += batch) {
		const uint32_t count = MIN(batch, uint32_t(_sky_vertices.size()) - offset);
		error = device.draw_triangles(_sky_vertices.ptr() + offset, count, clip_from_sky, native_texture, state);
		if (error != OK) {
			return error == ERR_UNAVAILABLE && offset == 0 ? OK : error;
		}
		if (r_info) {
			int *info = r_info->info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE];
			info[RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] = int(MIN(uint64_t(INT32_MAX), uint64_t(info[RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME]) + count / 3));
			info[RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] = int(MIN(int64_t(INT32_MAX), int64_t(info[RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]) + 1));
		}
	}
	return OK;
}
