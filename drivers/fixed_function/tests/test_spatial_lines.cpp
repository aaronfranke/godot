#include "test_spatial_lines.h"

#include "core/config/project_settings.h"
#include "servers/rendering/rendering_server.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

#include <limits>

#define FF_LINE_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Line FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

RenderingServerTypes::RenderInfo TestFixedFunctionSpatialLines::_render_info;
TestFixedFunctionSpatialLines::FFInstance::FFInstance(FixedFunctionRenderContext &p_context, RID p_mesh, RID p_material) : FixedFunctionGeometryInstance(p_context, p_mesh) {
	set_material_override(p_material);
	use_aabb_center = false;
}

bool TestFixedFunctionSpatialLines::_color(uint32_t p_pixel, uint32_t p_expected) {
	for (int shift = 0; shift < 24; shift += 8) {
		if (Math::abs(int((p_pixel >> shift) & 255) - int((p_expected >> shift) & 255)) > 3) {
			return false;
		}
	}
	return true;
}

bool TestFixedFunctionSpatialLines::_math() {
	typedef FixedFunctionSceneRenderer::FFSpatialVertex FFSpatialVertex;
	FFSpatialVertex endpoints[2];
	endpoints[0].position = Vector3(-1, -real_t(1) / 64, -3);
	endpoints[1].position = Vector3(1, -real_t(1) / 64, -3);
	endpoints[0].color = endpoints[1].color = Color(1, 0, 0);
	endpoints[0].uv = Vector2();
	endpoints[1].uv = Vector2(1, 0);
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	FixedFunctionVertex quad[6];
	int vertex_count;
	FF_LINE_CHECK(FixedFunctionSceneRenderer::_line_quad(endpoints, projection, Size2i(128, 96), 3, quad, vertex_count) == OK && vertex_count == 6);
	for (int i = 0; i < 6; i++) {
		const Vector3 ndc = projection.xform(Vector3(quad[i].x, quad[i].y, quad[i].z));
		FF_LINE_CHECK(Math::is_equal_approx(Math::abs((ndc.y + real_t(1) / 96) * 48), real_t(1.5)) && quad[i].color == 0xffff0000);
	}
	// A camera crossing must clip before perspective divide; all generated endpoints remain in front.
	projection.set_perspective(60, real_t(4) / 3, 0.1, 20);
	endpoints[0].position = Vector3(-0.1, 0, 0.5);
	endpoints[1].position = Vector3(0.5, 0, -3);
	FF_LINE_CHECK(FixedFunctionSceneRenderer::_line_quad(endpoints, projection, Size2i(128, 96), 3, quad, vertex_count) == OK && vertex_count == 6);
	for (int i = 0; i < 6; i++) {
		FF_LINE_CHECK(quad[i].z <= -0.0999f && !Math::is_nan(quad[i].x) && !Math::is_inf(quad[i].x) && !Math::is_nan(quad[i].y) && !Math::is_inf(quad[i].y));
	}
	endpoints[1].position.z = 2;
	FF_LINE_CHECK(FixedFunctionSceneRenderer::_line_quad(endpoints, projection, Size2i(128, 96), 3, quad, vertex_count) == OK && vertex_count == 0);
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	endpoints[0].position = Vector3(-1048576, 0, -3);
	endpoints[1].position = Vector3(1048576, 0, -3);
	FF_LINE_CHECK(FixedFunctionSceneRenderer::_line_quad(endpoints, projection, Size2i(128, 96), 3, quad, vertex_count) == OK && vertex_count == 6);
	FF_LINE_CHECK(Math::abs(quad[0].x) <= 2.01 && Math::abs(quad[5].x) <= 2.01 && quad[0].u < quad[5].u);
	// Avoid an absolute inverse-determinant cutoff for a valid large orthographic view.
	projection.set_orthogonal(-1000000, 1000000, -750000, 750000, 0.1, 20);
	FF_LINE_CHECK(FixedFunctionSceneRenderer::_line_quad(endpoints, projection, Size2i(128, 96), 3, quad, vertex_count) == OK && vertex_count == 6);
	const Vector3 a = projection.xform(Vector3(quad[0].x, quad[0].y, quad[0].z));
	const Vector3 b = projection.xform(Vector3(quad[1].x, quad[1].y, quad[1].z));
	FF_LINE_CHECK(Math::abs(Math::abs(a.y - b.y) * 48 - 3) < 0.001);
	endpoints[0].position = endpoints[1].position;
	FF_LINE_CHECK(FixedFunctionSceneRenderer::_line_quad(endpoints, projection, Size2i(128, 96), 3, quad, vertex_count) == OK && vertex_count == 0);
	FF_LINE_CHECK(FixedFunctionSceneRenderer::_line_quad(endpoints, projection, Size2i(), 3, quad, vertex_count) == ERR_INVALID_PARAMETER);
	FF_LINE_CHECK(FixedFunctionSceneRenderer::_line_quad(endpoints, projection, Size2i(128, 96), std::numeric_limits<float>::infinity(), quad, vertex_count) == ERR_INVALID_PARAMETER);
	return true;
}

