extends Node2D

var _frames = 0
var _failed = false


func _check(p_condition, p_message):
	if not p_condition:
		_failed = true
		printerr("SMOKE FAIL: ", p_message)
		get_tree().quit(1)
	return p_condition


func _ready():
	var driver = OS.get_video_driver_name(OS.get_current_video_driver())
	print("SMOKE driver=", driver)
	OS.set_window_title("Fixed Function Windows smoke - " + driver)
	if not _check(OS.get_native_handle(OS.WINDOW_HANDLE) != 0, "Missing native window"):
		return
	if driver.begins_with("D3D") or driver.begins_with("GLES"):
		var uses_gl = driver.begins_with("GLES")
		if not _check((OS.get_native_handle(OS.OPENGL_CONTEXT) != 0) == uses_gl, "Unexpected GL context"):
			return
		if not _check((OS.get_native_handle(OS.WINDOW_VIEW) != 0) == uses_gl, "Unexpected GL window view"):
			return
	get_viewport().usage = Viewport.USAGE_2D
	var rect = ColorRect.new()
	rect.color = Color(1, 0, 0)
	rect.rect_size = Vector2(160, 200)
	add_child(rect)

	var image = Image.new()
	image.create(2, 2, false, Image.FORMAT_RGBA8)
	image.fill(Color(1, 1, 1))
	var texture = ImageTexture.new()
	texture.create_from_image(image, 0)
	var sprite = Sprite.new()
	sprite.texture = texture
	sprite.modulate = Color(0, 1, 0)
	sprite.position = Vector2(240, 50)
	sprite.scale = Vector2(80, 50)
	add_child(sprite)

	var label = Label.new()
	label.text = "Fixed Function: ColorRect / Sprite / Label"
	label.rect_position = Vector2(8, 150)
	add_child(label)


func _process(_delta):
	if _failed:
		return
	_frames += 1
	if _frames == 4:
		var items = get_viewport().get_render_info(Viewport.RENDER_INFO_2D_ITEMS_IN_FRAME)
		var draws = get_viewport().get_render_info(Viewport.RENDER_INFO_2D_DRAW_CALLS_IN_FRAME)
		print("SMOKE viewport items=", items, " draws=", draws)
		if not _check(items > 0 and draws > 0, "The viewport did not draw"):
			return
		OS.window_size = Vector2(400, 240)
	if _frames == 8:
		OS.vsync_enabled = false
		OS.window_size = Vector2(320, 200)
	if _frames == 12:
		OS.vsync_enabled = true
	if _frames == 20:
		print("SMOKE PASS frames=", _frames)
		if not OS.get_cmdline_args().has("--smoke-hold"):
			get_tree().quit()
