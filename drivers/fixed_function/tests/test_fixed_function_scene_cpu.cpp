#include "test_fixed_function_scene_cpu.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "drivers/fixed_function/geometry_data.h"
#include "drivers/fixed_function/tests/test_canvas_skinning.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>
#include <string.h>

#include <limits>

#define FF_CPU_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Scene CPU FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionSceneCPU::_prepare(FixedFunctionSceneRenderer &p_renderer, const Vector<RID> &p_lights) {
	p_renderer._directional_lights.clear();
	p_renderer._local_lights.clear();
	FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	environment.ambient = Color(0.25, 0.25, 0.25);
	return p_renderer._prepare_lighting(Transform3D(), environment, p_lights.ptr(), p_lights.size()) == OK;
}

bool TestFixedFunctionSceneCPU::_generated_normals() {
	FixedFunctionSceneRenderer::FFDrawPacket packet;
	packet.surface.source.format = RSE::ARRAY_FORMAT_VERTEX;
	packet.surface.source.vertex_count = 4;
	packet.surface.vertex_stride = 12;
	packet.flip_v = packet.antialiased = false;
	const float positions[] = { -1, -1, 0, 1, -1, 0, 0, 1, 0, 0, -1, 1 };
	FF_CPU_CHECK(packet.surface.source.vertex_data.resize(sizeof(positions)) == OK);
	{
		uint8_t *data = packet.surface.source.vertex_data.ptrw();
		memcpy(data, positions, sizeof(positions));
	}
	Vector<FixedFunctionLightingVertex> vertices;
	Vector<uint32_t> indices;
	FF_CPU_CHECK(vertices.resize(4) == OK && indices.resize(6) == OK);
	{
		FixedFunctionLightingVertex *output = vertices.ptrw();
		uint32_t *output_indices = indices.ptrw();
		const uint32_t order[] = { 0, 2, 1, 0, 1, 3 };
		memcpy(output_indices, order, sizeof(order));
		for (int i = 0; i < 4; i++) {
			const FixedFunctionLightingVertex value = { positions[3 * i], positions[3 * i + 1], positions[3 * i + 2], i ? 0.0f : 0.6f, 0, i ? 0.0f : 0.8f, i * 0.25f, i * 0.125f, 1, 0.25f, 0.5f, 0.75f };
			output[i] = value;
		}
	}
	const Vector<FixedFunctionLightingVertex> source = vertices;
	const Vector<uint32_t> source_indices = indices;
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_generate_geometry_normals(packet, vertices, indices) == OK);
	FF_CPU_CHECK(vertices.size() == 5 && indices.size() == 6);
	{
		const FixedFunctionLightingVertex *original = source.ptr();
		const FixedFunctionLightingVertex *output = vertices.ptr();
		const uint32_t *input_indices = source_indices.ptr();
		const uint32_t *output_indices = indices.ptr();
		FF_CPU_CHECK(output_indices[0] == output_indices[3] && output_indices[2] != output_indices[4]);
		for (int i = 0; i < 6; i++) {
			const FixedFunctionLightingVertex &v = output[output_indices[i]];
			const FixedFunctionLightingVertex &old = original[input_indices[i]];
			FF_CPU_CHECK(v.x == old.x && v.y == old.y && v.z == old.z && v.u == old.u && v.v == old.v && v.r == old.r && v.g == old.g && v.b == old.b && v.a == old.a);
			const Vector3 normal(v.nx, v.ny, v.nz);
			Vector3 expected;
			if (input_indices[i] == 0) {
				expected = Vector3(0.6f, 0, 0.8f);
			} else if (i < 3) {
				expected = Vector3(0, 0, 1);
			} else {
				expected = Vector3(0, 1, 0);
			}
			FF_CPU_CHECK(normal == expected);
		}
		FF_CPU_CHECK(original[1].nz == 0 && source.size() == 4 && source_indices.size() == 6);
	}
	// Invalid indices must leave both caller arrays unchanged.
	indices = source_indices;
	indices.write[5] = 99;
	vertices = source;
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_generate_geometry_normals(packet, vertices, indices) == ERR_INVALID_DATA && vertices.size() == 4 && indices[5] == 99);
	// Nonindexed and degenerate triangles remain valid. Missing normals on a
	// degenerate face produce zero, preserving the established ambient-only result.
	packet.surface.source.vertex_count = 3;
	vertices.resize(3);
	indices = Vector<uint32_t>();
	{
		FixedFunctionLightingVertex *output = vertices.ptrw();
		for (int i = 0; i < 3; i++) {
			output[i].nx = output[i].ny = output[i].nz = 0;
		}
	}
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_generate_geometry_normals(packet, vertices, indices) == OK && vertices.size() == 3 && indices.size() == 3);
	{
		const FixedFunctionLightingVertex *output = vertices.ptr();
		for (int i = 0; i < 3; i++) {
			FF_CPU_CHECK(output[i].nz == -1);
		}
	}
	{
		uint8_t *data = packet.surface.source.vertex_data.ptrw();
		memset(data, 0, packet.surface.source.vertex_data.size());
		FixedFunctionLightingVertex *output = vertices.ptrw();
		for (int i = 0; i < 3; i++) {
			output[i].x = output[i].y = output[i].z = output[i].nx = output[i].ny = output[i].nz = 0;
		}
	}
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_generate_geometry_normals(packet, vertices, indices) == OK);
	{
		const FixedFunctionLightingVertex *output = vertices.ptr();
		for (int i = 0; i < 3; i++) {
			FF_CPU_CHECK(output[i].nx == 0 && output[i].ny == 0 && output[i].nz == 0);
		}
	}
	// Generated corners keep full-width indices; only the device partitioner
	// applies its local 16-bit limits.
	packet.surface.source.vertex_count = 4;
	{
		uint8_t *data = packet.surface.source.vertex_data.ptrw();
		memcpy(data, positions, sizeof(positions));
	}
	vertices = source;
	{
		FixedFunctionLightingVertex *data = vertices.ptrw();
		for (int i = 0; i < 4; i++) {
			data[i].nx = data[i].ny = data[i].nz = 0;
		}
	}
	FF_CPU_CHECK(indices.resize(65538) == OK);
	{
		uint32_t *data = indices.ptrw();
		const uint32_t *order = source_indices.ptr();
		for (int i = 0; i < indices.size(); i++) {
			data[i] = order[i % 6];
		}
	}
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_generate_geometry_normals(packet, vertices, indices) == OK && vertices.size() == 65538 && indices[65537] > 65535);
	Vector<FixedFunctionGeometryData::FFGeometryBatch> batches;
	FF_CPU_CHECK(FixedFunctionGeometryData::partition(vertices, indices, 65535, UINT32_MAX, batches) == OK && batches.size() == 2 && batches[0].lighting_vertices.size() == 65535 && batches[1].lighting_vertices.size() == 3);
	printf("PASS: CPU generated face corners, authored sharing, UV/color preservation, degenerates, atomic failure and full-width index partitioning.\n");
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_geometry_upload_fits(3, sizeof(FixedFunctionLightingVertex)));
	FF_CPU_CHECK(!FixedFunctionSceneRenderer::_geometry_upload_fits(uint64_t(SIZE_MAX) / sizeof(uint32_t) / 2 + 1, sizeof(uint32_t)));
	FF_CPU_CHECK(!FixedFunctionSceneRenderer::_geometry_upload_fits(uint64_t(INT32_MAX) + 1, sizeof(FixedFunctionLightingVertex)) && !FixedFunctionSceneRenderer::_geometry_upload_fits(1, 0));
	return true;
}

