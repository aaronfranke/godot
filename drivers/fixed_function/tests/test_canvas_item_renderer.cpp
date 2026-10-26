#include "test_canvas_item_renderer.h"

#include "test_canvas_skinning.h"
#include "test_nine_patches.h"

#include "core/project_settings.h"

#include <stdio.h>
#include <limits>

#define FIXED_FUNCTION_CANVAS_CHECK(m_condition)                            \
	do {                                                                    \
		if (!(m_condition)) {                                               \
			printf("Canvas FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false;                                                   \
		}                                                                   \
	} while (false)

RID TestFixedFunctionCanvasItemRenderer::FFCanvasFixture::create_item() {
	const RID item = server.canvas_item_create();
	items.push_back(item);
	return item;
}

TestFixedFunctionCanvasItemRenderer::FFCanvasFixture::~FFCanvasFixture() {
	for (List<RID>::Element *E = items.front(); E; E = E->next()) {
		server.free(E->get());
	}
}

Ref<Image> TestFixedFunctionCanvasItemRenderer::_make_image() {
	PoolVector<uint8_t> data;
	data.resize(4 * 4 * 4);
	{
		PoolVector<uint8_t>::Write write = data.write();
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
	image.instance();
	image->create(4, 4, false, Image::FORMAT_RGBA8, data);
	return image;
}

Ref<Image> TestFixedFunctionCanvasItemRenderer::_make_glyph() {
	PoolVector<uint8_t> data;
	data.resize(4 * 4 * 2);
	{
		PoolVector<uint8_t>::Write write = data.write();
		for (int i = 0; i < 16; i++) {
			write[i * 2] = 255;
			write[i * 2 + 1] = (i % 4) * 85;
		}
	}
	Ref<Image> image;
	image.instance();
	// Match the white luminance plus coverage-alpha atlas used by DynamicFont.
	image->create(4, 4, false, Image::FORMAT_LA8, data);
	return image;
}

Ref<Image> TestFixedFunctionCanvasItemRenderer::_make_nine_patch() {
	const uint32_t colors[] = { 0xffff0000, 0xffff8000, 0xff00ff00, 0xff00ffff, 0xffffffff, 0xffff00ff, 0xff0000ff, 0xff808080, 0xffffff00 };
	PoolVector<uint8_t> data;
	data.resize(8 * 8 * 4);
	{
		PoolVector<uint8_t>::Write write = data.write();
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
	image.instance();
	image->create(8, 8, false, Image::FORMAT_RGBA8, data);
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

bool TestFixedFunctionCanvasItemRenderer::_test_thin_lines(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionGraphicsDevice &p_device, std::vector<uint32_t> &r_pixels) {
	FFCanvasFixture fixture;
	const RID scaled = fixture.create_item();
	fixture.server.canvas_item_add_line(scaled, Vector2(4, 4), Vector2(20, 4), Color(1, 0, 0), 1, false);
	fixture.server.canvas_item_add_line(scaled, Vector2(4, 8), Vector2(20, 8), Color(0, 1, 0), -1, false);
	RasterizerCanvas::Item *scaled_item = fixture.server.canvas_item_owner.getptr(scaled);
	scaled_item->final_transform = Transform2D(3, 0, 0, 2, 4.5, 0.5);
	scaled_item->final_clip_owner = scaled_item;
	scaled_item->final_clip_rect = Rect2(0, 0, 50, CAPTURE_HEIGHT);
	const RID rotated = fixture.create_item();
	fixture.server.canvas_item_add_line(rotated, Vector2(4, 4), Vector2(20, 4), Color(0, 0, 1), 1, false);
	RasterizerCanvas::Item *rotated_item = fixture.server.canvas_item_owner.getptr(rotated);
	rotated_item->final_transform = Transform2D(0, 2, -3, 0, 112.5, 4.5);
	scaled_item->next = rotated_item;
	const RID other = fixture.create_item();
	fixture.server.canvas_item_add_line(other, Vector2(4.5, 30.5), Vector2(20.5, 30.5), Color(1, 1, 1, 0.5), 0.5, false);
	fixture.server.canvas_item_add_line(other, Vector2(5.5, 50.5), Vector2(25.5, 70.5), Color(1, 1, 0), 1, false);
	fixture.server.canvas_item_add_line(other, Vector2(80, 80), Vector2(80, 80), Color(1, 1, 1), 1, false);
	fixture.server.canvas_item_add_line(other, Vector2(40, 40), Vector2(64, 40), Color(1, 1, 1), 3, true);
	fixture.server.canvas_item_add_line(other, Vector2(40, 60), Vector2(64, 60), Color(1, 1, 1), 1, true);
	RasterizerCanvas::Item *other_item = fixture.server.canvas_item_owner.getptr(other);
	rotated_item->next = other_item;
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.begin_frame(0) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.clear_render_target(Color(24 / 255.0f, 36 / 255.0f, 48 / 255.0f)) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_renderer.render_items(scaled_item, Color(1, 1, 1)) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	const int rows[] = { 8, 16 };
	const uint32_t colors[] = { 0xffff0000, 0xff00ff00 };
	for (int i = 0; i < 2; i++) {
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 20, rows[i], colors[i]));
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 49, rows[i], colors[i]));
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 50, rows[i], 0xff182430));
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 20, rows[i] - 1, 0xff182430));
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 20, rows[i] + 1, 0xff182430));
	}
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 100, 20, 0xff0000ff));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 99, 20, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 101, 20, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 10, 30, 0xff8c9298));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 10, 29, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 10, 55, 0xffffff00));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 10, 54, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 80, 80, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 50, 39, 0xffffffff));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 50, 38, 0xff8c9298));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 50, 37, 0xff182430));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 50, 59, 0xff8c9298));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 50, 60, 0xff8c9298));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 50, 58, 0xff182430));
	printf("PASS: one-pixel canvas lines, scale/rotation, clipping, blending, diagonals and zero-length segments.\n");

	const RID polylines = fixture.create_item();
	Vector<Point2> points;
	points.push_back(Vector2(8, 8.5));
	points.push_back(Vector2(40, 8.5));
	points.push_back(Vector2(40, 17));
	Vector<Color> tints;
	tints.push_back(Color(1, 0, 0));
	tints.push_back(Color(0, 1, 0));
	tints.push_back(Color(0, 1, 0));
	fixture.server.canvas_item_add_polyline(polylines, points, tints, 1, false);
	points.clear();
	points.push_back(Vector2(70, 12.5));
	points.push_back(Vector2(80, 12.5));
	points.push_back(Vector2(90, 12.5));
	points.push_back(Vector2(100, 12.5));
	fixture.server.canvas_item_add_multiline(polylines, points, Vector<Color>(), 1, false);
	points.clear();
	points.push_back(Vector2(12, 26.25));
	points.push_back(Vector2(28, 26.25));
	points.push_back(Vector2(44, 26.25));
	points.push_back(Vector2(44, 46.25));
	fixture.server.canvas_item_add_polyline(polylines, points, Vector<Color>(), 4, true);
	points.clear();
	points.push_back(Vector2(12, 56));
	points.push_back(Vector2(28, 56));
	points.push_back(Vector2(44, 56));
	fixture.server.canvas_item_add_polyline(polylines, points, Vector<Color>(), 1, true);
	RasterizerCanvas::Item *polyline_item = fixture.server.canvas_item_owner.getptr(polylines);
	for (int reflected = 0; reflected < 2; reflected++) {
		if (reflected) {
			polyline_item->final_transform = Transform2D(-1, 0, 0, 1, CAPTURE_WIDTH, 0);
		}
		FIXED_FUNCTION_CANVAS_CHECK(p_manager.begin_frame(0) == OK && p_manager.clear_render_target(Color(0, 0, 0)) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_renderer.render_items(polyline_item, Color(1, 1, 1)) == OK && p_manager.end_frame(false) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		const auto x = [reflected](int p_x) { return reflected ? CAPTURE_WIDTH - 1 - p_x : p_x; };
		const uint32_t gradient = r_pixels[8 * CAPTURE_WIDTH + x(24)];
		FIXED_FUNCTION_CANVAS_CHECK(((gradient >> 16) & 255) > 80 && ((gradient >> 8) & 255) > 80);
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, x(74), 12, 0xffffffff) && _check_pixel(r_pixels, x(85), 12, 0xff000000) && _check_pixel(r_pixels, x(94), 12, 0xffffffff));
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, x(20), 25, 0xffffffff) && _check_pixel(r_pixels, x(20), 23, 0xff404040) && _check_pixel(r_pixels, x(20), 22, 0xff000000));
		FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, x(20), 55, 0xff808080) && _check_pixel(r_pixels, x(20), 56, 0xff808080) && _check_pixel(r_pixels, x(20), 57, 0xff000000));
	}
	printf("PASS: joined canvas hairlines, gradient colors, disjoint segments and antialiased thin and wide polylines.\n");
	return true;
}

