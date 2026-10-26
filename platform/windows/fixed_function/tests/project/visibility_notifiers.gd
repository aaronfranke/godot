extends Node3D

var _entered = 0
var _exited = 0
var _raw_entered = 0
var _raw_exited = 0


func _check(condition, message):
	if not condition:
		printerr("VISIBILITY NOTIFIER FAIL: ", message)
		get_tree().quit(1)
	return condition


func _frames(count = 6):
	for index in range(count):
		await get_tree().process_frame


func _enter():
	_entered += 1


func _exit():
	_exited += 1


func _raw_enter():
	_raw_entered += 1


func _raw_exit():
	_raw_exited += 1


func _ready():
	OS.low_processor_usage_mode = false
	var camera = Camera3D.new()
	camera.position = Vector3(0, 0, 5)
	camera.cull_mask = 1
	add_child(camera)
	camera.make_current()
	var cube = MeshInstance3D.new()
	cube.mesh = BoxMesh.new()
	var material = StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = Color(1, 0, 0)
	cube.material_override = material
	add_child(cube)
	var notifier = VisibleOnScreenNotifier3D.new()
	var bounds = AABB(Vector3(-0.5, -0.5, -0.5), Vector3.ONE)
	notifier.aabb = bounds
	notifier.screen_entered.connect(_enter)
	notifier.screen_exited.connect(_exit)
	add_child(notifier)
	var target = Node3D.new()
	target.name = "EnabledTarget"
	add_child(target)
	var enabler = VisibleOnScreenEnabler3D.new()
	enabler.enable_node_path = NodePath("../EnabledTarget")
	enabler.aabb = bounds
	add_child(enabler)
	await _frames()
	if not _check(notifier.is_on_screen() and _entered == 1 and _exited == 0, "initial camera entry"):
		return
	if not _check(target.process_mode == Node.PROCESS_MODE_INHERIT, "visible enabler must enable processing"):
		return
	await _frames()
	if not _check(_entered == 1 and _exited == 0, "stable visibility must not repeat signals"):
		return
	# Exercise a resource AABB edit independently of an instance transform edit.
	notifier.aabb = AABB(Vector3(1000, 0, 0), Vector3.ONE)
	enabler.position.x = 1000
	await _frames()
	if not _check(not notifier.is_on_screen() and _exited == 1, "bounds change must update culling"):
		return
	if not _check(target.process_mode == Node.PROCESS_MODE_DISABLED, "offscreen enabler must disable processing"):
		return
	notifier.aabb = bounds
	enabler.position.x = 0
	await _frames()
	if not _check(notifier.is_on_screen() and _entered == 2 and target.process_mode == Node.PROCESS_MODE_INHERIT, "re-entry"):
		return
	notifier.layers = 2
	await _frames()
	if not _check(not notifier.is_on_screen() and _exited == 2, "camera layer filtering"):
		return
	camera.cull_mask = 3
	await _frames()
	if not _check(notifier.is_on_screen() and _entered == 3, "camera mask change"):
		return
	notifier.hide()
	await _frames()
	if not _check(not notifier.is_on_screen() and _exited == 3, "hidden notifier"):
		return
	notifier.queue_free()
	enabler.queue_free()
	await _frames()
	# Free a server-owned base while its instance is still attached to the scenario.
	var resource = RenderingServer.visibility_notifier_create()
	var instance = RenderingServer.instance_create()
	RenderingServer.visibility_notifier_set_aabb(resource, bounds)
	RenderingServer.visibility_notifier_set_callbacks(resource, _raw_enter, _raw_exit)
	RenderingServer.instance_set_base(instance, resource)
	RenderingServer.instance_set_scenario(instance, get_world_3d().scenario)
	await _frames()
	if not _check(_raw_entered == 1 and _raw_exited == 0, "server notifier entry"):
		return
	RenderingServer.instance_set_transform(instance, Transform3D(Basis(), Vector3(1000, 0, 0)))
	await _frames()
	if not _check(_raw_exited == 1, "server notifier exit"):
		return
	RenderingServer.instance_set_transform(instance, Transform3D())
	await _frames()
	if not _check(_raw_entered == 2, "server notifier re-entry"):
		return
	# Deletion must detach a currently visible base, including its visible-list entry.
	RenderingServer.free_rid(resource)
	await _frames()
	RenderingServer.free_rid(instance)
	await RenderingServer.frame_post_draw
	var image = get_viewport().get_texture().get_image()
	if not _check(image != null and not image.is_empty(), "ordinary geometry must still render"):
		return
	var pixel = image.get_pixel(image.get_width() / 2, image.get_height() / 2)
	if not _check(pixel.r > 0.9 and pixel.g < 0.1 and pixel.b < 0.1, "notifiers must not suppress the cube"):
		return
	print("VISIBILITY NOTIFIER PASS: camera entry/exit, bounds, layers, visibility, process enabling, server deletion and rendered geometry")
	get_tree().quit()
