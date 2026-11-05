shader_type canvas_item;

// Contorno pixelado alrededor de lo opaco de un sprite (resaltar el objeto bajo el ratón en la mochila)
uniform vec4 color : source_color = vec4(0.94, 0.82, 0.48, 1.0);
uniform float brillo = 1.15;

void fragment() {
	vec4 c = texture(TEXTURE, UV);
	if (c.a < 0.1) {
		vec2 t = TEXTURE_PIXEL_SIZE;
		float a = texture(TEXTURE, UV + vec2(t.x, 0.0)).a + texture(TEXTURE, UV - vec2(t.x, 0.0)).a
			+ texture(TEXTURE, UV + vec2(0.0, t.y)).a + texture(TEXTURE, UV - vec2(0.0, t.y)).a;
		COLOR = a > 0.1 ? color : vec4(0.0);
	} else {
		COLOR = vec4(c.rgb * brillo, c.a);
	}
}