RID TestFixedFunctionSpatialLines::_mesh(FixedFunctionMeshStorage &p_meshes, RSE::PrimitiveType p_primitive, const Vector<Vector3> &p_positions, const Vector<int> &p_indices) {
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);
	arrays[RSE::ARRAY_VERTEX] = p_positions;
	if (p_indices.size()) {
		arrays[RSE::ARRAY_INDEX] = p_indices;
	}
	RenderingServerTypes::SurfaceData data;
	if (RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&data, p_primitive, arrays) != OK) {
		return RID();
	}
	const RID mesh = p_meshes.mesh_create();
	return p_meshes.mesh_add_surface(mesh, data) == OK ? mesh : RID();
}

bool TestFixedFunctionSpatialLines::_draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RenderGeometryInstance **p_instances, int p_count, const Projection &p_projection, std::vector<uint32_t> &r_pixels) {
	FF_LINE_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK);
	PagedArrayPool<RenderGeometryInstance *> pool;
	PagedArray<RenderGeometryInstance *> list;
	list.set_page_pool(&pool);
	for (int i = 0; i < p_count; i++) {
		list.push_back(p_instances[i]);
	}
	_render_info = RenderingServerTypes::RenderInfo();
	const Error error = p_renderer.render(Transform3D(), p_projection, list, &_render_info);
	if (error != OK) {
		printf("Line render error=%d\n", error);
	}
	FF_LINE_CHECK(error == OK && p_targets.end_frame(false) == OK);
	r_pixels.resize(p_context.get_device().get_width() * p_context.get_device().get_height());
	FF_LINE_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	return true;
}

bool TestFixedFunctionSpatialLines::_depth(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, const Projection &p_projection, std::vector<uint32_t> &r_pixels) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	FF_LINE_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded,cull_disabled; void fragment(){ ALBEDO=vec3(0,1,0); }") == OK);
	Vector<Vector3> positions;
	positions.push_back(Vector3(-1, -1, 0));
	positions.push_back(Vector3(0, 1, 0));
	positions.push_back(Vector3(1, -1, 0));
	const RID triangle = _mesh(meshes, RSE::PRIMITIVE_TRIANGLES, positions);
	positions.resize(0);
	positions.push_back(Vector3(-1, -real_t(1) / 64, 0));
	positions.push_back(Vector3(1, -real_t(1) / 64, 0));
	const RID line = _mesh(meshes, RSE::PRIMITIVE_LINES, positions);
	FF_LINE_CHECK(triangle.is_valid() && line.is_valid());
	FFInstance front(p_context, triangle, material), back(p_context, line, p_material);
	front.transform.origin.z = -2;
	back.transform.origin.z = -4;
	for (int order = 0; order < 2; order++) {
		RenderGeometryInstance *instances[] = { order ? &front : &back, order ? &back : &front };
		FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 2, p_projection, r_pixels));
		FF_LINE_CHECK(_color(r_pixels[48 * 128 + 64], 0xff00ff00) && _color(r_pixels[48 * 128 + 40], 0xffff0000));
	}
	FF_LINE_CHECK(materials.shader_set_code(p_shader, "shader_type spatial; render_mode unshaded,depth_test_disabled; void fragment(){ ALBEDO=vec3(1,0,0); }") == OK);
	RenderGeometryInstance *instances[] = { &front, &back };
	FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 2, p_projection, r_pixels) && _color(r_pixels[48 * 128 + 64], 0xffff0000));
	FF_LINE_CHECK(meshes.free(line) == OK && meshes.free(triangle) == OK && materials.free(material) == OK && materials.free(shader) == OK);
	return true;
}

