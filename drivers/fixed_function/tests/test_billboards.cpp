#include "drivers/fixed_function/tests/test_preset_scope.h"
#include "drivers/fixed_function/tests/test_retained_scene.h"
#include "scene/resources/material.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>
#include <string.h>

#define FF_BILLBOARD_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Billboard FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionRetainedScene::_test_billboards(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	TestFixedFunctionPresetScope scope(p_context.get_device(), nullptr);
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID icon_shader = materials.shader_create(), cube_shader = materials.shader_create();
	const RID icon_material = materials.material_create(), cube_material = materials.material_create();
	const RID texture = textures.texture_create(), icon_mesh = meshes.mesh_create(), cube_mesh = meshes.mesh_create();

	Ref<StandardMaterial3D> preset;
	preset.instantiate();
	preset->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	preset->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	preset->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
	preset->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	preset->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	preset->set_flag(BaseMaterial3D::FLAG_FIXED_SIZE, true);
	preset->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	preset->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_DISABLED);
	preset->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	preset->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	BaseMaterial3D::flush_changes();
	const String code = RenderingServer::get_singleton()->shader_get_code(preset->get_shader_rid());
	FF_BILLBOARD_CHECK(materials.shader_set_code(icon_shader, code) == OK && materials.material_set_shader(icon_material, icon_shader) == OK);
	FF_BILLBOARD_CHECK(materials.material_set_param(icon_material, "albedo", Color(1, 1, 1, 0.8)) == OK);
	FF_BILLBOARD_CHECK(materials.material_set_param(icon_material, "uv1_scale", Vector3(1, 1, 1)) == OK);
	FF_BILLBOARD_CHECK(materials.material_set_param(icon_material, "uv1_offset", Vector3()) == OK);
	FF_BILLBOARD_CHECK(materials.material_set_param(icon_material, "texture_albedo", texture) == OK);
	FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
	const uint8_t icon_flags = FixedFunctionSpatialMaterial::TRANSFORM_BILLBOARD | FixedFunctionSpatialMaterial::TRANSFORM_FIXED_SIZE;
	FF_BILLBOARD_CHECK(materials.material_get_spatial_parameters(icon_material, parameters) == OK && parameters.transform_flags == icon_flags);
	FF_BILLBOARD_CHECK(parameters.albedo_uses_vertex_color && parameters.albedo_uses_vertex_alpha && parameters.state.depth_test_enabled && !parameters.state.depth_write_enabled && parameters.state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_MIX);
	// A different model-view scale must continue to use the ordinary shader fallback.
	const String custom = code.replace("sc = -(MODELVIEW_MATRIX)[3].z;", "sc = 2.0;");
	FF_BILLBOARD_CHECK(custom != code && materials.shader_set_code(icon_shader, custom) == OK);
	FF_BILLBOARD_CHECK(materials.material_get_spatial_parameters(icon_material, parameters) == ERR_UNAVAILABLE);
	preset->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, true);
	BaseMaterial3D::flush_changes();
	FF_BILLBOARD_CHECK(materials.shader_set_code(icon_shader, RenderingServer::get_singleton()->shader_get_code(preset->get_shader_rid())) == OK);
	FF_BILLBOARD_CHECK(materials.material_get_spatial_parameters(icon_material, parameters) == OK && parameters.transform_flags == (icon_flags | FixedFunctionSpatialMaterial::TRANSFORM_KEEP_SCALE));
	FF_BILLBOARD_CHECK(materials.shader_set_code(icon_shader, code) == OK);
	FF_BILLBOARD_CHECK(materials.shader_set_code(cube_shader, "shader_type spatial; render_mode unshaded,cull_disabled; void fragment(){ ALBEDO=vec3(0,1,0); }") == OK && materials.material_set_shader(cube_material, cube_shader) == OK);

	const uint8_t texels[] = { 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 128 };
	Vector<uint8_t> bytes;
	FF_BILLBOARD_CHECK(bytes.resize(sizeof(texels)) == OK);
	memcpy(bytes.ptrw(), texels, sizeof(texels));
	Ref<Image> image = Image::create_from_data(4, 1, false, Image::FORMAT_RGBA8, bytes);
	FF_BILLBOARD_CHECK(textures.texture_set_data(texture, image) == OK);

	// Use the editor's indexed triangle layout and separate attribute data.
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	arrays[RSE::ARRAY_VERTEX] = Vector<Vector3>{ Vector3(-0.31, 0.31, 0), Vector3(0.31, 0.31, 0), Vector3(-0.31, -0.31, 0), Vector3(0.31, -0.31, 0) };
	arrays[RSE::ARRAY_COLOR] = Vector<Color>{ Color(1, 1, 1, 0.75), Color(1, 1, 1, 0.75), Color(1, 1, 1, 0.75), Color(1, 1, 1, 0.75) };
	arrays[RSE::ARRAY_TEX_UV] = Vector<Vector2>{ Vector2(0, 0), Vector2(1, 0), Vector2(0, 1), Vector2(1, 1) };
	arrays[RSE::ARRAY_INDEX] = Vector<int>{ 0, 1, 3, 0, 3, 2 };
	RenderingServerTypes::SurfaceData icon_surface;
	FF_BILLBOARD_CHECK(RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&icon_surface, RSE::PRIMITIVE_TRIANGLES, arrays) == OK && meshes.mesh_add_surface(icon_mesh, icon_surface) == OK);
	const float cube[][3] = { { -0.4f, -0.4f, -0.4f }, { 0.4f, -0.4f, -0.4f }, { 0.4f, 0.4f, -0.4f }, { -0.4f, 0.4f, -0.4f }, { -0.4f, -0.4f, 0.4f }, { 0.4f, -0.4f, 0.4f }, { 0.4f, 0.4f, 0.4f }, { -0.4f, 0.4f, 0.4f } };
	const uint16_t cube_indices[] = { 0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1, 3, 2, 6, 3, 6, 7, 0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2 };
	RenderingServerTypes::SurfaceData cube_surface;
	cube_surface.primitive = RSE::PRIMITIVE_TRIANGLES;
	cube_surface.format |= RSE::ARRAY_FORMAT_VERTEX | RSE::ARRAY_FORMAT_INDEX;
	cube_surface.vertex_count = 8;
	cube_surface.index_count = 36;
	cube_surface.aabb = AABB(Vector3(-0.4, -0.4, -0.4), Vector3(0.8, 0.8, 0.8));
	FF_BILLBOARD_CHECK(cube_surface.vertex_data.resize(sizeof(cube)) == OK && cube_surface.index_data.resize(sizeof(cube_indices)) == OK);
	memcpy(cube_surface.vertex_data.ptrw(), cube, sizeof(cube));
	memcpy(cube_surface.index_data.ptrw(), cube_indices, sizeof(cube_indices));
	FF_BILLBOARD_CHECK(meshes.mesh_add_surface(cube_mesh, cube_surface) == OK);

	FFInstance icon_instance(p_context, icon_mesh, icon_material), cube_instance(p_context, cube_mesh, cube_material);
	icon_instance.set_transform(Transform3D(Basis(Vector3(0, 1, 0), Math::PI / 2), Vector3()), AABB(), AABB());
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> instances;
	instances.set_page_pool(&pool);
	const int width = p_context.get_device().get_width(), height = p_context.get_device().get_height();
	const int center = height / 2 * width + width / 2;
	std::vector<uint32_t> pixels(size_t(width) * height), reference;
	// The texture, vertex alpha, and blend operations can each round down on RGB.
	const int blend_tolerance = 3;
	const auto draw = [&](const Transform3D &p_camera, const Projection &p_projection, int p_count) {
		instances.clear();
		instances.push_back(&icon_instance);
		if (p_count == 2) {
			instances.push_back(&cube_instance);
		}
		return p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK &&
				p_renderer.render(p_camera, p_projection, instances, nullptr) == OK && p_targets.end_frame(false) == OK &&
				p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK;
	};
	Projection perspective, orthogonal, zoomed;
	perspective.set_perspective(60, real_t(width) / height, 0.1, 30);
	orthogonal.set_orthogonal(3, real_t(width) / height, 0.1, 30);
	zoomed.set_orthogonal(6, real_t(width) / height, 0.1, 30);
	const Transform3D near_camera(Basis(), Vector3(0, 0, 4));
	const Transform3D far_camera(Basis(), Vector3(0, 0, 8));
	const Transform3D side_camera(Basis(Vector3(0, 1, 0), Math::PI / 2), Vector3(8, 0, 0));
	FF_BILLBOARD_CHECK(draw(near_camera, perspective, 1));
	const int alpha_sample = int(height * 0.5 * 0.31 * perspective.columns[1][1] * 0.75);
	FF_BILLBOARD_CHECK(pixels[center - alpha_sample] == 0xff0000ff || pixels[center - alpha_sample] == 0xff0000fe);
	FF_BILLBOARD_CHECK(_color_matches(pixels[center], 0xff9999ff, blend_tolerance));
	FF_BILLBOARD_CHECK(_color_matches(pixels[center + alpha_sample], 0xff4d4dff, blend_tolerance));
	reference = pixels;
	FF_BILLBOARD_CHECK(draw(far_camera, perspective, 1) && pixels == reference);
	FF_BILLBOARD_CHECK(draw(side_camera, perspective, 1) && pixels == reference);
	FF_BILLBOARD_CHECK(draw(near_camera, orthogonal, 1));
	FF_BILLBOARD_CHECK(_color_matches(pixels[center], 0xff9999ff, blend_tolerance));
	reference = pixels;
	FF_BILLBOARD_CHECK(draw(side_camera, zoomed, 1) && pixels == reference);

	// Supply the icon first. The opaque cube still draws first and occludes it only in front.
	cube_instance.set_transform(Transform3D(Basis(), Vector3(0, 0, 1)), AABB(), AABB());
	FF_BILLBOARD_CHECK(draw(near_camera, perspective, 2) && pixels[center] == 0xff00ff00);
	cube_instance.set_transform(Transform3D(Basis(), Vector3(0, 0, -1)), AABB(), AABB());
	FF_BILLBOARD_CHECK(draw(near_camera, perspective, 2) && _color_matches(pixels[center], 0xff99ff99, blend_tolerance));
	// The 4.x editor icon uses alpha scissor. Check its generated recipe and pixels too.
	preset->set_flag(BaseMaterial3D::FLAG_BILLBOARD_KEEP_SCALE, false);
	preset->set_cull_mode(BaseMaterial3D::CULL_BACK);
	preset->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
	preset->set_alpha_scissor_threshold(0.4);
	BaseMaterial3D::flush_changes();
	FF_BILLBOARD_CHECK(materials.shader_set_code(icon_shader, RenderingServer::get_singleton()->shader_get_code(preset->get_shader_rid())) == OK);
	FF_BILLBOARD_CHECK(materials.material_set_param(icon_material, "alpha_scissor_threshold", 0.4) == OK);
	FF_BILLBOARD_CHECK(materials.material_get_spatial_parameters(icon_material, parameters) == OK && parameters.state.alpha_cutoff_enabled && parameters.transform_flags == icon_flags);
	if (p_context.get_device().get_capabilities().alpha_cutoff) {
		FF_BILLBOARD_CHECK(draw(near_camera, perspective, 1) && _color_matches(pixels[center], 0xffffffff, 1) && pixels[center + alpha_sample] == 0xff0000ff);
	}
	FF_BILLBOARD_CHECK(meshes.free(icon_mesh) == OK && meshes.free(cube_mesh) == OK && textures.free(texture) == OK);
	FF_BILLBOARD_CHECK(materials.free(icon_material) == OK && materials.free(cube_material) == OK && materials.free(icon_shader) == OK && materials.free(cube_shader) == OK);
	printf("PASS: generated billboard materials, perspective and orthogonal fixed size, texture and vertex alpha, and opaque cube depth.\n");
	return true;
}

#undef FF_BILLBOARD_CHECK