bool TestFixedFunctionCanvasItemRenderer::_render(FixedFunctionRenderTargetManager &p_manager, RasterizerCanvasFixedFunction &p_canvas, FixedFunctionGraphicsDevice &p_device, RasterizerCanvas::Item *p_items, std::vector<uint32_t> &r_pixels) {
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.begin_frame(0.25) == OK);
	p_canvas.reset_frame();
	FIXED_FUNCTION_CANVAS_CHECK(p_manager.clear_render_target(Color(24 / 255.0f, 36 / 255.0f, 48 / 255.0f)) == OK);
	p_canvas.canvas_begin();
	FIXED_FUNCTION_CANVAS_CHECK(p_canvas.get_last_error() == OK && p_canvas.is_canvas_open());
	p_canvas.canvas_render_items(p_items, 0, Color(1, 1, 1), nullptr, Transform2D());
	FIXED_FUNCTION_CANVAS_CHECK(p_canvas.get_last_error() == OK);
	p_canvas.reset_canvas();
	FIXED_FUNCTION_CANVAS_CHECK(p_canvas.get_last_error() == OK);
	p_canvas.canvas_end();
	FIXED_FUNCTION_CANVAS_CHECK(!p_canvas.is_canvas_open());
	int margins[] = { 0, 0, 0, 0 };
	RID margin_textures[4];
	p_canvas.draw_window_margins(margins, margin_textures);
	FIXED_FUNCTION_CANVAS_CHECK(p_canvas.get_last_error() == OK);
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
	const bool scale_borders = int(ProjectSettings::get_singleton()->get("rendering/2d/options/ninepatch_mode")) == 1;
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
	RasterizerCanvas::Item item;
	RasterizerCanvas::Item::CommandRect *rect = memnew(RasterizerCanvas::Item::CommandRect);
	rect->rect = Rect2(0, 0, 16, 16);
	rect->modulate = Color(1, 1, 1);
	item.commands.push_back(rect);
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

