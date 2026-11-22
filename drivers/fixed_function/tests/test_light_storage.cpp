#include "test_light_storage.h"

#include "drivers/fixed_function/rasterizer_scene_fixed_function.h"
#include "drivers/fixed_function/rasterizer_storage_fixed_function.h"

#include <stdio.h>
#include <limits>

class TestFixedFunctionLightStorageSingletonScope {
	RasterizerStorage *_previous;

public:
	TestFixedFunctionLightStorageSingletonScope() :
			_previous(RasterizerStorage::base_singleton) {}
	~TestFixedFunctionLightStorageSingletonScope() { RasterizerStorage::base_singleton = _previous; }
};

#define FF_LIGHT_CHECK(m_condition)                                           \
	do {                                                                      \
		if (!(m_condition)) {                                                 \
			printf("Light storage FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                     \
		}                                                                     \
	} while (false)

TestFixedFunctionLightStorage::FFInstance::FFInstance(FixedFunctionLightStorage &p_storage, RID p_light) :
		storage(p_storage), other(nullptr), changes(0), removals(0), mutate(false), rebind_other(false), clear_on_change(false), success(true) {
	base_type = VS::INSTANCE_LIGHT;
	base = p_light;
	renderer_instance = storage.light_instance_create(base);
	success = renderer_instance.is_valid() && storage.instance_add_dependency(base, this) == OK;
}

TestFixedFunctionLightStorage::FFInstance::~FFInstance() {
	if (storage.has_dependency(base, this)) {
		storage.instance_remove_dependency(base, this);
	}
	if (renderer_instance.is_valid()) {
		storage.free(renderer_instance);
	}
}

void TestFixedFunctionLightStorage::FFInstance::base_changed(bool p_aabb, bool p_materials) {
	changes++;
	success = success && p_aabb && !p_materials && storage.update_dependencies() == ERR_BUSY;
	if (rebind_other && other) {
		rebind_other = false;
		success = success && storage.instance_remove_dependency(other->base, other) == OK;
		success = success && storage.instance_add_dependency(other->base, other) == OK;
	}
	if (mutate) {
		mutate = false;
		FixedFunctionLightStorage::FFLightData data;
		success = success && storage.light_get_data(base, data) == OK;
		data.parameters[VS::LIGHT_PARAM_ENERGY] = 3;
		success = success && storage.light_set_data(base, data) == OK;
	}
	if (clear_on_change) {
		clear_on_change = false;
		storage.clear();
	}
}

void TestFixedFunctionLightStorage::FFInstance::base_removed() {
	removals++;
	success = success && !storage.owns_light(base) && storage.instance_remove_dependency(base, this) == OK;
	if (renderer_instance.is_valid()) {
		success = success && storage.free(renderer_instance) == OK;
		renderer_instance = RID();
	}
}

bool TestFixedFunctionLightStorage::_test_metadata(FixedFunctionLightStorage &p_storage) {
	const RID light = p_storage.light_create(VS::LIGHT_DIRECTIONAL);
	const RID omni = p_storage.light_create(VS::LIGHT_OMNI);
	const RID spot = p_storage.light_create(VS::LIGHT_SPOT);
	FF_LIGHT_CHECK(light.is_valid() && omni.is_valid() && spot.is_valid() && p_storage.get_light_count() == 3);
	FixedFunctionLightStorage::FFLightData data, original;
	FF_LIGHT_CHECK(p_storage.light_get_data(light, data) == OK && data.type == VS::LIGHT_DIRECTIONAL && data.color == Color(1, 1, 1) && data.parameters[VS::LIGHT_PARAM_ENERGY] == 1 && data.version == 0);
	data.color = Color(2, -1, 0.5, 0.25);
	data.parameters[VS::LIGHT_PARAM_ENERGY] = 7;
	data.flags = FixedFunctionLightStorage::LIGHT_SHADOW | FixedFunctionLightStorage::LIGHT_NEGATIVE;
	data.cull_mask = 4;
	data.version = UINT64_MAX; // The owner, not the caller's snapshot, advances this counter.
	FF_LIGHT_CHECK(p_storage.light_set_data(light, data) == OK && p_storage.light_get_data(light, original) == OK && original.version == 1 && original.color == data.color && original.cull_mask == 4);
	data.parameters[VS::LIGHT_PARAM_ENERGY] = std::numeric_limits<float>::infinity();
	FF_LIGHT_CHECK(p_storage.light_set_data(light, data) == ERR_INVALID_PARAMETER && p_storage.light_get_data(light, data) == OK && data.parameters[VS::LIGHT_PARAM_ENERGY] == 7 && data.version == 1);
	data.type = VS::LIGHT_OMNI;
	FF_LIGHT_CHECK(p_storage.light_set_data(light, data) == ERR_INVALID_PARAMETER);
	AABB aabb;
	FF_LIGHT_CHECK(p_storage.light_get_aabb(light, aabb) == OK && aabb == AABB());
	FF_LIGHT_CHECK(p_storage.light_get_aabb(omni, aabb) == OK && aabb == AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2)));
	FF_LIGHT_CHECK(p_storage.light_get_aabb(spot, aabb) == OK && aabb.size.z == 1 && Math::is_equal_approx(aabb.size.x, real_t(2)));
	const RID instance = p_storage.light_instance_create(light);
	FF_LIGHT_CHECK(instance.is_valid() && p_storage.get_instance_count() == 1);
	Transform transform(Basis(Vector3(0, 1, 0), 0.5), Vector3(real_t(1) + real_t(1) / 16, 2, -3));
	FF_LIGHT_CHECK(p_storage.light_instance_set_transform(instance, transform) == OK);
	FixedFunctionLightStorage::FFLightInstanceData snapshot;
	FF_LIGHT_CHECK(p_storage.light_instance_get_data(instance, snapshot) == OK && snapshot.transform == transform && snapshot.light == light);
	transform.origin.x = std::numeric_limits<real_t>::quiet_NaN();
	FF_LIGHT_CHECK(p_storage.light_instance_set_transform(instance, transform) == ERR_INVALID_PARAMETER && p_storage.light_instance_get_data(instance, snapshot) == OK && snapshot.transform.origin.x == real_t(1) + real_t(1) / 16);
	FF_LIGHT_CHECK(p_storage.validate_light_instances(&instance, 1) == OK && p_storage.validate_light_instances(nullptr, 1) == ERR_INVALID_PARAMETER && p_storage.validate_light_instances(&light, 1) == ERR_INVALID_PARAMETER);
	FF_LIGHT_CHECK(p_storage.free(light) == OK && p_storage.light_instance_get_data(instance, snapshot) == OK && !snapshot.light.is_valid());
	FF_LIGHT_CHECK(p_storage.free(instance) == OK && p_storage.get_instance_count() == 0);
	p_storage.clear();
	FF_LIGHT_CHECK(p_storage.get_light_count() == 0 && p_storage.get_instance_count() == 0);
	return true;
}

bool TestFixedFunctionLightStorage::_test_dependencies(FixedFunctionLightStorage &p_storage) {
	RID light = p_storage.light_create(VS::LIGHT_DIRECTIONAL);
	FFInstance first(p_storage, light), second(p_storage, light);
	Map<RasterizerScene::InstanceBase *, int> order;
	order[&first] = 0;
	order[&second] = 0;
	FFInstance *early = static_cast<FFInstance *>(order.front()->key());
	FFInstance *late = early == &first ? &second : &first;
	early->other = late;
	early->rebind_other = true;
	early->mutate = true;
	FF_LIGHT_CHECK(p_storage.update_dependencies() == ERR_SKIP && early->changes == 1 && late->changes == 0);
	FF_LIGHT_CHECK(p_storage.update_dependencies() == OK && early->changes == 2 && late->changes == 1 && first.success && second.success);
	FF_LIGHT_CHECK(p_storage.free(light) == OK && first.removals == 1 && second.removals == 1 && first.success && second.success && p_storage.get_instance_count() == 0);
	light = p_storage.light_create(VS::LIGHT_DIRECTIONAL);
	FFInstance clearing(p_storage, light);
	clearing.clear_on_change = true;
	FF_LIGHT_CHECK(p_storage.update_dependencies() == OK && clearing.success && clearing.removals == 1 && p_storage.get_light_count() == 0 && p_storage.get_instance_count() == 0);
	return true;
}

bool TestFixedFunctionLightStorage::_test_facades(FixedFunctionRenderContext &p_context) {
	TestFixedFunctionLightStorageSingletonScope singleton_scope;
	{
		RasterizerStorageFixedFunction storage(p_context.get_texture_storage(), p_context.get_material_storage(), p_context.get_mesh_storage(), p_context.get_light_storage(), p_context.get_scene_storage(), p_context.get_storage_queries());
		FixedFunctionRenderTargetManager targets(p_context);
		RasterizerSceneFixedFunction scene(p_context, targets);
		const RID light = storage.light_create(VS::LIGHT_DIRECTIONAL);
		FF_LIGHT_CHECK(storage.get_last_error() == OK && storage.get_base_type(light) == VS::INSTANCE_LIGHT && p_context.get_storage_queries().get_resource_type(light) == FixedFunctionStorageQueries::RESOURCE_LIGHT);
		storage.light_set_color(light, Color(2, 0.5, 0.25));
		storage.light_set_param(light, VS::LIGHT_PARAM_ENERGY, 3);
		storage.light_set_shadow(light, true);
		storage.light_set_cull_mask(light, 8);
		storage.light_set_negative(light, true);
		FF_LIGHT_CHECK(storage.get_last_error() == OK && storage.light_get_color(light) == Color(2, 0.5, 0.25) && storage.light_get_param(light, VS::LIGHT_PARAM_ENERGY) == 3 && storage.light_get_version(light) == 5);
		FF_LIGHT_CHECK(storage.light_has_shadow(light) == p_context.get_device().supports_directional_shadows());
		storage.light_directional_set_shadow_mode(light, VS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS);
		FF_LIGHT_CHECK(storage.light_directional_get_shadow_mode(light) == (storage.light_has_shadow(light) ? VS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL : VS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS));
		FixedFunctionLightStorage::FFLightData data;
		FF_LIGHT_CHECK(p_context.get_light_storage().light_get_data(light, data) == OK && (data.flags & FixedFunctionLightStorage::LIGHT_SHADOW) && data.cull_mask == 8 && data.directional_shadow_mode == VS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS);
		const RID instance = scene.light_instance_create(light);
		scene.light_instance_set_transform(instance, Transform(Basis(), Vector3(1, 2, 3)));
		scene.light_instance_mark_visible(instance);
		FF_LIGHT_CHECK(scene.get_last_error() == OK && scene.free(instance) && storage.free(light));
	}
	return true;
}

bool TestFixedFunctionLightStorage::test(FixedFunctionRenderContext &p_context) {
	FixedFunctionLightStorage &storage = p_context.get_light_storage();
	FF_LIGHT_CHECK(_test_metadata(storage) && _test_dependencies(storage) && _test_facades(p_context));
	printf("PASS: light CPU snapshots, versions/bounds, instance transforms/detach, mutation-safe native dependencies, facade ownership and shadow fallback.\n");
	return true;
}

#undef FF_LIGHT_CHECK
