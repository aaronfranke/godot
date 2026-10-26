#include "test_deferred_textures.h"

#include "drivers/fixed_function/canvas_item_renderer.h"
#include "drivers/fixed_function/renderer_texture_storage_fixed_function.h"
#include "servers/rendering/renderer_compositor.h"

#include <stdio.h>

#include <type_traits>

#define FIXED_FUNCTION_DEFERRED_CHECK(m_condition) \
	do { \
		if (!(m_condition)) { \
			printf("Deferred texture FAIL at line %d: %s\n", __LINE__, #m_condition); \
			return false; \
		} \
	} while (false)

static_assert(!std::is_abstract<RendererTextureStorageFixedFunction>::value, "The restricted storage boundary must be concrete.");

TestFixedFunctionDeferredTextures::FFDevice::FFDevice(FixedFunctionGraphicsDevice &p_delegate) :
		_delegate(p_delegate), _storage(nullptr), _request(NONE), _point(DURING_UPLOAD), _creations(0), _callback_error(OK), _nested_error(OK), _injected_error(ERR_SKIP) {
}

void TestFixedFunctionDeferredTextures::FFDevice::arm(Request p_request, CallbackPoint p_point, RID p_texture, const Ref<Image> &p_image) {
	_request = p_request;
	_point = p_point;
	_texture = p_texture;
	_replacement = p_image;
	_callback_error = _nested_error = OK;
}

void TestFixedFunctionDeferredTextures::FFDevice::arm_failure(Error p_error) {
	arm(FAIL_UPLOAD, DURING_UPLOAD, RID());
	_injected_error = p_error;
}

void TestFixedFunctionDeferredTextures::FFDevice::_apply_request() {
	const int request = _request;
	_request = NONE;
	if (request == REPLACE_DATA) {
		_callback_error = _storage->texture_request_data(_texture, _replacement);
	} else if (request == REALLOCATE) {
		_callback_error = _storage->texture_request_allocate_2d(_texture, Size2i(4, 4), Image::FORMAT_RGB8, 0, 0);
	} else if (request == FREE_RESOURCE) {
		_callback_error = _storage->request_free(_texture);
	}
	_nested_error = _storage->update_pending();
}

Error TestFixedFunctionDeferredTextures::FFDevice::initialize(void *p_window, uint32_t p_width, uint32_t p_height, bool p_vsync) {
	return _delegate.initialize(p_window, p_width, p_height, p_vsync);
}

Error TestFixedFunctionDeferredTextures::FFDevice::finalize() {
	return _delegate.finalize();
}

Error TestFixedFunctionDeferredTextures::FFDevice::resize(uint32_t p_width, uint32_t p_height) {
	return _delegate.resize(p_width, p_height);
}

Error TestFixedFunctionDeferredTextures::FFDevice::set_use_vsync(bool p_enabled) {
	return _delegate.set_use_vsync(p_enabled);
}

const FixedFunctionGraphicsDevice::FFCapabilities &TestFixedFunctionDeferredTextures::FFDevice::get_capabilities() const {
	return _delegate.get_capabilities();
}

uint32_t TestFixedFunctionDeferredTextures::FFDevice::get_width() const {
	return _delegate.get_width();
}

uint32_t TestFixedFunctionDeferredTextures::FFDevice::get_height() const {
	return _delegate.get_height();
}

uint32_t TestFixedFunctionDeferredTextures::FFDevice::get_target_width() const {
	return _delegate.get_target_width();
}

uint32_t TestFixedFunctionDeferredTextures::FFDevice::get_target_height() const {
	return _delegate.get_target_height();
}

Error TestFixedFunctionDeferredTextures::FFDevice::texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent) {
	return _delegate.texture_create_render_target(p_width, p_height, r_texture, p_transparent);
}

bool TestFixedFunctionDeferredTextures::FFDevice::texture_is_render_target_ready(RID p_texture) const {
	return _delegate.texture_is_render_target_ready(p_texture);
}

Error TestFixedFunctionDeferredTextures::FFDevice::texture_free(RID p_texture) {
	return _delegate.texture_free(p_texture);
}

Error TestFixedFunctionDeferredTextures::FFDevice::begin_frame(uint32_t p_clear_color, RID p_target, bool p_clear, bool p_use_depth) {
	return _delegate.begin_frame(p_clear_color, p_target, p_clear, p_use_depth);
}

