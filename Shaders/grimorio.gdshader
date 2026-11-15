shader_type canvas_item;

// Pergamino mágico del HUD (grimorio):
//  - TEMBLOR: el dibujo "hierve" un poco a pocos fotogramas por segundo, como dibujado a mano.
//  - RUNAS: lo quemado del borde brilla en violeta a ratos, con chispas que suben.
// Va en el material del panel (no en sus hijos). 'magia' sube en lo seleccionado/activo.

uniform float temblor : hint_range(0.0, 2.0) = 0.7;   // en píxeles de la textura
uniform float pasos_por_segundo = 7.0;
uniform float magia : hint_range(0.0, 2.0) = 0.45;
uniform vec4 color_magia : source_color = vec4(0.62, 0.38, 1.0, 1.0);
uniform float semilla = 0.0;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void fragment() {
	float paso = floor(TIME * pasos_por_segundo) + semilla;
	vec2 px = TEXTURE_PIXEL_SIZE;
	vec2 o = vec2(sin(UV.y * 37.0 + paso * 1.7), cos(UV.x * 29.0 + paso * 2.3)) * px * temblor;
	vec4 c = texture(TEXTURE, UV + o);

	// Zonas quemadas (oscuras y opacas): runas que laten
	float lum = dot(c.rgb, vec3(0.299, 0.587, 0.114));
	float quemado = (1.0 - smoothstep(0.12, 0.32, lum)) * c.a;
	float onda = 0.5 + 0.5 * sin(TIME * 1.8 + (UV.x + UV.y) * 9.0 + semilla);
	// Chispas: celdas que se encienden un momento
	vec2 celda = floor(UV / (px * 3.0));
	float chispa = step(0.985, hash(celda + floor(TIME * 3.0 + hash(celda) * 10.0)));
	vec3 brillo = color_magia.rgb * magia * quemado * (onda * 0.9 + chispa * 2.5);
	COLOR = vec4(c.rgb + brillo, c.a) * COLOR;
}
