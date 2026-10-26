extends Control

export(bool) var use_immediate_geometry = false

var _frames = 0
var _failed = false
var _viewport
var _camera
var _sun
var _cube
var _floor
var _material
var _shadowed
var _unshadowed
var _sample
var _has_shadows
var _previous_shadows
var _previous_retained
var _lit = 0.0
var _shadow_world
var _skeleton
var _deformed
var _caster_material
var _caster_mesh
var _source_vertices
var _before_emission


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("DIRECTIONAL SHADOW SCENE FAIL: ", message)
		get_tree().quit(1)
	return condition


func _image():
	return _viewport.get_texture().get_data()


func _color(image, point):
	image.lock()
	var color = image.get_pixel(int(point.x), int(point.y))
	image.unlock()
	return color


func _pixel(image, point):
	var color = _color(image, point)
	return (color.r + color.g + color.b) / 3.0


func _save(image, suffix):
	for argument in OS.get_cmdline_args():
		if argument.begins_with("--shadow-capture="):
			image.save_png(argument.substr(17).plus_file(suffix + ".png"))


func _texture(color, split_alpha = false):
	var image = Image.new()
	image.create(4, 4, false, Image.FORMAT_RGBA8)
	image.fill(color)
	if split_alpha:
		image.lock()
		for y in range(4):
			for x in range(2):
				image.set_pixel(x, y, Color(1, 1, 1, 0))
		image.unlock()
	var texture = ImageTexture.new()
	texture.create_from_image(image, 0)
	return texture


func _animated_caster():
	_cube.hide()
	_skeleton = Skeleton.new()
	_skeleton.name = "ShadowSkeleton"
	_skeleton.translation.y = 1.5
	_skeleton.add_bone("Root")
	_viewport.add_child(_skeleton)
	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	var vertices = PoolVector3Array([Vector3(-0.6, 0, -0.6), Vector3(-0.6, 0, 0.6), Vector3(0.6, 0, 0.6), Vector3(0.6, 0, -0.6)])
	var normals = PoolVector3Array()
	var uv = PoolVector2Array()
	var bones = PoolIntArray()
	var weights = PoolRealArray()
	var target = PoolVector3Array()
	for vertex in vertices:
		normals.append(Vector3.UP)
		uv.append(Vector2(vertex.x / 1.2 + 0.5, vertex.z / 1.2 + 0.5))
		bones.append_array(PoolIntArray([0, 0, 0, 0]))
		weights.append_array(PoolRealArray([1, 0, 0, 0]))
		target.append(vertex + Vector3(2, 0, 0))
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TEX_UV] = uv
	arrays[Mesh.ARRAY_BONES] = bones
	arrays[Mesh.ARRAY_WEIGHTS] = weights
	arrays[Mesh.ARRAY_INDEX] = PoolIntArray([0, 1, 2, 0, 2, 3])
	var morph = arrays.duplicate()
	morph[Mesh.ARRAY_VERTEX] = target
	morph[Mesh.ARRAY_INDEX] = null
	_caster_mesh = ArrayMesh.new()
	_caster_mesh.add_blend_shape("Shift")
	_caster_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [morph], 0)
	_source_vertices = _caster_mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
	var skin = Skin.new()
	skin.add_bind(0, Transform())
	_deformed = MeshInstance.new()
	_deformed.mesh = _caster_mesh
	# Explicit input: 3.x instance_set_base does not initialize new morph weights.
	_deformed.set("blend_shapes/Shift", 0.0)
	_deformed.skin = skin
	_deformed.skeleton = NodePath("../ShadowSkeleton")
	_deformed.translation.y = 1.5
	_deformed.cast_shadow = GeometryInstance.SHADOW_CASTING_SETTING_SHADOWS_ONLY
	_caster_material = SpatialMaterial.new()
	_caster_material.params_cull_mode = SpatialMaterial.CULL_DISABLED
	_deformed.material_override = _caster_material
	_viewport.add_child(_deformed)


