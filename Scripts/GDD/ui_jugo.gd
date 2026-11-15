extends RefCounted

# UiJugo: animaciones pequeñas para cualquier Control (HUD, mochila, menús...). Todas son
# reutilizables y no tocan el layout: solo escala, rotación, modulate y el pivote.
#   const Jugo := preload("res://Scripts/GDD/ui_jugo.gd")
#   Jugo.boton(b)              al pasar crece un poco, al pulsar se aplasta
#   Jugo.aparecer(c)           entra con un rebote (escala + fundido)
#   Jugo.sacudir(c)            tiembla (daño, "no puedes")
#   Jugo.latido(c)             un pulso (algo ha cambiado: llave, turno, PA)
#   Jugo.barra(fondo, valor)   barra con "daño fantasma": la parte perdida se queda clara y se va después

const T_HOVER := 0.16
const ESCALA_HOVER := 1.05
const ESCALA_PULSADO := 0.95

## El pivote en el centro (y que siga ahí si cambia de tamaño)
static func centrar_pivote(c: Control) -> void:
	c.pivot_offset = c.size * 0.5
	if not c.resized.is_connected(_recentrar.bind(c)):
		c.resized.connect(_recentrar.bind(c))

static func _recentrar(c: Control) -> void:
	c.pivot_offset = c.size * 0.5

static func _tween(c: Control, clave: String) -> Tween:
	# Un tween por propiedad: el nuevo mata al anterior (si no, se pelean)
	var previo = c.get_meta(clave) if c.has_meta(clave) else null
	if previo is Tween and previo.is_valid():
		previo.kill()
	var t := c.create_tween()
	c.set_meta(clave, t)
	return t

static func escalar(c: Control, escala: float, tiempo: float = T_HOVER) -> void:
	if not c.is_inside_tree():
		c.scale = Vector2.ONE * escala
		return
	var t := _tween(c, "_jugo_escala")
	t.tween_property(c, "scale", Vector2.ONE * escala, tiempo).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)

## Giro de reposo (cartas un poco torcidas): sacudir y enderezar vuelven aquí
static func torcer(c: Control, grados: float) -> void:
	c.set_meta("_jugo_rot", deg_to_rad(grados))
	c.rotation = deg_to_rad(grados)

static func _rot_base(c: Control) -> float:
	return float(c.get_meta("_jugo_rot", 0.0))

static func girar(c: Control, rot: float, tiempo: float = T_HOVER) -> void:
	if not c.is_inside_tree():
		c.rotation = rot
		return
	var t := _tween(c, "_jugo_giro")
	t.tween_property(c, "rotation", rot, tiempo).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)

## Botón con vida: al pasar crece y se endereza, se aplasta al pulsar y rebota al soltar
static func boton(b: BaseButton, escala_hover: float = ESCALA_HOVER) -> void:
	centrar_pivote(b)
	b.mouse_entered.connect(func():
		if not b.disabled:
			escalar(b, escala_hover)
			girar(b, 0.0))
	b.mouse_exited.connect(func():
		escalar(b, 1.0)
		girar(b, _rot_base(b), 0.2))
	b.button_down.connect(func(): escalar(b, ESCALA_PULSADO, 0.06))
	b.button_up.connect(func(): escalar(b, escala_hover if b.is_hovered() else 1.0, 0.18))

## Al pasar el ratón sube 'px' (piezas sueltas, no dentro de un contenedor: la mano de cartas).
## La altura de reposo es la meta "_y_base" (la pone quien coloca la pieza)
static func elevar(b: Control, px: float) -> void:
	b.mouse_entered.connect(func():
		if b is BaseButton and b.disabled:
			return
		var t := _tween(b, "_jugo_pos")
		t.tween_property(b, "position:y", float(b.get_meta("_y_base", b.position.y)) - px, T_HOVER).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT))
	b.mouse_exited.connect(func():
		var t := _tween(b, "_jugo_pos")
		t.tween_property(b, "position:y", float(b.get_meta("_y_base", b.position.y)), 0.2).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT))

## Entra con un rebote desde pequeño y transparente
static func aparecer(c: Control, retraso: float = 0.0) -> void:
	if not c.is_inside_tree():
		return
	centrar_pivote(c)
	c.scale = Vector2.ONE * 0.88
	c.modulate.a = 0.0
	var t := _tween(c, "_jugo_escala")
	t.set_parallel(true)
	t.tween_property(c, "scale", Vector2.ONE, 0.28).set_delay(retraso).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	t.tween_property(c, "modulate:a", 1.0, 0.2).set_delay(retraso)

## Pulso corto (algo importante ha cambiado)
static func latido(c: Control, fuerza: float = 1.08) -> void:
	if not c.is_inside_tree():
		return
	centrar_pivote(c)
	var t := _tween(c, "_jugo_escala")
	t.tween_property(c, "scale", Vector2.ONE * fuerza, 0.1).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	t.tween_property(c, "scale", Vector2.ONE, 0.3).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)

## Tiembla de lado a lado (girando un poco: no mueve el layout)
static func sacudir(c: Control, fuerza: float = 0.06) -> void:
	if not c.is_inside_tree():
		return
	centrar_pivote(c)
	var t := _tween(c, "_jugo_giro")
	for i in 4:
		var lado := 1.0 if i % 2 == 0 else -1.0
		t.tween_property(c, "rotation", _rot_base(c) + lado * fuerza * (1.0 - i * 0.22), 0.05)
	t.tween_property(c, "rotation", _rot_base(c), 0.05)

## Destello de color que se apaga (daño = rojo, cura = verde)
static func destello(c: CanvasItem, color: Color, tiempo: float = 0.35) -> void:
	if not c.is_inside_tree():
		return
	var t := c.create_tween()
	c.self_modulate = color
	t.tween_property(c, "self_modulate", Color.WHITE, tiempo)

## Barra de vida/recurso con "daño fantasma". 'fondo' es el ColorRect del fondo y tiene dos hijos:
## "Fantasma" (claro, se va tarde) y el relleno 'relleno' (el color de la barra).
static func barra(fondo: Control, relleno: Control, fraccion: float) -> void:
	fraccion = clampf(fraccion, 0.0, 1.0)
	var ancho := fondo.size.x * fraccion
	var fantasma := fondo.get_node_or_null("Fantasma") as Control
	var previo := float(fondo.get_meta("_fraccion", -1.0))
	fondo.set_meta("_fraccion", fraccion)
	if previo < 0.0 or not fondo.is_inside_tree():
		relleno.size = Vector2(ancho, fondo.size.y)
		if fantasma:
			fantasma.size = relleno.size
		return
	if is_equal_approx(previo, fraccion):
		# Si cambia el tamaño del fondo, que el relleno lo siga
		if not is_equal_approx(relleno.size.x, ancho):
			relleno.size = Vector2(ancho, fondo.size.y)
			if fantasma:
				fantasma.size = relleno.size
		return
	var t := _tween(relleno, "_jugo_barra")
	t.tween_property(relleno, "size:x", ancho, 0.18).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	if fantasma:
		if fraccion < previo:
			var tf := _tween(fantasma, "_jugo_barra")
			tf.tween_interval(0.35)
			tf.tween_property(fantasma, "size:x", ancho, 0.4).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_IN_OUT)
		else:
			var previo_t = fantasma.get_meta("_jugo_barra") if fantasma.has_meta("_jugo_barra") else null
			if previo_t is Tween and previo_t.is_valid():
				previo_t.kill()
			fantasma.size.x = ancho
	if fraccion < previo:
		destello(relleno, Color(2.2, 2.2, 2.2))
