#include "test_scene_cpu.h"
#include "drivers/fixed_function/geometry_data.h"
#include "test_canvas_skinning.h"
#include "test_geometry.h"
#include "test_immediate_geometry.h"

#include "core/os/os.h"
#include "core/project_settings.h"

#include <stdio.h>
#include <string.h>
#include <limits>

#define FF_SCENE_CPU_CHECK(m_condition)                                   \
	do {                                                                  \
		if (!(m_condition)) {                                             \
			printf("Scene CPU FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                 \
		}                                                                 \
	} while (false)

bool TestFixedFunctionSceneCPU::_occlusion_sampling() {
	const uint8_t texels[] = { 0, 200, 20, 255, 64, 100, 40, 128, 128, 0, 60, 64, 192, 50, 80, 0 };
	FixedFunctionSpatialMaterial::FFOcclusionChannel channel;
	channel.weights = Color(1, 0, 0, 0);
	FixedFunctionSceneRenderer::FFOcclusionSample sample;
	sample.size = Size2i(2, 2);
	sample.rgba = texels;
	sample.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U;
	sample.single_channel = FixedFunctionSceneRenderer::_get_occlusion_sample_channel(channel.weights);
	FF_SCENE_CPU_CHECK(sample.single_channel == 0);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(0.5, 0.5)) == float(96.0 / 255));
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(0, 0.25)) == float(32.0 / 255));
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(1, 0.25)) == float(32.0 / 255));
	const Vector2 fractional_uv(0.89, 0.37);
	const float single_value = FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, fractional_uv);
	sample.single_channel = -1;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, fractional_uv) == single_value);
	sample.single_channel = 0;

	sample.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(-0.25, 0.25)) == 0);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(3, 0.25)) == float(64.0 / 255));
	// Mirroring applies to the repeated U axis. V still clamps.
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(3, -2)) == float(64.0 / 255));
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(std::numeric_limits<real_t>::infinity(), 0)) == 1);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(0, std::numeric_limits<real_t>::quiet_NaN())) == 1);

	sample.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U;
	channel.weights = Color(0.5, -0.25, 0, 0.25);
	sample.single_channel = FixedFunctionSceneRenderer::_get_occlusion_sample_channel(channel.weights);
	FF_SCENE_CPU_CHECK(sample.single_channel == -1);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(-0.25, 0.25)) == float(39.0 / 255));
	channel.weights = Color(0, 0, 0, 1);
	sample.single_channel = FixedFunctionSceneRenderer::_get_occlusion_sample_channel(channel.weights);
	FF_SCENE_CPU_CHECK(sample.single_channel == 3);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(1, 0.25)) == 1);

	sample.size = Size2i(1, 1);
	sample.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(0, 0)) == 1);
	return true;
}

bool TestFixedFunctionSceneCPU::_falloff() {
	FixedFunctionSceneRenderer::FFLocalLight light;
	light.position = Vector3(0, 0, 2);
	light.direction = Vector3(0, 0, -1);
	light.range = 4;
	light.attenuation = 1;
	light.spot_attenuation = 1;
	light.cos_spot_angle = Math::cos(Math::deg2rad(real_t(45)));
	light.spot = false;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(), Vector3(0, 0, 1)) == real_t(0.5));
	light.attenuation = 2;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(), Vector3(0, 0, 1)) == real_t(0.25));
	light.attenuation = 1;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(), Vector3(0, 0, -1)) == 0);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(), Vector3()) == 0);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, 2), Vector3(0, 0, 1)) == 0);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -2), Vector3(0, 0, 1)) == 0);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -3), Vector3(0, 0, 1)) == 0);
	light.spot = true;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(), Vector3(0, 0, 1)) == real_t(0.5));
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(3, 0, 0), Vector3(0, 0, 1)) == 0);
	light.direction = Vector3(0, 0, 1);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(), Vector3(0, 0, 1)) == 0);
	light.spot = false;
	light.position.z = std::numeric_limits<real_t>::max() / 4;
	light.range = std::numeric_limits<real_t>::max() / 2;
	FF_SCENE_CPU_CHECK(Math::abs(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(), Vector3(0, 0, 1)) - real_t(0.5)) < real_t(0.000001));
	light.position.x = std::numeric_limits<real_t>::infinity();
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(), Vector3(0, 0, 1)) == 0);
	light.position = Vector3(0, 0, 2);
	light.direction = Vector3(0, 0, -1);
	light.range = 4;
	for (int spot = 0; spot < 2; spot++) {
		light.spot = spot != 0;
		for (const Vector3 &position : { Vector3(), Vector3(0.5, 0.25, 0), Vector3(2, 0, 0), Vector3(3, 0, 0) }) {
			light.direct_distance = false;
			const real_t scaled = FixedFunctionSceneRenderer::_local_light_diffuse(light, position, Vector3(0, 0, 1));
			light.direct_distance = true;
			const real_t direct = FixedFunctionSceneRenderer::_local_light_diffuse(light, position, Vector3(0, 0, 1));
			FF_SCENE_CPU_CHECK(Math::is_equal_approx(scaled, direct));
		}
	}
	light.spot = false;
	light.position = Vector3();
	for (const real_t range : { std::numeric_limits<real_t>::min() * 32, std::numeric_limits<real_t>::max() / 4 }) {
		light.range = range;
		FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -range / 2), Vector3(0, 0, 1)) == real_t(0.5));
	}
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, std::numeric_limits<real_t>::infinity()), Vector3(0, 0, 1)) == 0);
	return true;
}

