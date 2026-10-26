extends Spatial

var _frames = 0
var _failed = false
var _skeleton
var _instances = []
var _material
var _normal_setting


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("DEFORMED NORMAL SCENE FAIL: ", message)


func _mesh():
	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	var vertices = PoolVector3Array([
		Vector3(-0.8, -0.8, 0), Vector3(-0.8, 0.8, 0), Vector3(0.8, 0.8, 0),
		Vector3(-0.8, -0.8, 0), Vector3(0.8, 0.8, 0), Vector3(0.8, -0.8, 0)
	])
	var normals = PoolVector3Array()
	var tangents = PoolRealArray()
	var target_tangents = PoolRealArray()
	var uv = PoolVector2Array()
	var uv2 = PoolVector2Array()
	var colors = PoolColorArray()
	var bones = PoolIntArray()
	var weights = PoolRealArray()
	for vertex in vertices:
		normals.append(Vector3.BACK)
		uv.append((Vector2(vertex.x, vertex.y) + Vector2.ONE) * 0.5)
		uv2.append(Vector2(0.5, 0.5))
		colors.append(Color(0.8, 1, 0.7, 1))
		tangents.append_array(PoolRealArray([1, 0, 0, 1]))
		target_tangents.append_array(PoolRealArray([0, 1, 0, -1]))
		bones.append_array(PoolIntArray([0, 1, 0, 0]))
		weights.append_array(PoolRealArray([0.25, 0.75, 0, 0]))
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TANGENT] = tangents
	arrays[Mesh.ARRAY_TEX_UV] = uv
	arrays[Mesh.ARRAY_TEX_UV2] = uv2
	arrays[Mesh.ARRAY_COLOR] = colors
	arrays[Mesh.ARRAY_BONES] = bones
	arrays[Mesh.ARRAY_WEIGHTS] = weights
	var target = arrays.duplicate()
	target[Mesh.ARRAY_TANGENT] = target_tangents
	var mesh = ArrayMesh.new()
	mesh.add_blend_shape("Turn")
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [target], 0)
	return mesh


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().usage = Viewport.USAGE_3D
	_normal_setting = ProjectSettings.get_setting("rendering/quality/fixed_function/normal_mapping")
	ProjectSettings.set_setting("rendering/quality/fixed_function/normal_mapping", true)
	var image = Image.new()
	image.create(2, 2, false, Image.FORMAT_RGBA8)
	image.fill(Color(0.8, 0.65, 0))
	var normal = ImageTexture.new()
	normal.create_from_image(image, 0)
	image.fill(Color(1, 0.5, 1))
	var occlusion = ImageTexture.new()
	occlusion.create_from_image(image, 0)
	_material = SpatialMaterial.new()
	_material.albedo_color = Color(0.8, 0.7, 0.5)
	_material.vertex_color_use_as_albedo = true
	_material.params_cull_mode = SpatialMaterial.CULL_DISABLED
	_material.normal_enabled = true
	_material.normal_texture = normal
	_material.ao_on_uv2 = true
	_material.ao_enabled = true
	_material.ao_texture = occlusion
	_material.ao_texture_channel = SpatialMaterial.TEXTURE_CHANNEL_GREEN
	_material.ao_light_affect = 0.5
	_skeleton = Skeleton.new()
	_skeleton.name = "Skeleton"
	_skeleton.add_bone("Root")
	_skeleton.add_bone("Animated")
	add_child(_skeleton)
	var skin = Skin.new()
	skin.add_bind(0, Transform())
	skin.add_bind(1, Transform())
	var mesh = _mesh()
	for index in range(2):
		var instance = MeshInstance.new()
		instance.mesh = mesh
		instance.material_override = _material
		instance.translation = Vector3(index * 2.0 - 1.0, 0, 0)
		if index == 0:
			instance.skin = skin
			instance.skeleton = NodePath("../Skeleton")
		add_child(instance)
		_instances.append(instance)
	var camera = Camera.new()
	camera.translation.z = 5
	camera.current = true
	add_child(camera)
	var light = DirectionalLight.new()
	light.rotation_degrees = Vector3(-20, -30, 0)
	light.light_energy = 0.8
	add_child(light)
	var world = WorldEnvironment.new()
	world.environment = Environment.new()
	world.environment.background_mode = Environment.BG_COLOR
	world.environment.background_color = Color(0.08, 0.1, 0.15)
	world.environment.ambient_light_color = Color(1, 1, 1)
	world.environment.ambient_light_energy = 0.1
	add_child(world)
	var layer = CanvasLayer.new()
	add_child(layer)
	var label = Label.new()
	label.text = "Animated normal maps + AO (D3D9) | Geometry-normal fallback (D3D7/OpenGL1)"
	label.rect_position = Vector2(8, 8)
	layer.add_child(label)


func _process(_delta):
	_frames += 1
	var phase = _frames * 0.07
	_skeleton.set_bone_pose(1, Transform(Basis(Vector3.UP, sin(phase) * 0.4), Vector3()))
	for instance in _instances:
		instance.set("blend_shapes/Turn", (sin(phase) + 1) * 0.5)
	if _frames == 10:
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "skin and morph instances are visible")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == 2, "normal maps and AO use the base draws")
		_material.normal_enabled = false
	if _frames == 18:
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == 2, "standalone AO without a normal map uses the base draws")
		_material.normal_enabled = true
		ProjectSettings.set_setting("rendering/quality/fixed_function/normal_mapping", false)
	if _frames == 26:
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == 2, "normal-map quality fallback preserves standalone AO and geometry")
		ProjectSettings.set_setting("rendering/quality/fixed_function/normal_mapping", true)
		_instances[0].scale = Vector3(-1, 0.9, 1.1)
		OS.window_size = Vector2(480, 320)
	if _frames == 42:
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "instances survive material/quality edits, mirror and resize")
		_check(get_viewport().get_render_info(Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == 2, "combined effects still use one draw per instance")
		_check(_material.normal_enabled and _material.normal_texture != null, "CPU material data remains intact")
		if not _failed:
			print("DEFORMED NORMAL SCENE PASS driver=", OS.get_video_driver_name(OS.get_current_video_driver()))
		if not OS.get_cmdline_args().has("--smoke-hold"):
			ProjectSettings.set_setting("rendering/quality/fixed_function/normal_mapping", _normal_setting)
			get_tree().quit(1 if _failed else 0)
