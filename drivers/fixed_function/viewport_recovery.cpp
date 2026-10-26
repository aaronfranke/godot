#include "viewport_recovery.h"

Error FixedFunctionViewportRecovery::update(FixedFunctionTextureStorage &p_storage, RendererViewport &p_viewports) {
	for (int i = 0; i < p_viewports.active_viewports.size(); i++) {
		RendererViewport::Viewport *viewport = p_viewports.active_viewports[i];
		const RID target = viewport->render_target;
		if (!p_storage.owns_render_target(target)) {
			continue;
		}
		// ONCE is consumed by the server even if native drawing is temporarily skipped.
		if (viewport->update_mode == RSE::VIEWPORT_UPDATE_ONCE) {
			const Error error = p_storage.render_target_mark_for_recovery(target);
			if (error != OK) {
				return error;
			}
		} else if (viewport->update_mode == RSE::VIEWPORT_UPDATE_DISABLED &&
				viewport->size.x > 1 && viewport->size.y > 1 && p_storage.render_target_needs_redraw(target)) {
			const Error error = p_storage.render_target_mark_for_recovery(target);
			if (error != OK) {
				return error;
			}
			p_viewports.viewport_set_update_mode(viewport->self, RSE::VIEWPORT_UPDATE_ONCE);
		}
	}
	return OK;
}
