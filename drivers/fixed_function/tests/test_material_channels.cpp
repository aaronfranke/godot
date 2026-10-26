#include "core/config/project_settings.h"
#include "drivers/fixed_function/tests/test_lambert.h"

#include <stdio.h>

#define FF_MATERIAL_CHANNEL_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Material channels FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionLambert::_test_material_channels(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	const uint8_t texels[] = { 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 0, 0, 255 };
	FixedFunctionSceneRenderer::FFOcclusionSample mirrored;
	mirrored.rgba = texels;
	mirrored.size = Size2i(2, 2);
	mirrored.sampler_flags = FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_U |
			FixedFunctionGraphicsDevice::SAMPLER_REPEAT_V | FixedFunctionGraphicsDevice::SAMPLER_REPEAT_MIRRORED;
	FixedFunctionSpatialMaterial::FFOcclusionChannel red;
	FF_MATERIAL_CHANNEL_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(mirrored, red, Vector2(-0.75, 1)) == 1.0f);
	FF_MATERIAL_CHANNEL_CHECK(FixedFunctionSceneRenderer::_sample_occlusion(mirrored, red, Vector2(1.75, 1)) == 0.0f);
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	const RID mesh = _mesh(meshes, 0, true);
	FF_MATERIAL_CHANNEL_CHECK(mesh.is_valid() && materials.material_set_shader(material, shader) == OK);
	FFInstance instance(p_context, mesh, material);
	// Nonconstant depth exercises the depth test of the additive pass.
	instance.transform.basis = Basis(Vector3(1, 0, 0), real_t(0.16));
	FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	environment.ambient = Color(0.25f, 0.25f, 0.25f);
	const RID light = lights.light_create(RSE::LIGHT_DIRECTIONAL);
	const RID light_instance = lights.light_instance_create(light);
	FixedFunctionLightStorage::FFLightData light_data;
	FF_MATERIAL_CHANNEL_CHECK(lights.light_get_data(light, light_data) == OK);
	light_data.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.25f;
	FF_MATERIAL_CHANNEL_CHECK(lights.light_set_data(light, light_data) == OK);
	Transform3D light_transform;
	light_transform.basis = instance.transform.basis;
	FF_MATERIAL_CHANNEL_CHECK(lights.light_instance_set_transform(light_instance, light_transform) == OK);
	Vector<RID> active;
	active.push_back(light_instance);
	std::vector<uint32_t> pixels;
	const size_t center = 48 * 128 + 64;
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const bool previous_emission = GLOBAL_GET("rendering/quality/fixed_function/emission");
	const bool previous_occlusion = GLOBAL_GET("rendering/quality/fixed_function/texture_occlusion");
	settings->set("rendering/quality/fixed_function/emission", true);
	settings->set("rendering/quality/fixed_function/texture_occlusion", true);

	const RID glow = textures.texture_create();
	Ref<Image> image = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	image->fill(Color(0, 0, 0, 0));
	image->set_pixel(1, 1, Color(0, 0.5f, 0, 0));
	FF_MATERIAL_CHANNEL_CHECK(textures.texture_set_data(glow, image) == OK);
	const String emission =
			"shader_type spatial; render_mode cull_disabled; uniform sampler2D glow : source_color, hint_default_black, filter_nearest, repeat_disable; "
			"uniform vec4 tint : source_color; uniform float energy=0.5; "
			"void vertex(){ UV2 = UV2 + vec2(0.75); } "
			"void fragment(){ ALBEDO=vec3(0.5); vec3 glow_tex=texture(glow,UV2).rgb; EMISSION=(tint.rgb+glow_tex)*energy; }";
	FF_MATERIAL_CHANNEL_CHECK(materials.shader_set_code(shader, emission) == OK);
	FF_MATERIAL_CHANNEL_CHECK(materials.material_set_param(material, "tint", Color(0.25f, 0, 0)) == OK);
	FF_MATERIAL_CHANNEL_CHECK(materials.material_set_param(material, "glow", glow) == OK);
	FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
	FF_MATERIAL_CHANNEL_CHECK(materials.material_get_spatial_parameters(material, parameters) == OK);
	FF_MATERIAL_CHANNEL_CHECK(parameters.emission.color.texture == glow && parameters.emission.color.uv_set == 1 && parameters.emission.additive);
	const bool add_supported = p_context.get_device().get_capabilities().blend_add;
	FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_MATERIAL_CHANNEL_CHECK(_color(pixels[center], add_supported ? 0xff608040 : 0xff404040));
	// Check the whole interior for missing emission due to an independently transformed depth.
	if (add_supported) {
		for (int y = 40; y < 58; y++) {
			for (int x = 59; x < 69; x++) {
				FF_MATERIAL_CHANNEL_CHECK(_color(pixels[y * 128 + x], 0xff608040));
			}
		}
		Projection perspective;
		perspective.set_perspective(60, real_t(128) / 96, 0.1, 20);
		PagedArrayPool<RenderGeometryInstance *> pool;
		PagedArray<RenderGeometryInstance *> instances;
		instances.set_page_pool(&pool);
		instances.push_back(&instance);
		FF_MATERIAL_CHANNEL_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
		FF_MATERIAL_CHANNEL_CHECK(p_renderer.render(Transform3D(), perspective, instances, nullptr, environment, active.ptr(), active.size()) == OK && p_targets.end_frame(false) == OK);
		FF_MATERIAL_CHANNEL_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK);
		for (int y = 42; y < 55; y++) {
			for (int x = 60; x < 68; x++) {
				FF_MATERIAL_CHANNEL_CHECK(_color(pixels[y * 128 + x], 0xff608040));
			}
		}
	}
	const String multiply = emission.replace("tint.rgb+glow_tex", "tint.rgb*glow_tex");
	FF_MATERIAL_CHANNEL_CHECK(materials.shader_set_code(shader, multiply) == OK && materials.material_set_param(material, "tint", Color(1, 0.5f, 1)) == OK);
	FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_MATERIAL_CHANNEL_CHECK(_color(pixels[center], add_supported ? 0xff406040 : 0xff404040));
	FF_MATERIAL_CHANNEL_CHECK(materials.material_set_param(material, "glow", Variant()) == OK);
	FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff404040));

	const RID ao = textures.texture_create();
	// RGB8 requires a cached CPU conversion. Distinct RGB values test channel selection.
	image = Image::create_empty(2, 2, false, Image::FORMAT_RGB8);
	image->fill(Color(1, 0.25f, 0.75f));
	FF_MATERIAL_CHANNEL_CHECK(textures.texture_set_data(ao, image) == OK);
	const String ao_shader = emission.replace("uniform vec4 tint", "uniform sampler2D orm; uniform vec4 weights; uniform float affect=0.5; uniform vec4 tint")
									 .replace("EMISSION=", "vec4 packed=texture(orm,UV2); METALLIC=packed.b; ROUGHNESS=packed.g; AO=dot(texture(orm,UV2),weights); AO_LIGHT_AFFECT=affect; EMISSION=");
	FF_MATERIAL_CHANNEL_CHECK(materials.shader_set_code(shader, ao_shader) == OK);
	FF_MATERIAL_CHANNEL_CHECK(materials.material_set_param(material, "orm", ao) == OK && materials.material_set_param(material, "weights", Color(0, 1, 0, 0)) == OK);
	FF_MATERIAL_CHANNEL_CHECK(materials.material_set_param(material, "tint", Color(0.25f, 0, 0)) == OK);
	FF_MATERIAL_CHANNEL_CHECK(materials.material_get_spatial_parameters(material, parameters) == OK);
	FF_MATERIAL_CHANNEL_CHECK(parameters.occlusion.texture == ao && parameters.occlusion.uv_set == 1 && parameters.occlusion.weights == Color(0, 1, 0, 0));
	FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_MATERIAL_CHANNEL_CHECK(_color(pixels[center], add_supported ? 0xff3c1c1c : 0xff1c1c1c));
	settings->set("rendering/quality/fixed_function/emission", false);
	FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff1c1c1c));
	settings->set("rendering/quality/fixed_function/texture_occlusion", false);
	FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff404040));
	settings->set("rendering/quality/fixed_function/emission", true);
	FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_MATERIAL_CHANNEL_CHECK(_color(pixels[center], add_supported ? 0xff604040 : 0xff404040));
	settings->set("rendering/quality/fixed_function/texture_occlusion", true);
	// Packed ORM uses the red selector. A deferred texture edit must invalidate the CPU sample.
	FF_MATERIAL_CHANNEL_CHECK(materials.shader_set_code(shader, ao_shader.replace("dot(texture(orm,UV2),weights)", "packed.r")) == OK);
	image->fill(Color(0.5f, 0.25f, 0.75f));
	FF_MATERIAL_CHANNEL_CHECK(textures.texture_request_data(ao, image) == OK);
	FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_MATERIAL_CHANNEL_CHECK(_color(pixels[center], add_supported ? 0xff482828 : 0xff282828));
	const RID replacement = textures.texture_create();
	image->fill(Color(0.75f, 0.25f, 0.75f));
	FF_MATERIAL_CHANNEL_CHECK(textures.texture_request_data(replacement, image) == OK && textures.texture_replace(ao, replacement) == OK);
	FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_MATERIAL_CHANNEL_CHECK(_color(pixels[center], add_supported ? 0xff543434 : 0xff343434));
	if (p_context.get_device().supports_vertex_ambient_occlusion()) {
		const char *native = "rendering/quality/fixed_function/native_lighting";
		const char *per_pixel = "rendering/quality/fixed_function/per_pixel_lighting";
		const int previous_native = GLOBAL_GET(native);
		const bool previous_per_pixel = GLOBAL_GET(per_pixel);
		settings->set(per_pixel, false);
		settings->set("rendering/quality/fixed_function/emission", false);
		const RID proxy = textures.texture_create();
		FF_MATERIAL_CHANNEL_CHECK(textures.texture_set_proxy(proxy, ao) == OK);
		FF_MATERIAL_CHANNEL_CHECK(materials.shader_set_code(shader, ao_shader) == OK && materials.material_set_param(material, "orm", proxy) == OK && materials.material_set_param(material, "affect", 0.0f) == OK);
		settings->set(native, 0);
		FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff282828));
		const uint32_t reference = pixels[center];
		settings->set(native, 1);
		FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels)); // Queue the bake.
		FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], reference));

		FixedFunctionGeometryCache &cache = p_context.get_geometry_cache();
		FixedFunctionMeshStorage::FFSurfaceData surface;
		FF_MATERIAL_CHANNEL_CHECK(meshes.mesh_surface_get_data(mesh, 0, surface) == OK && materials.material_get_spatial_parameters(material, parameters) == OK);
		FixedFunctionGeometryCache::FFAmbientOcclusion baked(parameters.occlusion, textures.texture_get_cpu_image_version(proxy));
		RID geometry;
		FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
		Vector3 anchor, extent;
		FF_MATERIAL_CHANNEL_CHECK(cache.get(surface.identity, surface.version, 0, geometry, info, anchor, extent, true, -1, false, &baked) == OK);
		const RID original_geometry = geometry;
		const FixedFunctionGeometryCache::FFAmbientOcclusion original_bake = baked;
		const int entry_count = cache.get_entry_count();
		const RID duplicate = materials.material_create();
		FF_MATERIAL_CHANNEL_CHECK(materials.material_set_shader(duplicate, shader) == OK && materials.material_set_param(duplicate, "orm", proxy) == OK &&
				materials.material_set_param(duplicate, "weights", Color(0, 1, 0, 0)) == OK && materials.material_set_param(duplicate, "affect", 0.0f) == OK);
		FFInstance duplicate_instance(p_context, mesh, duplicate);
		duplicate_instance.transform = instance.transform;
		FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, duplicate_instance, environment, active, pixels) && _color(pixels[center], reference));
		FF_MATERIAL_CHANNEL_CHECK(cache.get_entry_count() == entry_count && materials.free(duplicate) == OK);
		FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], reference));
		List<FixedFunctionTextureStorage::FFTextureUsage> usage;
		FF_MATERIAL_CHANNEL_CHECK(textures.texture_debug_usage(&usage) == OK);
		bool found_source = false;
		for (const FixedFunctionTextureStorage::FFTextureUsage &texture : usage) {
			if (texture.texture == ao) {
				found_source = true;
				FF_MATERIAL_CHANNEL_CHECK(texture.cpu_bytes == uint64_t(image->get_data().size()));
			}
		}
		FF_MATERIAL_CHANNEL_CHECK(found_source);

		image->fill(Color(0, 0.75f, 0));
		FF_MATERIAL_CHANNEL_CHECK(textures.texture_request_data_partial(ao, image, Rect2i(0, 0, 1, 1), Point2i(1, 1)) == OK);
		baked.image_version = textures.texture_get_cpu_image_version(proxy);
		FF_MATERIAL_CHANNEL_CHECK(cache.get(surface.identity, surface.version, 0, geometry, info, anchor, extent, true, -1, false, &baked) == ERR_DOES_NOT_EXIST);
		// The settling and queued frames both draw the latest CPU data.
		for (int i = 0; i < 3; i++) {
			FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff383838));
		}
		FF_MATERIAL_CHANNEL_CHECK(cache.get(surface.identity, surface.version, 0, geometry, info, anchor, extent, true, -1, false, &baked) == OK && geometry != original_geometry);

		const RID new_source = textures.texture_create();
		image->fill(Color(1, 0, 0));
		FF_MATERIAL_CHANNEL_CHECK(textures.texture_request_data(new_source, image) == OK && textures.texture_set_proxy(proxy, new_source) == OK);
		for (int i = 0; i < 2; i++) {
			FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff202020));
		}
		// Changing the channel weights also invalidates the bake without an image write.
		FF_MATERIAL_CHANNEL_CHECK(materials.material_set_param(material, "weights", Color(1, 0, 0, 0)) == OK);
		for (int i = 0; i < 2; i++) {
			FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff404040));
		}
		const int entries_before_edits = cache.get_entry_count();
		FF_MATERIAL_CHANNEL_CHECK(materials.material_get_spatial_parameters(material, parameters) == OK);
		baked = FixedFunctionGeometryCache::FFAmbientOcclusion(parameters.occlusion, 0);
		for (int i = 0; i < 10; i++) {
			image->fill(Color(i % 2 == 0 ? 0.5f : 1.0f, 0, 0));
			FF_MATERIAL_CHANNEL_CHECK(textures.texture_request_data(new_source, image) == OK);
			FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], i % 2 == 0 ? 0xff303030 : 0xff404040));
			FF_MATERIAL_CHANNEL_CHECK(textures.texture_get_cpu_image_version(proxy, true) == 0 && cache.get_entry_count() <= entries_before_edits);
			baked.image_version = textures.texture_get_cpu_image_version(proxy);
			FF_MATERIAL_CHANNEL_CHECK(cache.get(surface.identity, surface.version, 0, geometry, info, anchor, extent, true, -1, false, &baked) == ERR_DOES_NOT_EXIST);
		}
		FF_MATERIAL_CHANNEL_CHECK(cache.get(surface.identity, surface.version, 0, geometry, info, anchor, extent, true, -1, false, &original_bake) == ERR_DOES_NOT_EXIST);
		FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff404040));
		FF_MATERIAL_CHANNEL_CHECK(cache.get(surface.identity, surface.version, 0, geometry, info, anchor, extent, true, -1, false, &baked) == OK);
		// The same zero AO leaves the directional light intact above. With direct
		// AO enabled it must fall back and suppress the directional light too.
		FF_MATERIAL_CHANNEL_CHECK(materials.material_set_param(material, "weights", Color(0, 1, 0, 0)) == OK && materials.material_set_param(material, "affect", 1.0f) == OK);
		FF_MATERIAL_CHANNEL_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[center], 0xff000000));
		FF_MATERIAL_CHANNEL_CHECK(textures.free(proxy) == OK && textures.free(new_source) == OK);
		settings->set(native, previous_native);
		settings->set(per_pixel, previous_per_pixel);
		printf("PASS: retained ambient AO, CPU parity, shared bakes, decoded-copy retirement, texture churn and direct-light fallback.\n");
	}
	FF_MATERIAL_CHANNEL_CHECK(meshes.free(mesh) == OK && lights.free(light_instance) == OK && lights.free(light) == OK);
	FF_MATERIAL_CHANNEL_CHECK(materials.free(material) == OK && materials.free(shader) == OK && textures.free(glow) == OK && textures.free(ao) == OK);
	settings->set("rendering/quality/fixed_function/emission", previous_emission);
	settings->set("rendering/quality/fixed_function/texture_occlusion", previous_occlusion);
	printf("PASS: emission add/multiply, independent UV transform and alpha, AO channel and lighting weights, packed ORM, texture edits and quality switches.\n");
	return true;
}

#undef FF_MATERIAL_CHANNEL_CHECK