bool TestFixedFunctionSceneCPU::_prepare(FixedFunctionSceneRenderer &p_renderer, const Transform &p_camera, const Vector<RID> &p_lights) {
	p_renderer._directional_lights.clear();
	p_renderer._local_lights.clear();
	for (int i = 0; i < 3; i++) {
		p_renderer._ambient[i] = 0;
	}
	return p_renderer._prepare_lighting(p_camera, RID(), p_lights.ptr(), p_lights.size()) == OK;
}

bool TestFixedFunctionSceneCPU::_snapshots(FixedFunctionRenderContext &p_context, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionLightStorage &storage = p_context.get_light_storage();
	for (int spot = 0; spot < 2; spot++) {
		const RID source = storage.light_create(spot ? VS::LIGHT_SPOT : VS::LIGHT_OMNI);
		const RID instance = storage.light_instance_create(source);
		FixedFunctionLightStorage::FFLightData data;
		FF_SCENE_CPU_CHECK(storage.light_get_data(source, data) == OK);
		data.color = Color(0, 1, 0);
		data.parameters[VS::LIGHT_PARAM_RANGE] = 4;
		data.parameters[VS::LIGHT_PARAM_ATTENUATION] = 1;
		data.parameters[VS::LIGHT_PARAM_SPOT_ATTENUATION] = 1;
		FF_SCENE_CPU_CHECK(storage.light_set_data(source, data) == OK);
		Vector<RID> active;
		active.push_back(instance);
		FixedFunctionSceneRenderer::FFDrawPacket packet{};
		packet.layer_mask = 1;
		packet.material.lighting_flags = 0;
		for (int camera = 0; camera < 4; camera++) {
			Transform view;
			if (camera) {
				view.origin = Vector3(10, -2, 1);
				view.basis = Basis(Vector3(1, 2, 3).normalized(), real_t(0.7));
				if (camera >= 2) {
					view.basis.scale(Vector3(camera == 3 ? -2 : 2, 3, real_t(0.5)));
				}
			}
			FF_SCENE_CPU_CHECK(_prepare(p_renderer, view, active) && p_renderer._local_lights.size() == 1);
			FixedFunctionSceneRenderer::FFSpatialVertex vertex;
			vertex.position = view.affine_inverse().xform(Vector3(0, 0, -2));
			vertex.color = Color(1, 1, 1, 0.375);
			p_renderer._shade_vertex(packet, Vector3(0, 0, 1), vertex);
			FF_SCENE_CPU_CHECK(Math::abs(vertex.color.g - 0.5f) < 0.0001f && vertex.color.r == 0 && vertex.color.b == 0 && vertex.color.a == 0.375f);
			FixedFunctionSceneRenderer::FFDrawPacket filtered = packet;
			filtered.surface.identity = 1;
			filtered.surface.position_min = filtered.surface.position_max = Vector3(0, 0, -2);
			filtered.view_from_model = view.affine_inverse();
			p_renderer._filter_local_lights(filtered);
			FF_SCENE_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.size() == 1);
			const Color reference = vertex.color;
			vertex.color = Color(1, 1, 1, 0.375);
			p_renderer._shade_vertex(filtered, Vector3(0, 0, 1), vertex);
			FF_SCENE_CPU_CHECK(vertex.color == reference);
			filtered.surface.position_min = filtered.surface.position_max = Vector3(100, 0, -2);
			p_renderer._filter_local_lights(filtered);
			FF_SCENE_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.empty());
			filtered.surface.position_min = filtered.surface.position_max = Vector3(0, 0, -2);
			filtered.layer_mask = 0;
			p_renderer._filter_local_lights(filtered);
			FF_SCENE_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.empty());
		}
		FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active));
		if (spot) {
			data.parameters[VS::LIGHT_PARAM_SPOT_ANGLE] = 45;
			FF_SCENE_CPU_CHECK(storage.light_set_data(source, data) == OK && _prepare(p_renderer, Transform(), active));
			FF_SCENE_CPU_CHECK(p_renderer._local_lights[0].direct_distance);
			FixedFunctionSceneRenderer::FFDrawPacket filtered = packet;
			filtered.surface.identity = 1;
			filtered.surface.position_min = Vector3(3, -0.1, -0.6);
			filtered.surface.position_max = Vector3(3.1, 0.1, -0.4);
			p_renderer._filter_local_lights(filtered);
			FF_SCENE_CPU_CHECK(filtered.local_lights_filtered && !p_renderer._has_direct_lights(filtered));
			filtered.surface.position_min = Vector3(1.8, -0.1, -2);
			filtered.surface.position_max = Vector3(2.2, 0.1, -1.9);
			p_renderer._filter_local_lights(filtered);
			FF_SCENE_CPU_CHECK(filtered.local_lights_filtered && p_renderer._has_direct_lights(filtered));
		}
		FixedFunctionSceneRenderer::FFSpatialVertex vertex;
		vertex.position = Vector3(0, 0, -2);
		vertex.color = Color(1, 1, 1, 0.375);
		packet.layer_mask = 0;
		p_renderer._shade_vertex(packet, Vector3(0, 0, 1), vertex);
		FF_SCENE_CPU_CHECK(vertex.color == Color(0, 0, 0, 0.375));
		packet.layer_mask = 1;
		// Translate camera, light and geometry together before subtracting large origins.
		Transform shifted;
		shifted.origin = Vector3(100000, -200000, 300000);
		FF_SCENE_CPU_CHECK(storage.light_instance_set_transform(instance, shifted) == OK && _prepare(p_renderer, shifted, active));
		vertex.color = Color(1, 1, 1, 0.375);
		p_renderer._shade_vertex(packet, Vector3(0, 0, 1), vertex);
		FF_SCENE_CPU_CHECK(vertex.color == Color(0, 0.5, 0, 0.375));
		data.parameters[VS::LIGHT_PARAM_ATTENUATION] = -2;
		data.parameters[VS::LIGHT_PARAM_SPOT_ANGLE] = 120;
		FF_SCENE_CPU_CHECK(storage.light_set_data(source, data) == OK && _prepare(p_renderer, shifted, active));
		FixedFunctionLightStorage::FFLightData retained;
		FF_SCENE_CPU_CHECK(storage.light_get_data(source, retained) == OK && retained.parameters[VS::LIGHT_PARAM_ATTENUATION] == -2 && retained.parameters[VS::LIGHT_PARAM_SPOT_ANGLE] == 120);
		FF_SCENE_CPU_CHECK(p_renderer._local_lights[0].attenuation == 0);
		data.parameters[VS::LIGHT_PARAM_RANGE] = 0;
		FF_SCENE_CPU_CHECK(storage.light_set_data(source, data) == OK && _prepare(p_renderer, shifted, active) && p_renderer._local_lights.empty());
		data.parameters[VS::LIGHT_PARAM_RANGE] = 4;
		data.parameters[VS::LIGHT_PARAM_SPOT_ANGLE] = 0;
		FF_SCENE_CPU_CHECK(storage.light_set_data(source, data) == OK && _prepare(p_renderer, shifted, active) && p_renderer._local_lights.size() == (spot ? 0 : 1));
		FF_SCENE_CPU_CHECK(storage.free(source) == OK && _prepare(p_renderer, shifted, active) && p_renderer._local_lights.empty());
		FF_SCENE_CPU_CHECK(storage.free(instance) == OK);
	}
	return true;
}

