extends Node3D

var _multimesh
var _frames = 0
var _failed = false


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("MULTIMESH SCENE FAIL: ", message)


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().disable_3d = false
	var camera = Camera3D.new()
	camera.current = true
	camera.set_orthogonal(4, 0.1, 20)
	add_child(camera)

	var material = StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	var mesh = BoxMesh.new()
	mesh.size = Vector3(0.5, 0.5, 0.5)
	mesh.material = material

	_multimesh = MultiMesh.new()
	_multimesh.transform_format = MultiMesh.TRANSFORM_3D
	_multimesh.use_colors = true
	_multimesh.use_custom_data = true
	_multimesh.mesh = mesh
	_multimesh.instance_count = 2
	_multimesh.visible_instance_count = 1
	_multimesh.set_instance_transform(0, Transform3D(Basis(), Vector3(-0.8, 0.7, -3)))
	_multimesh.set_instance_transform(1, Transform3D(Basis(), Vector3(0.8, 0.7, -3)))
	_multimesh.set_instance_color(0, Color(1, 0, 0))
	_multimesh.set_instance_color(1, Color(0, 1, 0))
	_multimesh.set_instance_custom_data(1, Color(1, 0, 0, 1))
	var instances = MultiMeshInstance3D.new()
	instances.multimesh = _multimesh
	add_child(instances)

	var particles = CPUParticles3D.new()
	particles.amount = 3
	particles.lifetime = 4
	particles.explosiveness = 1.0
	particles.preprocess = 0.5
	particles.gravity = Vector3.ZERO
	particles.mesh = mesh
	particles.position = Vector3(0, -0.7, -3)
	particles.color = Color(0.2, 0.6, 1)
	add_child(particles)


func _process(_delta):
	_frames += 1
	var calls = get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
	if _frames == 16:
		_check(calls == 4, "one visible MultiMesh copy and three CPU particles, got %s draws" % calls)
		_check(_multimesh.get_instance_color(1) == Color(0, 1, 0), "float instance color round trip")
		_check(_multimesh.get_instance_custom_data(1) == Color(1, 0, 0, 1), "packed custom data round trip")
		_multimesh.visible_instance_count = -1
	if _frames == 28:
		_check(calls == 5, "both MultiMesh copies and three CPU particles, got %s draws" % calls)
		_multimesh.visible_instance_count = 0
	if _frames == 40:
		_check(calls == 3, "zero visible MultiMesh copies, got %s draws" % calls)
		if not _failed:
			print("MULTIMESH SCENE PASS: MultiMeshInstance and CPUParticles")
		get_tree().quit(1 if _failed else 0)
