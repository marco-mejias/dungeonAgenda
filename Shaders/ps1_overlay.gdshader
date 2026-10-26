// Overlay de casillas (alcance de movimiento / interactuables / enemigos).
// Por instancia (MultiMesh custom data): rgb = color, a = máscara de bordes / 15
//   bit 1 = norte (-Z), 2 = sur (+Z), 4 = oeste (-X), 8 = este (+X)
// Pinta un contorno solo en los lados exteriores del área y un relleno tramado (estilo PS1).
shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_never, shadows_disabled, fog_disabled;

uniform float fill_density = 0.2;
uniform float border = 0.1;
uniform float jitter = 1.5;

varying flat vec4 data;

const float BAYER[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

void vertex() {
	data = INSTANCE_CUSTOM;
	vec4 clip = PROJECTION_MATRIX * (MODELVIEW_MATRIX * vec4(VERTEX, 1.0));
	vec2 snap = VIEWPORT_SIZE / (2.0 * max(jitter, 0.01));
	clip.xy = floor((clip.xy / clip.w) * snap + 0.5) / snap * clip.w;
	POSITION = clip;
}

void fragment() {
	int mask = int(data.a * 15.0 + 0.5);
	vec2 uv = UV;
	bool edge = ((mask & 1) != 0 && uv.y < border) || ((mask & 2) != 0 && uv.y > 1.0 - border) ||
				((mask & 4) != 0 && uv.x < border) || ((mask & 8) != 0 && uv.x > 1.0 - border);

	float pulse = 0.5 + 0.5 * sin(TIME * 3.0);
	ivec2 p = ivec2(FRAGCOORD.xy) % 4;
	float threshold = (BAYER[p.y * 4 + p.x] + 0.5) / 16.0;

	if (edge) {
		ALBEDO = data.rgb * (0.85 + 0.35 * pulse);
	} else if (threshold < fill_density * (0.7 + 0.3 * pulse)) {
		ALBEDO = data.rgb * 0.8;
	} else {
		discard;
	}
}
