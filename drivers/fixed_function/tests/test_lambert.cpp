#include "test_lambert.h"
#include "test_scene_cpu.h"

#include "core/project_settings.h"

#include "scene/resources/material.h"

#include <stdio.h>
#include <limits>

#define FF_LAMBERT_CHECK(m_condition)                                   \
	do {                                                                \
		if (!(m_condition)) {                                           \
			printf("Lambert FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                               \
		}                                                               \
	} while (false)

TestFixedFunctionLambert::FFInstance::FFInstance(RID p_mesh, RID p_material) {
	base_type = VS::INSTANCE_MESH;
	base = p_mesh;
	material_override = p_material;
	depth = 2;
	transform.origin.z = -2;
}

bool TestFixedFunctionLambert::_color(uint32_t p_actual, uint32_t p_expected) {
	for (int shift = 0; shift < 32; shift += 8) {
		if (ABS(int((p_actual >> shift) & 255) - int((p_expected >> shift) & 255)) > 3) {
			return false;
		}
	}
	return true;
}

RID TestFixedFunctionLambert::_mesh(FixedFunctionMeshStorage &p_storage, uint32_t p_compression, bool p_normal, const Vector3 &p_direction, bool p_tangents, bool p_lines, bool p_gradient) {
	VisualServer *server = VisualServer::get_singleton();
	const RID source = server->mesh_create();
	Array arrays;
	arrays.resize(VS::ARRAY_MAX);
	PoolVector<Vector3> positions, normals;
	PoolRealArray tangents;
	positions.push_back(Vector3(-1, -1, 0));
	positions.push_back(Vector3(0, 1, 0));
	positions.push_back(Vector3(1, -1, 0));
	if (p_lines) {
		positions.resize(2);
		positions.write()[0] = Vector3(-1, 0, 0);
		positions.write()[1] = Vector3(1, 0, 0);
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
		tangents.push_back(1);
		tangents.push_back(0);
		tangents.push_back(0);
		tangents.push_back(1);
	}
	arrays[VS::ARRAY_VERTEX] = positions;
	if (p_normal) {
		arrays[VS::ARRAY_NORMAL] = normals;
	}
	if (p_tangents) {
		arrays[VS::ARRAY_TANGENT] = tangents;
	}
	const VS::PrimitiveType primitive = p_lines ? VS::PRIMITIVE_LINES : VS::PRIMITIVE_TRIANGLES;
	server->mesh_add_surface_from_arrays(source, primitive, arrays, Array(), p_compression);
	const RID mesh = p_storage.mesh_create();
	const Error error = p_storage.mesh_add_surface(mesh, server->mesh_surface_get_format(source, 0), primitive, server->mesh_surface_get_array(source, 0), positions.size(), PoolVector<uint8_t>(), 0, AABB(Vector3(-1, -1, 0), Vector3(2, 2, 0)), Vector<PoolVector<uint8_t>>(), Vector<AABB>());
	server->free(source);
	return error == OK ? mesh : RID();
}

bool TestFixedFunctionLambert::_draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, FFInstance &p_instance, RID p_environment, const Vector<RID> &p_lights, std::vector<uint32_t> &r_pixels, const Transform &p_camera) {
	CameraMatrix projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	RasterizerScene::InstanceBase *instances[] = { &p_instance };
	FF_LAMBERT_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
	FF_LAMBERT_CHECK(p_renderer.render(p_camera, projection, instances, 1, p_environment, p_lights.ptr(), p_lights.size()) == OK && p_targets.end_frame(false) == OK);
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
	FF_LAMBERT_CHECK(FixedFunctionSceneRenderer::_unit_normal(Vector3(std::numeric_limits<real_t>::quiet_NaN(), 0, 1)) == Vector3());
	return true;
}

bool TestFixedFunctionLambert::_materials(FixedFunctionMaterialStorage &p_materials) {
	const RID shader = p_materials.shader_create(), material = p_materials.material_create();
	FF_LAMBERT_CHECK(p_materials.material_set_shader(material, shader) == OK);
	for (int vertex = 0; vertex < 2; vertex++) {
		Ref<SpatialMaterial> preset;
		preset.instance();
		preset->set_flag(Material3D::FLAG_USE_VERTEX_LIGHTING, vertex != 0);
		preset->set_feature(Material3D::FEATURE_NORMAL_MAPPING, true);
		Material3D::flush_changes();
		const String source = VS::get_singleton()->shader_get_code(preset->get_shader_rid());
		FF_LAMBERT_CHECK(p_materials.shader_set_code(shader, source) == OK);
		FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
		FF_LAMBERT_CHECK(p_materials.material_get_spatial_parameters(material, parameters) == OK && parameters.lighting_flags == (vertex ? FixedFunctionSpatialMaterial::LIGHTING_VERTEX : 0));
	}
	FF_LAMBERT_CHECK(p_materials.free(material) == OK && p_materials.free(shader) == OK);
	return true;
}

bool TestFixedFunctionLambert::_normal_formats(FixedFunctionMeshStorage &p_storage) {
	const uint32_t formats[] = { 0, VS::ARRAY_COMPRESS_NORMAL, VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION, VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION | VS::ARRAY_COMPRESS_NORMAL | VS::ARRAY_COMPRESS_TANGENT };
	const Vector3 directions[] = { Vector3(0, 0, -1), Vector3(-1, 2, -3).normalized(), Vector3(1, -3, 2).normalized() };
	for (unsigned int format = 0; format < sizeof(formats) / sizeof(formats[0]); format++) {
		for (unsigned int direction = 0; direction < sizeof(directions) / sizeof(directions[0]); direction++) {
			const RID mesh = _mesh(p_storage, formats[format], true, directions[direction], format == 3);
			FixedFunctionMeshStorage::FFSurfaceData surface;
			FF_LAMBERT_CHECK(mesh.is_valid() && p_storage.mesh_surface_get_data(mesh, 0, surface) == OK);
			const uint32_t mask = VS::ARRAY_COMPRESS_NORMAL | VS::ARRAY_COMPRESS_TANGENT | VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION;
			FF_LAMBERT_CHECK((surface.format & mask) == formats[format]);
			const PoolVector<uint8_t>::Read bytes = surface.array.read();
			for (int vertex = 0; vertex < 3; vertex++) {
				const Vector3 decoded = FixedFunctionSceneRenderer::_read_normal(surface, bytes.ptr(), vertex);
				FF_LAMBERT_CHECK(decoded.distance_to(directions[direction]) < real_t(0.025));
			}
			FF_LAMBERT_CHECK(p_storage.free(mesh) == OK);
		}
	}
	return true;
}

bool TestFixedFunctionLambert::_test_vertex_path(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FF_LAMBERT_CHECK(_math() && _materials(p_context.get_material_storage()) && _normal_formats(p_context.get_mesh_storage()));
	FixedFunctionLightStorage &lights = p_context.get_light_storage();
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionSceneStorage &scenes = p_context.get_scene_storage();
	const RID shader = materials.shader_create(), material = materials.material_create();
	const String source = "shader_type spatial; render_mode cull_disabled; uniform vec4 tint : hint_color = vec4(1,0,0,1); void fragment(){ ALBEDO=tint.rgb; }";
	FF_LAMBERT_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, source) == OK);
	const RID environment = scenes.environment_create();
	FF_LAMBERT_CHECK(scenes.environment_set_ambient(environment, Color(0.25, 0.25, 0.25), 1, 1) == OK);
	const RID light = lights.light_create(VS::LIGHT_DIRECTIONAL), light_instance = lights.light_instance_create(light);
	Vector<RID> active;
	active.push_back(light_instance);
	FixedFunctionLightStorage::FFLightData data;
	FF_LAMBERT_CHECK(lights.light_get_data(light, data) == OK);
	data.parameters[VS::LIGHT_PARAM_ENERGY] = 0.5;
	FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK);
	const uint32_t formats[] = { 0, VS::ARRAY_COMPRESS_NORMAL, VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION, VS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION | VS::ARRAY_COMPRESS_NORMAL | VS::ARRAY_COMPRESS_TANGENT };
	std::vector<uint32_t> pixels;
	const RID no_environment_mesh = _mesh(meshes, 0, true);
	FF_LAMBERT_CHECK(no_environment_mesh.is_valid() && materials.material_set_param(material, "tint", Color(1, 0, 0.5)) == OK);
	FFInstance no_environment_instance(no_environment_mesh, material);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, no_environment_instance, RID(), Vector<RID>(), pixels));
	FF_LAMBERT_CHECK(_color(pixels[48 * 128 + 64], 0xff000080));
	FF_LAMBERT_CHECK(materials.material_set_param(material, "tint", Color(1, 0, 0)) == OK && meshes.free(no_environment_mesh) == OK);
	for (unsigned int format = 0; format < sizeof(formats) / sizeof(formats[0]); format++) {
		const RID mesh = _mesh(meshes, formats[format], true, Vector3(0, 0, 1), format == 3);
		FF_LAMBERT_CHECK(mesh.is_valid());
		FFInstance instance(mesh, material);
		FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xffbf0000));
		instance.layer_mask = 2;
		data.cull_mask = 1;
		FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
		instance.layer_mask = 1;
		const Basis camera_basis(Vector3(0, 1, 0), real_t(0.3));
		const Transform camera(camera_basis, Vector3(0, 0, -2) + camera_basis.get_axis(2) * 2);
		FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels, camera) && _color(pixels[48 * 128 + 64], 0xffbf0000));
		FF_LAMBERT_CHECK(meshes.free(mesh) == OK);
	}
	const RID mesh = _mesh(meshes, 0, false);
	FFInstance instance(mesh, material);
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
		const RID omni = lights.light_create(VS::LIGHT_OMNI);
		const RID omni_instance = lights.light_instance_create(omni);
		FF_LAMBERT_CHECK(lights.light_instance_set_transform(omni_instance, Transform(Basis(), Vector3(0, 0, -1.5))) == OK);
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
	FF_LAMBERT_CHECK(lights.light_instance_set_transform(light_instance, Transform(Basis(Vector3(0, 1, 0), Math_PI), Vector3())) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
	FF_LAMBERT_CHECK(lights.light_instance_set_transform(light_instance, Transform()) == OK);
	const RID invalid_normal = _mesh(meshes, 0, true, Vector3(std::numeric_limits<real_t>::quiet_NaN(), 0, 1));
	instance.base = invalid_normal;
	FF_LAMBERT_CHECK(invalid_normal.is_valid() && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xffbf0000));
	FF_LAMBERT_CHECK(meshes.free(invalid_normal) == OK);
	instance.base = mesh;
	instance.transform.basis.scale(Vector3(1, 1, 0));
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xffbf0000));
	instance.transform.basis = Basis();
	const RID oblique = _mesh(meshes, 0, true, Vector3(1, 0, 1).normalized());
	instance.base = oblique;
	instance.transform.basis.scale(Vector3(2, 1, real_t(0.5)));
	FF_LAMBERT_CHECK(scenes.environment_set_ambient(environment, Color(0, 0, 0), 1, 0) == OK);
	data.parameters[VS::LIGHT_PARAM_ENERGY] = 1;
	FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xfff70000));
	instance.base = mesh;
	instance.transform.basis = Basis();
	const RID gradient = _mesh(meshes, 0, true, Vector3(), false, false, true);
	instance.base = gradient;
	FF_LAMBERT_CHECK(gradient.is_valid() && _draw(p_context, p_targets, p_renderer, instance, environment, active, pixels));
	FF_LAMBERT_CHECK(int((pixels[72 * 128 + 40] >> 16) & 255) > int((pixels[72 * 128 + 88] >> 16) & 255) + 128 && ((pixels[20 * 128 + 64] >> 16) & 255) < 32);
	FF_LAMBERT_CHECK(meshes.free(gradient) == OK);
	instance.base = mesh;
	FF_LAMBERT_CHECK(materials.material_set_param(material, "tint", Color(2, 0, 0)) == OK);
	data.parameters[VS::LIGHT_PARAM_ENERGY] = 0.25;
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
	const RID negative = lights.light_create(VS::LIGHT_DIRECTIONAL), negative_instance = lights.light_instance_create(negative);
	FixedFunctionLightStorage::FFLightData subtraction;
	FF_LAMBERT_CHECK(lights.light_get_data(negative, subtraction) == OK);
	subtraction.flags |= FixedFunctionLightStorage::LIGHT_NEGATIVE;
	subtraction.parameters[VS::LIGHT_PARAM_ENERGY] = 0.125;
	FF_LAMBERT_CHECK(lights.light_set_data(negative, subtraction) == OK);
	active.push_back(negative_instance);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
	data.parameters[VS::LIGHT_PARAM_ENERGY] = 2;
	subtraction.parameters[VS::LIGHT_PARAM_ENERGY] = 1.75;
	FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK && lights.light_set_data(negative, subtraction) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff800000));
	active.resize(1);
	data.parameters[VS::LIGHT_PARAM_ENERGY] = 0.25;
	FF_LAMBERT_CHECK(lights.light_set_data(light, data) == OK);
	PoolVector<uint8_t> image_bytes;
	image_bytes.resize(4);
	{
		PoolVector<uint8_t>::Write write = image_bytes.write();
		write[0] = 128;
		write[1] = 255;
		write[2] = 64;
		write[3] = 255;
	}
	Ref<Image> image = memnew(Image(1, 1, false, Image::FORMAT_RGBA8, image_bytes));
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID texture = textures.texture_create();
	FF_LAMBERT_CHECK(textures.texture_allocate_2d(texture, Size2i(1, 1), Image::FORMAT_RGBA8, 0, 0) == OK);
	FF_LAMBERT_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; uniform sampler2D image; void fragment(){ ALBEDO=texture(image,UV).rgb; }") == OK && materials.material_set_param(material, "image", texture) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff404040));
	FF_LAMBERT_CHECK(textures.texture_set_data(texture, image) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff204010));
	FF_LAMBERT_CHECK(materials.material_set_param(material, "image", Variant()) == OK && textures.free(texture) == OK);
	FF_LAMBERT_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.5; }") == OK);
	if (p_context.get_device().get_capabilities().blend_mix) {
		FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff200080));
	}
	FF_LAMBERT_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode cull_disabled; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.5; ALPHA_SCISSOR=0.4; }") == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
	FF_LAMBERT_CHECK(materials.shader_set_code(shader, source) == OK);
	const RID line = _mesh(meshes, 0, false, Vector3(), false, true);
	instance.base = line;
	FF_LAMBERT_CHECK(scenes.environment_set_ambient(environment, Color(0.25, 0.25, 0.25), 1, 0) == OK);
	FF_LAMBERT_CHECK(materials.material_set_line_width(material, 3) == OK && materials.material_set_param(material, "tint", Color(1, 0, 0)) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
	FF_LAMBERT_CHECK(materials.shader_set_code(shader, source.replace("cull_disabled", "cull_disabled,ambient_light_disabled")) == OK);
	instance.base = mesh;
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff400000));
	FF_LAMBERT_CHECK(lights.free(light) == OK);
	FF_LAMBERT_CHECK(_draw(p_context, p_targets, p_renderer, instance, environment, active, pixels) && _color(pixels[48 * 128 + 64], 0xff000000));
	FF_LAMBERT_CHECK(lights.free(light_instance) == OK && lights.free(negative) == OK && lights.free(negative_instance) == OK);
	FF_LAMBERT_CHECK(meshes.free(mesh) == OK && meshes.free(oblique) == OK && meshes.free(line) == OK);
	FF_LAMBERT_CHECK(materials.free(material) == OK && materials.free(shader) == OK && scenes.free(environment) == OK);
	printf("PASS: directional/ambient Lambert, four normal formats, face/line fallbacks, inverse transpose/mirrors, masks, negative lights, HDR draw copies and independent alpha.\n");
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
	const bool result = _test_vertex_path(p_context, p_targets, p_renderer);
	settings->set("rendering/quality/fixed_function/per_pixel_lighting", pixel);
	settings->set("rendering/quality/fixed_function/native_lighting", native);
	settings->set("rendering/quality/fixed_function/retained_geometry", retained);
	return result && TestFixedFunctionSceneCPU::_deformed_tangents() && _test_sky_ambient(p_context, p_targets, p_renderer) && _test_material_channels(p_context, p_targets, p_renderer) && _test_emission_alpha(p_context, p_targets, p_renderer) && _test_transient_normal_maps(p_context, p_targets, p_renderer) && _test_skinning(p_context, p_targets, p_renderer) && _test_blend_shapes(p_context, p_targets, p_renderer) && _test_multimesh(p_context, p_targets, p_renderer);
}

#undef FF_LAMBERT_CHECK