bool TestFixedFunctionSceneCPU::_triangles(FixedFunctionRenderContext &p_context, FixedFunctionSceneRenderer &p_renderer, bool p_benchmark) {
	{
		// A large source mesh can use small local indices, including hash collisions
		// and a new batch after the generation counter wraps.
		FixedFunctionSceneRenderer::FFDrawPacket packet;
		packet.surface.index_count = 96;
		const uint32_t sources[] = { 65538, 32769, 0 };
		uint32_t corners[96];
		for (int i = 0; i < 96; i++) {
			corners[i] = sources[i % 3];
		}
		p_renderer._batch_index_generation = UINT16_MAX;
		for (int batch = 0; batch < 2; batch++) {
			FF_SCENE_CPU_CHECK(p_renderer._remap_batch(packet, nullptr, corners, 0, 96) == 3);
			for (int i = 0; i < 96; i++) {
				const uint16_t local = p_renderer._batch_draw_indices[i];
				FF_SCENE_CPU_CHECK(local == i % 3 && p_renderer._batch_source_indices[local] == corners[i]);
			}
			corners[0] = 32769;
			corners[1] = 65538;
			for (int i = 3; i < 96; i++) {
				corners[i] = corners[i % 3];
			}
		}
	}
	// Native 3.x uncompressed, interleaved float positions/normals; CPU math remains real_t.
	const float source[] = { -1, -1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, -1, 0, 0, 0, 1 };
	const uint8_t *bytes = reinterpret_cast<const uint8_t *>(source);
	FixedFunctionSceneRenderer::FFDrawPacket packet;
	packet.surface.format = VS::ARRAY_FORMAT_VERTEX | VS::ARRAY_FORMAT_NORMAL;
	packet.surface.vertex_count = 3;
	packet.surface.offsets[VS::ARRAY_NORMAL] = 12;
	packet.surface.strides[VS::ARRAY_VERTEX] = packet.surface.strides[VS::ARRAY_NORMAL] = 24;
	packet.view_from_model.origin.z = -2;
	packet.layer_mask = 1;
	packet.material.lighting_flags = 0;
	packet.flip_v = packet.antialiased = false;
	{
		FixedFunctionSceneRenderer::FFDrawPacket invalid = packet;
		FixedFunctionSceneRenderer::FFSpatialVertex vertex;
		const real_t nan = std::numeric_limits<real_t>::quiet_NaN();
		const real_t infinity = std::numeric_limits<real_t>::infinity();
		for (const real_t value : { nan, infinity, -infinity }) {
			invalid.material.albedo.uv_offset.x = value;
			invalid.material.albedo.factor.r = float(value);
			invalid.material.is_fallback = false;
			FF_SCENE_CPU_CHECK(p_renderer._read_vertex(invalid, bytes, 0, vertex) == ERR_INVALID_PARAMETER);
			invalid.material.is_fallback = true;
			FF_SCENE_CPU_CHECK(p_renderer._read_vertex(invalid, bytes, 0, vertex) == OK && vertex.uv == Vector2() && vertex.color == Color(1, 0, 1));
		}
		FixedFunctionVertex packed;
		const real_t native_limit = std::numeric_limits<float>::max();
		FF_SCENE_CPU_CHECK(p_renderer._pack_vertex_coordinates(Vector3(native_limit, -native_limit, 0), Vector2(), packed) == OK);
		FF_SCENE_CPU_CHECK(p_renderer._pack_vertex_coordinates(Vector3(), Vector2(infinity, 0), packed) == ERR_INVALID_PARAMETER);
	}
	const uint32_t indices[] = { 0, 1, 2 };
	FixedFunctionVertex output[3];
	FixedFunctionLightStorage &storage = p_context.get_light_storage();
	const RID omni = storage.light_create(VS::LIGHT_OMNI), spot = storage.light_create(VS::LIGHT_SPOT), directional = storage.light_create(VS::LIGHT_DIRECTIONAL);
	const RID omni_instance = storage.light_instance_create(omni), spot_instance = storage.light_instance_create(spot), directional_instance = storage.light_instance_create(directional);
	for (int type = 0; type < 2; type++) {
		const RID source_light = type ? spot : omni;
		FixedFunctionLightStorage::FFLightData data;
		FF_SCENE_CPU_CHECK(storage.light_get_data(source_light, data) == OK);
		data.parameters[VS::LIGHT_PARAM_RANGE] = 4;
		data.parameters[VS::LIGHT_PARAM_ATTENUATION] = data.parameters[VS::LIGHT_PARAM_SPOT_ATTENUATION] = 1;
		FF_SCENE_CPU_CHECK(storage.light_set_data(source_light, data) == OK);
	}
	Vector<RID> active;
	active.push_back(omni_instance);
	FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active) && p_renderer._lit_triangle(packet, bytes, indices, output) == OK);
	const int expected[] = { 81, 101, 81 }; // (2 / sqrt(distance_squared) - 0.5) * 255.
	for (int vertex = 0; vertex < 3; vertex++) {
		for (int channel = 0; channel < 3; channel++) {
			FF_SCENE_CPU_CHECK(ABS(int((output[vertex].color >> (channel * 8)) & 255) - expected[vertex]) <= 1);
		}
	}
	// A two-entry cache makes indices 0 and 2 collide. Reversing the triangle
	// exercises both hits and replacement while keeping authored normals.
	FixedFunctionSceneRenderer::FFLitVertexCacheEntry cache[2];
	cache[0].index = cache[1].index = UINT32_MAX;
	FixedFunctionVertex cached[3];
	FF_SCENE_CPU_CHECK(p_renderer._cached_lit_triangle(packet, bytes, indices, cached, nullptr, cache, 1) == OK && memcmp(cached, output, sizeof(cached)) == 0);
	FF_SCENE_CPU_CHECK(cache[0].index == 2 && cache[1].index == 1);
	const uint32_t reversed[] = { 2, 1, 0 };
	FF_SCENE_CPU_CHECK(p_renderer._cached_lit_triangle(packet, bytes, reversed, cached, nullptr, cache, 1) == OK && cache[0].index == 0);
	for (int vertex = 0; vertex < 3; vertex++) {
		FF_SCENE_CPU_CHECK(memcmp(&cached[vertex], &output[reversed[vertex]], sizeof(FixedFunctionVertex)) == 0);
	}

	// The same missing normal must still follow the winding of each face.
	float mixed_source[sizeof(source) / sizeof(source[0])];
	memcpy(mixed_source, source, sizeof(source));
	mixed_source[5] = 0;
	const uint8_t *mixed_bytes = reinterpret_cast<const uint8_t *>(mixed_source);
	cache[0].index = cache[1].index = UINT32_MAX;
	const uint32_t faces[][3] = { { 1, 2, 0 }, { 2, 1, 0 } };
	uint32_t face_colors[2];
	for (int face = 0; face < 2; face++) {
		FixedFunctionVertex reference[3];
		FF_SCENE_CPU_CHECK(p_renderer._lit_triangle(packet, mixed_bytes, faces[face], reference) == OK);
		FF_SCENE_CPU_CHECK(p_renderer._cached_lit_triangle(packet, mixed_bytes, faces[face], cached, nullptr, cache, 1) == OK && memcmp(cached, reference, sizeof(cached)) == 0);
		face_colors[face] = cached[2].color;
	}
	FF_SCENE_CPU_CHECK(face_colors[0] != face_colors[1] && cache[0].index == 2);
	{
		Vector<RID> no_lights;
		FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), no_lights) && !p_renderer._has_direct_lights(packet));
		p_renderer._ambient[0] = p_renderer._ambient[1] = p_renderer._ambient[2] = 0.5;
		const uint8_t texel[] = { 128, 255, 255, 255 };
		FixedFunctionSceneRenderer::FFOcclusionSample occlusion;
		occlusion.size = Size2i(1, 1);
		occlusion.rgba = texel;
		occlusion.sampler_flags = 0;
		occlusion.single_channel = 0;
		FixedFunctionVertex reference[3];
		FF_SCENE_CPU_CHECK(p_renderer._lit_triangle(packet, mixed_bytes, indices, reference, &occlusion) == OK);
		FF_SCENE_CPU_CHECK(p_renderer._pack_ambient_indexed_vertices(packet, mixed_bytes, nullptr, 3, cached, &occlusion) == OK && memcmp(cached, reference, sizeof(cached)) == 0);
		FF_SCENE_CPU_CHECK(p_renderer._pack_ambient_indexed_vertices(packet, mixed_bytes, reversed, 3, cached, &occlusion) == OK);
		for (int vertex = 0; vertex < 3; vertex++) {
			FF_SCENE_CPU_CHECK(memcmp(&cached[vertex], &reference[reversed[vertex]], sizeof(FixedFunctionVertex)) == 0);
		}
		FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active));
	}

	packet.surface.format &= ~VS::ARRAY_FORMAT_NORMAL;
	FixedFunctionVertex flat[3];
	FF_SCENE_CPU_CHECK(p_renderer._lit_triangle(packet, bytes, indices, flat) == OK && memcmp(output, flat, sizeof(flat)) == 0);
	packet.surface.format |= VS::ARRAY_FORMAT_NORMAL;
	FixedFunctionLightingVertex unlit[3];
	FF_SCENE_CPU_CHECK(p_renderer._lighting_triangle(packet, bytes, indices, unlit) == OK);
	for (int vertex = 0; vertex < 3; vertex++) {
		FF_SCENE_CPU_CHECK(unlit[vertex].nz == 1 && unlit[vertex].r == 1 && unlit[vertex].z == -2);
	}
	// Negative lights cancel before the final clamp, including HDR albedo and alpha isolation.
	FixedFunctionLightStorage::FFLightData data;
	FF_SCENE_CPU_CHECK(storage.light_get_data(spot, data) == OK);
	data.type = VS::LIGHT_OMNI;
	const RID negative = storage.light_create(VS::LIGHT_OMNI), negative_instance = storage.light_instance_create(negative);
	data.flags |= FixedFunctionLightStorage::LIGHT_NEGATIVE;
	data.parameters[VS::LIGHT_PARAM_ENERGY] = 0.5;
	FF_SCENE_CPU_CHECK(storage.light_set_data(negative, data) == OK);
	active.push_back(negative_instance);
	packet.material.albedo.factor = Color(2, 2, 2, 0.375);
	packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
	FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active) && p_renderer._lit_triangle(packet, bytes, indices, flat) == OK);
	for (int vertex = 0; vertex < 3; vertex++) {
		FF_SCENE_CPU_CHECK((flat[vertex].color & 0xffffff) == (output[vertex].color & 0xffffff) && ABS(int(flat[vertex].color >> 24) - 96) <= 1);
	}
	packet.material.albedo.factor = Color(1, 1, 1);
	packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED;
	if (p_benchmark) {
		const char *names[] = { "unshaded", "directional_1", "omni_1", "spot_1", "mixed_8", "pixel_upload" };
		for (int profile = 0; profile < 6; profile++) {
			active.clear();
			if (profile == 1) {
				active.push_back(directional_instance);
			}
			if (profile == 2) {
				active.push_back(omni_instance);
			}
			if (profile == 3) {
				active.push_back(spot_instance);
			}
			if (profile == 4) {
				for (int i = 0; i < 4; i++) {
					active.push_back(omni_instance);
					active.push_back(spot_instance);
				}
			}
			FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active));
			Vector<uint64_t> samples;
			volatile uint32_t checksum = 0;
			for (int sample = 0; sample < 10; sample++) {
				const uint64_t start = OS::get_singleton()->get_ticks_usec();
				for (int triangle = 0; triangle < 256; triangle++) {
					if (profile == 5) {
						FF_SCENE_CPU_CHECK(p_renderer._lighting_triangle(packet, bytes, indices, unlit) == OK);
					} else if (profile == 0) {
						for (int corner = 0; corner < 3; corner++) {
							FF_SCENE_CPU_CHECK(p_renderer._vertex(packet, bytes, corner, output[corner]) == OK);
						}
					} else {
						FF_SCENE_CPU_CHECK(p_renderer._lit_triangle(packet, bytes, indices, output) == OK);
					}
					checksum += profile == 5 ? uint32_t(unlit[triangle % 3].nz * 255) : output[triangle % 3].color;
				}
				const uint64_t elapsed = OS::get_singleton()->get_ticks_usec() - start;
				if (sample) { // One warmup block, then nine samples.
					samples.push_back(elapsed);
				}
			}
			samples.sort();
			printf("PERF3D_CPU profile=%s real_bits=%u pointer_bits=%u triangles=256 samples=9 median_us=%.3f min_us=%.3f max_us=%.3f checksum=%u\n", names[profile], unsigned(sizeof(real_t) * 8), unsigned(sizeof(void *) * 8), double(samples[4]), double(samples[0]), double(samples[8]), uint32_t(checksum));
		}
	}
	FF_SCENE_CPU_CHECK(storage.free(omni_instance) == OK && storage.free(spot_instance) == OK && storage.free(directional_instance) == OK && storage.free(negative_instance) == OK);
	FF_SCENE_CPU_CHECK(storage.free(omni) == OK && storage.free(spot) == OK && storage.free(directional) == OK && storage.free(negative) == OK);
	return true;
}

