#include "test_canvas_item_renderer.h"

#include "drivers/fixed_function/renderer_canvas_render_fixed_function.h"
#include "drivers/fixed_function/tests/test_canvas_skinning.h"
#include "drivers/fixed_function/tests/test_nine_patches.h"
#include "servers/rendering/rendering_server_globals.h"

#include <stdio.h>

#include <limits>
#include <type_traits>

#define FIXED_FUNCTION_CANVAS_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Canvas FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

static_assert(!std::is_abstract<RendererCanvasRenderFixedFunction>::value, "Canvas facade must match the branch interface.");

TestFixedFunctionCanvasItemRenderer::FFCanvasFixture::FFCanvasFixture(FixedFunctionCanvasItemRenderer &p_renderer) :
		server(*RSG::canvas), renderer(p_renderer) {
}

bool TestFixedFunctionCanvasItemRenderer::FFCanvasFixture::add_polygon(RID p_item, const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, RSE::PrimitiveType p_primitive) {
	uint64_t id = 0;
	if (renderer.polygon_create(p_indices, p_points, p_colors, Vector<Point2>(), -1, id) != OK) {
		return false;
	}
	RendererCanvasRender::Item::CommandPolygon *command = server.canvas_item_owner.get_or_null(p_item)->alloc_command<RendererCanvasRender::Item::CommandPolygon>();
	command->primitive = p_primitive;
	command->polygon.polygon_id = id;
	return true;
}

RID TestFixedFunctionCanvasItemRenderer::FFCanvasFixture::create_item() {
	const RID item = server.canvas_item_allocate();
	server.canvas_item_initialize(item);
	items.push_back(item);
	return item;
}

TestFixedFunctionCanvasItemRenderer::FFCanvasFixture::~FFCanvasFixture() {
	for (List<RID>::Element *E = items.front(); E; E = E->next()) {
		// These explicit command fixtures use our CPU polygon store, not the active
		// dummy renderer. Release their IDs here before the normal Item destructor.
		RendererCanvasRender::Item *item = server.canvas_item_owner.get_or_null(E->get());
		for (RendererCanvasRender::Item::Command *command = item->commands; command; command = command->next) {
			if (command->type == RendererCanvasRender::Item::Command::TYPE_POLYGON) {
				RendererCanvasRender::Item::CommandPolygon *polygon = static_cast<RendererCanvasRender::Item::CommandPolygon *>(command);
				renderer.polygon_free(polygon->polygon.polygon_id);
				polygon->polygon.polygon_id = 0;
			}
		}
		server.free(E->get());
	}
}

Ref<Image> TestFixedFunctionCanvasItemRenderer::_make_image() {
	Vector<uint8_t> data;
	data.resize(4 * 4 * 4);
	{
		uint8_t *write = data.ptrw();
		const uint32_t colors[] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 4; x++) {
				const uint32_t color = colors[(y / 2) * 2 + x / 2];
				const int offset = (y * 4 + x) * 4;
				write[offset] = (color >> 16) & 255;
				write[offset + 1] = (color >> 8) & 255;
				write[offset + 2] = color & 255;
				write[offset + 3] = 255;
			}
		}
	}
	Ref<Image> image;
	image = Image::create_from_data(4, 4, false, Image::FORMAT_RGBA8, data);
	return image;
}

Ref<Image> TestFixedFunctionCanvasItemRenderer::_make_glyph() {
	Vector<uint8_t> data;
	data.resize(4 * 4 * 2);
	{
		uint8_t *write = data.ptrw();
		for (int i = 0; i < 16; i++) {
			write[i * 2] = 255;
			write[i * 2 + 1] = (i % 4) * 85;
		}
	}
	Ref<Image> image;
	// White luminance plus coverage alpha exercises the common glyph upload path.
	image = Image::create_from_data(4, 4, false, Image::FORMAT_LA8, data);
	return image;
}

Ref<Image> TestFixedFunctionCanvasItemRenderer::_make_nine_patch() {
	const uint32_t colors[] = { 0xffff0000, 0xffff8000, 0xff00ff00, 0xff00ffff, 0xffffffff, 0xffff00ff, 0xff0000ff, 0xff808080, 0xffffff00 };
	Vector<uint8_t> data;
	data.resize(8 * 8 * 4);
	{
		uint8_t *write = data.ptrw();
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				int row = 2;
				if (y < 2) {
					row = 0;
				} else if (y < 6) {
					row = 1;
				}
				int column = 2;
				if (x < 2) {
					column = 0;
				} else if (x < 6) {
					column = 1;
				}
				const uint32_t color = colors[row * 3 + column];
				const int offset = (y * 8 + x) * 4;
				write[offset] = (color >> 16) & 255;
				write[offset + 1] = (color >> 8) & 255;
				write[offset + 2] = color & 255;
				write[offset + 3] = 255;
			}
		}
	}
	Ref<Image> image;
	image = Image::create_from_data(8, 8, false, Image::FORMAT_RGBA8, data);
	return image;
}

bool TestFixedFunctionCanvasItemRenderer::_check_pixel(const std::vector<uint32_t> &p_pixels, int p_x, int p_y, uint32_t p_expected) {
	const uint32_t actual = p_pixels[size_t(p_y) * CAPTURE_WIDTH + p_x];
	for (int shift = 0; shift < 24; shift += 8) {
		const int delta = int((actual >> shift) & 255) - int((p_expected >> shift) & 255);
		if (delta < -8 || delta > 8 || (actual >> 24) != 255) {
			printf("Canvas pixel (%d,%d) actual=%08x expected=%08x\n", p_x, p_y, actual, p_expected);
			return false;
		}
	}
	return true;
}