Error TestFixedFunctionDeferredTextures::FFDevice::clear_depth(float p_depth) {
	return _delegate.clear_depth(p_depth);
}

Error TestFixedFunctionDeferredTextures::FFDevice::clear_color(uint32_t p_color) {
	return _delegate.clear_color(p_color);
}

Error TestFixedFunctionDeferredTextures::FFDevice::draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_count, const Projection &p_projection, RID p_texture, const FFDrawParameters &p_state) {
	return _delegate.draw_triangles(p_vertices, p_count, p_projection, p_texture, p_state);
}

Error TestFixedFunctionDeferredTextures::FFDevice::end_frame(bool p_present, bool p_discard) {
	return _delegate.end_frame(p_present, p_discard);
}

Error TestFixedFunctionDeferredTextures::FFDevice::read_frame(uint32_t *r_pixels, size_t p_count) {
	return _delegate.read_frame(r_pixels, p_count);
}

Error TestFixedFunctionDeferredTextures::FFDevice::prepare_resources() {
	const Error error = _delegate.prepare_resources();
	if (error == OK && _request != NONE && _request != FAIL_UPLOAD && _point == DURING_PREPARE) {
		_apply_request();
	}
	return error;
}

Error TestFixedFunctionDeferredTextures::FFDevice::texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame) {
	if (_request == FAIL_UPLOAD) {
		_request = NONE;
		return _injected_error;
	}
	const Error error = _delegate.texture_create(p_image, r_texture, p_allow_during_frame);
	if (error == OK) {
		_creations++;
		if (_request != NONE && _point == DURING_UPLOAD) {
			_apply_request();
		}
	}
	return error;
}

Ref<Image> TestFixedFunctionDeferredTextures::_make_image(int p_size, const Color &p_color) {
	Ref<Image> image;
	image = Image::create_empty(p_size, p_size, false, Image::FORMAT_RGBA8);
	image->fill(p_color);
	return image;
}

Ref<Image> TestFixedFunctionDeferredTextures::_make_patch() {
	Ref<Image> image = _make_image(2, Color(1, 1, 1));
	image->set_pixel(0, 0, Color(1, 0, 0));
	image->set_pixel(1, 0, Color(0, 1, 0));
	image->set_pixel(0, 1, Color(0, 0, 1));
	return image;
}

bool TestFixedFunctionDeferredTextures::_check_snapshot(FixedFunctionTextureStorage &p_storage, RID p_texture, const uint32_t *p_expected) {
	Ref<Image> image = p_storage.texture_get_data(p_texture);
	FIXED_FUNCTION_DEFERRED_CHECK(image.is_valid() && image->get_width() == 4 && image->get_height() == 4);
	uint32_t actual[16];
	for (int y = 0; y < 4; y++) {
		for (int x = 0; x < 4; x++) {
			actual[y * 4 + x] = image->get_pixel(x, y).to_argb32();
		}
	}
	for (int i = 0; i < 16; i++) {
		FIXED_FUNCTION_DEFERRED_CHECK(actual[i] == p_expected[i]);
	}
	image->fill(Color(1, 0, 1));
	return true;
}

