extends Control

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
var _second_viewport
var _second_camera


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("DIRECTIONAL SHADOW SCENE FAIL: ", message)
		get_tree().quit(1)
	return condition


func _image():
	return _viewport.get_texture().get_image()


func _color(image, point):
	var color = image.get_pixel(int(point.x), int(point.y))
	return color


func _pixel(image, point):
	var color = _color(image, point)
	return (color.r + color.g + color.b) / 3.0


func _save(image, suffix):
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--shadow-capture="):
			image.save_png(argument.substr(17).path_join(suffix + ".png"))


func _texture(color, split_alpha = false):
	var image = Image.create_empty(4, 4, false, Image.FORMAT_RGBA8)
	image.fill(color)
	if split_alpha:
		for y in range(4):
			for x in range(2):
				image.set_pixel(x, y, Color(1, 1, 1, 0))
	var texture = ImageTexture.create_from_image(image)
	return texture


func _animated_caster():
	_cube.hide()
	_skeleton = Skeleton3D.new()
	_skeleton.name = "ShadowSkeleton"
	_skeleton.position.y = 1.5
	_skeleton.add_bone("Root")
	_viewport.add_child(_skeleton)
	var arrays = []
	arrays.resize(Mesh.ARRAY_MAX)
	var vertices = PackedVector3Array([Vector3(-0.6, 0, -0.6), Vector3(-0.6, 0, 0.6), Vector3(0.6, 0, 0.6), Vector3(0.6, 0, -0.6)])
	var normals = PackedVector3Array()
	var uv = PackedVector2Array()
	var bones = PackedInt32Array()
	var weights = PackedFloat32Array()
	var target = PackedVector3Array()
	for vertex in vertices:
		normals.append(Vector3.UP)
		uv.append(Vector2(vertex.x / 1.2 + 0.5, vertex.z / 1.2 + 0.5))
		bones.append_array(PackedInt32Array([0, 0, 0, 0]))
		weights.append_array(PackedFloat32Array([1, 0, 0, 0]))
		target.append(vertex + Vector3(2, 0, 0))
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TEX_UV] = uv
	arrays[Mesh.ARRAY_BONES] = bones
	arrays[Mesh.ARRAY_WEIGHTS] = weights
	arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2, 0, 2, 3])
	var morph = []
	morph.resize(Mesh.ARRAY_MAX)
	morph[Mesh.ARRAY_VERTEX] = target
	morph[Mesh.ARRAY_NORMAL] = normals
	_caster_mesh = ArrayMesh.new()
	_caster_mesh.add_blend_shape("Shift")
	_caster_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [morph], {}, 0)
	_source_vertices = _caster_mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
	var skin = Skin.new()
	skin.add_bind(0, Transform3D())
	_deformed = MeshInstance3D.new()
	_deformed.mesh = _caster_mesh
	# Keep the initial rest-pose input explicit on both branches.
	_deformed.set_blend_shape_value(0, 0.0)
	_deformed.skin = skin
	_deformed.skeleton = NodePath("../ShadowSkeleton")
	_deformed.position.y = 1.5
	_deformed.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
	_caster_material = StandardMaterial3D.new()
	_material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	_caster_material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	_caster_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	_deformed.material_override = _caster_material
	_viewport.add_child(_deformed)


func _expect_shadow(point, message):
	var value = _pixel(_image(), _camera.unproject_position(point))
	return _check(value < _lit * 0.6 if _has_shadows else abs(value - _lit) < 0.04, message)


func _expect_lit(point, message):
	return _check(abs(_pixel(_image(), _camera.unproject_position(point)) - _lit) < 0.04, message)


