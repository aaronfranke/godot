#include "test_viewport_recovery.h"

#include <stdio.h>

#define FIXED_FUNCTION_VIEWPORT_CHECK(m_condition)                                     \
	do {                                                                               \
		if (!(m_condition)) {                                                          \
			printf("Viewport recovery FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                              \
		}                                                                              \
	} while (false)

TestFixedFunctionViewportRecovery::FFViewportFixture::FFViewportFixture(VisualServerViewport &p_server, RID p_target, VS::ViewportUpdateMode p_mode) :
		server(p_server) {
	viewport.self = server.viewport_owner.make_rid(&viewport);
	viewport.render_target = p_target;
	viewport.size = Size2i(32, 32);
	server.viewport_set_update_mode(viewport.self, p_mode);
	server.viewport_set_active(viewport.self, true);
}

TestFixedFunctionViewportRecovery::FFViewportFixture::~FFViewportFixture() {
	server.viewport_set_active(viewport.self, false);
	server.viewport_owner.free(viewport.self);
}

bool TestFixedFunctionViewportRecovery::_render(FixedFunctionRenderContext &p_context, RID p_target, uint32_t p_color) {
	FIXED_FUNCTION_VIEWPORT_CHECK(p_context.begin_render_target(p_target, p_color) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(p_context.end_render_target() == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(!p_context.get_texture_storage().render_target_needs_redraw(p_target));
	return true;
}

bool TestFixedFunctionViewportRecovery::_copy_and_check(FixedFunctionRenderContext &p_context, FixedFunctionGraphicsDevice &p_device, RID p_target, uint32_t p_color, std::vector<uint32_t> &r_pixels) {
	RID native;
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	FIXED_FUNCTION_VIEWPORT_CHECK(storage.texture_get_device_texture(storage.render_target_get_texture(p_target), native) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(p_context.begin_device_frame(0) == OK);
	const FixedFunctionVertex quad[] = {
		{ -1, 1, 0, 0xffffffff, 0, 0 },
		{ 1, 1, 0, 0xffffffff, 1, 0 },
		{ 1, -1, 0, 0xffffffff, 1, 1 },
		{ -1, 1, 0, 0xffffffff, 0, 0 },
		{ 1, -1, 0, 0xffffffff, 1, 1 },
		{ -1, -1, 0, 0xffffffff, 0, 1 },
	};
	FIXED_FUNCTION_VIEWPORT_CHECK(p_device.draw_triangles(quad, 6, CameraMatrix(), native) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(p_context.end_frame(false) == OK);
	r_pixels.resize(size_t(p_device.get_width()) * p_device.get_height());
	FIXED_FUNCTION_VIEWPORT_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (size_t i = 0; i < r_pixels.size(); i++) {
		FIXED_FUNCTION_VIEWPORT_CHECK((r_pixels[i] >> 24) == 255);
		for (int shift = 0; shift < 24; shift += 8) {
			const int delta = int((r_pixels[i] >> shift) & 255) - int((p_color >> shift) & 255);
			FIXED_FUNCTION_VIEWPORT_CHECK(delta >= -8 && delta <= 8);
		}
	}
	return true;
}

bool TestFixedFunctionViewportRecovery::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	r_pixels.clear();
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.initialize() == OK);
	if (!p_device.get_capabilities().render_target_opaque) {
		printf("SKIP: viewport recovery requires opaque target support.\n");
		return true;
	}
	FixedFunctionTextureStorage &storage = context.get_texture_storage();
	FixedFunctionTextureStorage foreign_storage(p_device);
	VisualServerViewport viewports;
	const RID target = storage.render_target_create();
	const RID untouched = storage.render_target_create();
	const RID foreign = foreign_storage.render_target_create();
	FIXED_FUNCTION_VIEWPORT_CHECK(storage.render_target_set_size(target, 32, 32) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(storage.render_target_set_size(untouched, 32, 32) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(foreign_storage.render_target_set_size(foreign, 32, 32) == OK);
	FFViewportFixture once(viewports, target, VS::VIEWPORT_UPDATE_ONCE);
	FFViewportFixture disabled(viewports, untouched, VS::VIEWPORT_UPDATE_DISABLED);
	FFViewportFixture other(viewports, foreign, VS::VIEWPORT_UPDATE_ONCE);
	FIXED_FUNCTION_VIEWPORT_CHECK(!storage.render_target_needs_redraw(target));
	FIXED_FUNCTION_VIEWPORT_CHECK(!storage.render_target_is_ready(target));
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(storage.render_target_needs_redraw(target));
	FIXED_FUNCTION_VIEWPORT_CHECK(!storage.render_target_needs_redraw(untouched));
	FIXED_FUNCTION_VIEWPORT_CHECK(!foreign_storage.render_target_needs_redraw(foreign));
	// The server consumes ONCE even if the first drawing opportunity is skipped.
	viewports.viewport_set_update_mode(once.viewport.self, VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.resize(0, 0) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.begin_frame(0.25) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(once.viewport.update_mode == VS::VIEWPORT_UPDATE_ONCE);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.begin_render_target(target, 0) == ERR_SKIP);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.end_frame(false) == ERR_SKIP);
	viewports.viewport_set_update_mode(once.viewport.self, VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.begin_frame(0.25) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(once.viewport.update_mode == VS::VIEWPORT_UPDATE_ONCE);
	FIXED_FUNCTION_VIEWPORT_CHECK(_render(context, target, 0xffff0000));
	viewports.viewport_set_update_mode(once.viewport.self, VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(once.viewport.update_mode == VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(_copy_and_check(context, p_device, target, 0xffff0000, r_pixels));
	// Remember ONCE intent without overriding an explicitly disabled, still-ready viewport.
	viewports.viewport_set_update_mode(once.viewport.self, VS::VIEWPORT_UPDATE_ONCE);
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(storage.render_target_is_ready(target) && !storage.render_target_needs_redraw(target));
	viewports.viewport_set_update_mode(once.viewport.self, VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(once.viewport.update_mode == VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.begin_frame(0.25) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(_render(context, target, 0xff0000ff));
	viewports.viewport_set_update_mode(disabled.viewport.self, VS::VIEWPORT_UPDATE_ALWAYS);
	FIXED_FUNCTION_VIEWPORT_CHECK(_render(context, untouched, 0xffffffff));
	viewports.viewport_set_update_mode(disabled.viewport.self, VS::VIEWPORT_UPDATE_DISABLED);
	viewports.viewport_set_update_mode(once.viewport.self, VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(_copy_and_check(context, p_device, target, 0xff0000ff, r_pixels));
	// A resize queued during a target pass must not lose its recovery intent on completion.
	FIXED_FUNCTION_VIEWPORT_CHECK(context.begin_frame(0.25) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.begin_render_target(target, 0xff0000ff) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.resize(160, 120) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.end_render_target() == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.end_frame(false) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.begin_frame(0.25) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(storage.render_target_needs_redraw(target));
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(once.viewport.update_mode == VS::VIEWPORT_UPDATE_ONCE);
	FIXED_FUNCTION_VIEWPORT_CHECK(disabled.viewport.update_mode == VS::VIEWPORT_UPDATE_ONCE);
	FIXED_FUNCTION_VIEWPORT_CHECK(_render(context, target, 0xff00ff00));
	FIXED_FUNCTION_VIEWPORT_CHECK(_render(context, untouched, 0xffffffff));
	viewports.viewport_set_update_mode(disabled.viewport.self, VS::VIEWPORT_UPDATE_DISABLED);
	viewports.viewport_set_update_mode(once.viewport.self, VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(_copy_and_check(context, p_device, target, 0xff00ff00, r_pixels));
	// Intent belongs to the target, not a cache of active viewport handles.
	viewports.viewport_set_active(once.viewport.self, false);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.begin_frame(0.25) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(once.viewport.update_mode == VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(_render(context, untouched, 0xffffffff));
	viewports.viewport_set_update_mode(disabled.viewport.self, VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.end_frame(false) == OK);
	viewports.viewport_set_active(once.viewport.self, true);
	FIXED_FUNCTION_VIEWPORT_CHECK(context.begin_frame(0.25) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(once.viewport.update_mode == VS::VIEWPORT_UPDATE_ONCE);
	FIXED_FUNCTION_VIEWPORT_CHECK(_render(context, target, 0xff40a080));
	viewports.viewport_set_update_mode(once.viewport.self, VS::VIEWPORT_UPDATE_DISABLED);
	FIXED_FUNCTION_VIEWPORT_CHECK(_copy_and_check(context, p_device, target, 0xff40a080, r_pixels));
	FIXED_FUNCTION_VIEWPORT_CHECK(!storage.render_target_was_used(target));
	// Zero-sized and replacement targets must not inherit a stale ready state.
	FIXED_FUNCTION_VIEWPORT_CHECK(storage.render_target_set_size(target, 0, 0) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(!storage.render_target_needs_redraw(target));
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(once.viewport.update_mode == VS::VIEWPORT_UPDATE_DISABLED);
	once.viewport.render_target = RID();
	FIXED_FUNCTION_VIEWPORT_CHECK(storage.free(target) == OK);
	once.viewport.render_target = storage.render_target_create();
	FIXED_FUNCTION_VIEWPORT_CHECK(storage.render_target_set_size(once.viewport.render_target, 32, 32) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(FixedFunctionViewportRecovery::update(storage, viewports) == OK);
	FIXED_FUNCTION_VIEWPORT_CHECK(once.viewport.update_mode == VS::VIEWPORT_UPDATE_DISABLED);
	printf("PASS: frozen viewport recovery, skipped first draw, pending updates, current pixels, inactive lifetime, zero size, ownership and replacement.\n");
	return true;
}

#undef FIXED_FUNCTION_VIEWPORT_CHECK