bool TestFixedFunctionSceneCPU::_pixel_parameters(FixedFunctionRenderContext &p_context, FixedFunctionSceneRenderer &p_renderer, FFUnusedDevice &p_device, bool p_benchmark) {
	FixedFunctionLightStorage &storage = p_context.get_light_storage();
	const RID source = storage.light_create(VS::LIGHT_OMNI), instance = storage.light_instance_create(source);
	FixedFunctionLightStorage::FFLightData data;
	FF_SCENE_CPU_CHECK(storage.light_get_data(source, data) == OK);
	data.parameters[VS::LIGHT_PARAM_RANGE] = 4;
	data.parameters[VS::LIGHT_PARAM_ATTENUATION] = 1;
	FF_SCENE_CPU_CHECK(storage.light_set_data(source, data) == OK);
	FixedFunctionSceneRenderer::FFDrawPacket packet;
	packet.layer_mask = 1;
	packet.material.lighting_flags = 0;
	packet.view_from_model.origin.z = -2;
	packet.surface.aabb = AABB(Vector3(-1, -1, 0), Vector3(2, 2, 0));
	FixedFunctionGraphicsDevice::FFPixelLighting parameters;
	Vector<RID> active;
	active.push_back(instance);
	FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active));
	FF_SCENE_CPU_CHECK(!p_renderer._prepare_pixel_lighting(packet, parameters));
	p_device.set_pixel_limit(8);
	FF_SCENE_CPU_CHECK(p_renderer._prepare_pixel_lighting(packet, parameters) && parameters.light_count == 1);
	FF_SCENE_CPU_CHECK(parameters.lights[0].position_range[3] == 0.25f && parameters.lights[0].color_type[3] == 1);
	packet.material.lighting_flags = FixedFunctionSpatialMaterial::LIGHTING_UNSHADED;
	FF_SCENE_CPU_CHECK(!p_renderer._prepare_pixel_lighting(packet, parameters));
	packet.material.lighting_flags = 0;
	packet.material.albedo.factor.r = 2;
	FF_SCENE_CPU_CHECK(p_renderer._prepare_pixel_lighting(packet, parameters) && packet.material.albedo.factor.r == 2);
	packet.material.albedo.factor.r = 1.0e11f;
	FF_SCENE_CPU_CHECK(!p_renderer._prepare_pixel_lighting(packet, parameters) && packet.material.albedo.factor.r == 1.0e11f);
	packet.material.albedo.factor.r = 1;
	// This fixture calls the helper directly, so it supplies the render-time setting.
	p_renderer._per_pixel_lighting = false;
	FF_SCENE_CPU_CHECK(!p_renderer._prepare_pixel_lighting(packet, parameters));
	p_renderer._per_pixel_lighting = true;
	for (int i = 1; i < 8; i++) {
		active.push_back(instance);
	}
	FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active) && p_renderer._prepare_pixel_lighting(packet, parameters) && parameters.light_count == 8);
	active.push_back(instance);
	FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active) && p_renderer._prepare_pixel_lighting(packet, parameters) && parameters.light_count == 8);
	packet.layer_mask = 0;
	FF_SCENE_CPU_CHECK(p_renderer._prepare_pixel_lighting(packet, parameters) && parameters.light_count == 0);
	packet.layer_mask = 1;
	FF_SCENE_CPU_CHECK(storage.light_instance_set_transform(instance, Transform(Basis(), Vector3(100, 0, 0))) == OK);
	FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active) && p_renderer._prepare_pixel_lighting(packet, parameters) && parameters.light_count == 0);
	FF_SCENE_CPU_CHECK(storage.light_instance_set_transform(instance, Transform()) == OK);
	if (p_benchmark) {
		active.resize(32);
		for (int i = 0; i < active.size(); i++) {
			active.write[i] = instance;
		}
		FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active));
		Vector<uint64_t> samples;
		volatile uint32_t checksum = 0;
		for (int sample = 0; sample < 10; sample++) {
			const uint64_t start = OS::get_singleton()->get_ticks_usec();
			for (int object = 0; object < 256; object++) {
				FF_SCENE_CPU_CHECK(p_renderer._prepare_pixel_lighting(packet, parameters));
				checksum += parameters.light_count;
			}
			const uint64_t elapsed = OS::get_singleton()->get_ticks_usec() - start;
			if (sample) {
				samples.push_back(elapsed);
			}
		}
		samples.sort();
		printf("PERF3D_CPU profile=pixel_select_32 real_bits=%u pointer_bits=%u objects=256 samples=9 median_us=%.3f min_us=%.3f max_us=%.3f checksum=%u\n", unsigned(sizeof(real_t) * 8), unsigned(sizeof(void *) * 8), double(samples[4]), double(samples[0]), double(samples[8]), uint32_t(checksum));
	}
	active.resize(1);
	data.parameters[VS::LIGHT_PARAM_ENERGY] = std::numeric_limits<float>::max();
	FF_SCENE_CPU_CHECK(storage.light_instance_set_transform(instance, Transform()) == OK && storage.light_set_data(source, data) == OK);
	FF_SCENE_CPU_CHECK(_prepare(p_renderer, Transform(), active) && !p_renderer._prepare_pixel_lighting(packet, parameters));
	FF_SCENE_CPU_CHECK(p_context.get_device().draw_lit_triangles(nullptr, 0, CameraMatrix(), parameters, RID(), FixedFunctionGraphicsDevice::FFDrawParameters()) == ERR_UNAVAILABLE);
	p_device.set_pixel_limit(0);
	FF_SCENE_CPU_CHECK(storage.free(instance) == OK && storage.free(source) == OK);
	return true;
}

