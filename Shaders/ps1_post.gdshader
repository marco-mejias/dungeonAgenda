// PS1-style post process for the low-res map viewport: 15-bit color with ordered dithering.
shader_type canvas_item;

uniform float levels = 32.0;
uniform float dither_strength = 0.85;
uniform float darken = 1.0;
uniform bool crt = false; // scanlines + viñeta (opción del menú de pausa)

const float BAYER[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

void fragment() {
	vec4 c = texture(TEXTURE, UV);
	ivec2 p = ivec2(UV / TEXTURE_PIXEL_SIZE) % 4;
	float b = (BAYER[p.y * 4 + p.x] + 0.5) / 16.0 - 0.5;
	vec3 q = floor(c.rgb * (levels - 1.0) + 0.5 + b * dither_strength) / (levels - 1.0);
	q = clamp(q, 0.0, 1.0) * darken;
	if (crt) {
		float linea = mod(FRAGCOORD.y, 3.0) < 1.0 ? 0.78 : 1.0;
		vec2 d = UV - 0.5;
		q *= linea * (1.0 - dot(d, d) * 1.1);
	}
	COLOR = vec4(q, c.a);
}