func _expect_shadow(point, message):
	var value = _pixel(_image(), _camera.unproject_position(point))
	return _check(value < _lit * 0.6 if _has_shadows else abs(value - _lit) < 0.04, message)


func _expect_lit(point, message):
	return _check(abs(_pixel(_image(), _camera.unproject_position(point)) - _lit) < 0.04, message)


func _immediate_from_mesh(mesh):
	var arrays = mesh.surface_get_arrays(0)
	var geometry = ImmediateGeometry.new()
	geometry.begin(Mesh.PRIMITIVE_TRIANGLES)
	for index in arrays[Mesh.ARRAY_INDEX]:
		geometry.set_normal(arrays[Mesh.ARRAY_NORMAL][index])
		var tangent = arrays[Mesh.ARRAY_TANGENT]
		geometry.set_tangent(Plane(Vector3(tangent[index * 4], tangent[index * 4 + 1], tangent[index * 4 + 2]), tangent[index * 4 + 3]))
		geometry.set_uv(arrays[Mesh.ARRAY_TEX_UV][index])
		geometry.add_vertex(arrays[Mesh.ARRAY_VERTEX][index])
	geometry.end()
	return geometry


func _ready():
	OS.low_processor_usage_mode = false
	_previous_shadows = ProjectSettings.get_setting("rendering/quality/fixed_function/directional_shadows")
	_previous_retained = ProjectSettings.get_setting("rendering/quality/fixed_function/retained_geometry")
	ProjectSettings.set_setting("rendering/quality/fixed_function/directional_shadows", true)
	ProjectSettings.set_setting("rendering/quality/directional_shadow/size", 512)
	_has_shadows = OS.get_video_driver_name(OS.get_current_video_driver()) == "D3D9"
	get_viewport().usage = Viewport.USAGE_2D
	_viewport = Viewport.new()
	_viewport.size = Vector2(512, 384)
	_viewport.usage = Viewport.USAGE_3D
	_viewport.own_world = true
	_viewport.hdr = false
	_viewport.render_target_v_flip = true
	_viewport.render_target_update_mode = Viewport.UPDATE_ALWAYS
	_viewport.shadow_atlas_size = 1024
	add_child(_viewport)
	var preview = TextureRect.new()
	preview.texture = _viewport.get_texture()
	preview.rect_size = _viewport.size
	add_child(preview)
	_material = SpatialMaterial.new()
	_material.albedo_color = Color(0.8, 0.8, 0.8)
	_material.params_cull_mode = SpatialMaterial.CULL_DISABLED
	var plane = PlaneMesh.new()
	plane.size = Vector2(14, 14)
	if use_immediate_geometry:
		_floor = _immediate_from_mesh(plane)
	else:
		_floor = MeshInstance.new()
		_floor.mesh = plane
	_floor.material_override = _material
	_viewport.add_child(_floor)
	var cube_mesh = CubeMesh.new()
	cube_mesh.size = Vector3.ONE
	if use_immediate_geometry:
		_cube = _immediate_from_mesh(cube_mesh)
	else:
		_cube = MeshInstance.new()
		_cube.mesh = cube_mesh
	_cube.translation = Vector3(0, 1.5, 0)
	_cube.material_override = _material.duplicate()
	_viewport.add_child(_cube)
	_camera = Camera.new()
	_camera.translation = Vector3(4, 5, -6)
	_camera.far = 30
	_viewport.add_child(_camera)
	_camera.look_at(Vector3.ZERO, Vector3.UP)
	_camera.current = true
	_sun = DirectionalLight.new()
	_sun.rotation_degrees = Vector3(-45, -25, 0)
	_sun.light_energy = 0.8
	_sun.shadow_enabled = true
	_sun.shadow_bias = 0.1
	_sun.directional_shadow_normal_bias = 0.1
	_sun.directional_shadow_max_distance = 20
	# Preserve this requested default while executing the supported single-map profile.
	_sun.directional_shadow_mode = DirectionalLight.SHADOW_PARALLEL_4_SPLITS
	_viewport.add_child(_sun)
	var environment = WorldEnvironment.new()
	environment.environment = Environment.new()
	environment.environment.background_mode = Environment.BG_COLOR
	environment.environment.background_color = Color(0.05, 0.07, 0.1)
	environment.environment.ambient_light_color = Color(1, 1, 1)
	environment.environment.ambient_light_energy = 0.12
	_viewport.add_child(environment)
	var toward = _sun.transform.basis.z.normalized()
	_shadow_world = _cube.translation - toward * (_cube.translation.y / toward.y)
	_sample = _camera.unproject_position(_shadow_world)
	print("DIRECTIONAL SHADOW SAMPLE: ", _sample, " world=", _shadow_world)


