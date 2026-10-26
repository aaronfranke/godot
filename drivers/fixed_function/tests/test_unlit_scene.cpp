#include "test_unlit_scene.h"

#include "drivers/fixed_function/renderer_utilities_fixed_function.h"
#include "drivers/fixed_function/tests/test_lambert.h"
#include "drivers/fixed_function/tests/test_light_storage.h"
#include "drivers/fixed_function/tests/test_pixel_lighting.h"
#include "drivers/fixed_function/tests/test_preset_scope.h"
#include "drivers/fixed_function/tests/test_spatial_lines.h"
#include "scene/resources/material.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_globals.h"

#include <stdio.h>
#include <string.h>

#define FF_UNLIT_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Unlit FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

RenderingServerTypes::RenderInfo TestFixedFunctionUnlitScene::_render_info;
TestFixedFunctionUnlitScene::FFInstance::FFInstance(FixedFunctionRenderContext &p_context, RID p_mesh, RID p_material) : FixedFunctionGeometryInstance(p_context, p_mesh) {
	set_material_override(p_material);
}

bool TestFixedFunctionUnlitScene::_color(uint32_t p_actual, uint32_t p_expected) {
	for (int i = 0; i < 24; i += 8) {
		if (Math::abs(int((p_actual >> i) & 255) - int((p_expected >> i) & 255)) > 3) {
			return false;
		}
	}
	return true;
}

// The unit harness's dummy material store discards shader source. Route resource
// emission/freeing through real facades for this scope, without opening a device.

bool TestFixedFunctionUnlitScene::_test_materials(FixedFunctionGraphicsDevice &p_device, void *p_window) {
	TestFixedFunctionPresetScope scope(p_device, p_window);
	FixedFunctionMaterialStorage storage;
	const RID shader = storage.shader_create();
	const RID material = storage.material_create();
	FF_UNLIT_CHECK(storage.material_set_shader(material, shader) == OK);
	for (int flags = 0; flags < 8; flags++) {
		Ref<StandardMaterial3D> preset;
		preset.instantiate();
		preset->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		preset->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, (flags & 1) != 0);
		preset->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, (flags & 2) != 0);
		preset->set_flag(BaseMaterial3D::FLAG_ALBEDO_TEXTURE_FORCE_SRGB, (flags & 4) != 0);
		BaseMaterial3D::flush_changes();
		const String code = RenderingServer::get_singleton()->shader_get_code(preset->get_shader_rid());
		FF_UNLIT_CHECK(storage.shader_set_code(shader, code) == OK);
		FF_UNLIT_CHECK(storage.material_set_param(material, "albedo", Color(0.5, 0.75, 1)) == OK);
		FF_UNLIT_CHECK(storage.material_set_param(material, "uv1_scale", Vector3(2, 3, 1)) == OK && storage.material_set_param(material, "uv1_offset", Vector3(0.25, 0.5, 0)) == OK);
		FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
		const Error error = storage.material_get_spatial_parameters(material, parameters);
		if (error != OK) {
			printf("Preset flags=%d, error=%d\n%s\n", flags, error, code.utf8().get_data());
		}
		FF_UNLIT_CHECK(error == OK && parameters.albedo.factor == Color(0.5, 0.75, 1) && parameters.albedo_uses_vertex_color == bool(flags & 1));
		FF_UNLIT_CHECK(parameters.albedo.uv_set == 0 && parameters.albedo.uv_scale == Vector2(2, 3) && parameters.albedo.uv_offset == Vector2(0.25, 0.5));
	}
	const String supported = "shader_type spatial; render_mode unshaded,cull_disabled; uniform vec4 tint : source_color = vec4(1,0,0,1); void fragment(){ ALBEDO=tint.rgb; }";
	FF_UNLIT_CHECK(storage.shader_set_code(shader, supported) == OK);
	FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
	FF_UNLIT_CHECK(storage.material_get_spatial_parameters(material, parameters) == OK && parameters.albedo.factor == Color(1, 0, 0));
	FF_UNLIT_CHECK(storage.shader_set_code(shader, "shader_type spatial; render_mode unshaded; uniform vec4 tint=vec4(1,0,0,1); void fragment(){ALBEDO=tint.rgb;}") == OK);
	FF_UNLIT_CHECK(storage.material_get_spatial_parameters(material, parameters) == OK && parameters.albedo.factor == Color(1, 0, 0));
	FF_UNLIT_CHECK(storage.material_set_param(material, "tint", Vector4(0, 1, 0, 1)) == OK && storage.material_get_spatial_parameters(material, parameters) == OK && parameters.albedo.factor == Color(0, 1, 0));
	FF_UNLIT_CHECK(storage.material_set_param(material, "tint", Variant()) == OK);
	FF_UNLIT_CHECK(storage.shader_set_code(shader, "shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=vec3(1.0,0.0,0.0); }") == OK);
	FF_UNLIT_CHECK(storage.material_get_spatial_parameters(material, parameters) == OK && parameters.albedo.factor.r == 1 && parameters.albedo.factor.g == 0);
	const char *unsupported[] = {
		"shader_type spatial; void fragment() { ALBEDO=vec3(1.0); } void light(){ DIFFUSE_LIGHT+=vec3(1.0); }",
		"shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=vec3(1.0); ALPHA=sin(TIME); }",
		"shader_type spatial; render_mode unshaded; void vertex(){ VERTEX.x+=1.0; }",
		"shader_type spatial; render_mode unshaded; void fragment(){ discard; }",
		"shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=texture(SCREEN_TEXTURE,SCREEN_UV).rgb; }",
	};
	for (unsigned int i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); i++) {
		FF_UNLIT_CHECK(storage.shader_set_code(shader, unsupported[i]) == OK && storage.material_get_spatial_parameters(material, parameters) != OK && parameters.albedo.factor == Color(1, 0, 0));
	}
	return _test_cutout_materials();
}

