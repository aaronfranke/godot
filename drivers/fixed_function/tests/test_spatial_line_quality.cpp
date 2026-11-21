#include "core/config/project_settings.h"
#include "drivers/fixed_function/tests/test_preset_scope.h"
#include "drivers/fixed_function/tests/test_spatial_lines.h"
#include "scene/resources/material.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>
#include <string.h>

#include <limits>

#define FF_QUALITY_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Line quality FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionSpatialLines::_quality(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	TestFixedFunctionPresetScope scope(p_context.get_device(), nullptr);
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	FF_QUALITY_CHECK(materials.material_set_shader(material, shader) == OK);
	FF_QUALITY_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded; uniform float fixed_function_line_width=5.0; void fragment(){ ALBEDO=vec3(1); }") == OK);
	FF_QUALITY_CHECK(materials.material_get_line_width(material) == 5 && materials.material_set_line_width(material, 3) == OK && materials.material_get_line_width(material) == 3);
	FF_QUALITY_CHECK(materials.material_set_param(material, "fixed_function_line_width", 1e100) == OK && materials.material_get_line_width(material) > 1e30f);
	FF_QUALITY_CHECK(double(materials.material_get_param(material, "fixed_function_line_width")) == 1e100);
	FF_QUALITY_CHECK(materials.material_set_param(material, "fixed_function_line_width", std::numeric_limits<double>::infinity()) == OK && materials.material_get_line_width(material) == 1);
	FF_QUALITY_CHECK(materials.material_set_param(material, "fixed_function_line_width", Variant()) == OK && materials.material_get_line_width(material) == 5);
	// Use actual generated material code, rather than accepting only hand-written shaders.
	Ref<StandardMaterial3D> preset;
	preset.instantiate();
	preset->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	preset->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	BaseMaterial3D::flush_changes();
	FF_QUALITY_CHECK(materials.shader_set_code(shader, RenderingServer::get_singleton()->shader_get_code(preset->get_shader_rid())) == OK);
	FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
	FF_QUALITY_CHECK(materials.material_set_param(material, "albedo", Color(1, 0, 0, 0.5)) == OK && materials.material_get_spatial_parameters(material, parameters) == OK);
	FF_QUALITY_CHECK(parameters.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MIX && !parameters.state.depth_write_enabled && parameters.albedo.factor.a == 0.5f);
	const String opaque = "shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=vec3(1,0,0); }";
	const String transparent = "shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.5; }";
	FF_QUALITY_CHECK(materials.shader_set_code(shader, transparent.replace("unshaded", "unshaded,depth_draw_always")) == OK && materials.material_get_spatial_parameters(material, parameters) == OK && parameters.state.depth_write_enabled);
	FF_QUALITY_CHECK(materials.shader_set_code(shader, opaque.replace("unshaded", "unshaded,depth_test_disabled,depth_draw_always")) == OK && materials.material_get_spatial_parameters(material, parameters) == OK && !parameters.state.depth_test_enabled && !parameters.state.depth_write_enabled);
	FF_QUALITY_CHECK(materials.shader_set_code(shader, opaque) == OK && materials.material_set_line_width(material, 3) == OK);
	Vector<Vector3> positions;
	positions.push_back(Vector3(-1, -real_t(1) / 128, 0)); // Fractional pixel position exposes coverage pixels.
	positions.push_back(Vector3(1, -real_t(1) / 128, 0));
	const RID mesh = _mesh(meshes, RSE::PRIMITIVE_LINES, positions);
	FF_QUALITY_CHECK(mesh.is_valid());
	FFInstance instance(p_context, mesh, material);
	instance.transform.origin.z = -2;
	instance.depth = 2;
	RenderGeometryInstance *instances[] = { &instance };
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	std::vector<uint32_t> pixels;
	FF_QUALITY_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels));
	const std::vector<uint32_t> aliased = pixels;
	settings->set("rendering/quality/fixed_function/antialiased_lines", true);
	FF_QUALITY_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && pixels != aliased);
	int partial = 0;
	for (int y = 44; y < 53; y++) {
		const uint32_t pixel = pixels[y * 128 + 64];
		const int red = (pixel >> 16) & 255;
		partial += red > 0 && red < 255;
	}
	FF_QUALITY_CHECK(partial >= 2 && _color(pixels[48 * 128 + 64], 0xffff0000));
	const std::vector<uint32_t> antialiased = pixels;
	FF_QUALITY_CHECK(materials.material_set_line_width(material, 1) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels));
	const int thin_red = (pixels[48 * 128 + 64] >> 16) & 255;
	FF_QUALITY_CHECK(thin_red > 0 && thin_red < 255); // The zero-width solid core still has a visible coverage fringe.
	FF_QUALITY_CHECK(_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 6); // No degenerate solid core or end caps.
	FF_QUALITY_CHECK(materials.material_set_line_width(material, 3) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && pixels == antialiased);
	FixedFunctionGraphicsDevice::FFCapabilities &caps = const_cast<FixedFunctionGraphicsDevice::FFCapabilities &>(p_context.get_device().get_capabilities());
	const uint32_t old_limit = caps.max_primitives_per_draw;
	caps.max_primitives_per_draw = 1;
	const bool batched = _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels);
	caps.max_primitives_per_draw = old_limit;
	FF_QUALITY_CHECK(batched && pixels == antialiased && _render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] == 10);
	FF_QUALITY_CHECK(_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 10);
	FF_QUALITY_CHECK(p_context.resize(160, 120) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels));
	FF_QUALITY_CHECK(p_context.resize(128, 96) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && pixels == antialiased);
	const bool old_mix = caps.blend_mix;
	caps.blend_mix = false;
	const bool fallback = _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels);
	caps.blend_mix = old_mix;
	FF_QUALITY_CHECK(fallback && pixels == aliased);
	// Coverage uses diffuse alpha only for an opaque material, even with an alpha-zero texture.
	Vector<uint8_t> bytes;
	bytes.resize(8);
	{
		uint8_t *write = bytes.ptrw();
		const uint8_t rgba[] = { 255, 255, 255, 0, 255, 255, 255, 0 };
		memcpy(write, rgba, sizeof(rgba));
	}
	const Ref<Image> image = Image::create_from_data(2, 1, false, Image::FORMAT_RGBA8, bytes);
	const RID texture = textures.texture_create();
	FF_QUALITY_CHECK(textures.texture_allocate_2d(texture, Size2i(2, 1), Image::FORMAT_RGBA8, 0, 0) == OK && textures.texture_set_data(texture, image) == OK);
	const String textured = "shader_type spatial; render_mode unshaded; uniform sampler2D image : filter_nearest, repeat_disable; void fragment(){ ALBEDO=vec3(1,0,0)*texture(image,UV).rgb; $ALPHA }";
	FF_QUALITY_CHECK(materials.shader_set_code(shader, textured.replace("$ALPHA", "")) == OK && materials.material_set_param(material, "image", texture) == OK);
	FF_QUALITY_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && pixels.size() == antialiased.size());
	// Modulating by white can round the red channel down by one on D3D7 RGB.
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_QUALITY_CHECK(pixels[i] == antialiased[i] || (((antialiased[i] >> 16) & 255) > 0 && pixels[i] == antialiased[i] - 0x00010000));
	}
	FF_QUALITY_CHECK(materials.shader_set_code(shader, textured.replace("$ALPHA", "ALPHA=texture(image,UV).a;")) == OK);
	FF_QUALITY_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && _color(pixels[48 * 128 + 64], 0xff0000ff));
	// Texture-alpha state must be rebound when returning from a coverage-only material.
	FF_QUALITY_CHECK(materials.shader_set_code(shader, opaque) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && pixels == antialiased);
	FF_QUALITY_CHECK(materials.shader_set_code(shader, transparent) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && _color(pixels[48 * 128 + 64], 0xff80007f));
	caps.blend_mix = false;
	const bool transparent_fallback = _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels);
	caps.blend_mix = old_mix;
	FF_QUALITY_CHECK(transparent_fallback && pixels == aliased);
	const RID green_shader = materials.shader_create();
	const RID green_material = materials.material_create();
	FF_QUALITY_CHECK(materials.material_set_shader(green_material, green_shader) == OK && materials.material_set_line_width(green_material, 3) == OK);
	FF_QUALITY_CHECK(materials.shader_set_code(green_shader, transparent.replace("vec3(1,0,0)", "vec3(0,1,0)")) == OK);
	FFInstance depth_far(p_context, mesh, green_material);
	depth_far.transform.origin.z = -4;
	depth_far.depth = 4;
	for (int order = 0; order < 2; order++) {
		RenderGeometryInstance *ordered[] = { order ? &depth_far : &instance, order ? &instance : &depth_far };
		FF_QUALITY_CHECK(_draw(p_context, p_targets, p_renderer, ordered, 2, projection, pixels) && _color(pixels[48 * 128 + 64], 0xff80403f));
		caps.blend_mix = false;
		const bool ordered_fallback = _draw(p_context, p_targets, p_renderer, ordered, 2, projection, pixels);
		caps.blend_mix = old_mix;
		FF_QUALITY_CHECK(ordered_fallback && _color(pixels[48 * 128 + 64], 0xffff0000));
	}
	FF_QUALITY_CHECK(materials.material_set_render_priority(green_material, 1) == OK);
	RenderGeometryInstance *ordered[] = { &depth_far, &instance };
	FF_QUALITY_CHECK(_draw(p_context, p_targets, p_renderer, ordered, 2, projection, pixels) && _color(pixels[48 * 128 + 64], 0xff40803f));
	// Sorting may interleave surfaces of one object with another object: count each object once.
	const RID top = materials.material_create();
	FF_QUALITY_CHECK(materials.material_set_shader(top, shader) == OK && materials.material_set_line_width(top, 3) == OK && materials.material_set_render_priority(top, 2) == OK);
	const RID multi = meshes.mesh_create();
	FixedFunctionMeshStorage::FFSurfaceData surface;
	FF_QUALITY_CHECK(meshes.mesh_surface_get_data(mesh, 0, surface) == OK);
	for (int i = 0; i < 2; i++) {
		FF_QUALITY_CHECK(meshes.mesh_add_surface(multi, surface.source) == OK);
		FF_QUALITY_CHECK(meshes.mesh_surface_set_material(multi, i, i ? top : material) == OK);
	}
	instance.data->base = multi;
	instance.data->material_override = RID();
	FF_QUALITY_CHECK(_draw(p_context, p_targets, p_renderer, ordered, 2, projection, pixels) && _color(pixels[48 * 128 + 64], 0xffa0401f));
	FF_QUALITY_CHECK(_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] == 2);
	FF_QUALITY_CHECK(_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 30);
	// An opaque triangle submitted after the lines still fills depth before transparency.
	positions.resize(0);
	positions.push_back(Vector3(-1, -1, 0));
	positions.push_back(Vector3(0, 1, 0));
	positions.push_back(Vector3(1, -1, 0));
	const RID triangle = _mesh(meshes, RSE::PRIMITIVE_TRIANGLES, positions);
	FFInstance occluder(p_context, triangle, green_material);
	occluder.transform.origin.z = -3;
	occluder.depth = 3;
	FF_QUALITY_CHECK(materials.shader_set_code(green_shader, "shader_type spatial; render_mode unshaded,cull_disabled; void fragment(){ ALBEDO=vec3(0,1,0); }") == OK);
	instance.data->base = mesh;
	instance.data->material_override = material;
	FFInstance hidden(p_context, mesh, material);
	hidden.transform.origin.z = -4;
	hidden.depth = 4;
	for (int order = 0; order < 2; order++) {
		RenderGeometryInstance *with_depth[] = { &hidden, order ? &instance : &occluder, order ? &occluder : &instance };
		FF_QUALITY_CHECK(_draw(p_context, p_targets, p_renderer, with_depth, 3, projection, pixels) && _color(pixels[48 * 128 + 64], 0xff807f00));
	}
	FF_QUALITY_CHECK(meshes.free(triangle) == OK && meshes.free(multi) == OK && meshes.free(mesh) == OK);
	FF_QUALITY_CHECK(materials.free(top) == OK && materials.free(material) == OK && materials.free(shader) == OK && materials.free(green_material) == OK && materials.free(green_shader) == OK && textures.free(texture) == OK);
	printf("PASS: spatial alpha blending, transparent depth/priority ordering, unique object counts, line coverage AA, texture-alpha isolation, capability fallback and resize recovery.\n");
	return true;
}

#undef FF_QUALITY_CHECK
