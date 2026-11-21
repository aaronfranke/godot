#include "core/config/project_settings.h"
#include "drivers/fixed_function/tests/test_retained_lighting.h"
#include "scene/resources/surface_tool.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>
#include <string.h>

#include <limits>

#define FF_NORMAL_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Normal mapping FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

RID TestFixedFunctionRetainedLighting::_normal_mesh(FixedFunctionMeshStorage &p_storage, uint64_t p_compression, float p_sign, bool p_tangent) {
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	Vector<Vector3> positions, normals;
	Vector<float> tangents;
	Vector<Vector2> uv, uv2;
	positions.push_back(Vector3(-1, -1, 0));
	positions.push_back(Vector3(0, 1, 0));
	positions.push_back(Vector3(1, -1, 0));
	for (int i = 0; i < 3; i++) {
		normals.push_back(Vector3(0, 0, 1));
		tangents.push_back(1);
		tangents.push_back(0);
		tangents.push_back(0);
		tangents.push_back(p_sign);
		uv.push_back(Vector2(0.25, 0.25));
		if (p_tangent) {
			uv2.push_back(Vector2(0.75, 0.75));
		} else {
			uv2.push_back(Vector2(0.75 + positions[i].x * 0.1, 0.75 + positions[i].y * 0.1));
		}
	}
	arrays[RSE::ARRAY_VERTEX] = positions;
	arrays[RSE::ARRAY_NORMAL] = normals;
	arrays[RSE::ARRAY_TEX_UV] = uv;
	arrays[RSE::ARRAY_TEX_UV2] = uv2;
	if (p_tangent) {
		arrays[RSE::ARRAY_TANGENT] = tangents;
	}
	RenderingServerTypes::SurfaceData data;
	RenderingServer *server = RenderingServer::get_singleton();
	if (server->mesh_create_surface_data_from_arrays(&data, RSE::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), p_compression) != OK) {
		return RID();
	}
	if (!p_tangent) {
		// The 4.x array helper synthesizes tangents whenever normals exist. Build
		// a real normal-only native layout to exercise upload-side tangent generation.
		uint32_t old_offsets[RSE::ARRAY_MAX], offsets[RSE::ARRAY_MAX];
		uint32_t vertex_stride, old_normal_stride, normal_stride, attribute_stride, skin_stride;
		server->mesh_surface_make_offsets_from_format(data.format, data.vertex_count, 0, old_offsets, vertex_stride, old_normal_stride, attribute_stride, skin_stride);
		const Vector<uint8_t> original = data.vertex_data;
		data.format &= ~uint64_t(RSE::ARRAY_FORMAT_TANGENT);
		server->mesh_surface_make_offsets_from_format(data.format, data.vertex_count, 0, offsets, vertex_stride, normal_stride, attribute_stride, skin_stride);
		Vector<uint8_t> packed;
		if (packed.resize(uint64_t(data.vertex_count) * (vertex_stride + normal_stride)) != OK) {
			return RID();
		}
		memcpy(packed.ptrw(), original.ptr(), uint64_t(data.vertex_count) * vertex_stride);
		for (uint32_t i = 0; i < data.vertex_count; i++) {
			memcpy(packed.ptrw() + offsets[RSE::ARRAY_NORMAL] + i * normal_stride, original.ptr() + old_offsets[RSE::ARRAY_NORMAL] + i * old_normal_stride, normal_stride);
		}
		data.vertex_data = packed;
	}
	const RID mesh = p_storage.mesh_create();
	return p_storage.mesh_add_surface(mesh, data) == OK ? mesh : RID();
}

