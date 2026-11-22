#include "test_canvas_skinning.h"

#include <stdio.h>
#include <limits>

#define FF_CANVAS_SKIN_CHECK(m_condition)                                       \
	do {                                                                        \
		if (!(m_condition)) {                                                   \
			printf("Canvas skinning FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                       \
		}                                                                       \
	} while (false)

bool TestFixedFunctionCanvasSkinning::_storage() {
	FixedFunctionMaterialStorage materials;
	FixedFunctionMeshStorage meshes(materials);
	const RID skeleton = meshes.skeleton_create();
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_allocate(skeleton, 2, true) == OK && meshes.skeleton_get_bone_count(skeleton) == 2);
	Vector<Transform2D> bones;
	Transform2D base;
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_get_pose_2d(skeleton, bones, base) == OK && bones[0] == Transform2D() && base == Transform2D());
	const Transform2D pose(1, 0, 0.5, -2, 8, 12);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_set_bone_transform_2d(skeleton, 0, pose) == OK);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_get_bone_transform_2d(skeleton, 0) == pose && bones[0] == Transform2D());
	const uint32_t revision = meshes.skeleton_get_revision(skeleton);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_set_bone_transform_2d(skeleton, 0, pose) == OK && meshes.skeleton_get_revision(skeleton) == revision);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_set_bone_transform(skeleton, 0, Transform()) == ERR_INVALID_PARAMETER);
	Transform2D invalid = pose;
	invalid[2].x = std::numeric_limits<real_t>::quiet_NaN();
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_set_bone_transform_2d(skeleton, 0, invalid) == ERR_INVALID_PARAMETER);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_set_base_transform_2d(skeleton, invalid) == ERR_INVALID_PARAMETER);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_set_base_transform_2d(skeleton, pose) == OK && meshes.skeleton_get_revision(skeleton) == revision + 1);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_get_pose_2d(skeleton, bones, base) == OK && base == pose && bones[0] == pose);
	Vector<Transform> spatial;
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_get_pose(skeleton, spatial) == ERR_UNAVAILABLE);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_allocate(skeleton, 2, false) == OK && meshes.skeleton_get_pose(skeleton, spatial) == OK && spatial.size() == 2);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_get_pose_2d(skeleton, bones, base) == ERR_UNAVAILABLE);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_set_bone_transform_2d(skeleton, 0, pose) == ERR_INVALID_PARAMETER);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_allocate(skeleton, 0, true) == OK && meshes.skeleton_get_bone_count(skeleton) == 0);
	FF_CANVAS_SKIN_CHECK(meshes.free(skeleton) == OK);
	return true;
}

bool TestFixedFunctionCanvasSkinning::_positions() {
	Vector<Point2> points;
	points.push_back(Point2(2, 3));
	points.push_back(Point2(4, 3));
	points.push_back(Point2(2, 5));
	points.push_back(Point2(4, 5));
	const Vector<Point2> original = points;
	Vector<int> indices;
	Vector<float> weights;
	indices.resize(16);
	weights.resize(16);
	for (int i = 0; i < 16; i++) {
		indices.write[i] = 999;
		weights.write[i] = 0;
	}
	indices.write[0] = 0;
	indices.write[1] = 1;
	weights.write[0] = 1;
	weights.write[1] = 3;
	weights.write[8] = 1; // Unavailable bone retains the ordinary point.
	weights.write[12] = std::numeric_limits<float>::quiet_NaN();
	weights.write[13] = -1;
	weights.write[14] = std::numeric_limits<float>::infinity();
	indices.write[15] = 0;
	weights.write[15] = 0.5;
	Vector<Transform2D> bones;
	bones.push_back(Transform2D(0, Vector2(2, -3)));
	bones.push_back(Transform2D(-1, 0, 0, 1, 10, 1));
	const Transform2D canvas(2, 0, 0, 3, 10, 20);
	const Transform2D skeleton_canvas = canvas * Transform2D(0, Vector2(5, 7));
	const Transform2D item = skeleton_canvas * Transform2D(-1, 0, 0.5, 2, 3, 4) * Transform2D(0, Vector2(1, -1));
	Vector<Point2> result;
	FF_CANVAS_SKIN_CHECK(FixedFunctionCanvasItemRenderer::_skin_polygon(points, indices, weights, bones, item, skeleton_canvas, result) == OK);
	const Vector2 expected[] = { Vector2(35, 65), Vector2(18, 65), Vector2(24, 77), Vector2(24, 68) };
	for (int i = 0; i < 4; i++) {
		FF_CANVAS_SKIN_CHECK(result[i].distance_to(expected[i]) < 0.0001 && points[i] == original[i]);
	}
	weights.resize(1);
	FF_CANVAS_SKIN_CHECK(FixedFunctionCanvasItemRenderer::_skin_polygon(points, indices, weights, bones, item, skeleton_canvas, result) == OK);
	for (int i = 0; i < 4; i++) {
		FF_CANVAS_SKIN_CHECK(result[i] == item.xform(points[i]));
	}
	weights.resize(16);
	FF_CANVAS_SKIN_CHECK(FixedFunctionCanvasItemRenderer::_skin_polygon(points, indices, weights, bones, item, Transform2D(0, 0, 0, 1, 0, 0), result) == OK);
	FF_CANVAS_SKIN_CHECK(result[0] == item.xform(points[0]));

	for (int i = 0; i < weights.size(); i++) {
		weights.write[i] = 0;
	}
	weights.write[0] = 1;
	bones.write[0] = Transform2D(0, Vector2(0.25, -0.25));
	const Transform2D fractional(0, Vector2(-2.5, 0.5));
	FF_CANVAS_SKIN_CHECK(FixedFunctionCanvasItemRenderer::_skin_polygon(points, indices, weights, bones, fractional, Transform2D(), result, true) == OK);
	FF_CANVAS_SKIN_CHECK(result[0] == Vector2(0.25, 3.75) && result[1] == Vector2(2, 4));
	return true;
}

