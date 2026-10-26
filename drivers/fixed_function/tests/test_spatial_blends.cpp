#include "test_retained_scene.h"

#include "core/project_settings.h"

#include <stdio.h>

#define FF_BLEND_CHECK(m_condition)                                                 \
	do {                                                                            \
		if (!(m_condition)) {                                                       \
			printf("Spatial blends FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                           \
		}                                                                           \
	} while (false)

bool TestFixedFunctionRetainedScene::_test_spatial_blends(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID shader = materials.shader_create(), material = materials.material_create();
	const RID texture = textures.texture_create(), mesh = meshes.mesh_create();
	FF_BLEND_CHECK(_add(meshes, mesh) && materials.material_set_shader(material, shader) == OK);
	Ref<Image> image = memnew(Image(2, 2, false, Image::FORMAT_RGBA8));
	image->fill(Color(1, 0, 0, 0));
	FF_BLEND_CHECK(textures.texture_set_data(texture, image) == OK);
	FF_BLEND_CHECK(materials.material_set_param(material, "image", texture) == OK);
	FFInstance instance(mesh, material);
	FFInstance *instances[] = { &instance };
	const uint32_t width = p_context.get_device().get_width(), height = p_context.get_device().get_height();
	const size_t center = height / 2 * width + width / 2;
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = p_context.get_device().get_capabilities();
	const char *setting = "rendering/quality/fixed_function/retained_geometry";
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const bool previous = settings->get(setting);
	std::vector<uint32_t> pixels;
	// RGB software texture modulation and blending can round down at each operation.
	const int blend_tolerance = 2;
	FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
	for (int retained = 0; retained < 2; retained++) {
		settings->set(setting, retained != 0);
		const String additive = "shader_type spatial; render_mode unshaded,blend_add,cull_disabled; uniform sampler2D image; void fragment(){ ALBEDO=texture(image,UV).rgb; $ALPHA }";
		FF_BLEND_CHECK(materials.shader_set_code(shader, additive.replace("$ALPHA", "")) == OK && materials.material_get_spatial_parameters(material, parameters) == OK);
		FF_BLEND_CHECK(parameters.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD && !parameters.state.depth_write_enabled && !parameters.state.texture_alpha_enabled);
		FF_BLEND_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels));
		FF_BLEND_CHECK(_color_matches(pixels[center], caps.blend_add ? 0xffff00ff : 0xffff0000, blend_tolerance));
		FF_BLEND_CHECK(materials.shader_set_code(shader, additive.replace("$ALPHA", "ALPHA=texture(image,UV).a;")) == OK);
		FF_BLEND_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels));
		FF_BLEND_CHECK(pixels[center] == ((caps.blend_add || caps.blend_mix) ? 0xff0000ff : 0xffff0000));
		const String multiply = "shader_type spatial; render_mode unshaded,blend_mul,cull_disabled,depth_draw_always; void fragment(){ ALBEDO=vec3(1,0,0.5); }";
		FF_BLEND_CHECK(materials.shader_set_code(shader, multiply) == OK && materials.material_get_spatial_parameters(material, parameters) == OK);
		FF_BLEND_CHECK(parameters.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MUL && parameters.state.depth_write_enabled);
		FF_BLEND_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels));
		FF_BLEND_CHECK(_color_matches(pixels[center], caps.blend_mul ? 0xff000080 : 0xffff0080, blend_tolerance));
		if (caps.alpha_cutoff) {
			const String cutout = additive.replace("$ALPHA", "ALPHA=texture(image,UV).a; ALPHA_SCISSOR=0.5;");
			FF_BLEND_CHECK(materials.shader_set_code(shader, cutout) == OK && materials.material_get_spatial_parameters(material, parameters) == OK);
			FF_BLEND_CHECK(parameters.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD && parameters.state.alpha_cutoff_enabled);
			FF_BLEND_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && pixels[center] == 0xff0000ff);
		}
	}
	settings->set(setting, previous);
	if (caps.blend_add && caps.blend_mul) {
		const RID skipped = materials.material_create(), next = materials.material_create(), overlay = materials.material_create();
		const RID next_shader = materials.shader_create(), overlay_shader = materials.shader_create();
		FF_BLEND_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded,cull_disabled; void fragment(){ ALBEDO=vec3(0,1,0); }") == OK);
		FF_BLEND_CHECK(materials.shader_set_code(next_shader, "shader_type spatial; render_mode unshaded,blend_add,cull_disabled; void fragment(){ ALBEDO=vec3(0,0,0.5); }") == OK);
		FF_BLEND_CHECK(materials.shader_set_code(overlay_shader, "shader_type spatial; render_mode unshaded,blend_mul,cull_disabled; void fragment(){ ALBEDO=vec3(0,0.5,1); }") == OK);
		FF_BLEND_CHECK(materials.material_set_shader(next, next_shader) == OK && materials.material_set_shader(overlay, overlay_shader) == OK);
		FF_BLEND_CHECK(materials.material_set_next_pass(material, skipped) == OK && materials.material_set_next_pass(skipped, next) == OK);
		instance.material_overlay = overlay;
		FF_BLEND_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && _color_matches(pixels[center], 0xff008080, blend_tolerance));
		int objects = 0, draws = 0;
		FF_BLEND_CHECK(p_context.get_storage_queries().get_captured_render_info(VS::INFO_OBJECTS_IN_FRAME, objects) == OK && objects == 1);
		FF_BLEND_CHECK(p_context.get_storage_queries().get_captured_render_info(VS::INFO_DRAW_CALLS_IN_FRAME, draws) == OK && draws == 3);

		// An unrendered optional viewport input cannot discard the base or overlay.
		const RID target = textures.render_target_create();
		FF_BLEND_CHECK(textures.render_target_set_size(target, 2, 2) == OK);
		FF_BLEND_CHECK(materials.shader_set_code(next_shader, "shader_type spatial; render_mode unshaded,blend_add,cull_disabled; uniform sampler2D image; void fragment(){ ALBEDO=texture(image,UV).rgb; }") == OK);
		FF_BLEND_CHECK(materials.material_set_param(next, "image", textures.render_target_get_texture(target)) == OK);
		FF_BLEND_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && _color_matches(pixels[center], 0xff008000, blend_tolerance));
		FF_BLEND_CHECK(textures.free(target) == OK && materials.free(skipped) == OK && materials.free(next) == OK && materials.free(overlay) == OK);
		FF_BLEND_CHECK(materials.free(next_shader) == OK && materials.free(overlay_shader) == OK);
		printf("PASS: supported next passes and overlays, unsupported pass isolation, optional viewport deferral and unique object counts.\n");
	}
	FF_BLEND_CHECK(meshes.free(mesh) == OK && textures.free(texture) == OK && materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: spatial additive and multiply materials, alpha independence, optional cutoff and retained/transient equivalence.\n");
	return true;
}

#undef FF_BLEND_CHECK