bool TestFixedFunctionSceneCPU::_occlusion_sampling() {
	const uint8_t texels[] = { 0, 200, 20, 255, 64, 100, 40, 128, 128, 0, 60, 64, 192, 50, 80, 0 };
	FixedFunctionSpatialMaterial::FFOcclusionChannel channel;
	channel.weights = Color(1, 0, 0, 0);
	FixedFunctionSceneRenderer::FFOcclusionSample sample;
	sample.size = Size2i(2, 2);
	sample.rgba = texels;
	sample.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U;
	sample.single_channel = FixedFunctionSceneRenderer::_get_occlusion_sample_channel(channel.weights);
	FF_CPU_CHECK(sample.single_channel == 0);
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(0.5, 0.5)) == float(96.0 / 255));
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(0, 0.25)) == float(32.0 / 255));
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(1, 0.25)) == float(32.0 / 255));
	const Vector2 fractional_uv(0.89, 0.37);
	const float single_value = FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, fractional_uv);
	sample.single_channel = -1;
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, fractional_uv) == single_value);
	sample.single_channel = 0;

	sample.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(-0.25, 0.25)) == 0);
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(3, 0.25)) == float(64.0 / 255));
	// Mirroring applies to the repeated U axis. V still clamps.
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(3, -2)) == float(64.0 / 255));
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(std::numeric_limits<real_t>::infinity(), 0)) == 1);
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(0, std::numeric_limits<real_t>::quiet_NaN())) == 1);

	sample.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U;
	channel.weights = Color(0.5, -0.25, 0, 0.25);
	sample.single_channel = FixedFunctionSceneRenderer::_get_occlusion_sample_channel(channel.weights);
	FF_CPU_CHECK(sample.single_channel == -1);
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(-0.25, 0.25)) == float(39.0 / 255));
	channel.weights = Color(0, 0, 0, 1);
	sample.single_channel = FixedFunctionSceneRenderer::_get_occlusion_sample_channel(channel.weights);
	FF_CPU_CHECK(sample.single_channel == 3);
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(1, 0.25)) == 1);

	sample.size = Size2i(1, 1);
	sample.sampler_flags |= FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR;
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(sample, channel, Vector2(0, 0)) == 1);
	return true;
}

