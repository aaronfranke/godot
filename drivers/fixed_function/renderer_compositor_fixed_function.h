#pragma once

#include "drivers/fixed_function/renderer_canvas_render_fixed_function.h"
#include "drivers/fixed_function/renderer_fog_fixed_function.h"
#include "drivers/fixed_function/renderer_gi_fixed_function.h"
#include "drivers/fixed_function/renderer_light_storage_fixed_function.h"
#include "drivers/fixed_function/renderer_material_storage_fixed_function.h"
#include "drivers/fixed_function/renderer_mesh_storage_fixed_function.h"
#include "drivers/fixed_function/renderer_particles_storage_fixed_function.h"
#include "drivers/fixed_function/renderer_scene_render_fixed_function.h"
#include "drivers/fixed_function/renderer_texture_storage_fixed_function.h"
#include "drivers/fixed_function/renderer_utilities_fixed_function.h"
#include "servers/rendering/renderer_compositor.h"

// Restricted 2D/diffuse-3D profile. The platform owns the device and compositor factory.
class RendererCompositorFixedFunction : public RendererCompositor {
	FixedFunctionRenderContext _context;
	FixedFunctionRenderTargetManager _render_targets;
	RendererTextureStorageFixedFunction _textures;
	RendererMaterialStorageFixedFunction _materials;
	RendererCanvasRenderFixedFunction _canvas;
	RendererSceneRenderFixedFunction _scene;
	RendererLightStorageFixedFunction _lights;
	RendererMeshStorageFixedFunction _meshes;
	RendererParticlesStorageFixedFunction _particles;
	RendererFogFixedFunction _fog;
	RendererGIFixedFunction _gi;
	RendererUtilitiesFixedFunction _utilities;
	uint64_t _next_frame_usec = 0;
	enum { MIN_FRAME_INTERVAL_USEC = 250 };
	Error _last_error;
	static RendererCompositorFixedFunction *_singleton;

	void _report_error(const char *p_operation) const;

	RendererCompositorFixedFunction(const RendererCompositorFixedFunction &);
	RendererCompositorFixedFunction &operator=(const RendererCompositorFixedFunction &);

protected:
	FixedFunctionRenderContext &get_render_context() { return _context; }
	virtual void _wait_for_frame(uint32_t p_remaining_usec);

public:
	static RendererCompositorFixedFunction *get_singleton() { return _singleton; }
	static void make_current(RendererCompositor *(*p_factory)());
	RendererCanvasRender *get_canvas() override { return &_canvas; }
	RendererSceneRender *get_scene() override { return &_scene; }
	RendererFog *get_fog() override { return &_fog; }
	RendererGI *get_gi() override { return &_gi; }
	RendererLightStorage *get_light_storage() override { return &_lights; }
	RendererMaterialStorage *get_material_storage() override { return &_materials; }
	RendererMeshStorage *get_mesh_storage() override { return &_meshes; }
	RendererParticlesStorage *get_particles_storage() override { return &_particles; }
	RendererTextureStorage *get_texture_storage() override { return &_textures; }
	RendererUtilities *get_utilities() override { return &_utilities; }
	void blit_render_targets_to_screen(DisplayServerEnums::WindowID p_screen, const RenderingServerTypes::BlitToScreen *p_render_targets, int p_amount) override;
	void initialize() override;
	void finalize() override;
	void begin_frame(double p_frame_step) override;
	void end_frame(bool p_present) override;
	void set_boot_image_with_stretch(const Ref<Image> &p_image, const Color &p_color, RSE::SplashStretchMode p_stretch_mode, bool p_use_filter = true) override;
	bool is_opengl() override { return false; }
	bool is_xr_enabled() const override { return false; }
	void gl_end_frame(bool p_swap_buffers) override {}
	bool can_create_resources_async() const override { return false; }
	uint64_t get_frame_number() const override { return _context.get_frame_number(); }
	double get_frame_delta_time() const override { return _context.get_frame_delta_time(); }
	double get_total_time() const override { return _context.get_total_time(); }

	Error resize(uint32_t p_width, uint32_t p_height) { return _last_error = _context.resize(p_width, p_height); }
	Error set_use_vsync(bool p_enabled) { return _last_error = _context.set_use_vsync(p_enabled); }
	Error get_last_error() const { return _last_error; }
	bool is_initialized() const { return _context.is_initialized(); }

	RendererCompositorFixedFunction(FixedFunctionGraphicsDevice &p_device, void *p_native_window, uint32_t p_width, uint32_t p_height, bool p_vsync);
	~RendererCompositorFixedFunction() override;
};
