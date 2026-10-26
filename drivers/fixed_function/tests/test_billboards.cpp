#include "test_retained_scene.h"

#include "scene/resources/material.h"

#include <stdio.h>
#include <string.h>

#define FF_BILLBOARD_CHECK(m_condition)                                        \
	do {                                                                       \
		if (!(m_condition)) {                                                  \
			printf("Billboard FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                      \
		}                                                                      \
	} while (false)

bool TestFixedFunctionRetainedScene::_test_billboards(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID icon_shader = materials.shader_create(), cube_shader = materials.shader_create();
	const RID icon_material = materials.material_create(), cube_material = materials.material_create();
	const RID texture = textures.texture_create(), icon_mesh = meshes.mesh_create(), cube_mesh = meshes.mesh_create();

	Ref<SpatialMaterial> preset;
	preset.instance();
	preset->set_flag(Material3D::FLAG_UNSHADED, true);
	preset->set_flag(Material3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	preset->set_flag(Material3D::FLAG_SRGB_VERTEX_COLOR, true);
	preset->set_flag(Material3D::FLAG_FIXED_SIZE, true);
	preset->set_cull_mode(Material3D::CULL_DISABLED);
	preset->set_depth_draw_mode(Material3D::DEPTH_DRAW_DISABLED);
	preset->set_feature(Material3D::FEATURE_TRANSPARENT, true);
	preset->set_billboard_mode(Material3D::BILLBOARD_ENABLED);
	Material3D::flush_changes();
	const String code = VS::get_singleton()->shader_get_code(preset->get_shader_rid());
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
	const String custom = code.replace("float sc = -(MODELVIEW_MATRIX)[3].z;", "float sc = 2.0;");
	FF_BILLBOARD_CHECK(custom != code && materials.shader_set_code(icon_shader, custom) == OK);
	FF_BILLBOARD_CHECK(materials.material_get_spatial_parameters(icon_material, parameters) == ERR_UNAVAILABLE);
	preset->set_flag(Material3D::FLAG_BILLBOARD_KEEP_SCALE, true);
	Material3D::flush_changes();
	FF_BILLBOARD_CHECK(materials.shader_set_code(icon_shader, VS::get_singleton()->shader_get_code(preset->get_shader_rid())) == OK);
	FF_BILLBOARD_CHECK(materials.material_get_spatial_parameters(icon_material, parameters) == OK && parameters.transform_flags == (icon_flags | FixedFunctionSpatialMaterial::TRANSFORM_KEEP_SCALE));
	FF_BILLBOARD_CHECK(materials.shader_set_code(icon_shader, code) == OK);
	FF_BILLBOARD_CHECK(materials.shader_set_code(cube_shader, "shader_type spatial; render_mode unshaded,cull_disabled; void fragment(){ ALBEDO=vec3(0,1,0); }") == OK && materials.material_set_shader(cube_material, cube_shader) == OK);

	const uint8_t texels[] = { 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 128 };
	PoolVector<uint8_t> bytes;
	FF_BILLBOARD_CHECK(bytes.resize(sizeof(texels)) == OK);
	{
		PoolVector<uint8_t>::Write write = bytes.write();
		memcpy(write.ptr(), texels, sizeof(texels));
	}
	Ref<Image> image = memnew(Image(4, 1, false, Image::FORMAT_RGBA8, bytes));
	FF_BILLBOARD_CHECK(textures.texture_set_data(texture, image) == OK);

	// Match the editor icon fan's packed position, COLOR and UV streams.
	const float icon[][9] = {
		{ -0.31f, 0.31f, 0, 1, 1, 1, 0.75f, 0, 0 },
		{ 0.31f, 0.31f, 0, 1, 1, 1, 0.75f, 1, 0 },
		{ 0.31f, -0.31f, 0, 1, 1, 1, 0.75f, 1, 1 },
		{ -0.31f, -0.31f, 0, 1, 1, 1, 0.75f, 0, 1 },
	};
	const uint32_t icon_format = VS::ARRAY_FORMAT_VERTEX | VS::ARRAY_FORMAT_COLOR | VS::ARRAY_FORMAT_TEX_UV;
	FF_BILLBOARD_CHECK(bytes.resize(sizeof(icon)) == OK);
	{
		PoolVector<uint8_t>::Write write = bytes.write();
		memcpy(write.ptr(), icon, sizeof(icon));
	}
	FF_BILLBOARD_CHECK(meshes.mesh_add_surface(icon_mesh, icon_format, VS::PRIMITIVE_TRIANGLE_FAN, bytes, 4, PoolVector<uint8_t>(), 0, AABB(Vector3(-0.31, -0.31, 0), Vector3(0.62, 0.62, 0)), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
	const float cube[][3] = { { -0.4f, -0.4f, -0.4f }, { 0.4f, -0.4f, -0.4f }, { 0.4f, 0.4f, -0.4f }, { -0.4f, 0.4f, -0.4f }, { -0.4f, -0.4f, 0.4f }, { 0.4f, -0.4f, 0.4f }, { 0.4f, 0.4f, 0.4f }, { -0.4f, 0.4f, 0.4f } };
	const uint16_t cube_indices[] = { 0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1, 3, 2, 6, 3, 6, 7, 0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2 };
	PoolVector<uint8_t> indices;
	FF_BILLBOARD_CHECK(bytes.resize(sizeof(cube)) == OK && indices.resize(sizeof(cube_indices)) == OK);
	{
		PoolVector<uint8_t>::Write vertices_write = bytes.write(), indices_write = indices.write();
		memcpy(vertices_write.ptr(), cube, sizeof(cube));
		memcpy(indices_write.ptr(), cube_indices, sizeof(cube_indices));
	}
	FF_BILLBOARD_CHECK(meshes.mesh_add_surface(cube_mesh, VS::ARRAY_FORMAT_VERTEX | VS::ARRAY_FORMAT_INDEX, VS::PRIMITIVE_TRIANGLES, bytes, 8, indices, 36, AABB(Vector3(-0.4, -0.4, -0.4), Vector3(0.8, 0.8, 0.8)), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);

	FFInstance icon_instance(icon_mesh, icon_material), cube_instance(cube_mesh, cube_material);
	icon_instance.transform = Transform(Basis(Vector3(0, 1, 0), Math_PI / 2), Vector3());
	RasterizerScene::InstanceBase *instances[] = { &icon_instance, &cube_instance };
	const int width = p_context.get_device().get_width(), height = p_context.get_device().get_height();
	const int center = height / 2 * width + width / 2;
	std::vector<uint32_t> pixels(size_t(width) * height), reference;
	// The texture, vertex alpha, and blend operations can each round down on RGB.
	const int blend_tolerance = 3;
	const auto draw = [&](const Transform &p_camera, const CameraMatrix &p_projection, int p_count) {
		return p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK &&
				p_renderer.render(p_camera, p_projection, instances, p_count) == OK && p_targets.end_frame(false) == OK &&
				p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK;
	};
	CameraMatrix perspective, orthogonal, zoomed;
	perspective.set_perspective(60, real_t(width) / height, 0.1, 30);
	orthogonal.set_orthogonal(3, real_t(width) / height, 0.1, 30);
	zoomed.set_orthogonal(6, real_t(width) / height, 0.1, 30);
	const Transform near_camera(Basis(), Vector3(0, 0, 4));
	const Transform far_camera(Basis(), Vector3(0, 0, 8));
	const Transform side_camera(Basis(Vector3(0, 1, 0), Math_PI / 2), Vector3(8, 0, 0));
	FF_BILLBOARD_CHECK(draw(near_camera, perspective, 1));
	const int alpha_sample = int(height * 0.5 * 0.31 * perspective.matrix[1][1] * 0.75);
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
	cube_instance.transform.origin = Vector3(0, 0, 1);
	FF_BILLBOARD_CHECK(draw(near_camera, perspective, 2) && pixels[center] == 0xff00ff00);
	cube_instance.transform.origin.z = -1;
	FF_BILLBOARD_CHECK(draw(near_camera, perspective, 2) && _color_matches(pixels[center], 0xff99ff99, blend_tolerance));
	FF_BILLBOARD_CHECK(meshes.free(icon_mesh) == OK && meshes.free(cube_mesh) == OK && textures.free(texture) == OK);
	FF_BILLBOARD_CHECK(materials.free(icon_material) == OK && materials.free(cube_material) == OK && materials.free(icon_shader) == OK && materials.free(cube_shader) == OK);
	printf("PASS: generated billboard materials, perspective and orthogonal fixed size, texture and vertex alpha, and opaque cube depth.\n");
	return true;
}

#undef FF_BILLBOARD_CHECK