bool TestFixedFunctionCanvasItemRenderer::_test_rect_batches(FixedFunctionCanvasItemRenderer &p_renderer, FixedFunctionRenderTargetManager &p_manager, FixedFunctionRenderContext &p_context, RID p_glyph, std::vector<uint32_t> &r_pixels) {
	RasterizerCanvas::Item items[5];
	for (int i = 0; i < 4; i++) {
		items[i].next = &items[i + 1];
	}
	for (int i = 0; i < 5; i++) {
		const bool glyph = i >= 3;
		auto *rect = memnew(RasterizerCanvas::Item::CommandRect);
		rect->rect = Rect2(glyph ? 0 : 8 * i, glyph ? 24 : 0, i == 1 ? 0 : 24, 16);
		rect->modulate = i == 0 ? Color(1, 0, 0) : Color(0, 1, 0);
		if (glyph) {
			rect->flags = RasterizerCanvas::CANVAS_RECT_REGION | RasterizerCanvas::CANVAS_RECT_FONT_GLYPH;
			rect->source = Rect2(0, 0, 4, 4);
			rect->texture = p_glyph;
		}
		if (i == 4) {
			auto *transform = memnew(RasterizerCanvas::Item::CommandTransform);
			transform->xform.set_origin(Vector2(32, 0));
			items[3].commands.push_back(transform);
		}
		items[MIN(i, 3)].commands.push_back(rect);
	}
	// A final untextured run also covers grouping inside a single item.
	for (int i = 0; i < 2; i++) {
		auto *rect = memnew(RasterizerCanvas::Item::CommandRect);
		rect->rect = Rect2(i * 8, 56, 8, 8);
		rect->modulate = Color(0, 0, 1);
		items[4].commands.push_back(rect);
	}
	FixedFunctionGraphicsDevice &device = p_context.get_device();
	FixedFunctionStorageQueries &queries = p_context.get_storage_queries();
	std::vector<uint32_t> reference;
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	for (int split = 0; split < 2; split++) {
		if (split) {
			auto *boundary = memnew(RasterizerCanvas::Item::CommandClipIgnore);
			boundary->ignore = false;
			items[3].commands.insert(2, boundary);
		}
		FIXED_FUNCTION_CANVAS_CHECK(p_manager.begin_frame(0) == OK && queries.render_info_begin_capture() == OK);
		FIXED_FUNCTION_CANVAS_CHECK(p_renderer.render_items(items, Color(1, 1, 1)) == OK && queries.render_info_end_capture() == OK);
		int count = 0;
		FIXED_FUNCTION_CANVAS_CHECK(queries.get_captured_render_info(VS::INFO_2D_ITEMS_IN_FRAME, count) == OK && count == 5);
		FIXED_FUNCTION_CANVAS_CHECK(queries.get_captured_render_info(VS::INFO_2D_DRAW_CALLS_IN_FRAME, count) == OK);
		FIXED_FUNCTION_CANVAS_CHECK(count == (device.get_capabilities().canvas_fill_blit ? 6 : 3 + split));
		FIXED_FUNCTION_CANVAS_CHECK(p_manager.end_frame(false) == OK && device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		if (split) {
			FIXED_FUNCTION_CANVAS_CHECK(r_pixels == reference);
		} else {
			reference = r_pixels;
		}
	}
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 4, 4, 0xffff0000) && _check_pixel(r_pixels, 18, 4, 0xff00ff00));
	printf("PASS: ordered solid rectangles across and within items, degenerate item counts, and glyph transforms with clip boundaries.\n");
	return true;
}