bool TestFixedFunctionUnlitScene::_test_textures(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, std::vector<uint32_t> &r_pixels) {
	// UV1 and UV2 intentionally sample different texels. Vertex color and tint modulate RGB.
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	Vector<Vector3> positions;
	positions.push_back(Vector3(-1, -1, 0));
	positions.push_back(Vector3(0, 1, 0));
	positions.push_back(Vector3(1, -1, 0));
	Vector<Vector2> uv1, uv2;
	Vector<Color> colors;
	for (int i = 0; i < 3; i++) {
		uv1.push_back(Vector2(0.25, 0.5));
		uv2.push_back(Vector2(0.75, 0.5));
		colors.push_back(Color(1, 0.5, 1));
	}
	arrays[RSE::ARRAY_VERTEX] = positions;
	arrays[RSE::ARRAY_TEX_UV] = uv1;
	arrays[RSE::ARRAY_TEX_UV2] = uv2;
	arrays[RSE::ARRAY_COLOR] = colors;
	RenderingServerTypes::SurfaceData surface_data;
	FF_UNLIT_CHECK(RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&surface_data, RSE::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) == OK);
	const RID mesh = p_context.get_mesh_storage().mesh_create();
	FF_UNLIT_CHECK(p_context.get_mesh_storage().mesh_add_surface(mesh, surface_data) == OK);
	Vector<uint8_t> bytes;
	bytes.resize(8);
	{
		uint8_t *write = bytes.ptrw();
		const uint8_t pixels[] = { 255, 0, 0, 0, 0, 255, 0, 0 };
		memcpy(write, pixels, 8);
	}
	Ref<Image> image = Image::create_from_data(2, 1, false, Image::FORMAT_RGBA8, bytes);
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID texture = textures.texture_create();
	FF_UNLIT_CHECK(textures.texture_allocate_2d(texture, Size2i(2, 1), Image::FORMAT_RGBA8, 0, 0) == OK && textures.texture_set_data(texture, image) == OK);
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	const String code = "shader_type spatial; render_mode unshaded,cull_back; uniform sampler2D image; uniform vec4 tint : source_color = vec4(0.5,1,1,1); uniform vec2 scale_uv = vec2(1,1); uniform vec2 offset_uv = vec2(0,0); void vertex(){ UV2=UV2*scale_uv+offset_uv; } void fragment(){ ALBEDO=tint.rgb*texture(image,UV2).rgb*COLOR.rgb; }";
	FF_UNLIT_CHECK(materials.shader_set_code(p_shader, code) == OK && materials.material_set_param(p_material, "image", texture) == OK && materials.material_set_param(p_material, "tint", Variant()) == OK);
	FFInstance instance(p_context, mesh, p_material);
	instance.transform.origin.z = -3;
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	FF_UNLIT_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, r_pixels) && _color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xff008000));
	FF_UNLIT_CHECK(materials.material_set_param(p_material, "offset_uv", Vector2(-0.5, 0)) == OK);
	FF_UNLIT_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, r_pixels) && _color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xff800000));
	FF_UNLIT_CHECK(materials.material_set_param(p_material, "offset_uv", Vector2()) == OK);
	FF_UNLIT_CHECK(p_context.resize(160, 120) == OK && p_context.prepare_resources() == OK && p_context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FF_UNLIT_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, r_pixels) && _color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xff008000));
	const RID target = textures.render_target_create();
	const RID target_texture = textures.render_target_get_texture(target);
	FF_UNLIT_CHECK(textures.render_target_set_size(target, 2, 2) == OK && materials.material_set_param(p_material, "image", target_texture) == OK);
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	instances.push_back(&instance);
	FF_UNLIT_CHECK(p_targets.begin_frame(0) == OK);
	FF_UNLIT_CHECK(p_renderer.render(Transform3D(), projection, instances) == ERR_SKIP && p_renderer.is_redraw_requested() && textures.render_target_was_used(target));
	FF_UNLIT_CHECK(p_targets.end_frame(false) == ERR_SKIP);
	// An asymmetric 2D producer tests native viewport orientation and explicit shader UV flips.
	FF_UNLIT_CHECK(p_targets.begin_frame(0) == OK && p_targets.select_render_target(target) == OK && p_targets.clear_render_target(Color(0, 1, 0)) == OK && p_targets.begin_draw() == OK);
	const FixedFunctionVertex top[] = {
		{ -1, 1, 0, 0xffff0000, 0, 0 }, { 1, 1, 0, 0xffff0000, 0, 0 }, { 1, 0, 0, 0xffff0000, 0, 0 }, { -1, 1, 0, 0xffff0000, 0, 0 }, { 1, 0, 0, 0xffff0000, 0, 0 }, { -1, 0, 0, 0xffff0000, 0, 0 }
	};
	FF_UNLIT_CHECK(p_context.get_device().draw_triangles(top, 6, Projection()) == OK && p_targets.select_render_target(RID()) == OK && p_targets.end_frame(false) == OK);
	FF_UNLIT_CHECK(materials.material_set_param(p_material, "offset_uv", Vector2(0, -0.25)) == OK);
	FF_UNLIT_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, r_pixels) && _color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xff800000));
	FF_UNLIT_CHECK(materials.material_set_param(p_material, "scale_uv", Vector2(1, -1)) == OK && materials.material_set_param(p_material, "offset_uv", Vector2(0, 1.25)) == OK);
	FF_UNLIT_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, r_pixels) && _color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xff008000));
	FF_UNLIT_CHECK(materials.material_set_param(p_material, "image", texture) == OK && materials.material_set_param(p_material, "scale_uv", Vector2(1, 1)) == OK && materials.material_set_param(p_material, "offset_uv", Vector2()) == OK && textures.free(target) == OK);
	FF_UNLIT_CHECK(_draw(p_context, p_targets, p_renderer, instance, projection, r_pixels) && _color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xff008000));
	return true;
}

