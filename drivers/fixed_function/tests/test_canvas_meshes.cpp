#include "test_canvas_item_renderer.h"

#include <stdio.h>
#include <string.h>

#define FF_CANVAS_MESH_CHECK(m_condition)                                        \
	do {                                                                         \
		if (!(m_condition)) {                                                    \
			printf("Canvas mesh FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                        \
		}                                                                        \
	} while (false)

bool TestFixedFunctionCanvasItemRenderer::_test_meshes(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionRenderContext &p_context) {
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	VisualServer *server = VS::get_singleton();
	const RID source = server->mesh_create(), mesh = meshes.mesh_create(), multi = meshes.multimesh_create(), texture = textures.texture_create();
	Array arrays;
	arrays.resize(VS::ARRAY_MAX);
	PoolVector<Vector2> points, uvs;
	const Vector2 quad[] = { Vector2(0, 0), Vector2(8, 0), Vector2(0, 8), Vector2(8, 8) };
	for (int i = 0; i < 4; i++) {
		points.push_back(quad[i]);
		uvs.push_back(quad[i] / 8);
	}
	arrays[VS::ARRAY_VERTEX] = points;
	arrays[VS::ARRAY_TEX_UV] = uvs;
	server->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_TRIANGLE_STRIP, arrays);
	FF_CANVAS_MESH_CHECK(meshes.mesh_add_surface(mesh, server->mesh_surface_get_format(source, 0), VS::PRIMITIVE_TRIANGLE_STRIP,
								 server->mesh_surface_get_array(source, 0), 4, PoolVector<uint8_t>(), 0, AABB(Vector3(), Vector3(8, 8, 0)),
								 Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
	server->free(source);
	FF_CANVAS_MESH_CHECK(textures.texture_set_data(texture, _make_image()) == OK);
	FF_CANVAS_MESH_CHECK(meshes.multimesh_allocate(multi, 3, VS::MULTIMESH_COLOR_8BIT, VS::MULTIMESH_CUSTOM_DATA_FLOAT, VS::MULTIMESH_TRANSFORM_2D) == OK);
	FF_CANVAS_MESH_CHECK(meshes.multimesh_set_mesh(multi, mesh) == OK);
	float entries[3][13] = {
		{ 1, 0, 0, 32, 0, 1, 0, 12, 0, 0.25f, 0, 0, 0 },
		{ 0, 0, 0, 80, 0, 0, 0, 12, 0, 0, 0, 0, 0 },
		{ -1, 0, 0, 56, 0, 1, 0, 12, 0, 0, 0, 0, 0 },
	};
	const uint8_t red[] = { 255, 0, 0, 255 };
	const uint8_t green[] = { 0, 255, 0, 255 };
	memcpy(&entries[0][8], red, sizeof(red));
	memcpy(&entries[2][8], green, sizeof(green));
	PoolVector<float> bulk;
	FF_CANVAS_MESH_CHECK(bulk.resize(39) == OK);
	{
		PoolVector<float>::Write write = bulk.write();
		memcpy(write.ptr(), entries, sizeof(entries));
	}
	FF_CANVAS_MESH_CHECK(meshes.multimesh_set_bulk(multi, bulk) == OK);
	FF_CANVAS_MESH_CHECK(meshes.multimesh_get_color(multi, 0, true).r == 0.25f);
	FF_CANVAS_MESH_CHECK(FixedFunctionMeshStorage::multimesh_to_transform_2d(meshes.multimesh_get_transform(multi, 2)).get_origin() == Vector2(56, 12));
	AABB bounds;
	FF_CANVAS_MESH_CHECK(meshes.multimesh_get_aabb(multi, bounds) == OK && bounds.position == Vector3(32, 12, 0) && bounds.size == Vector3(24, 8, 0));

	// This native fixture owns a standalone storage, separate from the engine's
	// canvas RID registry. The public SceneTree fixture exercises backlinks.
	struct FFLocalMultiMesh : RasterizerCanvas::Item::CommandMultiMesh {
		~FFLocalMultiMesh() { multimesh = RID(); }
	};
	RasterizerCanvas::Item item;
	item.final_transform.set_origin(Vector2(2, 3));
	RasterizerCanvas::Item::CommandMesh *command = memnew(RasterizerCanvas::Item::CommandMesh);
	command->mesh = mesh;
	command->texture = texture;
	command->transform.set_origin(Vector2(8, 8));
	command->modulate = Color(1, 1, 1);
	item.commands.push_back(command);
	FFLocalMultiMesh *copies = memnew(FFLocalMultiMesh);
	copies->multimesh = multi;
	copies->texture = texture;
	item.commands.push_back(copies);
	FixedFunctionGraphicsDevice &device = p_context.get_device();
	std::vector<uint32_t> pixels(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	const auto draw = [&]() {
		return p_manager.begin_frame(0) == OK && p_manager.clear_render_target(Color(0, 0, 0)) == OK &&
				p_renderer.render_items(&item, Color(1, 1, 1)) == OK && p_manager.end_frame(false) == OK &&
				device.read_frame(pixels.data(), pixels.size()) == OK;
	};
	FF_CANVAS_MESH_CHECK(meshes.multimesh_set_visible(multi, 2) == OK && draw());
	FF_CANVAS_MESH_CHECK(_check_pixel(pixels, 11, 12, 0xffff0000) && _check_pixel(pixels, 16, 17, 0xffffffff));
	FF_CANVAS_MESH_CHECK(_check_pixel(pixels, 40, 21, 0xffff0000) && _check_pixel(pixels, 52, 21, 0xff000000));
	FF_CANVAS_MESH_CHECK(meshes.multimesh_set_visible(multi, -1) == OK && draw());
	FF_CANVAS_MESH_CHECK(_check_pixel(pixels, 52, 21, 0xff00ff00) && _check_pixel(pixels, 56, 21, 0xff000000));
	FF_CANVAS_MESH_CHECK(meshes.free(multi) == OK && meshes.free(mesh) == OK && textures.free(texture) == OK);
	printf("PASS: canvas Mesh and MultiMesh strip drawing, packed 2D transforms and colors, visible counts and collapsed copies.\n");
	return true;
}

#undef FF_CANVAS_MESH_CHECK