bool TestFixedFunctionSceneCPU::_falloff() {
	FixedFunctionSceneRenderer::FFLocalLight light = {};
	light.range = 4;
	light.attenuation = 1;
	light.direction = Vector3(0, 0, -1);
	light.cos_spot_angle = Math::cos(Math::PI / 4);
	light.spot_attenuation = 1;
	const Vector3 normal(0, 0, 1);
	FF_CPU_CHECK(Math::is_equal_approx(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -2), normal), real_t(0.5)));
	FF_CPU_CHECK(Math::is_equal_approx(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -1), normal), real_t(0.75)));
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -4), normal) == 0);
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(), normal) == 0);
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -2), -normal) == 0);
	light.spot = true;
	FF_CPU_CHECK(Math::is_equal_approx(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -2), normal), real_t(0.5)));
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(3, 0, -1), normal) == 0);
	light.attenuation = 2;
	FF_CPU_CHECK(Math::is_equal_approx(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -2), normal), real_t(0.25)));
	light.spot_attenuation = 0;
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -2), normal) == 0);

	light.attenuation = light.spot_attenuation = 1;
	const Vector3 positions[] = { Vector3(1, 0.5, -2), Vector3(3, 0, -1), Vector3(0, 0, -4), Vector3() };
	for (int spot = 0; spot < 2; spot++) {
		light.spot = spot != 0;
		for (const Vector3 &position : positions) {
			light.direct_distance = false;
			const real_t scaled = FixedFunctionSceneRenderer::_local_light_diffuse(light, position, normal);
			light.direct_distance = true;
			const real_t direct = FixedFunctionSceneRenderer::_local_light_diffuse(light, position, normal);
			FF_CPU_CHECK(Math::is_equal_approx(direct, scaled));
		}
	}
	// The direct squared length can overflow or underflow while the falloff is well defined.
	const real_t ranges[] = { std::numeric_limits<real_t>::max(), std::numeric_limits<real_t>::min() * 4 };
	for (const real_t range : ranges) {
		light.range = range;
		FF_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, -range / 2), normal) == real_t(0.5));
	}
	FF_CPU_CHECK(FixedFunctionSceneRenderer::_local_light_diffuse(light, Vector3(0, 0, std::numeric_limits<real_t>::infinity()), normal) == 0);
	return true;
}

