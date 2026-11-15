// Overlay de casillas (alcance de movimiento / interactuables / enemigos).
// Por instancia (MultiMesh custom data): rgb = color, a = máscara de bordes / 15
//   bit 1 = norte (-Z), 2 = sur (+Z), 4 = oeste (-X), 8 = este (+X)
// Pinta solo el contorno de los lados exteriores del área (sin relleno), que se enciende y se apaga
// despacio (un respiro de ~2.5 s) en vez de estar siempre a tope.
shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_never, shadows_disabled, fog_disabled;

uniform float periodo = 2.5;      // segundos de un respiro completo
uniform float minimo = 0.25;      // lo más apagado que se queda el contorno
uniform float border = 0.07;
// Fuerza del temblor de vértices en píxeles de baja resolución (opción del menú de pausa)
global uniform float ps1_jitter;

varying flat vec4 data;

const float BAYER[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

void vertex() {
	data = INSTANCE_CUSTOM;
	vec4 clip = PROJECTION_MATRIX * (MODELVIEW_MATRIX * vec4(VERTEX, 1.0));
	vec2 snap = VIEWPORT_SIZE / (2.0 * max(ps1_jitter, 0.01));
	clip.xy = floor((clip.xy / clip.w) * snap + 0.5) / snap * clip.w;
	POSITION = clip;
}

void fragment() {
	int mask = int(data.a * 15.0 + 0.5);
	vec2 uv = UV;
	bool edge = ((mask & 1) != 0 && uv.y < border) || ((mask & 2) != 0 && uv.y > 1.0 - border) ||
				((mask & 4) != 0 && uv.x < border) || ((mask & 8) != 0 && uv.x > 1.0 - border);

	if (!edge) {
		discard;
	}
	float pulse = 0.5 + 0.5 * sin(TIME * 6.2831853 / periodo);
	ALBEDO = data.rgb;
	ALPHA = mix(minimo, 1.0, pulse);
}
