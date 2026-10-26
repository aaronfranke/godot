#include "drivers/fixed_function/tests/test_preset_scope.h"
#include "drivers/fixed_function/tests/test_unlit_scene.h"
#include "scene/resources/material.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>
#include <string.h>

#include <limits>

#define FF_CUTOUT_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Cutout FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionUnlitScene::_test_cutout_materials() {
	FixedFunctionMaterialStorage storage;
	const RID shader = storage.shader_create();
	const RID material = storage.material_create();
	FF_CUTOUT_CHECK(storage.material_set_shader(material, shader) == OK);
	for (int flags = 0; flags < 4; flags++) {
		Ref<StandardMaterial3D> preset;
		preset.instantiate();
		preset->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		preset->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
		preset->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, (flags & 1) != 0);
		preset->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, (flags & 2) != 0);
		BaseMaterial3D::flush_changes();
		const String code = RenderingServer::get_singleton()->shader_get_code(preset->get_shader_rid());
		FF_CUTOUT_CHECK(storage.shader_set_code(shader, code) == OK);
		FF_CUTOUT_CHECK(storage.material_set_param(material, "albedo", Color(0.5, 0.75, 1, 0.6)) == OK);
		FF_CUTOUT_CHECK(storage.material_set_param(material, "alpha_scissor_threshold", 0.4) == OK);
		FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
		const Error error = storage.material_get_spatial_parameters(material, parameters);
		if (error != OK) {
			printf("Cutout preset flags=%d error=%d\n%s\n", flags, error, code.utf8().get_data());
		}
		FF_CUTOUT_CHECK(error == OK && parameters.albedo.factor == Color(0.5, 0.75, 1, 0.6));
		FF_CUTOUT_CHECK(parameters.state.alpha_cutoff_enabled && parameters.state.alpha_cutoff == 0.4f);
		FF_CUTOUT_CHECK(parameters.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_DISABLED && parameters.state.depth_write_enabled);
		FF_CUTOUT_CHECK(parameters.albedo_uses_vertex_color == bool(flags & 1) && parameters.albedo_uses_vertex_alpha == bool(flags & 1));
	}
	const String source = "shader_type spatial; render_mode unshaded,cull_disabled; uniform float cutoff = 0.25; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.5; ALPHA_SCISSOR_THRESHOLD=cutoff; }";
	FF_CUTOUT_CHECK(storage.shader_set_code(shader, source) == OK);
	FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
	FF_CUTOUT_CHECK(storage.material_get_spatial_parameters(material, parameters) == OK && parameters.state.alpha_cutoff == 0.25f && parameters.albedo.factor.a == 0.5f);
	FF_CUTOUT_CHECK(storage.material_set_param(material, "cutoff", 1.1) == OK && storage.material_get_spatial_parameters(material, parameters) == OK && parameters.state.alpha_cutoff == 1.1f);
	FF_CUTOUT_CHECK(storage.material_set_param(material, "cutoff", -1) == OK && storage.material_get_spatial_parameters(material, parameters) == OK && parameters.state.alpha_cutoff == -1);
	FF_CUTOUT_CHECK(storage.material_set_param(material, "cutoff", Variant()) == OK && storage.material_get_spatial_parameters(material, parameters) == OK && parameters.state.alpha_cutoff == 0.25f);
	for (int invalid = 0; invalid < 3; invalid++) {
		Variant value;
		if (invalid == 0) {
			value = std::numeric_limits<double>::infinity();
		} else if (invalid == 1) {
			value = std::numeric_limits<double>::quiet_NaN();
		} else {
			value = "0.5";
		}
		FF_CUTOUT_CHECK(storage.material_set_param(material, "cutoff", value) == OK && storage.material_get_spatial_parameters(material, parameters) == ERR_INVALID_PARAMETER && parameters.state.alpha_cutoff == 0.25f);
	}
	FF_CUTOUT_CHECK(storage.material_set_param(material, "cutoff", Variant()) == OK);
	const char *unsupported[] = {
		"shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=vec3(1); ALPHA=sin(TIME); }",
		"shader_type spatial; render_mode unshaded,blend_sub; void fragment(){ ALBEDO=vec3(1); ALPHA=0.5; ALPHA_SCISSOR_THRESHOLD=0.4; }",
		"shader_type spatial; render_mode unshaded; uniform sampler2D a; uniform sampler2D b; void fragment(){ ALBEDO=texture(a,UV).rgb; ALPHA=texture(b,UV).a; ALPHA_SCISSOR_THRESHOLD=0.5; }",
		"shader_type spatial; render_mode unshaded; uniform sampler2D a; void fragment(){ ALBEDO=texture(a,UV).rgb; ALPHA=texture(a,UV2).a; ALPHA_SCISSOR_THRESHOLD=0.5; }",
		"shader_type spatial; render_mode unshaded; uniform sampler2D a; void fragment(){ ALBEDO=texture(a,UV).rgb; ALPHA=1.0; ALPHA_SCISSOR_THRESHOLD=0.5; }",
		"shader_type spatial; render_mode unshaded; uniform vec4 a; uniform vec4 b; void fragment(){ ALBEDO=a.rgb; ALPHA=b.a; ALPHA_SCISSOR_THRESHOLD=0.5; }",
	};
	for (unsigned int i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); i++) {
		FF_CUTOUT_CHECK(storage.shader_set_code(shader, unsupported[i]) == OK && storage.material_get_spatial_parameters(material, parameters) == ERR_UNAVAILABLE && parameters.state.alpha_cutoff == 0.25f);
	}
	FF_CUTOUT_CHECK(storage.shader_set_code(shader, source.replace("ALPHA=0.5;", "")) == OK);
	FF_CUTOUT_CHECK(storage.material_get_spatial_parameters(material, parameters) == OK && parameters.albedo.factor.a == 1);
	FF_CUTOUT_CHECK(storage.shader_set_code(shader, "shader_type spatial; render_mode unshaded; uniform vec4 tint : source_color=vec4(1,1,1,0.8); void fragment(){ ALBEDO=tint.rgb; ALPHA=0.5*tint.a*0.25; ALPHA_SCISSOR_THRESHOLD=0.05; }") == OK);
	FF_CUTOUT_CHECK(storage.material_get_spatial_parameters(material, parameters) == OK && Math::is_equal_approx(parameters.albedo.factor.a, 0.1f) && parameters.state.alpha_cutoff == 0.05f);
	return true;
}

