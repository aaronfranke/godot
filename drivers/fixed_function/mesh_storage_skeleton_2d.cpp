#include "mesh_storage.h"

#include "servers/visual/visual_server_canvas.h"
#include "servers/visual/visual_server_globals.h"

bool FixedFunctionMeshStorage::_valid_transform_2d(const Transform2D &p_transform) {
	for (int column = 0; column < 3; column++) {
		for (int row = 0; row < 2; row++) {
			if (Math::is_nan(p_transform[column][row]) || Math::is_inf(p_transform[column][row])) {
				return false;
			}
		}
	}
	return true;
}

Error FixedFunctionMeshStorage::skeleton_set_bone_transform_2d(RID p_skeleton, int p_bone, const Transform2D &p_transform) {
	FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	if (_clearing || !skeleton || !skeleton->use_2d || p_bone < 0 || p_bone >= skeleton->bones_2d.size() || !_valid_transform_2d(p_transform)) {
		return ERR_INVALID_PARAMETER;
	}
	if (skeleton->bones_2d[p_bone] != p_transform) {
		skeleton->bones_2d.write[p_bone] = p_transform;
		skeleton->revision++;
		skeleton->dirty = true;
	}
	return OK;
}

Transform2D FixedFunctionMeshStorage::skeleton_get_bone_transform_2d(RID p_skeleton, int p_bone) const {
	const FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	ERR_FAIL_COND_V(!skeleton || !skeleton->use_2d || p_bone < 0 || p_bone >= skeleton->bones_2d.size(), Transform2D());
	return skeleton->bones_2d[p_bone];
}

Error FixedFunctionMeshStorage::skeleton_set_base_transform_2d(RID p_skeleton, const Transform2D &p_transform) {
	FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	if (_clearing || !skeleton || !_valid_transform_2d(p_transform)) {
		return ERR_INVALID_PARAMETER;
	}
	if (skeleton->base_transform_2d != p_transform) {
		skeleton->base_transform_2d = p_transform;
		skeleton->revision++;
		skeleton->dirty = true;
	}
	return OK;
}

Error FixedFunctionMeshStorage::skeleton_get_pose_2d(RID p_skeleton, Vector<Transform2D> &r_bones, Transform2D &r_base) const {
	const FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	if (!skeleton) {
		return ERR_INVALID_PARAMETER;
	}
	if (!skeleton->use_2d) {
		return ERR_UNAVAILABLE;
	}
	r_bones = skeleton->bones_2d;
	r_base = skeleton->base_transform_2d;
	return OK;
}

Error FixedFunctionMeshStorage::skeleton_attach_canvas_item(RID p_skeleton, RID p_canvas_item, bool p_attach) {
	FFSkeleton *skeleton = _get_skeleton(p_skeleton);
	if (!skeleton) {
		return p_attach ? ERR_INVALID_PARAMETER : OK;
	}
	if (!p_canvas_item.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	const int index = skeleton->canvas_items.find(p_canvas_item);
	if (p_attach && index < 0) {
		return skeleton->canvas_items.push_back(p_canvas_item) ? ERR_OUT_OF_MEMORY : OK;
	}
	if (!p_attach && index >= 0) {
		skeleton->canvas_items.remove(index);
	}
	return OK;
}

void FixedFunctionMeshStorage::_invalidate_skeleton_canvas_items(FFSkeleton *p_skeleton, bool p_detach) {
	if (!VSG::canvas) {
		return;
	}
	for (int i = 0; i < p_skeleton->canvas_items.size(); i++) {
		const RID rid = p_skeleton->canvas_items[i];
		VisualServerCanvas::Item *item = VSG::canvas->canvas_item_owner.getornull(rid);
		if (!item || item->skeleton != p_skeleton->self) {
			continue;
		}
		if (p_detach) {
			// Retain the commands as unskinned geometry after the resource is freed.
			item->skeleton = RID();
		}
		item->rect_dirty = true;
		VSG::canvas->_canvas_item_invalidate_local_bound(rid);
	}
}