bool TestFixedFunctionSpatialLines::_attributes(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, const Projection &p_projection, std::vector<uint32_t> &r_pixels) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FF_LINE_CHECK(materials.shader_set_code(p_shader, "shader_type spatial; render_mode unshaded; uniform sampler2D image : filter_nearest, repeat_disable; void fragment(){ ALBEDO=texture(image,UV).rgb*COLOR.rgb; }") == OK);
	Vector<uint8_t> bytes;
	bytes.resize(8);
	{
		uint8_t *write = bytes.ptrw();
		const uint8_t pixels[] = { 255, 0, 0, 255, 0, 255, 0, 255 };
		memcpy(write, pixels, sizeof(pixels));
	}
	Ref<Image> image = Image::create_from_data(2, 1, false, Image::FORMAT_RGBA8, bytes);
	const RID texture = textures.texture_create();
	FF_LINE_CHECK(textures.texture_allocate_2d(texture, Size2i(2, 1), Image::FORMAT_RGBA8, 0, 0) == OK && textures.texture_set_data(texture, image) == OK && materials.material_set_param(p_material, "image", texture) == OK);
	for (int variant = 0; variant < 3; variant++) {
		const bool compressed = variant == 1;
		const bool perspective = variant == 2;
		Projection projection = p_projection;
		if (perspective) {
			projection.set_perspective(60, real_t(4) / 3, 0.1, 20);
		}
		Array arrays;
		arrays.resize(RSE::ARRAY_MAX);
		Vector<Vector3> positions;
		positions.push_back(perspective ? Vector3(-1, -real_t(2) / (96 * projection.columns[1][1]), -2) : Vector3(-1, -real_t(1) / 64, 0));
		positions.push_back(perspective ? Vector3(4, -real_t(8) / (96 * projection.columns[1][1]), -8) : Vector3(1, -real_t(1) / 64, 0));
		Vector<Vector2> uvs;
		uvs.push_back(Vector2(0, 0));
		uvs.push_back(Vector2(1, 0));
		Vector<Color> colors;
		colors.push_back(Color(0.5, 0.5, 1));
		colors.push_back(Color(0.5, 0.5, 1));
		arrays[RSE::ARRAY_VERTEX] = positions;
		arrays[RSE::ARRAY_TEX_UV] = uvs;
		arrays[RSE::ARRAY_COLOR] = colors;
		RenderingServerTypes::SurfaceData surface;
		FF_LINE_CHECK(RenderingServer::get_singleton()->mesh_create_surface_data_from_arrays(&surface, RSE::PRIMITIVE_LINES, arrays, Array(), Dictionary(), compressed ? uint64_t(RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES) : 0) == OK);
		const RID mesh = p_context.get_mesh_storage().mesh_create();
		FF_LINE_CHECK(p_context.get_mesh_storage().mesh_add_surface(mesh, surface) == OK);
		FFInstance instance(p_context, mesh, p_material);
		instance.transform.origin.z = perspective ? 0 : -3;
		RenderGeometryInstance *instances[] = { &instance };
		FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, r_pixels));
		FF_LINE_CHECK(_color(r_pixels[48 * 128 + 40], 0xff800000) && _color(r_pixels[48 * 128 + (perspective ? 96 : 88)], 0xff008000));
		if (perspective) {
			FF_LINE_CHECK(_color(r_pixels[48 * 128 + 64], 0xff800000)); // Perspective-correct u=0.2, not affine u=0.5.
		}
		FF_LINE_CHECK(p_context.get_mesh_storage().free(mesh) == OK);
	}
	FF_LINE_CHECK(textures.free(texture) == OK);
	return true;
}

