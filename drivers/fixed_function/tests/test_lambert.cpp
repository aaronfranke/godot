#include "test_lambert.h"

#include "core/config/project_settings.h"
#include "drivers/fixed_function/tests/test_preset_scope.h"
#include "scene/resources/material.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>

#include <limits>

#define FF_LAMBERT_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Lambert FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

TestFixedFunctionLambert::FFInstance::FFInstance(FixedFunctionRenderContext &p_context, RID p_mesh, RID p_material) : FixedFunctionGeometryInstance(p_context, p_mesh) {
	data->material_override = p_material;
	transform.origin.z = -2;
}

bool TestFixedFunctionLambert::_color(uint32_t p_actual, uint32_t p_expected) {
	for (int shift = 0; shift < 32; shift += 8) {
		if (Math::abs(int((p_actual >> shift) & 255) - int((p_expected >> shift) & 255)) > 3) {
			return false;
		}
	}
	return true;
}

RID TestFixedFunctionLambert::_mesh(FixedFunctionMeshStorage &p_storage, uint64_t p_compression, bool p_normal, const Vector3 &p_direction, bool p_tangents, bool p_lines, bool p_gradient) {
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	PackedVector3Array positions, normals;
	PackedFloat32Array tangents;
	positions.push_back(Vector3(-1, -1, 0));
	positions.push_back(Vector3(0, 1, 0));
	positions.push_back(Vector3(1, -1, 0));
	if (p_lines) {
		positions.resize(2);
		positions.set(0, Vector3(-1, 0, 0));
		positions.set(1, Vector3(1, 0, 0));
	}
	for (int i = 0; i < positions.size(); i++) {
		Vector3 normal = p_direction;
		if (p_gradient) {
			if (i == 0) {
				normal = Vector3(0, 0, 1);
			} else if (i == 1) {
				normal = Vector3(1, 0, 0);
			} else {
				normal = Vector3(0, 0, -1);
			}
		}
		normals.push_back(normal);
		if (p_tangents) {
			const Vector3 tangent = (Math::abs(normal.x) > 0.9f ? Vector3(0, 1, 0) : Vector3(1, 0, 0)).slide(normal).normalized();
			tangents.push_back(tangent.x);
			tangents.push_back(tangent.y);
			tangents.push_back(tangent.z);
			tangents.push_back(1);
		}
	}
	arrays[RSE::ARRAY_VERTEX] = positions;
	if (p_normal) {
		arrays[RSE::ARRAY_NORMAL] = normals;
	}
	if (p_tangents) {
		arrays[RSE::ARRAY_TANGENT] = tangents;
	}
	RenderingServerTypes::SurfaceData data;
	if (RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&data, p_lines ? RSE::PRIMITIVE_LINES : RSE::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), p_compression) != OK) {
		return RID();
	}
	const RID mesh = p_storage.mesh_create();
	return p_storage.mesh_add_surface(mesh, data) == OK ? mesh : RID();
}

bool TestFixedFunctionLambert::_draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, FFInstance &p_instance, const FixedFunctionSceneRenderer::FFLightingEnvironment &p_environment, const Vector<RID> &p_lights, std::vector<uint32_t> &r_pixels, const Transform3D &p_camera) {
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	instances.push_back(&p_instance);
	FF_LAMBERT_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
	FF_LAMBERT_CHECK(p_renderer.render(p_camera, projection, instances, nullptr, p_environment, p_lights.ptr(), p_lights.size()) == OK && p_targets.end_frame(false) == OK);
	r_pixels.resize(128 * 96);
	FF_LAMBERT_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	return true;
}

bool TestFixedFunctionLambert::_math() {
	const Vector3 direction = Vector3(1, 2, 3).normalized();
	for (int mirror = 0; mirror < 2; mirror++) {
		Basis basis(Vector3(1, 2, 1).normalized(), real_t(0.7));
		basis.scale(Vector3(mirror ? -2 : 2, 3, real_t(0.5)));
		const Vector3 reference = basis.inverse().transposed().xform(direction).normalized();
		const Vector3 actual = FixedFunctionSceneRenderer::_unit_normal(FixedFunctionSceneRenderer::_normal_matrix(basis).xform(direction));
		FF_LAMBERT_CHECK(reference.distance_to(actual) < real_t(0.00001));
	}
	FF_LAMBERT_CHECK(FixedFunctionSceneRenderer::_unit_normal(Vector3(std::numeric_limits<real_t>::max(), 0, 0)) == Vector3(1, 0, 0));
	FF_LAMBERT_CHECK(FixedFunctionSceneRenderer::_unit_normal(Vector3(0, -std::numeric_limits<real_t>::min(), 0)) == Vector3(0, -1, 0));
	FF_LAMBERT_CHECK(FixedFunctionSceneRenderer::_unit_normal(Vector3(0, 0, std::numeric_limits<real_t>::infinity())) == Vector3());
	FF_LAMBERT_CHECK(FixedFunctionSceneRenderer::_unit_normal(Vector3(std::numeric_limits<real_t>::quiet_NaN(), 0, 1)) == Vector3());
	return true;
}