bool TestFixedFunctionDeferredTextures::_render_and_check(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, RID p_texture, const uint32_t *p_expected, std::vector<uint32_t> &r_pixels) {
	FixedFunctionCanvasItemRenderer renderer(p_context, p_manager);
	RendererCanvasRender::Item item;
	RendererCanvasRender::Item::CommandRect *rect = item.alloc_command<RendererCanvasRender::Item::CommandRect>();
	rect->rect = Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT);
	rect->modulate = Color(1, 1, 1);
	rect->texture = p_texture;
	FIXED_FUNCTION_DEFERRED_CHECK(p_manager.begin_frame(0) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(renderer.render_items(&item, Color(1, 1, 1)) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(p_manager.end_frame(false) == OK);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_DEFERRED_CHECK(p_context.get_device().read_frame(r_pixels.data(), r_pixels.size()) == OK);
	for (int y = 0; y < CAPTURE_HEIGHT; y++) {
		for (int x = 0; x < CAPTURE_WIDTH; x++) {
			const uint32_t expected = p_expected[(y * 4 / CAPTURE_HEIGHT) * 4 + x * 4 / CAPTURE_WIDTH];
			const uint32_t actual = r_pixels[size_t(y) * CAPTURE_WIDTH + x];
			FIXED_FUNCTION_DEFERRED_CHECK((actual >> 24) == 255);
			for (int shift = 0; shift < 24; shift += 8) {
				const int delta = int((actual >> shift) & 255) - int((expected >> shift) & 255);
				FIXED_FUNCTION_DEFERRED_CHECK(delta >= -8 && delta <= 8);
			}
		}
	}
	return true;
}

bool TestFixedFunctionDeferredTextures::_test_reentrant_requests(FFDevice &p_device, FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, std::vector<uint32_t> &r_pixels) {
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	const int previous_count = storage.get_texture_count();
	const RID texture = storage.texture_create();
	uint32_t green[16];
	for (int i = 0; i < 16; i++) {
		green[i] = 0xff00ff00;
	}
	for (int point = FFDevice::DURING_PREPARE; point <= FFDevice::DURING_UPLOAD; point++) {
		FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, _make_image(4, Color(1, 0, 0))) == OK);
		p_device.arm(FFDevice::REPLACE_DATA, FFDevice::CallbackPoint(point), texture, _make_image(4, Color(0, 1, 0)));
		const int creations = p_device.get_creations();
		FIXED_FUNCTION_DEFERRED_CHECK(storage.update_pending() == ERR_SKIP);
		FIXED_FUNCTION_DEFERRED_CHECK(p_device.get_callback_error() == OK && p_device.get_nested_error() == ERR_BUSY);
		FIXED_FUNCTION_DEFERRED_CHECK(p_device.get_creations() == creations + (point == FFDevice::DURING_UPLOAD ? 1 : 0));
		FIXED_FUNCTION_DEFERRED_CHECK(storage.get_pending_upload_count() == 1);
		FIXED_FUNCTION_DEFERRED_CHECK(_check_snapshot(storage, texture, green));
		FIXED_FUNCTION_DEFERRED_CHECK(_render_and_check(p_context, p_manager, texture, green, r_pixels));
		FIXED_FUNCTION_DEFERRED_CHECK(storage.get_pending_upload_count() == 0);
	}
	// A legacy synchronous write must also yield to a newer deferred request.
	p_device.arm(FFDevice::REPLACE_DATA, FFDevice::DURING_UPLOAD, texture, _make_image(4, Color(0, 1, 0)));
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_data(texture, _make_image(4, Color(1, 0, 0))) == ERR_SKIP);
	FIXED_FUNCTION_DEFERRED_CHECK(p_device.get_callback_error() == OK && p_device.get_nested_error() == ERR_BUSY);
	FIXED_FUNCTION_DEFERRED_CHECK(_check_snapshot(storage, texture, green));
	FIXED_FUNCTION_DEFERRED_CHECK(_render_and_check(p_context, p_manager, texture, green, r_pixels));
	// Reallocation admitted during completion must not expose the older image's backing.
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, _make_image(4, Color(1, 0, 0))) == OK);
	p_device.arm(FFDevice::REALLOCATE, FFDevice::DURING_UPLOAD, texture);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.update_pending() == ERR_SKIP);
	FIXED_FUNCTION_DEFERRED_CHECK(p_device.get_callback_error() == OK && p_device.get_nested_error() == ERR_BUSY);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_format(texture) == Image::FORMAT_RGB8 && storage.texture_get_data(texture).is_null());
	RID native;
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_device_texture(texture, native) == ERR_SKIP && !native.is_valid());
	FIXED_FUNCTION_DEFERRED_CHECK(storage.update_pending() == OK && storage.get_pending_upload_count() == 0);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_device_texture(texture, native) == ERR_UNCONFIGURED);
	Ref<Image> rgb = _make_image(4, Color(0, 1, 0));
	rgb->convert(Image::FORMAT_RGB8);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, rgb) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(_render_and_check(p_context, p_manager, texture, green, r_pixels));
	// Free during upload retains the record until the native call returns safely.
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_allocate_2d(texture, Size2i(4, 4), Image::FORMAT_RGBA8, 0, 0) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, _make_image(4, Color(1, 0, 0))) == OK);
	p_device.arm(FFDevice::FREE_RESOURCE, FFDevice::DURING_UPLOAD, texture);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.update_pending() == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(p_device.get_callback_error() == OK && p_device.get_nested_error() == ERR_BUSY);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.get_pending_upload_count() == 0 && storage.get_pending_release_count() == 1);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.update_pending() == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.get_texture_count() == previous_count && storage.get_pending_release_count() == 0);
	return true;
}

