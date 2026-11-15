extends PanelContainer

# Bocadillo de un personaje (nodo "Prevision" del HUD). Sale al pasar el ratón:
#   - sobre un héroe o un enemigo del mapa (encima de su cabeza)
#   - sobre su retrato en la barra de arriba (debajo del retrato)
# Corto por defecto: nombre, vida y lo justo (héroe: sus PA; enemigo: fuerza y defensa).
# Manteniendo Mayús salen todas sus stats.
# Si es un enemigo al que el héroe activo puede atacar, en vez de eso: solo las stats que toca el
# golpe ("Vida 10 → 5") y la probabilidad de acertar.

const STATS := ["fuerza", "defensa", "magia", "velocidad", "suerte", "radio_luz"]
const STATS_ENEMIGO_CORTO := ["fuerza", "defensa"]
const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
# Aspecto de grimorio (como el HUD): placa de cuero con filete dorado, texto hueso, títulos en IM Fell.
# Grande a propósito: los números de los iconos tienen que leerse de un vistazo
const FUENTE_TITULO := preload("res://Assets/Fonts/imfell/grimorio_titulo.tres")
const FUENTE_FUERTE := preload("res://Assets/Fonts/alegreya/alegreya_fuerte.tres")
const C_HUESO := Color(0.93, 0.87, 0.70)
const C_DORADO := Color(0.85, 0.67, 0.34)
const C_SUAVE := Color(0.66, 0.58, 0.45)
const C_BIEN := Color(0.62, 0.86, 0.48)
const C_MAL := Color(1.0, 0.50, 0.40)
const C_VIDA := Color(0.80, 0.16, 0.13)
const TAM_ICONO := 26.0
const SOMBRA := 4

var titulo: Label
var filas: VBoxContainer   # modo ataque: stats tocadas
var rejilla: GridContainer # modo stats
var acierto: Label
var aviso: Label
var pista: Label
var razones: Label
var lbl_estados: Label
var clave_pintada := ""

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	visible = false
	set_anchors_preset(Control.PRESET_TOP_LEFT)
	var caja := StyleBoxTexture.new()
	caja.texture = load("res://Assets/UI/Grimorio/cuero.png")
	for lado in [SIDE_LEFT, SIDE_TOP, SIDE_RIGHT, SIDE_BOTTOM]:
		caja.set_texture_margin(lado, 12 + SOMBRA)
		caja.set_content_margin(lado, 12 + SOMBRA)
	add_theme_stylebox_override("panel", caja)
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 6)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(v)
	titulo = _label(v, 17, C_DORADO)
	titulo.add_theme_font_override("font", FUENTE_TITULO)
	filas = VBoxContainer.new()
	filas.add_theme_constant_override("separation", 2)
	filas.mouse_filter = Control.MOUSE_FILTER_IGNORE
	v.add_child(filas)
	rejilla = GridContainer.new()
	rejilla.columns = 3
	rejilla.add_theme_constant_override("h_separation", 18)
	rejilla.add_theme_constant_override("v_separation", 6)
	rejilla.mouse_filter = Control.MOUSE_FILTER_IGNORE
	v.add_child(rejilla)
	lbl_estados = _label(v, 14, C_DORADO)
	acierto = _label(v, 19, C_HUESO)
	acierto.add_theme_font_override("font", FUENTE_FUERTE)
	razones = _label(v, 14, C_BIEN)
	aviso = _label(v, 14, C_MAL)
	pista = _label(v, 12, C_SUAVE)
	pista.text = "Mayús: todas sus stats"

func _label(padre: Control, tam: int, color: Color) -> Label:
	var l := Label.new()
	l.add_theme_font_size_override("font_size", tam)
	l.add_theme_color_override("font_color", color)
	l.add_theme_color_override("font_outline_color", Color(0.06, 0.035, 0.02))
	l.add_theme_constant_override("outline_size", 4)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	padre.add_child(l)
	return l

func _icono(tex: Texture2D, tam: float) -> TextureRect:
	var ic := TextureRect.new()
	ic.texture = tex
	ic.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	ic.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	ic.custom_minimum_size = Vector2(tam, tam)
	ic.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return ic