bool TestFixedFunctionLambert::_materials(FixedFunctionRenderContext &p_context) {
	TestFixedFunctionPresetScope scope(p_context.get_device(), nullptr);
	FixedFunctionMaterialStorage &p_materials = p_context.get_material_storage();
	const RID shader = p_materials.shader_create(), material = p_materials.material_create();
	FF_LAMBERT_CHECK(p_materials.material_set_shader(material, shader) == OK);
	for (int vertex = 0; vertex < 2; vertex++) {
		Ref<StandardMaterial3D> preset;
		preset.instantiate();
		preset->set_shading_mode(vertex ? BaseMaterial3D::SHADING_MODE_PER_VERTEX : BaseMaterial3D::SHADING_MODE_PER_PIXEL);
		preset->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
		BaseMaterial3D::flush_changes();
		const String source = RenderingServer::get_singleton()->shader_get_code(preset->get_shader_rid());
		FF_LAMBERT_CHECK(p_materials.shader_set_code(shader, source) == OK);
		FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
		FF_LAMBERT_CHECK(p_materials.material_get_spatial_parameters(material, parameters) == OK && parameters.lighting_flags == (vertex ? FixedFunctionSpatialMaterial::LIGHTING_VERTEX : 0));
	}
	FF_LAMBERT_CHECK(p_materials.free(material) == OK && p_materials.free(shader) == OK);
	return true;
}

bool TestFixedFunctionLambert::_normal_formats(FixedFunctionMeshStorage &p_storage) {
	const uint64_t formats[] = { 0, RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES, 0, RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES };
	const Vector3 directions[] = { Vector3(0, 0, -1), Vector3(-1, 2, -3).normalized(), Vector3(1, -3, 2).normalized() };
	for (unsigned int format = 0; format < sizeof(formats) / sizeof(formats[0]); format++) {
		for (unsigned int direction = 0; direction < sizeof(directions) / sizeof(directions[0]); direction++) {
			const RID mesh = _mesh(p_storage, formats[format], true, directions[direction], format >= 2);
			FixedFunctionMeshStorage::FFSurfaceData surface;
			FF_LAMBERT_CHECK(mesh.is_valid() && p_storage.mesh_surface_get_data(mesh, 0, surface) == OK);
			const uint64_t mask = RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES;
			FF_LAMBERT_CHECK((surface.source.format & mask) == formats[format]);
			const uint8_t *bytes = surface.source.vertex_data.ptr();
			for (int vertex = 0; vertex < 3; vertex++) {
				const Vector3 decoded = FixedFunctionMeshStorage::surface_get_normal(surface, bytes, vertex);
				FF_LAMBERT_CHECK(decoded.distance_to(directions[direction]) < real_t(0.0005));
			}
			FF_LAMBERT_CHECK(p_storage.free(mesh) == OK);
		}
	}
	return true;
}

