#include "drivers/fixed_function/tests/test_canvas_item_renderer.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>

#define FF_CANVAS_MESH_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Canvas mesh FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionCanvasItemRenderer::_test_canvas_meshes(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionRenderContext &p_context) {
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	arrays[RSE::ARRAY_VERTEX] = Vector<Vector2>{ Vector2(0, 0), Vector2(0, 12), Vector2(14, 0), Vector2(14, 12) };
	arrays[RSE::ARRAY_INDEX] = Vector<int>{ 0, 1, 2, 3 };
	RenderingServerTypes::SurfaceData surface;
	FF_CANVAS_MESH_CHECK(RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&surface, RSE::PRIMITIVE_TRIANGLE_STRIP, arrays) == OK);
	const RID mesh = meshes.mesh_create();
	FF_CANVAS_MESH_CHECK(meshes.mesh_add_surface(mesh, surface) == OK);
	const RID multimesh = meshes.multimesh_allocate();
	meshes.multimesh_initialize(multimesh);
	FF_CANVAS_MESH_CHECK(meshes.multimesh_allocate_data(multimesh, 3, true, true, true) == OK && meshes.multimesh_set_mesh(multimesh, mesh) == OK);
	FF_CANVAS_MESH_CHECK(meshes.multimesh_set_transform_2d(multimesh, 0, Transform2D(0, Vector2(30, 12))) == OK);
	FF_CANVAS_MESH_CHECK(meshes.multimesh_set_transform_2d(multimesh, 1, Transform2D(0, Vector2(50, 12))) == OK);
	FF_CANVAS_MESH_CHECK(meshes.multimesh_set_channel(multimesh, 0, Color(1, 1, 0), false) == OK && meshes.multimesh_set_channel(multimesh, 1, Color(0, 1, 1), false) == OK);
	FF_CANVAS_MESH_CHECK(meshes.multimesh_set_channel(multimesh, 1, Color(1, 0, 1), true) == OK && meshes.multimesh_get_channel(multimesh, 1, true) == Color(1, 0, 1));
	FF_CANVAS_MESH_CHECK(meshes.multimesh_get_buffer(multimesh).size() == 48 && meshes.multimesh_get_aabb(multimesh).position.x == 30);

	FFCanvasFixture fixture(p_renderer);
	const RID item = fixture.create_item();
	fixture.server.canvas_item_add_mesh(item, mesh, Transform2D(0, Vector2(10, 12)), Color(1, 0, 0), RID());
	fixture.server.canvas_item_add_multimesh(item, multimesh, RID());
	const Vector<Point2> line = { Point2(10, 40.5), Point2(30.5, 40.5), Point2(30.5, 55) };
	FF_CANVAS_MESH_CHECK(fixture.add_polygon(item, Vector<int>(), line, Vector<Color>{ Color(1, 1, 1) }, RSE::PRIMITIVE_LINE_STRIP));
	RendererCanvasRender::Item *draw = fixture.server.canvas_item_owner.get_or_null(item);
	std::vector<uint32_t> pixels(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	for (int visible = 2; visible >= 1; visible--) {
		FF_CANVAS_MESH_CHECK(meshes.multimesh_set_visible_instances(multimesh, visible) == OK);
		FF_CANVAS_MESH_CHECK(p_manager.begin_frame(0) == OK && p_manager.clear_render_target(Color(0, 0, 0)) == OK);
		FF_CANVAS_MESH_CHECK(p_renderer.render_items(draw, Color(1, 1, 1)) == OK && p_manager.end_frame(false) == OK);
		FF_CANVAS_MESH_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
		FF_CANVAS_MESH_CHECK(_check_pixel(pixels, 16, 18, 0xffff0000) && _check_pixel(pixels, 36, 18, 0xffffff00));
		FF_CANVAS_MESH_CHECK(_check_pixel(pixels, 56, 18, visible == 2 ? 0xff00ffff : 0xff000000));
		FF_CANVAS_MESH_CHECK(_check_pixel(pixels, 18, 40, 0xffffffff) && _check_pixel(pixels, 30, 49, 0xffffffff));
	}
	meshes.multimesh_free(multimesh);
	FF_CANVAS_MESH_CHECK(meshes.free(mesh) == OK);
	printf("PASS: canvas Mesh and MultiMesh triangle strips, native 2D transforms and channels, visible count and joined hairlines.\n");
	return true;
}

#undef FF_CANVAS_MESH_CHECK