# ¿A quién señala el ratón? [nodo con las stats, ficha 3D o null, slot de la barra o null]
func _objetivo(dm: Node) -> Array:
	var hud := get_parent().get_node_or_null("HUD")
	var slot = hud.get_meta("slot_hover") if hud and hud.has_meta("slot_hover") else null
	if slot is Control and is_instance_valid(slot) and slot.visible and slot.has_meta("personaje"):
		var p = slot.get_meta("personaje")
		if is_instance_valid(p):
			return [p, _ficha(dm, p), slot]
	var celda: Vector2i = dm.get_meta("hover_celda", Vector2i(-1, -1))
	if celda.x < 0:
		return [null, null, null]
	var e := dm.get_node_or_null("Enemy_%d_%d" % [celda.x, celda.y])
	if e and e.visible:
		return [e, e, null]
	var pm := get_node_or_null("/root/Main/PartyManager")
	if pm:
		for h in pm.get_children():
			if h.has_meta("grid_pos") and h.get_meta("grid_pos") == celda and not h.get_meta("ha_escapado", false):
				return [h, _ficha(dm, h), null]
	return [null, null, null]

func _ficha(dm: Node, p: Node) -> Node3D:
	return p if p is Node3D else dm.get_node_or_null(NodePath(str(p.name))) as Node3D

func _process(_delta: float) -> void:
	var dm := get_node_or_null(RUTA_DM)
	if not dm or dm.get_meta("bloqueo_evento", false) or get_tree().paused:
		visible = false
		return
	var cam := dm.get_node_or_null("Camera3D")
	if cam and cam.has_meta("encuadre"):
		visible = false # durante un golpe o una habilidad no tapa los efectos
		return
	var o := _objetivo(dm)
	var p = o[0]
	if p == null:
		visible = false
		return
	# Enemigo al alcance del héroe activo: previsión del golpe
	var d: Dictionary = {}
	if p is Node3D and str(p.name).begins_with("Enemy_") and not dm.get_meta("turno_enemigo", false):
		var c: Vector2i = p.get_meta("grid_pos")
		var combate := dm.get_node_or_null("CombatManager")
		d = combate.call("PrevisionAtaque", c.x, c.y) if combate else {}
		if not d.get("en_alcance", false):
			d = {}
	visible = true
	if d.get("valido", false):
		_modo_ataque(d)
	else:
		_modo_stats(p)
	size = get_combined_minimum_size()
	_colocar(dm, o[1], o[2])

func _modo_ataque(d: Dictionary) -> void:
	titulo.text = str(d["defensor"]).to_upper()
	rejilla.visible = false
	filas.visible = true
	acierto.visible = true
	acierto.text = "%d%% de acertar" % ceili(float(d.get("prob_acierto", 100)))
	pista.visible = false
	lbl_estados.visible = false
	# Por qué: flanqueo, altura, cobertura, ayuda...
	var r: PackedStringArray = d.get("razones", PackedStringArray())
	var v := int(d.get("ventaja", 0))
	razones.visible = not r.is_empty()
	razones.text = ("▲ Ventaja: " if v > 0 else ("▼ Desventaja: " if v < 0 else "")) + ", ".join(r)
	razones.add_theme_color_override("font_color", C_BIEN if v >= 0 else C_MAL)
	var stats: Array = d.get("stats", [])
	var clave := "ataque" + str(stats)
	if clave != clave_pintada:
		clave_pintada = clave
		for c in filas.get_children():
			c.queue_free()
		for s in stats:
			filas.add_child(_fila(s))
	if int(d["pa"]) < int(d["coste"]):
		aviso.text = "Faltan PA (%d)" % d["coste"]
	else:
		aviso.text = ""
	aviso.visible = aviso.text != ""

# Sus stats, como el bocadillo del retrato: vida arriba y una rejilla de icono + valor
func _modo_stats(p: Node) -> void:
	titulo.text = str(p.get_meta("nombre_display", p.name)).to_upper()
	filas.visible = true
	rejilla.visible = true
	acierto.visible = false
	aviso.visible = false
	razones.visible = false
	# Sus estados (ardiendo, aturdido...)
	var est: Dictionary = Estados.de(p)
	var partes := []
	for e in est:
		partes.append("%s %d" % [Estados.nombre(e), int(est[e]["dias"])])
	# Sigilo: qué clase de guardia es y si os ha visto
	if str(p.name).begins_with("Enemy_") and p.has_meta("vigilancia"):
		var tipo_g: String = Sigilo.NOMBRES.get(Sigilo.tipo(p), "")
		partes.push_front(tipo_g + (" · os ha visto" if p.get_meta("in_combat", false) else " · no os ha visto"))
	lbl_estados.visible = not partes.is_empty()
	lbl_estados.text = " · ".join(partes)
	var vida := {"stat": "hp_actual", "actual": int(p.get_meta("hp_actual", 0)), "despues": int(p.get_meta("hp_actual", 0)), "max": int(p.get_meta("hp_max", 1))}
	var completo := Input.is_key_pressed(KEY_SHIFT)
	var es_enemigo := str(p.name).begins_with("Enemy_")
	var valores := []
	if completo:
		for s in STATS:
			if p.has_meta(s):
				valores.append([s, str(int(p.get_meta(s)))])
	elif es_enemigo:
		for s in STATS_ENEMIGO_CORTO:
			if p.has_meta(s):
				valores.append([s, str(int(p.get_meta(s)))])
	else:
		valores.append(["pa_actual", "%d/%d" % [int(p.get_meta("pa_actual", 0)), int(p.get_meta("pa_max", p.get_meta("velocidad", 0)))]])
	rejilla.columns = 3 if completo else 2
	pista.visible = not completo
	var clave := "stats" + str(p.get_instance_id()) + str(vida) + str(valores)
	if clave == clave_pintada:
		return
	clave_pintada = clave
	for c in filas.get_children():
		c.queue_free()
	filas.add_child(_fila(vida))
	for c in rejilla.get_children():
		c.queue_free()
	for par in valores:
		var h := HBoxContainer.new()
		h.add_theme_constant_override("separation", 6)
		h.mouse_filter = Control.MOUSE_FILTER_IGNORE
		h.add_child(_icono(Indicadores.icono(par[0]), TAM_ICONO))
		var l := _label(h, 18, C_HUESO)
		l.add_theme_font_override("font", FUENTE_FUERTE)
		l.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		l.text = str(par[1])
		rejilla.add_child(h)