bool TestFixedFunctionDeferredTextures::_test_first_upload(FFDevice &p_device, FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, std::vector<uint32_t> &r_pixels) {
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	const RID texture = storage.texture_create(true);
	const RID proxy = storage.texture_create(true);
	const Ref<Image> red = _make_image(4, Color(1, 0, 0));
	const Ref<Image> green = _make_image(4, Color(0, 1, 0));
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_proxy(proxy, texture) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_allocate_2d(texture, Size2i(4, 4), Image::FORMAT_RGBA8, 0, 0) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, red) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data_partial(texture, green, Rect2i(0, 0, 4, 4), Point2i()) == OK);
	uint32_t expected[16];
	for (int i = 0; i < 16; i++) {
		expected[i] = 0xff00ff00;
	}
	const int before = p_device.get_creations();
	RID native;
	FIXED_FUNCTION_DEFERRED_CHECK(storage.update_pending() == OK && storage.get_pending_upload_count() == 0);
	FIXED_FUNCTION_DEFERRED_CHECK(_check_snapshot(storage, texture, expected));
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_can_upload_on_use(proxy) && storage.texture_get_device_texture(proxy, native) == ERR_UNCONFIGURED);
	FIXED_FUNCTION_DEFERRED_CHECK(p_device.get_creations() == before);

	p_device.arm_failure(ERR_UNAVAILABLE);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_or_upload_device_texture(proxy, native) == ERR_UNCONFIGURED && !native.is_valid());
	FIXED_FUNCTION_DEFERRED_CHECK(!storage.texture_can_upload_on_use(texture) && _check_snapshot(storage, texture, expected));
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, red) == OK);
	p_device.arm(FFDevice::REPLACE_DATA, FFDevice::DURING_UPLOAD, texture, green);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_or_upload_device_texture(proxy, native) == ERR_SKIP && !native.is_valid());
	FIXED_FUNCTION_DEFERRED_CHECK(p_device.get_callback_error() == OK && p_device.get_nested_error() == ERR_BUSY);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.get_pending_upload_count() == 0 && storage.get_pending_release_count() == 1);
	FIXED_FUNCTION_DEFERRED_CHECK(_render_and_check(p_context, p_manager, proxy, expected, r_pixels));
	FIXED_FUNCTION_DEFERRED_CHECK(p_device.get_creations() == before + 2 && storage.get_pending_release_count() == 0);
	RID direct;
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_device_texture(texture, direct) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_or_upload_device_texture(proxy, native) == OK && native == direct);
	FIXED_FUNCTION_DEFERRED_CHECK(p_device.get_creations() == before + 2);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, red) == OK && storage.get_pending_upload_count() == 1);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_or_upload_device_texture(proxy, native) == ERR_SKIP);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.free(proxy) == OK && storage.free(texture) == OK);
	printf("PASS: first draw uploads the latest CPU snapshot once through a proxy, with safe failure and replacement.\n");
	return true;
}

