#include "mesh_storage.h"

#include "servers/visual/visual_server_canvas.h"
#include "servers/visual/visual_server_globals.h"

#include <string.h>

FixedFunctionMeshStorage::FFMultiMesh::FFMultiMesh() :
		visible(-1), color_format(VS::MULTIMESH_COLOR_NONE), custom_format(VS::MULTIMESH_CUSTOM_DATA_NONE), transform_format(VS::MULTIMESH_TRANSFORM_3D), aabb_dirty(false), materials_dirty(false) {}

FixedFunctionMeshStorage::FFMultiMesh *FixedFunctionMeshStorage::_get_multimesh(RID p_multimesh) const {
	return _multimesh_owner.owns(p_multimesh) ? _multimesh_owner.getptr(p_multimesh) : nullptr;
}

RID FixedFunctionMeshStorage::multimesh_create() {
	if (_clearing || _next_multimesh_identity == UINT64_MAX) {
		return RID();
	}
	FFMultiMesh *multimesh = memnew(FFMultiMesh);
	multimesh->geometry_identity = ++_next_multimesh_identity;
	multimesh->self = _multimesh_owner.make_rid(multimesh);
	_multimeshes.push_back(multimesh);
	_multimesh_identities[multimesh->geometry_identity] = multimesh;
	return multimesh->self;
}

Error FixedFunctionMeshStorage::multimesh_allocate(RID p_multimesh, int p_count, VS::MultimeshColorFormat p_color, VS::MultimeshCustomDataFormat p_custom, VS::MultimeshTransformFormat p_format) {
	FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	if (!multimesh || _clearing || p_count < 0 || p_color < 0 || p_color >= VS::MULTIMESH_COLOR_MAX ||
			p_custom < 0 || p_custom >= VS::MULTIMESH_CUSTOM_DATA_MAX ||
			(p_format != VS::MULTIMESH_TRANSFORM_2D && p_format != VS::MULTIMESH_TRANSFORM_3D)) {
		return ERR_INVALID_PARAMETER;
	}
	if (uint64_t(p_count) > MIN(uint64_t(INT32_MAX) / 20, uint64_t(SIZE_MAX) / (2 * sizeof(FFMultiMeshInstance)))) {
		return ERR_OUT_OF_MEMORY;
	}
	if (multimesh->copies.size() == p_count && multimesh->color_format == p_color && multimesh->custom_format == p_custom && multimesh->transform_format == p_format) {
		return OK;
	}
	Vector<FFMultiMeshInstance> copies;
	Vector<Color> custom_data;
	Error error = copies.resize(p_count);
	if (error == OK && p_custom != VS::MULTIMESH_CUSTOM_DATA_NONE) {
		error = custom_data.resize(p_count);
		if (error == OK) {
			custom_data.fill(Color(0, 0, 0, 0));
		}
	}
	if (error != OK) {
		return error;
	}
	multimesh->copies = copies;
	multimesh->custom_data = custom_data;
	multimesh->color_format = p_color;
	multimesh->custom_format = p_custom;
	multimesh->transform_format = p_format;
	multimesh->aabb_dirty = true;
	multimesh->geometry_version++;
	multimesh->geometry_changed_frame = _frame_number;
	return OK;
}

VS::MultimeshTransformFormat FixedFunctionMeshStorage::multimesh_get_transform_format(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	return multimesh ? multimesh->transform_format : VS::MULTIMESH_TRANSFORM_3D;
}

Error FixedFunctionMeshStorage::multimesh_attach_canvas_item(RID p_multimesh, RID p_canvas_item, bool p_attach) {
	FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	if (!multimesh) {
		return p_attach ? ERR_INVALID_PARAMETER : OK;
	}
	if (!p_canvas_item.is_valid()) {
		return ERR_INVALID_PARAMETER;
	}
	const int index = multimesh->canvas_items.find(p_canvas_item);
	if (p_attach && index < 0) {
		return multimesh->canvas_items.push_back(p_canvas_item) ? ERR_OUT_OF_MEMORY : OK;
	}
	if (!p_attach && index >= 0) {
		multimesh->canvas_items.remove(index);
	}
	return OK;
}

Error FixedFunctionMeshStorage::multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
	FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	if (!multimesh || _clearing || (p_mesh.is_valid() && !owns_mesh(p_mesh))) {
		return ERR_INVALID_PARAMETER;
	}
	if (multimesh->mesh != p_mesh) {
		multimesh->mesh = p_mesh;
		multimesh->aabb_dirty = multimesh->materials_dirty = true;
		multimesh->geometry_version++;
		multimesh->geometry_changed_frame = _frame_number;
	}
	return OK;
}

RID FixedFunctionMeshStorage::multimesh_get_mesh(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	return multimesh ? multimesh->mesh : RID();
}