bool TestFixedFunctionSceneCPU::_light_selection() {
	FixedFunctionSceneRenderer::FFPixelLightCandidate selected[8];
	uint32_t count = 0;
	for (int i = 9; i >= 1; i--) {
		FixedFunctionSceneRenderer::FFPixelLightCandidate candidate;
		candidate.score = 1;
		candidate.instance_id = i;
		candidate.index = i;
		candidate.local = true;
		FixedFunctionSceneRenderer::_select_pixel_light(selected, count, 8, candidate);
	}
	FF_SCENE_CPU_CHECK(count == 8);
	for (int i = 0; i < 8; i++) {
		FF_SCENE_CPU_CHECK(selected[i].instance_id == uint32_t(i + 1));
	}
	FixedFunctionSceneRenderer::FFPixelLightCandidate brighter;
	brighter.score = 2;
	brighter.instance_id = 99;
	brighter.index = 99;
	brighter.local = false;
	FixedFunctionSceneRenderer::_select_pixel_light(selected, count, 8, brighter);
	FF_SCENE_CPU_CHECK(count == 8 && selected[0].instance_id == 99 && selected[7].instance_id == 7);
	FixedFunctionSceneRenderer::FFLocalLight light;
	for (int i = 0; i < 3; i++) {
		light.radiance[i] = 1;
	}
	light.position = Vector3(0, 0, 3);
	light.direction = Vector3(0, 0, -1);
	light.range = 10;
	light.attenuation = light.spot_attenuation = 1;
	light.cos_spot_angle = Math::cos(Math::deg2rad(real_t(45)));
	light.spot = false;
	const double near_score = FixedFunctionSceneRenderer::_local_light_score(light, AABB());
	FF_SCENE_CPU_CHECK(Math::abs(near_score - 0.7) < 0.000001);
	for (int i = 0; i < 3; i++) {
		light.radiance[i] = -1;
	}
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_score(light, AABB()) == near_score);
	light.position.z = 8;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_score(light, AABB()) < near_score);
	light.position.z = 3;
	light.spot = true;
	FF_SCENE_CPU_CHECK(Math::abs(FixedFunctionSceneRenderer::_local_light_score(light, AABB()) - near_score) < 0.000001);
	light.direction = Vector3(0, 0, 1);
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_score(light, AABB()) == 0);
	light.spot_attenuation = 0;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_score(light, AABB(Vector3(-5, -5, -5), Vector3(10, 10, 10))) == 0);
	return true;
}