bool TestFixedFunctionDeferredTextures::_test_msdf(FFDevice &p_device, FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_manager, std::vector<uint32_t> &r_pixels) {
	FixedFunctionTextureStorage &storage = p_context.get_texture_storage();
	FixedFunctionCanvasItemRenderer renderer(p_context, p_manager);
	const RID atlas = storage.texture_create(true);
	const RID proxy = storage.texture_create();
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_proxy(proxy, atlas) == OK);
	RendererCanvasRender::Item item;
	RendererCanvasRender::Item::CommandRect *glyph = item.alloc_command<RendererCanvasRender::Item::CommandRect>();
	glyph->rect = Rect2(0, 0, 8, 8);
	glyph->source = Rect2(0, 0, 4, 4);
	glyph->texture = proxy;
	glyph->modulate = Color(1, 0, 0);
	glyph->flags = RendererCanvasRender::CANVAS_RECT_REGION | RendererCanvasRender::CANVAS_RECT_MSDF;
	glyph->px_range = 16;
	glyph->outline = 0;
	RendererCanvasRender::Item::CommandRect *marker = item.alloc_command<RendererCanvasRender::Item::CommandRect>();
	marker->rect = Rect2(8, 0, 8, 8);
	marker->modulate = Color(0, 1, 0);
	for (int i = 0; i < 2; i++) {
		FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(atlas, _make_image(4, Color(1, i, 1))) == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(p_manager.begin_frame(0) == OK && p_manager.clear_render_target(Color(0, 0, 1)) == OK);
		const int before = p_device.get_creations();
		if (i == 0) {
			p_device.arm_failure(ERR_UNAVAILABLE);
		}
		FIXED_FUNCTION_DEFERRED_CHECK(renderer.render_items(&item, Color(1, 1, 1)) == OK);
		const RID coverage = storage.texture_get_msdf_coverage(atlas);
		FIXED_FUNCTION_DEFERRED_CHECK(coverage.is_valid() == (i == 1) && storage.texture_get_msdf_coverage(proxy) == coverage);
		FIXED_FUNCTION_DEFERRED_CHECK(p_device.get_creations() == before + i);
		RID native;
		FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_device_texture(atlas, native) == ERR_UNCONFIGURED && !native.is_valid());
		FIXED_FUNCTION_DEFERRED_CHECK(p_manager.end_frame(false) == OK);
		r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
		FIXED_FUNCTION_DEFERRED_CHECK(p_device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		// The RGB device compounds fixed-point texture modulation and blending for glyphs.
		const uint32_t glyph_pixel = r_pixels[CAPTURE_WIDTH + 1];
		const uint32_t marker_pixel = r_pixels[CAPTURE_WIDTH + 10];
		FIXED_FUNCTION_DEFERRED_CHECK(i == 0 ? glyph_pixel == 0xff0000ff : ((glyph_pixel & 0xff00ffff) == 0xff000000 && ((glyph_pixel >> 16) & 0xff) >= 252));
		FIXED_FUNCTION_DEFERRED_CHECK(marker_pixel == 0xff00ff00 || marker_pixel == 0xff00fe00);
	}
	const int before_outlines = p_device.get_creations();
	const RID thin = storage.texture_get_msdf_coverage(atlas, 20);
	const RID thick = storage.texture_get_msdf_coverage(proxy, 80);
	FIXED_FUNCTION_DEFERRED_CHECK(thin.is_valid() && thick.is_valid() && thin != thick && p_device.get_creations() == before_outlines + 2);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_msdf_coverage(proxy, 20) == thin && storage.texture_get_msdf_coverage(atlas, 72) == thick);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_msdf_coverage(atlas) != thin && p_device.get_creations() == before_outlines + 2);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_data(atlas, _make_image(4, Color(0, 1, 0))) == OK);
	p_device.arm_failure(ERR_UNAVAILABLE);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_msdf_coverage(proxy, 20).is_null() && storage.texture_get_msdf_coverage(atlas, 20).is_null());
	const RID rebuilt = storage.texture_get_msdf_coverage(atlas, 80);
	FIXED_FUNCTION_DEFERRED_CHECK(rebuilt.is_valid() && rebuilt != thick && storage.texture_get_msdf_coverage(atlas, 72) == rebuilt);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.free(proxy) == OK && storage.free(atlas) == OK);
	printf("PASS: MSDF fill refusal preserves the canvas, and bounded outline widths share backing through proxies and rebuild after updates.\n");
	return true;
}