bool TestFixedFunctionCanvasItemRenderer::_render(FixedFunctionRenderTargetManager &p_manager, FixedFunctionCanvasItemRenderer &p_canvas, FixedFunctionGraphicsDevice &p_device, RendererCanvasRender::Item *p_items, std::vector<uint32_t> &r_pixels) {
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.begin_frame(0.25) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.clear_render_target(Color(24 / 255.0f, 36 / 255.0f, 48 / 255.0f)) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_canvas.render_items(p_items, Color(1, 1, 1)) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	const uint32_t colors[] = { 0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff };
	const int transformed[8][4] = { { 0, 1, 2, 3 }, { 1, 0, 3, 2 }, { 2, 3, 0, 1 }, { 3, 2, 1, 0 }, { 0, 2, 1, 3 }, { 2, 0, 3, 1 }, { 1, 3, 0, 2 }, { 3, 1, 2, 0 } };
	for (int variant = 0; variant < 8; variant++) {
		for (int y = 0; y < 16; y++) {
			for (int x = 0; x < 16; x++) {
				FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, variant * 16 + x, y, colors[transformed[variant][(y / 8) * 2 + x / 8]]));
			}
		}
	}
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 2, 18, 0xffff0000));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 14, 18, 0xff00ff00));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 2, 30, 0xff0000ff));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 14, 30, 0xffffffff));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 34, 20, 0xff00ff00));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 50, 20, 0xff0000ff));
	for (int y = 0; y < 16; y++) {
		for (int x = 0; x < 32; x++) {
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 64 + x, 16 + y, colors[((y % 4) / 2) * 2 + (x % 4) / 2]));
		}
	}
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 16, 40, 0xffff0000));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 39, 55, 0xffff0000));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 40, 40, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 16, 56, 0xff00ff00));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 47, 63, 0xff00ff00));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 24, 32, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 8, 72, 0xff4c9298));
	for (int x = 0; x < 32; x++) {
		const float sample = (x + 0.5f) / 8 - 0.5f;
		const float coverage = p_device.get_capabilities().sampler_filter_linear ? CLAMP(sample / 3, 0.0f, 1.0f) : float(x / 8) / 3;
		const uint32_t red = uint32_t(24 + 231 * coverage + 0.5f);
		const uint32_t green = uint32_t(36 * (1 - coverage) + 0.5f);
		const uint32_t blue = uint32_t(48 * (1 - coverage) + 0.5f);
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 32 + x, 72, 0xff000000 | (red << 16) | (green << 8) | blue));
	}
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 100, 40, 0xffffffff));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 80, 48, 0xffffff00));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 88, 48, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 70, 36, 0xff00ffff));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 70, 33, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 68, 60, 0xffff00ff));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 80, 68, 0xffff00ff));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 64, 57, 0xff182430));
	const uint32_t patch_colors[] = { 0xffff0000, 0xffff8000, 0xff00ff00, 0xff00ffff, 0xffffffff, 0xffff00ff, 0xff0000ff, 0xff808080, 0xffffff00 };
	for (int y = 0; y < 24; y++) {
		for (int x = 0; x < 12; x++) {
			int row = 2;
			if (y < 2) {
				row = 0;
			} else if (y < 22) {
				row = 1;
			}
			int column = 2;
			if (x < 2) {
				column = 0;
			} else if (x < 10) {
				column = 1;
			}
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, x, 32 + y, patch_colors[row * 3 + column]));
		}
	}
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 120, 48, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 112, 48, 0xff00ffff));
	const bool scale_borders = false;
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			int row = 2;
			if (scale_borders) {
				if (y == 0) {
					row = 0;
				} else if (y < 3) {
					row = 1;
				}
			} else if (y < 2) {
				row = 0;
			}
			int column = 2;
			if (scale_borders) {
				if (x == 0) {
					column = 0;
				} else if (x < 3) {
					column = 1;
				}
			} else if (x < 2) {
				column = 0;
			}
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 112 + x, 64 + y, patch_colors[row * 3 + column]));
		}
	}
	for (int y = 0; y < 16; y++) {
		for (int x = 0; x < 128; x++) {
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, x, 80 + y, colors[((y / 4) * 32 + x / 4) % 4]));
		}
	}
	return true;
}

bool TestFixedFunctionCanvasItemRenderer::_test_lines_and_viewport_sampling(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionRenderContext &p_context, std::vector<uint32_t> &r_pixels) {
	FixedFunctionGraphicsDevice &device = p_context.get_device();
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	FFCanvasFixture fixture(p_renderer);
	const RID line = fixture.create_item();
	fixture.server.canvas_item_add_line(line, Vector2(4, 4), Vector2(20, 4), Color(1, 0, 0), -1, false);
	RendererCanvasRender::Item *line_item = fixture.server.canvas_item_owner.get_or_null(line);
	line_item->final_transform = Transform2D(3, 0, 0, 2, 0.5, 0.5);
	const RID strips = fixture.create_item();
	Vector<Point2> points;
	points.push_back(Vector2(8.5, 24.5));
	points.push_back(Vector2(24.5, 24.5));
	points.push_back(Vector2(24.5, 40.5));
	Vector<Color> colors;
	colors.push_back(Color(0, 1, 0));
	FIXED_FUNCTION_CANVAS_CHECK(fixture.add_polygon(strips, Vector<int>(), points, colors, RSE::PRIMITIVE_LINE_STRIP));
	Vector<int> indices;
	indices.push_back(0);
	indices.push_back(2);
	const RID indexed = fixture.create_item();
	colors.write[0] = Color(0, 0, 1);
	FIXED_FUNCTION_CANVAS_CHECK(fixture.add_polygon(indexed, indices, points, colors, RSE::PRIMITIVE_LINES));
	RendererCanvasRender::Item *strip_item = fixture.server.canvas_item_owner.get_or_null(strips);
	line_item->next = strip_item;
	strip_item->next = fixture.server.canvas_item_owner.get_or_null(indexed);
	strip_item->next->final_transform.set_origin(Vector2(40, 0));
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.begin_frame(0) == OK && p_manager.clear_render_target(Color(0, 0, 0)) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_renderer.render_items(line_item, Color(1, 1, 1)) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.end_frame(false) == OK && device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 20, 8, 0xffff0000));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 20, 7, 0xff000000) && _check_pixel(r_pixels, 20, 9, 0xff000000));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 16, 24, 0xff00ff00) && _check_pixel(r_pixels, 24, 32, 0xff00ff00));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 56, 32, 0xff0000ff));
	if (device.get_capabilities().render_target_opaque) {
		const RID target = storage.render_target_create();
		FIXED_FUNCTION_CANVAS_CHECK(storage.render_target_set_size(target, 16, 16) == OK);
		RendererCanvasRender::Item source;
		RendererCanvasRender::Item::CommandRect *red = source.alloc_command<RendererCanvasRender::Item::CommandRect>();
		red->rect = Rect2(0, 0, 16, 8);
		red->modulate = Color(1, 0, 0);
		FIXED_FUNCTION_CANVAS_CHECK(p_manager.begin_frame(0) == OK && p_manager.select_render_target(target) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_manager.clear_render_target(Color(0, 0, 1)) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_renderer.render_items(&source, Color(1, 1, 1)) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_manager.select_render_target(RID()) == OK && p_manager.end_frame(false) == OK);
		RendererCanvasRender::Item sample;
		RendererCanvasRender::Item::CommandRect *rect = sample.alloc_command<RendererCanvasRender::Item::CommandRect>();
		rect->rect = Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT);
		rect->texture = storage.render_target_get_texture(target);
		rect->modulate = Color(1, 1, 1);
		for (int flip = 0; flip < 2; flip++) {
			FIXED_FUNCTION_CANVAS_CHECK(storage.render_target_set_flag(target, FixedFunctionTextureStorage::RENDER_TARGET_VFLIP, flip != 0) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(p_manager.begin_frame(0) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(p_renderer.render_items(&sample, Color(1, 1, 1)) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(p_manager.end_frame(false) == OK && device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 64, 24, flip ? 0xff0000ff : 0xffff0000));
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 64, 72, flip ? 0xffff0000 : 0xff0000ff));
		}
		FIXED_FUNCTION_CANVAS_CHECK(storage.free(target) == OK);
	}
	printf("PASS: 4.x transformed hairlines, indexed lines/strips, top-left viewport sampling and explicit V flip.\n");
	return true;
}

