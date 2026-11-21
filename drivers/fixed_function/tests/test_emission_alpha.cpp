#include "core/config/project_settings.h"
#include "drivers/fixed_function/tests/test_lambert.h"

#include <stdio.h>

#define FF_EMISSION_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Emission alpha FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionLambert::_test_emission_alpha(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	const RID mesh = _mesh(meshes, 0, true);
	FF_EMISSION_CHECK(mesh.is_valid() && materials.material_set_shader(material, shader) == OK);
	FFInstance instance(p_context, mesh, material);
	instance.transform.basis = Basis(Vector3(1, 0, 0), real_t(0.16));
	const RID mask = textures.texture_create();
	const RID glow = textures.texture_create();
	Ref<Image> image = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 0, 0.5f));
	FF_EMISSION_CHECK(textures.texture_set_data(mask, image) == OK);
	image->fill(Color(0, 0, 0, 0));
	image->set_pixel(1, 1, Color(0, 0.5f, 0, 0));
	FF_EMISSION_CHECK(textures.texture_set_data(glow, image) == OK);
	FF_EMISSION_CHECK(materials.material_set_param(material, "mask", mask) == OK && materials.material_set_param(material, "glow", glow) == OK);
	const String source =
			"shader_type spatial; render_mode cull_disabled$MODE; uniform sampler2D mask : filter_nearest, repeat_disable; uniform sampler2D glow : filter_nearest, repeat_disable; "
			"uniform vec4 tint : source_color = vec4(0,0,0,0.5); "
			"void vertex(){ UV2=UV2+vec2(0.75); } "
			"void fragment(){ ALBEDO=tint.rgb*texture(mask,UV).rgb; ALPHA=tint.a*texture(mask,UV).a$VERTEX; $CUTOFF EMISSION=$EMISSION; }";
	const bool separate = p_context.get_device().supports_texture_mask();
	const bool additive = p_context.get_device().get_capabilities().blend_add;
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const bool previous = GLOBAL_GET("rendering/quality/fixed_function/emission");
	settings->set("rendering/quality/fixed_function/emission", true);
	const Vector<RID> lights;
	const FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	std::vector<uint32_t> pixels;
	const size_t center = 48 * 128 + 64;
	// Retained and transient base draws must use the same opacity for emission.
	for (int vertex = 0; vertex < 2; vertex++) {
		for (int blend = 0; blend < 2; blend++) {
			const String code = source.replace("$MODE", blend ? ",blend_add" : "").replace("$VERTEX", vertex ? "*COLOR.a" : "").replace("$CUTOFF", "").replace("$EMISSION", "texture(glow,UV2).rgb");
			FF_EMISSION_CHECK(materials.shader_set_code(shader, code) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, lights, pixels));
			uint32_t expected = blend ? 0xff0000ff : 0xff0000bf;
			if (separate && additive) {
				expected |= 0x00002000;
			}
			if (!_color(pixels[center], expected)) {
				printf("Emission case vertex=%d blend=%d actual=%08x expected=%08x\n", vertex, blend, pixels[center], expected);
				return false;
			}
		}
	}
	const String mix = source.replace("$MODE", "").replace("$VERTEX", "").replace("$CUTOFF", "");
	FF_EMISSION_CHECK(materials.shader_set_code(shader, mix.replace("$EMISSION", "vec3(0,0.5,0)")) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, lights, pixels));
	FF_EMISSION_CHECK(_color(pixels[center], additive ? 0xff0020bf : 0xff0000bf));
	// A color texture's alpha must not affect constant material opacity.
	const String scalar_alpha = mix.replace("tint.a*texture(mask,UV).a", "tint.a").replace("tint.rgb*texture(mask,UV).rgb", "tint.rgb");
	FF_EMISSION_CHECK(materials.shader_set_code(shader, scalar_alpha.replace("$EMISSION", "texture(glow,UV2).rgb")) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, lights, pixels));
	FF_EMISSION_CHECK(_color(pixels[center], additive ? 0xff004080 : 0xff000080));
	// Cutout emission uses full RGB after the exact base alpha comparison.
	const String cutout = source.replace("$MODE", "").replace("$VERTEX", "").replace("$EMISSION", "texture(glow,UV2).rgb");
	for (int rejected = 0; rejected < 2; rejected++) {
		FF_EMISSION_CHECK(materials.shader_set_code(shader, cutout.replace("$CUTOFF", rejected ? "ALPHA_SCISSOR_THRESHOLD=0.3;" : "ALPHA_SCISSOR_THRESHOLD=0.2;")) == OK);
		FF_EMISSION_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, lights, pixels));
		uint32_t expected = rejected ? 0xff0000ff : 0xff000000;
		if (!rejected && separate && p_context.get_device().get_capabilities().blend_add_unweighted) {
			expected = 0xff008000;
		}
		FF_EMISSION_CHECK(_color(pixels[center], expected));
	}
	// A lower-capability draw retains its constant Add term and the base material.
	FixedFunctionGraphicsDevice::FFCapabilities &caps = const_cast<FixedFunctionGraphicsDevice::FFCapabilities &>(p_context.get_device().get_capabilities());
	const uint32_t old_stages = caps.max_texture_stages;
	caps.max_texture_stages = 1;
	FF_EMISSION_CHECK(materials.shader_set_code(shader, mix.replace("$EMISSION", "vec3(0,0.25,0)+texture(glow,UV2).rgb")) == OK);
	const bool fallback = _draw(p_context, p_targets, p_renderer, instance, environment, lights, pixels);
	caps.max_texture_stages = old_stages;
	FF_EMISSION_CHECK(fallback && _color(pixels[center], additive ? 0xff0010bf : 0xff0000bf));
	// Recreating native resources preserves the current CPU channel assignments.
	FF_EMISSION_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(128, 96) == OK);
	FF_EMISSION_CHECK(materials.shader_set_code(shader, mix.replace("$EMISSION", "texture(glow,UV2).rgb")) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, lights, pixels));
	FF_EMISSION_CHECK(_color(pixels[center], separate && additive ? 0xff0020bf : 0xff0000bf));
	const RID blocker_shader = materials.shader_create();
	const RID blocker_material = materials.material_create();
	FF_EMISSION_CHECK(materials.material_set_shader(blocker_material, blocker_shader) == OK);
	FF_EMISSION_CHECK(materials.shader_set_code(blocker_shader, "shader_type spatial; render_mode unshaded,cull_disabled; void fragment(){ ALBEDO=vec3(0,1,0); }") == OK);
	FFInstance blocker(p_context, mesh, blocker_material);
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	for (int front = 0; front < 2; front++) {
		blocker.transform.origin.z = front ? -1 : -4;
		for (int order = 0; order < 2; order++) {
			PagedArrayPool<RenderGeometryInstance *> pool;
			PagedArray<RenderGeometryInstance *> instances;
			instances.set_page_pool(&pool);
			instances.push_back(order ? &blocker : &instance);
			instances.push_back(order ? &instance : &blocker);
			FF_EMISSION_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
			FF_EMISSION_CHECK(p_renderer.render(Transform3D(), projection, instances, nullptr) == OK && p_targets.end_frame(false) == OK);
			FF_EMISSION_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
			uint32_t expected = front ? 0xff00ff00 : 0xff00bf00;
			if (!front && separate && additive) {
				expected = 0xff00df00;
			}
			FF_EMISSION_CHECK(_color(pixels[center], expected));
		}
	}
	FF_EMISSION_CHECK(materials.free(blocker_material) == OK && materials.free(blocker_shader) == OK);
	settings->set("rendering/quality/fixed_function/emission", false);
	FF_EMISSION_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, lights, pixels) && _color(pixels[center], 0xff0000bf));
	settings->set("rendering/quality/fixed_function/emission", previous);
	FF_EMISSION_CHECK(materials.free(material) == OK && materials.free(shader) == OK && meshes.free(mesh) == OK && textures.free(mask) == OK && textures.free(glow) == OK);
	printf("PASS: emission alpha weighting, independent cutout mask, constant/mapped channels, single-texture fallback, quality toggle and reset.\n");
	return true;
}

#undef FF_EMISSION_CHECK