bool TestFixedFunctionUnlitScene::_test_depth_batches(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, std::vector<uint32_t> &r_pixels) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FF_UNLIT_CHECK(materials.shader_set_code(p_shader, "shader_type spatial; render_mode unshaded,cull_back; uniform vec4 tint : source_color; void fragment(){ALBEDO=tint.rgb;}") == OK && materials.material_set_param(p_material, "tint", Color(1, 0, 0)) == OK);
	const RID far_material = materials.material_create();
	FF_UNLIT_CHECK(materials.material_set_shader(far_material, p_shader) == OK && materials.material_set_param(far_material, "tint", Color(0, 1, 0)) == OK);
	const RID mesh = _mesh(p_context.get_mesh_storage());
	FFInstance near_instance(p_context, mesh, p_material), far_instance(p_context, mesh, far_material);
	near_instance.transform.origin.z = -2;
	far_instance.transform.origin.z = -4;
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	instances.push_back(&near_instance);
	instances.push_back(&far_instance);
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	for (int order = 0; order < 2; order++) {
		FF_UNLIT_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
		// Promote a completed color-only background to depth without losing its color.
		FF_UNLIT_CHECK(p_targets.begin_draw() == OK);
		FF_UNLIT_CHECK(p_renderer.render(Transform3D(), projection, instances) == OK && p_targets.end_frame(false) == OK);
		r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
		FF_UNLIT_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK && _color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xffff0000) && _color(r_pixels[0], 0xff0000ff));
		SWAP(instances[0], instances[1]);
	}
	// A native draw cap changes batches, never the logical surface or vertex counts.
	FFInstance batch(p_context, _mesh(p_context.get_mesh_storage(), 9), p_material);
	batch.transform.origin.z = -3;
	FixedFunctionGraphicsDevice::FFCapabilities &caps = const_cast<FixedFunctionGraphicsDevice::FFCapabilities &>(p_context.get_device().get_capabilities());
	const uint32_t old_limit = caps.max_primitives_per_draw;
	caps.max_primitives_per_draw = 1;
	const bool drawn = _draw(p_context, p_targets, p_renderer, batch, projection, r_pixels);
	caps.max_primitives_per_draw = old_limit;
	FF_UNLIT_CHECK(drawn && p_context.get_mesh_storage().mesh_get_surface_count(batch.data->base) == 1 && _color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xffff0000));
	int draws = 0;
	FF_UNLIT_CHECK((draws = _render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]) == 3);
	return true;
}

