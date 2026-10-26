extends Control

var _frame = 0
var _mode = 0
var _viewport_texture
var _failed = false


func _ready():
	_mode = ProjectSettings.get_setting("rendering/fixed_function/d3d7/oversize_mode")
	_viewport_texture = $Viewport.get_texture()
	$TextureRect.texture = _viewport_texture


func _check(condition, description):
	if not condition and not _failed:
		_failed = true
		push_error("RESOLUTION FAIL: " + description)
		get_tree().quit(1)


func _process(_delta):
	if _failed:
		return
	_frame += 1
	match _frame:
		1:
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
			DisplayServer.window_set_size(Vector2i(3840, 2160))
		3:
			if _mode == 0:
				_check(Vector2(DisplayServer.window_get_size()).x <= 2048 and Vector2(DisplayServer.window_get_size()).y <= 2048, "restricted programmatic resize")
			else:
				_check(Vector2(DisplayServer.window_get_size()) == Vector2(3840, 2160), "unrestricted programmatic resize")
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_MAXIMIZED)
		5:
			if _mode == 0:
				_check(Vector2(DisplayServer.window_get_size()).x <= 2048 and Vector2(DisplayServer.window_get_size()).y <= 2048, "restricted maximize: " + str(Vector2(DisplayServer.window_get_size())) + " outer=" + str(DisplayServer.window_get_size_with_decorations()))
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
		8:
			_check(Vector2(DisplayServer.window_get_size()) == Vector2(DisplayServer.screen_get_size(DisplayServer.window_get_current_screen())), "fullscreen size remains the monitor size: " + str(DisplayServer.window_get_size()) + " screen=" + str(DisplayServer.screen_get_size(DisplayServer.window_get_current_screen())))
			_check(Vector2($Viewport.size) == Vector2(4608, 288), "large viewport retains logical dimensions")
			_check(_viewport_texture.get_size() == Vector2(4608, 288), "large texture retains logical dimensions")
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN)
		10:
			_check(DisplayServer.window_get_size() == DisplayServer.screen_get_size(DisplayServer.window_get_current_screen()), "exclusive fullscreen retains monitor size")
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
			DisplayServer.window_set_size(Vector2i(640, 400))
			$Viewport.size = Vector2i(4096, 512)
		12:
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_MINIMIZED)
		13:
			DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
		14:
			_check($Viewport.get_texture() == _viewport_texture, "stable viewport texture after resize")
			_check(_viewport_texture.get_size() == Vector2(4096, 512), "resized logical dimensions")
			$Viewport/Blue.color = Color(0, 1, 0, 1)
		16:
			_check(Vector2(DisplayServer.window_get_size()) == Vector2(640, 400), "window size survives minimize/restore")
		25:
			print("RESOLUTION PASS mode=", _mode, " frames=", _frame)
			get_tree().quit()
