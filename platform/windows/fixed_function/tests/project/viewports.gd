extends Node2D

var _frames = 0
var _failed = false
var _presses = 0
var _container
var _child
var _grandchild
var _nested
var _background
var _foreground
var _frozen
var _frozen_color
var _child_texture
var _frozen_texture


func _check(p_condition, p_message):
	if not p_condition:
		_failed = true
		printerr("VIEWPORT FAIL: ", p_message)
		get_tree().quit(1)
	return p_condition


func _color_rect(p_parent, p_color, p_size):
	var rect = ColorRect.new()
	rect.color = p_color
	rect.rect_size = p_size
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	p_parent.add_child(rect)
	return rect


func _make_viewport(p_parent, p_size):
	var viewport = Viewport.new()
	viewport.size = p_size
	viewport.usage = Viewport.USAGE_2D
	viewport.disable_3d = true
	viewport.handle_input_locally = false
	p_parent.add_child(viewport)
	viewport.render_target_update_mode = Viewport.UPDATE_WHEN_VISIBLE
	return viewport


func _resize_pattern():
	_background.rect_size = _child.size
	_foreground.rect_size = Vector2(_child.size.x / 2, _child.size.y)
	_nested.rect_position = Vector2(_child.size.x / 2, 0)
	_nested.rect_size = _child.size / 2


func _ready():
	var driver = OS.get_video_driver_name(OS.get_current_video_driver())
	print("VIEWPORT driver=", driver)
	OS.set_window_title("Fixed Function sub-viewports - " + driver)
	get_viewport().usage = Viewport.USAGE_2D
	_container = ViewportContainer.new()
	_container.stretch = true
	_container.rect_position = Vector2(8, 8)
	_container.rect_size = Vector2(128, 96)
	add_child(_container)
	_child = _make_viewport(_container, Vector2(128, 96))
	_background = _color_rect(_child, Color(0, 0, 1), _child.size)
	_foreground = _color_rect(_child, Color(1, 0, 0), Vector2(64, 96))
	_nested = ViewportContainer.new()
	_nested.stretch = true
	_child.add_child(_nested)
	_resize_pattern()
	_grandchild = _make_viewport(_nested, _nested.rect_size)
	_color_rect(_grandchild, Color(1, 1, 0), Vector2(128, 96))
	var button = Button.new()
	button.text = "Sub-viewport"
	button.rect_position = Vector2(4, 60)
	button.rect_size = Vector2(100, 24)
	button.connect("pressed", self, "_on_pressed")
	_child.add_child(button)
	_child_texture = _child.get_texture().get_rid()

	_frozen = _make_viewport(self, Vector2(128, 96))
	_frozen.render_target_v_flip = true
	_frozen.render_target_update_mode = Viewport.UPDATE_ONCE
	_frozen_color = _color_rect(_frozen, Color(1, 0, 1), _frozen.size)
	var preview = TextureRect.new()
	preview.texture = _frozen.get_texture()
	preview.rect_position = Vector2(168, 8)
	preview.rect_size = _frozen.size
	add_child(preview)
	_frozen_texture = _frozen.get_texture().get_rid()
	var label = Label.new()
	label.text = "Nested containers / frozen texture"
	label.rect_position = Vector2(8, 152)
	add_child(label)


func _on_pressed():
	_presses += 1


func _process(_delta):
	if _failed:
		return
	_frames += 1
	if _frames == 6:
		if not _check(_child.get_render_info(Viewport.RENDER_INFO_2D_DRAW_CALLS_IN_FRAME) > 0, "Child viewport did not draw"):
			return
		if not _check(_grandchild.get_render_info(Viewport.RENDER_INFO_2D_DRAW_CALLS_IN_FRAME) > 0, "Nested viewport did not draw"):
			return
		var event = InputEventMouseButton.new()
		event.position = Vector2(40, 80)
		event.global_position = event.position
		event.button_index = BUTTON_LEFT
		event.pressed = true
		Input.parse_input_event(event)
		event = event.duplicate()
		event.pressed = false
		Input.parse_input_event(event)
	if _frames == 10:
		if not _check(_presses == 1, "ViewportContainer did not forward the button click"):
			return
		_frozen_color.color = Color(0, 1, 1)
		# The server consumes UPDATE_ONCE; the node retains the requested property value.
	if _frames == 14:
		_frozen.render_target_update_mode = Viewport.UPDATE_ONCE
	if _frames == 18:
		_container.rect_size = Vector2(144, 108)
		_resize_pattern()
	if _frames == 22:
		_container.stretch_shrink = 2
		_resize_pattern()
		if not _check(_child.size == Vector2(72, 54), "Stretch/shrink did not resize the viewport"):
			return
	if _frames == 26:
		_container.stretch_shrink = 1
		_container.rect_size = Vector2(128, 96)
		_resize_pattern()
	if _frames == 30:
		if not _check(_child.get_texture().get_rid() == _child_texture and _frozen.get_texture().get_rid() == _frozen_texture, "Viewport texture identity changed"):
			return
		print("VIEWPORT PASS frames=", _frames, " presses=", _presses)
		if not OS.get_cmdline_args().has("--smoke-hold"):
			get_tree().quit()
