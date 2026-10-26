extends Node3D

const EMISSION_SETTING = "rendering/quality/fixed_function/emission"
const AO_SETTING = "rendering/quality/fixed_function/texture_occlusion"

var _frames = 0
var _failed = false
var _separate
var _packed


func _texture(color):
	var image = Image.create_empty(4, 4, false, Image.FORMAT_RGBA8)
	image.fill(color)
	var texture = ImageTexture.create_from_image(image)
	return texture


func _add_mesh(material, x):
	var instance = MeshInstance3D.new()
	instance.mesh = BoxMesh.new()
	instance.material_override = material
	instance.position.x = x
	add_child(instance)


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("MATERIAL CHANNEL SCENE FAIL: ", message)


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().disable_3d = false
	ProjectSettings.set_setting(EMISSION_SETTING, true)
	ProjectSettings.set_setting(AO_SETTING, true)
	var glow = _texture(Color(0.1, 0.2, 0.3, 0))
	var orm = _texture(Color(0.25, 0.7, 0.2, 1))
	_separate = StandardMaterial3D.new()
	_separate.albedo_color = Color(0.5, 0.5, 0.5)
	_separate.emission_enabled = true
	_separate.emission = Color(0.2, 0, 0)
	_separate.emission_texture = glow
	_separate.emission_on_uv2 = true
	_separate.ao_enabled = true
	_separate.ao_texture = orm
	_separate.ao_texture_channel = BaseMaterial3D.TEXTURE_CHANNEL_GREEN
	_separate.metallic_texture = orm
	_separate.roughness_texture = orm
	_add_mesh(_separate, -1.1)
	_packed = ORMMaterial3D.new()
	_packed.orm_texture = orm
	_packed.ao_enabled = true
	_packed.emission_enabled = true
	_packed.emission_operator = BaseMaterial3D.EMISSION_OP_MULTIPLY
	_packed.emission = Color(0.5, 1, 1)
	_packed.emission_texture = glow
	_add_mesh(_packed, 1.1)
	var camera = Camera3D.new()
	camera.position = Vector3(0, 0, 5)
	camera.current = true
	add_child(camera)
	var world = WorldEnvironment.new()
	world.environment = Environment.new()
	world.environment.background_mode = Environment.BG_COLOR
	world.environment.background_color = Color(0.08, 0.1, 0.15)
	world.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	world.environment.ambient_light_color = Color(1, 1, 1)
	world.environment.ambient_light_energy = 0.4
	add_child(world)


func _process(_delta):
	_frames += 1
	var calls = get_viewport().get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
	if _frames == 12:
		_check(calls == 5, "two base draws and three emission draws, got %s" % calls)
		ProjectSettings.set_setting(EMISSION_SETTING, false)
	if _frames == 20:
		_check(calls == 2, "AO does not add draws, got %s" % calls)
		ProjectSettings.set_setting(AO_SETTING, false)
	if _frames == 28:
		_check(calls == 2, "both disabled leave two base draws, got %s" % calls)
		_check(_separate.emission_enabled and _packed.ao_enabled, "material data survived settings toggles")
		var mask = _texture(Color(0.3, 0.2, 0.1, 0.75))
		_packed.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
		_packed.alpha_scissor_threshold = 0.5
		_packed.albedo_texture = mask
		_packed.emission_texture = mask
		ProjectSettings.set_setting(EMISSION_SETTING, true)
	if _frames == 36:
		_check(calls == 5, "cutout uses its shared texture for emission and coverage, got %s" % calls)
		_packed.emission_texture = _texture(Color(1, 0, 0))
	if _frames == 44:
		var driver = RenderingServer.get_current_rendering_driver_name()
		var separate_mask = driver == "d3d7" or driver == "d3d9"
		_check(calls == (5 if separate_mask else 4), "independently masked emission or its single-texture fallback, got %s" % calls)
		_packed.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		_packed.albedo_color.a = 0.5
		_packed.emission_texture = _texture(Color(1, 0, 0, 0))
	if _frames == 52:
		var driver = RenderingServer.get_current_rendering_driver_name()
		var separate_mask = driver == "d3d7" or driver == "d3d9"
		_check(calls == (5 if separate_mask else 4), "alpha-blended emission with independent texture coverage, got %s" % calls)
		_packed.albedo_texture = null
	if _frames == 60:
		_check(calls == 5, "constant opacity uses mapped emission without sampling its alpha, got %s" % calls)
		ProjectSettings.set_setting(EMISSION_SETTING, false)
	if _frames == 68:
		_check(calls == 2, "disabling emission retains opaque and transparent base draws, got %s" % calls)
		if not _failed:
			print("MATERIAL CHANNEL SCENE PASS: StandardMaterial3D and ORMMaterial3D")
		get_tree().quit(1 if _failed else 0)