bool TestFixedFunctionUnlitScene::_test_rejections(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FF_UNLIT_CHECK(materials.shader_set_code(p_shader, "shader_type spatial; render_mode unshaded; uniform vec4 tint : source_color; void fragment(){ALBEDO=tint.rgb;}") == OK);
	FF_UNLIT_CHECK(materials.material_set_param(p_material, "tint", Color(1, 0, 0)) == OK);
	const RID overbright = materials.material_create();
	FF_UNLIT_CHECK(materials.material_set_shader(overbright, p_shader) == OK && materials.material_set_param(overbright, "tint", Color(2, 1, 1)) == OK);
	const RID mesh = _mesh(p_context.get_mesh_storage());
	FFInstance valid(p_context, mesh, p_material), unsupported(p_context, mesh, overbright);
	valid.transform.origin.z = -2;
	unsupported.transform.origin.z = -4;
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	instances.push_back(&valid);
	instances.push_back(&unsupported);
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	FF_UNLIT_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
	FF_UNLIT_CHECK(p_renderer.render(Transform3D(), projection, instances) == OK && p_targets.end_frame(false) == OK);
	std::vector<uint32_t> pixels(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FF_UNLIT_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xffff0000));
	FF_UNLIT_CHECK(Color(materials.material_get_param(overbright, "tint")) == Color(2, 1, 1));
	// Failure after valid geometry was submitted cannot publish a partial image; CPU data survives.
	FF_UNLIT_CHECK(_draw(p_context, p_targets, p_renderer, valid, projection, pixels) && _color(pixels[48 * CAPTURE_WIDTH + 64], 0xffff0000));
	return true;
}

RID TestFixedFunctionUnlitScene::_mesh(FixedFunctionMeshStorage &p_meshes, int p_vertex_count) {
	const RID mesh = p_meshes.mesh_create();
	Vector<uint8_t> bytes;
	bytes.resize(p_vertex_count * 12);
	{
		uint8_t *write = bytes.ptrw();
		const float positions[3][3] = { { -1, -1, 0 }, { 0, 1, 0 }, { 1, -1, 0 } };
		for (int i = 0; i < p_vertex_count; i++) {
			memcpy(write + i * 12, positions[i % 3], 12);
		}
	}
	Vector<uint8_t> indices;
	if (p_vertex_count >= 65536) {
		indices.resize(12);
		uint8_t *write = indices.ptrw();
		const uint32_t corners[] = { 65535, 65536, 65537 };
		memcpy(write, corners, 12);
	}
	RenderingServerTypes::SurfaceData data;
	data.format |= RSE::ARRAY_FORMAT_VERTEX | (indices.size() ? uint64_t(RSE::ARRAY_FORMAT_INDEX) : 0);
	data.primitive = RSE::PRIMITIVE_TRIANGLES;
	data.vertex_data = bytes;
	data.index_data = indices;
	data.vertex_count = p_vertex_count;
	data.index_count = indices.size() ? 3 : 0;
	data.aabb = AABB(Vector3(-1, -1, 0), Vector3(2, 2, 0));
	const Error error = p_meshes.mesh_add_surface(mesh, data);
	return error == OK ? mesh : RID();
}

Error TestFixedFunctionUnlitScene::_render(FixedFunctionSceneRenderer &p_renderer, const Projection &p_projection, RenderGeometryInstance **p_instances, int p_count) {
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> list;
	list.set_page_pool(&pool);
	for (int i = 0; i < p_count; i++) {
		list.push_back(p_instances[i]);
	}
	return p_renderer.render(Transform3D(), p_projection, list);
}

