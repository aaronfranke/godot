#include "scene_renderer.h"

#include <float.h>

FixedFunctionVertex FixedFunctionSceneRenderer::_sky_vertex(int p_column, int p_row, uint32_t p_color) {
	const real_t u = real_t(p_column) / int(SKY_COLUMNS);
	const real_t v = real_t(p_row) / int(SKY_ROWS);
	// U=1 and U=0 share positions but keep their respective UVs at the seam.
	const real_t longitude = p_column == SKY_COLUMNS ? 0 : u * real_t(Math_TAU);
	const real_t latitude = v * real_t(Math_PI);
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

Error FixedFunctionSceneRenderer::_prepare_sky_vertices(uint32_t p_color) {
	if (!_sky_vertices.empty()) {
		if (_sky_color != p_color) {
			FixedFunctionVertex *vertices = _sky_vertices.ptrw();
			for (int i = 0; i < _sky_vertices.size(); i++) {
				vertices[i].color = p_color;
			}
			_sky_color = p_color;
		}
		return OK;
	}
	const Error error = _sky_vertices.resize(SKY_COLUMNS * (SKY_ROWS - 1) * 6);
	if (error != OK) {
		return error;
	}
	FixedFunctionVertex *vertices = _sky_vertices.ptrw();
	for (int row = 0; row < SKY_ROWS; row++) {
		for (int column = 0; column < SKY_COLUMNS; column++) {
			const FixedFunctionVertex top_left = _sky_vertex(column, row, p_color);
			FixedFunctionVertex top_right = _sky_vertex(column + 1, row, p_color);
			FixedFunctionVertex bottom_left = _sky_vertex(column, row + 1, p_color);
			const FixedFunctionVertex bottom_right = _sky_vertex(column + 1, row + 1, p_color);
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
	_sky_color = p_color;
	return OK;
}

Error FixedFunctionSceneRenderer::_submit_sky(const Transform &p_camera, const CameraMatrix &p_projection, RID p_environment) {
	if (_shadow_pass) {
		return OK;
	}
	// Transparent viewports retain environment lighting without drawing its background.
	if (_render_targets.is_render_target_transparent()) {
		return OK;
	}
	Basis orientation;
	float fov, energy;
	FixedFunctionSceneStorage &scenes = _context.get_scene_storage();
	RID panorama = scenes.environment_get_panorama(p_environment, orientation, fov, energy);
	if (!panorama.is_valid()) {
		return OK;
	}
	FixedFunctionTextureStorage &textures = _context.get_texture_storage();
	FixedFunctionGraphicsDevice &device = _context.get_device();
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = device.get_capabilities();
	if (textures.texture_get_render_target(panorama).is_valid()) {
		return OK; // Panorama backgrounds use ordinary 2D images.
	}
	RID native_texture;
	Error error = OK;
	if (!caps.sky_solid_hemispheres) {
		error = textures.texture_get_device_texture(panorama, native_texture);
		if (error != OK && !textures.texture_can_upload_on_use(panorama)) {
			_redraw_requested |= error == ERR_SKIP;
			return OK;
		}
	}
	error = _render_targets.begin_draw();
	if (error != OK) {
		return error;
	}
	// Native begin can recreate backing resources and run window callbacks.
	panorama = scenes.environment_get_panorama(p_environment, orientation, fov, energy);
	if (!panorama.is_valid() || textures.texture_get_render_target(panorama).is_valid()) {
		return OK;
	}
	// Use the same near-plane view as GLES for orthogonal cameras. The identity
	// projection used by the server's camera-less pass has no valid near plane.
	CameraMatrix projection = p_projection;
	if (p_projection.is_orthogonal() || fov > 0) {
		const real_t near_plane = p_projection.get_z_near();
		const Vector2 half_extents = p_projection.get_viewport_half_extents();
		if (!(near_plane > 0 && half_extents.x > 0 && half_extents.y > 0)) {
			return OK;
		}
		const real_t view_fov = fov > 0 ? real_t(fov) : Math::rad2deg(real_t(2) * Math::atan2(half_extents.y, near_plane));
		// Sky clipping only needs the X, Y and W rows.
		projection.set_perspective(MIN(view_fov, real_t(179.9)), half_extents.x / half_extents.y, 0.1, 2);
	}
	const float brightness = CLAMP(energy, 0.0f, 1.0f);
	if (caps.sky_solid_hemispheres) {
		Color sky, ground;
		if (!textures.texture_get_panorama_hemispheres(panorama, sky, ground)) {
			return OK;
		}
		// Camera Z points backward, so its normalized Y component is -sin(pitch).
		// Perspective horizon displacement uses tan(pitch); roll and sky orientation are ignored.
		const real_t pitch_sine = CLAMP(-p_camera.basis.get_axis(2).normalized().y, real_t(-1), real_t(1));
		real_t horizon_y = 1;
		if (pitch_sine <= -1) {
			horizon_y = 0;
		} else if (pitch_sine < 1) {
			const real_t pitch_tangent = pitch_sine / Math::sqrt(1 - pitch_sine * pitch_sine);
			horizon_y = (1 + projection.matrix[2][1] + projection.matrix[1][1] * pitch_tangent) * real_t(0.5);
		}
		const Color sky_color(sky.r * brightness, sky.g * brightness, sky.b * brightness);
		const Color ground_color(ground.r * brightness, ground.g * brightness, ground.b * brightness);
		uint32_t draw_calls = 0;
		error = device.draw_sky_hemispheres(horizon_y, sky_color.to_argb32(), ground_color.to_argb32(), draw_calls);
		if (error == OK) {
			_context.get_storage_queries().note_scene_draw(0, false, false, draw_calls);
		}
		// An unavailable background still permits the scene and canvas to draw.
		return error == ERR_UNAVAILABLE ? OK : error;
	}
	error = textures.texture_get_or_upload_device_texture(panorama, native_texture);
	if (error != OK) {
		_redraw_requested |= error == ERR_SKIP;
		return OK;
	}
	// Project directions around the camera origin. All sky fragments use the same
	// clip depth, so neither camera translation nor scene near/far planes matter.
	CameraMatrix clip_from_sky = projection * CameraMatrix(Transform(p_camera.basis.inverse() * orientation, Vector3()));
	for (int column = 0; column < 4; column++) {
		clip_from_sky.matrix[column][2] = 0;
		for (int row = 0; row < 4; row++) {
			const real_t value = clip_from_sky.matrix[column][row];
			if (!(value >= -FLT_MAX && value <= FLT_MAX)) {
				return OK;
			}
		}
	}
	error = _prepare_sky_vertices(Color(brightness, brightness, brightness).to_argb32());
	if (error != OK) {
		return error;
	}
	const uint32_t batch = MIN(caps.max_primitives_per_draw, uint32_t(MAX_BATCH_PRIMITIVES)) * 3;
	if (!batch) {
		return OK;
	}
	FixedFunctionGraphicsDevice::FFDrawParameters state;
	state.texture_alpha_enabled = false;
	state.sampler_flags = textures.texture_get_sampler_flags(panorama) & FixedFunctionGraphicsDevice::SAMPLER_MIPMAPS;
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
		_context.get_storage_queries().note_scene_draw(count, false, false);
	}
	return OK;
}
