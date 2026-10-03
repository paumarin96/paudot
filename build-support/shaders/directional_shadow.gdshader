shader_type spatial;

uniform vec3 shadow_offset = vec3(0.0);
varying vec3 shadow_position;

void fragment() {
	ALBEDO = vec3(1.0);
	// VERTEX is already in view space here; shadow_offset is in view space too.
	shadow_position = VERTEX + shadow_offset;
}

void light() {
	if (LIGHT_IS_DIRECTIONAL) {
		float visibility = sample_directional_shadow(LIGHT_INDEX, shadow_position);
		DIFFUSE_LIGHT += ALBEDO * LIGHT_COLOR * max(dot(NORMAL, LIGHT), 0.0) * visibility;
	} else {
		DIFFUSE_LIGHT += ALBEDO * LIGHT_COLOR * max(dot(NORMAL, LIGHT), 0.0) * ATTENUATION;
	}
}