int FixedFunctionMeshStorage::multimesh_get_instance_count(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	return multimesh ? multimesh->copies.size() : 0;
}

Error FixedFunctionMeshStorage::multimesh_set_visible(RID p_multimesh, int p_visible) {
	FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	if (!multimesh || _clearing || p_visible < -1) {
		return ERR_INVALID_PARAMETER;
	}
	if (multimesh->visible != p_visible) {
		multimesh->visible = p_visible;
		multimesh->aabb_dirty = true;
		multimesh->geometry_version++;
		multimesh->geometry_changed_frame = _frame_number;
	}
	return OK;
}

int FixedFunctionMeshStorage::multimesh_get_visible(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	return multimesh ? multimesh->visible : -1;
}

Error FixedFunctionMeshStorage::multimesh_set_transform(RID p_multimesh, int p_index, const Transform &p_transform) {
	FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	if (!multimesh || _clearing || p_index < 0 || p_index >= multimesh->copies.size()) {
		return ERR_INVALID_PARAMETER;
	}
	for (int row = 0; row < 3; row++) {
		const real_t values[] = { p_transform.basis[row][0], p_transform.basis[row][1], p_transform.basis[row][2], p_transform.origin[row] };
		for (int i = 0; i < 4; i++) {
			if (Math::is_nan(values[i]) || Math::is_inf(values[i])) {
				return ERR_INVALID_PARAMETER;
			}
		}
	}
	if (multimesh->copies[p_index].transform != p_transform) {
		multimesh->copies.write[p_index].transform = p_transform;
		multimesh->aabb_dirty = true;
		multimesh->geometry_version++;
		multimesh->geometry_changed_frame = _frame_number;
	}
	return OK;
}

Color FixedFunctionMeshStorage::_multimesh_quantize_color(const Color &p_color) {
	Color quantized;
	for (int i = 0; i < 4; i++) {
		quantized[i] = uint8_t(CLAMP(p_color[i], 0.0f, 1.0f) * 255) / 255.0f;
	}
	return quantized;
}

Error FixedFunctionMeshStorage::multimesh_set_color(RID p_multimesh, int p_index, const Color &p_color, bool p_custom) {
	FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	if (!multimesh || _clearing || p_index < 0 || p_index >= multimesh->copies.size()) {
		return ERR_INVALID_PARAMETER;
	}
	for (int i = 0; i < 4; i++) {
		if (Math::is_nan(p_color[i]) || Math::is_inf(p_color[i])) {
			return ERR_INVALID_PARAMETER;
		}
	}
	if (p_custom) {
		if (multimesh->custom_format == VS::MULTIMESH_CUSTOM_DATA_NONE) {
			return ERR_UNAVAILABLE;
		}
		multimesh->custom_data.write[p_index] = multimesh->custom_format == VS::MULTIMESH_CUSTOM_DATA_8BIT ? _multimesh_quantize_color(p_color) : p_color;
	} else {
		if (multimesh->color_format == VS::MULTIMESH_COLOR_NONE) {
			return ERR_UNAVAILABLE;
		}
		multimesh->copies.write[p_index].color = multimesh->color_format == VS::MULTIMESH_COLOR_8BIT ? _multimesh_quantize_color(p_color) : p_color;
	}
	return OK;
}

Transform FixedFunctionMeshStorage::multimesh_get_transform(RID p_multimesh, int p_index) const {
	const FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	ERR_FAIL_COND_V(!multimesh || p_index < 0 || p_index >= multimesh->copies.size(), Transform());
	return multimesh->copies[p_index].transform;
}

Color FixedFunctionMeshStorage::multimesh_get_color(RID p_multimesh, int p_index, bool p_custom) const {
	const FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	ERR_FAIL_COND_V(!multimesh || p_index < 0 || p_index >= multimesh->copies.size(), Color());
	if (p_custom) {
		return multimesh->custom_data.empty() ? Color(0, 0, 0, 0) : multimesh->custom_data[p_index];
	}
	return multimesh->copies[p_index].color;
}

bool FixedFunctionMeshStorage::_multimesh_finite(const float *p_values, int p_count) {
	for (int i = 0; i < p_count; i++) {
		if (Math::is_nan(p_values[i]) || Math::is_inf(p_values[i])) {
			return false;
		}
	}
	return true;
}

Color FixedFunctionMeshStorage::_multimesh_read_color(const float *p_data, int p_size) {
	if (p_size == 1) {
		// A packed RGBA word travels through a float slot as raw bytes.
		uint8_t bytes[4];
		memcpy(bytes, p_data, sizeof(bytes));
		return Color(bytes[0] / 255.0f, bytes[1] / 255.0f, bytes[2] / 255.0f, bytes[3] / 255.0f);
	}
	return Color(p_data[0], p_data[1], p_data[2], p_data[3]);
}

