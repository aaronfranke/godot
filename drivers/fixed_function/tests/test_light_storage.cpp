#include "test_light_storage.h"

#include "drivers/fixed_function/renderer_light_storage_fixed_function.h"

#include <stdio.h>

#include <limits>

#define FF_LIGHT_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Light storage FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

TestFixedFunctionLightStorage::FFObserver::FFObserver() : storage(nullptr), changes(0), deletions(0), error(OK), mutate(false), release(false), clear(false) {
	tracker.userdata = this;
	tracker.changed_callback = _changed;
	tracker.deleted_callback = _deleted;
}

void TestFixedFunctionLightStorage::_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
	if (p_notification != Dependency::DEPENDENCY_CHANGED_LIGHT) {
		return;
	}
	FFObserver &observer = *static_cast<FFObserver *>(p_tracker->userdata);
	observer.changes++;
	if (observer.mutate) {
		observer.mutate = false;
		FixedFunctionLightStorage::FFLightData data;
		observer.error = observer.storage->light_get_data(observer.light, data);
		if (observer.error == OK) {
			data.parameters[RSE::LIGHT_PARAM_ENERGY] += 1;
			observer.error = observer.storage->light_set_data(observer.light, data);
		}
	}
	if (observer.release) {
		observer.release = false;
		observer.error = observer.storage->free(observer.light);
	}
	if (observer.clear) {
		observer.clear = false;
		observer.storage->clear();
	}
}

void TestFixedFunctionLightStorage::_deleted(const RID &p_dependency, DependencyTracker *p_tracker) {
	static_cast<FFObserver *>(p_tracker->userdata)->deletions++;
}

bool TestFixedFunctionLightStorage::_dependencies(FixedFunctionLightStorage &p_storage) {
	FFObserver observer;
	observer.storage = &p_storage;
	observer.light = p_storage.light_create(RSE::LIGHT_OMNI);
	const RID instance = p_storage.light_instance_create(observer.light);
	FF_LIGHT_CHECK(instance.is_valid());
	observer.tracker.update_begin();
	FF_LIGHT_CHECK(p_storage.light_update_dependency(observer.light, &observer.tracker) == OK);
	observer.tracker.update_end();
	FixedFunctionLightStorage::FFLightData data;
	FF_LIGHT_CHECK(p_storage.light_get_data(observer.light, data) == OK);
	data.parameters[RSE::LIGHT_PARAM_RANGE] = 8;
	FF_LIGHT_CHECK(p_storage.light_set_data(observer.light, data) == OK);
	observer.mutate = true;
	FF_LIGHT_CHECK(p_storage.update_dependencies() == ERR_SKIP && observer.changes == 1 && observer.error == OK);
	FF_LIGHT_CHECK(p_storage.update_dependencies() == OK && observer.changes == 2);
	FF_LIGHT_CHECK(p_storage.light_get_data(observer.light, data) == OK && data.parameters[RSE::LIGHT_PARAM_ENERGY] == 2 && data.version == 2);
	FF_LIGHT_CHECK(p_storage.light_set_data(observer.light, data) == OK);
	observer.release = true;
	FF_LIGHT_CHECK(p_storage.update_dependencies() == OK && observer.deletions == 1 && observer.error == OK);
	FF_LIGHT_CHECK(!p_storage.owns_light(observer.light));
	FixedFunctionLightStorage::FFLightInstanceData detached;
	FF_LIGHT_CHECK(p_storage.light_instance_get_data(instance, detached) == OK && !detached.light.is_valid());
	FF_LIGHT_CHECK(p_storage.free(instance) == OK);
	observer.light = p_storage.light_create(RSE::LIGHT_SPOT);
	FF_LIGHT_CHECK(p_storage.light_instance_create(observer.light).is_valid());
	observer.tracker.update_begin();
	FF_LIGHT_CHECK(p_storage.light_update_dependency(observer.light, &observer.tracker) == OK);
	observer.tracker.update_end();
	FF_LIGHT_CHECK(p_storage.light_get_data(observer.light, data) == OK && p_storage.light_set_data(observer.light, data) == OK);
	observer.clear = true;
	FF_LIGHT_CHECK(p_storage.update_dependencies() == OK && observer.deletions == 2);
	FF_LIGHT_CHECK(!p_storage.get_light_count() && !p_storage.get_instance_count());
	return true;
}