bool TestFixedFunctionLambert::_test_vertex_path(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FF_LAMBERT_CHECK(_math() && _materials(p_context) && _normal_formats(p_context.get_mesh_storage()));
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID shader = materials.shader_create(), material = materials.material_create();
	const String source = "shader_type spatial; render_mode cull_disabled; uniform vec4 tint : source_color = vec4(1,0,0,1); void fragment(){ ALBEDO=tint.rgb; }";
	FF_LAMBERT_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, source) == OK);
	FixedFunctionSceneRenderer::FFLightingEnvironment environment;
	environment.ambient = Color(0.25, 0.25, 0.25);
	const RID light = lights.light_create(RSE::LIGHT_DIRECTIONAL), light_instance = lights.light_instance_create(light);
	Vector<RID> active;
	active.push_back(light_instance);
	FixedFunctionLightStorage::FFLightData data;
	FF_LAMBERT_CHECK(lights.light_get_data(light, data) == OK);
	data.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.5;
	FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK);
	const uint64_t formats[] = { 0, RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES, 0, RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES };
	std::vector<uint32_t> pixels;
	for (unsigned int format = 0; format < sizeof(formats) / sizeof(formats[0]); format++) {
		const RID mesh = _mesh(meshes, formats[format], true, Vector3(0, 0, 1), format >= 2);
		FF_LAMBERT_CHECK(mesh.is_valid());
		FFInstance instance(p_context, mesh, material);
		FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xffbf0000));
		instance.layer_mask = 2;
		data.cull_mask = 1;
		FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
		instance.layer_mask = 1;
		const Basis camera_basis(Vector3(0, 1, 0), real_t(0.3));
		const Transform3D camera(camera_basis, Vector3(0, 0, -2) + camera_basis.get_column(2) * 2);
		FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels, camera) && _color(pixels[48 * 128 + 64], 0xffbf0000));
		FF_LAMBERT_CHECK(meshes.free(mesh) == OK);
	}
	const RID mesh = _mesh(meshes, 0, false);
	FFInstance instance(p_context, mesh, material);
	if (p_context.get_device().get_max_vertex_lights()) {
		FixedFunctionMeshStorage::FFSurfaceData surface;
		FF_LAMBERT_CHECK(meshes.mesh_surface_get_data(mesh, 0, surface) == OK);
		FixedFunctionGeometryCache &cache = p_context.get_geometry_cache();
		RID geometry;
		FixedFunctionGraphicsDevice::FFGeometryDrawInfo info;
		Vector3 anchor, extent;
		instance.transform.basis.scale(Vector3(1, 1, 2));
		FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_LAMBERT_CHECK(cache.get(surface.identity, surface.version, 0, geometry, info, anchor, extent, true) == ERR_DOES_NOT_EXIST);
		instance.transform.basis = Basis();
		const RID omni = lights.light_create(RSE::LIGHT_OMNI);
		const RID omni_instance = lights.light_instance_create(omni);
		FF_LAMBERT_CHECK(lights.light_instance_set_transform(omni_instance, Transform3D(Basis(), Vector3(0, 0, -1.5))) == OK);
		FF_LAMBERT_CHECK(active.push_back(omni_instance) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_LAMBERT_CHECK(cache.get(surface.identity, surface.version, 0, geometry, info, anchor, extent, true) == ERR_DOES_NOT_EXIST);
		active.resize(1);
		FF_LAMBERT_CHECK(lights.free(omni_instance) == OK && lights.free(omni) == OK);
		FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
		FF_LAMBERT_CHECK(cache.get(surface.identity, surface.version, 0, geometry, info, anchor, extent, true) == OK);
		printf("PASS: native vertex lighting retains geometry only after an eligible draw.\n");
	}
	FF_LAMBERT_CHECK(mesh.is_valid() && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xffbf0000));
	instance.transform.basis.scale(Vector3(-1, 1, 1));
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xffbf0000));
	instance.transform.basis = Basis();
	FF_LAMBERT_CHECK(lights.light_instance_set_transform(light_instance, Transform3D(Basis(Vector3(0, 1, 0), Math::PI), Vector3())) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
	FF_LAMBERT_CHECK(lights.light_instance_set_transform(light_instance, Transform3D()) == OK);

	instance.data->base = mesh;
	instance.transform.basis.scale(Vector3(1, 1, 0));
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xffbf0000));
	instance.transform.basis = Basis();
	const RID oblique = _mesh(meshes, 0, true, Vector3(1, 0, 1).normalized());
	instance.data->base = oblique;
	instance.transform.basis.scale(Vector3(2, 1, real_t(0.5)));
	environment.ambient = Color(0, 0, 0);
	data.parameters[RSE::LIGHT_PARAM_ENERGY] = 1;
	FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xfff70000));
	instance.data->base = mesh;
	instance.transform.basis = Basis();
	const RID gradient = _mesh(meshes, 0, true, Vector3(), false, false, true);
	instance.data->base = gradient;
	FF_LAMBERT_CHECK(gradient.is_valid() && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_LAMBERT_CHECK(int((pixels[72 * 128 + 40] >> 16) & 255) > int((pixels[72 * 128 + 88] >> 16) & 255) + 128 && ((pixels[20 * 128 + 64] >> 16) & 255) < 32);
	FF_LAMBERT_CHECK(meshes.free(gradient) == OK);
	instance.data->base = mesh;
	FF_LAMBERT_CHECK(materials.material_set_param(material, "tint", Color(2, 0, 0)) == OK);
	data.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.25;
	FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff800000));
	FF_LAMBERT_CHECK(Color(materials.material_get_param(material, "tint")).r == 2);
	FF_LAMBERT_CHECK(materials.material_set_param(material, "tint", Color(std::numeric_limits<float>::quiet_NaN(), 0, 0)) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xffff00ff));
	FF_LAMBERT_CHECK(Math::is_nan(Color(materials.material_get_param(material, "tint")).r));
	FF_LAMBERT_CHECK(materials.material_set_param(material, "tint", Color(2, 0, 0)) == OK);
	FF_LAMBERT_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(128, 96) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff800000));
	p_renderer.set_debug_unshaded(true);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xffff0000));
	p_renderer.set_debug_unshaded(false);
	const RID negative = lights.light_create(RSE::LIGHT_DIRECTIONAL), negative_instance = lights.light_instance_create(negative);
	FixedFunctionLightStorage::FFLightData subtraction;
	FF_LAMBERT_CHECK(lights.light_get_data(negative, subtraction) == OK);
	subtraction.flags |= FixedFunctionLightStorage::LIGHT_NEGATIVE;
	subtraction.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.125;
	FF_LAMBERT_CHECK(lights.light_set_data(negative, subtraction) == OK);
	active.push_back(negative_instance);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
	data.parameters[RSE::LIGHT_PARAM_ENERGY] = 2;
	subtraction.parameters[RSE::LIGHT_PARAM_ENERGY] = 1.75;
	FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK && lights.light_set_data(negative, subtraction) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff800000));
	active.resize(1);
	data.parameters[RSE::LIGHT_PARAM_ENERGY] = 0.25;
	FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK);
	Vector<uint8_t> image_bytes;
	image_bytes.resize(4);
	{
		uint8_t *write = image_bytes.ptrw();
		write[0] = 128;
		write[1] = 255;
		write[2] = 64;
		write[3] = 255;
	}
	Ref<Image> image = memnew(Image(1, 1, false, Image::FORMAT_RGBA8, image_bytes));
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID texture = textures.texture_create();
	FF_LAMBERT_CHECK(textures.texture_allocate_2d(texture, Size2i(1, 1), Image::FORMAT_RGBA8, 0, 0) == OK && textures.texture_set_data(texture, image) == OK);
	FF_LAMBERT_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; uniform sampler2D image; void fragment(){ ALBEDO=texture(image,UV).rgb; }") == OK && materials.material_set_param(material, "image", texture) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff204010));
	FF_LAMBERT_CHECK(textures.texture_allocate_2d(texture, Size2i(1, 1), Image::FORMAT_RGBA8, 0, 0) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff404040));
	FF_LAMBERT_CHECK(materials.material_set_param(material, "image", Variant()) == OK && textures.free(texture) == OK);
	FF_LAMBERT_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.5; }") == OK);
	if (p_context.get_device().get_capabilities().blend_mix) {
		FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff200080));
	}
	FF_LAMBERT_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.5; ALPHA_SCISSOR_THRESHOLD=0.4; }") == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
	FF_LAMBERT_CHECK(materials.shader_set_code(shader, source) == OK);
	const RID line = _mesh(meshes, 0, false, Vector3(), false, true);
	instance.data->base = line;
	environment.ambient = Color(0.25, 0.25, 0.25);
	FF_LAMBERT_CHECK(materials.material_set_line_width(material, 3) == OK && materials.material_set_param(material, "tint", Color(1, 0, 0)) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
	FF_LAMBERT_CHECK(materials.shader_set_code(shader, source.replace("cull_disabled", "cull_disabled,ambient_light_disabled")) == OK);
	instance.data->base = mesh;
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
	FF_LAMBERT_CHECK(lights.free(light) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff000000));
	FF_LAMBERT_CHECK(lights.free(light_instance) == OK && lights.free(negative) == OK && lights.free(negative_instance) == OK);
	FF_LAMBERT_CHECK(meshes.free(mesh) == OK && meshes.free(oblique) == OK && meshes.free(line) == OK);
	FF_LAMBERT_CHECK(materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: directional/ambient Lambert, octahedral/compressed-TBN normal layouts, face/line fallbacks, inverse transpose/mirrors, masks, negative lights, HDR draw copies and independent alpha.\n");
	return true;
}

bool TestFixedFunctionLambert::test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const Variant pixel = GLOBAL_GET("rendering/quality/fixed_function/per_pixel_lighting");
	const Variant native = GLOBAL_GET("rendering/quality/fixed_function/native_lighting");
	const Variant retained = GLOBAL_GET("rendering/quality/fixed_function/retained_geometry");
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", false);
	settings->set("rendering/quality/fixed_function/native_lighting", 1);
	settings->set("rendering/quality/fixed_function/retained_geometry", true);
	p_renderer.set_debug_unshaded(false);
	const bool result = _test_vertex_path(p_context, p_targets, p_renderer) && _test_material_channels(p_context, p_targets, p_renderer) && _test_sky_ambient(p_context, p_targets, p_renderer) && _test_emission_alpha(p_context, p_targets, p_renderer) && _test_transient_normal_maps(p_context, p_targets, p_renderer) && test_streamed_vertex_lighting(p_context, p_targets, p_renderer);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", pixel);
	settings->set("rendering/quality/fixed_function/native_lighting", native);
	settings->set("rendering/quality/fixed_function/retained_geometry", retained);
	p_renderer.set_debug_unshaded(true);
	return result;
}

#undef FF_LAMBERT_CHECK