Error FixedFunctionMeshStorage::multimesh_set_bulk(RID p_multimesh, const PoolVector<float> &p_data) {
	FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	if (!multimesh || _clearing) {
		return ERR_INVALID_PARAMETER;
	}
	const int color_size = multimesh->color_format == VS::MULTIMESH_COLOR_FLOAT ? 4 : int(multimesh->color_format == VS::MULTIMESH_COLOR_8BIT);
	const int custom_size = multimesh->custom_format == VS::MULTIMESH_CUSTOM_DATA_FLOAT ? 4 : int(multimesh->custom_format == VS::MULTIMESH_CUSTOM_DATA_8BIT);
	const int transform_size = multimesh->transform_format == VS::MULTIMESH_TRANSFORM_2D ? 8 : 12;
	const int stride = transform_size + color_size + custom_size;
	const int count = multimesh->copies.size();
	if (p_data.size() != count * stride) {
		return ERR_INVALID_PARAMETER;
	}
	const PoolVector<float>::Read data = p_data.read();
	for (int i = 0; i < count; i++) {
		const float *entry = data.ptr() + i * stride;
		if (!_multimesh_finite(entry, transform_size) || (color_size == 4 && !_multimesh_finite(entry + transform_size, 4)) ||
				(custom_size == 4 && !_multimesh_finite(entry + transform_size + color_size, 4))) {
			return ERR_INVALID_DATA;
		}
	}
	FFMultiMeshInstance *copies = multimesh->copies.ptrw();
	Color *custom = multimesh->custom_data.ptrw();
	for (int i = 0; i < count; i++) {
		const float *entry = data.ptr() + i * stride;
		for (int row = 0; row < transform_size / 4; row++) {
			const float z = transform_size == 12 ? entry[row * 4 + 2] : 0;
			copies[i].transform.basis[row] = Vector3(entry[row * 4], entry[row * 4 + 1], z);
			copies[i].transform.origin[row] = entry[row * 4 + 3];
		}
		if (color_size) {
			copies[i].color = _multimesh_read_color(entry + transform_size, color_size);
		}
		if (custom_size) {
			custom[i] = _multimesh_read_color(entry + transform_size + color_size, custom_size);
		}
	}
	multimesh->aabb_dirty = true;
	multimesh->geometry_version++;
	multimesh->geometry_changed_frame = _frame_number;
	return OK;
}

Error FixedFunctionMeshStorage::multimesh_get_draw_data(RID p_multimesh, RID &r_mesh, Vector<FFMultiMeshInstance> &r_copies, int &r_visible) const {
	const FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	if (!multimesh) {
		return ERR_INVALID_PARAMETER;
	}
	r_mesh = multimesh->mesh;
	r_copies = multimesh->copies;
	r_visible = multimesh->visible < 0 ? r_copies.size() : MIN(multimesh->visible, r_copies.size());
	return OK;
}

Error FixedFunctionMeshStorage::multimesh_get_draw_data(uint64_t p_identity, RID &r_mesh, Vector<FFMultiMeshInstance> &r_copies, int &r_visible) const {
	const Map<uint64_t, FFMultiMesh *>::Element *entry = _multimesh_identities.find(p_identity);
	return entry ? multimesh_get_draw_data(entry->get()->self, r_mesh, r_copies, r_visible) : ERR_DOES_NOT_EXIST;
}

uint64_t FixedFunctionMeshStorage::multimesh_get_geometry_identity(RID p_multimesh) const {
	const FFMultiMesh *multimesh = _get_multimesh(p_multimesh);
	return multimesh ? multimesh->geometry_identity : 0;
}

uint64_t FixedFunctionMeshStorage::multimesh_get_geometry_version(uint64_t p_identity) const {
	enum { GEOMETRY_SETTLE_FRAMES = 4 };
	const Map<uint64_t, FFMultiMesh *>::Element *entry = _multimesh_identities.find(p_identity);
	const FFMultiMesh *multimesh = entry ? entry->get() : nullptr;
	if (!multimesh || multimesh->transform_format != VS::MULTIMESH_TRANSFORM_3D || _frame_number - multimesh->geometry_changed_frame <= GEOMETRY_SETTLE_FRAMES) {
		return 0;
	}
	return multimesh->geometry_version;
}

