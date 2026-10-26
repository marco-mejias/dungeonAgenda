// Fondo del vacío bajo la mazmorra: nubes lentas de ruido con colores del ambiente,
// para que entre las salas no haya un negro plano.
shader_type spatial;
render_mode unshaded, cull_disabled, fog_disabled, shadows_disabled;

uniform vec3 color_a : source_color = vec3(0.035, 0.025, 0.07);
uniform vec3 color_b : source_color = vec3(0.12, 0.07, 0.2);
uniform vec3 color_c : source_color = vec3(0.22, 0.1, 0.26);
uniform float escala = 7.0;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

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
	// Se apaga hacia los bordes del plano
	float borde = smoothstep(0.5, 0.25, length(UV - 0.5));
	ALBEDO = col * (0.35 + 0.65 * borde);
}
