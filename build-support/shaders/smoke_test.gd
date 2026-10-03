extends SceneTree

var floor_mesh: MeshInstance3D
var sun: DirectionalLight3D
var failures := 0

func _initialize() -> void:
	call_deferred("run")

func check(condition: bool, message: String) -> void:
	if not condition:
		push_error(message)
		failures += 1

func render_shader(code: String) -> Image:
	var shader := Shader.new()
	shader.code = code
	var material := ShaderMaterial.new()
	material.shader = shader
	floor_mesh.material_override = material
	for frame in range(10):
		await process_frame
	await RenderingServer.frame_post_draw
	return root.get_texture().get_image()

func run() -> void:
	root.size = Vector2i(256, 256)
	var world := Node3D.new()
	root.add_child(world)
	var environment := WorldEnvironment.new()
	environment.environment = Environment.new()
	environment.environment.background_mode = Environment.BG_COLOR
	environment.environment.background_color = Color.BLACK
	environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.environment.ambient_light_color = Color.BLACK
	environment.environment.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED
	world.add_child(environment)
	var camera := Camera3D.new()
	camera.position = Vector3(0, 8, 0)
	camera.rotation_degrees.x = -90
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = 8
	camera.cull_mask = 1
	world.add_child(camera)
	camera.make_current()
	floor_mesh = MeshInstance3D.new()
	var plane := PlaneMesh.new()
	plane.size = Vector2(10, 10)
	floor_mesh.mesh = plane
	world.add_child(floor_mesh)
	var caster := MeshInstance3D.new()
	caster.mesh = BoxMesh.new()
	caster.position = Vector3(0, 1, 0)
	# Keep the caster in the camera's cull set while hiding its visible geometry.
	caster.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
	world.add_child(caster)
	sun = DirectionalLight3D.new()
	sun.rotation_degrees.x = -90
	sun.shadow_enabled = true
	sun.directional_shadow_mode = DirectionalLight3D.SHADOW_PARALLEL_4_SPLITS
	sun.directional_shadow_blend_splits = true
	world.add_child(sun)
	var prefix := "shader_type spatial; render_mode unshaded; "
	var image := await render_shader(prefix + "void fragment(){ ALBEDO = vec3(sample_directional_shadow(0u, VERTEX)); }")
	var compatibility := RenderingServer.get_current_rendering_method() == "gl_compatibility"
	var center := image.get_pixel(128, 128).r
	var outside := image.get_pixel(32, 32).r
	print("SHADOW_SAMPLE renderer=", RenderingServer.get_current_rendering_method(), " center=", center, " outside=", outside)
	check(outside > 0.8, "An unoccluded fragment must be lit.")
	check(center > 0.8 if compatibility else center < 0.2, "Shadow visibility did not match the renderer contract.")
	image.save_png("res://shadow-sample-" + RenderingServer.get_current_rendering_method() + ".png")
	image = await render_shader(prefix + "float sample_at(vec3 p){return sample_directional_shadow(0u,p);} void fragment(){ALBEDO=vec3(sample_at(VERTEX + vec3(2.0,0.0,0.0)));}")
	check(image.get_pixel(128, 128).r > 0.8, "Moving the sample away from the occluder must remove its shadow.")
	image = await render_shader(prefix + "void fragment(){ALBEDO=vec3(sample_directional_shadow(0xFFFFFFFFu,VERTEX));}")
	check(image.get_pixel(128, 128).r > 0.8, "An invalid light index must return full visibility.")
	image = await render_shader("shader_type spatial; render_mode unshaded, shadows_disabled; void fragment(){ALBEDO=vec3(sample_directional_shadow(0u,VERTEX));}")
	check(image.get_pixel(128, 128).r > 0.8, "shadows_disabled must return full visibility.")
	sun.shadow_enabled = false
	image = await render_shader(prefix + "void fragment(){ALBEDO=vec3(sample_directional_shadow(0u,VERTEX));}")
	check(image.get_pixel(128, 128).r > 0.8, "A light without shadows must return full visibility.")
	sun.shadow_enabled = true
	var sample_shader := FileAccess.get_file_as_string("res://directional_shadow.gdshader")
	image = await render_shader(sample_shader)
	print("LIGHT_INDEX_SAMPLE center=", image.get_pixel(128, 128).r, " outside=", image.get_pixel(32, 32).r)
	check(image.get_pixel(128, 128).r < 0.2 if not compatibility else image.get_pixel(128, 128).r > 0.5, "LIGHT_INDEX must identify the current directional light.")
	# Compile the same callback with each positional-light type present too.
	for light in [OmniLight3D.new(), SpotLight3D.new(), AreaLight3D.new()]:
		light.position = Vector3(0, 3, 0)
		light.rotation_degrees.x = -90
		world.add_child(light)
		var area_shader := sample_shader.replace("if (LIGHT_IS_DIRECTIONAL)", "if (LIGHT_IS_AREA) { DIFFUSE_LIGHT += LIGHT_COLOR * LIGHT_AREA_DIFFUSE_MULTIPLIER * (1.0 + float(LIGHT_INDEX) * 0.0); } else if (LIGHT_IS_DIRECTIONAL)")
		image = await render_shader(area_shader)
		light.queue_free()
	# Exercise Forward+'s PCSS path as well as the default PCF path.
	sun.light_angular_distance = 1.0
	image = await render_shader(prefix + "void fragment(){ALBEDO=vec3(sample_directional_shadow(0u,VERTEX));}")
	check(image.get_pixel(128, 128).r > 0.8 if compatibility else image.get_pixel(128, 128).r < 0.2, "Soft directional shadow sampling failed.")
	sun.queue_free()
	await process_frame
	image = await render_shader(prefix + "void fragment(){ALBEDO=vec3(sample_directional_shadow(0u,VERTEX));}")
	check(image.get_pixel(128, 128).r > 0.8, "A scene without directional lights must return full visibility.")
	print("SHADOW_SMOKE_RESULT failures=", failures)
	quit(1 if failures else 0)
