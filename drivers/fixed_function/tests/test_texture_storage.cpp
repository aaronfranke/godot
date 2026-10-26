#include "test_texture_storage.h"

#include "drivers/fixed_function/texture_storage.h"

#include <limits.h>
#include <stdio.h>

#define FIXED_FUNCTION_STORAGE_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Storage FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionTextureStorage::test(FixedFunctionGraphicsDevice &p_device) {
	FixedFunctionTextureStorage storage(p_device);
	FixedFunctionTextureStorage other(p_device);
	FIXED_FUNCTION_STORAGE_CHECK(storage.get_texture_count() == 0 && storage.get_render_target_count() == 0);
	const RID source = storage.texture_create();
	const RID proxy = storage.texture_create();
	const RID wrapper = storage.texture_create();
	const RID foreign = other.texture_create();
	FIXED_FUNCTION_STORAGE_CHECK(storage.owns_texture(source) && !storage.owns_render_target(source));
	FIXED_FUNCTION_STORAGE_CHECK(!other.owns_texture(source) && !storage.owns_texture(RID()));
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_size(source) == Size2i());
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_size(source, Size2i(16, 8)) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_size(proxy, Size2i(3, 5)) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_proxy(proxy, source) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_proxy(wrapper, proxy) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_size(proxy) == Size2i(3, 5));
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_size_with_proxy(proxy) == Size2i(16, 8));
	// GLES2 resolves one hop: this wrapper sees the proxy's own size, not the source's.
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_size_with_proxy(wrapper) == Size2i(3, 5));
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_proxy(source, wrapper) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(!storage.texture_get_proxy(source).is_valid());
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_proxy(proxy, proxy) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_proxy(proxy, foreign) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_proxy(proxy) == source);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_size(source, Size2i(-1, 8)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_size(source) == Size2i(16, 8));
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_size(foreign, Size2i(1, 1)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(storage.free(foreign) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(storage.free(RID()) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(other.owns_texture(foreign));

	const RID target = storage.render_target_create();
	const RID target_texture = storage.render_target_get_texture(target);
	FIXED_FUNCTION_STORAGE_CHECK(target.is_valid() && target_texture.is_valid() && target != target_texture);
	FIXED_FUNCTION_STORAGE_CHECK(storage.owns_render_target(target) && storage.owns_texture(target_texture));
	FIXED_FUNCTION_STORAGE_CHECK(!storage.owns_texture(target) && !storage.owns_render_target(target_texture));
	FIXED_FUNCTION_STORAGE_CHECK(storage.get_texture_count() == 4 && storage.get_render_target_count() == 1);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_render_target(target_texture) == target);
	FIXED_FUNCTION_STORAGE_CHECK(!storage.texture_get_render_target(source).is_valid());
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_get_rect(target) == Rect2i());
	FIXED_FUNCTION_STORAGE_CHECK(!storage.render_target_was_used(target));
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_set_position(target, -7, 11) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_set_size(target, 320, 240) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_get_rect(target).position == Point2i(-7, 11));
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_size(target_texture) == Size2i(320, 240));
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_set_size(target, 640, 480) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_get_texture(target) == target_texture);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_size(target_texture) == Size2i(640, 480));
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_set_size(target, -1, 240) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_size(target_texture) == Size2i(640, 480));
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_set_size(foreign, 1, 1) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_set_size(target_texture, 1, 1) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(storage.free(target_texture) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_size(target_texture, Size2i(1, 1)) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_proxy(target_texture, source) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_get_texture(target) == target_texture);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_render_target(target_texture) == target);

	// Retargeting must remove the old reverse link before the old source is freed.
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_proxy(proxy, target_texture) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.free(source) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_proxy(proxy) == target_texture);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_render_target(proxy) == target);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_size_with_proxy(proxy) == Size2i(640, 480));
	FIXED_FUNCTION_STORAGE_CHECK(!storage.texture_get_render_target(wrapper).is_valid());
	FIXED_FUNCTION_STORAGE_CHECK(!storage.render_target_was_used(target)); // Queries do not imply a draw.
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_mark_used(target) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_was_used(target));
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_clear_used(target) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(!storage.render_target_was_used(target));
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_mark_used(foreign) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_clear_used(RID()) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_set_size(target, 0, 0) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_size_with_proxy(proxy) == Size2i());
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_get_texture(target) == target_texture);
	// Preserve integer-exact metadata at the signed boundaries.
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_set_position(target, INT_MIN, INT_MAX) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.render_target_set_size(target, INT_MAX, INT_MAX) == OK);
	const Rect2i rect = storage.render_target_get_rect(target);
	FIXED_FUNCTION_STORAGE_CHECK(rect.position.x == INT_MIN && rect.position.y == INT_MAX);
	FIXED_FUNCTION_STORAGE_CHECK(rect.size.x == INT_MAX && rect.size.y == INT_MAX);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_size_with_proxy(proxy).x == INT_MAX);

	// Destroying a target releases its texture and detaches surviving viewport wrappers.
	FIXED_FUNCTION_STORAGE_CHECK(storage.free(target) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.get_render_target_count() == 0 && storage.get_texture_count() == 2);
	FIXED_FUNCTION_STORAGE_CHECK(!storage.texture_get_proxy(proxy).is_valid());
	FIXED_FUNCTION_STORAGE_CHECK(!storage.texture_get_render_target(proxy).is_valid());
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_size_with_proxy(proxy) == Size2i(3, 5));
	FIXED_FUNCTION_STORAGE_CHECK(storage.free(proxy) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(!storage.texture_get_proxy(wrapper).is_valid());
	FIXED_FUNCTION_STORAGE_CHECK(storage.free(wrapper) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.get_texture_count() == 0);

	// Exercise explicit record-list cleanup and repeated reuse.
	for (int pass = 0; pass < 2; pass++) {
		for (int i = 0; i < 32; i++) {
			const RID viewport = storage.render_target_create();
			const RID texture = storage.texture_create();
			FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_proxy(texture, storage.render_target_get_texture(viewport)) == OK);
		}
		FIXED_FUNCTION_STORAGE_CHECK(storage.get_render_target_count() == 32 && storage.get_texture_count() == 64);
		FIXED_FUNCTION_STORAGE_CHECK(storage.clear() == OK);
		FIXED_FUNCTION_STORAGE_CHECK(storage.clear() == OK);
		FIXED_FUNCTION_STORAGE_CHECK(storage.get_render_target_count() == 0 && storage.get_texture_count() == 0);
	}
	// Leave linked resources for the destructor, including a wrapper freed before its base.
	const RID remaining = storage.texture_create();
	const RID first_wrapper = storage.texture_create();
	const RID second_wrapper = storage.texture_create();
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_proxy(first_wrapper, remaining) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_set_proxy(second_wrapper, remaining) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.free(first_wrapper) == OK);
	FIXED_FUNCTION_STORAGE_CHECK(storage.texture_get_proxy(second_wrapper) == remaining);
	storage.render_target_create();
	printf("PASS: logical texture/target ownership, stable viewport textures, proxy detach, size metadata, usage tracking, and cleanup.\n");
	return true;
}

#undef FIXED_FUNCTION_STORAGE_CHECK
