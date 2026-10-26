#include "test_storage_queries.h"

#include "drivers/fixed_function/rasterizer_storage_fixed_function.h"

#include <stdio.h>
#include <type_traits>

#define FF_QUERY_CHECK(m_condition)                                             \
	do {                                                                        \
		if (!(m_condition)) {                                                   \
			printf("Storage queries FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                       \
		}                                                                       \
	} while (false)

static_assert(!std::is_abstract<RasterizerStorageFixedFunction>::value, "The restricted storage boundary must be concrete.");

void TestFixedFunctionStorageQueries::_detect(void *p_userdata) {
	(*static_cast<int *>(p_userdata))++;
}

Ref<Image> TestFixedFunctionStorageQueries::_image(int p_size) {
	PoolVector<uint8_t> bytes;
	bytes.resize(p_size * p_size * 3);
	{
		PoolVector<uint8_t>::Write write = bytes.write();
		for (int i = 0; i < bytes.size(); i++) {
			write[i] = 255;
		}
	}
	return memnew(Image(p_size, p_size, false, Image::FORMAT_RGB8, bytes));
}

bool TestFixedFunctionStorageQueries::_test_callbacks(FixedFunctionTextureStorage &p_storage, RID p_texture, int &r_calls) {
	for (int i = 0; i < FixedFunctionTextureStorage::DETECT_MAX; i++) {
		const FixedFunctionTextureStorage::FFTextureDetection detection = FixedFunctionTextureStorage::FFTextureDetection(i);
		FixedFunctionTextureStorage::FFTextureDetectCallback callback = _detect;
		void *userdata = &r_calls;
		FF_QUERY_CHECK(p_storage.texture_get_detect_callback(p_texture, detection, callback, userdata) == OK && !callback && !userdata);
		FF_QUERY_CHECK(p_storage.texture_set_detect_callback(p_texture, detection, _detect, &r_calls) == OK);
		FF_QUERY_CHECK(p_storage.texture_get_detect_callback(p_texture, detection, callback, userdata) == OK && callback == _detect && userdata == &r_calls);
	}
	FixedFunctionTextureStorage::FFTextureDetectCallback callback = _detect;
	void *userdata = &r_calls;
	FF_QUERY_CHECK(p_storage.texture_get_detect_callback(RID(), FixedFunctionTextureStorage::DETECT_3D, callback, userdata) == ERR_INVALID_PARAMETER && callback == _detect && userdata == &r_calls);
	FF_QUERY_CHECK(p_storage.texture_set_detect_callback(p_texture, FixedFunctionTextureStorage::DETECT_MAX, _detect, &r_calls) == ERR_INVALID_PARAMETER);
	FF_QUERY_CHECK(p_storage.texture_set_detect_callback(p_texture, FixedFunctionTextureStorage::DETECT_NORMAL, nullptr, &r_calls) == OK);
	FF_QUERY_CHECK(p_storage.texture_get_detect_callback(p_texture, FixedFunctionTextureStorage::DETECT_NORMAL, callback, userdata) == OK && !callback && !userdata && r_calls == 0);
	return true;
}

bool TestFixedFunctionStorageQueries::_find_usage(const FixedFunctionTextureStorage &p_storage, RID p_texture, FixedFunctionTextureStorage::FFTextureUsage &r_info) {
	List<FixedFunctionTextureStorage::FFTextureUsage> usage;
	if (p_storage.texture_debug_usage(&usage) != OK) {
		return false;
	}
	for (const List<FixedFunctionTextureStorage::FFTextureUsage>::Element *E = usage.front(); E; E = E->next()) {
		if (E->get().texture == p_texture) {
			r_info = E->get();
			return true;
		}
	}
	return false;
}

bool TestFixedFunctionStorageQueries::_render(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RID p_texture, bool p_previous_frame, std::vector<uint32_t> &r_pixels) {
	FixedFunctionStorageQueries &queries = p_context.get_storage_queries();
	FF_QUERY_CHECK(p_manager.begin_frame(0) == OK);
	uint64_t value = UINT64_MAX;
	FF_QUERY_CHECK(queries.get_render_info(VS::INFO_2D_ITEMS_IN_FRAME, value) == OK && value == (p_previous_frame ? 2 : 0));
	FF_QUERY_CHECK(queries.get_render_info(VS::INFO_2D_DRAW_CALLS_IN_FRAME, value) == OK && value == (p_previous_frame ? 2 : 0));
	RasterizerCanvas::Item first, second;
	for (int i = 0; i < 2; i++) {
		RasterizerCanvas::Item::CommandRect *rect = memnew(RasterizerCanvas::Item::CommandRect);
		rect->rect = Rect2(0, i * 48, 64, 48);
		rect->texture = p_texture;
		rect->modulate = i ? Color(0, 1, 0) : Color(1, 0, 0);
		first.commands.push_back(rect);
	}
	RasterizerCanvas::Item::CommandRect *rect = memnew(RasterizerCanvas::Item::CommandRect);
	rect->rect = Rect2(64, 0, 64, 96);
	rect->modulate = Color(0.25, 0.5, 0.75);
	second.commands.push_back(rect);
	FixedFunctionCanvasItemRenderer renderer(p_context, p_manager);
	FF_QUERY_CHECK(queries.render_info_begin_capture() == OK && queries.render_info_begin_capture() == ERR_BUSY);
	int captured = -1;
	FF_QUERY_CHECK(queries.get_captured_render_info(VS::INFO_2D_ITEMS_IN_FRAME, captured) == ERR_BUSY && captured == -1);
	FF_QUERY_CHECK(renderer.render_items(&first, Color(1, 1, 1)) == OK && queries.render_info_end_capture() == OK);
	FF_QUERY_CHECK(queries.get_captured_render_info(VS::INFO_2D_ITEMS_IN_FRAME, captured) == OK && captured == 1);
	// The adjacent textured rectangles share one draw while keeping their individual tints.
	FF_QUERY_CHECK(queries.get_captured_render_info(VS::INFO_2D_DRAW_CALLS_IN_FRAME, captured) == OK && captured == 1);
	FF_QUERY_CHECK(queries.render_info_end_capture() == ERR_UNCONFIGURED);
	FF_QUERY_CHECK(renderer.render_items(&second, Color(1, 1, 1)) == OK);
	FF_QUERY_CHECK(queries.get_captured_render_info(VS::INFO_2D_DRAW_CALLS_IN_FRAME, captured) == OK && captured == 1);
	for (int i = VS::INFO_OBJECTS_IN_FRAME; i <= VS::INFO_DRAW_CALLS_IN_FRAME; i++) {
		FF_QUERY_CHECK(queries.get_captured_render_info(VS::RenderInfo(i), captured) == OK && captured == 0);
	}
	FF_QUERY_CHECK(p_manager.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FF_QUERY_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			uint32_t expected = 0xff4080bf;
			if (x < 64) {
				expected = y >= 48 ? 0xff00ff00 : 0xffff0000;
			}
			const uint32_t actual = r_pixels[size_t(y) * CAPTURE_WIDTH + x];
			FF_QUERY_CHECK((actual >> 24) == 255);
			for (int shift = 0; shift < 24; shift += 8) {
				const int delta = int((actual >> shift) & 255) - int((expected >> shift) & 255);
				FF_QUERY_CHECK(delta >= -8 && delta <= 8);
			}
		}
	}
	return true;
}

bool TestFixedFunctionStorageQueries::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	RasterizerStorage *const singleton = RasterizerStorage::base_singleton;
	FF_QUERY_CHECK(p_device.get_texture_memory_usage() == 0 && p_device.get_adapter_name() == "Unknown" && p_device.get_adapter_vendor_id() == 0);
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FixedFunctionRenderTargetManager manager(context);
	FF_QUERY_CHECK(context.initialize() == OK);
	FixedFunctionStorageQueries &queries = context.get_storage_queries();
	FixedFunctionTextureStorage &textures = context.get_texture_storage();
	const String adapter = queries.get_video_adapter_name();
	const String vendor = queries.get_video_adapter_vendor();
	FF_QUERY_CHECK(!adapter.empty() && !vendor.empty());
	printf("Adapter: %s; %s\n", adapter.utf8().get_data(), vendor.utf8().get_data());
	const RID texture = textures.texture_create();
	const RID proxy = textures.texture_create();
	const RID target = textures.render_target_create();
	const RID target_texture = textures.render_target_get_texture(target);
	const RID shader = context.get_material_storage().shader_create();
	const RID material = context.get_material_storage().material_create();
	const RID mesh = context.get_mesh_storage().mesh_create();
	FF_QUERY_CHECK(queries.get_resource_type(mesh) == FixedFunctionStorageQueries::RESOURCE_MESH && queries.get_base_type(mesh) == VS::INSTANCE_MESH);
	FF_QUERY_CHECK(queries.get_resource_type(texture) == FixedFunctionStorageQueries::RESOURCE_TEXTURE && queries.get_resource_type(target_texture) == FixedFunctionStorageQueries::RESOURCE_TEXTURE);
	FF_QUERY_CHECK(queries.get_resource_type(target) == FixedFunctionStorageQueries::RESOURCE_RENDER_TARGET && queries.get_resource_type(shader) == FixedFunctionStorageQueries::RESOURCE_SHADER && queries.get_resource_type(material) == FixedFunctionStorageQueries::RESOURCE_MATERIAL);
	{
		FixedFunctionTextureStorage foreign(p_device);
		const RID foreign_rid = foreign.texture_create();
		FF_QUERY_CHECK(queries.get_resource_type(foreign_rid) == FixedFunctionStorageQueries::RESOURCE_NONE && queries.get_base_type(foreign_rid) == VS::INSTANCE_NONE);
	}
	const RID ids[] = { texture, proxy, target, target_texture, shader, material, RID() };
	for (unsigned int i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
		FF_QUERY_CHECK(queries.get_base_type(ids[i]) == VS::INSTANCE_NONE);
	}
	FF_QUERY_CHECK(queries.has_os_feature("s3tc") == (Image::_image_decompress_bc != nullptr));
	FF_QUERY_CHECK(queries.has_os_feature("bptc") == (Image::_image_decompress_bptc != nullptr));
	const char *features[] = { "etc", "etc2", "pvrtc", "astc", "skinning_fallback", "unknown_feature" };
	for (unsigned int i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
		FF_QUERY_CHECK(!queries.has_os_feature(features[i]));
	}
	FF_QUERY_CHECK(queries.set_debug_generate_wireframes(false) == OK && queries.set_debug_generate_wireframes(true) == ERR_UNAVAILABLE);
	FF_QUERY_CHECK(queries.texture_set_shrink_all_x2_on_set_data(false) == OK && queries.texture_set_shrink_all_x2_on_set_data(true) == ERR_UNAVAILABLE);
	uint32_t gl_id = 42;
	FF_QUERY_CHECK(queries.texture_get_texid(texture, gl_id) == ERR_UNAVAILABLE && gl_id == 42 && queries.texture_bind(texture, 0) == ERR_UNAVAILABLE);
	FF_QUERY_CHECK(queries.texture_get_texid(RID(), gl_id) == ERR_INVALID_PARAMETER && queries.texture_bind(shader, 0) == ERR_INVALID_PARAMETER);
	FF_QUERY_CHECK(queries.texture_create_radiance_cubemap(texture, -1) == ERR_UNAVAILABLE && queries.texture_create_radiance_cubemap(texture, 0) == ERR_INVALID_PARAMETER && queries.texture_create_radiance_cubemap(RID(), 16) == ERR_INVALID_PARAMETER);
	int callback_calls = 0;
	FF_QUERY_CHECK(_test_callbacks(textures, texture, callback_calls));
	FF_QUERY_CHECK(textures.texture_set_path(texture, "res://query_texture.png") == OK);
	FF_QUERY_CHECK(textures.texture_request_allocate_2d(texture, Size2i(4, 4), Image::FORMAT_RGB8, 0, 0) == OK && textures.texture_request_data(texture, _image(4)) == OK);
	FixedFunctionTextureStorage::FFTextureUsage usage;
	FF_QUERY_CHECK(_find_usage(textures, texture, usage) && usage.cpu_bytes == 48 && usage.native_bytes == 0 && usage.size == Size2i(4, 4));
	FF_QUERY_CHECK(context.prepare_resources() == OK);
	FF_QUERY_CHECK(_find_usage(textures, texture, usage) && usage.cpu_bytes == 48 && (usage.native_bytes == 32 || usage.native_bytes == 64));
	const uint64_t small_bytes = usage.native_bytes;
	FF_QUERY_CHECK(p_device.get_texture_memory_usage() == small_bytes);
	FF_QUERY_CHECK(textures.texture_set_proxy(proxy, texture) == OK && _find_usage(textures, proxy, usage) && usage.cpu_bytes == 0 && usage.native_bytes == 0);
	FF_QUERY_CHECK(textures.texture_request_allocate_2d(texture, Size2i(8, 8), Image::FORMAT_RGB8, 0, 0) == OK && textures.texture_request_data(texture, _image(8)) == OK);
	FF_QUERY_CHECK(_find_usage(textures, texture, usage) && usage.cpu_bytes == 192 && usage.native_bytes == small_bytes && usage.size == Size2i(4, 4));
	queries.textures_keep_original(true);
	FF_QUERY_CHECK(queries.get_keep_original_requested());
	queries.textures_keep_original(false);
	FF_QUERY_CHECK(!queries.get_keep_original_requested() && textures.texture_get_data(texture)->get_width() == 8);
	FF_QUERY_CHECK(context.prepare_resources() == OK && _find_usage(textures, texture, usage) && usage.native_bytes == small_bytes * 4 && usage.size == Size2i(8, 8));
	const uint64_t ordinary_bytes = usage.native_bytes;
	FF_QUERY_CHECK(textures.render_target_set_size(target, 8, 8) == OK && textures.render_target_allocate(target) == OK);
	FF_QUERY_CHECK(_find_usage(textures, target_texture, usage) && usage.cpu_bytes == 0 && (usage.native_bytes == 128 || usage.native_bytes == 256));
	uint64_t total = ordinary_bytes + usage.native_bytes;
	uint64_t value = UINT64_MAX;
	FF_QUERY_CHECK(queries.get_render_info(VS::INFO_TEXTURE_MEM_USED, value) == OK && value == total && p_device.get_texture_memory_usage() == total);
	FF_QUERY_CHECK(queries.get_render_info(VS::INFO_VIDEO_MEM_USED, value) == OK && value == total);
	FF_QUERY_CHECK(queries.get_render_info(VS::INFO_USAGE_VIDEO_MEM_TOTAL, value) == OK && value == 0);
	FF_QUERY_CHECK(queries.get_render_info(VS::INFO_VERTEX_MEM_USED, value) == OK && value == 0);
	value = 123;
	int captured = 123;
	FF_QUERY_CHECK(queries.get_render_info(VS::RenderInfo(-1), value) == ERR_INVALID_PARAMETER && value == 123);
	FF_QUERY_CHECK(queries.get_captured_render_info(VS::RenderInfo(100), captured) == ERR_INVALID_PARAMETER && captured == 123);
	List<VS::TextureInfo> rows;
	VS::TextureInfo sentinel;
	sentinel.path = "sentinel";
	rows.push_back(sentinel);
	FF_QUERY_CHECK(queries.texture_debug_usage(&rows) == OK && rows.size() == 4 && rows.front()->get().path == "sentinel");
	uint64_t listed_bytes = 0;
	for (const List<VS::TextureInfo>::Element *E = rows.front()->next(); E; E = E->next()) {
		listed_bytes += E->get().bytes;
	}
	FF_QUERY_CHECK(listed_bytes == total && queries.texture_debug_usage(nullptr) == ERR_INVALID_PARAMETER);
	FF_QUERY_CHECK(_render(context, manager, proxy, false, r_pixels));
	const std::vector<uint32_t> original = r_pixels;
	FF_QUERY_CHECK(context.resize(160, 120) == OK && context.prepare_resources() == OK && context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FF_QUERY_CHECK(_render(context, manager, proxy, true, r_pixels) && r_pixels == original && callback_calls == 0);
	FF_QUERY_CHECK(queries.get_video_adapter_name() == adapter && queries.get_video_adapter_vendor() == vendor);
	FF_QUERY_CHECK(textures.request_free(texture) == OK);
	FF_QUERY_CHECK(!_find_usage(textures, texture, usage) && queries.texture_bind(texture, 0) == ERR_INVALID_PARAMETER);
	FF_QUERY_CHECK(queries.get_resource_type(texture) == FixedFunctionStorageQueries::RESOURCE_TEXTURE && p_device.get_texture_memory_usage() == total);
	FF_QUERY_CHECK(textures.texture_set_detect_callback(texture, FixedFunctionTextureStorage::DETECT_3D, _detect, &callback_calls) == ERR_ALREADY_IN_USE);
	FF_QUERY_CHECK(context.prepare_resources() == OK && callback_calls == 0);
	FF_QUERY_CHECK(p_device.get_texture_memory_usage() == total - ordinary_bytes);
	FF_QUERY_CHECK(context.finalize() == OK && p_device.get_texture_memory_usage() == 0 && p_device.get_adapter_name() == "Unknown" && p_device.get_adapter_vendor() == "Unknown");
	FF_QUERY_CHECK(queries.get_render_info(VS::INFO_2D_ITEMS_IN_FRAME, value) == OK && value == 0 && RasterizerStorage::base_singleton == singleton);
	printf("PASS: resource/profile contracts, borrowed detection callbacks, CPU/native texture diagnostics, deferred/proxy/release accounting, adapter identity, real canvas counters/captures and reset/shutdown.\n");
	return true;
}

#undef FF_QUERY_CHECK
