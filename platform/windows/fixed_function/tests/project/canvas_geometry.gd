extends Node2D

var _mesh: ArrayMesh
var _copies: MultiMesh
var _frames = 0
var _failed = false
var _canvas_objects = 0


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("CANVAS GEOMETRY FAIL: ", message)


func _ready():
	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector2Array([Vector2(0, 0), Vector2(0, 22), Vector2(26, 0), Vector2(26, 22)])
	arrays[Mesh.ARRAY_COLOR] = PackedColorArray([Color.WHITE, Color.WHITE, Color.WHITE, Color.WHITE])
	_mesh = ArrayMesh.new()
	_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLE_STRIP, arrays)
	_copies = MultiMesh.new()
	_copies.transform_format = MultiMesh.TRANSFORM_2D
	_copies.use_colors = true
	_copies.use_custom_data = true
	_copies.mesh = _mesh
	_copies.instance_count = 2
	_copies.set_instance_transform_2d(0, Transform2D(0, Vector2(70, 28)))
	_copies.set_instance_transform_2d(1, Transform2D(0.25, Vector2(120, 28)))
	_copies.set_instance_color(0, Color.YELLOW)
	_copies.set_instance_color(1, Color.CYAN)
	_copies.set_instance_custom_data(1, Color.RED)
	_check(_copies.buffer.size() == 32, "two transforms with float color and custom data")
	_check(_copies.get_instance_transform_2d(1).origin == Vector2(120, 28), "2D transform round trip")
	_check(_copies.get_instance_color(1) == Color.CYAN and _copies.get_instance_custom_data(1) == Color.RED, "float channels round trip")
	var node = MultiMeshInstance2D.new()
	node.multimesh = _copies
	add_child(node)
	var particles = CPUParticles2D.new()
	particles.position = Vector2(165, 48)
	particles.amount = 4
	particles.gravity = Vector2.ZERO
	particles.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC
	var image = Image.create_empty(8, 8, false, Image.FORMAT_RGBA8)
	image.fill(Color.MAGENTA)
	image.generate_mipmaps()
	particles.texture = ImageTexture.create_from_image(image)
	add_child(particles)
	particles.emitting = true


func _draw():
	draw_mesh(_mesh, null, Transform2D(0, Vector2(18, 28)), Color.GREEN)
	draw_polyline(PackedVector2Array([Vector2(12, 82), Vector2(47, 69), Vector2(72, 85)]), Color.WHITE, -1.0)
	draw_polyline(PackedVector2Array([Vector2(92, 82), Vector2(127, 69), Vector2(152, 85)]), Color.ORANGE, 5.0, true)
	draw_multiline(PackedVector2Array([Vector2(12, 102), Vector2(47, 102), Vector2(59, 108), Vector2(72, 108)]), Color.CYAN, 2.0, true)


func _process(_delta):
	_frames += 1
	if _frames == 24:
		_canvas_objects = get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_CANVAS, Viewport.RENDER_INFO_OBJECTS_IN_FRAME)
		_check(_canvas_objects >= 6, "canvas mesh, MultiMesh, CPU particles and line commands, got %s" % _canvas_objects)
		_copies.visible_instance_count = 0
	if _frames == 36:
		var objects = get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_CANVAS, Viewport.RENDER_INFO_OBJECTS_IN_FRAME)
		_check(objects == _canvas_objects - 1, "hiding the MultiMesh removes just its canvas command, got %s" % objects)
		if not _failed:
			print("CANVAS GEOMETRY PASS")
		get_tree().quit(1 if _failed else 0)