bool FixedFunctionMeshStorage::multimesh_get_geometry_bounds(uint64_t p_identity, const FFSurfaceData &p_surface, FFGeometryBounds &r_bounds) {
	const Map<uint64_t, FFMultiMesh *>::Element *entry = _multimesh_identities.find(p_identity);
	FFMultiMesh *multimesh = entry ? entry->get() : nullptr;
	if (!multimesh || !p_surface.identity) {
		return false;
	}
	if (multimesh->bounds_geometry_version != multimesh->geometry_version || multimesh->bounds_surface_identity != p_surface.identity || multimesh->bounds_surface_version != p_surface.version) {
		multimesh->bounds_geometry_version = multimesh->geometry_version;
		multimesh->bounds_surface_identity = p_surface.identity;
		multimesh->bounds_surface_version = p_surface.version;
		multimesh->geometry_bounds_valid = false;
		// Native light selection uses the authored AABB. CPU lighting uses the
		// decoded positions, which can extend beyond it after a region edit.
		FFGeometryBounds source;
		const Vector3 end = p_surface.aabb.position + p_surface.aabb.size;
		for (int axis = 0; axis < 3; axis++) {
			source.minimum[axis] = MIN(p_surface.aabb.position[axis], p_surface.position_min[axis]);
			source.maximum[axis] = MAX(end[axis], p_surface.position_max[axis]);
		}
		FFGeometryBounds bounds = {};
		bool has_bounds = false;
		const int count = multimesh->visible < 0 ? multimesh->copies.size() : MIN(multimesh->visible, multimesh->copies.size());
		const FFMultiMeshInstance *copies = multimesh->copies.ptr();
		for (int i = 0; i < count; i++) {
			const Transform &copy = copies[i].transform;
			if (multimesh_is_collapsed(copy)) {
				continue;
			}
			FFGeometryBounds copy_bounds = source;
			if (!copy_bounds.transform(copy)) {
				return false;
			}
			if (!has_bounds) {
				bounds = copy_bounds;
				has_bounds = true;
				continue;
			}
			for (int axis = 0; axis < 3; axis++) {
				bounds.minimum[axis] = MIN(bounds.minimum[axis], copy_bounds.minimum[axis]);
				bounds.maximum[axis] = MAX(bounds.maximum[axis], copy_bounds.maximum[axis]);
			}
		}
		multimesh->geometry_bounds = bounds;
		multimesh->geometry_bounds_valid = has_bounds;
	}
	if (!multimesh->geometry_bounds_valid) {
		return false;
	}
	r_bounds = multimesh->geometry_bounds;
	return true;
}

bool FixedFunctionMeshStorage::multimesh_is_collapsed(const Transform &p_transform, bool p_2d) {
	// CPUParticles uses a zero transform for inactive entries.
	return p_transform.basis[0] == Vector3() && p_transform.basis[1] == Vector3() && (p_2d || p_transform.basis[2] == Vector3());
}

Transform2D FixedFunctionMeshStorage::multimesh_to_transform_2d(const Transform &p_transform) {
	return Transform2D(p_transform.basis[0][0], p_transform.basis[1][0], p_transform.basis[0][1], p_transform.basis[1][1],
			p_transform.origin.x, p_transform.origin.y);
}

Error FixedFunctionMeshStorage::multimesh_get_aabb(RID p_multimesh, AABB &r_bounds) const {
	RID mesh;
	Vector<FFMultiMeshInstance> copies;
	int visible;
	const Error error = multimesh_get_draw_data(p_multimesh, mesh, copies, visible);
	r_bounds = AABB();
	if (error != OK || !mesh.is_valid() || visible == 0 || mesh_get_surface_count(mesh) == 0) {
		return error;
	}
	AABB source;
	const Error mesh_error = mesh_get_aabb(mesh, RID(), source);
	if (mesh_error != OK) {
		return mesh_error;
	}
	bool first = true;
	const bool is_2d = multimesh_get_transform_format(p_multimesh) == VS::MULTIMESH_TRANSFORM_2D;
	for (int i = 0; i < visible; i++) {
		const Transform &transform = copies[i].transform;
		if (multimesh_is_collapsed(transform, is_2d)) {
			continue;
		}
		const AABB bounds = transform.xform(source);
		if (_valid_aabb(bounds)) {
			if (first) {
				r_bounds = bounds;
				first = false;
			} else {
				r_bounds.merge_with(bounds);
			}
		}
	}
	return OK;
}

void FixedFunctionMeshStorage::_free_multimesh(FFMultiMesh *p_multimesh) {
	_multimesh_identities.erase(p_multimesh->geometry_identity);
	_multimesh_owner.free(p_multimesh->self);
	_multimeshes.erase(p_multimesh);
	if (VSG::canvas) {
		for (int i = 0; i < p_multimesh->canvas_items.size(); i++) {
			const RID item = p_multimesh->canvas_items[i];
			VisualServerCanvas::Item *canvas_item = VSG::canvas->canvas_item_owner.getornull(item);
			if (canvas_item) {
				canvas_item->remove_references(p_multimesh->self);
				canvas_item->rect_dirty = true;
				VSG::canvas->_canvas_item_invalidate_local_bound(item);
			}
		}
	}
	_detach_base_instances(p_multimesh->instances);
	memdelete(p_multimesh);
}
