extends Control

# Franja que cruza la pantalla al cambiar de héroe ("TURNO DE IRONCAT") o de día ("DÍA 2").

const ORO := Color(1.0, 0.8, 0.3)

var banda: ColorRect
var linea_sup: ColorRect
var linea_inf: ColorRect
var texto: Label
var tween: Tween
var ultimo_heroe := ""
var ultimo_dia := -1
var pendiente := ""

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_preset(Control.PRESET_TOP_WIDE)
	offset_top = 150
	offset_bottom = 206
	modulate.a = 0.0

	banda = ColorRect.new()
	banda.color = Color(0.03, 0.035, 0.11, 0.88)
	banda.set_anchors_preset(Control.PRESET_FULL_RECT)
	banda.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(banda)
	linea_sup = _linea(0)
	linea_inf = _linea(1)

	texto = Label.new()
	texto.set_anchors_preset(Control.PRESET_FULL_RECT)
	texto.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	texto.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	texto.add_theme_font_size_override("font_size", 26)
	texto.add_theme_color_override("font_color", ORO)
	texto.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(texto)

func _linea(abajo: int) -> ColorRect:
	var l := ColorRect.new()
	l.color = Color(ORO, 0.9)
	l.anchor_right = 1.0
	l.anchor_top = abajo
	l.anchor_bottom = abajo
	l.offset_top = -2 if abajo else 0
	l.offset_bottom = 0 if abajo else 2
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(l)
	return l

func _process(_delta: float) -> void:
	var pm := get_node_or_null("/root/Main/PartyManager")
	if not pm:
		return
	var dia: int = pm.get_meta("turno_actual", 1)
	var act = pm.call("GetActiveCharacter")
	var nombre := str(act.get_meta("nombre_display", act.name)) if act else ""

	if ultimo_dia == -1:
		# Primer frame: solo memorizamos (la transición de entrada ya anuncia el piso)
		ultimo_dia = dia
		ultimo_heroe = nombre
		return
	if dia != ultimo_dia:
		ultimo_dia = dia
		ultimo_heroe = nombre
		mostrar("DÍA %d" % dia)
		pendiente = "TURNO DE %s" % nombre.to_upper() if nombre != "" else ""
	elif nombre != "" and nombre != ultimo_heroe:
		ultimo_heroe = nombre
		mostrar("TURNO DE %s" % nombre.to_upper())

func mostrar(mensaje: String) -> void:
	# No tapamos eventos ni la fogata
	var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	var fogata := get_node_or_null("/root/Main/CanvasLayer3")
	if (dm and dm.get_meta("bloqueo_evento", false)) or (fogata and fogata.visible):
		return
	if tween:
		tween.kill()
	texto.text = mensaje
	pivot_offset = Vector2(size.x * 0.5, size.y * 0.5)
	scale = Vector2(1, 0.1)
	modulate.a = 1.0
	texto.position.x = -260
	tween = create_tween()
	tween.tween_property(self, "scale", Vector2.ONE, 0.14).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tween.parallel().tween_property(texto, "position:x", 0.0, 0.22).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	tween.tween_interval(0.85)
	tween.tween_property(self, "modulate:a", 0.0, 0.25)
	tween.tween_callback(_siguiente)

func _siguiente() -> void:
	if pendiente != "":
		var m := pendiente
		pendiente = ""
		mostrar(m)
