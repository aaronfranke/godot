extends Control

const WINDOW_SIZE = Vector2(640, 400)
const MODES = ["STRETCH", "TILE", "TILE_FIT"]

var _frames = 0
var _last_patch


func _label(p_text, p_position):
	var label = Label.new()
	label.text = p_text
	label.rect_position = p_position
	add_child(label)


func _ready():
	var driver = OS.get_video_driver_name(OS.get_current_video_driver())
	print("NINEPATCH driver=", driver)
	OS.set_window_title("Fixed Function Nine-patches - " + driver)
	OS.window_size = WINDOW_SIZE
	rect_size = WINDOW_SIZE
	get_viewport().usage = Viewport.USAGE_2D
	var image = Image.new()
	image.create(8, 8, false, Image.FORMAT_RGBA8)
	image.lock()
	for y in range(8):
		for x in range(8):
			image.set_pixel(x, y, Color((32 + x * 24) / 255.0, (16 + y * 28) / 255.0, ((x ^ y) * 31) / 255.0))
	image.unlock()
	var texture = ImageTexture.new()
	texture.create_from_image(image, 0)
	_label("Horizontal mode → / Vertical mode ↓", Vector2(12, 12))
	for horizontal in range(3):
		_label(MODES[horizontal], Vector2(120 + horizontal * 160, 36))
	for vertical in range(3):
		_label(MODES[vertical], Vector2(12, 84 + vertical * 96))
		for horizontal in range(3):
			var patch = NinePatchRect.new()
			patch.texture = texture
			patch.region_rect = Rect2(1, 1, 6, 6)
			patch.rect_position = Vector2(120 + horizontal * 160, 60 + vertical * 96)
			patch.rect_size = Vector2(128, 72)
			for margin in range(4):
				patch.set_patch_margin(margin, 1)
			patch.axis_stretch_horizontal = horizontal
			patch.axis_stretch_vertical = vertical
			add_child(patch)
			_last_patch = patch
	_label("All nine mode pairs; the last panel omits its center", Vector2(12, 352))


func _process(_delta):
	_frames += 1
	if _frames == 8:
		_last_patch.draw_center = false
	if _frames == 12:
		OS.window_size = Vector2(672, 432)
	if _frames == 18:
		OS.window_size = WINDOW_SIZE
	if _frames == 24:
		var items = get_viewport().get_render_info(Viewport.RENDER_INFO_2D_ITEMS_IN_FRAME)
		var draws = get_viewport().get_render_info(Viewport.RENDER_INFO_2D_DRAW_CALLS_IN_FRAME)
		if items <= 0 or draws <= 0 or OS.window_size != WINDOW_SIZE:
			printerr("NINEPATCH FAIL: missing output or unsettled resize")
			get_tree().quit(1)
			return
		print("NINEPATCH PASS frames=", _frames, " items=", items, " draws=", draws)
		if not OS.get_cmdline_args().has("--smoke-hold"):
			get_tree().quit()
