extends SceneTree

func _initialize() -> void:
	var cases := {
		"vertex": "shader_type spatial; void vertex(){VERTEX.x=sample_directional_shadow(0u,VERTEX);}",
		"vertex_helper": "shader_type spatial; float sample_at(vec3 p){return sample_directional_shadow(0u,p);} void vertex(){VERTEX.x=sample_at(VERTEX);}",
		"canvas": "shader_type canvas_item; void fragment(){COLOR.r=sample_directional_shadow(0u,vec3(0.0));}",
		"wrong_arguments": "shader_type spatial; void fragment(){ALBEDO.r=sample_directional_shadow(vec3(0.0),0u);}",
	}
	var shader := Shader.new()
	shader.code = cases[OS.get_cmdline_user_args()[0]]
	# Force renderer-side compilation. The runner expects a shader compile error.
	shader.get_shader_uniform_list()
	print("SHADER_VALIDATION_COMPLETE")
	quit()