bool TestFixedFunctionSceneCPU::_run(bool p_benchmark) {
	FF_CPU_CHECK(_texture_adaptation() && _occlusion_sampling());
	FF_CPU_CHECK(TestFixedFunctionCanvasSkinning::cpu());
	FF_CPU_CHECK(_generated_normals() && _falloff());
	FFUnusedDevice unused;
	FixedFunctionRenderContext context(unused, nullptr, 128, 96, false);
	FixedFunctionRenderTargetManager targets(context);
	FixedFunctionSceneRenderer renderer(context, targets);
	{
		// A large source mesh can use small local indices, including hash collisions
		// and a new batch after the generation counter wraps.
		FixedFunctionSceneRenderer::FFDrawPacket packet;
		packet.surface.source.index_count = 96;
		const uint32_t sources[] = { 65538, 32769, 0 };
		uint32_t corners[96];
		for (int i = 0; i < 96; i++) {
			corners[i] = sources[i % 3];
		}
		renderer._batch_index_generation = UINT16_MAX;
		for (int batch = 0; batch < 2; batch++) {
			FF_CPU_CHECK(renderer._remap_batch(packet, nullptr, corners, 0, 96) == 3);
			for (int i = 0; i < 96; i++) {
				const uint16_t local = renderer._batch_draw_indices[i];
				FF_CPU_CHECK(local == i % 3 && renderer._batch_source_indices[local] == corners[i]);
			}
			corners[0] = 32769;
			corners[1] = 65538;
			for (int i = 3; i < 96; i++) {
				corners[i] = corners[i % 3];
			}
		}
	}
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	PackedVector3Array positions, normals;
	positions.push_back(Vector3(-1, -1, 0));
	positions.push_back(Vector3(0, 1, 0));
	positions.push_back(Vector3(1, -1, 0));
	for (int i = 0; i < 3; i++) {
		normals.push_back(Vector3(0, 0, 1));
	}
	arrays[RSE::ARRAY_VERTEX] = positions;
	arrays[RSE::ARRAY_NORMAL] = normals;
	RenderingServerTypes::SurfaceData data;
	FF_CPU_CHECK(RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&data, RSE::PRIMITIVE_TRIANGLES, arrays) == OK);
	const RID mesh = context.get_mesh_storage().mesh_create();
	FF_CPU_CHECK(context.get_mesh_storage().mesh_add_surface(mesh, data) == OK);
	FixedFunctionSceneRenderer::FFDrawPacket packet{};
	FF_CPU_CHECK(context.get_mesh_storage().mesh_surface_get_data(mesh, 0, packet.surface) == OK);
	packet.view_from_model.origin.z = -2;
	packet.layer_mask = 1;
	packet.antialiased = packet.flip_v = false;
	const uint32_t indices[] = { 0, 1, 2 };
	const uint8_t *bytes = packet.surface.source.vertex_data.ptr();
	{
		const real_t invalid_values[] = { std::numeric_limits<real_t>::quiet_NaN(), std::numeric_limits<real_t>::infinity(), -std::numeric_limits<real_t>::infinity() };
		for (const real_t invalid : invalid_values) {
			FixedFunctionSceneRenderer::FFDrawPacket invalid_packet = packet;
			FixedFunctionSceneRenderer::FFSpatialVertex vertex;
			invalid_packet.material.albedo.uv_offset.x = invalid;
			FF_CPU_CHECK(FixedFunctionSceneRenderer::_read_vertex(invalid_packet, bytes, 0, vertex) == ERR_INVALID_PARAMETER);
			invalid_packet.material.albedo.uv_offset.x = 0;
			invalid_packet.material.albedo.factor.g = float(invalid);
			FF_CPU_CHECK(FixedFunctionSceneRenderer::_read_vertex(invalid_packet, bytes, 0, vertex) == ERR_INVALID_PARAMETER);
			invalid_packet.material.albedo.uv_offset.x = invalid;
			invalid_packet.material.is_fallback = true;
			FF_CPU_CHECK(FixedFunctionSceneRenderer::_read_vertex(invalid_packet, bytes, 0, vertex) == OK);
			FF_CPU_CHECK(vertex.uv == Vector2() && vertex.color == Color(1, 0, 1));
			invalid_packet.view_from_model.origin.z = invalid;
			FF_CPU_CHECK(FixedFunctionSceneRenderer::_read_vertex(invalid_packet, bytes, 0, vertex) == ERR_INVALID_PARAMETER);
		}
		const real_t native_limit = std::numeric_limits<float>::max();
		FixedFunctionVertex packed;
		FF_CPU_CHECK(FixedFunctionSceneRenderer::_pack_vertex_coordinates(Vector3(native_limit, -native_limit, 0), Vector2(), packed) == OK);
		FF_CPU_CHECK(packed.x == native_limit && packed.y == -native_limit);
		if (std::numeric_limits<real_t>::max() > native_limit) {
			FixedFunctionSceneRenderer::FFDrawPacket wide_packet = packet;
			FixedFunctionSceneRenderer::FFSpatialVertex vertex;
			wide_packet.material.albedo.uv_offset.x = std::numeric_limits<real_t>::max();
			FF_CPU_CHECK(FixedFunctionSceneRenderer::_read_vertex(wide_packet, bytes, 0, vertex) == OK);
			FF_CPU_CHECK(FixedFunctionSceneRenderer::_pack_vertex_coordinates(vertex.position, vertex.uv, packed) == ERR_INVALID_PARAMETER);
		}
	}
	FixedFunctionLightStorage &storage = context.get_light_storage();
	RID instances[3];
	for (int i = 0; i < 3; i++) {
		const RID light = storage.light_create(static_cast<RSE::LightType>(i));
		FixedFunctionLightStorage::FFLightData light_data;
		FF_CPU_CHECK(storage.light_get_data(light, light_data) == OK);
		light_data.parameters[RSE::LIGHT_PARAM_RANGE] = 5;
		FF_CPU_CHECK(storage.light_set_data(light, light_data) == OK);
		instances[i] = storage.light_instance_create(light);
	}
	FixedFunctionLightStorage::FFLightInstanceData directional;
	FixedFunctionLightStorage::FFLightData original;
	FF_CPU_CHECK(storage.light_instance_get_data(instances[0], directional) == OK && storage.light_get_data(directional.light, original) == OK);
	FixedFunctionLightStorage::FFLightData physical = original;
	physical.parameters[RSE::LIGHT_PARAM_ENERGY] = 2;
	physical.parameters[RSE::LIGHT_PARAM_INTENSITY] = float(Math::PI * 4);
	FF_CPU_CHECK(storage.light_set_data(directional.light, physical) == OK);
	FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	environment.ambient = Color(0.5, 0.25, 0.125);
	environment.ambient_energy = 2;
	environment.exposure_normalization = 0.5;
	ProjectSettings::get_singleton()->set("rendering/lights_and_shadows/use_physical_light_units", true);
	const Error prepared = renderer._prepare_lighting(Transform3D(), environment, instances, 1);
	ProjectSettings::get_singleton()->set("rendering/lights_and_shadows/use_physical_light_units", false);
	FF_CPU_CHECK(prepared == OK && renderer._directional_lights.size() == 1);
	FF_CPU_CHECK(Math::is_equal_approx(renderer._directional_lights[0].radiance[0], 4.0) && renderer._ambient[0] == 0.5);
	FF_CPU_CHECK(storage.light_set_data(directional.light, original) == OK);
	{
		Vector<RID> local_lights;
		local_lights.push_back(instances[RSE::LIGHT_OMNI]);
		FF_CPU_CHECK(_prepare(renderer, local_lights) && renderer._local_lights.size() == 1);
		FixedFunctionSceneRenderer::FFDrawPacket filtered = packet;
		FF_CPU_CHECK(filtered.surface.position_bounds.valid);
		FF_CPU_CHECK(filtered.surface.position_bounds.minimum == Vector3(-1, -1, 0));
		FF_CPU_CHECK(filtered.surface.position_bounds.maximum == Vector3(1, 1, 0));
		filtered.view_from_model.basis.scale(Vector3(-2, 3, 1));
		renderer._filter_local_lights(filtered);
		FF_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.size() == 1);
		FixedFunctionSceneRenderer::FFSpatialVertex reference_vertex{ Vector3(0, 0, -2), Vector2(), Color(1, 1, 1, 0.5) };
		FixedFunctionSceneRenderer::FFSpatialVertex filtered_vertex = reference_vertex;
		renderer._shade_vertex(packet, Vector3(0, 0, 1), reference_vertex);
		renderer._shade_vertex(filtered, Vector3(0, 0, 1), filtered_vertex);
		FF_CPU_CHECK(reference_vertex.color == filtered_vertex.color);
		filtered.view_from_model.origin.x = 100;
		renderer._filter_local_lights(filtered);
		FF_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.is_empty());
		filtered.view_from_model.origin.x = 0;
		filtered.layer_mask = 0;
		renderer._filter_local_lights(filtered);
		FF_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.is_empty());
	}
	{
		const RID spot = instances[RSE::LIGHT_SPOT];
		FixedFunctionLightStorage::FFLightInstanceData instance;
		FixedFunctionLightStorage::FFLightData light;
		FF_CPU_CHECK(storage.light_instance_get_data(spot, instance) == OK && storage.light_get_data(instance.light, light) == OK);
		light.parameters[RSE::LIGHT_PARAM_SPOT_ANGLE] = 45;
		FF_CPU_CHECK(storage.light_set_data(instance.light, light) == OK);
		Vector<RID> spot_lights;
		spot_lights.push_back(spot);
		FF_CPU_CHECK(_prepare(renderer, spot_lights) && renderer._local_lights.size() == 1);
		FF_CPU_CHECK(renderer._local_lights[0].direct_distance);

		FixedFunctionSceneRenderer::FFDrawPacket filtered = packet;
		renderer._filter_local_lights(filtered);
		FF_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.size() == 1);
		FF_CPU_CHECK(renderer._has_direct_lights(filtered));
		// This box overlaps the light's range sphere but lies outside its cone.
		filtered.view_from_model.origin.x = 4;
		renderer._filter_local_lights(filtered);
		FF_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.is_empty());
		FF_CPU_CHECK(!renderer._has_direct_lights(filtered) && renderer._has_direct_lights(packet));
		for (const Vector3 &position : positions) {
			FixedFunctionSceneRenderer::FFSpatialVertex reference_vertex{ filtered.view_from_model.xform(position), Vector2(), Color(1, 1, 1, 0.5) };
			FixedFunctionSceneRenderer::FFSpatialVertex filtered_vertex = reference_vertex;
			renderer._shade_vertex(packet, Vector3(0, 0, 1), reference_vertex);
			renderer._shade_vertex(filtered, Vector3(0, 0, 1), filtered_vertex);
			FF_CPU_CHECK(reference_vertex.color == filtered_vertex.color);
		}
		filtered.view_from_model.origin = Vector3(0, 0, 2);
		renderer._filter_local_lights(filtered);
		FF_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.is_empty());
		// The near side is outside the cone, but the far side of the rotated box overlaps it.
		filtered.view_from_model = Transform3D(Basis(Vector3(0, 1, 0), Math::PI / 2), Vector3(2, 0, -2));
		renderer._filter_local_lights(filtered);
		FF_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.size() == 1);
		// Keep exact boundary cases for the per-vertex test.
		filtered.view_from_model.basis = Basis::from_scale(Vector3());
		renderer._filter_local_lights(filtered);
		FF_CPU_CHECK(filtered.local_lights_filtered && filtered.local_light_indices.size() == 1);
		const float attenuation = light.parameters[RSE::LIGHT_PARAM_ATTENUATION];
		light.parameters[RSE::LIGHT_PARAM_ATTENUATION] = 2;
		FF_CPU_CHECK(storage.light_set_data(instance.light, light) == OK && _prepare(renderer, spot_lights));
		FF_CPU_CHECK(renderer._local_lights.size() == 1 && !renderer._local_lights[0].direct_distance);
		light.parameters[RSE::LIGHT_PARAM_ATTENUATION] = attenuation;
		FF_CPU_CHECK(storage.light_set_data(instance.light, light) == OK);
	}
	// Low bits alone give the opposite tie result; keep the full 4.x RID identity.
	FixedFunctionSceneRenderer::FFPixelLightCandidate selected[1];
	FixedFunctionSceneRenderer::FFPixelLightCandidate candidate;
	candidate.score = 1;
	candidate.index = 0;
	candidate.local = false;
	candidate.instance_id = 0x200000001ULL;
	uint32_t count = 0;
	FixedFunctionSceneRenderer::_select_pixel_light(selected, count, 1, candidate);
	candidate.instance_id = 0x100000002ULL;
	FixedFunctionSceneRenderer::_select_pixel_light(selected, count, 1, candidate);
	FF_CPU_CHECK(count == 1 && selected[0].instance_id == 0x100000002ULL);

	Vector<RID> reuse_lights;
	reuse_lights.push_back(instances[0]);
	FF_CPU_CHECK(_prepare(renderer, reuse_lights));
	packet.material.lighting_flags = 0;
	FixedFunctionVertex reference[3];
	FF_CPU_CHECK(renderer._lit_triangle(packet, bytes, indices, reference) == OK);
	// Indices 0 and 2 collide in a two-entry cache. Reversal exercises hits
	// and replacement while keeping authored normals.
	FixedFunctionSceneRenderer::FFLitVertexCacheEntry cache[2];
	cache[0].index = cache[1].index = UINT32_MAX;
	FixedFunctionVertex cached[3];
	FF_CPU_CHECK(renderer._cached_lit_triangle(packet, bytes, indices, cached, nullptr, cache, 1) == OK && memcmp(cached, reference, sizeof(cached)) == 0);
	FF_CPU_CHECK(cache[0].index == 2 && cache[1].index == 1);
	const uint32_t reversed[] = { 2, 1, 0 };
	FF_CPU_CHECK(renderer._cached_lit_triangle(packet, bytes, reversed, cached, nullptr, cache, 1) == OK && cache[0].index == 0);
	for (int i = 0; i < 3; i++) {
		FF_CPU_CHECK(memcmp(&cached[i], &reference[reversed[i]], sizeof(FixedFunctionVertex)) == 0);
	}

	// The same missing deformed normal must still follow each face's winding.
	FixedFunctionSceneRenderer::FFDrawPacket mixed_packet = packet;
	FF_CPU_CHECK(mixed_packet.deformed.resize(3) == OK);
	for (int i = 0; i < 3; i++) {
		mixed_packet.deformed.write[i].position = positions[i];
		mixed_packet.deformed.write[i].normal = i == 0 ? Vector3() : normals[i];
	}
	cache[0].index = cache[1].index = UINT32_MAX;
	const uint32_t faces[][3] = { { 1, 2, 0 }, { 2, 1, 0 } };
	uint32_t face_colors[2];
	for (int face = 0; face < 2; face++) {
		FF_CPU_CHECK(renderer._lit_triangle(mixed_packet, bytes, faces[face], reference) == OK);
		FF_CPU_CHECK(renderer._cached_lit_triangle(mixed_packet, bytes, faces[face], cached, nullptr, cache, 1) == OK && memcmp(cached, reference, sizeof(cached)) == 0);
		face_colors[face] = cached[2].color;
	}
	FF_CPU_CHECK(face_colors[0] != face_colors[1] && cache[0].index == 2);
	{
		FF_CPU_CHECK(renderer._has_direct_lights(mixed_packet));
		// Mask out the directional light. The ambient result still uses AO and vertex alpha state.
		mixed_packet.layer_mask = 0;
		mixed_packet.material.albedo.factor = Color(0.8, 0.5, 0.2, 0.3);
		mixed_packet.material.state.blend_mode = FixedFunctionGraphicsDevice::BLEND_MODE_MIX;
		mixed_packet.instance_opacity = 0.75f;
		const uint8_t rgba[] = { 128, 255, 255, 255 };
		const FixedFunctionSceneRenderer::FFOcclusionSample occlusion{ rgba, Size2i(1, 1), 0, 0 };
		FF_CPU_CHECK(!renderer._has_direct_lights(mixed_packet));
		for (const uint8_t flags : { uint8_t(0), uint8_t(FixedFunctionSpatialMaterial::LIGHTING_AMBIENT_DISABLED) }) {
			mixed_packet.material.lighting_flags = flags;
			FF_CPU_CHECK(renderer._lit_triangle(mixed_packet, bytes, indices, reference, &occlusion) == OK);
			FF_CPU_CHECK(renderer._pack_ambient_indexed_vertices(mixed_packet, bytes, nullptr, 3, cached, &occlusion) == OK);
			FF_CPU_CHECK(memcmp(cached, reference, sizeof(cached)) == 0);
			FF_CPU_CHECK(renderer._pack_ambient_indexed_vertices(mixed_packet, bytes, reversed, 3, cached, &occlusion) == OK);
			for (int i = 0; i < 3; i++) {
				FF_CPU_CHECK(memcmp(&cached[i], &reference[reversed[i]], sizeof(FixedFunctionVertex)) == 0);
			}
		}
	}

	const char *names[] = { "unshaded", "directional_1", "omni_1", "spot_1", "mixed_8", "pixel_upload", "pixel_select_32" };
	for (int profile = 0; profile < 7; profile++) {
		Vector<RID> active;
		if (profile >= 1 && profile <= 3) {
			active.push_back(instances[profile - 1]);
		} else if (profile == 4 || profile == 6) {
			for (int i = 0; i < (profile == 4 ? 8 : 32); i++) {
				active.push_back(instances[1 + i % 2]);
			}
		}
		FF_CPU_CHECK(_prepare(renderer, active));
		packet.material.lighting_flags = profile == 0 ? FixedFunctionSpatialMaterial::LIGHTING_UNSHADED : 0;
		Vector<uint64_t> samples;
		uint32_t checksum = 0;
		for (int sample = 0; sample < (p_benchmark ? 10 : 1); sample++) {
			const uint64_t start = OS::get_singleton()->get_ticks_usec();
			for (int triangle = 0; triangle < (p_benchmark ? 256 : 1); triangle++) {
				FixedFunctionVertex output[3];
				if (profile == 0) {
					for (int vertex = 0; vertex < 3; vertex++) {
						FF_CPU_CHECK(FixedFunctionSceneRenderer::_vertex(packet, bytes, vertex, output[vertex]) == OK);
					}
				} else if (profile == 5) {
					FixedFunctionLightingVertex lit[3];
					FF_CPU_CHECK(FixedFunctionSceneRenderer::_lighting_triangle(packet, bytes, indices, lit) == OK);
					FF_CPU_CHECK(lit[0].nz > 0.99f && lit[0].z == -2);
					checksum += uint32_t(lit[triangle % 3].nz * 255);
					continue;
				} else if (profile == 6) {
					FixedFunctionGraphicsDevice::FFPixelLighting lighting;
					FF_CPU_CHECK(renderer._prepare_pixel_lighting(packet, lighting) && lighting.light_count == 8);
					checksum += lighting.light_count;
					continue;
				} else {
					FF_CPU_CHECK(renderer._lit_triangle(packet, bytes, indices, output) == OK);
				}
				FF_CPU_CHECK(output[0].z == -2 && (output[0].color >> 24) == 255);
				if (profile == 0 || profile == 1) {
					FF_CPU_CHECK(output[0].color == 0xffffffff);
				}
				checksum += output[triangle % 3].color;
			}
			const uint64_t elapsed = OS::get_singleton()->get_ticks_usec() - start;
			if (sample) {
				samples.push_back(elapsed);
			}
		}
		if (p_benchmark) {
			samples.sort();
			printf("PERF3D_CPU4 profile=%s real_bits=%u pointer_bits=%u units=256 samples=9 median_us=%llu min_us=%llu max_us=%llu checksum=%u\n", names[profile], unsigned(sizeof(real_t) * 8), unsigned(sizeof(void *) * 8), static_cast<unsigned long long>(samples[4]), static_cast<unsigned long long>(samples[0]), static_cast<unsigned long long>(samples[8]), checksum);
		}
	}
	printf("PASS: CPU-only native mesh preparation, Lambert/pixel packing, lit vertex reuse and full-width light selection.\n");
	return true;
}

