#include "core/config/project_settings.h"
#include "drivers/fixed_function/tests/test_retained_scene.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>

#define FF_TOPOLOGY_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Triangle topology FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
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
	const Vector3 strip[] = { Vector3(-0.8, -0.8, 0), Vector3(-0.8, 0.8, 0), Vector3(0.8, -0.8, 0), Vector3(0.8, 0.8, 0) };
	const uint32_t reverse[] = { 3, 2, 1, 0 };
	for (int indexed = 0; indexed < 2; indexed++) {
		Vector<Vector3> positions;
		for (int i = 0; i < 4; i++) {
			positions.push_back(strip[indexed ? reverse[i] : i]);
		}
		Array arrays;
		arrays.resize(RSE::ARRAY_MAX);
		arrays[RSE::ARRAY_VERTEX] = positions;
		if (indexed) {
			arrays[RSE::ARRAY_INDEX] = Vector<int>{ 3, 2, 1, 0 };
		}
		RenderingServerTypes::SurfaceData data;
		FF_TOPOLOGY_CHECK(RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&data, RSE::PRIMITIVE_TRIANGLE_STRIP, arrays) == OK);
		const RID mesh = meshes.mesh_create();
		FF_TOPOLOGY_CHECK(meshes.mesh_add_surface(mesh, data) == OK);
		FFInstance instance(p_context, mesh, material);
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
		FF_TOPOLOGY_CHECK(meshes.mesh_surface_get_data(mesh, 0, surface) == OK && surface.source.primitive == RSE::PRIMITIVE_TRIANGLE_STRIP && surface.source.index_count == (indexed ? 4U : 0U));
		RID native;
		Vector3 anchor, extent;
		FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
		FF_TOPOLOGY_CHECK(cache.get(surface.identity, surface.version, 0, native, info, anchor, extent) == OK && info.primitive_count == 2);
		// Expanding from an odd offset must preserve the original strip winding.
		uint32_t expanded[3];
		FixedFunctionMeshStorage::surface_expand_triangle_strip(surface, surface.source.index_data.ptr(), 1, 1, expanded);
		FF_TOPOLOGY_CHECK(expanded[0] == (indexed ? 1U : 2U) && expanded[1] == (indexed ? 2U : 1U) && expanded[2] == (indexed ? 0U : 3U));
		FF_TOPOLOGY_CHECK(meshes.free(mesh) == OK && p_context.prepare_resources() == OK);
	}
	settings->set(retained_setting, previous);
	FF_TOPOLOGY_CHECK(materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: indexed and non-indexed 3D triangle strips, back-face culling, odd batch offsets and retained/transient pixel equivalence.\n");
	return true;
}

#undef FF_TOPOLOGY_CHECK