bool TestFixedFunctionCanvasItemRenderer::_test_redraw_requests(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionTextureStorage &p_storage, RID p_texture) {
	const RID proxy = p_storage.texture_create();
	FIXED_FUNCTION_CANVAS_CHECK(p_storage.texture_set_proxy(proxy, p_texture) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(!p_storage.texture_get_force_redraw_if_visible(p_texture) && !p_storage.texture_get_force_redraw_if_visible(proxy));
	FIXED_FUNCTION_CANVAS_CHECK(p_storage.texture_set_force_redraw_if_visible(RID(), true) == ERR_INVALID_PARAMETER);
	struct FFRedrawCase {
		RID texture;
		bool base_enabled;
		bool proxy_enabled;
		bool empty;
		bool expected;
	};
	const FFRedrawCase cases[] = {
		{ p_texture, false, false, false, false },
		{ p_texture, true, false, false, true },
		{ proxy, true, false, false, false }, // Match GLES's before-proxy scheduling.
		{ proxy, false, true, false, true },
		{ RID(), false, true, false, false },
		{ proxy, false, true, true, false }, // An empty call must clear the previous intent.
		{ proxy, false, false, false, false },
	};
	RendererCanvasRender::Item item;
	RendererCanvasRender::Item::CommandRect *rect = item.alloc_command<RendererCanvasRender::Item::CommandRect>();
	rect->rect = Rect2(0, 0, 16, 16);
	rect->modulate = Color(1, 1, 1);
	for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		const FFRedrawCase &test = cases[i];
		FIXED_FUNCTION_CANVAS_CHECK(p_storage.texture_set_force_redraw_if_visible(p_texture, test.base_enabled) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_storage.texture_set_force_redraw_if_visible(proxy, test.proxy_enabled) == OK);
		rect->texture = test.texture;
		FIXED_FUNCTION_CANVAS_CHECK(p_manager.begin_frame(0) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_renderer.render_items(test.empty ? nullptr : &item, Color(1, 1, 1)) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_renderer.is_redraw_requested() == test.expected);
		FIXED_FUNCTION_CANVAS_CHECK(p_manager.end_frame(false) == OK);
	}
	// CPU image replacement must preserve visibility metadata through deferred upload.
	FIXED_FUNCTION_CANVAS_CHECK(p_storage.texture_set_force_redraw_if_visible(p_texture, true) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_storage.texture_request_data(p_texture, p_storage.texture_get_data(p_texture)) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_storage.texture_get_force_redraw_if_visible(p_texture));
	rect->texture = p_texture;
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.begin_frame(0) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_renderer.render_items(&item, Color(1, 1, 1)) == OK && p_renderer.is_redraw_requested());
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.end_frame(false) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_storage.texture_set_force_redraw_if_visible(p_texture, false) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_storage.texture_set_force_redraw_if_visible(proxy, true) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_storage.request_free(proxy) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(!p_storage.texture_get_force_redraw_if_visible(proxy));
	FIXED_FUNCTION_CANVAS_CHECK(p_storage.texture_set_force_redraw_if_visible(proxy, true) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_CANVAS_CHECK(p_storage.update_pending() == OK);
	printf("PASS: visible-texture redraw intent, before-proxy semantics, empty/untextured calls, deferred replacement, and pending release.\n");
	return true;
}

