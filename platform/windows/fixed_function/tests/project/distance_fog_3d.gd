extends Control

const DEPTHS = [2.0, 6.0, 10.0]
const FOG_COLOR = Color(0.0, 0.0, 1.0)
const CANVAS_COLOR = Color(1.0, 0.5, 0.25)
const CAMERA_FAR = 13.0

var _viewport
var _camera
var _environment
var _meshes = []
var _reference
var _stage = 0
var _frames = 0
var _driver


func _ready():
	_driver = OS.get_video_driver_name(OS.get_current_video_driver())
	get_viewport().usage = Viewport.USAGE_2D
	_viewport = Viewport.new()
	_viewport.size = Vector2(320, 240)
	_viewport.own_world = true
	_viewport.hdr = false
	_viewport.render_target_v_flip = true
	_viewport.render_target_update_mode = Viewport.UPDATE_ALWAYS
	add_child(_viewport)

	_camera = Camera.new()
	_camera.current = true
	_camera.set_orthogonal(8, 0.1, CAMERA_FAR)
	_environment = Environment.new()
	_environment.background_mode = Environment.BG_COLOR
	_environment.background_color = Color(0.0, 0.5, 0.0)
	_environment.ambient_light_color = Color.white
	_environment.ambient_light_energy = 1.0
	_environment.ambient_light_sky_contribution = 0.0
	_environment.fog_color = FOG_COLOR
	_environment.fog_depth_begin = 3.0
	_environment.fog_depth_end = 9.0
	_camera.environment = _environment
	_viewport.add_child(_camera)

	var image = Image.new()
	image.create(4, 4, false, Image.FORMAT_RGBA8)
	image.fill(Color(0.65, 0.0, 0.0))
	var texture = ImageTexture.new()
	texture.create_from_image(image, 0)
	var quad = QuadMesh.new()
	for row in range(2):
		var material = SpatialMaterial.new()
		material.flags_unshaded = row == 0
		material.params_cull_mode = SpatialMaterial.CULL_DISABLED
		material.albedo_texture = texture
		material.emission_enabled = row == 1
		material.emission = Color(0.0, 0.4, 0.0)
		for column in range(3):
			var mesh = MeshInstance.new()
			mesh.mesh = quad
			mesh.material_override = material
			_meshes.append(mesh)
			_viewport.add_child(mesh)
	_place_meshes(false)

	var marker = ColorRect.new()
	marker.color = CANVAS_COLOR
	marker.rect_size = Vector2(12, 12)
	_viewport.add_child(marker)


func _place_meshes(perspective):
	for i in range(_meshes.size()):
		var depth = DEPTHS[i % 3]
		var height = 1.2 if i < 3 else -1.2
		var scale = depth * 0.13 if perspective else 1.0
		_meshes[i].scale = Vector3.ONE * scale
		_meshes[i].translation = Vector3((i % 3 - 1) * 2.5 * scale, height * scale, -depth)


func _close(color, expected, tolerance):
	return abs(color.r - expected.r) < tolerance and abs(color.g - expected.g) < tolerance and abs(color.b - expected.b) < tolerance


func _fail(message):
	printerr("DISTANCE FOG FAIL: ", message)
	set_process(false)
	get_tree().quit(1)


func _process(_delta):
	_frames += 1
	if _frames % 4 != 0:
		return
	var image = _viewport.get_texture().get_data()
	if image == null or image.is_empty():
		_fail("missing readback")
		return
	image.lock()
	var colors = []
	for mesh in _meshes:
		var point = _camera.unproject_position(mesh.translation)
		colors.append(image.get_pixel(int(point.x), int(point.y)))
	var marker = image.get_pixel(5, 5)
	var background = image.get_pixel(280, 5)
	image.unlock()
	if not _close(marker, CANVAS_COLOR, 0.035):
		_fail("canvas color changed: " + str(marker))
		return
	if _stage % 2 == 0:
		for color in colors:
			if color.r < 0.55 or color.b > 0.03:
				_fail("unfogged surface missing: " + str(color))
				return
		_reference = colors
		_reference.append(background)
	else:
		if not _close(background, _reference[6], 0.01):
			_fail("background changed")
			return
		for i in range(colors.size()):
			var amount = 0.0
			if _driver != "D3D3 Software":
				var fog_end = CAMERA_FAR if _stage == 5 else 9.0
				amount = clamp((DEPTHS[i % 3] - 3.0) / (fog_end - 3.0), 0.0, 1.0)
			var expected = _reference[i].linear_interpolate(FOG_COLOR, amount)
			if not _close(colors[i], expected, 0.08):
				_fail("stage %d sample %d: expected %s, got %s" % [_stage, i, expected, colors[i]])
				return
		print("DISTANCE FOG ", _driver, " stage=", _stage, " colors=", colors)

	_stage += 1
	if _stage == 6:
		print("DISTANCE FOG PASS")
		get_tree().quit()
		return
	if _stage == 2:
		_camera.set_perspective(70, 0.1, CAMERA_FAR)
		_place_meshes(true)
	if _stage == 4:
		ProjectSettings.set_setting("rendering/quality/fixed_function/retained_geometry", false)
		_environment.fog_depth_end = 0.0
	_environment.fog_depth_enabled = _stage != 4
	_environment.fog_enabled = _stage % 2 == 1 or _stage == 4