bool TestFixedFunctionSceneCPU::test(bool p_benchmark) {
	const bool had_pixel = ProjectSettings::get_singleton()->has_setting("rendering/quality/fixed_function/per_pixel_lighting");
	const bool had_physical = ProjectSettings::get_singleton()->has_setting("rendering/lights_and_shadows/use_physical_light_units");
	const bool previous_pixel = had_pixel ? bool(GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting")) : true;
	const bool previous_physical = had_physical ? bool(GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) : false;
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/per_pixel_lighting", true);
	ProjectSettings::get_singleton()->set("rendering/lights_and_shadows/use_physical_light_units", false);
	const bool result = _run(p_benchmark);
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/per_pixel_lighting", previous_pixel);
	ProjectSettings::get_singleton()->set("rendering/lights_and_shadows/use_physical_light_units", previous_physical);
	if (!had_pixel) {
		ProjectSettings::get_singleton()->clear("rendering/quality/fixed_function/per_pixel_lighting");
	}
	if (!had_physical) {
		ProjectSettings::get_singleton()->clear("rendering/lights_and_shadows/use_physical_light_units");
	}
	return result;
}

#undef FF_CPU_CHECK

#include "drivers/fixed_function/tests/test_geometry.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_fixed_function_scene_cpu)

TEST_CASE("[FixedFunction][CPU] Indexed geometry partitioning") {
	CHECK(TestFixedFunctionGeometry::partition());
}

TEST_CASE("[FixedFunction][CPU] Render timestamp capture") {
	CHECK(TestFixedFunctionSceneCPU::test_timestamps());
}

TEST_CASE("[FixedFunction][CPU] Visibility notifier and default query contracts") {
	CHECK(TestFixedFunctionSceneCPU::test_resource_contracts());
}

TEST_CASE("[FixedFunction][CPU] Scene preparation and light ranking") {
	// No native window/device is initialized, including when requesting timings.
	CHECK(TestFixedFunctionSceneCPU::test(OS::get_singleton()->get_cmdline_args().find("--fixed-function-cpu-benchmark") != nullptr));
}

TEST_CASE("[FixedFunction][CPU] Deformed tangent frames") {
	CHECK(TestFixedFunctionSceneCPU::test_deformed_tangents());
}