bool TestFixedFunctionCanvasItemRenderer::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FixedFunctionRenderTargetManager manager(context);
	FixedFunctionCanvasItemRenderer renderer(context, manager);
	FixedFunctionCanvasItemRenderer &canvas = renderer;
	RendererCanvasRender *const original_canvas = RendererCanvasRender::singleton;
	FIXED_FUNCTION_CANVAS_CHECK(RSG::canvas != nullptr);
	FIXED_FUNCTION_CANVAS_CHECK(context.initialize() == OK);
	FixedFunctionTextureStorage &storage = context.get_texture_storage();
	const RID texture = storage.texture_create();
	const RID proxy = storage.texture_create();
	FIXED_FUNCTION_CANVAS_CHECK(storage.texture_set_data(texture, _make_image()) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(storage.texture_set_proxy(proxy, texture) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(storage.texture_set_sampler_flags(texture, 128) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_CANVAS_CHECK(storage.texture_set_sampler_flags(texture, FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(storage.texture_get_sampler_flags(proxy) == FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR);
	FIXED_FUNCTION_CANVAS_CHECK(storage.texture_set_sampler_flags(texture, 0) == OK);
	const RID glyph = storage.texture_create();
	FIXED_FUNCTION_CANVAS_CHECK(storage.texture_set_data(glyph, _make_glyph()) == OK);
	const RID patch = storage.texture_create();
	FIXED_FUNCTION_CANVAS_CHECK(storage.texture_set_data(patch, _make_nine_patch()) == OK);
	FFCanvasFixture fixture(renderer);
	RendererCanvasCull &server = fixture.server;
	const RID rectangles = fixture.create_item();
	const RID atlas = storage.canvas_texture_create();
	FIXED_FUNCTION_CANVAS_CHECK(storage.canvas_texture_set_channel(atlas, RSE::CANVAS_TEXTURE_CHANNEL_DIFFUSE, texture) == OK);
	for (int variant = 0; variant < 8; variant++) {
		server.canvas_item_add_texture_rect(rectangles, Rect2(variant * 16, 0, (variant & 1) ? -16 : 16, (variant & 2) ? -16 : 16), proxy, false, Color(1, 1, 1), (variant & 4) != 0);
	}
	server.canvas_item_add_texture_rect_region(rectangles, Rect2(0, 16, 16, 16), atlas, Rect2(1, 1, 2, 2), Color(1, 1, 1), false, true);
	Vector<Rect2> rects;
	Vector<Rect2> sources;
	rects.push_back(Rect2(32, 16, 16, 16));
	rects.push_back(Rect2(48, 16, 16, 16));
	sources.push_back(Rect2(2, 0, 2, 2));
	sources.push_back(Rect2(0, 2, 2, 2));
	for (int i = 0; i < rects.size(); i++) {
		server.canvas_item_add_texture_rect_region(rectangles, rects[i], atlas, sources[i], Color(1, 1, 1), false, true);
	}
	server.canvas_item_owner.get_or_null(rectangles)->alloc_command<RendererCanvasRender::Item::CommandParticles>();
	server.canvas_item_add_texture_rect(rectangles, Rect2(64, 16, 32, 16), texture, true, Color(1, 1, 1), false);
	const RID transformed = fixture.create_item();
	RendererCanvasCull::Item *clipped = server.canvas_item_owner.get_or_null(transformed);
	clipped->final_transform.set_origin(Vector2(8, 32));
	clipped->final_clip_owner = clipped;
	clipped->final_clip_rect = Rect2(16, 40, 24, 16);
	Transform2D extra;
	extra.set_origin(Vector2(8, 8));
	server.canvas_item_add_set_transform(transformed, extra);
	server.canvas_item_add_rect(transformed, Rect2(0, 0, 32, 16), Color(1, 0, 0), false);
	server.canvas_item_add_clip_ignore(transformed, true);
	server.canvas_item_add_rect(transformed, Rect2(0, 16, 32, 8), Color(0, 1, 0), false);
	extra.set_origin(Vector2(16, 0));
	server.canvas_item_add_set_transform(transformed, extra);
	server.canvas_item_add_clip_ignore(transformed, false);
	server.canvas_item_add_rect(transformed, Rect2(0, 0, 8, 8), Color(0, 0, 1), false);
	const RID alpha = fixture.create_item();
	server.canvas_item_owner.get_or_null(alpha)->final_modulate = Color(0.5, 1, 1, 0.5);
	server.canvas_item_add_rect(alpha, Rect2(0, 64, 16, 16), Color(1, 1, 1), false);
	const RID text = fixture.create_item();
	server.canvas_item_owner.get_or_null(text)->texture_filter = p_device.get_capabilities().sampler_filter_linear ? RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR : RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST;
	server.canvas_item_add_texture_rect_region(text, Rect2(32, 64, 32, 16), glyph, Rect2(0, 0, 4, 4), Color(1, 0, 0), false, true);
	const RID primitive = fixture.create_item();
	Vector<Point2> points;
	Vector<Color> colors;
	points.push_back(Vector2(96, 32));
	points.push_back(Vector2(112, 32));
	points.push_back(Vector2(112, 48));
	points.push_back(Vector2(96, 48));
	colors.push_back(Color(1, 1, 1));
	server.canvas_item_add_primitive(primitive, points, colors, Vector<Point2>(), RID());
	const RID polygon = fixture.create_item();
	points.clear();
	colors.clear();
	Vector<int> indices;
	const Color palette[] = { Color(1, 0, 0), Color(0, 1, 0), Color(0, 0, 1), Color(1, 1, 1) };
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 32; x++) {
			const int base = points.size();
			points.push_back(Vector2(x * 4, 80 + y * 4));
			points.push_back(Vector2(x * 4 + 4, 80 + y * 4));
			points.push_back(Vector2(x * 4 + 4, 84 + y * 4));
			points.push_back(Vector2(x * 4, 84 + y * 4));
			const int quad[] = { 0, 1, 2, 0, 2, 3 };
			for (int j = 0; j < 6; j++) {
				indices.push_back(base + quad[j]);
			}
			for (int j = 0; j < 4; j++) {
				colors.push_back(palette[x % 4]);
			}
		}
	}
	FIXED_FUNCTION_CANVAS_CHECK(fixture.add_polygon(polygon, indices, points, colors, RSE::PRIMITIVE_TRIANGLES));
	const RID shapes = fixture.create_item();
	// 4.x stores circle/polyline tessellation as polygon IDs. Exercise that boundary
	// explicitly while keeping the active canvas singleton untouched.
	Vector<Point2> circle;
	Vector<int> circle_indices;
	Vector<Color> circle_colors;
	circle_colors.push_back(Color(1, 1, 0));
	for (int i = 0; i < 32; i++) {
		const real_t angle = real_t(i) * real_t(Math::TAU) / 32;
		circle.push_back(Vector2(80, 48) + Vector2(Math::sin(angle), Math::cos(angle)) * 8);
		circle_indices.push_back(i);
		circle_indices.push_back((i + 1) % 32);
		circle_indices.push_back(32);
	}
	circle.push_back(Vector2(80, 48));
	FIXED_FUNCTION_CANVAS_CHECK(fixture.add_polygon(shapes, circle_indices, circle, circle_colors, RSE::PRIMITIVE_TRIANGLES));
	server.canvas_item_add_line(shapes, Vector2(64, 36), Vector2(88, 36), Color(0, 1, 1), 4, false);
	Vector<Point2> line_points;
	line_points.push_back(Vector2(64, 60));
	line_points.push_back(Vector2(80, 60));
	line_points.push_back(Vector2(80, 72));
	Vector<Color> line_colors;
	line_colors.push_back(Color(1, 0, 1));
	Vector<Point2> strip;
	strip.push_back(Vector2(64, 58));
	strip.push_back(Vector2(64, 62));
	strip.push_back(Vector2(82, 58));
	strip.push_back(Vector2(78, 62));
	strip.push_back(Vector2(82, 72));
	strip.push_back(Vector2(78, 72));
	FIXED_FUNCTION_CANVAS_CHECK(fixture.add_polygon(shapes, Vector<int>(), strip, line_colors, RSE::PRIMITIVE_TRIANGLE_STRIP));
	server.canvas_item_add_nine_patch(shapes, Rect2(0, 32, 12, 24), Rect2(), patch, Vector2(2, 2), Vector2(2, 2), RSE::NINE_PATCH_STRETCH, RSE::NINE_PATCH_STRETCH, true, Color(1, 1, 1));
	server.canvas_item_add_nine_patch(shapes, Rect2(112, 32, 16, 32), Rect2(), patch, Vector2(2, 2), Vector2(2, 2), RSE::NINE_PATCH_STRETCH, RSE::NINE_PATCH_STRETCH, false, Color(1, 1, 1));
	server.canvas_item_add_nine_patch(shapes, Rect2(112, 64, 4, 4), Rect2(), patch, Vector2(2, 2), Vector2(2, 2), RSE::NINE_PATCH_STRETCH, RSE::NINE_PATCH_STRETCH, true, Color(1, 1, 1));
	RendererCanvasRender::Item *first = nullptr;
	RendererCanvasRender::Item *previous = nullptr;
	for (List<RID>::Element *E = fixture.items.front(); E; E = E->next()) {
		RendererCanvasRender::Item *item = server.canvas_item_owner.get_or_null(E->get());
		if (previous) {
			previous->next = item;
		} else {
			first = item;
		}
		previous = item;
	}
	FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
	RendererCanvasRender::Item *next = first->next;
	first->next = nullptr;
	const Error rectangles_error = canvas.render_items(first, Color(1, 1, 1));
	first->next = next;
	FIXED_FUNCTION_CANVAS_CHECK(rectangles_error == OK && manager.end_frame(false) == OK);
	const auto &rect_info = canvas.get_render_info().info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS];
	// The texture change and the tiled draw split the runs; the particle draws nothing.
	FIXED_FUNCTION_CANVAS_CHECK(rect_info[RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] == 3 &&
			rect_info[RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] == 12 &&
			rect_info[RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 24);
	{
		const RID fills = fixture.create_item();
		RendererCanvasRender::Item *fill_item = server.canvas_item_owner.get_or_null(fills);
		fill_item->final_clip_owner = fill_item;
		fill_item->final_clip_rect = Rect2(0, 0, 12, 12);
		server.canvas_item_add_rect(fills, Rect2(0, 0, 24, 12), Color(1, 0, 0), false);
		server.canvas_item_add_rect(fills, Rect2(0, 0, 0, 12), Color(1, 1, 1), false);
		server.canvas_item_add_rect(fills, Rect2(4, 0, 16, 12), Color(0, 1, 0, 0.5), false);
		server.canvas_item_add_clip_ignore(fills, true);
		server.canvas_item_add_rect(fills, Rect2(16, 0, 4, 12), Color(0, 0, 1), false);
		server.canvas_item_add_rect(fills, Rect2(20, 0, 4, 12), Color(1, 1, 0), false);
		server.canvas_item_add_set_transform(fills, Transform2D(0, Vector2(32, 0)));
		server.canvas_item_add_rect(fills, Rect2(0, 0, 8, 12), Color(1, 1, 1), false);
		FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK && manager.clear_render_target(Color(0, 0, 0)) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(canvas.render_items(fill_item, Color(1, 1, 1)) == OK && manager.end_frame(false) == OK);
		const auto &fill_info = canvas.get_render_info().info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS];
		const auto &fill_caps = p_device.get_capabilities();
		FIXED_FUNCTION_CANVAS_CHECK(fill_info[RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] == (fill_caps.canvas_fill_blit ? 5 : 3) &&
				fill_info[RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] == 5 && fill_info[RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 10);
		FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 2, 6, 0xffff0000) && _check_pixel(r_pixels, 6, 6, fill_caps.blend_mix ? 0xff7f8000 : 0xff00ff00));
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 14, 6, 0xff000000) && _check_pixel(r_pixels, 18, 6, 0xff0000ff));
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 22, 6, 0xffffff00) && _check_pixel(r_pixels, 34, 6, 0xffffffff));
	}
	{
		RID rect_items[8];
		RendererCanvasRender::Item *items[8];
		for (int i = 0; i < 8; i++) {
			rect_items[i] = fixture.create_item();
			items[i] = server.canvas_item_owner.get_or_null(rect_items[i]);
			if (i > 0) {
				items[i - 1]->next = items[i];
			}
		}
		server.canvas_item_add_rect(rect_items[0], Rect2(0, 0, 16, 12), Color(1, 0, 0), false);
		server.canvas_item_add_rect(rect_items[1], Rect2(0, 0, 16, 12), Color(0, 1, 0), false);
		items[1]->final_transform.set_origin(Vector2(8, 0));
		items[1]->final_modulate.a = 0.5;
		server.canvas_item_add_rect(rect_items[2], Rect2(24, 0, 12, 12), Color(0, 0, 1), false);
		items[2]->final_clip_owner = items[2];
		items[2]->final_clip_rect = Rect2(24, 0, 6, 12);
		for (int i = 3; i < 8; i++) {
			server.canvas_item_add_texture_rect_region(rect_items[i], Rect2(0, 0, i == 4 ? -8 : 8, 8), atlas, Rect2(1, 1, 2, 2), Color(1, 1, 1), false, true);
			items[i]->final_transform.set_origin(Vector2((i - 3) * 8, 16));
		}
		items[5]->texture_filter = RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR;
		std::vector<uint32_t> individual_items;
		for (int pass = 0; pass < 2; pass++) {
			// The explicit repeat setting renders identically and keeps the reference unbatched.
			for (RendererCanvasRender::Item *item : items) {
				item->texture_repeat = pass == 0 ? RSE::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED : RSE::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;
			}
			FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK && manager.clear_render_target(Color(0, 0, 0)) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(canvas.render_items(items[0], Color(1, 1, 1)) == OK && manager.end_frame(false) == OK);
			const auto &item_info = canvas.get_render_info().info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS];
			FIXED_FUNCTION_CANVAS_CHECK(item_info[RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] == (pass == 0 ? 8 : 5) &&
					item_info[RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] == 8 && item_info[RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 16);
			FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 25, 6, 0xff0000ff) && _check_pixel(r_pixels, 33, 6, 0xff000000));
			if (pass == 0) {
				individual_items = r_pixels;
			} else {
				FIXED_FUNCTION_CANVAS_CHECK(r_pixels == individual_items);
			}
		}
	}
	{
		const RID msdf = storage.texture_create();
		Ref<Image> distances = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
		distances->fill(Color(0.45f, 0.45f, 0.45f, 0.45f));
		FIXED_FUNCTION_CANVAS_CHECK(storage.texture_set_data(msdf, distances) == OK);
		const RID glyphs = fixture.create_item();
		RendererCanvasRender::Item *glyph_item = server.canvas_item_owner.get_or_null(glyphs);
		glyph_item->final_transform = Transform2D(Vector2(1.125, 0), Vector2(0, 1.25), Vector2(10, 8));
		glyph_item->final_clip_owner = glyph_item;
		glyph_item->final_clip_rect = Rect2(0, 0, 35, 32);
		const struct {
			RID texture;
			float range;
			int outline;
		} commands[] = { { glyph, -1, 0 }, { glyph, -1, 0 }, { texture, -1, 0 }, { msdf, 4, 0 }, { msdf, 8, 0 }, { msdf, 4, 0 },
			{ msdf, 4, 2 }, { msdf, 0, 0 }, { msdf, 4, 2 }, { msdf, 8, 4 } };
		const int command_count = sizeof(commands) / sizeof(commands[0]);
		std::vector<uint32_t> individual_glyphs;
		for (int pass = 0; pass < 2; pass++) {
			server.canvas_item_clear(glyphs);
			for (int i = 0; i < command_count; i++) {
				// The first pass splits every glyph. Both passes lift the clip at the same point.
				if (i == 5 || (pass == 0 && i > 0)) {
					server.canvas_item_add_clip_ignore(glyphs, i >= 5);
				}
				if (i % 2 == 0) {
					server.canvas_item_add_set_transform(glyphs, Transform2D(0, Vector2(-20, 0)));
				}
				server.canvas_item_add_set_transform(glyphs, Transform2D(real_t(i % 3) * real_t(0.1), Vector2(i % 2 * 4, i % 3)));
				const Rect2 dest(i * 6, 0, 8, 8);
				const Rect2 source(0, 0, 4, 4);
				const Color tint = i % 2 ? Color(0, 1, 0, 0.75) : Color(1, 0, 0);
				if (commands[i].range < 0) {
					server.canvas_item_add_font_glyph(glyphs, dest, commands[i].texture, source, tint);
				} else {
					server.canvas_item_add_msdf_texture_rect_region(glyphs, dest, commands[i].texture, source, tint, commands[i].outline, commands[i].range, 1);
				}
			}
			// The last packed glyph's transform applies to the following fill, until explicitly reset.
			server.canvas_item_add_rect(glyphs, Rect2(2, 20, 3, 3), Color(1, 0, 1), false);
			server.canvas_item_add_set_transform(glyphs, Transform2D());
			server.canvas_item_add_rect(glyphs, Rect2(2, 20, 3, 3), Color(1, 1, 1), false);
			FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK && manager.clear_render_target(Color(0, 0, 0)) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(canvas.render_items(glyph_item, Color(1, 1, 1)) == OK && manager.end_frame(false) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
			const auto &glyph_info = canvas.get_render_info().info[RSE::VIEWPORT_RENDER_INFO_TYPE_CANVAS];
			FIXED_FUNCTION_CANVAS_CHECK(glyph_info[RSE::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME] == (pass == 0 ? 11 : 8) &&
					glyph_info[RSE::VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME] == 11 && glyph_info[RSE::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] == 22);
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 18, 34, 0xffff00ff) && _check_pixel(r_pixels, 13, 34, 0xffffffff));
			if (pass == 0) {
				individual_glyphs = r_pixels;
			} else {
				FIXED_FUNCTION_CANVAS_CHECK(r_pixels == individual_glyphs);
			}
		}
		FIXED_FUNCTION_CANVAS_CHECK(storage.free(msdf) == OK);
	}
	FIXED_FUNCTION_CANVAS_CHECK(_render(manager, canvas, p_device, first, r_pixels));
	const std::vector<uint32_t> capture = r_pixels;
	{
		FixedFunctionMaterialStorage &materials = context.get_material_storage();
		const RID shader = materials.shader_create();
		const RID material = materials.material_create();
		FIXED_FUNCTION_CANVAS_CHECK(materials.shader_set_code(shader, "shader_type canvas_item; void fragment(){ COLOR=vec4(0,1,0,1); }") == OK && materials.material_set_shader(material, shader) == OK);
		FixedFunctionGraphicsDevice::FFDrawParameters state;
		FIXED_FUNCTION_CANVAS_CHECK(materials.material_get_canvas_parameters(material, state) == ERR_UNAVAILABLE);
		FFCanvasFixture fallback(renderer);
		const RID rect = fallback.create_item();
		fallback.server.canvas_item_add_rect(rect, Rect2(0, 0, 16, 16), Color(1, 0, 1), false);
		RendererCanvasRender::Item *item = fallback.server.canvas_item_owner.get_or_null(rect);
		item->material = material;
		FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK && manager.clear_render_target(Color(0, 0, 1)) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(canvas.render_items(item, Color(1, 1, 1)) == OK && manager.end_frame(false) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK && _check_pixel(r_pixels, 1, 1, 0xffff00ff));
		if (!p_device.get_capabilities().blend_add && p_device.get_capabilities().blend_mix) {
			FIXED_FUNCTION_CANVAS_CHECK(materials.shader_set_code(shader, "shader_type canvas_item; render_mode blend_add;") == OK);
			FIXED_FUNCTION_CANVAS_CHECK(materials.material_get_canvas_parameters(material, state) == OK && state.blend_mode == FixedFunctionGraphicsDevice::BLEND_MODE_ADD);
			FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK && manager.clear_render_target(Color(0, 0, 1)) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(canvas.render_items(item, Color(1, 1, 1)) == OK && manager.end_frame(false) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK && _check_pixel(r_pixels, 1, 1, 0xffff00ff));
		}
		item->material = RID();
		FIXED_FUNCTION_CANVAS_CHECK(materials.free(material) == OK && materials.free(shader) == OK);
	}
	FIXED_FUNCTION_CANVAS_CHECK(_test_redraw_requests(renderer, manager, storage, texture));
	FIXED_FUNCTION_CANVAS_CHECK(TestFixedFunctionNinePatches::test(context, manager, renderer, r_pixels));
	FIXED_FUNCTION_CANVAS_CHECK(TestFixedFunctionCanvasSkinning::test(context, manager, r_pixels));
	FIXED_FUNCTION_CANVAS_CHECK(_test_lines_and_viewport_sampling(renderer, manager, context, r_pixels));
	FIXED_FUNCTION_CANVAS_CHECK(_test_canvas_meshes(renderer, manager, context));
	// Reject a command after some pixels were drawn, without publishing partial contents.
	FFCanvasFixture rejected(renderer);
	const RID bad = rejected.create_item();
	rejected.server.canvas_item_add_rect(bad, Rect2(0, 0, 16, 16), Color(1, 0, 1), false);
	Vector<Point2> unsupported_point;
	unsupported_point.push_back(Vector2());
	rejected.server.canvas_item_add_primitive(bad, unsupported_point, Vector<Color>(), Vector<Point2>(), RID());
	FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
	const RID target = storage.render_target_create();
	FIXED_FUNCTION_CANVAS_CHECK(storage.render_target_set_size(target, 32, 32) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(storage.render_target_mark_for_recovery(target) == OK);
	if (p_device.get_capabilities().render_target_opaque) {
		FIXED_FUNCTION_CANVAS_CHECK(manager.select_render_target(target) == OK);
	}
	FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(rejected.server.canvas_item_owner.get_or_null(bad), Color(1, 1, 1)) == ERR_UNAVAILABLE);
	FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == (p_device.get_capabilities().render_target_opaque ? OK : ERR_UNAVAILABLE));
	FIXED_FUNCTION_CANVAS_CHECK(!storage.render_target_is_ready(target));
	FIXED_FUNCTION_CANVAS_CHECK(storage.render_target_has_failed(target) == p_device.get_capabilities().render_target_opaque);
	FIXED_FUNCTION_CANVAS_CHECK(storage.render_target_needs_redraw(target) != p_device.get_capabilities().render_target_opaque);
	FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == (p_device.get_capabilities().render_target_opaque ? OK : ERR_UNCONFIGURED));
	// An isolated offscreen failure still permits a completed, readable window pass.
	if (p_device.get_capabilities().render_target_opaque) {
		FIXED_FUNCTION_CANVAS_CHECK(p_device.begin_frame(0, RID(), false) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_device.end_frame(true, true) == OK);
	}
	FIXED_FUNCTION_CANVAS_CHECK(p_device.begin_frame(0, RID(), false) == ERR_UNCONFIGURED);
	// A pending/failed source must not hide invalid self-sampling of the active target.
	if (p_device.get_capabilities().render_target_opaque) {
		rejected.server.canvas_item_clear(bad);
		rejected.server.canvas_item_add_texture_rect(bad, Rect2(0, 0, 16, 16), storage.render_target_get_texture(target));
		FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK && manager.select_render_target(target) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(rejected.server.canvas_item_owner.get_or_null(bad), Color(1, 1, 1)) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == ERR_INVALID_PARAMETER);
	}
	// Malformed geometry must fail safely, and the next normal frame must still work.
	rejected.server.canvas_item_clear(bad);
	indices.write[0] = -1;
	uint64_t invalid_id = 0;
	FIXED_FUNCTION_CANVAS_CHECK(renderer.polygon_create(indices, points, colors, Vector<Point2>(), -1, invalid_id) == ERR_INVALID_PARAMETER && invalid_id == 0);
	RendererCanvasRender::Item::CommandPolygon *invalid = rejected.server.canvas_item_owner.get_or_null(bad)->alloc_command<RendererCanvasRender::Item::CommandPolygon>();
	invalid->primitive = RSE::PRIMITIVE_TRIANGLES;
	invalid->polygon.polygon_id = 0;
	FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(rejected.server.canvas_item_owner.get_or_null(bad), Color(1, 1, 1)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == ERR_INVALID_PARAMETER);
	// 4.x polygon IDs retain independent snapshots after the source arrays change.
	const int polygon_count = renderer.get_polygon_count();
	{
		FFCanvasFixture snapshot(renderer);
		const RID item = snapshot.create_item();
		Vector<Point2> quad;
		quad.push_back(Vector2(0, 0));
		quad.push_back(Vector2(16, 0));
		quad.push_back(Vector2(16, 16));
		quad.push_back(Vector2(0, 16));
		Vector<int> quad_indices;
		const int triangle_indices[] = { 0, 1, 2, 0, 2, 3 };
		for (int i = 0; i < 6; i++) {
			quad_indices.push_back(triangle_indices[i]);
		}
		Vector<Color> white;
		white.push_back(Color(1, 1, 1));
		FIXED_FUNCTION_CANVAS_CHECK(snapshot.add_polygon(item, quad_indices, quad, white, RSE::PRIMITIVE_TRIANGLES));
		FIXED_FUNCTION_CANVAS_CHECK(renderer.get_polygon_count() == polygon_count + 1);
		quad.write[0] = Vector2(std::numeric_limits<real_t>::quiet_NaN(), 0);
		quad_indices.write[0] = -1;
		white.write[0] = Color(0, 0, 0);
		FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(server.canvas_item_owner.get_or_null(item), Color(1, 1, 1)) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 8, 8, 0xffffffff));
	}
	FIXED_FUNCTION_CANVAS_CHECK(renderer.get_polygon_count() == polygon_count);
	// Approved mipmap fallback retains base-level bilinear sampling.
	server.canvas_item_owner.get_or_null(text)->texture_filter = RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS;
	FIXED_FUNCTION_CANVAS_CHECK(_render(manager, canvas, p_device, first, r_pixels));
	FIXED_FUNCTION_CANVAS_CHECK(r_pixels == capture);
	// Both item repeat and a CanvasTexture repeat override reach the native sampler.
	const RID mirrored_item = fixture.create_item();
	RendererCanvasCull::Item *mirrored = server.canvas_item_owner.get_or_null(mirrored_item);
	mirrored->texture_repeat = RSE::CANVAS_ITEM_TEXTURE_REPEAT_MIRROR;
	mirrored->texture_filter = RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST;
	const RID canvas_texture = storage.canvas_texture_create();
	FIXED_FUNCTION_CANVAS_CHECK(storage.canvas_texture_set_channel(canvas_texture, RSE::CANVAS_TEXTURE_CHANNEL_DIFFUSE, proxy) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(storage.canvas_texture_set_repeat(canvas_texture, RSE::CANVAS_ITEM_TEXTURE_REPEAT_MIRROR) == OK);
	server.canvas_item_add_texture_rect(mirrored_item, Rect2(0, 0, 8, 4), proxy, true, Color(1, 1, 1), false);
	server.canvas_item_add_texture_rect(mirrored_item, Rect2(0, 4, 8, 4), canvas_texture, true, Color(1, 1, 1), false);
	FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(mirrored, Color(1, 1, 1)) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	const FixedFunctionGraphicsDevice::FFCapabilities &caps = p_device.get_capabilities();
	const uint32_t second_left = !caps.sampler_repeat_mirrored && caps.sampler_repeat ? 0xffff0000 : 0xff00ff00;
	const uint32_t second_right = caps.sampler_repeat_mirrored ? 0xffff0000 : 0xff00ff00;
	for (int y : { 0, 4 }) {
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 0, y, 0xffff0000));
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 4, y, second_left));
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 7, y, second_right));
	}
	FIXED_FUNCTION_CANVAS_CHECK(storage.canvas_texture_free(canvas_texture) == OK);

	// Parallax offsets inherit the source basis and keep the original canvas clip.
	{
		const RID source = fixture.create_item();
		const RID repeated = fixture.create_item();
		const RID following = fixture.create_item();
		RendererCanvasRender::Item *repeat_source = server.canvas_item_owner.get_or_null(source);
		RendererCanvasRender::Item *repeat_item = server.canvas_item_owner.get_or_null(repeated);
		repeat_source->final_transform = Transform2D(Vector2(0, 1), Vector2(-1, 0), Vector2());
		repeat_item->repeat_source_item = repeat_source;
		repeat_item->repeat_size = Vector2(8, 6);
		repeat_item->repeat_times = 2;
		repeat_item->final_transform.set_origin(Vector2(20, 24));
		repeat_item->final_clip_owner = repeat_item;
		repeat_item->final_clip_rect = Rect2(14, 16, 10, 20);
		repeat_item->next = server.canvas_item_owner.get_or_null(following);
		server.canvas_item_add_rect(repeated, Rect2(0, 0, 4, 4), Color(1, 0, 0), false);
		server.canvas_item_add_rect(following, Rect2(40, 24, 4, 4), Color(0, 0, 1), false);
		for (int pass = 0; pass < 2; pass++) {
			if (pass == 1) {
				repeat_item->repeat_times = std::numeric_limits<int>::max();
			}
			FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(repeat_item, Color(1, 1, 1)) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 22, 26, 0xffff0000));
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 42, 26, 0xff0000ff));
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 28, 26, 0xff000000));
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 16, 18, pass == 0 ? 0xffff0000 : 0xff000000));
			FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 22, 34, pass == 0 ? 0xffff0000 : 0xff000000));
		}
	}

	server.canvas_item_owner.get_or_null(text)->texture_filter = p_device.get_capabilities().sampler_filter_linear ? RSE::CANVAS_ITEM_TEXTURE_FILTER_LINEAR : RSE::CANVAS_ITEM_TEXTURE_FILTER_NEAREST;
	// A float real_t cannot represent INT32_MAX exactly; do not cast a rounded-up clip.
	rejected.server.canvas_item_clear(bad);
	rejected.server.canvas_item_add_rect(bad, Rect2(0, 0, 16, 16), Color(1, 1, 1), false);
	RendererCanvasRender::Item *bad_item = rejected.server.canvas_item_owner.get_or_null(bad);
	bad_item->final_clip_owner = bad_item;
	bad_item->final_clip_rect = Rect2(real_t(2147483648.0), 0, 16, 16);
	FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(bad_item, Color(1, 1, 1)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == ERR_INVALID_PARAMETER);
	bad_item->final_clip_owner = nullptr;
	bad_item->final_transform.set_origin(Vector2(std::numeric_limits<real_t>::quiet_NaN(), 0));
	FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(bad_item, Color(1, 1, 1)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == ERR_INVALID_PARAMETER);
	// Minimized windows still receive a valid logical canvas bracket, with no error spam.
	FIXED_FUNCTION_CANVAS_CHECK(context.resize(0, 0) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0.25) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(first, Color(1, 1, 1)) == ERR_SKIP);
	FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == ERR_SKIP);
	FIXED_FUNCTION_CANVAS_CHECK(context.resize(160, 120) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(context.prepare_resources() == OK);
	FIXED_FUNCTION_CANVAS_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(_render(manager, canvas, p_device, first, r_pixels));
	FIXED_FUNCTION_CANVAS_CHECK(r_pixels == capture);
	FIXED_FUNCTION_CANVAS_CHECK(RendererCanvasRender::singleton == original_canvas);
	FIXED_FUNCTION_CANVAS_CHECK(storage.canvas_texture_free(atlas) == OK);
	printf("PASS: 4.x canvas commands and polygon snapshots, texture regions/flips/tiling, proxies, transforms/clipping, LA8 glyph filtering/alpha, primitives, indexed batching, circles/wide lines/polylines, fixed-pixel stretched nine-patches, invalid data, discard on failure, and reset recovery.\n");
	return true;
}