bool TestFixedFunctionUnlitScene::_draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, FFInstance &p_instance, const Projection &p_projection, std::vector<uint32_t> &r_pixels, const Transform3D &p_camera) {
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	instances.push_back(&p_instance);
	FF_UNLIT_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
	_render_info = RenderingServerTypes::RenderInfo();
	const Error error = p_renderer.render(p_camera, p_projection, instances, &_render_info);
	if (error != OK) {
		printf("Render error=%d\n", error);
	}
	FF_UNLIT_CHECK(error == OK && p_targets.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FF_UNLIT_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	return true;
}

bool TestFixedFunctionUnlitScene::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	FF_UNLIT_CHECK(_test_materials(p_device, p_window));
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FixedFunctionRenderTargetManager targets(context);
	FixedFunctionSceneRenderer renderer(context, targets);
	renderer.set_debug_unshaded(true);
	FF_UNLIT_CHECK(context.initialize() == OK);
	FixedFunctionMaterialStorage &materials = context.get_material_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	FF_UNLIT_CHECK(materials.material_set_shader(material, shader) == OK);
	const String source = "shader_type spatial; render_mode unshaded,cull_back; uniform vec4 tint : source_color = vec4(1,0,0,1); void fragment(){ ALBEDO=tint.rgb; }";
	FF_UNLIT_CHECK(materials.shader_set_code(shader, source) == OK);
	FFInstance instance(context, _mesh(context.get_mesh_storage()), material);
	FF_UNLIT_CHECK(instance.data->base.is_valid());
	instance.transform.origin.z = -3;
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	FF_UNLIT_CHECK(_draw(context, targets, renderer, instance, projection, r_pixels));
	if (!_color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xffff0000)) {
		printf("Center pixel=%08x, caps cull=%d/%d\n", r_pixels[48 * CAPTURE_WIDTH + 64], p_device.get_capabilities().cull_back, p_device.get_capabilities().cull_front);
	}
	FF_UNLIT_CHECK(_color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xffff0000) && _color(r_pixels[0], 0xff0000ff));
	const std::vector<uint32_t> original = r_pixels;
	instance.transform.basis.scale(Vector3(-1, 1, 1));
	FF_UNLIT_CHECK(_draw(context, targets, renderer, instance, projection, r_pixels) && r_pixels == original);
	instance.transform.basis = Basis();
	instance.transform.origin = Vector3(5, 0, -2);
	FF_UNLIT_CHECK(_draw(context, targets, renderer, instance, projection, r_pixels, Transform3D(Basis(), Vector3(5, 0, 1))) && r_pixels == original);
	instance.transform.origin = Vector3(0, 0, -3);
	FF_UNLIT_CHECK(materials.shader_set_code(shader, source.replace("cull_back", "cull_front")) == OK);
	FF_UNLIT_CHECK(_draw(context, targets, renderer, instance, projection, r_pixels) && _color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xff0000ff));
	FF_UNLIT_CHECK(materials.shader_set_code(shader, source) == OK);
	instance.data->base = _mesh(context.get_mesh_storage(), 65538);
	FF_UNLIT_CHECK(instance.data->base.is_valid() && _draw(context, targets, renderer, instance, projection, r_pixels) && r_pixels == original);
	projection.set_perspective(60, real_t(CAPTURE_WIDTH) / real_t(CAPTURE_HEIGHT), 0.1, 20);
	FF_UNLIT_CHECK(_draw(context, targets, renderer, instance, projection, r_pixels) && _color(r_pixels[48 * CAPTURE_WIDTH + 64], 0xffff0000));
	FF_UNLIT_CHECK(_test_depth_batches(context, targets, renderer, shader, material, r_pixels));
	FF_UNLIT_CHECK(_test_rejections(context, targets, renderer, shader, material));
	FF_UNLIT_CHECK(_test_fallbacks(context, targets, renderer) && _test_cutouts(context, targets, renderer));
	FF_UNLIT_CHECK(TestFixedFunctionSpatialLines::test(context, targets, renderer));
	FF_UNLIT_CHECK(TestFixedFunctionLambert::test(context, targets, renderer));
	FF_UNLIT_CHECK(TestFixedFunctionPixelLighting::test(context, targets, renderer));
	FF_UNLIT_CHECK(_test_textures(context, targets, renderer, shader, material, r_pixels));
	FF_UNLIT_CHECK(TestFixedFunctionLightStorage::test(context));
	FF_UNLIT_CHECK(context.finalize() == OK);
	printf("PASS: opaque unlit presets, shader rejection, camera/mesh draws, culling/mirrors and large source indices.\n");
	return true;
}

#undef FF_UNLIT_CHECK
