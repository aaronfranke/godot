#include "drivers/fixed_function/tests/test_preset_scope.h"
#include "drivers/fixed_function/tests/test_retained_scene.h"
#include "scene/resources/material.h"
#include "servers/rendering/rendering_server.h"

#include <stdio.h>

#define FF_POINT_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Spatial points FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

bool TestFixedFunctionRetainedScene::_test_points(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	TestFixedFunctionPresetScope scope(p_context.get_device(), nullptr);
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	const RID shader = materials.shader_create(), material = materials.material_create(), texture = textures.texture_create();
	Ref<StandardMaterial3D> preset;
	preset.instantiate();
	preset->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	preset->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
	preset->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
	preset->set_flag(BaseMaterial3D::FLAG_USE_POINT_SIZE, true);
	preset->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	preset->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	preset->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
	preset->set_on_top_of_alpha();
	preset->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	BaseMaterial3D::flush_changes();
	FF_POINT_CHECK(materials.shader_set_code(shader, RenderingServer::get_singleton()->shader_get_code(preset->get_shader_rid())) == OK);
	FF_POINT_CHECK(materials.material_set_shader(material, shader) == OK);
	FF_POINT_CHECK(materials.material_set_param(material, "albedo", Color(1, 1, 1)) == OK);
	FF_POINT_CHECK(materials.material_set_param(material, "texture_albedo", texture) == OK);
	FF_POINT_CHECK(materials.material_set_param(material, "point_size", 16.0) == OK);
	FixedFunctionSpatialMaterial::FFMaterialParameters parameters;
	FF_POINT_CHECK(materials.material_get_spatial_parameters(material, parameters) == OK && parameters.point_size == 16 && parameters.albedo.uv_set == FixedFunctionSpatialMaterial::UV_POINT_COORD);
	FF_POINT_CHECK(parameters.albedo_uses_vertex_color && parameters.albedo_uses_vertex_alpha && !parameters.state.depth_test_enabled && !parameters.state.depth_write_enabled);

	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	const Color quadrants[] = { Color(1, 0, 0), Color(0, 0, 0, 0), Color(0, 1, 0, 0.5), Color(1, 1, 1) };
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			image->set_pixel(x, y, quadrants[(y / 2) * 2 + x / 2]);
		}
	}
	FF_POINT_CHECK(textures.texture_set_data(texture, image) == OK);
	const RID mesh = meshes.mesh_create(), indexed = meshes.mesh_create();
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	arrays[RSE::ARRAY_VERTEX] = Vector<Vector3>{ Vector3(), Vector3(1, 0, 0) };
	arrays[RSE::ARRAY_COLOR] = Vector<Color>{ Color(1, 0.5, 1, 0.8), Color(1, 1, 1, 1) };
	RenderingServerTypes::SurfaceData surface;
	RenderingServer *server = RenderingServer::get_singleton();
	FF_POINT_CHECK(server->mesh_create_surface_data_from_arrays(&surface, RSE::PRIMITIVE_POINTS, arrays) == OK && meshes.mesh_add_surface(mesh, surface) == OK);
	arrays[RSE::ARRAY_INDEX] = Vector<int>{ 0 };
	FF_POINT_CHECK(server->mesh_create_surface_data_from_arrays(&surface, RSE::PRIMITIVE_POINTS, arrays) == OK && meshes.mesh_add_surface(indexed, surface) == OK);

	FFInstance instance(p_context, indexed, material);
	FFInstance *instances[] = { &instance };
	const int width = p_context.get_device().get_width(), height = p_context.get_device().get_height();
	const int center = height / 2 * width + width / 2;
	const int second = center + width / 4;
	const uint32_t blue = 0xff0000ff;
	std::vector<uint32_t> pixels;
	// Allow RGB software rounding for pixels that went through texture and blend stages.
	const int blend_tolerance = 3;
	FF_POINT_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels));
	FF_POINT_CHECK(_color_matches(pixels[center - 4 * width - 4], 0xffcc0033, blend_tolerance) && _color_matches(pixels[center - 4 * width + 4], blue, 1));
	const uint32_t translucent = pixels[center + 4 * width - 4];
	FF_POINT_CHECK(Math::abs(int((translucent >> 8) & 255) - 51) <= 2 && Math::abs(int(translucent & 255) - 153) <= 2);
	FF_POINT_CHECK(pixels[second + 4 * width + 4] == blue);
	instance.data->base = mesh;
	instance._mark_dirty();
	FF_POINT_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels) && _color_matches(pixels[second + 4 * width + 4], 0xffffffff, blend_tolerance));
	instance.data->base = indexed;
	instance._mark_dirty();
	FF_POINT_CHECK(materials.material_set_param(material, "point_size", 8.0) == OK);
	FF_POINT_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, pixels));
	FF_POINT_CHECK(_color_matches(pixels[center - 2 * width - 2], 0xffcc0033, blend_tolerance) && pixels[center - 6 * width - 6] == blue);

	Projection projection;
	projection.set_perspective(60, real_t(width) / height, 0.1, 30);
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> draw_instances;
	draw_instances.set_page_pool(&pool);
	draw_instances.push_back(&instance);
	const auto draw_perspective = [&]() {
		return p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK &&
				p_renderer.render(Transform3D(), projection, draw_instances, nullptr) == OK && p_targets.end_frame(false) == OK &&
				p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK;
	};
	FF_POINT_CHECK(draw_perspective() && _color_matches(pixels[center - 2 * width - 2], 0xffcc0033, blend_tolerance));
	const std::vector<uint32_t> near_pixels = pixels;
	instance.set_transform(Transform3D(Basis(), Vector3(0, 0, -9)), AABB(), AABB());
	FF_POINT_CHECK(draw_perspective() && pixels == near_pixels);
	instance.set_transform(Transform3D(Basis(), Vector3(0, 0, 1)), AABB(), AABB());
	FF_POINT_CHECK(draw_perspective() && pixels[center - 2 * width - 2] == blue);
	FF_POINT_CHECK(meshes.free(mesh) == OK && meshes.free(indexed) == OK && textures.free(texture) == OK);
	FF_POINT_CHECK(materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: 3D points and gizmo handle materials, indexed draws, point size, oriented texture coordinates, alpha and projection clipping.\n");
	return true;
}

#undef FF_POINT_CHECK