bool TestFixedFunctionRetainedLighting::_normal_pixel(uint32_t p_actual, const Color &p_texel, float p_strength, float p_sign, const Basis &p_model, const Color &p_albedo, float p_occlusion, float p_light_affect) {
	const Vector3 n = p_model.inverse().transposed().xform(Vector3(0, 0, 1)).normalized();
	const Vector3 t = p_model.xform(Vector3(1, 0, 0)).normalized();
	const Vector3 b = p_model.xform(Vector3(0, p_sign, 0)).normalized();
	const real_t x = p_texel.r * 2 - 1, y = p_texel.g * 2 - 1;
	const real_t z = Math::sqrt(MAX(real_t(0), 1 - x * x - y * y));
	const Vector3 normal = n.lerp(t * x + b * y + n * z, p_strength).normalized();
	const real_t diffuse = MAX(real_t(0), normal.dot(Vector3(0.5, 0.4, 1).normalized()));
	const float value = 0.05f * p_occlusion + float(diffuse) * 0.6f * Math::lerp(1.0f, p_occlusion, p_light_affect);
	const uint32_t expected = Color(p_albedo.r * value, p_albedo.g * value, p_albedo.b * value).to_argb32();
	if (!TestFixedFunctionLambert::_color(p_actual, expected)) {
		printf("Normal-map pixel %08x expected %08x\n", p_actual, expected);
		return false;
	}
	return true;
}

