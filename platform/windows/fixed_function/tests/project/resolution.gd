extends Control

var _frame = 0
var _mode = 0
var _viewport_texture
var _failed = false


func _ready():
	_mode = ProjectSettings.get_setting("rendering/quality/d3d7/oversize_mode")
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
			OS.window_fullscreen = false
			OS.window_maximized = false
			OS.window_size = Vector2(3840, 2160)
		3:
			if _mode == 0:
				_check(OS.window_size.x <= 2048 and OS.window_size.y <= 2048, "restricted programmatic resize")
			else:
				_check(OS.window_size == Vector2(3840, 2160), "unrestricted programmatic resize")
			OS.window_maximized = true
		5:
			if _mode == 0:
				_check(OS.window_size.x <= 2048 and OS.window_size.y <= 2048, "restricted maximize: " + str(OS.window_size) + " outer=" + str(OS.get_real_window_size()))
			OS.window_fullscreen = true
		8:
			_check(OS.window_size == OS.get_screen_size(OS.current_screen), "fullscreen size remains the monitor size")
			_check($Viewport.size == Vector2(4608, 288), "large viewport retains logical dimensions")
			_check(_viewport_texture.get_size() == Vector2(4608, 288), "large texture retains logical dimensions")
			OS.window_fullscreen = false
			OS.window_maximized = false
			OS.window_size = Vector2(640, 400)
			$Viewport.size = Vector2(4096, 512)
		10:
			OS.window_minimized = true
		11:
			OS.window_minimized = false
		12:
			_check($Viewport.get_texture() == _viewport_texture, "stable viewport texture after resize")
			_check(_viewport_texture.get_size() == Vector2(4096, 512), "resized logical dimensions")
			$Viewport/Blue.color = Color(0, 1, 0, 1)
		14:
			_check(OS.window_size == Vector2(640, 400), "window size survives minimize/restore")
		25:
			print("RESOLUTION PASS mode=", _mode, " frames=", _frame)
			get_tree().quit()
