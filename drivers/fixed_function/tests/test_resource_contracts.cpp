#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"
#include "core/variant/typed_array.h"
#include "drivers/fixed_function/render_scene_buffers_fixed_function.h"
#include "drivers/fixed_function/renderer_utilities_fixed_function.h"
#include "drivers/fixed_function/tests/test_fixed_function_scene_cpu.h"
#include "tests/test_macros.h"

#include <stdio.h>

#include <limits>

#define FF_RESOURCE_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Resource contract FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

struct TestFixedFunctionVisibilityObserver : RefCounted {
	RendererUtilitiesFixedFunction *utilities;
	RID notifier;
	RID sibling;
	int entered;
	int exited;
	int changed;
	int deleted;
	bool rewrite_bounds;
	bool free_on_change;
	bool free_on_enter;
	bool valid_notification;

	TestFixedFunctionVisibilityObserver() :
			utilities(nullptr), entered(0), exited(0), changed(0), deleted(0), rewrite_bounds(false), free_on_change(false), free_on_enter(false), valid_notification(true) {}

	void enter() {
		entered++;
		if (free_on_enter) {
			utilities->free(notifier);
		}
	}

	void exit() { exited++; }

	static void bounds_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
		TestFixedFunctionVisibilityObserver *observer = static_cast<TestFixedFunctionVisibilityObserver *>(p_tracker->userdata);
		observer->changed++;
		observer->valid_notification &= p_notification == Dependency::DEPENDENCY_CHANGED_AABB;
		if (observer->rewrite_bounds) {
			observer->rewrite_bounds = false;
			observer->utilities->visibility_notifier_set_aabb(observer->notifier, AABB(Vector3(2, 3, 4), Vector3(5, 6, 7)));
		}
		if (observer->free_on_change) {
			observer->utilities->free(observer->notifier);
		}
	}

	static void resource_deleted(const RID &p_rid, DependencyTracker *p_tracker) {
		TestFixedFunctionVisibilityObserver *observer = static_cast<TestFixedFunctionVisibilityObserver *>(p_tracker->userdata);
		observer->deleted++;
		observer->valid_notification &= p_rid == observer->notifier;
		if (observer->sibling.is_valid()) {
			observer->utilities->free(observer->sibling);
			observer->sibling = RID();
		}
	}
};

