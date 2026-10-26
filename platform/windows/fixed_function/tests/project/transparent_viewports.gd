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
	image.lock()
	# Godot 3.x viewport readback is flipped unless render_target_v_flip is enabled.
	var actual = image.get_pixel(int(point.x), image.get_height() - 1 - int(point.y))
	image.unlock()
	for channel in range(4):
		if not _check(abs(actual[channel] - expected[channel]) < 0.045, "pixel %s: got %s expected %s" % [point, actual, expected]):
			return false
	return true


func _rect(parent, bounds, color):
	var rectangle = ColorRect.new()
	rectangle.rect_position = bounds.position
	rectangle.rect_size = bounds.size
	rectangle.color = color
	parent.add_child(rectangle)
	return rectangle


func _viewport(size, spatial = false):
	var viewport = Viewport.new()
	# Set transparency before allocation-affecting flags/size (also exercises GLES3 controls).
	viewport.transparent_bg = true
	viewport.usage = Viewport.USAGE_3D if spatial else Viewport.USAGE_2D
	viewport.hdr = false
	viewport.size = size
	viewport.render_target_update_mode = Viewport.UPDATE_ALWAYS
	viewport.own_world = spatial
	add_child(viewport)
	return viewport


func _preview(parent, texture, bounds):
	var rectangle = TextureRect.new()
	rectangle.texture = texture
	rectangle.rect_position = bounds.position
	rectangle.rect_size = bounds.size
	rectangle.expand = true
	var material = CanvasItemMaterial.new()
	material.blend_mode = CanvasItemMaterial.BLEND_MODE_PREMULT_ALPHA
	rectangle.material = material
	parent.add_child(rectangle)


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().hdr = false
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
	var camera = Camera.new()
	camera.projection = Camera.PROJECTION_ORTHOGONAL
	camera.size = 4
	camera.translation.z = 3
	_spatial.add_child(camera)
	_spatial.add_child(DirectionalLight.new())
	var mesh = MeshInstance.new()
	mesh.mesh = QuadMesh.new()
	mesh.mesh.size = Vector2(2, 2)
	var material = SpatialMaterial.new()
	material.flags_transparent = true
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
		_snapshot = _source.get_texture().get_data()
		if not _color(_snapshot, Vector2(2, 2), Color(0, 0, 0, 0)):
			return
		if not _color(_snapshot, Vector2(10, 20), Color(0.5, 0, 0, 0.5)):
			return
		if not _color(_snapshot, Vector2(24, 20), Color(0.25, 0, 0.5, 0.75)):
			return
		if not _color(_snapshot, Vector2(36, 20), Color(0, 0, 0.5, 0.5)):
			return
		if not _color(_nested.get_texture().get_data(), Vector2(48, 40), Color(0.25, 0, 0.5, 0.75)):
			return
		if not _color(get_viewport().get_texture().get_data(), Vector2(48, 40), Color(0.25, 0.25, 0.5, 1)):
			return
		if not _check(_snapshot.save_png("user://transparent_viewport.png") == OK, "alpha PNG export failed"):
			return
		var loaded = Image.new()
		if not _check(loaded.load("user://transparent_viewport.png") == OK, "alpha PNG reload failed"):
			return
		if not _color(loaded, Vector2(10, 20), Color(0.5, 0, 0, 0.5)):
			return
		# The transparent 3D viewport must retain surface coverage through its emission pass.
		var spatial_image = _spatial.get_texture().get_data()
		if not _color(spatial_image, Vector2(2, 2), Color(0, 0, 0, 0)):
			return
		spatial_image.lock()
		var center = spatial_image.get_pixel(32, 32)
		spatial_image.unlock()
		if not _check(abs(center.a - 0.5) < 0.045 and center.r > 0.03 and center.g > 0.01, "3D emission changed coverage or failed to draw: %s" % center):
			return
		_source.transparent_bg = false
	if _frames == 18:
		if not _color(_source.get_texture().get_data(), Vector2(2, 2), Color(0.125, 0.25, 0.5, 1)):
			return
		_source.transparent_bg = true
		_spatial_material.flags_vertex_lighting = true
		var sky = PanoramaSky.new()
		var panorama = Image.new()
		panorama.create(4, 2, false, Image.FORMAT_RGBA8)
		panorama.fill(Color(0, 0, 1))
		var sky_texture = ImageTexture.new()
		sky_texture.create_from_image(panorama, 0)
		sky.panorama = sky_texture
		_environment.background_sky = sky
		_environment.background_mode = Environment.BG_SKY
		_source.size = Vector2(80, 56)
		_red.color = Color(1, 1, 0, 0.5)
	if _frames == 24:
		if not _color(_source.get_texture().get_data(), Vector2(10, 20), Color(0.5, 0.5, 0, 0.5)):
			return
		if not _color(_snapshot, Vector2(10, 20), Color(0.5, 0, 0, 0.5)):
			return
		var vertex_image = _spatial.get_texture().get_data()
		if not _color(vertex_image, Vector2(2, 2), Color(0, 0, 0, 0)):
			return
		vertex_image.lock()
		var vertex_center = vertex_image.get_pixel(32, 32)
		vertex_image.unlock()
		if not _check(abs(vertex_center.a - 0.5) < 0.045 and vertex_center.g > 0.01, "vertex-lit emission changed coverage: %s" % vertex_center):
			return
		print("TRANSPARENT VIEWPORT PASS: overlap, nested premultiplied composition, 3D emission, alpha readback/PNG, opaque transition, resize and independent snapshots")
		get_tree().quit(0)
