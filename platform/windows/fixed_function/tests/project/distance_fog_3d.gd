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
	_driver = RenderingServer.get_current_rendering_driver_name()
	get_viewport().disable_3d = true
	_viewport = SubViewport.new()
	_viewport.size = Vector2(320, 240)
	_viewport.own_world_3d = true
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(_viewport)

	_camera = Camera3D.new()
	_camera.current = true
	_camera.set_orthogonal(8, 0.1, CAMERA_FAR)
	_environment = Environment.new()
	_environment.background_mode = Environment.BG_COLOR
	_environment.background_color = Color(0.0, 0.5, 0.0)
	_environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	_environment.ambient_light_color = Color.WHITE
	_environment.ambient_light_energy = 1.0
	_environment.ambient_light_sky_contribution = 0.0
	_environment.fog_mode = Environment.FOG_MODE_DEPTH
	_environment.fog_light_color = FOG_COLOR
	_environment.fog_density = 1.0
	_environment.fog_depth_begin = 3.0
	_environment.fog_depth_end = 9.0
	_camera.environment = _environment
	_viewport.add_child(_camera)

	var image = Image.create(4, 4, false, Image.FORMAT_RGBA8)
	image.fill(Color(0.65, 0.0, 0.0))
	var texture = ImageTexture.create_from_image(image)
	var quad = QuadMesh.new()
	for row in range(3):
		var material = StandardMaterial3D.new()
		if row != 1:
			material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		material.disable_fog = row == 2
		material.cull_mode = BaseMaterial3D.CULL_DISABLED
		material.albedo_texture = texture
		material.emission_enabled = row == 1
		material.emission = Color(0.0, 0.4, 0.0)
		for column in range(3):
			var mesh = MeshInstance3D.new()
			mesh.mesh = quad
			mesh.material_override = material
			_meshes.append(mesh)
			_viewport.add_child(mesh)
	_place_meshes(false)

	var marker = ColorRect.new()
	marker.color = CANVAS_COLOR
	marker.size = Vector2(12, 12)
	_viewport.add_child(marker)


func _place_meshes(perspective):
	for i in range(_meshes.size()):
		var depth = DEPTHS[i % 3]
		var height = 2.1 - floori(i / 3.0) * 2.1
		var scale = depth * 0.13 if perspective else 1.0
		_meshes[i].scale = Vector3.ONE * scale
		_meshes[i].position = Vector3((i % 3 - 1) * 2.5 * scale, height * scale, -depth)


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
	var image = _viewport.get_texture().get_image()
	if image == null or image.is_empty():
		_fail("missing readback")
		return
	var colors = []
	for mesh in _meshes:
		var point = _camera.unproject_position(mesh.position)
		colors.append(image.get_pixel(int(point.x), int(point.y)))
	var marker = image.get_pixel(5, 5)
	var background = image.get_pixel(280, 5)
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
		if not _close(background, _reference[_meshes.size()], 0.01):
			_fail("background changed")
			return
		for i in range(colors.size()):
			var amount = 0.0
			if _driver != "d3d3_software" and i < 6:
				var depth = DEPTHS[i % 3]
				match _stage:
					1: amount = clampf((depth - 3.0) / 6.0, 0.0, 1.0)
					3: amount = clampf((depth - 3.0) / 12.0, 0.0, 1.0)
					5: amount = depth / CAMERA_FAR
					7: amount = depth / 12.0
			var color = FOG_COLOR
			if _stage == 7:
				color = Color(0, 0, 0.5).linear_to_srgb()
			var expected = _reference[i].lerp(color, amount)
			if not _close(colors[i], expected, 0.08):
				_fail("stage %d sample %d: expected %s, got %s" % [_stage, i, expected, colors[i]])
				return
		print("DISTANCE FOG ", _driver, " stage=", _stage, " colors=", colors)

	_stage += 1
	if _stage == 8:
		print("DISTANCE FOG PASS")
		get_tree().quit()
		return
	if _stage == 2:
		_camera.set_perspective(70, 0.1, CAMERA_FAR)
		_place_meshes(true)
	if _stage == 4:
		ProjectSettings.set_setting("rendering/quality/fixed_function/retained_geometry", false)
		# Environment lowers the start to zero when assigning an automatic end.
		_environment.fog_depth_end = 0.0
	if _stage == 6:
		_environment.fog_mode = Environment.FOG_MODE_EXPONENTIAL
		_environment.fog_light_energy = 0.5
		_environment.fog_density = log(2.0) / 6.0
	elif _stage == 2:
		_environment.fog_density = 0.5
	elif _stage == 4:
		_environment.fog_density = 0.0
	elif _stage == 5:
		_environment.fog_density = 1.0
	_environment.fog_enabled = _stage % 2 == 1 or _stage == 4
