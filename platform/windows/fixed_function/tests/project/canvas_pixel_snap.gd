extends Node2D

const LANE_WIDTH = 64

class CanvasSample extends Node2D:
	var texture
	var mesh
	var multimesh
	var reference = false

	func _draw():
		var offset = 0.25
		if reference:
			offset = 0.5
		draw_set_transform(Vector2(offset, offset), 0, Vector2.ONE)
		draw_texture_rect(texture, Rect2(3, 4, 23, 12), false)
		var points = PackedVector2Array([Vector2(33, 4), Vector2(57, 8), Vector2(35, 20)])
		if not reference:
			# The combined transforms make these round to the reference independently.
			points = PackedVector2Array([Vector2(33.375, 3.875), Vector2(56.875, 8.5), Vector2(35.5, 20.375)])
		draw_polygon(points,
				PackedColorArray([Color.WHITE]), PackedVector2Array([Vector2(0, 0), Vector2(1, 0), Vector2(0, 1)]), texture)
		draw_mesh(mesh, texture, Transform2D(0, Vector2(4, 25)))
		draw_multimesh(multimesh, texture)

		var item = get_canvas_item()
		var source = Rect2(0, 0, 8, 8)
		var margin = Vector2(2, 2)
		RenderingServer.canvas_item_add_nine_patch(item, Rect2(3, 44, 24, 12), source,
				texture.get_rid(), margin, margin, RenderingServer.NINE_PATCH_STRETCH, RenderingServer.NINE_PATCH_STRETCH)
		RenderingServer.canvas_item_add_nine_patch(item, Rect2(32, 44, 24, 12), source,
				texture.get_rid(), margin, margin, RenderingServer.NINE_PATCH_TILE, RenderingServer.NINE_PATCH_TILE)

		draw_line(Vector2(4, 63), Vector2(26, 67), Color.GREEN, -1.0, false)
		draw_polyline(PackedVector2Array([Vector2(33, 63), Vector2(44, 66), Vector2(55, 62)]), Color.GREEN, -1.0, false)
		draw_line(Vector2(4, 79), Vector2(27, 79), Color.GREEN, 4.0, false)
		draw_polyline(PackedVector2Array([Vector2(33, 79), Vector2(44, 79), Vector2(55, 79)]), Color.GREEN, 4.0, false)


var _target
var _captures = ""
var _failed = false


func _check(condition, message):
	if not condition and not _failed:
		_failed = true
		printerr("CANVAS PIXEL SNAP FAIL: ", message)
		get_tree().quit(1)
	return condition


func _ready():
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--shadow-capture="):
			_captures = argument.substr(17)
	_target = SubViewport.new()
	_target.size = Vector2i(128, 128)
	_target.disable_3d = true
	_target.world_2d = World2D.new()
	_target.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(_target)
	_target.canvas_transform = Transform2D(0, Vector2(0.25, 0.25))

	var image = Image.create_empty(8, 8, false, Image.FORMAT_RGBA8)
	image.fill(Color.RED)
	image.fill_rect(Rect2i(4, 0, 4, 8), Color.BLUE)
	var texture = ImageTexture.create_from_image(image)

	var mesh = ArrayMesh.new()
	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector2Array([Vector2(0, 0), Vector2(23, 4), Vector2(2, 12)])
	arrays[Mesh.ARRAY_TEX_UV] = PackedVector2Array([Vector2(0, 0), Vector2(1, 0), Vector2(0, 1)])
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_2D
	multimesh.mesh = mesh
	multimesh.instance_count = 1
	multimesh.set_instance_transform_2d(0, Transform2D(0, Vector2(34, 25)))

	for lane in range(2):
		var sample = CanvasSample.new()
		sample.texture = texture
		sample.mesh = mesh
		sample.multimesh = multimesh
		sample.reference = lane == 1
		sample.position = Vector2(lane * LANE_WIDTH + 0.25, 0.25)
		_target.add_child(sample)
	call_deferred("_run")


func _settle():
	for _i in range(4):
		await get_tree().process_frame


func _verify(name, snap):
	var image = _target.get_texture().get_image()
	if not _check(image != null and not image.is_empty() and image.get_size() == _target.size, "viewport readback"):
		return false
	if _captures != "":
		if not _check(image.save_png(_captures.path_join(name + ".png")) == OK, "save capture"):
			return false
	var regions = [
		["texture rectangle", Rect2(1, 2, 29, 18)],
		["polygon", Rect2(31, 2, 30, 21)],
		["mesh", Rect2(1, 24, 29, 17)],
		["multimesh", Rect2(31, 24, 30, 17)],
		["stretch nine-patch", Rect2(1, 43, 29, 16)],
		["tiled nine-patch", Rect2(31, 43, 30, 16)],
		["thin lines", Rect2(1, 60, 60, 12)],
		["wide lines", Rect2(1, 75, 60, 11)],
	]
	var rectangle_difference = 0.0
	for region in regions:
		var rect = region[1]
		var largest_delta = 0.0
		var visible = false
		for y in range(rect.position.y, rect.end.y):
			for x in range(rect.position.x, rect.end.x):
				var a = image.get_pixel(x, y)
				var b = image.get_pixel(x + LANE_WIDTH, y)
				largest_delta = max(largest_delta, max(abs(a.r - b.r), max(abs(a.g - b.g), abs(a.b - b.b))))
				visible = visible or max(a.r, max(a.g, a.b)) > 0.9
		if not _check(visible, "%s produced no visible pixels" % region[0]):
			return false
		if snap and not _check(largest_delta < 0.06, "%s differs from aligned reference by %.3f" % [region[0], largest_delta]):
			return false
		if region[0] == "texture rectangle":
			rectangle_difference = largest_delta
	if not snap and not _check(rectangle_difference > 0.08, "disabled setting should preserve fractional texture placement"):
		return false
	return true


func _run():
	await _settle()
	if not _verify("disabled", false):
		return
	_target.snap_2d_vertices_to_pixel = true
	await _settle()
	if not _verify("enabled", true):
		return
	_target.size = Vector2i(128, 96)
	await _settle()
	if not _verify("resized", true):
		return
	print("CANVAS PIXEL SNAP PASS: per-viewport flag, textures, polygons, meshes, nine-patches, lines and viewport resize.")
	get_tree().quit()