func _process(_delta):
	if _failed:
		return
	_frames += 1
	if _frames == 8:
		_shadowed = _image()
		if not _check(_shadowed != null and not _shadowed.is_empty(), "completed shadowed viewport"):
			return
		_save(_shadowed, "shadowed")
		_sun.shadow_enabled = false
	if _frames == 14:
		_unshadowed = _image()
		_save(_unshadowed, "unshadowed")
		var lit = _pixel(_unshadowed, _sample)
		_lit = lit
		var shadow = _pixel(_shadowed, _sample)
		print("DIRECTIONAL SHADOW VALUES: on=", shadow, " off=", lit)
		if not _check(lit > 0.4, "ordinary floor lighting"):
			return
		if _has_shadows:
			if not _check(shadow < lit * 0.6 and shadow > 0.04, "directional shadow must remove direct light and preserve ambient"):
				return
		elif not _check(abs(shadow - lit) < 0.04, "unsupported backend must retain unshadowed lighting"):
			return
		_sun.shadow_enabled = true
		_material.flags_do_not_receive_shadows = true
	if _frames == 20:
		if not _check(abs(_pixel(_image(), _sample) - _pixel(_unshadowed, _sample)) < 0.04, "material receive-shadow opt-out"):
			return
		_material.flags_do_not_receive_shadows = false
		ProjectSettings.set_setting("rendering/quality/fixed_function/directional_shadows", false)
	if _frames == 26:
		if not _check(abs(_pixel(_image(), _sample) - _pixel(_unshadowed, _sample)) < 0.04, "global shadow opt-out must not reuse stale contents"):
			return
		ProjectSettings.set_setting("rendering/quality/fixed_function/directional_shadows", true)
		_cube.cast_shadow = GeometryInstance.SHADOW_CASTING_SETTING_OFF
	if _frames == 32:
		if not _check(abs(_pixel(_image(), _sample) - _pixel(_unshadowed, _sample)) < 0.04, "empty caster map must clear old occlusion"):
			return
		_cube.cast_shadow = GeometryInstance.SHADOW_CASTING_SETTING_SHADOWS_ONLY
	if _frames == 38:
		var value = _pixel(_image(), _sample)
		if _has_shadows and not _check(value < _pixel(_unshadowed, _sample) * 0.6, "shadow-only caster"):
			return
		_material.albedo_texture = _texture(Color(1, 1, 1))
		_material.normal_enabled = true
		_material.normal_texture = _texture(Color(0.5, 0.5, 1))
	if _frames == 44:
		if not _expect_shadow(_shadow_world, "normal-mapped textured receiver"):
			return
		_material.ao_enabled = true
		_material.ao_texture = _texture(Color(1, 1, 1))
		ProjectSettings.set_setting("rendering/quality/fixed_function/retained_geometry", false)
	if _frames == 50:
		if not _expect_shadow(_shadow_world, "combined normal/AO receiver"):
			return
		_material.normal_enabled = false
	if _frames == 56:
		if not _expect_shadow(_shadow_world, "standalone AO receiver"):
			return
		_material.ao_enabled = false
	if _frames == 62:
		if not _expect_shadow(_shadow_world, "plain textured receiver"):
			return
		ProjectSettings.set_setting("rendering/quality/fixed_function/retained_geometry", _previous_retained)
		_animated_caster()
	if _frames == 68:
		_save(_image(), "animated_rest")
		if not _expect_shadow(_shadow_world, "skinned caster in rest pose"):
			return
		_skeleton.set_bone_pose(0, Transform(Basis(), Vector3(2, 0, 0)))
	if _frames == 74:
		if not _expect_shadow(_shadow_world + Vector3(2, 0, 0), "skinned shadow follows bone pose") or not _expect_lit(_shadow_world, "bone pose clears old shadow"):
			return
		_skeleton.set_bone_pose(0, Transform())
		_deformed.set("blend_shapes/Shift", 1.0)
	if _frames == 80:
		if not _expect_shadow(_shadow_world + Vector3(2, 0, 0), "morphed shadow follows positions") or not _expect_lit(_shadow_world, "morph clears old shadow"):
			return
		_caster_material.params_use_alpha_scissor = true
		_caster_material.params_alpha_scissor_threshold = 0.5
		_caster_material.albedo_texture = _texture(Color(1, 1, 1), true)
	if _frames == 86:
		if not _expect_lit(_shadow_world + Vector3(1.7, 0, 0), "cutout hole transmits sunlight") or not _expect_shadow(_shadow_world + Vector3(2.3, 0, 0), "opaque cutout texels cast"):
			return
		_save(_image(), "cutout")
		_caster_material.uv1_offset.x = 0.5
	if _frames == 92:
		if not _expect_shadow(_shadow_world + Vector3(1.7, 0, 0), "caster coverage honors UV transforms"):
			return
		_caster_material.params_use_alpha_scissor = false
		_caster_material.flags_transparent = true
	if _frames == 98:
		if not _expect_lit(_shadow_world + Vector3(1.7, 0, 0), "ordinary blended material does not cast opaque shadows"):
			return
		_caster_material.flags_transparent = false
		_caster_material.params_use_alpha_scissor = true
		_material.flags_vertex_lighting = true
	if _frames == 104:
		if not _expect_lit(_shadow_world + Vector3(1.7, 0, 0), "explicit vertex-lighting receiver stays unshadowed"):
			return
		_material.flags_vertex_lighting = false
		_viewport.size = Vector2(400, 300)
		ProjectSettings.set_setting("rendering/quality/directional_shadow/size", 256)
		OS.window_size = Vector2(700, 500)
	if _frames == 112:
		if not _expect_shadow(_shadow_world + Vector3(1.7, 0, 0), "map resize and native target recovery"):
			return
		if not _check(_caster_mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX] == _source_vertices, "deformation must preserve CPU source mesh"):
			return
		_save(_image(), "recovered")
		_sun.shadow_color = Color(0.3, 0, 0)
	if _frames == 118:
		_before_emission = _color(_image(), _camera.unproject_position(_shadow_world + Vector3(1.7, 0, 0)))
		if _has_shadows and not _check(_before_emission.r > _before_emission.g + 0.08 and abs(_before_emission.g - _before_emission.b) < 0.02, "colored shadow affects direct light only"):
			return
		_material.emission_enabled = true
		_material.emission = Color(0.2, 0.2, 0.2)
	if _frames == 124:
		var after = _color(_image(), _camera.unproject_position(_shadow_world + Vector3(1.7, 0, 0)))
		var delta = after - _before_emission
		if OS.get_video_driver_name(OS.get_current_video_driver()) != "D3D3 Software":
			if not _check(abs(delta.r - 0.2) < 0.04 and abs(delta.g - 0.2) < 0.04 and abs(delta.b - 0.2) < 0.04, "emission remains unshadowed"):
				return
		ProjectSettings.set_setting("rendering/quality/fixed_function/directional_shadows", _previous_shadows)
		ProjectSettings.set_setting("rendering/quality/fixed_function/retained_geometry", _previous_retained)
		print("DIRECTIONAL SHADOW SCENE PASS: direct/ambient/emission, cascade approximation, fallback, retained/streamed channels, skin/morphs, cutout UVs and recovery")
		get_tree().quit()
