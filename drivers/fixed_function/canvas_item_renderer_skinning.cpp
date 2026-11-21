#include "drivers/fixed_function/canvas_item_renderer.h"

Error FixedFunctionCanvasItemRenderer::_skin_polygon(const Vector<Point2> &p_points, const Vector<int> &p_bone_indices, const Vector<float> &p_bone_weights, const Vector<Transform2D> &p_bones, const Transform2D &p_transform, const Transform2D &p_skeleton_transform, Vector<Point2> &r_points) {
	if (p_points.size() > INT32_MAX / RSE::ARRAY_WEIGHTS_SIZE) {
		return ERR_OUT_OF_MEMORY;
	}
	const Error error = r_points.resize(p_points.size());
	if (error != OK) {
		return error;
	}
	const real_t determinant = p_skeleton_transform.determinant();
	// An unconfigured rig or a noninvertible canvas frame retains ordinary geometry.
	const bool skin = p_bone_indices.size() == p_points.size() * RSE::ARRAY_WEIGHTS_SIZE && p_bone_weights.size() == p_bone_indices.size() && !p_bones.is_empty() &&
			determinant != 0 && !Math::is_nan(determinant) && !Math::is_inf(determinant);
	Transform2D into_skeleton;
	if (skin) {
		into_skeleton = p_skeleton_transform.affine_inverse() * p_transform;
	}
	Point2 *points = r_points.ptrw();
	for (int i = 0; i < p_points.size(); i++) {
		points[i] = p_transform.xform(p_points[i]);
		if (!skin) {
			continue;
		}
		double total = 0;
		for (int j = 0; j < RSE::ARRAY_WEIGHTS_SIZE; j++) {
			const int bone = p_bone_indices[i * RSE::ARRAY_WEIGHTS_SIZE + j];
			const float weight = p_bone_weights[i * RSE::ARRAY_WEIGHTS_SIZE + j];
			if (bone >= 0 && bone < p_bones.size() && weight > 0 && !Math::is_inf(weight)) {
				total += weight;
			}
		}
		if (total == 0) {
			continue;
		}
		const Point2 source = into_skeleton.xform(p_points[i]);
		Point2 skinned;
		for (int j = 0; j < RSE::ARRAY_WEIGHTS_SIZE; j++) {
			const int bone = p_bone_indices[i * RSE::ARRAY_WEIGHTS_SIZE + j];
			const float weight = p_bone_weights[i * RSE::ARRAY_WEIGHTS_SIZE + j];
			if (bone >= 0 && bone < p_bones.size() && weight > 0 && !Math::is_inf(weight)) {
				// Normalize valid influences as in the shared 3D CPU skinning path.
				skinned += p_bones[bone].xform(source) * real_t(weight / total);
			}
		}
		const Point2 result = p_skeleton_transform.xform(skinned);
		if (!Math::is_nan(result.x) && !Math::is_inf(result.x) && !Math::is_nan(result.y) && !Math::is_inf(result.y)) {
			points[i] = result;
		}
	}
	return OK;
}
