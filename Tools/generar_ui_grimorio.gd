extends SceneTree
# Genera las texturas del HUD "grimorio" en Assets/UI/Grimorio. Estilo LIMPIO y OSCURO:
#   - CUERO de las tapas del grimorio (marcos y placas): marrón muy oscuro, tono suave, canto
#     oscuro, un brillo fino arriba y un filete dorado por dentro.
#   - CARTAS de pergamino en tono vela (ocre apagado, más oscuras hacia el borde).
#   - Pergamino oscuro (fichas de ayuda) y el SELLO de lacre.
# Las piezas que flotan sueltas llevan su sombra suave dentro de la textura (SOMBRA px alrededor).
#   godot --headless --path . -s res://Tools/generar_ui_grimorio.gd

const DIR := "res://Assets/UI/Grimorio/"
const SOMBRA := 4 # px de sombra alrededor (el HUD lo tiene en cuenta: Hud.SOMBRA)

func _initialize():
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(DIR))
	# Cuero
	_guardar(con_sombra(cuero(120, 90, 7, 5.0, 3)), "cuero")
	_guardar(con_sombra(cuero(170, 50, 6, 4.0, 5)), "cuero_tira")
	# Cartas en tono vela: normal, encima (tinta violeta, más luz), apagada, roja (enemigos)
	var carta := {
		"base": Color(0.78, 0.67, 0.48), "tostado": Color(0.36, 0.24, 0.13), "canto": Color(0.16, 0.10, 0.06),
		"tinta": Color(0.24, 0.14, 0.08, 0.75), "tinta_en": 3.5, "tostado_ancho": 9.0, "irregular": 0.8,
	}
	_guardar(con_sombra(pergamino(42, 54, carta, 51)), "carta")
	_guardar(con_sombra(pergamino(42, 54, carta.merged({"base": Color(0.90, 0.81, 0.62), "tinta": Color(0.45, 0.25, 0.78, 0.95)}), 51)), "carta_hover")
	_guardar(con_sombra(pergamino(42, 54, carta.merged({"base": Color(0.46, 0.40, 0.32), "tostado": Color(0.24, 0.19, 0.14), "tinta": Color(0.20, 0.16, 0.12, 0.4)}), 51)), "carta_off")
	_guardar(con_sombra(pergamino(42, 54, carta.merged({"tinta": Color(0.55, 0.10, 0.08, 0.85)}), 51)), "carta_roja")
	# Fichas de ayuda: pergamino oscuro
	var oscuro := {
		"base": Color(0.20, 0.14, 0.10), "tostado": Color(0.12, 0.08, 0.05), "canto": Color(0.06, 0.04, 0.03),
		"tinta": Color(0.78, 0.62, 0.36, 0.45), "tinta_en": 7.0, "tostado_ancho": 10.0, "irregular": 1.6,
	}
	_guardar(pergamino(180, 120, oscuro, 37), "pergamino_oscuro")
	# Sello de lacre
	_guardar(con_sombra(sello(64, Color(0.60, 0.11, 0.09))), "sello")
	_guardar(con_sombra(sello(64, Color(0.72, 0.16, 0.12))), "sello_hover")
	_guardar(con_sombra(sello(64, Color(0.36, 0.27, 0.23))), "sello_off")
	# Las cajas comunes (Estilo.caja: ventanas, botones, huecos de las pantallas) en cuero
	var oro := Color(0.72, 0.55, 0.27)
	_guardar(cuero_v(160, 120, 8, 6.0, {}), "estilo_panel")
	_guardar(cuero_v(160, 120, 8, 6.0, {"filete": Color(0.95, 0.78, 0.40), "grosor": 1.3}), "estilo_panel_oro")
	_guardar(cuero_v(160, 120, 8, 6.0, {"filete": Color(0.72, 0.20, 0.15)}), "estilo_panel_rojo")
	_guardar(cuero_v(160, 120, 8, 6.0, {"filete": Color(0.95, 0.55, 0.20)}), "estilo_panel_alerta")
	_guardar(pergamino(120, 80, oscuro, 41), "estilo_tooltip")
	_guardar(cuero_v(48, 28, 5, 3.0, {"filete": oro.darkened(0.2), "grosor": 0.8}), "estilo_boton")
	_guardar(cuero_v(48, 28, 5, 3.0, {"base": Color(0.25, 0.17, 0.12), "filete": Color(0.95, 0.78, 0.40)}), "estilo_boton_hover")
	_guardar(cuero_v(48, 28, 5, 3.0, {"base": Color(0.10, 0.065, 0.045), "filete": Color(0.95, 0.78, 0.40), "brillo": false}), "estilo_boton_pulsado")
	_guardar(cuero_v(48, 28, 5, 3.0, {"base": Color(0.12, 0.10, 0.09), "filete": Color(0.30, 0.27, 0.22), "brillo": false}), "estilo_boton_off")
	_guardar(cuero_v(40, 40, 5, 1.2, {"base": Color(0.07, 0.045, 0.03), "filete": Color(0.40, 0.29, 0.14), "brillo": false, "hundido": true}), "estilo_hueco")
	_guardar(cuero_v(40, 40, 5, 1.2, {"base": Color(0.09, 0.06, 0.04), "filete": Color(0.95, 0.78, 0.40), "brillo": false, "hundido": true, "grosor": 1.2}), "estilo_hueco_oro")
	_guardar(cuero_v(40, 40, 5, 1.2, {"base": Color(0.11, 0.075, 0.05), "filete": Color(0.70, 0.54, 0.27), "brillo": false, "hundido": true}), "estilo_hueco_hover")
	print("[grimorio] texturas en ", DIR)
	quit()

