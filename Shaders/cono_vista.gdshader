shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

// Light cone of an enemy's sight (Sigilo.gd). Fan mesh glued to the floor:
//   UV.x = distance to the enemy (0) .. reach (1);  UV.y = from one edge (-1) to the other (1).
// The light is EVEN up to the edge (if your tile is in the light, it sees you: Sigilo.en_luz), a bit
// stronger near the enemy, and fades softly on the sides. It appears and disappears slowly (pulse),
// each enemy at its own pace (semilla). Where its reach ends, a thin LINE that never fully fades:
// you can always see how far it goes (where a wall cuts it there's no line).

uniform vec4 color : source_color = vec4(1.0, 0.62, 0.32, 1.0);
// intensidad and semilla are per cone (instance uniform): one single material for all of them
instance uniform float intensidad = 0.22;
uniform float velocidad = 0.9;
instance uniform float semilla = 0.0;
uniform float grosor_linea = 0.045; // As a fraction of the reach
uniform float fuerza_linea = 1.6;

void fragment() {
	float d = clamp(UV.x, 0.0, 1.0);
	float lado = abs(UV.y);
	float radial = (0.7 + 0.3 * (1.0 - d)) * smoothstep(0.0, 0.12, d);
	float bordes = smoothstep(1.0, 0.86, lado);
	// Intermittent and soft: it lights up, stays a bit and fades
	float onda = 0.5 + 0.5 * sin(TIME * velocidad + semilla);
	float pulso = smoothstep(0.15, 0.85, onda);
	float luz = radial * bordes * pulso;
	// The reach line: thin, fainter at the tips of the arc, and it breathes less than the cone
	float linea = smoothstep(1.0 - grosor_linea, 1.0 - grosor_linea * 0.25, d) * smoothstep(1.0, 0.9, lado);
	linea *= fuerza_linea * (0.55 + 0.45 * pulso);
	ALBEDO = color.rgb * (luz + linea) * intensidad;
}
