#include "test_lambert.h"

#include <stdio.h>
#include <string.h>
#include <limits>

#define FF_MULTIMESH_CHECK(m_condition)                                   \
	do {                                                                  \
		if (!(m_condition)) {                                             \
			printf("MultiMesh FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                 \
		}                                                                 \
	} while (false)

bool TestFixedFunctionLambert::_test_multimesh(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	const RID mesh = _mesh(meshes, 0, false);
	const RID multimesh = meshes.multimesh_create();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	const String source = "shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=COLOR.rgb; }";
	FF_MULTIMESH_CHECK(materials.shader_set_code(shader, source) == OK && materials.material_set_shader(material, shader) == OK);
	FF_MULTIMESH_CHECK(meshes.multimesh_allocate(multimesh, 3, VS::MULTIMESH_COLOR_8BIT, VS::MULTIMESH_CUSTOM_DATA_FLOAT) == OK);
	FF_MULTIMESH_CHECK(meshes.multimesh_set_mesh(multimesh, mesh) == OK);
	FF_MULTIMESH_CHECK(meshes.multimesh_get_transform(multimesh, 0) == Transform());
	FF_MULTIMESH_CHECK(meshes.multimesh_get_color(multimesh, 0, false) == Color(1, 1, 1));
	PoolVector<float> bulk;
	FF_MULTIMESH_CHECK(bulk.resize(3 * 17) == OK);
	{
		PoolVector<float>::Write data = bulk.write();
		const float rows[] = { 1, 0, 0, -1, 0, 1, 0, 0, 0, 0, 1, 0 };
		const uint8_t colors[3][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 255, 255, 255, 255 } };
		for (int i = 0; i < 3; i++) {
			float *entry = data.ptr() + i * 17;
			memcpy(entry, rows, sizeof(rows));
			memcpy(entry + 12, colors[i], sizeof(colors[i]));
			entry[13] = i;
			entry[14] = 0.5f;
			entry[15] = entry[16] = 0;
		}
		data[17] = -1; // Reflected copy still uses the material's face culling.
		data[20] = 1;
		memset(data.ptr() + 34, 0, sizeof(rows));
		data[37] = 50; // A collapsed copy must not expand bounds or submit geometry.
	}
	FF_MULTIMESH_CHECK(meshes.multimesh_set_bulk(multimesh, bulk) == OK);
	FF_MULTIMESH_CHECK(meshes.multimesh_get_color(multimesh, 1, true) == Color(1, 0.5f, 0, 0));
	{
		PoolVector<float>::Write data = bulk.write();
		data[3] = 10;
		data[20] = std::numeric_limits<float>::quiet_NaN();
	}
	FF_MULTIMESH_CHECK(meshes.multimesh_set_bulk(multimesh, bulk) == ERR_INVALID_DATA);
	FF_MULTIMESH_CHECK(meshes.multimesh_get_transform(multimesh, 0).origin.x == -1);
	FFInstance instance(multimesh, material);
	instance.base_type = VS::INSTANCE_MULTIMESH;
	FF_MULTIMESH_CHECK(meshes.instance_add_dependency(multimesh, &instance) == OK);
	const Vector<RID> lights;
	std::vector<uint32_t> pixels;
	const int left = 48 * 128 + 32;
	const int right = 48 * 128 + 96;
	FF_MULTIMESH_CHECK(meshes.multimesh_set_visible(multimesh, 1) == OK);
	AABB bounds;
	FF_MULTIMESH_CHECK(meshes.multimesh_get_aabb(multimesh, bounds) == OK && bounds.position.x + bounds.size.x < 0.01f);
	FF_MULTIMESH_CHECK(_draw(p_context, p_targets, p_renderer, instance, RID(), lights, pixels));
	FF_MULTIMESH_CHECK(_color(pixels[left], 0xffff0000) && _color(pixels[right], 0xff0000ff));
	FF_MULTIMESH_CHECK(meshes.multimesh_set_visible(multimesh, -1) == OK);
	FF_MULTIMESH_CHECK(meshes.multimesh_get_aabb(multimesh, bounds) == OK && bounds.size.x > 3.9f && bounds.size.x < 4.1f);
	FF_MULTIMESH_CHECK(_draw(p_context, p_targets, p_renderer, instance, RID(), lights, pixels));
	FF_MULTIMESH_CHECK(_color(pixels[left], 0xffff0000) && _color(pixels[right], 0xff00ff00));
	FF_MULTIMESH_CHECK(materials.shader_set_code(shader, source.replace("COLOR.rgb", "vec3(1)")) == OK);
	FF_MULTIMESH_CHECK(_draw(p_context, p_targets, p_renderer, instance, RID(), lights, pixels));
	FF_MULTIMESH_CHECK(_color(pixels[left], 0xffffffff) && _color(pixels[right], 0xffffffff));
	FF_MULTIMESH_CHECK(meshes.free(mesh) == OK && !meshes.multimesh_get_mesh(multimesh).is_valid());
	FF_MULTIMESH_CHECK(_draw(p_context, p_targets, p_renderer, instance, RID(), lights, pixels));
	FF_MULTIMESH_CHECK(_color(pixels[left], 0xff0000ff) && _color(pixels[right], 0xff0000ff));
	FF_MULTIMESH_CHECK(meshes.free(multimesh) == OK && !instance.base.is_valid());
	FF_MULTIMESH_CHECK(materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: 3D MultiMesh transforms, colors, packed bulk writes, visible bounds, mirrored drawing and resource detachment.\n");
	return true;
}

#undef FF_MULTIMESH_CHECK
