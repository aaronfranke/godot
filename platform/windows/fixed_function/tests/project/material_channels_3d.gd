extends Spatial

const EMISSION_SETTING = "rendering/quality/fixed_function/emission"
const AO_SETTING = "rendering/quality/fixed_function/texture_occlusion"

var _frames = 0
var _failed = false
var _separate
var _packed


func _texture(color):
	var image = Image.new()
	image.create(4, 4, false, Image.FORMAT_RGBA8)
	image.fill(color)
	var texture = ImageTexture.new()
	texture.create_from_image(image, 0)
	return texture


func _add_mesh(material, x):
	var instance = MeshInstance.new()
	instance.mesh = CubeMesh.new()
	instance.material_override = material
	instance.translation.x = x
	add_child(instance)


func _check(condition, message):
	if not condition:
		_failed = true
		printerr("MATERIAL CHANNEL SCENE FAIL: ", message)


func _ready():
	OS.low_processor_usage_mode = false
	get_viewport().usage = Viewport.USAGE_3D
	ProjectSettings.set_setting(EMISSION_SETTING, true)
	ProjectSettings.set_setting(AO_SETTING, true)
	var glow = _texture(Color(0.1, 0.2, 0.3, 0))
	var orm = _texture(Color(0.25, 0.7, 0.2, 1))
	_separate = SpatialMaterial.new()
	_separate.albedo_color = Color(0.5, 0.5, 0.5)
	_separate.emission_enabled = true
	_separate.emission = Color(0.2, 0, 0)
	_separate.emission_texture = glow
	_separate.emission_on_uv2 = true
	_separate.ao_enabled = true
	_separate.ao_texture = orm
	_separate.ao_texture_channel = SpatialMaterial.TEXTURE_CHANNEL_GREEN
	_separate.metallic_texture = orm
	_separate.roughness_texture = orm
	_add_mesh(_separate, -1.1)
	_packed = ORMSpatialMaterial.new()
	_packed.orm_texture = orm
	_packed.ao_enabled = true
	_packed.emission_enabled = true
	_packed.emission_operator = SpatialMaterial.EMISSION_OP_MULTIPLY
	_packed.emission = Color(0.5, 1, 1)
	_packed.emission_texture = glow
	_add_mesh(_packed, 1.1)
	var camera = Camera.new()
	camera.translation = Vector3(0, 0, 5)
	camera.current = true
	add_child(camera)
	var world = WorldEnvironment.new()
	world.environment = Environment.new()
	world.environment.background_mode = Environment.BG_COLOR
	world.environment.background_color = Color(0.08, 0.1, 0.15)
	world.environment.ambient_light_color = Color(1, 1, 1)
	world.environment.ambient_light_energy = 0.4
	add_child(world)


func _process(_delta):
	_frames += 1
	var calls = get_viewport().get_render_info(Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME)
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
		_packed.params_use_alpha_scissor = true
		_packed.params_alpha_scissor_threshold = 0.5
		_packed.albedo_texture = mask
		_packed.emission_texture = mask
		ProjectSettings.set_setting(EMISSION_SETTING, true)
	if _frames == 36:
		_check(calls == 5, "the cutout uses its shared texture for emission and coverage, got %s" % calls)
		_packed.emission_texture = _texture(Color(1, 0, 0))
	if _frames == 44:
		var driver = OS.get_video_driver_name(OS.get_current_video_driver())
		var separate_mask = driver == "D3D7" or driver == "D3D9"
		_check(calls == (5 if separate_mask else 4), "independently masked emission or its single-texture fallback, got %s" % calls)
		_packed.params_use_alpha_scissor = false
		_packed.flags_transparent = true
		_packed.albedo_color.a = 0.5
		_packed.emission_texture = _texture(Color(1, 0, 0, 0))
	if _frames == 52:
		var driver = OS.get_video_driver_name(OS.get_current_video_driver())
		var separate_mask = driver == "D3D7" or driver == "D3D9"
		_check(calls == (5 if separate_mask else 4), "alpha-blended emission with independent texture coverage, got %s" % calls)
		_packed.albedo_texture = null
	if _frames == 60:
		_check(calls == 5, "constant opacity uses mapped emission without sampling its alpha, got %s" % calls)
		ProjectSettings.set_setting(EMISSION_SETTING, false)
	if _frames == 68:
		_check(calls == 2, "disabling emission retains opaque and transparent base draws, got %s" % calls)
		if not _failed:
			print("MATERIAL CHANNEL SCENE PASS: SpatialMaterial and ORMSpatialMaterial, including cutout and alpha-blended emission")
		get_tree().quit(1 if _failed else 0)