bool TestFixedFunctionCanvasSkinning::cpu() {
	FF_CANVAS_SKIN_CHECK(_storage() && _positions());
	printf("PASS: CPU 2D skeleton ownership/modes, immutable poses, revisions, weighted/mirrored/sheared positions, canvas/extra transforms and bind fallback.\n");
	return true;
}

bool TestFixedFunctionCanvasSkinning::_pixel(const std::vector<uint32_t> &p_pixels, int p_x, int p_y, uint32_t p_color) {
	const uint32_t actual = p_pixels[p_y * 128 + p_x];
	for (int shift = 0; shift < 32; shift += 8) {
		if (Math::abs(int((actual >> shift) & 255) - int((p_color >> shift) & 255)) > 8) {
			printf("Canvas skinning pixel %d,%d: %08x expected %08x\n", p_x, p_y, actual, p_color);
			return false;
		}
	}
	return true;
}

bool TestFixedFunctionCanvasSkinning::test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, std::vector<uint32_t> &r_pixels) {
	FF_CANVAS_SKIN_CHECK(cpu());
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionGraphicsDevice &device = p_context.get_device();
	FixedFunctionCanvasItemRenderer renderer(p_context, p_targets);
	const RID skeleton = meshes.skeleton_create();
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_allocate(skeleton, 2, true) == OK);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_set_bone_transform_2d(skeleton, 0, Transform2D(0, Vector2(8, 0))) == OK);
	FF_CANVAS_SKIN_CHECK(meshes.skeleton_set_bone_transform_2d(skeleton, 1, Transform2D(0, Vector2(24, 0))) == OK);
	RasterizerCanvas::Item item;
	item.skeleton = skeleton;
	const Transform2D canvas(2, 0, 0, 1, 3, 2);
	item.final_transform = canvas * Transform2D(0, Vector2(8, 12));
	RasterizerCanvas::Item::CommandPolygon *polygon = memnew(RasterizerCanvas::Item::CommandPolygon);
	item.commands.push_back(polygon);
	polygon->antialiased = false;
	polygon->points.push_back(Point2(0, 0));
	polygon->points.push_back(Point2(16, 0));
	polygon->points.push_back(Point2(16, 16));
	polygon->points.push_back(Point2(0, 16));
	const int order[] = { 0, 1, 2, 0, 2, 3 };
	for (int i = 0; i < 6; i++) {
		polygon->indices.push_back(order[i]);
	}
	polygon->count = 6;
	polygon->colors.push_back(Color(1, 0, 0));
	polygon->bones.resize(16);
	polygon->weights.resize(16);
	for (int i = 0; i < 16; i++) {
		polygon->bones.write[i] = 0;
		polygon->weights.write[i] = i % 4 == 0 ? 1 : 0;
	}
	const Vector<Point2> original = polygon->points;
	r_pixels.resize(128 * 96);
	for (int step = 0; step < 5; step++) {
		if (step == 1) {
			for (int i = 0; i < 4; i++) {
				polygon->bones.write[i * 4 + 1] = 1;
				polygon->weights.write[i * 4] = 3;
				polygon->weights.write[i * 4 + 1] = 1;
			}
		} else if (step == 2) {
			item.final_clip_owner = &item;
			item.final_clip_rect = Rect2(48, 14, 8, 16);
		} else if (step == 3) {
			item.final_clip_owner = nullptr;
			item.skeleton = RID();
		} else if (step == 4) {
			item.skeleton = skeleton;
			FF_CANVAS_SKIN_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(128, 96) == OK);
		}
		FF_CANVAS_SKIN_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 0)) == OK);
		FF_CANVAS_SKIN_CHECK(renderer.render_items(&item, Color(1, 1, 1), canvas) == OK && p_targets.end_frame(false) == OK);
		FF_CANVAS_SKIN_CHECK(device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FF_CANVAS_SKIN_CHECK(_pixel(r_pixels, 52, 20, step == 3 ? 0xff000000 : 0xffff0000));
		FF_CANVAS_SKIN_CHECK(_pixel(r_pixels, 24, 20, step == 3 ? 0xffff0000 : 0xff000000));
		FF_CANVAS_SKIN_CHECK(_pixel(r_pixels, 44, 20, step == 2 ? 0xff000000 : 0xffff0000));
		for (int i = 0; i < original.size(); i++) {
			FF_CANVAS_SKIN_CHECK(polygon->points[i] == original[i]);
		}
	}
	item.skeleton = RID();
	FF_CANVAS_SKIN_CHECK(meshes.free(skeleton) == OK);
	printf("PASS: native weighted canvas polygons, camera/base transforms, clipping, unskinned detachment, immutable commands and resize recovery.\n");
	return true;
}

#undef FF_CANVAS_SKIN_CHECK