bool TestFixedFunctionDeferredTextures::test(FixedFunctionGraphicsDevice &p_device, void *p_window, std::vector<uint32_t> &r_pixels) {
	RendererCompositor *const singleton = RendererCompositor::get_singleton();
	FFDevice device(p_device);
	FixedFunctionRenderContext context(device, p_window, CAPTURE_WIDTH, CAPTURE_HEIGHT, false);
	FixedFunctionRenderTargetManager manager(context);
	FixedFunctionCanvasItemRenderer renderer(context, manager);
	FixedFunctionTextureStorage &storage = context.get_texture_storage();
	device.set_storage(storage);
	FIXED_FUNCTION_DEFERRED_CHECK(context.initialize() == OK);
	const RID texture = storage.texture_create();
	const RID proxy = storage.texture_create();
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_size(proxy, Size2i(3, 5)) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_proxy(proxy, texture) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_data(texture, _make_image(4, Color(0, 0, 1))) == OK);
	const int initial_creations = device.get_creations();
	FIXED_FUNCTION_DEFERRED_CHECK(manager.begin_frame(0) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(manager.begin_draw() == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_allocate_2d(texture, Size2i(4, 4), Image::FORMAT_RGBA8, 0, 0) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_size_override(texture, Size2i(8, 8)) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_path(texture, "res://deferred.png") == OK);
	Ref<Image> black = _make_image(4, Color(0, 0, 0));
	Ref<Image> patch = _make_patch();
	Ref<Image> mipped = black->duplicate();
	FIXED_FUNCTION_DEFERRED_CHECK(mipped->generate_mipmaps() == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, mipped) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data_partial(texture, patch, Rect2i(0, 0, 2, 2), Point2i(1, 1)) == ERR_UNAVAILABLE);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, black) == OK);
	for (int i = 0; i < 64; i++) {
		FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, black) == OK);
	}
	// An outstanding writer must not mutate the accepted full-image snapshot.
	black->set_pixel(3, 3, Color(1, 0, 1));
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data_partial(texture, patch, Rect2i(0, 0, 2, 2), Point2i(1, 1)) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data_partial(texture, patch, Rect2i(1, 1, 1, 1), Point2i(0, 0)) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data_partial(texture, patch, Rect2i(2, 2, 0, 0), Point2i(4, 4)) == OK);
	patch->fill(Color(0, 0, 0));
	black->fill(Color(1, 0, 1));
	const uint32_t expected[] = {
		0xffffffff,
		0xff000000,
		0xff000000,
		0xff000000,
		0xff000000,
		0xffff0000,
		0xff00ff00,
		0xff000000,
		0xff000000,
		0xff0000ff,
		0xffffffff,
		0xff000000,
		0xff000000,
		0xff000000,
		0xff000000,
		0xff000000,
	};
	FIXED_FUNCTION_DEFERRED_CHECK(_check_snapshot(storage, texture, expected));
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_size(texture) == Size2i(8, 8) && storage.texture_get_image_size(texture) == Size2i(4, 4));
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_path(texture) == "res://deferred.png");
	FIXED_FUNCTION_DEFERRED_CHECK(storage.get_pending_upload_count() == 1 && device.get_creations() == initial_creations);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, Ref<Image>()) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, _make_image(8, Color())) == ERR_INVALID_PARAMETER);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_data(texture, _make_image(4, Color())) == ERR_BUSY);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.update_pending() == ERR_BUSY);
	RID native;
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_device_texture(proxy, native) == ERR_SKIP && !native.is_valid());
	RendererCanvasRender::Item item;
	RendererCanvasRender::Item::CommandRect *rect = item.alloc_command<RendererCanvasRender::Item::CommandRect>();
	rect->rect = Rect2(0, 0, CAPTURE_WIDTH, CAPTURE_HEIGHT);
	rect->modulate = Color(1, 1, 1);
	rect->texture = proxy;
	FIXED_FUNCTION_DEFERRED_CHECK(renderer.render_items(&item, Color(1, 1, 1)) == ERR_SKIP);
	FIXED_FUNCTION_DEFERRED_CHECK(manager.end_frame(true) == ERR_SKIP);
	r_pixels.resize(CAPTURE_WIDTH * CAPTURE_HEIGHT);
	FIXED_FUNCTION_DEFERRED_CHECK(device.read_frame(r_pixels.data(), r_pixels.size()) == ERR_UNCONFIGURED);
	FIXED_FUNCTION_DEFERRED_CHECK(_render_and_check(context, manager, proxy, expected, r_pixels));
	FIXED_FUNCTION_DEFERRED_CHECK(storage.get_pending_upload_count() == 0 && device.get_creations() == initial_creations + 1);
	const std::vector<uint32_t> capture = r_pixels;
	Ref<Image> final_image = storage.texture_get_data(texture);
	FIXED_FUNCTION_DEFERRED_CHECK(context.resize(0, 0) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, _make_image(4, Color(1, 1, 0))) == OK);
	const int before_suspension = device.get_creations();
	FIXED_FUNCTION_DEFERRED_CHECK(context.begin_frame(0.5) == OK && context.get_frame_delta_time() == 0.5);
	FIXED_FUNCTION_DEFERRED_CHECK(context.get_frame_status() == ERR_SKIP && context.end_frame(false) == ERR_SKIP);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.get_pending_upload_count() == 1 && device.get_creations() == before_suspension);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, final_image) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(context.resize(CAPTURE_WIDTH, CAPTURE_HEIGHT) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(_render_and_check(context, manager, proxy, expected, r_pixels));
	FIXED_FUNCTION_DEFERRED_CHECK(r_pixels == capture && device.get_creations() == before_suspension + 1);
	// A transient device failure retries the queued upload.
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, _make_image(4, Color(1, 1, 0))) == OK);
	device.arm_failure(ERR_SKIP);
	FIXED_FUNCTION_DEFERRED_CHECK(context.begin_frame(0) == OK && context.get_frame_status() == ERR_SKIP);
	FIXED_FUNCTION_DEFERRED_CHECK(context.end_frame(false) == ERR_SKIP && storage.get_pending_upload_count() == 1);
	// A texture-specific refusal must leave later uploads and unrelated draws usable.
	const RID other = storage.texture_create();
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_data(other, _make_image(4, Color(0, 0, 1))) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(other, _make_image(4, Color(1, 1, 0))) == OK);
	const int before_failure = device.get_creations();
	device.arm_failure(ERR_CANT_CREATE);
	FIXED_FUNCTION_DEFERRED_CHECK(manager.begin_frame(0) == OK && context.get_frame_status() == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.get_pending_upload_count() == 0 && device.get_creations() == before_failure + 1);
	const Error first_status = storage.texture_get_device_texture(texture, native);
	const Error other_status = storage.texture_get_device_texture(other, native);
	FIXED_FUNCTION_DEFERRED_CHECK((first_status == ERR_UNCONFIGURED && other_status == OK) || (first_status == OK && other_status == ERR_UNCONFIGURED));
	const RID failed_texture = first_status == ERR_UNCONFIGURED ? texture : other;
	const uint32_t yellow[16] = {
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
		0xffffff00,
	};
	FIXED_FUNCTION_DEFERRED_CHECK(_check_snapshot(storage, failed_texture, yellow));
	{
		FixedFunctionCanvasItemRenderer fallback_renderer(context, manager);
		RendererCanvasRender::Item fallback_item;
		RendererCanvasRender::Item::CommandRect *missing = fallback_item.alloc_command<RendererCanvasRender::Item::CommandRect>();
		missing->rect = Rect2(0, 0, 4, 8);
		missing->modulate = Color(1, 1, 1);
		missing->texture = failed_texture;
		const RID canvas_texture = storage.canvas_texture_create();
		RendererCanvasRender::Item::CommandRect *empty = fallback_item.alloc_command<RendererCanvasRender::Item::CommandRect>();
		empty->rect = Rect2(4, 0, 4, 8);
		empty->modulate = Color(1, 1, 1);
		empty->texture = canvas_texture;
		RendererCanvasRender::Item::CommandRect *solid = fallback_item.alloc_command<RendererCanvasRender::Item::CommandRect>();
		solid->rect = Rect2(8, 0, 8, 8);
		solid->modulate = Color(0, 1, 0);
		FIXED_FUNCTION_DEFERRED_CHECK(manager.clear_render_target(Color(0, 0, 1)) == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(fallback_renderer.render_items(&fallback_item, Color(1, 1, 1)) == OK && manager.end_frame(false) == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(device.read_frame(r_pixels.data(), r_pixels.size()) == OK);
		// The D3D7 RGB blend path can round full green down to 254.
		const uint32_t green = r_pixels[CAPTURE_WIDTH + 10];
		FIXED_FUNCTION_DEFERRED_CHECK(r_pixels[CAPTURE_WIDTH + 1] == 0xff0000ff && r_pixels[CAPTURE_WIDTH + 5] == 0xff0000ff && (green == 0xff00ff00 || green == 0xff00fe00));
		FIXED_FUNCTION_DEFERRED_CHECK(storage.canvas_texture_free(canvas_texture) == OK);
	}
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(failed_texture, _make_image(4, Color(1, 1, 0))) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(context.prepare_resources() == OK && storage.texture_get_device_texture(failed_texture, native) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.free(other) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, final_image) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(_render_and_check(context, manager, proxy, expected, r_pixels));
	// Free cancels a pending upload and defers native release until the pass closes.
	FIXED_FUNCTION_DEFERRED_CHECK(manager.begin_frame(0) == OK && manager.begin_draw() == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, final_image) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.request_free(texture) == OK && storage.request_free(texture) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.get_pending_upload_count() == 0 && storage.get_pending_release_count() == 1);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(texture, final_image) == ERR_ALREADY_IN_USE);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.update_pending() == ERR_BUSY);
	FIXED_FUNCTION_DEFERRED_CHECK(manager.end_frame(false) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(context.prepare_resources() == OK && storage.get_pending_release_count() == 0);
	FIXED_FUNCTION_DEFERRED_CHECK(!storage.texture_get_proxy(proxy).is_valid() && storage.texture_get_size(proxy) == Size2i(3, 5));
	if (device.get_capabilities().render_target_opaque) {
		const RID target = storage.render_target_create();
		const RID target_texture = storage.render_target_get_texture(target);
		FIXED_FUNCTION_DEFERRED_CHECK(storage.render_target_set_size(target, 16, 16) == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_set_proxy(proxy, target_texture) == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(manager.begin_frame(0) == OK && manager.select_render_target(target) == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(manager.begin_draw() == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(storage.request_free(target_texture) == ERR_ALREADY_IN_USE);
		FIXED_FUNCTION_DEFERRED_CHECK(storage.request_free(target) == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(!storage.render_target_is_ready(target) && !storage.render_target_needs_redraw(target));
		FIXED_FUNCTION_DEFERRED_CHECK(manager.select_render_target(RID()) == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(storage.update_pending() == OK && storage.get_render_target_count() == 1);
		FIXED_FUNCTION_DEFERRED_CHECK(manager.end_frame(false) == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(manager.begin_frame(0) == OK && storage.get_render_target_count() == 0);
		FIXED_FUNCTION_DEFERRED_CHECK(storage.get_pending_release_count() == 0 && !storage.texture_get_proxy(proxy).is_valid());
		FIXED_FUNCTION_DEFERRED_CHECK(manager.end_frame(false) == OK);
	}
	FIXED_FUNCTION_DEFERRED_CHECK(_test_reentrant_requests(device, context, manager, r_pixels));
	FIXED_FUNCTION_DEFERRED_CHECK(_test_first_upload(device, context, manager, r_pixels));
	FIXED_FUNCTION_DEFERRED_CHECK(_test_msdf(device, context, manager, r_pixels));
	if (Image::_image_decompress_bc) {
		// The fixture only offers RGBA8 uploads. Remove its BC3 decoder after acceptance.
		Vector<uint8_t> blocks;
		FIXED_FUNCTION_DEFERRED_CHECK(blocks.resize(16) == OK);
		memset(blocks.ptrw(), 0, blocks.size());
		const Ref<Image> image = Image::create_from_data(4, 4, false, Image::FORMAT_DXT5, blocks);
		const RID deferred = storage.texture_create();
		FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(deferred, image) == OK);
		void (*decoder)(Image *) = Image::_image_decompress_bc;
		Image::_image_decompress_bc = nullptr;
		const Error decoded = context.prepare_resources();
		Image::_image_decompress_bc = decoder;
		FIXED_FUNCTION_DEFERRED_CHECK(decoded == OK && storage.get_pending_upload_count() == 0);
		FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_device_texture(deferred, native) == ERR_UNCONFIGURED);
		FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_data(deferred)->get_format() == Image::FORMAT_DXT5);
		FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(deferred, image) == OK && context.prepare_resources() == OK);
		FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_get_device_texture(deferred, native) == OK && storage.free(deferred) == OK);
	}
	const RID cancelled = storage.texture_create();
	FIXED_FUNCTION_DEFERRED_CHECK(storage.texture_request_data(cancelled, _make_image(4, Color(1, 0, 0))) == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.request_free(proxy) == OK);
	const int before_finalize = device.get_creations();
	FIXED_FUNCTION_DEFERRED_CHECK(context.finalize() == OK);
	FIXED_FUNCTION_DEFERRED_CHECK(storage.get_texture_count() == 0 && storage.get_pending_upload_count() == 0 && storage.get_pending_release_count() == 0);
	FIXED_FUNCTION_DEFERRED_CHECK(device.get_creations() == before_finalize);
	FIXED_FUNCTION_DEFERRED_CHECK(context.initialize() == OK && context.prepare_resources() == OK && device.get_creations() == before_finalize);
	FIXED_FUNCTION_DEFERRED_CHECK(context.finalize() == OK && RendererCompositor::get_singleton() == singleton);
	r_pixels = capture;
	printf("PASS: deferred CPU snapshots/metadata, coalesced full/partial writes, suspension/retry, release cancellation, safe target lifetimes, reentrant requests, failed upload isolation, and shutdown.\n");
	return true;
}

#undef FIXED_FUNCTION_DEFERRED_CHECK