func _guardar(img: Image, nombre: String) -> void:
	img.save_png(ProjectSettings.globalize_path(DIR + nombre + ".png"))

func _ruido(semilla: int, freq: float, octavas: int = 2) -> FastNoiseLite:
	var n := FastNoiseLite.new()
	n.seed = semilla
	n.frequency = freq
	n.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	n.fractal_octaves = octavas
	return n

# Distancia (positiva dentro) al borde de un rectángulo de esquinas redondas
func _dentro_redondeado(p: Vector2, w: int, h: int, r: float) -> float:
	var media := Vector2(w, h) * 0.5
	var q := (p - media).abs() - (media - Vector2(r, r))
	var fuera := Vector2(maxf(q.x, 0.0), maxf(q.y, 0.0)).length() + minf(maxf(q.x, q.y), 0.0) - r
	return -fuera

# Cuero: tono suave, canto oscuro, brillo fino arriba y filete dorado a 'filete' px del borde
func cuero(w: int, h: int, radio: float, filete: float, semilla: int) -> Image:
	var tono := _ruido(semilla, 0.02)
	var poro := _ruido(semilla + 1, 0.45, 1)
	var base := Color(0.17, 0.11, 0.08)
	var img := Image.create(w, h, false, Image.FORMAT_RGBA8)
	for y in h:
		for x in w:
			var d := _dentro_redondeado(Vector2(x + 0.5, y + 0.5), w, h, radio)
			var alfa := clampf(d + 0.5, 0.0, 1.0)
			if alfa <= 0.0:
				img.set_pixel(x, y, Color(0, 0, 0, 0))
				continue
			var col := base.darkened(clampf(tono.get_noise_2d(x, y) * 0.12, -0.08, 0.12))
			col = col.darkened(clampf(poro.get_noise_2d(x, y) * 0.05, 0.0, 0.05)) # poro muy leve
			col = col.lerp(Color(0.06, 0.035, 0.025), clampf(1.0 - (d - 0.5) / 2.0, 0.0, 1.0))  # canto
			if y < h * 0.5:
				col = col.lerp(Color(0.32, 0.22, 0.15), clampf(1.0 - absf(d - 2.2) / 0.8, 0.0, 1.0) * 0.5) # brillo
			col = col.lerp(Color(0.72, 0.55, 0.27), clampf(1.0 - absf(d - filete) / 0.75, 0.0, 1.0) * 0.8) # filete
			col.a = alfa
			img.set_pixel(x, y, col)
	return img

# Cuero con opciones: base, filete (color), grosor del filete, brillo arriba, hundido (hueco: sombra
# interior arriba en vez de brillo)
func cuero_v(w: int, h: int, radio: float, filete: float, o: Dictionary) -> Image:
	var tono := _ruido(9, 0.02)
	var base: Color = o.get("base", Color(0.17, 0.11, 0.08))
	var c_filete: Color = o.get("filete", Color(0.72, 0.55, 0.27))
	var grosor: float = o.get("grosor", 0.75)
	var brillo: bool = o.get("brillo", true)
	var hundido: bool = o.get("hundido", false)
	var img := Image.create(w, h, false, Image.FORMAT_RGBA8)
	for y in h:
		for x in w:
			var d := _dentro_redondeado(Vector2(x + 0.5, y + 0.5), w, h, radio)
			var alfa := clampf(d + 0.5, 0.0, 1.0)
			if alfa <= 0.0:
				img.set_pixel(x, y, Color(0, 0, 0, 0))
				continue
			var col := base.darkened(clampf(tono.get_noise_2d(x, y) * 0.1, -0.06, 0.1))
			if hundido:
				col = col.darkened(clampf(1.0 - (y - 1.0) / 6.0, 0.0, 1.0) * 0.5) # sombra interior arriba
			else:
				col = col.lerp(Color(0.05, 0.03, 0.02), clampf(1.0 - (d - 0.5) / 2.0, 0.0, 1.0))
				if brillo and y < h * 0.5:
					col = col.lerp(base.lightened(0.35), clampf(1.0 - absf(d - 2.2) / 0.8, 0.0, 1.0) * 0.5)
			col = col.lerp(c_filete, clampf(1.0 - absf(d - filete) / grosor, 0.0, 1.0) * 0.85)
			col.a = alfa
			img.set_pixel(x, y, col)
	return img

