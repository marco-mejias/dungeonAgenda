// Background of the void under the dungeon: slow noise clouds with the ambient colours, so there's
// no flat black between the rooms.
shader_type spatial;
render_mode unshaded, cull_disabled, fog_disabled, shadows_disabled;

uniform vec3 color_a : source_color = vec3(0.035, 0.025, 0.07);
uniform vec3 color_b : source_color = vec3(0.12, 0.07, 0.2);
uniform vec3 color_c : source_color = vec3(0.22, 0.1, 0.26);
uniform float escala = 7.0;

// Cheap pseudo-random number from a 2D point
float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Smooth value noise (bilinear between the 4 corners)
float ruido(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash(i), b = hash(i + vec2(1.0, 0.0)), c = hash(i + vec2(0.0, 1.0)), d = hash(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void fragment() {
	vec2 p = UV * escala;
	float n = ruido(p + vec2(TIME * 0.03, TIME * 0.018)) * 0.6 + ruido(p * 2.3 - vec2(TIME * 0.05, 0.0)) * 0.4;
	vec3 col = mix(color_a, color_b, smoothstep(0.35, 0.8, n));
	col = mix(col, color_c, smoothstep(0.72, 0.95, n) * 0.6);
	// Fades out towards the edges of the plane
	float borde = smoothstep(0.5, 0.25, length(UV - 0.5));
	ALBEDO = col * (0.35 + 0.65 * borde);
}