bool TestFixedFunctionRetainedLighting::_normal_maps(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	if (!p_context.get_device().supports_normal_mapping()) {
		return true;
	}
	FF_NORMAL_CHECK(p_context.resize(128, 96) == OK && p_context.prepare_resources() == OK);
	const Variant previous = GLOBAL_GET("rendering/quality/fixed_function/normal_mapping");
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/normal_mapping", true);
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", true);
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	FixedFunctionGeometryCache &cache = p_context.get_geometry_cache();
	const RID shader = materials.shader_create(), material = materials.material_create();
	const String source = "shader_type spatial; render_mode cull_disabled; uniform sampler2D bump : hint_normal, filter_nearest, repeat_disable; uniform float depth=1.0; uniform vec2 scale=vec2(1); uniform vec2 offset=vec2(0); void fragment(){ ALBEDO=vec3(0.5); NORMAL_MAP=texture(bump,UV*scale+offset).rgb; NORMAL_MAP_DEPTH=depth; }";
	FF_NORMAL_CHECK(materials.shader_set_code(shader, source) == OK && materials.material_set_shader(material, shader) == OK);
	const RID texture = textures.texture_create();
	Ref<Image> image;
	image = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	// Exact byte values; blue and alpha deliberately cannot encode the expected normal.
	const Color first(204.0f / 255, 128.0f / 255, 0, 0), second(128.0f / 255, 204.0f / 255, 0, 0);
	image->set_pixel(0, 0, first);
	image->set_pixel(1, 0, second);
	image->set_pixel(0, 1, second);
	image->set_pixel(1, 1, second);
	FF_NORMAL_CHECK(textures.texture_allocate_2d(texture, Size2i(2, 2), Image::FORMAT_RGBA8, 0, 0) == OK && textures.texture_set_data(texture, image) == OK && materials.material_set_param(material, "bump", texture) == OK);
	FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	environment.ambient = Color(0.05, 0.05, 0.05);
	const RID light = lights.light_create(RSE::LIGHT_DIRECTIONAL), light_instance = lights.light_instance_create(light);
	FixedFunctionLightStorage::FFLightData light_data;
	FF_NORMAL_CHECK(lights.light_get_data(light, light_data) == OK);
	light_data.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.6;
	FF_NORMAL_CHECK(lights.light_set_data(light, light_data) == OK);
	Transform3D light_transform;
	light_transform.basis.set_column(2, Vector3(0.5, 0.4, 1).normalized());
	FF_NORMAL_CHECK(lights.light_instance_set_transform(light_instance, light_transform) == OK);
	Vector<RID> active;
	active.push_back(light_instance);
	std::vector<uint32_t> pixels;
	for (int format = 0; format < 2; format++) {
		const uint64_t compression[] = { 0, RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES };
		for (int sign_index = 0; sign_index < 2; sign_index++) {
			const float sign = sign_index ? -1 : 1;
			const RID mesh = _normal_mesh(meshes, compression[format], sign);
			FF_NORMAL_CHECK(mesh.is_valid());
			TestFixedFunctionLambert::FFInstance instance(p_context, mesh, material);
			FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, sign));
			FixedFunctionMeshStorage::FFSurfaceData snapshot, after;
			FF_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, snapshot) == OK);
			if (format != 0) {
				const uint8_t *source_data = snapshot.source.vertex_data.ptr();
				Vector4 tangent;
				const Vector3 normal = FixedFunctionMeshStorage::surface_get_normal(snapshot, source_data, 0, &tangent);
				FF_NORMAL_CHECK(normal == FixedFunctionMeshStorage::surface_get_normal(snapshot, source_data, 0));
				FF_NORMAL_CHECK(tangent == FixedFunctionMeshStorage::surface_get_tangent(snapshot, source_data, 0) && tangent.w == sign);
				if (p_context.get_device().supports_transient_normal_mapping()) {
					ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", false);
					FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, sign));
					ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", true);
				}
			}
			RID geometry;
			FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
			Vector3 anchor, extent;
			FF_NORMAL_CHECK(cache.get(snapshot.identity, snapshot.version, 0, geometry, info, anchor, extent, true, 0) == OK);
			FF_NORMAL_CHECK(info.vertex_count == 3 && info.draw_calls == 1);
			// Changing only mapping/strength reuses the exact same native upload.
			FF_NORMAL_CHECK(materials.material_set_param(material, "offset", Vector2(0.5, 0.5)) == OK);
			FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], second, 1, sign));
			FF_NORMAL_CHECK(materials.material_set_param(material, "depth", 0.5) == OK);
			FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], second, 0.5, sign));
			instance.transform.basis = Basis(Vector3(0, 1, 0), real_t(0.15));
			instance.transform.basis.scale(Vector3(-0.8, 1.2, 0.7));
			FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], second, 0.5, sign, instance.transform.basis));
			RID reused;
			FF_NORMAL_CHECK(cache.get(snapshot.identity, snapshot.version, 0, reused, info, anchor, extent, true, 0) == OK && reused == geometry);
			FF_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, after) == OK && after.identity == snapshot.identity && after.version == snapshot.version && after.source.vertex_count == 3);
			FF_NORMAL_CHECK(snapshot.source.vertex_data == after.source.vertex_data && snapshot.source.attribute_data == after.source.attribute_data);
			FF_NORMAL_CHECK(materials.material_set_param(material, "depth", 1.0) == OK && materials.material_set_param(material, "offset", Vector2()) == OK);
			FF_NORMAL_CHECK(meshes.free(mesh) == OK && p_context.prepare_resources() == OK);
		}
	}
	const RID mesh = _normal_mesh(meshes, 0, 1);
	TestFixedFunctionLambert::FFInstance instance(p_context, mesh, material);
	FF_NORMAL_CHECK(materials.shader_set_code(shader, source.replace("UV*scale", "UV2*scale")) == OK);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], second, 1, 1));
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/normal_mapping", false);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1));
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/normal_mapping", true);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], second, 1, 1));
	TestFixedFunctionRetainedScene::FFDevice &device = static_cast<TestFixedFunctionRetainedScene::FFDevice &>(p_context.get_device());
	device.set_normal_mapping_disabled(true);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1));
	device.set_normal_mapping_disabled(false);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], second, 1, 1));
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", false);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1));
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", true);
	FF_NORMAL_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(128, 96) == OK);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], second, 1, 1));
	// Two independent bindings: albedo uses UV2 and bilinear, normal uses UV and nearest.
	const RID paint = textures.texture_create();
	Ref<Image> paint_image;
	paint_image = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	paint_image->fill(Color(1, 0, 0));
	paint_image->set_pixel(1, 1, Color(64.0f / 255, 128.0f / 255, 1));
	FF_NORMAL_CHECK(textures.texture_allocate_2d(paint, Size2i(2, 2), Image::FORMAT_RGBA8, 0, FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR) == OK && textures.texture_set_data(paint, paint_image) == OK);
	const String painted = source.replace("uniform sampler2D bump", "uniform sampler2D paint : filter_linear, repeat_disable; uniform sampler2D bump").replace("ALBEDO=vec3(0.5);", "ALBEDO=texture(paint,UV2).rgb*vec3(0.5);");
	FF_NORMAL_CHECK(materials.shader_set_code(shader, painted) == OK && materials.material_set_param(material, "paint", paint) == OK);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), Color(32.0f / 255, 64.0f / 255, 0.5)));
	// Retained lighting must apply the albedo mapping independently of the normal UV.
	const String painted_offset = painted.replace("uniform sampler2D paint", "uniform vec2 paint_offset; uniform sampler2D paint").replace("texture(paint,UV2)", "texture(paint,UV2+paint_offset)");
	FF_NORMAL_CHECK(materials.shader_set_code(shader, painted_offset) == OK && materials.material_set_param(material, "paint_offset", Vector2(-0.5, -0.5)) == OK);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), Color(0.5, 0, 0)));

	if (device.supports_normal_mapped_occlusion()) {
		const RID orm = textures.texture_create();
		Ref<Image> ao = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
		ao->fill(Color(0.5, 0.75, 1, 1));
		ao->set_pixel(1, 1, Color(0.5, 0.25, 1, 1));
		FF_NORMAL_CHECK(textures.texture_set_data(orm, ao) == OK);
		const String combined = painted.replace("texture(paint,UV2", "texture(paint,UV").replace("void fragment()", "uniform sampler2D orm : filter_nearest, repeat_disable; uniform vec2 ao_offset=vec2(0); uniform float light_affect=0.0; void fragment()").replace("NORMAL_MAP_DEPTH=depth;", "NORMAL_MAP_DEPTH=depth; AO=dot(texture(orm,UV2+ao_offset),vec4(0,1,0,0)); AO_LIGHT_AFFECT=light_affect;");
		FF_NORMAL_CHECK(materials.shader_set_code(shader, combined) == OK);
		FF_NORMAL_CHECK(materials.material_set_param(material, "paint", paint) == OK);
		FF_NORMAL_CHECK(materials.material_set_param(material, "orm", orm) == OK);
		const Color red(0.5, 0, 0);
		const int before_mapped = device.get_retained_draws();
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_NORMAL_CHECK(device.get_retained_draws() == before_mapped + 1 && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), red, 0.25));
		const int after_creation = device.get_creations();
		FF_NORMAL_CHECK(materials.material_set_param(material, "light_affect", 1.0) == OK);
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), red, 0.25, 1));
		FF_NORMAL_CHECK(materials.material_set_param(material, "ao_offset", Vector2(-0.5, -0.5)) == OK);
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), red, 0.75, 1));
		FF_NORMAL_CHECK(materials.shader_set_code(shader, combined.replace("dot(texture(orm,UV2+ao_offset),vec4(0,1,0,0))", "texture(orm,UV2+ao_offset).r")) == OK);
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), red, 0.5, 1));
		ProjectSettings *settings = ProjectSettings::get_singleton();
		const bool previous_occlusion = GLOBAL_GET("rendering/quality/fixed_function/texture_occlusion");
		settings->set("rendering/quality/fixed_function/texture_occlusion", false);
		const bool unoccluded = TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), red);
		settings->set("rendering/quality/fixed_function/texture_occlusion", previous_occlusion);
		FF_NORMAL_CHECK(unoccluded && device.get_creations() == after_creation);
		device.set_normal_mapping_disabled(true);
		const int before_ao = device.get_retained_draws();
		const bool retained_ao = TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1, Basis(), red, 0.5, 1);
		FF_NORMAL_CHECK(retained_ao && device.get_retained_draws() == before_ao + 1);
		const String ao_only = combined.replace("NORMAL_MAP=texture(bump,UV*scale+offset).rgb;", "").replace("NORMAL_MAP_DEPTH=depth;", "");
		FF_NORMAL_CHECK(materials.shader_set_code(shader, ao_only) == OK && materials.material_set_param(material, "ao_offset", Vector2()) == OK);
		const int ao_creations = device.get_creations();
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1, Basis(), red, 0.25, 1));
		FF_NORMAL_CHECK(materials.material_set_param(material, "ao_offset", Vector2(-0.5, -0.5)) == OK);
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1, Basis(), red, 0.75, 1));
		FF_NORMAL_CHECK(device.get_retained_draws() == before_ao + 3 && device.get_creations() == ao_creations);
		settings->set("rendering/quality/fixed_function/retained_geometry", false);
		const bool vertex_ao = TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1, Basis(), red, 0.75, 1);
		settings->set("rendering/quality/fixed_function/retained_geometry", true);
		FF_NORMAL_CHECK(vertex_ao && device.get_retained_draws() == before_ao + 3);
		device.set_normal_mapping_disabled(false);
		FF_NORMAL_CHECK(materials.shader_set_code(shader, combined.replace("dot(texture(orm,UV2+ao_offset),vec4(0,1,0,0))", "texture(orm,UV2+ao_offset).r")) == OK);
		const RID missing_normal = textures.texture_create();
		FF_NORMAL_CHECK(materials.material_set_param(material, "bump", missing_normal) == OK);
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1, Basis(), red, 0.5, 1));
		FF_NORMAL_CHECK(materials.material_set_param(material, "bump", texture) == OK && textures.free(missing_normal) == OK && textures.free(orm) == OK);
		printf("PASS: D3D9 normal maps with per-pixel AO, independent UVs, channel selectors, light affect, quality controls and fallback.\n");
		printf("PASS: D3D9 standalone per-pixel AO with independent UV2, retained upload reuse and per-vertex fallback.\n");
	}
	FF_NORMAL_CHECK(materials.shader_set_code(shader, source.replace("UV*scale", "UV2*scale")) == OK && textures.free(paint) == OK);
	// A texture write uses the latest CPU data after upload, without changing mesh data.
	image->fill(first);
	FF_NORMAL_CHECK(textures.texture_set_data(texture, image) == OK);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1));
	FF_NORMAL_CHECK(materials.material_set_param(material, "depth", std::numeric_limits<double>::quiet_NaN()) == OK);
	FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
	FF_NORMAL_CHECK(materials.material_get_spatial_parameters(material, parameters) == OK && !parameters.normal.texture.is_valid() && !parameters.is_fallback);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1));
	FF_NORMAL_CHECK(materials.material_set_param(material, "depth", 1.0) == OK);
	const RID missing = _normal_mesh(meshes, 0, 1, false);
	instance.data->base = missing;
	if (SurfaceTool::generate_tangents_func) {
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1));
		FixedFunctionMeshStorage::FFSurfaceData source_without_tangents;
		FF_NORMAL_CHECK(meshes.mesh_surface_get_data(missing, 0, source_without_tangents) == OK && !(source_without_tangents.source.format & RSE::ARRAY_FORMAT_TANGENT));
		RID generated;
		FixedFunctionGraphicsDevice::FFGeometryDrawInfo generated_info;
		Vector3 generated_anchor, generated_extent;
		FF_NORMAL_CHECK(cache.get(source_without_tangents.identity, source_without_tangents.version, 0, generated, generated_info, generated_anchor, generated_extent, true, 1) == OK);
	} else {
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1));
	}
	instance.data->base = mesh;
	FixedFunctionMeshStorage::FFSurfaceData data;
	FF_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, data) == OK);
	Vector<uint8_t> patch;
	FF_NORMAL_CHECK(patch.resize(sizeof(float)) == OK);
	{
		const Vector2 encoded = Vector3(0, 0, 1).octahedron_tangent_encode(1);
		const uint16_t parallel[] = { uint16_t(Math::round(encoded.x * 65535)), uint16_t(Math::round(encoded.y * 65535)) };
		memcpy(patch.ptrw(), parallel, sizeof(parallel));
	}
	FF_NORMAL_CHECK(meshes.mesh_surface_update_region(mesh, 0, FixedFunctionMeshStorage::BUFFER_VERTEX, data.offsets[RSE::ARRAY_TANGENT], patch) == OK);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1));
	FF_NORMAL_CHECK(meshes.free(missing) == OK && meshes.free(mesh) == OK && materials.free(material) == OK && materials.free(shader) == OK && textures.free(texture) == OK && lights.free(light_instance) == OK && lights.free(light) == OK && p_context.prepare_resources() == OK && cache.get_entry_count() == 0 && p_context.get_device().get_geometry_memory_usage() == 0);
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/normal_mapping", previous);
	printf("PASS: normal mapping, native packed tangent formats/signs, independent UV2/transform/strength, reflected nonuniform transforms, quality fallback, invalid data, immutable source and reset.\n");
	return true;
}

#undef FF_NORMAL_CHECK
