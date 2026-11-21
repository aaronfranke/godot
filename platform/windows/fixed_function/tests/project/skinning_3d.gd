extends Node3D

var _skeleton
var _instance
var _eight_weight_mesh
var _frames = 0
var _failed = false


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("SKINNING SCENE FAIL: ", message)


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().disable_3d = false
	var camera = Camera3D.new()
	camera.current = true
	camera.set_orthogonal(4, 0.1, 20)
	add_child(camera)
	_skeleton = Skeleton3D.new()
	_skeleton.name = "Skeleton"
	_skeleton.add_bone("Root")
	_skeleton.add_bone("Second")
	add_child(_skeleton)

	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([
		Vector3(5.4, -0.6, -3), Vector3(6, 0.6, -3), Vector3(6.6, -0.6, -3)
	])
	arrays[Mesh.ARRAY_NORMAL] = PackedVector3Array([Vector3.BACK, Vector3.BACK, Vector3.BACK])
	arrays[Mesh.ARRAY_BONES] = PackedInt32Array([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
	arrays[Mesh.ARRAY_WEIGHTS] = PackedFloat32Array([1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0])
	var mesh = ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var material = StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	mesh.surface_set_material(0, material)
	var skin = Skin.new()
	skin.add_bind(0, Transform3D())
	skin.add_bind(1, Transform3D())
	_instance = MeshInstance3D.new()
	_instance.mesh = mesh
	_instance.skin = skin
	_instance.skeleton = NodePath("../Skeleton")
	add_child(_instance)

	var bones = PackedInt32Array()
	var weights = PackedFloat32Array()
	for i in range(3):
		bones.append_array(PackedInt32Array([0, 0, 0, 0, 0, 0, 0, 1]))
		weights.append_array(PackedFloat32Array([0, 0, 0, 0, 0, 0, 0, 1]))
	arrays[Mesh.ARRAY_BONES] = bones
	arrays[Mesh.ARRAY_WEIGHTS] = weights
	_eight_weight_mesh = ArrayMesh.new()
	_eight_weight_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, Mesh.ARRAY_FLAG_USE_8_BONE_WEIGHTS)
	_eight_weight_mesh.surface_set_material(0, material)


func _process(_delta):
	_frames += 1
	var visible_objects = get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME)
	if _frames == 8:
		_check(visible_objects == 0, "bind pose starts outside the camera")
		_skeleton.set_bone_pose_position(0, Vector3(-6, 0, 0))
	if _frames == 16:
		_check(visible_objects == 1, "changing the pose updates culling and draws the mesh")
		_skeleton.set_bone_pose_position(0, Vector3.ZERO)
	if _frames == 24:
		_check(visible_objects == 0, "returning to the bind pose updates culling again")
		_instance.mesh = _eight_weight_mesh
		_skeleton.set_bone_pose_position(1, Vector3(-6, 0, 0))
	if _frames == 32:
		_check(visible_objects == 1, "a weight in the second group of four influences draws the mesh")
		if not _failed:
			print("SKINNING SCENE PASS: four/eight influence streams, pose updates and camera culling")
		get_tree().quit(1 if _failed else 0)