bool TestFixedFunctionLightStorage::test(FixedFunctionRenderContext &p_context) {
	FixedFunctionLightStorage &storage = p_context.get_light_storage();
	FF_LIGHT_CHECK(!storage.get_light_count() && !storage.get_instance_count());
	RendererLightStorageFixedFunction facade(p_context);
	const RID allocated = facade.directional_light_allocate();
	FixedFunctionLightStorage::FFLightData data;
	FF_LIGHT_CHECK(allocated.is_valid() && storage.light_get_data(allocated, data) == ERR_INVALID_PARAMETER);
	facade.directional_light_initialize(allocated);
	FF_LIGHT_CHECK(facade.get_last_error() == OK && storage.light_get_data(allocated, data) == OK && data.type == RSE::LIGHT_DIRECTIONAL);
	FF_LIGHT_CHECK(storage.light_initialize(allocated, RSE::LIGHT_OMNI) == ERR_ALREADY_IN_USE);
	const RID instance = facade.light_instance_create(allocated);
	FF_LIGHT_CHECK(instance.is_valid());
	facade.light_set_color(allocated, Color(2, 3, 4));
	facade.light_set_param(allocated, RSE::LIGHT_PARAM_ENERGY, 7);
	facade.light_set_shadow(allocated, true);
	facade.light_set_negative(allocated, true);
	facade.light_set_cull_mask(allocated, 0x80000001);
	facade.light_set_shadow_caster_mask(allocated, 0x40000000);
	facade.light_set_projector(allocated, RID::from_uint64(0x123400000001));
	facade.light_set_distance_fade(allocated, true, 20, 30, 10);
	facade.light_directional_set_sky_mode(allocated, RSE::LIGHT_DIRECTIONAL_SKY_MODE_LIGHT_ONLY);
	FF_LIGHT_CHECK(facade.get_last_error() == OK && storage.light_get_data(allocated, data) == OK);
	FF_LIGHT_CHECK(data.color == Color(2, 3, 4) && data.parameters[RSE::LIGHT_PARAM_ENERGY] == 7 && data.cull_mask == 0x80000001);
	FF_LIGHT_CHECK(data.shadow_caster_mask == 0x40000000 && data.projector.is_valid() && data.distance_fade_enabled && data.distance_fade_begin == 20);
	FF_LIGHT_CHECK(data.flags & FixedFunctionLightStorage::LIGHT_SHADOW);
	FF_LIGHT_CHECK(data.flags & FixedFunctionLightStorage::LIGHT_NEGATIVE);
	FF_LIGHT_CHECK(facade.light_has_shadow(allocated) == p_context.get_device().supports_directional_shadows() && !facade.light_has_projector(allocated));
	const uint64_t version = data.version;
	data.color.r = std::numeric_limits<float>::quiet_NaN();
	FF_LIGHT_CHECK(storage.light_set_data(allocated, data) == ERR_INVALID_PARAMETER);
	FF_LIGHT_CHECK(storage.light_get_data(allocated, data) == OK && data.version == version && data.color == Color(2, 3, 4));
	data.parameters[RSE::LIGHT_PARAM_ENERGY] = std::numeric_limits<float>::infinity();
	FF_LIGHT_CHECK(storage.light_set_data(allocated, data) == ERR_INVALID_PARAMETER);
	Transform3D transform(Basis(), Vector3(3, 2, 1));
	FF_LIGHT_CHECK(storage.light_instance_set_transform(instance, transform) == OK);
	FixedFunctionLightStorage::FFLightInstanceData snapshot;
	FF_LIGHT_CHECK(storage.light_instance_get_data(instance, snapshot) == OK && snapshot.transform == transform);
	transform.origin.x = std::numeric_limits<real_t>::infinity();
	FF_LIGHT_CHECK(storage.light_instance_set_transform(instance, transform) == ERR_INVALID_PARAMETER);
	FF_LIGHT_CHECK(storage.light_instance_get_data(instance, snapshot) == OK && snapshot.transform.origin.x == 3);
	FixedFunctionLightStorage::FFShadowTransform shadow;
	FF_LIGHT_CHECK(storage.light_instance_get_shadow_transform(instance, shadow) == OK && !shadow.valid);
	Projection projection;
	projection.set_orthogonal(-2, 2, -2, 2, 0, 20);
	const Transform3D shadow_transform(Basis(), Vector3(0, 8, 0));
	facade.light_instance_set_shadow_transform(instance, projection, shadow_transform, 20, 20, 0, 0.25f, 20);
	FF_LIGHT_CHECK(facade.get_last_error() == OK && storage.light_instance_get_shadow_transform(instance, shadow) == OK);
	FF_LIGHT_CHECK(shadow.valid && shadow.projection == projection && shadow.transform == shadow_transform && shadow.texel_size == 0.25f && shadow.bias_scale == 20);
	FF_LIGHT_CHECK(storage.light_instance_set_shadow_transform(instance, projection, shadow_transform, 1, 1, 1) == ERR_UNAVAILABLE);
	FF_LIGHT_CHECK(storage.light_instance_set_shadow_transform(instance, projection, shadow_transform, 0, std::numeric_limits<float>::infinity(), 1) == ERR_INVALID_PARAMETER);
	FF_LIGHT_CHECK(storage.light_instance_get_shadow_transform(instance, shadow) == OK && shadow.texel_size == 0.25f && shadow.bias_scale == 20);
	facade.light_directional_set_shadow_mode(allocated, RSE::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS);
	FF_LIGHT_CHECK(storage.light_get_data(allocated, data) == OK && data.directional_shadow_mode == RSE::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS);
	FF_LIGHT_CHECK(facade.light_directional_get_shadow_mode(allocated) == (facade.light_has_shadow(allocated) ? RSE::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL : data.directional_shadow_mode));
	facade.directional_shadow_atlas_set_size(0, false);
	FF_LIGHT_CHECK(!facade.light_has_shadow(allocated) && facade.get_directional_light_shadow_size(instance) == 0);
	facade.directional_shadow_atlas_set_size(1024, false);
	facade.light_free(allocated);
	FF_LIGHT_CHECK(storage.light_instance_get_data(instance, snapshot) == OK && !snapshot.light.is_valid());
	facade.light_instance_free(instance);
	const RID uninitialized = storage.light_allocate();
	FF_LIGHT_CHECK(storage.free(uninitialized) == OK);
	FF_LIGHT_CHECK(_dependencies(storage));
	printf("PASS: light allocation/initialization, source validation/versions, facade metadata, detached instances and mutation/free/clear-safe native dependencies.\n");
	return true;
}

#undef FF_LIGHT_CHECK