func pergamino(w: int, h: int, c: Dictionary, semilla: int) -> Image:
	var tono := _ruido(semilla, 0.012)
	var borde := _ruido(semilla + 2, 0.07)
	var irregular: float = c["irregular"]
	var ancho: float = c["tostado_ancho"]
	var tinta_en: float = c["tinta_en"]
	var img := Image.create(w, h, false, Image.FORMAT_RGBA8)
	for y in h:
		for x in w:
			var d := minf(minf(x + 0.5, y + 0.5), minf(w - x - 0.5, h - y - 0.5)) + borde.get_noise_2d(x, y) * irregular
			var alfa := clampf(d - 0.5, 0.0, 1.0)
			if alfa <= 0.0:
				img.set_pixel(x, y, Color(0, 0, 0, 0))
				continue
			var col: Color = (c["base"] as Color).darkened(clampf(tono.get_noise_2d(x, y) * 0.06 + 0.02, 0.0, 0.08))
			var k := clampf(1.0 - (d - 1.0) / ancho, 0.0, 1.0)
			col = col.lerp(c["tostado"], k * k * 0.85)
			col = col.lerp(c["canto"], clampf(1.0 - (d - 1.0) / 2.0, 0.0, 1.0))
			var tinta: Color = c["tinta"]
			var l := clampf(1.0 - absf(d - tinta_en) / 0.9, 0.0, 1.0)
			col = col.lerp(Color(tinta, 1.0), l * tinta.a)
			col.a = alfa
			img.set_pixel(x, y, col)
	return img

func sello(w: int, rojo: Color) -> Image:
	var centro := Vector2(w, w) * 0.5
	var radio := w * 0.44
	var borde := _ruido(61, 0.12)
	var img := Image.create(w, w, false, Image.FORMAT_RGBA8)
	var puntas := []
	for i in 5:
		var a := -PI * 0.5 + i * TAU / 5.0
		puntas.append(centro + Vector2(cos(a), sin(a)) * radio * 0.48)
	for y in w:
		for x in w:
			var p := Vector2(x + 0.5, y + 0.5)
			var r := p.distance_to(centro)
			var lim := radio + borde.get_noise_2d(x, y) * 1.6
			var alfa := clampf(lim - r + 0.5, 0.0, 1.0)
			if alfa <= 0.0:
				img.set_pixel(x, y, Color(0, 0, 0, 0))
				continue
			var luz := clampf(((p - centro).dot(Vector2(-0.7, -0.7)) / radio) * 0.5 + 0.5, 0.0, 1.0)
			var col := rojo.darkened(0.25).lerp(rojo.lightened(0.18), luz)
			col = col.lerp(rojo.darkened(0.45), clampf(1.0 - (lim - r) / 2.5, 0.0, 1.0))
			col = col.lerp(rojo.darkened(0.35), clampf(1.0 - absf(r - radio * 0.74) / 1.2, 0.0, 1.0) * 0.8)
			for i in 5:
				var g := clampf(1.0 - _dist_segmento(p, puntas[i], puntas[(i + 2) % 5]) / 1.1, 0.0, 1.0)
				col = col.lerp(rojo.darkened(0.4), g * 0.85)
			col.a = alfa
			img.set_pixel(x, y, col)
	return img

# Añade SOMBRA px de sombra suave alrededor (un poco hacia abajo)
func con_sombra(src: Image) -> Image:
	var w := src.get_width() + SOMBRA * 2
	var h := src.get_height() + SOMBRA * 2
	var alfa := PackedFloat32Array()
	alfa.resize(w * h)
	for y in src.get_height():
		for x in src.get_width():
			var yy := y + SOMBRA + 2
			if yy < h:
				alfa[yy * w + x + SOMBRA] = src.get_pixel(x, y).a
	alfa = _difuminar(_difuminar(alfa, w, h, 2), w, h, 2)
	var img := Image.create(w, h, false, Image.FORMAT_RGBA8)
	for y in h:
		for x in w:
			img.set_pixel(x, y, Color(0, 0, 0, alfa[y * w + x] * 0.6))
	img.blend_rect(src, Rect2i(Vector2i.ZERO, src.get_size()), Vector2i(SOMBRA, SOMBRA))
	return img

func _difuminar(a: PackedFloat32Array, w: int, h: int, r: int) -> PackedFloat32Array:
	var tmp := PackedFloat32Array()
	tmp.resize(w * h)
	for y in h:
		for x in w:
			var s := 0.0
			for k in range(-r, r + 1):
				s += a[y * w + clampi(x + k, 0, w - 1)]
			tmp[y * w + x] = s / (2 * r + 1)
	var out := PackedFloat32Array()
	out.resize(w * h)
	for y in h:
		for x in w:
			var s := 0.0
			for k in range(-r, r + 1):
				s += tmp[clampi(y + k, 0, h - 1) * w + x]
			out[y * w + x] = s / (2 * r + 1)
	return out

func _dist_segmento(p: Vector2, a: Vector2, b: Vector2) -> float:
	var ab := b - a
	var t := clampf((p - a).dot(ab) / ab.length_squared(), 0.0, 1.0)
	return p.distance_to(a + ab * t)
