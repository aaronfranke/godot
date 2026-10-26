extends Node2D

var _frames := 0
var _failed := false


func _check(p_condition: bool, p_message: String) -> bool:
	if not p_condition:
		_failed = true
		printerr("SMOKE FAIL: ", p_message)
		get_tree().quit(1)
	return p_condition


func _ready() -> void:
	var driver := RenderingServer.get_current_rendering_driver_name()
	print("SMOKE driver=", driver, " method=", RenderingServer.get_current_rendering_method())
	DisplayServer.window_set_title("Fixed Function Windows smoke - " + driver)
	if not _check(DisplayServer.window_get_native_handle(DisplayServer.WINDOW_HANDLE) != 0, "Missing native window"):
		return
	var uses_gl := driver.begins_with("opengl")
	if not _check((DisplayServer.window_get_native_handle(DisplayServer.OPENGL_CONTEXT) != 0) == uses_gl, "Unexpected GL context"):
		return
	if not _check((DisplayServer.window_get_native_handle(DisplayServer.WINDOW_VIEW) != 0) == uses_gl, "Unexpected GL window view"):
		return
	get_viewport().disable_3d = true
	var rect := ColorRect.new()
	rect.color = Color(1, 0, 0)
	rect.size = Vector2(160, 200)
	add_child(rect)
	var image := Image.create_empty(2, 2, false, Image.FORMAT_RGBA8)
	image.fill(Color(1, 1, 1))
	var texture := ImageTexture.create_from_image(image)
	var sprite := Sprite2D.new()
	sprite.texture = texture
	sprite.modulate = Color(0, 1, 0)
	sprite.position = Vector2(240, 50)
	sprite.scale = Vector2(80, 50)
	var material := CanvasItemMaterial.new()
	material.light_mode = CanvasItemMaterial.LIGHT_MODE_UNSHADED
	sprite.material = material
	sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	add_child(sprite)
	var label := Label.new()
	label.text = "Fixed Function: ColorRect / Sprite / Label"
	label.position = Vector2(8, 150)
	add_child(label)


func _process(_delta: float) -> void:
	if _failed:
		return
	_frames += 1
	if _frames == 4:
		var items := get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_CANVAS, Viewport.RENDER_INFO_OBJECTS_IN_FRAME)
		var draws := get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_CANVAS, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
		print("SMOKE viewport items=", items, " draws=", draws)
		if not _check(items > 0 and draws > 0, "The viewport did not draw"):
			return
		DisplayServer.window_set_size(Vector2i(400, 240))
	if _frames == 8:
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
		DisplayServer.window_set_size(Vector2i(320, 200))
	if _frames == 12:
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_ENABLED)
	if _frames == 20:
		print("SMOKE PASS frames=", _frames)
		if not OS.get_cmdline_user_args().has("--smoke-hold"):
			get_tree().quit()