# Encima de la cabeza (mapa) o debajo del retrato (barra), siempre dentro de la pantalla
func _colocar(dm: Node, ficha: Node3D, slot) -> void:
	var pantalla := get_viewport_rect().size
	var pos := Vector2.ZERO
	if slot is Control:
		var r: Rect2 = slot.get_global_rect()
		pos = Vector2(r.get_center().x - size.x * 0.5, r.end.y + 8)
	elif ficha:
		var cam: Camera3D = dm.get_node_or_null("Camera3D")
		var cont: SubViewportContainer = get_node_or_null("/root/Main/ContenedorMapa")
		if cam and cont and not cam.is_position_behind(ficha.global_position):
			var p: Vector2 = cont.global_position + cam.unproject_position(ficha.global_position + Vector3(0, 1.1, 0)) * cont.stretch_shrink
			pos = p + Vector2(24, -size.y - 8)
	position = pos.clamp(Vector2(8, 8), pantalla - size - Vector2(8, 8))

# Una stat: icono + "Vida 10 → 5" (rojo si baja, verde si sube); la vida lleva su barra
func _fila(s: Dictionary) -> Control:
	var stat := str(s["stat"])
	var antes := int(s["actual"])
	var despues := int(s["despues"])
	var caja := VBoxContainer.new()
	caja.mouse_filter = Control.MOUSE_FILTER_IGNORE
	caja.add_theme_constant_override("separation", 1)
	var h := HBoxContainer.new()
	h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_theme_constant_override("separation", 6)
	caja.add_child(h)
	var tex: Texture2D = Indicadores.icono(stat)
	if tex:
		h.add_child(_icono(tex, TAM_ICONO))
	var color := C_HUESO
	if despues < antes:
		color = C_MAL
	elif despues > antes:
		color = C_BIEN
	var l := _label(h, 17, color)
	l.add_theme_font_override("font", FUENTE_FUERTE)
	l.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var maximo := ("/%d" % int(s["max"])) if s.has("max") and despues == antes else ""
	l.text = "%s %d → %d" % [Indicadores.nombre(stat), antes, despues] if despues != antes else "%s %d%s" % [Indicadores.nombre(stat), antes, maximo]
	if s.has("max"):
		caja.add_child(_barras(int(s["max"]), antes, despues))
	return caja

# Vida: fondo = vida actual, encima = lo que le quedará
func _barras(maximo: int, antes: int, despues: int) -> Control:
	var barras := Control.new()
	barras.custom_minimum_size = Vector2(200, 7)
	barras.mouse_filter = Control.MOUSE_FILTER_IGNORE
	for i in 2:
		var b := ProgressBar.new()
		b.show_percentage = false
		b.set_anchors_preset(Control.PRESET_FULL_RECT)
		b.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var fondo := StyleBoxFlat.new()
		fondo.bg_color = Color(0.05, 0.03, 0.02) if i == 0 else Color(0, 0, 0, 0)
		var relleno := StyleBoxFlat.new()
		relleno.bg_color = C_VIDA.darkened(0.55) if i == 0 else C_VIDA
		b.add_theme_stylebox_override("background", fondo)
		b.add_theme_stylebox_override("fill", relleno)
		b.max_value = maxi(1, maximo)
		b.value = antes if i == 0 else despues
		barras.add_child(b)
	return barras
