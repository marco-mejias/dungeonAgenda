shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

// Cono de luz de la vista de un enemigo (Sigilo.gd). Malla en abanico pegada al suelo:
//   UV.x = distancia al enemigo (0) .. alcance (1);  UV.y = de un borde (-1) al otro (1).
// La luz es PAREJA hasta el borde (si tu casilla está en la luz, te ve: Sigilo.en_luz), un poco más
// fuerte cerca del enemigo, y se corta suave en los lados. Aparece y desaparece
// despacio (pulso), cada enemigo a su ritmo (semilla). Donde acaba su alcance, una LÍNEA fina que
// no se apaga del todo: se ve siempre hasta dónde llega (donde la corta una pared no hay línea).

uniform vec4 color : source_color = vec4(1.0, 0.62, 0.32, 1.0);
// intensidad y semilla van por cono (instance uniform): un solo material para todos
instance uniform float intensidad = 0.22;
uniform float velocidad = 0.9;
instance uniform float semilla = 0.0;
uniform float grosor_linea = 0.045; // en fracción del alcance
uniform float fuerza_linea = 1.6;

void fragment() {
	float d = clamp(UV.x, 0.0, 1.0);
	float lado = abs(UV.y);
	float radial = (0.7 + 0.3 * (1.0 - d)) * smoothstep(0.0, 0.12, d);
	float bordes = smoothstep(1.0, 0.86, lado);
	// Intermitente y suave: se enciende, se queda un poco y se apaga
	float onda = 0.5 + 0.5 * sin(TIME * velocidad + semilla);
	float pulso = smoothstep(0.15, 0.85, onda);
	float luz = radial * bordes * pulso;
	// La línea del alcance: fina, más tenue en las puntas del arco, y respira menos que el cono
	float linea = smoothstep(1.0 - grosor_linea, 1.0 - grosor_linea * 0.25, d) * smoothstep(1.0, 0.9, lado);
	linea *= fuerza_linea * (0.55 + 0.45 * pulso);
	ALBEDO = color.rgb * (luz + linea) * intensidad;
}
