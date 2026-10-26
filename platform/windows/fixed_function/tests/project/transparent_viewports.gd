extends Node2D

var _frames = 0
var _failed = false
var _source
var _nested
var _spatial
var _spatial_material
var _environment
var _red
var _snapshot


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("TRANSPARENT VIEWPORT FAIL: ", message)
		get_tree().quit(1)
	return condition


func _color(image, point, expected):
	if not _check(image != null, "missing completed viewport image"):
		return false
	# Godot 4.x viewport image readback uses top-left rows.
	var actual = image.get_pixel(int(point.x), int(point.y))
	for channel in range(4):
		if not _check(abs(actual[channel] - expected[channel]) < 0.045, "pixel %s: got %s expected %s" % [point, actual, expected]):
			return false
	return true


func _rect(parent, bounds, color):
	var rectangle = ColorRect.new()
	rectangle.position = bounds.position
	rectangle.size = bounds.size
	rectangle.color = color
	parent.add_child(rectangle)
	return rectangle


func _viewport(size, spatial = false):
	var viewport = SubViewport.new()
	# Set transparency before allocation-affecting flags/size (also exercises GLES3 controls).
	viewport.transparent_bg = true
	viewport.size = Vector2i(size)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	viewport.own_world_3d = spatial
	viewport.disable_3d = not spatial
	add_child(viewport)
	return viewport


func _preview(parent, texture, bounds):
	var rectangle = TextureRect.new()
	rectangle.texture = texture
	rectangle.position = bounds.position
	rectangle.size = bounds.size
	rectangle.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	var material = CanvasItemMaterial.new()
	material.blend_mode = CanvasItemMaterial.BLEND_MODE_PREMULT_ALPHA
	rectangle.material = material
	parent.add_child(rectangle)


func _ready():
	OS.low_processor_usage_mode = false
	_rect(self, Rect2(0, 0, 400, 300), Color(0, 1, 0))
	_source = _viewport(Vector2(64, 48))
	_red = _rect(_source, Rect2(4, 4, 24, 40), Color(1, 0, 0, 0.5))
	_rect(_source, Rect2(20, 4, 24, 40), Color(0, 0, 1, 0.5))
	_nested = _viewport(Vector2(128, 96))
	_preview(_nested, _source.get_texture(), Rect2(0, 0, 128, 96))
	_preview(self, _nested.get_texture(), Rect2(0, 0, 128, 96))
	_spatial = _viewport(Vector2(64, 64), true)
	var world_environment = WorldEnvironment.new()
	_environment = Environment.new()
	_environment.background_mode = Environment.BG_COLOR
	_environment.background_color = Color(1, 0, 1)
	_environment.ambient_light_color = Color(1, 1, 1)
	_environment.ambient_light_energy = 0.25
	_environment.ambient_light_sky_contribution = 0
	world_environment.environment = _environment
	_spatial.add_child(world_environment)
	var camera = Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 4
	camera.position.z = 3
	_spatial.add_child(camera)
	_spatial.add_child(DirectionalLight3D.new())
	var mesh = MeshInstance3D.new()
	mesh.mesh = QuadMesh.new()
	mesh.mesh.size = Vector2(2, 2)
	var material = StandardMaterial3D.new()
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = Color(0.4, 0, 0, 0.5)
	material.emission_enabled = true
	material.emission = Color(0, 0.2, 0)
	_spatial_material = material
	mesh.material_override = material
	_spatial.add_child(mesh)
	_preview(self, _spatial.get_texture(), Rect2(160, 0, 128, 128))


func _process(_delta):
	if _failed:
		return
	_frames += 1
	if _frames == 12:
		_snapshot = _source.get_texture().get_image()
		if not _color(_snapshot, Vector2(2, 2), Color(0, 0, 0, 0)):
			return
		if not _color(_snapshot, Vector2(10, 20), Color(0.5, 0, 0, 0.5)):
			return
		if not _color(_snapshot, Vector2(24, 20), Color(0.25, 0, 0.5, 0.75)):
			return
		if not _color(_snapshot, Vector2(36, 20), Color(0, 0, 0.5, 0.5)):
			return
		if not _color(_nested.get_texture().get_image(), Vector2(48, 40), Color(0.25, 0, 0.5, 0.75)):
			return
		if not _color(get_viewport().get_texture().get_image(), Vector2(48, 40), Color(0.25, 0.25, 0.5, 1)):
			return
		if not _check(_snapshot.save_png("user://transparent_viewport.png") == OK, "alpha PNG export failed"):
			return
		var loaded = Image.new()
		if not _check(loaded.load("user://transparent_viewport.png") == OK, "alpha PNG reload failed"):
			return
		if not _color(loaded, Vector2(10, 20), Color(0.5, 0, 0, 0.5)):
			return
		# The transparent 3D viewport must retain surface coverage through its emission pass.
		var spatial_image = _spatial.get_texture().get_image()
		if not _color(spatial_image, Vector2(2, 2), Color(0, 0, 0, 0)):
			return
		var center = spatial_image.get_pixel(32, 32)
		if not _check(abs(center.a - 0.5) < 0.045 and center.r > 0.03 and center.g > 0.01, "3D emission changed coverage or failed to draw: %s" % center):
			return
		_source.transparent_bg = false
	if _frames == 18:
		if not _color(_source.get_texture().get_image(), Vector2(2, 2), Color(0.125, 0.25, 0.5, 1)):
			return
		_source.transparent_bg = true
		_spatial_material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_VERTEX
		var sky = Sky.new()
		var panorama = Image.create_empty(4, 2, false, Image.FORMAT_RGBA8)
		panorama.fill(Color(0, 0, 1))
		var sky_material = PanoramaSkyMaterial.new()
		sky_material.panorama = ImageTexture.create_from_image(panorama)
		sky.sky_material = sky_material
		_environment.sky = sky
		_environment.background_mode = Environment.BG_SKY
		_source.size = Vector2(80, 56)
		_red.color = Color(1, 1, 0, 0.5)
	if _frames == 24:
		if not _color(_source.get_texture().get_image(), Vector2(10, 20), Color(0.5, 0.5, 0, 0.5)):
			return
		if not _color(_snapshot, Vector2(10, 20), Color(0.5, 0, 0, 0.5)):
			return
		var vertex_image = _spatial.get_texture().get_image()
		if not _color(vertex_image, Vector2(2, 2), Color(0, 0, 0, 0)):
			return
		var vertex_center = vertex_image.get_pixel(32, 32)
		if not _check(abs(vertex_center.a - 0.5) < 0.045 and vertex_center.g > 0.01, "vertex-lit emission changed coverage: %s" % vertex_center):
			return
		print("TRANSPARENT VIEWPORT PASS: overlap, nested premultiplied composition, 3D emission, alpha readback/PNG, opaque transition, resize and independent snapshots")
		get_tree().quit(0)
