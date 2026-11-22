#include "test_retained_scene.h"

#include "core/project_settings.h"

#include <stdio.h>
#include <string.h>

#define FF_TOPOLOGY_CHECK(m_condition)                                                 \
	do {                                                                               \
		if (!(m_condition)) {                                                          \
			printf("Triangle topology FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                              \
		}                                                                              \
	} while (false)

bool TestFixedFunctionRetainedScene::_test_triangle_topology(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	const char *retained_setting = "rendering/quality/fixed_function/retained_geometry";
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const bool previous = settings->get(retained_setting);
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionGeometryCache &cache = p_context.get_geometry_cache();
	const RID shader = materials.shader_create(), material = materials.material_create();
	FF_TOPOLOGY_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded,cull_back; void fragment(){ ALBEDO=vec3(1,0,0); }") == OK);
	FF_TOPOLOGY_CHECK(materials.material_set_shader(material, shader) == OK);
	const float strip[][3] = { { -0.8f, -0.8f, 0 }, { -0.8f, 0.8f, 0 }, { 0.8f, -0.8f, 0 }, { 0.8f, 0.8f, 0 } };
	const float fan[][3] = { { -0.8f, -0.8f, 0 }, { -0.8f, 0.8f, 0 }, { 0.8f, 0.8f, 0 }, { 0.8f, -0.8f, 0 } };
	const uint16_t reverse[] = { 3, 2, 1, 0 };
	const VS::PrimitiveType primitives[] = { VS::PRIMITIVE_TRIANGLE_STRIP, VS::PRIMITIVE_TRIANGLE_FAN };
	const AABB bounds(Vector3(-0.8, -0.8, 0), Vector3(1.6, 1.6, 0));
	for (VS::PrimitiveType primitive : primitives) {
		const float(*positions)[3] = primitive == VS::PRIMITIVE_TRIANGLE_STRIP ? strip : fan;
		for (int indexed = 0; indexed < 2; indexed++) {
			PoolVector<uint8_t> vertices, indices;
			FF_TOPOLOGY_CHECK(vertices.resize(sizeof(strip)) == OK);
			if (indexed) {
				FF_TOPOLOGY_CHECK(indices.resize(sizeof(reverse)) == OK);
				PoolVector<uint8_t>::Write write = indices.write();
				memcpy(write.ptr(), reverse, sizeof(reverse));
			}
			{
				PoolVector<uint8_t>::Write write = vertices.write();
				for (int i = 0; i < 4; i++) {
					memcpy(write.ptr() + i * sizeof(strip[0]), positions[indexed ? reverse[i] : i], sizeof(strip[0]));
				}
			}
			const RID mesh = meshes.mesh_create();
			const uint32_t format = VS::ARRAY_FORMAT_VERTEX | (indexed ? uint32_t(VS::ARRAY_FORMAT_INDEX) : 0);
			FF_TOPOLOGY_CHECK(meshes.mesh_add_surface(mesh, format, primitive, vertices, 4, indices, indexed ? 4 : 0, bounds, Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
			FFInstance instance(mesh, material);
			FFInstance *instances[] = { &instance };
			std::vector<uint32_t> transient, retained;
			settings->set(retained_setting, false);
			FF_TOPOLOGY_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, transient));
			const uint32_t width = p_context.get_device().get_width();
			const uint32_t height = p_context.get_device().get_height();
			for (uint32_t y : { height * 3 / 8, height * 5 / 8 }) {
				for (uint32_t x : { width * 3 / 8, width * 5 / 8 }) {
					FF_TOPOLOGY_CHECK(transient[y * width + x] == 0xffff0000);
				}
			}
			settings->set(retained_setting, true);
			FF_TOPOLOGY_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, retained) && retained == transient);
			FixedFunctionMeshStorage::FFSurfaceData surface;
			FF_TOPOLOGY_CHECK(meshes.mesh_surface_get_data(mesh, 0, surface) == OK && surface.primitive == primitive && surface.index_count == (indexed ? 4 : 0));
			RID native;
			Vector3 anchor, extent;
			FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
			FF_TOPOLOGY_CHECK(cache.get(surface.identity, surface.version, 0, native, info, anchor, extent) == OK && info.primitive_count == 2);
			// Expanding from an odd offset must retain the original winding and fan center.
			uint32_t expanded[3];
			const uint32_t second = primitive == VS::PRIMITIVE_TRIANGLE_STRIP ? 1 : 2;
			{
				const PoolVector<uint8_t>::Read source = surface.index_array.read();
				FixedFunctionMeshStorage::surface_expand_triangles(surface, source.ptr(), 1, 1, expanded);
			}
			const uint32_t first = primitive == VS::PRIMITIVE_TRIANGLE_STRIP ? 2 : 0;
			FF_TOPOLOGY_CHECK(expanded[0] == (indexed ? reverse[first] : first) && expanded[1] == (indexed ? reverse[second] : second) && expanded[2] == (indexed ? 0u : 3u));
			FF_TOPOLOGY_CHECK(meshes.free(mesh) == OK && p_context.prepare_resources() == OK);
		}
	}
	settings->set(retained_setting, previous);
	FF_TOPOLOGY_CHECK(materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: indexed and non-indexed 3D triangle strips and fans, back-face culling, odd batch offsets and retained/transient pixel equivalence.\n");
	return true;
}

#undef FF_TOPOLOGY_CHECK