func _ready():
	OS.low_processor_usage_mode = false
	_previous_shadows = ProjectSettings.get_setting("rendering/quality/fixed_function/directional_shadows")
	_previous_retained = ProjectSettings.get_setting("rendering/quality/fixed_function/retained_geometry")
	ProjectSettings.set_setting("rendering/quality/fixed_function/directional_shadows", true)
	RenderingServer.directional_shadow_atlas_set_size(512, false)
	_has_shadows = RenderingServer.get_current_rendering_driver_name() == "d3d9"
	get_viewport().disable_3d = true
	_viewport = SubViewport.new()
	_viewport.size = Vector2(512, 384)
	_viewport.own_world_3d = true
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_viewport.positional_shadow_atlas_size = 1024
	add_child(_viewport)
	var preview = TextureRect.new()
	preview.texture = _viewport.get_texture()
	preview.size = _viewport.size
	add_child(preview)
	_material = StandardMaterial3D.new()
	_material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	_material.albedo_color = Color(0.8, 0.8, 0.8)
	_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	_floor = MeshInstance3D.new()
	var plane = PlaneMesh.new()
	plane.size = Vector2(14, 14)
	_floor.mesh = plane
	_floor.material_override = _material
	_floor.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_viewport.add_child(_floor)
	_cube = MeshInstance3D.new()
	_cube.mesh = BoxMesh.new()
	_cube.mesh.size = Vector3.ONE
	_cube.position = Vector3(0, 1.5, 0)
	_cube.material_override = _material.duplicate()
	_viewport.add_child(_cube)
	_camera = Camera3D.new()
	_camera.position = Vector3(4, 5, -6)
	_camera.far = 30
	_viewport.add_child(_camera)
	_camera.look_at(Vector3.ZERO, Vector3.UP)
	_camera.current = true
	_sun = DirectionalLight3D.new()
	_sun.rotation_degrees = Vector3(-45, -25, 0)
	_sun.light_energy = 0.8
	_sun.shadow_enabled = true
	_sun.shadow_bias = 0.02
	_sun.shadow_normal_bias = 1.0
	_sun.directional_shadow_max_distance = 20
	# Preserve this requested default while executing the supported single-map profile.
	_sun.directional_shadow_mode = DirectionalLight3D.SHADOW_PARALLEL_4_SPLITS
	_viewport.add_child(_sun)
	var environment = WorldEnvironment.new()
	environment.environment = Environment.new()
	environment.environment.background_mode = Environment.BG_COLOR
	environment.environment.background_color = Color(0.05, 0.07, 0.1)
	environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.environment.ambient_light_sky_contribution = 0
	environment.environment.ambient_light_color = Color(1, 1, 1)
	environment.environment.ambient_light_energy = 0.12
	_viewport.add_child(environment)
	var toward = _sun.transform.basis.z.normalized()
	_shadow_world = _cube.position - toward * (_cube.position.y / toward.y)
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
		if not _check(_viewport.get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_OBJECTS_IN_FRAME) == 2, "shadow draws must not inflate visible object counts"):
			return
		var shadow_draws = _viewport.get_render_info(Viewport.RENDER_INFO_TYPE_SHADOW, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
		if not _check(shadow_draws == (1 if _has_shadows else 0), "shadow draws use their own render-info category"):
			return
		_sun.shadow_enabled = false
	if _frames == 14:
		_unshadowed = _image()
		_save(_unshadowed, "unshadowed")
		var lit = _pixel(_unshadowed, _sample)
		_lit = lit
		if not _check(_viewport.get_render_info(Viewport.RENDER_INFO_TYPE_SHADOW, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME) == 0, "disabled shadows clear render-info counters"):
			return
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
		_material.disable_receive_shadows = true
	if _frames == 20:
		if not _check(abs(_pixel(_image(), _sample) - _pixel(_unshadowed, _sample)) < 0.04, "material receive-shadow opt-out"):
			return
		_material.disable_receive_shadows = false
		ProjectSettings.set_setting("rendering/quality/fixed_function/directional_shadows", false)
	if _frames == 26:
		if not _check(abs(_pixel(_image(), _sample) - _pixel(_unshadowed, _sample)) < 0.04, "global shadow opt-out must not reuse stale contents"):
			return
		ProjectSettings.set_setting("rendering/quality/fixed_function/directional_shadows", true)
		_cube.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	if _frames == 32:
		if not _check(abs(_pixel(_image(), _sample) - _pixel(_unshadowed, _sample)) < 0.04, "empty caster map must clear old occlusion"):
			return
		_cube.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
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
		_skeleton.set_bone_pose(0, Transform3D(Basis(), Vector3(2, 0, 0)))
	if _frames == 74:
		if not _expect_shadow(_shadow_world + Vector3(2, 0, 0), "skinned shadow follows bone pose") or not _expect_lit(_shadow_world, "bone pose clears old shadow"):
			return
		_skeleton.set_bone_pose(0, Transform3D())
		_deformed.set_blend_shape_value(0, 1.0)
	if _frames == 80:
		if not _expect_shadow(_shadow_world + Vector3(2, 0, 0), "morphed shadow follows positions") or not _expect_lit(_shadow_world, "morph clears old shadow"):
			return
		_caster_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
		_caster_material.alpha_scissor_threshold = 0.5
		_caster_material.albedo_texture = _texture(Color(1, 1, 1), true)
	if _frames == 86:
		if not _expect_lit(_shadow_world + Vector3(1.7, 0, 0), "cutout hole transmits sunlight") or not _expect_shadow(_shadow_world + Vector3(2.3, 0, 0), "opaque cutout texels cast"):
			return
		_save(_image(), "cutout")
		_caster_material.uv1_offset.x = 0.5
	if _frames == 92:
		if not _expect_shadow(_shadow_world + Vector3(1.7, 0, 0), "caster coverage honors UV transforms"):
			return
		_caster_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	if _frames == 98:
		if not _expect_lit(_shadow_world + Vector3(1.7, 0, 0), "ordinary blended material does not cast opaque shadows"):
			return
		_caster_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
		_material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_VERTEX
	if _frames == 104:
		if not _expect_lit(_shadow_world + Vector3(1.7, 0, 0), "explicit vertex-lighting receiver stays unshadowed"):
			return
		_material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
		_viewport.size = Vector2(400, 300)
		RenderingServer.directional_shadow_atlas_set_size(256, false)
		DisplayServer.window_set_size(Vector2i(700, 500))
	if _frames == 112:
		if not _expect_shadow(_shadow_world + Vector3(1.7, 0, 0), "map resize and native target recovery"):
			return
		if not _check(_caster_mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX] == _source_vertices, "deformation must preserve CPU source mesh"):
			return
		_save(_image(), "recovered")
		_sun.shadow_opacity = 0.5
	if _frames == 118:
		_before_emission = _color(_image(), _camera.unproject_position(_shadow_world + Vector3(1.7, 0, 0)))
		if _has_shadows and not _check(_before_emission.r > 0.2 and _before_emission.r < _lit - 0.08 and abs(_before_emission.r - _before_emission.g) < 0.02, "shadow opacity affects direct light only"):
			return
		_material.emission_enabled = true
		_material.emission = Color(0.2, 0.2, 0.2)
	if _frames == 124:
		var after = _color(_image(), _camera.unproject_position(_shadow_world + Vector3(1.7, 0, 0)))
		var delta = after - _before_emission
		if RenderingServer.get_current_rendering_driver_name() != "d3d3_software":
			if not _check(abs(delta.r - 0.2) < 0.04 and abs(delta.g - 0.2) < 0.04 and abs(delta.b - 0.2) < 0.04, "emission remains unshadowed"):
				return
		_material.emission_enabled = false
		_sun.shadow_opacity = 1.0
		_second_viewport = SubViewport.new()
		_second_viewport.size = Vector2i(320, 240)
		_second_viewport.world_3d = _viewport.find_world_3d()
		_second_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		add_child(_second_viewport)
		_second_camera = Camera3D.new()
		_second_camera.position = Vector3(6, 5, 5)
		_second_viewport.add_child(_second_camera)
		_second_camera.look_at(Vector3.ZERO, Vector3.UP)
		_second_camera.current = true
		var preview = TextureRect.new()
		preview.texture = _second_viewport.get_texture()
		preview.size = Vector2(160, 120)
		preview.position.x = 400
		add_child(preview)
	if _frames == 132:
		if not _check_second_view("independent cameras share a shadowed world"):
			return
		_sun.shadow_enabled = false
	if _frames == 138:
		var second = _pixel(_second_viewport.get_texture().get_image(), _second_camera.unproject_position(_shadow_world + Vector3(1.7, 0, 0)))
		if not _expect_lit(_shadow_world + Vector3(1.7, 0, 0), "first camera drops old shadow") or not _check(abs(second - _lit) < 0.04, "second camera drops old shadow"):
			return
		_sun.shadow_enabled = true
	if _frames == 144:
		if not _check_second_view("shadow maps redraw independently for both cameras"):
			return
		_second_viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
	if _frames == 148:
		if not _check(RenderingServer.viewport_get_update_mode(_second_viewport.get_viewport_rid()) == RenderingServer.VIEWPORT_UPDATE_DISABLED, "one-shot viewport is frozen before reset"):
			return
		DisplayServer.window_set_size(Vector2i(720, 520))
	if _frames == 156:
		if not _check_second_view("frozen viewport recovers with a fresh shadow map after reset"):
			return
		_save(_second_viewport.get_texture().get_image(), "second_camera")
		ProjectSettings.set_setting("rendering/quality/fixed_function/directional_shadows", _previous_shadows)
		ProjectSettings.set_setting("rendering/quality/fixed_function/retained_geometry", _previous_retained)
		print("DIRECTIONAL SHADOW SCENE PASS: direct/ambient/emission, cascade approximation, fallback, retained/streamed channels, skin/morphs, cutout UVs, opacity, counters, independent cameras and frozen recovery")
		get_tree().quit()


func _check_second_view(message):
	if not _expect_shadow(_shadow_world + Vector3(1.7, 0, 0), message + " (first)"):
		return false
	var image = _second_viewport.get_texture().get_image()
	var value = _pixel(image, _second_camera.unproject_position(_shadow_world + Vector3(1.7, 0, 0)))
	return _check(value < _lit * 0.6 if _has_shadows else abs(value - _lit) < 0.04, message + " (second)")