bool TestFixedFunctionCanvasItemRenderer::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	FixedFunctionRenderContext context(p_device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FixedFunctionRenderTargetManager manager(context);
	FixedFunctionCanvasItemRenderer renderer(context, manager);
	RasterizerCanvasFixedFunction canvas(context, manager);
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
	if (p_device.get_capabilities().sampler_filter_linear) {
		FIXED_FUNCTION_CANVAS_CHECK(storage.texture_set_sampler_flags(glyph, FixedFunctionGraphicsDevice::SAMPLER_FILTER_LINEAR) == OK);
	}
	FIXED_FUNCTION_CANVAS_CHECK(_test_rect_batches(renderer, manager, context, glyph, r_pixels));
	const RID patch = storage.texture_create();
	FIXED_FUNCTION_CANVAS_CHECK(storage.texture_set_data(patch, _make_nine_patch()) == OK);
	FFCanvasFixture fixture;
	VisualServerCanvas &server = fixture.server;
	const RID rectangles = fixture.create_item();
	for (int variant = 0; variant < 8; variant++) {
		server.canvas_item_add_texture_rect(rectangles, Rect2(variant * 16, 0, (variant & 1) ? -16 : 16, (variant & 2) ? -16 : 16), proxy, false, Color(1, 1, 1), (variant & 4) != 0, RID());
	}
	server.canvas_item_add_texture_rect_region(rectangles, Rect2(0, 16, 16, 16), texture, Rect2(1, 1, 2, 2), Color(1, 1, 1), false, RID(), true);
	Vector<Rect2> rects;
	Vector<Rect2> sources;
	rects.push_back(Rect2(32, 16, 16, 16));
	rects.push_back(Rect2(48, 16, 16, 16));
	sources.push_back(Rect2(2, 0, 2, 2));
	sources.push_back(Rect2(0, 2, 2, 2));
	server.canvas_item_add_texture_multirect_region(rectangles, rects, texture, sources, Color(1, 1, 1), 0, RID());
	server.canvas_item_owner.getptr(rectangles)->commands.push_back(memnew(RasterizerCanvas::Item::CommandParticles));
	server.canvas_item_add_texture_rect(rectangles, Rect2(64, 16, 32, 16), texture, true, Color(1, 1, 1), false, RID());
	const RID transformed = fixture.create_item();
	VisualServerCanvas::Item *clipped = server.canvas_item_owner.getptr(transformed);
	clipped->final_transform.set_origin(Vector2(8, 32));
	clipped->final_clip_owner = clipped;
	clipped->final_clip_rect = Rect2(16, 40, 24, 16);
	Transform2D extra;
	extra.set_origin(Vector2(8, 8));
	server.canvas_item_add_set_transform(transformed, extra);
	server.canvas_item_add_rect(transformed, Rect2(0, 0, 32, 16), Color(1, 0, 0));
	server.canvas_item_add_clip_ignore(transformed, true);
	server.canvas_item_add_rect(transformed, Rect2(0, 16, 32, 8), Color(0, 1, 0));
	extra.set_origin(Vector2(16, 0));
	server.canvas_item_add_set_transform(transformed, extra);
	server.canvas_item_add_clip_ignore(transformed, false);
	server.canvas_item_add_rect(transformed, Rect2(0, 0, 8, 8), Color(0, 0, 1));
	const RID alpha = fixture.create_item();
	server.canvas_item_owner.getptr(alpha)->final_modulate = Color(0.5, 1, 1, 0.5);
	server.canvas_item_add_rect(alpha, Rect2(0, 64, 16, 16), Color(1, 1, 1));
	const RID text = fixture.create_item();
	server.canvas_item_add_texture_rect_region(text, Rect2(32, 64, 32, 16), glyph, Rect2(0, 0, 4, 4), Color(1, 0, 0), false, RID(), true);
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
	// Exercise the fill fallback with antialiasing requested and other items still queued.
	server.canvas_item_add_triangle_array(polygon, indices, points, colors, Vector<Point2>(), Vector<int>(), Vector<float>(), RID(), -1, RID(), true, false);
	const RID shapes = fixture.create_item();
	server.canvas_item_add_circle(shapes, Vector2(80, 48), 8, Color(1, 1, 0));
	server.canvas_item_add_line(shapes, Vector2(64, 36), Vector2(88, 36), Color(0, 1, 1), 4, false);
	Vector<Point2> line_points;
	line_points.push_back(Vector2(64, 60));
	line_points.push_back(Vector2(80, 60));
	line_points.push_back(Vector2(80, 72));
	Vector<Color> line_colors;
	line_colors.push_back(Color(1, 0, 1));
	server.canvas_item_add_polyline(shapes, line_points, line_colors, 4, false);
	server.canvas_item_add_nine_patch(shapes, Rect2(0, 32, 12, 24), Rect2(), patch, Vector2(2, 2), Vector2(2, 2), VS::NINE_PATCH_STRETCH, VS::NINE_PATCH_STRETCH, true, Color(1, 1, 1), RID());
	server.canvas_item_add_nine_patch(shapes, Rect2(112, 32, 16, 32), Rect2(), patch, Vector2(2, 2), Vector2(2, 2), VS::NINE_PATCH_STRETCH, VS::NINE_PATCH_STRETCH, false, Color(1, 1, 1), RID());
	server.canvas_item_add_nine_patch(shapes, Rect2(112, 64, 4, 4), Rect2(), patch, Vector2(2, 2), Vector2(2, 2), VS::NINE_PATCH_STRETCH, VS::NINE_PATCH_STRETCH, true, Color(1, 1, 1), RID());
	RasterizerCanvas::Item *first = nullptr;
	RasterizerCanvas::Item *previous = nullptr;
	for (List<RID>::Element *E = fixture.items.front(); E; E = E->next()) {
		RasterizerCanvas::Item *item = server.canvas_item_owner.getptr(E->get());
		if (previous) {
			previous->next = item;
		} else {
			first = item;
		}
		previous = item;
	}
	FIXED_FUNCTION_CANVAS_CHECK(_render(manager, canvas, p_device, first, r_pixels));
	const std::vector<uint32_t> capture = r_pixels;
	FIXED_FUNCTION_CANVAS_CHECK(_test_meshes(renderer, manager, context));
	FIXED_FUNCTION_CANVAS_CHECK(TestFixedFunctionCanvasSkinning::test(context, manager, r_pixels));
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const Variant previous_patch_mode = settings->get("rendering/2d/options/ninepatch_mode");
	settings->set("rendering/2d/options/ninepatch_mode", int(previous_patch_mode) == 1 ? 0 : 1);
	const bool alternate_patch_mode_passed = _render(manager, canvas, p_device, first, r_pixels);
	settings->set("rendering/2d/options/ninepatch_mode", previous_patch_mode);
	FIXED_FUNCTION_CANVAS_CHECK(alternate_patch_mode_passed);
	// Use the actual canvas facade to compose textured/black window margins.
	FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
	canvas.reset_frame();
	FIXED_FUNCTION_CANVAS_CHECK(manager.clear_render_target(Color(1, 1, 1)) == OK);
	int margins[] = { 8, 4, 8, 4 };
	RID margin_textures[4];
	margin_textures[MARGIN_TOP] = texture;
	canvas.draw_window_margins(margins, margin_textures);
	FIXED_FUNCTION_CANVAS_CHECK(canvas.get_last_error() == OK && !canvas.is_canvas_open());
	FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 0, 40, 0xff000000));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 127, 40, 0xff000000));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 64, 95, 0xff000000));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 64, 48, 0xffffffff));
	FIXED_FUNCTION_CANVAS_CHECK(_check_pixel(r_pixels, 64, 0, 0xff00ff00));
	// Reject a command after some pixels were drawn, without publishing partial contents.
	FFCanvasFixture rejected;
	const RID bad = rejected.create_item();
	rejected.server.canvas_item_add_rect(bad, Rect2(0, 0, 16, 16), Color(1, 0, 1));
	rejected.server.canvas_item_owner.getptr(bad)->commands.push_back(memnew(RasterizerCanvas::Item::CommandPrimitive));
	FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
	const RID target = storage.render_target_create();
	FIXED_FUNCTION_CANVAS_CHECK(storage.render_target_set_size(target, 32, 32) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(storage.render_target_mark_for_recovery(target) == OK);
	if (p_device.get_capabilities().render_target_opaque) {
		FIXED_FUNCTION_CANVAS_CHECK(manager.select_render_target(target) == OK);
	}
	FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(rejected.server.canvas_item_owner.getptr(bad), Color(1, 1, 1)) == ERR_UNAVAILABLE);
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
		FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(rejected.server.canvas_item_owner.getptr(bad), Color(1, 1, 1)) == ERR_INVALID_PARAMETER);
		FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == ERR_INVALID_PARAMETER);
	}
	// Malformed geometry must fail safely, and the next normal frame must still work.
	rejected.server.canvas_item_clear(bad);
	indices.write[0] = -1;
	rejected.server.canvas_item_add_triangle_array(bad, indices, points, colors, Vector<Point2>(), Vector<int>(), Vector<float>(), RID(), -1, RID(), false, false);
	FIXED_FUNCTION_CANVAS_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(renderer.render_items(rejected.server.canvas_item_owner.getptr(bad), Color(1, 1, 1)) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == ERR_INVALID_PARAMETER);
	// A float real_t cannot represent INT32_MAX exactly; do not cast a rounded-up clip.
	rejected.server.canvas_item_clear(bad);
	rejected.server.canvas_item_add_rect(bad, Rect2(0, 0, 16, 16), Color(1, 1, 1));
	RasterizerCanvas::Item *bad_item = rejected.server.canvas_item_owner.getptr(bad);
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
	canvas.reset_frame();
	canvas.canvas_begin();
	FIXED_FUNCTION_CANVAS_CHECK(canvas.get_last_error() == ERR_SKIP && canvas.is_canvas_open());
	canvas.canvas_render_items(first, 0, Color(1, 1, 1), nullptr, Transform2D());
	FIXED_FUNCTION_CANVAS_CHECK(canvas.get_last_error() == ERR_SKIP);
	canvas.reset_canvas();
	FIXED_FUNCTION_CANVAS_CHECK(canvas.get_last_error() == ERR_SKIP);
	canvas.canvas_end();
	FIXED_FUNCTION_CANVAS_CHECK(canvas.get_last_error() == ERR_SKIP && !canvas.is_canvas_open());
	FIXED_FUNCTION_CANVAS_CHECK(manager.end_frame(false) == ERR_SKIP);
	FIXED_FUNCTION_CANVAS_CHECK(context.resize(160, 120) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(context.prepare_resources() == OK);
	FIXED_FUNCTION_CANVAS_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FIXED_FUNCTION_CANVAS_CHECK(_test_redraw_requests(renderer, manager, storage, texture));
	FIXED_FUNCTION_CANVAS_CHECK(_test_thin_lines(renderer, manager, p_device, r_pixels));
	FIXED_FUNCTION_CANVAS_CHECK(TestFixedFunctionNinePatches::test(context, manager, renderer, r_pixels));
	FIXED_FUNCTION_CANVAS_CHECK(_render(manager, canvas, p_device, first, r_pixels));
	FIXED_FUNCTION_CANVAS_CHECK(r_pixels == capture);
	printf("PASS: native Godot canvas facade/commands, margins, texture regions/flips/tiling, proxies, transforms/clipping, LA8 glyph filtering/alpha, primitives, indexed batching, circles/wide lines/polylines, stretched nine-patches in both border modes, invalid data, discard on failure, and reset recovery.\n");
	return true;
}
