#include "test_spatial_lines.h"

#include "core/project_settings.h"

#include <float.h>
#include <stdio.h>
#include <string.h>
#include <limits>

#define FF_LINE_CHECK(m_condition)                                   \
	do {                                                             \
		if (!(m_condition)) {                                        \
			printf("Line FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                            \
		}                                                            \
	} while (false)

TestFixedFunctionSpatialLines::FFInstance::FFInstance(RID p_mesh, RID p_material) {
	base_type = VS::INSTANCE_MESH;
	base = p_mesh;
	material_override = p_material;
	depth = 0;
}

bool TestFixedFunctionSpatialLines::_color(uint32_t p_pixel, uint32_t p_expected) {
	for (int shift = 0; shift < 24; shift += 8) {
		if (ABS(int((p_pixel >> shift) & 255) - int((p_expected >> shift) & 255)) > 3) {
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
	CameraMatrix projection;
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

RID TestFixedFunctionSpatialLines::_mesh(FixedFunctionMeshStorage &p_meshes, VS::PrimitiveType p_primitive, const PoolVector<Vector3> &p_positions, const PoolVector<int> &p_indices) {
	Array arrays;
	arrays.resize(VS::ARRAY_MAX);
	arrays[VS::ARRAY_VERTEX] = p_positions;
	if (p_indices.size()) {
		arrays[VS::ARRAY_INDEX] = p_indices;
	}
	const RID source = VS::get_singleton()->mesh_create();
	VS::get_singleton()->mesh_add_surface_from_arrays(source, p_primitive, arrays, Array(), 0);
	const RID mesh = p_meshes.mesh_create();
	const Error error = p_meshes.mesh_add_surface(mesh, VS::get_singleton()->mesh_surface_get_format(source, 0), p_primitive, VS::get_singleton()->mesh_surface_get_array(source, 0), p_positions.size(), VS::get_singleton()->mesh_surface_get_index_array(source, 0), p_indices.size(), VS::get_singleton()->mesh_surface_get_aabb(source, 0), Vector<PoolVector<uint8_t>>(), Vector<AABB>());
	VS::get_singleton()->free(source);
	return error == OK ? mesh : RID();
}

bool TestFixedFunctionSpatialLines::_draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RasterizerScene::InstanceBase **p_instances, int p_count, const CameraMatrix &p_projection, std::vector<uint32_t> &r_pixels) {
	FF_LINE_CHECK(p_targets.begin_frame(0) == OK && p_targets.clear_render_target(Color(0, 0, 1)) == OK && p_context.get_storage_queries().render_info_begin_capture() == OK);
	const Error error = p_renderer.render(Transform(), p_projection, p_instances, p_count);
	if (error != OK) {
		printf("Line render error=%d\n", error);
	}
	FF_LINE_CHECK(error == OK && p_targets.end_frame(false) == OK && p_context.get_storage_queries().render_info_end_capture() == OK);
	r_pixels.resize(p_context.get_device().get_width() * p_context.get_device().get_height());
	FF_LINE_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	return true;
}

bool TestFixedFunctionSpatialLines::_depth(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, const CameraMatrix &p_projection, std::vector<uint32_t> &r_pixels) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionMeshStorage &meshes = p_context.get_mesh_storage();
	const RID shader = materials.shader_create();
	const RID material = materials.material_create();
	FF_LINE_CHECK(materials.material_set_shader(material, shader) == OK && materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded,cull_disabled; void fragment(){ ALBEDO=vec3(0,1,0); }") == OK);
	PoolVector<Vector3> positions;
	positions.push_back(Vector3(-1, -1, 0));
	positions.push_back(Vector3(0, 1, 0));
	positions.push_back(Vector3(1, -1, 0));
	const RID triangle = _mesh(meshes, VS::PRIMITIVE_TRIANGLES, positions);
	positions.resize(0);
	positions.push_back(Vector3(-1, -real_t(1) / 64, 0));
	positions.push_back(Vector3(1, -real_t(1) / 64, 0));
	const RID line = _mesh(meshes, VS::PRIMITIVE_LINES, positions);
	FF_LINE_CHECK(triangle.is_valid() && line.is_valid());
	FFInstance front(triangle, material), back(line, p_material);
	front.transform.origin.z = -2;
	back.transform.origin.z = -4;
	for (int order = 0; order < 2; order++) {
		RasterizerScene::InstanceBase *instances[] = { order ? &front : &back, order ? &back : &front };
		FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 2, p_projection, r_pixels));
		FF_LINE_CHECK(_color(r_pixels[48 * 128 + 64], 0xff00ff00) && _color(r_pixels[48 * 128 + 40], 0xffff0000));
	}
	FF_LINE_CHECK(materials.shader_set_code(p_shader, "shader_type spatial; render_mode unshaded,depth_test_disable; void fragment(){ ALBEDO=vec3(1,0,0); }") == OK);
	RasterizerScene::InstanceBase *instances[] = { &front, &back };
	FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 2, p_projection, r_pixels) && _color(r_pixels[48 * 128 + 64], 0xffff0000));
	FF_LINE_CHECK(meshes.free(line) == OK && meshes.free(triangle) == OK && materials.free(material) == OK && materials.free(shader) == OK);
	return true;
}

bool TestFixedFunctionSpatialLines::_attributes(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_shader, RID p_material, const CameraMatrix &p_projection, std::vector<uint32_t> &r_pixels) {
	FixedFunctionMaterialStorage &materials = p_context.get_material_storage();
	FixedFunctionTextureStorage &textures = p_context.get_texture_storage();
	FF_LINE_CHECK(materials.shader_set_code(p_shader, "shader_type spatial; render_mode unshaded; uniform sampler2D image; void fragment(){ ALBEDO=texture(image,UV).rgb*COLOR.rgb; }") == OK);
	PoolVector<uint8_t> bytes;
	bytes.resize(8);
	{
		PoolVector<uint8_t>::Write write = bytes.write();
		const uint8_t pixels[] = { 255, 0, 0, 255, 0, 255, 0, 255 };
		memcpy(write.ptr(), pixels, sizeof(pixels));
	}
	Ref<Image> image = memnew(Image(2, 1, false, Image::FORMAT_RGBA8, bytes));
	const RID texture = textures.texture_create();
	FF_LINE_CHECK(textures.texture_allocate_2d(texture, Size2i(2, 1), Image::FORMAT_RGBA8, 0, 0) == OK && textures.texture_set_data(texture, image) == OK && materials.material_set_param(p_material, "image", texture) == OK);
	for (int variant = 0; variant < 3; variant++) {
		const bool compressed = variant == 1;
		const bool perspective = variant == 2;
		CameraMatrix projection = p_projection;
		if (perspective) {
			projection.set_perspective(60, real_t(4) / 3, 0.1, 20);
		}
		Array arrays;
		arrays.resize(VS::ARRAY_MAX);
		PoolVector<Vector3> positions;
		positions.push_back(perspective ? Vector3(-1, -real_t(2) / (96 * projection.matrix[1][1]), -2) : Vector3(-1, -real_t(1) / 64, 0));
		positions.push_back(perspective ? Vector3(4, -real_t(8) / (96 * projection.matrix[1][1]), -8) : Vector3(1, -real_t(1) / 64, 0));
		PoolVector<Vector2> uvs;
		uvs.push_back(Vector2(0, 0));
		uvs.push_back(Vector2(1, 0));
		PoolVector<Color> colors;
		colors.push_back(Color(0.5, 0.5, 1));
		colors.push_back(Color(0.5, 0.5, 1));
		arrays[VS::ARRAY_VERTEX] = positions;
		arrays[VS::ARRAY_TEX_UV] = uvs;
		arrays[VS::ARRAY_COLOR] = colors;
		const RID source = VS::get_singleton()->mesh_create();
		VS::get_singleton()->mesh_add_surface_from_arrays(source, VS::PRIMITIVE_LINES, arrays, Array(), compressed ? uint32_t(VS::ARRAY_COMPRESS_DEFAULT) : 0);
		const RID mesh = p_context.get_mesh_storage().mesh_create();
		FF_LINE_CHECK(p_context.get_mesh_storage().mesh_add_surface(mesh, VS::get_singleton()->mesh_surface_get_format(source, 0), VS::PRIMITIVE_LINES, VS::get_singleton()->mesh_surface_get_array(source, 0), 2, PoolVector<uint8_t>(), 0, VS::get_singleton()->mesh_surface_get_aabb(source, 0), Vector<PoolVector<uint8_t>>(), Vector<AABB>()) == OK);
		VS::get_singleton()->free(source);
		FFInstance instance(mesh, p_material);
		instance.transform.origin.z = perspective ? 0 : -3;
		RasterizerScene::InstanceBase *instances[] = { &instance };
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
	PoolVector<Vector3> positions;
	positions.push_back(Vector3(-1, -real_t(1) / 64, 0));
	positions.push_back(Vector3(1, -real_t(1) / 64, 0));
	const RID mesh = _mesh(meshes, VS::PRIMITIVE_LINES, positions);
	FF_LINE_CHECK(mesh.is_valid());
	FFInstance instance(mesh, material);
	instance.transform.origin.z = -3;
	RasterizerScene::InstanceBase *instances[] = { &instance };
	CameraMatrix projection;
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
	int draws, vertices, objects;
	FF_LINE_CHECK(drawn && pixels == original && p_context.get_storage_queries().get_captured_render_info(VS::INFO_DRAW_CALLS_IN_FRAME, draws) == OK && draws == 2);
	FF_LINE_CHECK(p_context.get_storage_queries().get_captured_render_info(VS::INFO_VERTICES_IN_FRAME, vertices) == OK && vertices == 6 && p_context.get_storage_queries().get_captured_render_info(VS::INFO_OBJECTS_IN_FRAME, objects) == OK && objects == 1);
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
	PoolVector<int> indices;
	indices.push_back(0);
	indices.push_back(1);
	indices.push_back(2);
	for (int loop = 0; loop < 2; loop++) {
		const RID topology = _mesh(meshes, loop ? VS::PRIMITIVE_LINE_LOOP : VS::PRIMITIVE_LINE_STRIP, positions, indices);
		FF_LINE_CHECK(topology.is_valid());
		instance.base = topology;
		FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && _color(pixels[64 * 128 + 64], loop ? 0xffff0000 : 0xff0000ff));
		FF_LINE_CHECK(meshes.free(topology) == OK);
	}
	instance.base = mesh;
	// Missing alpha testing uses a visible base fallback, including on offscreen-capable devices.
	FF_LINE_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded; void fragment(){ ALBEDO=vec3(1,0,0); ALPHA=0.5; ALPHA_SCISSOR=0.75; }") == OK);
	const bool old_alpha_cutoff = caps.alpha_cutoff;
	caps.alpha_cutoff = false;
	Error begin_error = p_targets.begin_frame(0);
	if (begin_error == OK) {
		begin_error = p_targets.clear_render_target(Color(0, 0, 1));
	}
	const Error draw_error = begin_error == OK ? p_renderer.render(Transform(), projection, instances, 1) : begin_error;
	const Error end_error = p_targets.end_frame(false);
	caps.alpha_cutoff = old_alpha_cutoff;
	FF_LINE_CHECK(begin_error == OK && draw_error == OK && end_error == OK);
	FF_LINE_CHECK(p_context.get_device().read_frame(pixels.data(), pixels.size()) == OK && _color(pixels[48 * 128 + 64], 0xff800080));
	const bool old_blend_mix = caps.blend_mix;
	caps.alpha_cutoff = caps.blend_mix = false;
	const bool opaque_fallback = _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels);
	caps.alpha_cutoff = old_alpha_cutoff;
	caps.blend_mix = old_blend_mix;
	FF_LINE_CHECK(opaque_fallback && pixels.size() == original.size());
	for (size_t i = 0; i < pixels.size(); i++) {
		// The alpha-blended fallback can round a fully opaque line's color.
		FF_LINE_CHECK(_color(pixels[i], original[i]) && (pixels[i] >> 24) == (original[i] >> 24));
	}
	// A real native depth refusal must still cancel the frame and recover afterward.
	const bool old_depth = caps.depth_buffer;
	caps.depth_buffer = false;
	const bool depth_refused = p_targets.begin_frame(0) == OK && p_renderer.render(Transform(), projection, instances, 1) == ERR_UNAVAILABLE;
	const Error depth_end = p_targets.end_frame(false);
	caps.depth_buffer = old_depth;
	FF_LINE_CHECK(depth_refused && depth_end == ERR_UNAVAILABLE);
	FF_LINE_CHECK(materials.shader_set_code(shader, "shader_type spatial; render_mode unshaded,cull_front; void fragment(){ ALBEDO=vec3(1,0,0); }") == OK && _draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels) && pixels == original);
	instance.transform.origin.x = 10;
	FF_LINE_CHECK(_draw(p_context, p_targets, p_renderer, instances, 1, projection, pixels));
	for (size_t i = 0; i < pixels.size(); i++) {
		FF_LINE_CHECK(_color(pixels[i], 0xff0000ff));
	}
	FF_LINE_CHECK(_depth(p_context, p_targets, p_renderer, shader, material, projection, pixels));
	positions.resize(65538);
	{
		PoolVector<Vector3>::Write write = positions.write();
		for (int i = 0; i < positions.size(); i++) {
			write[i] = Vector3();
		}
		write[65536] = Vector3(-1, -real_t(1) / 64, 0);
		write[65537] = Vector3(1, -real_t(1) / 64, 0);
	}
	indices.resize(0);
	indices.push_back(65536);
	indices.push_back(65537);
	const RID large = _mesh(meshes, VS::PRIMITIVE_LINES, positions, indices);
	FF_LINE_CHECK(large.is_valid());
	FFInstance large_instance(large, material);
	large_instance.transform.origin.z = -3;
	RasterizerScene::InstanceBase *large_instances[] = { &large_instance };
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