bool TestFixedFunctionSpatialLines::_aliased(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	FF_LINE_CHECK(_math());
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	FF_LINE_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded,cull_front; void fragment(){ ALBEDO=vec3(1,0,0); }") == OK);
	Vector<Vector3> positions;
	positions.push_back(Vector3(-1, -real_t(1) / 64, 0));
	positions.push_back(Vector3(1, -real_t(1) / 64, 0));
	const RID mesh = _mesh(meshes, RSE::PRIMITIVE_LINES, positions);
	FF_LINE_CHECK(mesh.is_valid());
	FFInstance instance(p_context, mesh, material);
	instance.transform.origin.z = -3;
	RenderGeometryInstance *instances[] = { &instance };
	Projection projection;
	projection.set_orthogonal(-2, 2, -1.5, 1.5, 0.1, 20);
	std::vector<uint32_t> pixels;
	for (int width = 1; width <= 7; width += 2) {
		FF_LINE_CHECK(materials.material_set_line_width(material, width) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels));
		int rows = 0;
		for (int y = 0; y < 96; y++) {
			rows += _color(pixels[y * 128 + 64], 0xffff0000);
		}
		FF_LINE_CHECK(rows == width && _color(pixels[48 * 128 + 40], 0xffff0000) && _color(pixels[48 * 128 + 20], 0xff0000ff));
	}
	FF_LINE_CHECK(materials.material_set_line_width(material, 3) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels));
	const std::vector<uint32_t> original = pixels;
	FF_LINE_CHECK(materials.material_set_line_width(material, FLT_MAX) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels));
	FF_LINE_CHECK(materials.material_get_line_width(material) == FLT_MAX && _color(pixels[0 * 128 + 64], 0xffff0000) && _color(pixels[95 * 128 + 64], 0xffff0000));
	FF_LINE_CHECK(materials.material_set_line_width(material, 3) == OK);
	instance.transform.basis.scale(Vector3(-1, 1, 1));
	FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && pixels == original);
	instance.transform.basis = Basis();
	instance.transform.basis.scale(Vector3(0.5, 4, 1));
	FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels));
	int scaled_rows = 0;
	for (int y = 0; y < 96; y++) {
		scaled_rows += _color(pixels[y * 128 + 64], 0xffff0000);
	}
	FF_LINE_CHECK(scaled_rows == 3);
	instance.transform.basis = Basis();
	FixedFunctionGraphicsDevice::FFCapabilities &caps = const_cast<FixedFunctionGraphicsDevice::FFCapabilities &>(p_context.get_device().get_capabilities());
	const uint32_t old_limit = caps.max_primitives_per_draw;
	caps.max_primitives_per_draw = 1;
	const bool drawn = _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels);
	caps.max_primitives_per_draw = old_limit;
	FF_LINE_CHECK(drawn && pixels == original && _render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] == 2);
	FF_LINE_CHECK(_render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 2 && _render_info.info[RSE::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] == 1);
	FF_LINE_CHECK(p_context.resize(160, 120) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels));
	int rows = 0;
	for (int y = 0; y < 120; y++) {
		rows += _color(pixels[y * 160 + 80], 0xffff0000);
	}
	FF_LINE_CHECK(rows == 3 && p_context.resize(128, 96) == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && pixels == original);
	// Strips share vertices, and loops additionally close their last segment.
	positions.resize(0);
	positions.push_back(Vector3(-1, -real_t(33) / 64, 0));
	positions.push_back(Vector3(real_t(1) / 64, real_t(31) / 64, 0));
	positions.push_back(Vector3(1, -real_t(33) / 64, 0));
	Vector<int> indices;
	indices.push_back(0);
	indices.push_back(1);
	indices.push_back(2);
	for (int loop = 0; loop < 2; loop++) {
		if (loop) {
			indices.push_back(0);
		}
		const RID topology = _mesh(meshes, RSE::PRIMITIVE_LINE_STRIP, positions, indices);
		FF_LINE_CHECK(topology.is_valid());
		instance.data->base = topology;
		FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && _color(pixels[64 * 128 + 64], loop ? 0xffff0000 : 0xff0000ff));
		FF_LINE_CHECK(meshes.free(topology) == OK);
	}
	instance.data->base = mesh;
	// Missing alpha testing lowers a base cutout to blending instead of discarding the viewport.
	FF_LINE_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=1.0; ALPHA_SCISSOR_THRESHOLD=0.5; }") == OK);
	const bool old_alpha_cutoff = caps.alpha_cutoff;
	caps.alpha_cutoff = false;
	const bool fallback_drawn = _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels);
	caps.alpha_cutoff = old_alpha_cutoff;
	FF_LINE_CHECK(fallback_drawn && pixels.size() == original.size());
	for (size_t i = 0; i < pixels.size(); i++) {
		// The alpha-blended fallback can round a fully opaque line's color.
		FF_LINE_CHECK(_color(pixels[i], original[i]) && (pixels[i] >> 24) == (original[i] >> 24));
	}
	// A native depth refusal must still propagate through the shared renderer and recover.
	const bool old_depth_buffer = caps.depth_buffer;
	caps.depth_buffer = false;
	PagedArrayPool<RenderGeometryInstance *> failure_pool;
	PagedArray<RenderGeometryInstance *> failure_instances;
	failure_instances.set_page_pool(&failure_pool);
	failure_instances.push_back(&instance);
	const Error begin_error = p_targets.begin_frame(0);
	const Error draw_error = begin_error == OK ? p_renderer.render(Transform3D(), projection, failure_instances) : begin_error;
	const Error end_error = p_targets.end_frame(false);
	caps.depth_buffer = old_depth_buffer;
	FF_LINE_CHECK(begin_error == OK && draw_error == ERR_UNAVAILABLE && end_error == ERR_UNAVAILABLE);
	FF_LINE_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded,cull_front; void fragment(){ ALBEDO=vec3(1,0,0); }") == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && pixels == original);
	instance.transform.origin.x = 10;
	FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels));
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_LINE_CHECK(_color(pixels[i], 0xff0000ff));
	}
	FF_LINE_CHECK(_depth(p_context, p_targets, p_renderer, shader, material, projection, pixels));
	positions.resize(65538);
	{
		Vector3 *write = positions.ptrw();
		for (int i = 0; i < positions.size(); i++) {
			write[i] = Vector3();
		}
		write[65536] = Vector3(-1, -real_t(1) / 64, 0);
		write[65537] = Vector3(1, -real_t(1) / 64, 0);
	}
	indices.resize(0);
	indices.push_back(65536);
	indices.push_back(65537);
	const RID large = _mesh(meshes, RSE::PRIMITIVE_LINES, positions, indices);
	FF_LINE_CHECK(large.is_valid());
	FFInstance large_instance(p_context, large, material);
	large_instance.transform.origin.z = -3;
	RenderGeometryInstance *large_instances[] = { &large_instance };
	FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, large_instances, 1, projection, pixels) && pixels == original);
	FF_LINE_CHECK(meshes.free(large) == OK);
	FF_LINE_CHECK(_attributes(p_context, p_targets, p_renderer, shader, material, projection, pixels));
	FF_LINE_CHECK(meshes.free(mesh) == OK && materials.free(material) == OK && materials.free(shader) == OK);
	printf("PASS: spatial line lists/strips/loops, homogeneous clipping, constant pixel widths, mirror/cull behavior, depth, attributes, bounded batches and recovery.\n");
	return true;
}

bool TestFixedFunctionSpatialLines::test(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer) {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const bool requested = GLOBAL_GET("rendering/quality/fixed_function/antialiased_lines");
	settings->set("rendering/quality/fixed_function/antialiased_lines", false);
	const bool passed = _aliased(p_context, p_targets, p_renderer) && _quality(p_context, p_targets, p_renderer);
	settings->set("rendering/quality/fixed_function/antialiased_lines", requested);
	return passed;
}

#undef FF_LINE_CHECK