bool TestFixedFunctionUnlitScene::_test_cutouts(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	const RID back_shader = materials.shader_create();
	const RID back_material = materials.material_create();
	FF_CUTOUT_CHECK(materials.material_set_shader(material, shader) == OK && materials.material_set_shader(back_material, back_shader) == OK);
	FF_CUTOUT_CHECK(materials.shader_set_code(back_shader, "shader_type spatial; render_mode unshaded,cull_disabled; void fragment(){ ALBEDO=vec3(0,1,0); }") == OK);
	const String source = "shader_type spatial; render_mode unshaded,cull_disabled; uniform sampler2D image : filter_nearest, repeat_disable; uniform vec4 tint : source_color = vec4(1,1,1,0.75); uniform float cutoff=0.3; void fragment(){ ALBEDO=tint.rgb*texture(image,UV).rgb$RGB; ALPHA=tint.a*texture(image,UV).a$ALPHA; ALPHA_SCISSOR_THRESHOLD=cutoff; }";
	Vector<uint8_t> bytes;
	bytes.resize(8);
	{
		uint8_t *write = bytes.ptrw();
		const uint8_t pixels[] = { 255, 0, 0, 64, 255, 0, 0, 255 };
		memcpy(write, pixels, sizeof(pixels));
	}
	Ref<Image> image = Image::create_from_data(2, 1, false, Image::FORMAT_RGBA8, bytes);
	const RID texture = textures.texture_create();
	FF_CUTOUT_CHECK(textures.texture_allocate_2d(texture, Size2i(2, 1), Image::FORMAT_RGBA8, 0, 0) == OK && textures.texture_set_data(texture, image) == OK);
	FF_CUTOUT_CHECK(materials.material_set_param(material, "image", texture) == OK);
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	std::vector<uint32_t> pixels(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	for (int compressed = 0; compressed < 2; compressed++) {
		Array arrays;
		arrays.resize(RSE::ARRAY_MAX);
		Vector<Vector3> positions;
		Vector<Vector2> uvs;
		Vector<Color> colors;
		const Vector2 corners[] = { Vector2(-1, -1), Vector2(-1, 1), Vector2(1, 1), Vector2(-1, -1), Vector2(1, 1), Vector2(1, -1) };
		for (int i = 0; i < 6; i++) {
			positions.push_back(Vector3(corners[i].x, corners[i].y, 0));
			uvs.push_back((corners[i] + Vector2(1, 1)) * 0.5);
			colors.push_back(Color(0.5, 1, 1, 0.5));
		}
		arrays[RSE::ARRAY_VERTEX] = positions;
		arrays[RSE::ARRAY_TEX_UV] = uvs;
		arrays[RSE::ARRAY_COLOR] = colors;
		RenderingServerTypes::SurfaceData data;
		FF_CUTOUT_CHECK(RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&data, RSE::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), compressed ? uint64_t(RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) : 0) == OK);
		const RID mesh = meshes.mesh_create();
		FF_CUTOUT_CHECK(meshes.mesh_add_surface(mesh, data) == OK);
		FFInstance front(p_context, mesh, material), back(p_context, mesh, back_material);
		front.transform.origin.z = -2;
		back.transform.origin.z = -4;
		for (int flags = 0; flags < 4; flags++) {
			const String code = source.replace("$RGB", flags & 1 ? "*COLOR.rgb" : "").replace("$ALPHA", flags & 2 ? "*COLOR.a" : "");
			FF_CUTOUT_CHECK(materials.shader_set_code(shader, code) == OK && materials.material_set_param(material, "cutoff", flags & 2 ? 0.3 : 0.6) == OK);
			for (int order = 0; order < 2; order++) {
				RenderGeometryInstance *instances[] = { order ? &back : &front, order ? &front : &back };
				FF_CUTOUT_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
				FF_CUTOUT_CHECK(_render(p_renderer, projection, instances, 2) == OK && p_targets.end_frame(false) == OK);
				FF_CUTOUT_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
				FF_CUTOUT_CHECK(_color(pixels[48 * CAPTURE_WIDTH + 48], 0xff00ff00) && _color(pixels[48 * CAPTURE_WIDTH + 80], flags & 1 ? 0xff800000 : 0xffff0000));
			}
		}
		FF_CUTOUT_CHECK(materials.shader_set_code(shader, source.replace("$RGB", "").replace("$ALPHA", "")) == OK);
		if (p_context.get_device().get_capabilities().blend_mix) {
			const size_t left_sample = 48 * CAPTURE_WIDTH + 48;
			const size_t right_sample = 48 * CAPTURE_WIDTH + 80;
			RenderGeometryInstance *opacity_layers[] = { &front, &back };
			FF_CUTOUT_CHECK(materials.material_set_param(material, "cutoff", 0.5) == OK);
			front.set_transparency(0.5);
			FF_CUTOUT_CHECK(p_targets.begin_frame(0) == OK && _render(p_renderer, projection, opacity_layers, 2) == OK && p_targets.end_frame(false) == OK);
			FF_CUTOUT_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
			FF_CUTOUT_CHECK(_color(pixels[left_sample], 0xff00ff00) && _color(pixels[right_sample], 0xff609f00));
			front.set_transparency(1);
			FF_CUTOUT_CHECK(p_targets.begin_frame(0) == OK && _render(p_renderer, projection, opacity_layers, 2) == OK && p_targets.end_frame(false) == OK);
			FF_CUTOUT_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
			FF_CUTOUT_CHECK(_color(pixels[left_sample], 0xff00ff00) && _color(pixels[right_sample], 0xff00ff00));
			front.set_transparency(0);
		}
		for (int extreme = 0; extreme < 2; extreme++) {
			FF_CUTOUT_CHECK(materials.material_set_param(material, "cutoff", extreme ? 1.1 : -1) == OK);
			RenderGeometryInstance *instances[] = { &front, &back };
			FF_CUTOUT_CHECK(p_targets.begin_frame(0) == OK && _render(p_renderer, projection, instances, 2) == OK && p_targets.end_frame(false) == OK);
			FF_CUTOUT_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK && _color(pixels[48 * CAPTURE_WIDTH + 48], extreme ? 0xff00ff00 : 0xffff0000) && _color(pixels[48 * CAPTURE_WIDTH + 80], extreme ? 0xff00ff00 : 0xffff0000));
		}
		FF_CUTOUT_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
		FF_CUTOUT_CHECK(materials.material_set_param(material, "cutoff", 0.5) == OK && _draw(p_context, p_targets, p_renderer, front, projection, pixels));
		FF_CUTOUT_CHECK(_color(pixels[48 * CAPTURE_WIDTH + 48], 0xff0000ff) && _color(pixels[48 * CAPTURE_WIDTH + 80], 0xffff0000));
		FF_CUTOUT_CHECK(materials.material_set_param(material, "tint", Color(1, 1, 1, 1)) == OK);
		for (int boundary = 0; boundary < 2; boundary++) {
			// Leave room for native modulation to round 255 down by one.
			const float cutoff = boundary ? 254.0f / 255.0f : 0.0f;
			FF_CUTOUT_CHECK(materials.material_set_param(material, "cutoff", cutoff) == OK && _draw(p_context, p_targets, p_renderer, front, projection, pixels));
			FF_CUTOUT_CHECK(_color(pixels[48 * CAPTURE_WIDTH + 48], boundary ? 0xff0000ff : 0xffff0000) && _color(pixels[48 * CAPTURE_WIDTH + 80], 0xffff0000));
		}
		// The native LDR draw copy is clamped while the CPU factor remains intact.
		FF_CUTOUT_CHECK(materials.material_set_param(material, "tint", Color(1, 1, 1, 2)) == OK);
		RenderGeometryInstance *instances[] = { &front };
		FF_CUTOUT_CHECK(p_targets.begin_frame(0) == OK && _render(p_renderer, projection, instances, 1) == OK && p_targets.end_frame(false) == OK);
		FF_CUTOUT_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK && _color(pixels[48 * CAPTURE_WIDTH + 80], 0xffff0000) && Color(materials.material_get_param(material, "tint")).a == 2);
		FF_CUTOUT_CHECK(materials.material_set_param(material, "tint", Variant()) == OK && materials.material_set_param(material, "cutoff", 0.5) == OK);
		FF_CUTOUT_CHECK(_draw(p_context, p_targets, p_renderer, front, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 80], 0xffff0000));

		p_renderer.set_debug_unshaded(false);
		const bool emission_supported = p_context.get_device().get_capabilities().blend_add_unweighted;
		const size_t left = 48 * CAPTURE_WIDTH + 48;
		const size_t right = 48 * CAPTURE_WIDTH + 80;
		const String lit_cutout = source.replace("unshaded,", "").replace("$RGB", "").replace("$ALPHA", "*COLOR.a").replace("ALPHA_SCISSOR_THRESHOLD=cutoff;", "ALPHA_SCISSOR_THRESHOLD=cutoff; EMISSION=$EMISSION;");
		FF_CUTOUT_CHECK(materials.material_set_param(material, "cutoff", 0.3) == OK);
		FF_CUTOUT_CHECK(materials.shader_set_code(shader, lit_cutout.replace("$EMISSION", "vec3(0.5,0.25,0)")) == OK);
		// Only the right texel survives, at alpha 0.375. The full emission
		// color is independent of both the red mask texel and its alpha.
		RenderGeometryInstance *layers[] = { &front, &back };
		FF_CUTOUT_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
		FF_CUTOUT_CHECK(_render(p_renderer, projection, layers, 2) == OK && p_targets.end_frame(false) == OK);
		FF_CUTOUT_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
		FF_CUTOUT_CHECK(_color(pixels[left], 0xff00ff00) && _color(pixels[right], emission_supported ? 0xff804000 : 0xff000000));
		FF_CUTOUT_CHECK(materials.material_set_param(material, "cutoff", 0.4) == OK);
		FF_CUTOUT_CHECK(_draw(p_context, p_targets, p_renderer, front, projection, pixels) && _color(pixels[right], 0xff0000ff));
		FF_CUTOUT_CHECK(materials.material_set_param(material, "cutoff", 0.3) == OK);

		FF_CUTOUT_CHECK(materials.shader_set_code(shader, lit_cutout.replace("$EMISSION", "vec3(0.5)*texture(image,UV).rgb")) == OK);
		FF_CUTOUT_CHECK(_draw(p_context, p_targets, p_renderer, front, projection, pixels));
		FF_CUTOUT_CHECK(_color(pixels[left], 0xff0000ff) && _color(pixels[right], emission_supported ? 0xff800000 : 0xff000000));
		// Without vertex alpha, D3D9 can use retained per-pixel lighting for the base pass.
		FF_CUTOUT_CHECK(materials.shader_set_code(shader, lit_cutout.replace("*COLOR.a", "").replace("$EMISSION", "vec3(0,0.5,0)+texture(image,UV).rgb")) == OK);
		FF_CUTOUT_CHECK(_draw(p_context, p_targets, p_renderer, front, projection, pixels));
		if (!_color(pixels[left], 0xff0000ff) || !_color(pixels[right], emission_supported ? 0xffff8000 : 0xff000000)) {
			printf("Cutout Add pixels left=%08x right=%08x\n", pixels[left], pixels[right]);
		}
		FF_CUTOUT_CHECK(_color(pixels[left], 0xff0000ff) && _color(pixels[right], emission_supported ? 0xffff8000 : 0xff000000));
		uint32_t separate_emission_color = 0xff000000;
		if (emission_supported) {
			separate_emission_color = p_context.get_device().supports_texture_mask() ? 0xffff8000 : 0xff008000;
		}
		FF_CUTOUT_CHECK(materials.shader_set_code(shader, lit_cutout.replace("$EMISSION", "vec3(0,0.5,0)+texture(image,UV2).rgb")) == OK);
		FF_CUTOUT_CHECK(_draw(p_context, p_targets, p_renderer, front, projection, pixels));
		FF_CUTOUT_CHECK(_color(pixels[left], 0xff0000ff) && _color(pixels[right], separate_emission_color));

		const String vertex_cutout = "shader_type spatial; render_mode cull_disabled; uniform sampler2D image : filter_nearest, repeat_disable; void fragment(){ ALBEDO=vec3(1); ALPHA=0.75*COLOR.a; ALPHA_SCISSOR_THRESHOLD=0.3; EMISSION=texture(image,UV).rgb; }";
		FF_CUTOUT_CHECK(materials.shader_set_code(shader, vertex_cutout) == OK && _draw(p_context, p_targets, p_renderer, front, projection, pixels));
		FF_CUTOUT_CHECK(_color(pixels[left], emission_supported ? 0xffff0000 : 0xff000000) && _color(pixels[right], pixels[left]));
		p_renderer.set_debug_unshaded(true);
		FF_CUTOUT_CHECK(meshes.free(mesh) == OK);
	}
	FF_CUTOUT_CHECK(materials.free(material) == OK && materials.free(back_material) == OK && materials.free(shader) == OK && materials.free(back_shader) == OK && textures.free(texture) == OK);
	printf("PASS: unlit cutout presets, independent RGB/alpha vertex inputs, texture modulation, threshold updates, discarded depth and recovery.\n");
	printf("PASS: cutout emission preserves coverage, keeps full brightness, and reuses compatible texture mappings.\n");
	return true;
}

#undef FF_CUTOUT_CHECK