bool TestFixedFunctionSceneCPU::_generated_normals() {
	FixedFunctionSceneRenderer::FFDrawPacket packet;
	packet.surface.format = VS::ARRAY_FORMAT_VERTEX;
	packet.surface.vertex_count = 4;
	packet.surface.strides[VS::ARRAY_VERTEX] = 12;
	packet.flip_v = packet.antialiased = false;
	const float positions[] = { -1, -1, 0, 1, -1, 0, 0, 1, 0, 0, -1, 1 };
	FF_SCENE_CPU_CHECK(packet.surface.array.resize(sizeof(positions)) == OK);
	{
		PoolVector<uint8_t>::Write data = packet.surface.array.write();
		memcpy(data.ptr(), positions, sizeof(positions));
	}
	PoolVector<FixedFunctionLightingVertex> vertices;
	PoolVector<uint32_t> indices;
	FF_SCENE_CPU_CHECK(vertices.resize(4) == OK && indices.resize(6) == OK);
	{
		PoolVector<FixedFunctionLightingVertex>::Write output = vertices.write();
		PoolVector<uint32_t>::Write output_indices = indices.write();
		const uint32_t order[] = { 0, 2, 1, 0, 1, 3 };
		memcpy(output_indices.ptr(), order, sizeof(order));
		for (int i = 0; i < 4; i++) {
			const FixedFunctionLightingVertex value = { positions[3 * i], positions[3 * i + 1], positions[3 * i + 2], i ? 0.0f : 0.6f, 0, i ? 0.0f : 0.8f, i * 0.25f, i * 0.125f, 1, 0.25f, 0.5f, 0.75f };
			output[i] = value;
		}
	}
	const PoolVector<FixedFunctionLightingVertex> source = vertices;
	const PoolVector<uint32_t> source_indices = indices;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_generate_geometry_normals(packet, vertices, indices) == OK);
	FF_SCENE_CPU_CHECK(vertices.size() == 5 && indices.size() == 6);
	{
		const PoolVector<FixedFunctionLightingVertex>::Read original = source.read(), output = vertices.read();
		const PoolVector<uint32_t>::Read input_indices = source_indices.read(), output_indices = indices.read();
		FF_SCENE_CPU_CHECK(output_indices[0] == output_indices[3] && output_indices[2] != output_indices[4]);
		for (int i = 0; i < 6; i++) {
			const FixedFunctionLightingVertex &v = output[output_indices[i]];
			const FixedFunctionLightingVertex &old = original[input_indices[i]];
			FF_SCENE_CPU_CHECK(v.x == old.x && v.y == old.y && v.z == old.z && v.u == old.u && v.v == old.v && v.r == old.r && v.g == old.g && v.b == old.b && v.a == old.a);
			const Vector3 normal(v.nx, v.ny, v.nz);
			Vector3 expected;
			if (input_indices[i] == 0) {
				expected = Vector3(0.6f, 0, 0.8f);
			} else if (i < 3) {
				expected = Vector3(0, 0, 1);
			} else {
				expected = Vector3(0, 1, 0);
			}
			FF_SCENE_CPU_CHECK(normal == expected);
		}
		FF_SCENE_CPU_CHECK(original[1].nz == 0 && source.size() == 4 && source_indices.size() == 6);
	}
	// Invalid indices must leave both caller arrays unchanged.
	indices = source_indices;
	indices.write()[5] = 99;
	vertices = source;
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_generate_geometry_normals(packet, vertices, indices) == ERR_INVALID_DATA && vertices.size() == 4 && indices.read()[5] == 99);
	// Nonindexed and degenerate triangles remain valid. Missing normals on a
	// degenerate face produce zero, preserving the established ambient-only result.
	packet.surface.vertex_count = 3;
	vertices.resize(3);
	indices = PoolVector<uint32_t>();
	{
		PoolVector<FixedFunctionLightingVertex>::Write output = vertices.write();
		for (int i = 0; i < 3; i++) {
			output[i].nx = output[i].ny = output[i].nz = 0;
		}
	}
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_generate_geometry_normals(packet, vertices, indices) == OK && vertices.size() == 3 && indices.size() == 3);
	{
		const PoolVector<FixedFunctionLightingVertex>::Read output = vertices.read();
		for (int i = 0; i < 3; i++) {
			FF_SCENE_CPU_CHECK(output[i].nz == -1);
		}
	}
	{
		PoolVector<uint8_t>::Write data = packet.surface.array.write();
		memset(data.ptr(), 0, packet.surface.array.size());
		PoolVector<FixedFunctionLightingVertex>::Write output = vertices.write();
		for (int i = 0; i < 3; i++) {
			output[i].x = output[i].y = output[i].z = output[i].nx = output[i].ny = output[i].nz = 0;
		}
	}
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_generate_geometry_normals(packet, vertices, indices) == OK);
	{
		const PoolVector<FixedFunctionLightingVertex>::Read output = vertices.read();
		for (int i = 0; i < 3; i++) {
			FF_SCENE_CPU_CHECK(output[i].nx == 0 && output[i].ny == 0 && output[i].nz == 0);
		}
	}
	// Generated corners keep full-width indices; only the device partitioner
	// applies its local 16-bit limits.
	packet.surface.vertex_count = 4;
	{
		PoolVector<uint8_t>::Write data = packet.surface.array.write();
		memcpy(data.ptr(), positions, sizeof(positions));
	}
	vertices = source;
	{
		PoolVector<FixedFunctionLightingVertex>::Write data = vertices.write();
		for (int i = 0; i < 4; i++) {
			data[i].nx = data[i].ny = data[i].nz = 0;
		}
	}
	FF_SCENE_CPU_CHECK(indices.resize(65538) == OK);
	{
		PoolVector<uint32_t>::Write data = indices.write();
		const PoolVector<uint32_t>::Read order = source_indices.read();
		for (int i = 0; i < indices.size(); i++) {
			data[i] = order[i % 6];
		}
	}
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_generate_geometry_normals(packet, vertices, indices) == OK && vertices.size() == 65538 && indices.read()[65537] > 65535);
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	FF_SCENE_CPU_CHECK(FixedFunctionGeometryData::partition(vertices, indices, 65535, UINT32_MAX, batches) == OK && batches.size() == 2 && batches[0].lighting_vertices.size() == 65535 && batches[1].lighting_vertices.size() == 3);
	printf("PASS: CPU generated face corners, authored sharing, UV/color preservation, degenerates, atomic failure and full-width index partitioning.\n");
	FF_SCENE_CPU_CHECK(FixedFunctionSceneRenderer::_geometry_upload_fits(3, sizeof(FixedFunctionLightingVertex)));
	FF_SCENE_CPU_CHECK(!FixedFunctionSceneRenderer::_geometry_upload_fits(uint64_t(SIZE_MAX) / sizeof(uint32_t) / 2 + 1, sizeof(uint32_t)));
	FF_SCENE_CPU_CHECK(!FixedFunctionSceneRenderer::_geometry_upload_fits(uint64_t(INT32_MAX) + 1, sizeof(FixedFunctionLightingVertex)) && !FixedFunctionSceneRenderer::_geometry_upload_fits(1, 0));
	return true;
}

bool TestFixedFunctionSceneCPU::test(bool p_benchmark) {
	FF_SCENE_CPU_CHECK(_texture_adaptation() && _occlusion_sampling());
	FF_SCENE_CPU_CHECK(TestFixedFunctionCanvasSkinning::cpu());
	FF_SCENE_CPU_CHECK(TestFixedFunctionImmediateGeometry::cpu());
	FFUnusedDevice unused;
	FixedFunctionRenderContext context(unused, nullptr, 128, 96, false);
	FixedFunctionRenderTargetManager targets(context);
	FixedFunctionSceneRenderer renderer(context, targets);
	FF_SCENE_CPU_CHECK(TestFixedFunctionGeometry::partition() && _generated_normals() && _deformed_tangents() && _falloff() && _snapshots(context, renderer) && _triangles(context, renderer, p_benchmark) && _pixel_parameters(context, renderer, unused, p_benchmark) && _light_selection());
	printf("PASS: CPU-only omni/spot Lambert, range/cone falloff, finite extremes, camera transforms, masks, source lifetime, packed triangles and missing normals.\n");
	return true;
}

#undef FF_SCENE_CPU_CHECK
