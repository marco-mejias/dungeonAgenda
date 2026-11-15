extends Control

# Strip that crosses the screen when changing hero ("IRONCAT'S TURN") or day ("DAY 2").
# anunciar(texto): the same strip, shorter, when casting an ability ("SUMMON CHICKEN").

var ORO: Color = Estilo.color("oro")

var banda: ColorRect
var linea_sup: ColorRect
var linea_inf: ColorRect
var texto: Label
var tween: Tween
var ultimo_heroe := ""
var ultimo_dia := -1
var pendiente := ""
var turno_enemigo := false
var bloqueando := false # The current message blocks acting until it disappears

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_preset(Control.PRESET_TOP_WIDE)
	offset_top = 150
	offset_bottom = 206
	modulate.a = 0.0

	banda = ColorRect.new()
	banda.color = Color(Estilo.color("piedra_oscura"), 0.9)
	banda.set_anchors_preset(Control.PRESET_FULL_RECT)
	banda.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(banda)
	linea_sup = _linea(0)
	linea_inf = _linea(1)

	texto = Label.new()
	texto.set_anchors_preset(Control.PRESET_FULL_RECT)
	texto.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	texto.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	Estilo.como_titulo(texto, 32)
	texto.add_theme_color_override("font_color", ORO)
	texto.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(texto)

# Thin golden line above or below the strip
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
	# While a turn or day change is being read, the DungeonManager and the HUD don't take actions
	var dm_b := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	if dm_b:
		var activo := bloqueando and tween != null and tween.is_running() and not Engine.has_meta("test_sin_espera_banner")
		if bool(dm_b.get_meta("bloqueo_banner", false)) != activo:
			dm_b.set_meta("bloqueo_banner", activo)
	var pm := get_node_or_null("/root/Main/PartyManager")
	if not pm:
		return
	var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	var enemigos: bool = dm.get_meta("turno_enemigo", false) if dm else false
	if enemigos != turno_enemigo:
		turno_enemigo = enemigos
		if enemigos:
			mostrar("Turno enemigo")
			return
	var dia: int = pm.get_meta("turno_actual", 1)
	var act = pm.call("GetActiveCharacter")
	var nombre := str(act.get_meta("nombre_display", act.name)) if act else ""

	if ultimo_dia == -1:
		# First frame: we only remember (the entry transition already announces the floor)
		ultimo_dia = dia
		ultimo_heroe = nombre
		return
	var explorando: bool = not Modo.con_turnos()
	if dia != ultimo_dia:
		ultimo_dia = dia
		ultimo_heroe = nombre
		# While exploring the day goes by walking: we tell the player, but without stopping them
		mostrar("Día %d" % dia, 0.85, not explorando)
		pendiente = "Turno de %s" % nombre if nombre != "" and not explorando else ""
	elif nombre != "" and nombre != ultimo_heroe:
		ultimo_heroe = nombre
		# While exploring there are no turns: switching heroes isn't announced
		if not explorando:
			mostrar("Turno de %s" % nombre)

## Ability cast: the turn strip but shorter (it doesn't slow combat down)
func anunciar(mensaje: String) -> void:
	mostrar(mensaje, 0.45, false) # An ability doesn't block: the player is already doing it

# Shows the strip with 'mensaje'; if 'bloquea', actions wait until it's gone
func mostrar(mensaje: String, lectura: float = 0.85, bloquea: bool = true) -> void:
	# We don't cover events or the campfire
	var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	var fogata := get_node_or_null("/root/Main/CanvasLayer3")
	if (dm and dm.get_meta("bloqueo_evento", false)) or (fogata and fogata.visible):
		return
	bloqueando = bloquea
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
	tween.tween_interval(lectura)
	tween.tween_property(self, "modulate:a", 0.0, 0.25)
	tween.tween_callback(_siguiente)

# If a message was queued while another one was showing, now it's its turn
func _siguiente() -> void:
	if pendiente != "":
		var m := pendiente
		pendiente = ""
		mostrar(m)
