extends Control

var _frames = 0
var _viewports = []
var _rectangles = []
var _frozen_cpu = 0.0
var _frozen_gpu = 0.0
var _failed = false


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("RENDER TIMING FAIL: ", message)
		get_tree().quit(1)
	return condition


func _add_viewport():
	var child = SubViewport.new()
	child.size = Vector2i(160, 120)
	child.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(child)
	var canvas = Control.new()
	child.add_child(canvas)
	for i in range(128):
		var rectangle = ColorRect.new()
		rectangle.position = Vector2((i % 16) * 10, floori(i / 16.0) * 15)
		rectangle.size = Vector2(10, 15)
		rectangle.color = Color(float(i % 7) / 7, 0.5, 0.8)
		canvas.add_child(rectangle)
	var display = TextureRect.new()
	display.texture = child.get_texture()
	display.position = Vector2(8 + _viewports.size() * 168, 40)
	add_child(display)
	RenderingServer.viewport_set_measure_render_time(child.get_viewport_rid(), true)
	_viewports.append(child)
	_rectangles.append(display)
	return child


func _ready():
	var label = Label.new()
	label.text = "Fixed Function viewport timing"
	label.position = Vector2(8, 8)
	add_child(label)
	_add_viewport()
	_add_viewport()
	RenderingServer.viewport_set_measure_render_time(get_viewport().get_viewport_rid(), true)


func _check_timing(viewport):
	var cpu = RenderingServer.viewport_get_measured_render_time_cpu(viewport.get_viewport_rid())
	var gpu = RenderingServer.viewport_get_measured_render_time_gpu(viewport.get_viewport_rid())
	return _check(cpu > 0 and cpu < 1000 and is_finite(cpu), "missing/invalid CPU sample") and _check(is_equal_approx(cpu, gpu), "GPU-slot estimate has incorrect units or frame pairing")


func _process(_delta):
	if _failed:
		return
	_frames += 1
	if _frames == 10:
		if not _check_timing(get_viewport()) or not _check_timing(_viewports[0]) or not _check_timing(_viewports[1]):
			return
		RenderingServer.viewport_set_measure_render_time(_viewports[1].get_viewport_rid(), false)
		_frozen_cpu = RenderingServer.viewport_get_measured_render_time_cpu(_viewports[1].get_viewport_rid())
		_frozen_gpu = RenderingServer.viewport_get_measured_render_time_gpu(_viewports[1].get_viewport_rid())
	if _frames == 14:
		if not _check(RenderingServer.viewport_get_measured_render_time_cpu(_viewports[1].get_viewport_rid()) == _frozen_cpu, "disabled CPU sample changed"):
			return
		if not _check(RenderingServer.viewport_get_measured_render_time_gpu(_viewports[1].get_viewport_rid()) == _frozen_gpu, "disabled GPU estimate changed"):
			return
		_rectangles[1].queue_free()
		_viewports[1].queue_free()
	if _frames == 17:
		var replacement = _add_viewport()
		_check(RenderingServer.viewport_get_measured_render_time_cpu(replacement.get_viewport_rid()) == 0, "new viewport inherited a stale sample")
	if _frames == 25:
		if not _check_timing(get_viewport()) or not _check_timing(_viewports[0]) or not _check_timing(_viewports[2]):
			return
		print("RENDER TIMING PASS driver=", RenderingServer.get_current_rendering_driver_name(), " cpu_ms=", RenderingServer.viewport_get_measured_render_time_cpu(_viewports[0].get_viewport_rid()), " gpu_estimate_ms=", RenderingServer.viewport_get_measured_render_time_gpu(_viewports[0].get_viewport_rid()))
		get_tree().quit()
