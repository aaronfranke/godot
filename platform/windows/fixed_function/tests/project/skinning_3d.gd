extends Spatial

var _skeleton
var _frames = 0
var _failed = false


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("SKINNING SCENE FAIL: ", message)


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().usage = Viewport.USAGE_3D
	var camera = Camera.new()
	camera.current = true
	camera.set_orthogonal(4, 0.1, 20)
	add_child(camera)
	_skeleton = Skeleton.new()
	_skeleton.name = "Skeleton"
	_skeleton.add_bone("Root")
	add_child(_skeleton)

	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PoolVector3Array([
		Vector3(5.4, -0.6, -3), Vector3(6, 0.6, -3), Vector3(6.6, -0.6, -3)
	])
	arrays[Mesh.ARRAY_NORMAL] = PoolVector3Array([Vector3.BACK, Vector3.BACK, Vector3.BACK])
	arrays[Mesh.ARRAY_BONES] = PoolIntArray([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
	arrays[Mesh.ARRAY_WEIGHTS] = PoolRealArray([1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0])
	var mesh = ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var material = SpatialMaterial.new()
	material.flags_unshaded = true
	material.params_cull_mode = SpatialMaterial.CULL_DISABLED
	mesh.surface_set_material(0, material)
	var skin = Skin.new()
	skin.add_bind(0, Transform())
	var instance = MeshInstance.new()
	instance.mesh = mesh
	instance.skin = skin
	instance.skeleton = NodePath("../Skeleton")
	add_child(instance)


func _process(_delta):
	_frames += 1
	var visible_objects = get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME)
	if _frames == 8:
		_check(visible_objects == 0, "bind pose starts outside the camera")
		_skeleton.set_bone_pose(0, Transform(Basis(), Vector3(-6, 0, 0)))
	if _frames == 16:
		_check(visible_objects == 1, "changing the pose updates culling and draws the mesh")
		_skeleton.set_bone_pose(0, Transform())
	if _frames == 24:
		_check(visible_objects == 0, "returning to the bind pose updates culling again")
		if not _failed:
			print("SKINNING SCENE PASS: Skin, Skeleton, pose updates and camera culling")
		get_tree().quit(1 if _failed else 0)