bool TestFixedFunctionSceneCPU::test_resource_contracts() {
	FFUnusedDevice unused;
	FixedFunctionRenderContext context(unused, nullptr, 128, 96, false);
	FixedFunctionRenderTargetManager targets(context);
	RendererTextureStorageFixedFunction textures(context.get_texture_storage(), &targets);
	RendererMaterialStorageFixedFunction materials(context);
	RendererLightStorageFixedFunction lights(context);
	RendererSceneRenderFixedFunction scene(context, targets);
	RendererUtilitiesFixedFunction utilities(context, textures, materials, lights, scene);
	{
		const RID lightmap = lights.lightmap_allocate();
		FF_RESOURCE_CHECK(lightmap.is_valid());
		lights.lightmap_initialize(lightmap);
		FF_RESOURCE_CHECK(utilities.get_base_type(lightmap) == RSE::INSTANCE_LIGHTMAP);
		const RID instance = lights.lightmap_instance_create(lightmap);
		FF_RESOURCE_CHECK(instance.is_valid() && instance != lightmap);
		const PackedVector3Array points = { Vector3(1, 2, 3) };
		PackedColorArray sh;
		sh.resize(9);
		const PackedInt32Array tetrahedra = { 0, 0, 0, 0 };
		const PackedInt32Array bsp = { 1, 0, 0, 0, -1, -1 };
		lights.lightmap_set_probe_capture_data(lightmap, points, sh, tetrahedra, bsp);
		FF_RESOURCE_CHECK(lights.lightmap_get_probe_capture_points(lightmap) == points);
		FF_RESOURCE_CHECK(lights.lightmap_get_probe_capture_sh(lightmap) == sh);
		FF_RESOURCE_CHECK(lights.lightmap_get_probe_capture_tetrahedra(lightmap) == tetrahedra);
		FF_RESOURCE_CHECK(lights.lightmap_get_probe_capture_bsp_tree(lightmap) == bsp);
		lights.lightmap_set_probe_bounds(lightmap, AABB(Vector3(), Vector3(4, 4, 4)));
		FF_RESOURCE_CHECK(!lights.lightmap_get_aabb(lightmap).has_surface());
		RID deleted;
		DependencyTracker lightmap_tracker;
		lightmap_tracker.userdata = &deleted;
		lightmap_tracker.deleted_callback = [](const RID &p_rid, DependencyTracker *p_tracker) {
			*static_cast<RID *>(p_tracker->userdata) = p_rid;
		};
		lightmap_tracker.update_begin();
		utilities.base_update_dependency(lightmap, &lightmap_tracker);
		lightmap_tracker.update_end();
		FF_RESOURCE_CHECK(utilities.free(instance) && !utilities.free(instance));
		FF_RESOURCE_CHECK(utilities.free(lightmap) && deleted == lightmap);
		FF_RESOURCE_CHECK(utilities.get_base_type(lightmap) == RSE::INSTANCE_NONE);
	}
	{
		const RID target = textures.render_target_create();
		FF_RESOURCE_CHECK(target.is_valid());
		textures.render_target_set_use_debanding(target, true);
		FF_RESOURCE_CHECK(textures.get_last_error() == OK && !textures.render_target_is_using_debanding(target));
		scene.material_set_use_debanding(true);
		FF_RESOURCE_CHECK(scene.get_last_error() == OK);
		Ref<RenderSceneBuffersFixedFunction> buffers = scene.render_buffers_create();
		RenderSceneBuffersConfiguration config;
		config.set_render_target(target);
		config.set_internal_size(Size2i(64, 64));
		config.set_target_size(Size2i(64, 64));
		config.set_use_debanding(true);
		buffers->configure(&config);
		FF_RESOURCE_CHECK(buffers->get_error() == OK && buffers->get_render_target() == target);
		config.set_msaa_3d(RSE::VIEWPORT_MSAA_4X);
		config.set_screen_space_aa(RSE::VIEWPORT_SCREEN_SPACE_AA_FXAA);
		config.set_use_taa(true);
		buffers->configure(&config);
		FF_RESOURCE_CHECK(buffers->get_error() == OK && buffers->get_render_target() == target);
		config.set_use_debanding(false);
		buffers->configure(&config);
		buffers->set_use_debanding(true);
		FF_RESOURCE_CHECK(buffers->get_error() == OK);
		config.set_view_count(2);
		buffers->configure(&config);
		buffers->set_use_debanding(false);
		FF_RESOURCE_CHECK(buffers->get_error() == ERR_UNAVAILABLE);
		config.set_view_count(1);
		config.set_internal_size(Size2i(32, 32));
		buffers->configure(&config);
		FF_RESOURCE_CHECK(buffers->get_error() == OK && buffers->get_render_target() == target);
		textures.render_target_free(target);
	}
	Ref<TestFixedFunctionVisibilityObserver> observer;
	observer.instantiate();
	observer->utilities = &utilities;
	observer->notifier = utilities.visibility_notifier_allocate();
	FF_RESOURCE_CHECK(observer->notifier.is_valid());
	utilities.visibility_notifier_initialize(observer->notifier);
	FF_RESOURCE_CHECK(utilities.get_last_error() == OK && utilities.get_base_type(observer->notifier) == RSE::INSTANCE_VISIBLITY_NOTIFIER);
	FF_RESOURCE_CHECK(utilities.visibility_notifier_get_aabb(observer->notifier) == AABB());
	DependencyTracker tracker;
	tracker.userdata = observer.ptr();
	tracker.changed_callback = TestFixedFunctionVisibilityObserver::bounds_changed;
	tracker.deleted_callback = TestFixedFunctionVisibilityObserver::resource_deleted;
	tracker.update_begin();
	utilities.base_update_dependency(observer->notifier, &tracker);
	tracker.update_end();
	FF_RESOURCE_CHECK(utilities.get_last_error() == OK);
	utilities.visibility_notifier_set_aabb(observer->notifier, AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2)));
	const AABB latest(Vector3(1, 2, 3), Vector3(4, 5, 6));
	utilities.visibility_notifier_set_aabb(observer->notifier, latest);
	FF_RESOURCE_CHECK(utilities.visibility_notifier_get_aabb(observer->notifier) == latest && observer->changed == 0);
	utilities.update_dirty_resources();
	FF_RESOURCE_CHECK(utilities.get_last_error() == OK && observer->changed == 1 && observer->valid_notification);
	utilities.update_dirty_resources();
	FF_RESOURCE_CHECK(observer->changed == 1);

	utilities.visibility_notifier_set_callbacks(observer->notifier, callable_mp(observer.ptr(), &TestFixedFunctionVisibilityObserver::enter), callable_mp(observer.ptr(), &TestFixedFunctionVisibilityObserver::exit));
	utilities.visibility_notifier_call(observer->notifier, true, false);
	utilities.visibility_notifier_call(observer->notifier, false, true);
	FF_RESOURCE_CHECK(observer->entered == 1 && observer->exited == 0);
	MessageQueue::get_singleton()->flush();
	FF_RESOURCE_CHECK(observer->exited == 1);
	utilities.visibility_notifier_set_callbacks(observer->notifier, Callable(), Callable());
	utilities.visibility_notifier_call(observer->notifier, true, false);
	FF_RESOURCE_CHECK(observer->entered == 1 && utilities.get_last_error() == OK);

	// Reentrant AABB writes become a new event, rather than being lost or recursing.
	observer->rewrite_bounds = true;
	utilities.visibility_notifier_set_aabb(observer->notifier, latest);
	utilities.update_dirty_resources();
	FF_RESOURCE_CHECK(observer->changed == 2 && utilities.visibility_notifier_get_aabb(observer->notifier).position == Vector3(2, 3, 4));
	utilities.update_dirty_resources();
	FF_RESOURCE_CHECK(observer->changed == 3);
	observer->sibling = utilities.visibility_notifier_allocate();
	utilities.visibility_notifier_initialize(observer->sibling);
	const RID sibling = observer->sibling;
	observer->free_on_change = true;
	utilities.visibility_notifier_set_aabb(observer->notifier, latest);
	utilities.update_dirty_resources();
	FF_RESOURCE_CHECK(observer->changed == 4 && observer->deleted == 1 && observer->valid_notification);
	FF_RESOURCE_CHECK(utilities.get_base_type(observer->notifier) == RSE::INSTANCE_NONE && utilities.get_base_type(sibling) == RSE::INSTANCE_NONE);
	FF_RESOURCE_CHECK(!utilities.free(observer->notifier));

	// A copied callback remains safe when it destroys the resource containing it.
	observer->notifier = utilities.visibility_notifier_allocate();
	utilities.visibility_notifier_initialize(observer->notifier);
	observer->free_on_enter = true;
	utilities.visibility_notifier_set_callbacks(observer->notifier, callable_mp(observer.ptr(), &TestFixedFunctionVisibilityObserver::enter), Callable());
	utilities.visibility_notifier_call(observer->notifier, true, false);
	FF_RESOURCE_CHECK(observer->entered == 2 && utilities.get_base_type(observer->notifier) == RSE::INSTANCE_NONE);
	observer->free_on_enter = false;
	observer->notifier = utilities.visibility_notifier_allocate();
	utilities.visibility_notifier_initialize(observer->notifier);
	utilities.visibility_notifier_set_callbacks(observer->notifier, Callable(), callable_mp(observer.ptr(), &TestFixedFunctionVisibilityObserver::exit));
	utilities.visibility_notifier_call(observer->notifier, false, true);
	FF_RESOURCE_CHECK(utilities.free(observer->notifier));
	MessageQueue::get_singleton()->flush();
	FF_RESOURCE_CHECK(observer->exited == 2);
	const RID validated = utilities.visibility_notifier_allocate();
	utilities.visibility_notifier_initialize(validated);
	utilities.visibility_notifier_set_aabb(validated, latest);
	AABB invalid_bounds = latest;
	invalid_bounds.position.x = std::numeric_limits<real_t>::infinity();
	ERR_PRINT_OFF;
	utilities.visibility_notifier_set_aabb(validated, invalid_bounds);
	const Error bounds_error = utilities.get_last_error();
	utilities.visibility_notifier_initialize(validated);
	const Error initialize_error = utilities.get_last_error();
	utilities.base_update_dependency(validated, nullptr);
	const Error tracker_error = utilities.get_last_error();
	ERR_PRINT_ON;
	FF_RESOURCE_CHECK(bounds_error == ERR_INVALID_PARAMETER && initialize_error == ERR_INVALID_PARAMETER && tracker_error == ERR_INVALID_PARAMETER);
	FF_RESOURCE_CHECK(utilities.visibility_notifier_get_aabb(validated) == latest && utilities.free(validated));

	// Destruction also detaches dependencies, including allocated-only resources.
	observer->free_on_change = false;
	{
		RendererUtilitiesFixedFunction temporary(context, textures, materials, lights, scene);
		observer->utilities = &temporary;
		observer->notifier = temporary.visibility_notifier_allocate();
		temporary.visibility_notifier_initialize(observer->notifier);
		tracker.update_begin();
		temporary.base_update_dependency(observer->notifier, &tracker);
		tracker.update_end();
		temporary.visibility_notifier_allocate();
	}
	FF_RESOURCE_CHECK(observer->deleted == 2 && observer->valid_notification);

	// Valid absence queries do not claim that global uniforms or drawable blits work.
	FF_RESOURCE_CHECK(materials.global_shader_parameter_get_list().is_empty());
	FF_RESOURCE_CHECK(!textures.texture_drawable_get_default_material().is_valid());
	const RID texture = textures.texture_allocate();
	ERR_PRINT_OFF;
	materials.global_shader_parameter_add("unsupported", RSE::GLOBAL_VAR_TYPE_FLOAT, 1.0);
	const Error global_error = materials.get_last_error();
	utilities.visibility_notifier_call(RID(), true, false);
	const Error invalid_error = utilities.get_last_error();
	ERR_PRINT_ON;
	FF_RESOURCE_CHECK(global_error == ERR_UNAVAILABLE && invalid_error == ERR_INVALID_PARAMETER);
	FF_RESOURCE_CHECK(materials.global_shader_parameter_get_list().is_empty());
	const Color base_color(0.2f, 0.4f, 0.6f, 0.8f);
	textures.texture_drawable_initialize(texture, 500, 400, RSE::TEXTURE_DRAWABLE_FORMAT_RGBAF, base_color, true);
	FF_RESOURCE_CHECK(textures.get_last_error() == OK && textures.texture_size_with_proxy(texture) == Size2(500, 400));
	const Ref<Image> backing = textures.texture_2d_get(texture);
	FF_RESOURCE_CHECK(backing.is_valid() && backing->get_size() == Size2i(1, 1) && !backing->has_mipmaps());
	FF_RESOURCE_CHECK(backing->get_pixel(0, 0).is_equal_approx(base_color));
	textures.texture_drawable_blit_rect(TypedArray<RID>(), Rect2i(0, 0, 2, 2), RID(), Color(), TypedArray<RID>(), 0);
	FF_RESOURCE_CHECK(textures.get_last_error() == ERR_UNAVAILABLE);
	textures.texture_drawable_generate_mipmaps(texture);
	FF_RESOURCE_CHECK(textures.get_last_error() == ERR_UNAVAILABLE && textures.texture_2d_get(texture)->get_pixel(0, 0).is_equal_approx(base_color));
	textures.texture_free(texture);
	printf("PASS: notifier ownership/type, CPU bounds/coalescing, dependencies, immediate/deferred callbacks, reentrant writes/free, teardown and truthful absent-feature queries without native calls.\n");
	return true;
}

#undef FF_RESOURCE_CHECK
