extends Spatial

var _instance
var _frames = 0
var _failed = false


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("BLEND SHAPES SCENE FAIL: ", message)


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().usage = Viewport.USAGE_3D
	var camera = Camera.new()
	camera.name = "Camera"
	camera.translation.x = 12
	camera.current = true
	camera.set_orthogonal(4, 0.1, 20)
	add_child(camera)

	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PoolVector3Array([
		Vector3(5.4, -0.6, -3), Vector3(6, 0.6, -3), Vector3(6.6, -0.6, -3)
	])
	var target = []
	target.resize(Mesh.ARRAY_MAX)
	target[Mesh.ARRAY_VERTEX] = PoolVector3Array([Vector3(6, 0, 0), Vector3(6, 0, 0), Vector3(6, 0, 0)])
	var mesh = ArrayMesh.new()
	mesh.add_blend_shape("Slide")
	mesh.blend_shape_mode = Mesh.BLEND_SHAPE_MODE_RELATIVE
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [target])
	var material = SpatialMaterial.new()
	material.flags_unshaded = true
	material.params_cull_mode = SpatialMaterial.CULL_DISABLED
	mesh.surface_set_material(0, material)
	_instance = MeshInstance.new()
	_instance.mesh = mesh
	add_child(_instance)
	_instance.set("blend_shapes/Slide", 1.0)


func _process(_delta):
	_frames += 1
	if _frames == 12:
		_check(_instance.get("blend_shapes/Slide") == 1.0, "named instance weight was set")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 1, "relative morph extends beyond the packed source AABB")
		_instance.set("blend_shapes/Slide", -1.0)
		$Camera.translation.x = 0
	if _frames == 24:
		_check(_instance.get("blend_shapes/Slide") == -1.0, "negative instance weight was set")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 1, "negative relative morph draws")
		if not _failed:
			print("BLEND SHAPES SCENE PASS: named MeshInstance weights and relative bounds")
		get_tree().quit(1 if _failed else 0)
