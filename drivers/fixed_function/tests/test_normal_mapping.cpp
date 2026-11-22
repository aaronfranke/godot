#include "test_retained_lighting.h"

#include "core/project_settings.h"

#include <stdio.h>
#include <string.h>
#include <limits>

#define FF_NORMAL_CHECK(m_condition)                                           \
	do {                                                                       \
		if (!(m_condition)) {                                                  \
			printf("Normal mapping FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                      \
		}                                                                      \
	} while (false)

RID TestFixedFunctionRetainedLighting::_normal_mesh(FixedFunctionMeshStorage &p_storage, uint32_t p_compression, float p_sign, bool p_tangent) {
	Array arrays;
	arrays.resize(VS::ARRAY_MAX);
	PoolVector<Vector3> positions, normals;
	PoolRealArray tangents;
	PoolVector<Vector2> uv, uv2;
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
		uv2.push_back(Vector2(0.75, 0.75));
	}
	arrays[VS::ARRAY_VERTEX] = positions;
	arrays[VS::ARRAY_NORMAL] = normals;
	arrays[VS::ARRAY_TEX_UV] = uv;
	arrays[VS::ARRAY_TEX_UV2] = uv2;
	if (p_tangent) {
		arrays[VS::ARRAY_TANGENT] = tangents;
	}
	return _pack_normal_mesh(p_storage, arrays, p_compression);
}

RID TestFixedFunctionRetainedLighting::_pack_normal_mesh(FixedFunctionMeshStorage &p_storage, const Array &p_arrays, uint32_t p_compression) {
	VisualServer *server = VisualServer::get_singleton();
	const RID source = server->mesh_create();
	server->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_TRIANGLES, p_arrays, Array(), p_compression);
	const RID mesh = p_storage.mesh_create();
	const Error error = p_storage.mesh_add_surface(mesh, server->mesh_surface_get_format(source, 0), VS::PRIMITIVE_TRIANGLES, server->mesh_surface_get_array(source, 0), server->mesh_surface_get_array_len(source, 0), server->mesh_surface_get_index_array(source, 0), server->mesh_surface_get_array_index_len(source, 0), AABB(Vector3(-1, -1, 0), Vector3(2, 2, 0)), Vector<PoolVector<uint8_t>>(), Vector<AABB>());
	server->free(source);
	return error == OK ? mesh : RID();
}

RID TestFixedFunctionRetainedLighting::_tangent_source_mesh(FixedFunctionMeshStorage &p_storage, bool p_mirrored, bool p_indexed) {
	// The UV2 charts meet at vertices 1 and 2. UV0 is deliberately degenerate.
	const Vector3 points[] = { Vector3(-1, -1, 0), Vector3(0, -1, 0), Vector3(0, 1, 0), Vector3(1, -1, 0) };
	const Vector2 chart[] = { Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(p_mirrored ? 0 : 2, 0) };
	const int order[] = { 0, 2, 1, 1, 2, 3 };
	PoolVector<Vector3> positions, normals;
	PoolVector<Vector2> uv, uv2;
	PoolVector<int> indices;
	for (int i = 0; i < (p_indexed ? 4 : 6); i++) {
		const int vertex = p_indexed ? i : order[i];
		positions.push_back(points[vertex]);
		normals.push_back(Vector3(0, 0, 1));
		uv.push_back(Vector2());
		uv2.push_back(chart[vertex]);
	}
	Array arrays;
	arrays.resize(VS::ARRAY_MAX);
	arrays[VS::ARRAY_VERTEX] = positions;
	arrays[VS::ARRAY_NORMAL] = normals;
	arrays[VS::ARRAY_TEX_UV] = uv;
	arrays[VS::ARRAY_TEX_UV2] = uv2;
	if (p_indexed) {
		for (int i = 0; i < 6; i++) {
			indices.push_back(order[i]);
		}
		arrays[VS::ARRAY_INDEX] = indices;
	}
	return _pack_normal_mesh(p_storage, arrays, VS::ARRAY_COMPRESS_TEX_UV2);
}

bool TestFixedFunctionRetainedLighting::_normal_pixel(uint32_t p_actual, const Color &p_texel, float p_strength, float p_sign, const Basis &p_model, const Color &p_albedo, float p_occlusion, float p_light_affect) {
	const Vector3 n = p_model.inverse().transposed().xform(Vector3(0, 0, 1)).normalized();
	const Vector3 t = p_model.xform(Vector3(1, 0, 0)).normalized();
	const Vector3 b = p_model.xform(Vector3(0, p_sign, 0)).normalized();
	const real_t x = p_texel.r * 2 - 1, y = p_texel.g * 2 - 1;
	const real_t z = Math::sqrt(MAX(real_t(0), 1 - x * x - y * y));
	const Vector3 normal = n.linear_interpolate(t * x + b * y + n * z, p_strength).normalized();
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
	const String source = "shader_type spatial; render_mode cull_disabled; uniform sampler2D bump : hint_normal; uniform float depth=1.0; uniform vec2 scale=vec2(1); uniform vec2 offset=vec2(0); void fragment(){ ALBEDO=vec3(0.5); NORMALMAP=texture(bump,UV*scale+offset).rgb; NORMALMAP_DEPTH=depth; }";
	FF_NORMAL_CHECK(materials.shader_set_code(shader, source) == OK && materials.material_set_shader(material, shader) == OK);
	const RID texture = textures.texture_create();
	Ref<Image> image;
	image.instance();
	image->create(2, 2, false, Image::FORMAT_RGBA8);
	// Exact byte values; blue and alpha deliberately cannot encode the expected normal.
	const Color first(204.0f / 255, 128.0f / 255, 0, 0), second(128.0f / 255, 204.0f / 255, 0, 0);
	image->lock();
	image->set_pixel(0, 0, first);
	image->set_pixel(1, 0, second);
	image->set_pixel(0, 1, second);
	image->set_pixel(1, 1, second);
	image->unlock();
	FF_NORMAL_CHECK(textures.texture_allocate_2d(texture, Size2i(2, 2), Image::FORMAT_RGBA8, 0, 0) == OK && textures.texture_set_data(texture, image) == OK && materials.material_set_param(material, "bump", texture) == OK);
	const RID environment = p_context.get_scene_storage().environment_create();
	FF_NORMAL_CHECK(p_context.get_scene_storage().environment_set_ambient(environment, Color(0.05, 0.05, 0.05), 1, 0) == OK);
	const RID light = lights.light_create(VS::LIGHT_DIRECTIONAL), light_instance = lights.light_instance_create(light);
	FixedFunctionLightStorage::FFLightData light_data;
	FF_NORMAL_CHECK(lights.light_get_data(light, light_data) == OK);
	light_data.parameters[VS::LIGHT_PARAM_ENERGY] = 0.6;
	FF_NORMAL_CHECK(lights.light_set_data(light, light_data) == OK);
	Transform light_transform;
	light_transform.basis.set_axis(2, Vector3(0.5, 0.4, 1).normalized());
	FF_NORMAL_CHECK(lights.light_instance_set_transform(light_instance, light_transform) == OK);
	Vector<RID> active;
	active.push_back(light_instance);
	std::vector<uint32_t> pixels;
	for (int format = 0; format < 4; format++) {
		const uint32_t compression[] = { 0, VS::ARRAY_COMPRESS_NORMAL | VS::ARRAY_COMPRESS_TANGENT, VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION | VS::ARRAY_COMPRESS_NORMAL | VS::ARRAY_COMPRESS_TANGENT, VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION };
		for (int sign_index = 0; sign_index < 2; sign_index++) {
			const float sign = sign_index ? -1 : 1;
			const RID mesh = _normal_mesh(meshes, compression[format], sign);
			FF_NORMAL_CHECK(mesh.is_valid());
			TestFixedFunctionLambert::FFInstance instance(mesh, material);
			FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, sign));
			FixedFunctionMeshStorage::FFSurfaceData snapshot, after;
			FF_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, snapshot) == OK);
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
			FF_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, after) == OK && after.identity == snapshot.identity && after.version == snapshot.version && after.vertex_count == 3);
			{
				const PoolVector<uint8_t>::Read before = snapshot.array.read(), current = after.array.read();
				FF_NORMAL_CHECK(snapshot.array.size() == after.array.size() && memcmp(before.ptr(), current.ptr(), snapshot.array.size()) == 0);
			}
			FF_NORMAL_CHECK(materials.material_set_param(material, "depth", 1.0) == OK && materials.material_set_param(material, "offset", Vector2()) == OK);
			FF_NORMAL_CHECK(meshes.free(mesh) == OK && p_context.prepare_resources() == OK);
		}
	}
	const RID mesh = _normal_mesh(meshes, 0, 1);
	TestFixedFunctionLambert::FFInstance instance(mesh, material);
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
	paint_image.instance();
	paint_image->create(2, 2, false, Image::FORMAT_RGBA8);
	paint_image->fill(Color(1, 0, 0));
	paint_image->lock();
	paint_image->set_pixel(1, 1, Color(64.0f / 255, 128.0f / 255, 1));
	paint_image->unlock();
	FF_NORMAL_CHECK(textures.texture_allocate_2d(paint, Size2i(2, 2), Image::FORMAT_RGBA8, VS::TEXTURE_FLAG_FILTER, FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR) == OK && textures.texture_set_data(paint, paint_image) == OK);
	const String painted = source.replace("uniform sampler2D bump", "uniform sampler2D paint; uniform vec2 paint_scale=vec2(1); uniform vec2 paint_offset=vec2(0); uniform sampler2D bump").replace("ALBEDO=vec3(0.5);", "ALBEDO=texture(paint,UV2*paint_scale+paint_offset).rgb*vec3(0.5);");
	FF_NORMAL_CHECK(materials.shader_set_code(shader, painted) == OK && materials.material_set_param(material, "paint", paint) == OK);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), Color(32.0f / 255, 64.0f / 255, 0.5)));
	const int creations = device.get_creations();
	const int transient_draws = device.get_transient_draws();
	FF_NORMAL_CHECK(materials.material_set_param(material, "paint_scale", Vector2(0.5, 0.5)) == OK && materials.material_set_param(material, "paint_offset", Vector2(-0.125, 0.45)) == OK);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), Color(0.5, 0, 0)));
	FF_NORMAL_CHECK(device.get_creations() == creations && device.get_transient_draws() == transient_draws);

	if (device.get_capabilities().render_target_opaque) {
		// The transformed V is 0.825. Flipping after the transform samples the red
		// top half, while flipping first would still sample the green bottom half.
		const RID target = textures.render_target_create();
		FF_NORMAL_CHECK(textures.render_target_set_size(target, 16, 16) == OK);
		const FixedFunctionVertex top[] = {
			{ -1, 1, 0, 0xffff0000, 0, 0 },
			{ 1, 1, 0, 0xffff0000, 0, 0 },
			{ 1, 0, 0, 0xffff0000, 0, 0 },
			{ -1, 1, 0, 0xffff0000, 0, 0 },
			{ 1, 0, 0, 0xffff0000, 0, 0 },
			{ -1, 0, 0, 0xffff0000, 0, 0 },
		};
		FF_NORMAL_CHECK(p_targets.begin_frame(0) == OK && p_targets.select_render_target(target) == OK && p_targets.clear_render_target(Color(0, 1, 0)) == OK && p_targets.begin_draw() == OK);
		FF_NORMAL_CHECK(device.draw_triangles(top, 6, CameraMatrix(), RID(), FixedFunctionGraphicsDevice::FFDrawParameters()) == OK && p_targets.end_frame(false) == OK);
		const int target_transient_draws = device.get_transient_draws();
		FF_NORMAL_CHECK(materials.material_set_param(material, "paint", textures.render_target_get_texture(target)) == OK);
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), Color(0.5, 0, 0)));
		FF_NORMAL_CHECK(textures.render_target_set_flag(target, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP, true) == OK);
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 1, 1, Basis(), Color(0, 0.5, 0)));
		FF_NORMAL_CHECK(device.get_creations() == creations && device.get_transient_draws() == target_transient_draws);

		// Ordinary retained SM3 shares the albedo transform with the mapped variant.
		ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/normal_mapping", false);
		const int retained_draws = device.get_retained_draws();
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1, Basis(), Color(0, 0.5, 0)));
		FF_NORMAL_CHECK(device.get_retained_draws() == retained_draws + 1 && device.get_transient_draws() == target_transient_draws);
		const uint32_t retained_pixel = pixels[48 * 128 + 64];
		ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", false);
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && pixels[48 * 128 + 64] == retained_pixel);
		ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/retained_geometry", true);
		ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/normal_mapping", true);
		FF_NORMAL_CHECK(textures.free(target) == OK);
	}
	if (device.supports_normal_mapped_occlusion()) {
		const RID orm = textures.texture_create();
		Ref<Image> ao = memnew(Image(2, 2, false, Image::FORMAT_RGBA8));
		ao->fill(Color(0.5, 0.75, 1, 1));
		ao->lock();
		ao->set_pixel(1, 1, Color(0.5, 0.25, 1, 1));
		ao->unlock();
		FF_NORMAL_CHECK(textures.texture_set_data(orm, ao) == OK);
		const String combined = painted.replace("texture(paint,UV2", "texture(paint,UV").replace("void fragment()", "uniform sampler2D orm; uniform vec2 ao_offset=vec2(0); uniform float light_affect=0.0; void fragment()").replace("NORMALMAP_DEPTH=depth;", "NORMALMAP_DEPTH=depth; AO=dot(texture(orm,UV2+ao_offset),vec4(0,1,0,0)); AO_LIGHT_AFFECT=light_affect;");
		FF_NORMAL_CHECK(materials.shader_set_code(shader, combined) == OK);
		FF_NORMAL_CHECK(materials.material_set_param(material, "paint", paint) == OK && materials.material_set_param(material, "paint_scale", Vector2(1, 1)) == OK && materials.material_set_param(material, "paint_offset", Vector2()) == OK);
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
		const String ao_only = combined.replace("NORMALMAP=texture(bump,UV*scale+offset).rgb;", "").replace("NORMALMAP_DEPTH=depth;", "");
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
	instance.base = missing;
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1));
	instance.base = mesh;
	FixedFunctionMeshStorage::FFSurfaceData data;
	FF_NORMAL_CHECK(meshes.mesh_surface_get_data(mesh, 0, data) == OK);
	PoolVector<uint8_t> patch;
	FF_NORMAL_CHECK(patch.resize(sizeof(float)) == OK);
	{
		const float nan = std::numeric_limits<float>::quiet_NaN();
		PoolVector<uint8_t>::Write bytes = patch.write();
		memcpy(bytes.ptr(), &nan, sizeof(nan));
	}
	FF_NORMAL_CHECK(meshes.mesh_surface_update_region(mesh, 0, data.offsets[VS::ARRAY_TANGENT], patch) == OK);
	FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _normal_pixel(pixels[48 * 128 + 64], first, 0, 1));
	const Color tangent_texel(204.0f / 255, 180.0f / 255, 0, 0);
	image->fill(tangent_texel);
	FF_NORMAL_CHECK(textures.texture_set_data(texture, image) == OK);
	for (int variant = 0; variant < 3; variant++) {
		const bool mirrored = variant != 0;
		const bool indexed = variant != 2;
		instance.base = _tangent_source_mesh(meshes, mirrored, indexed);
		FixedFunctionMeshStorage::FFSurfaceData before, after;
		FF_NORMAL_CHECK(meshes.mesh_surface_get_data(instance.base, 0, before) == OK && !(before.format & VS::ARRAY_FORMAT_TANGENT));
		const uint64_t previous_memory = device.get_geometry_memory_usage();
		FF_NORMAL_CHECK(TestFixedFunctionLambert::_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_NORMAL_CHECK(_normal_pixel(pixels[48 * 128 + 52], tangent_texel, 1, -1));
		const Basis right_frame = mirrored ? Basis(Vector3(-1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)) : Basis();
		FF_NORMAL_CHECK(_normal_pixel(pixels[48 * 128 + 76], tangent_texel, 1, -1, right_frame));
		const uint64_t payload = device.get_geometry_memory_usage() - previous_memory;
		FF_NORMAL_CHECK(payload == 0 || payload == (mirrored ? 6 : 4) * sizeof(FixedFunctionNormalMapVertex) + 6 * sizeof(uint16_t));
		RID retained;
		FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
		Vector3 anchor, extent;
		FF_NORMAL_CHECK(cache.get(before.identity, before.version, 0, retained, info, anchor, extent, true, 1) == OK && info.vertex_count == 6);
		FF_NORMAL_CHECK(meshes.mesh_surface_get_data(instance.base, 0, after) == OK && after.version == before.version && after.format == before.format && after.vertex_count == before.vertex_count && after.index_count == before.index_count);
		FF_NORMAL_CHECK(meshes.free(instance.base) == OK && p_context.prepare_resources() == OK);
	}
	FF_NORMAL_CHECK(meshes.free(missing) == OK && meshes.free(mesh) == OK && materials.free(material) == OK && materials.free(shader) == OK && textures.free(texture) == OK && lights.free(light_instance) == OK && lights.free(light) == OK && p_context.get_scene_storage().free(environment) == OK && p_context.prepare_resources() == OK && cache.get_entry_count() == 0 && p_context.get_device().get_geometry_memory_usage() == 0);
	ProjectSettings::get_singleton()->set("rendering/quality/fixed_function/normal_mapping", previous);
	printf("PASS: normal mapping, authored tangents and generated MikkTSpace frames, mirrored UV splits, independent UV2/transform/strength, quality fallback, immutable source and reset.\n");
	return true;
}

#undef FF_NORMAL_CHECK
