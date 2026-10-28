extends CanvasLayer

# Panel de depuración de inventario y equipo (F10).
# Muestra stats base -> finales, el arma, la carga y los efectos temporales del héroe elegido,
# y deja equipar, quitar, usar, dar y tirar objetos, añadir cualquier objeto del catálogo,
# tirar tablas de botín y recargar los .tres editados sin cerrar el juego.
# Todo pasa por las mismas funciones del PartyManager que usará la interfaz de verdad.

const Textos := preload("res://Scripts/GDD/textos_objetos.gd")
const ANCHO := 500.0
const STATS := [
	["hp_max", "Vida máx"], ["velocidad", "Velocidad (PA)"], ["fuerza", "Fuerza"], ["defensa", "Defensa"],
	["magia", "Magia"], ["suerte", "Suerte"], ["radio_luz", "Radio de luz"], ["sanidad_max", "Cordura máx"], ["hambre_max", "Hambre máx"],
]
const RANURAS := [
	["mano_principal", "Mano princ."], ["mano_secundaria", "Mano secun."], ["cabeza", "Cabeza"], ["cuerpo", "Cuerpo"],
	["accesorio_1", "Accesorio 1"], ["accesorio_2", "Accesorio 2"], ["luz", "Luz"], ["mochila", "Mochila"],
]
const VERDE := Color(0.5, 1.0, 0.55)
const ROJO := Color(1.0, 0.5, 0.45)
const GRIS := Color(0.65, 0.7, 0.85)

var pm: Node
var cat: Node
var heroe := ""

var caja_heroes: HFlowContainer
var lbl_stats: RichTextLabel
var lbl_peso: Label
var barra_peso: ProgressBar
var caja_equipo: VBoxContainer
var caja_inv: VBoxContainer
var opt_objeto: OptionButton
var spin_cantidad: SpinBox
var opt_tabla: OptionButton
var lbl_log: Label

var firma := ""
var t_refresco := 0.0

func _ready() -> void:
	layer = 20
	visible = false
	pm = get_node_or_null("/root/Main/PartyManager")
	cat = get_node_or_null("/root/Catalogo")
	_construir()

func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.physical_keycode == KEY_F10:
		alternar()

func alternar() -> void:
	visible = not visible
	if visible:
		if heroe == "" or not _nodo_heroe():
			var act = pm.call("GetActiveCharacter") if pm else null
			heroe = str(act.name) if act else ""
		_rellenar_desplegables()
		_refrescar(true)

func _process(delta: float) -> void:
	if not visible:
		return
	t_refresco -= delta
	if t_refresco <= 0.0:
		t_refresco = 0.3
		_refrescar(false)

# ==========================================
# CONSTRUCCIÓN DEL PANEL
# ==========================================

func _construir() -> void:
	var panel := PanelContainer.new()
	panel.name = "PanelDebug"
	panel.anchor_left = 1.0
	panel.anchor_right = 1.0
	panel.anchor_bottom = 1.0
	panel.offset_left = -ANCHO - 12
	panel.offset_right = -12
	panel.offset_top = 200
	panel.offset_bottom = -12
	panel.add_theme_stylebox_override("panel", Estilo.caja("panel_oro"))
	add_child(panel)

	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	panel.add_child(scroll)
	var v := VBoxContainer.new()
	v.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	v.add_theme_constant_override("separation", 5)
	scroll.add_child(v)

	var titulo := _etiqueta("DEPURACIÓN · INVENTARIO   (F10 para cerrar)", 13, Color(1.0, 0.85, 0.3))
	v.add_child(titulo)

	caja_heroes = HFlowContainer.new()
	v.add_child(caja_heroes)

	lbl_stats = RichTextLabel.new()
	lbl_stats.bbcode_enabled = true
	lbl_stats.fit_content = true
	lbl_stats.scroll_active = false
	lbl_stats.add_theme_font_size_override("normal_font_size", 11)
	lbl_stats.add_theme_font_size_override("bold_font_size", 11)
	v.add_child(lbl_stats)

	lbl_peso = _etiqueta("", 11, GRIS)
	v.add_child(lbl_peso)
	barra_peso = ProgressBar.new()
	barra_peso.custom_minimum_size = Vector2(0, 8)
	barra_peso.show_percentage = false
	v.add_child(barra_peso)

	v.add_child(_cabecera("EQUIPO"))
	caja_equipo = VBoxContainer.new()
	v.add_child(caja_equipo)

	v.add_child(_cabecera("INVENTARIO"))
	caja_inv = VBoxContainer.new()
	v.add_child(caja_inv)

	v.add_child(_cabecera("AÑADIR DEL CATÁLOGO"))
	var fila_add := HBoxContainer.new()
	opt_objeto = OptionButton.new()
	opt_objeto.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	opt_objeto.add_theme_font_size_override("font_size", 11)
	opt_objeto.fit_to_longest_item = false # si no, el desplegable ensancha el panel
	opt_objeto.clip_text = true
	fila_add.add_child(opt_objeto)
	spin_cantidad = SpinBox.new()
	spin_cantidad.min_value = 1
	spin_cantidad.max_value = 99
	spin_cantidad.value = 1
	fila_add.add_child(spin_cantidad)
	fila_add.add_child(_boton("Añadir", _on_anadir))
	fila_add.add_child(_boton("Probar efectos", _on_probar))
	v.add_child(fila_add)

	v.add_child(_cabecera("BOTÍN"))
	var fila_botin := HBoxContainer.new()
	opt_tabla = OptionButton.new()
	opt_tabla.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	opt_tabla.add_theme_font_size_override("font_size", 11)
	opt_tabla.fit_to_longest_item = false
	opt_tabla.clip_text = true
	fila_botin.add_child(opt_tabla)
	fila_botin.add_child(_boton("Tirar tabla al héroe", _on_botin))
	v.add_child(fila_botin)

	v.add_child(_cabecera("ATAJOS"))
	var rap1 := HFlowContainer.new()
	rap1.add_child(_boton("-20 PV", func(): _sumar_meta("hp_actual", -20, 0, "hp_max")))
	rap1.add_child(_boton("Curar", func(): _sumar_meta("hp_actual", 9999, 0, "hp_max")))
	rap1.add_child(_boton("PA máx", func(): _sumar_meta("pa_actual", 99, 0, "pa_max")))
	rap1.add_child(_boton("-30 cordura", func(): _sumar_meta("sanidad_actual", -30, 0, "sanidad_max")))
	rap1.add_child(_boton("+50 oro", func(): _sumar_meta("oro", 50, 0, "")))
	v.add_child(rap1)
	var rap2 := HFlowContainer.new()
	rap2.add_child(_boton("Pasar un día (efectos)", func():
		pm.call("AvanzarDiaEfectos", heroe)
		_log("Un día menos para los efectos temporales")))
	rap2.add_child(_boton("Recargar .tres", _on_recargar))
	v.add_child(rap2)

	lbl_log = _etiqueta("", 11, Color(1.0, 0.85, 0.4))
	lbl_log.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	v.add_child(lbl_log)

func _etiqueta(texto: String, tamano: int, color: Color) -> Label:
	var l := Label.new()
	l.text = texto
	l.add_theme_font_size_override("font_size", tamano)
	l.add_theme_color_override("font_color", color)
	return l

func _cabecera(texto: String) -> Control:
	var l := _etiqueta(texto, 12, Color(0.55, 0.75, 1.0))
	return l

func _boton(texto: String, accion: Callable) -> Button:
	var b := Button.new()
	b.text = texto
	b.add_theme_font_size_override("font_size", 11)
	b.focus_mode = Control.FOCUS_NONE
	b.pressed.connect(accion)
	return b

func _icono(tex: Texture2D) -> TextureRect:
	var t := TextureRect.new()
	t.texture = tex
	t.custom_minimum_size = Vector2(20, 20)
	t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	t.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	return t

func _log(texto: String) -> void:
	lbl_log.text = texto
	print("[DebugInventario] ", texto)
	_refrescar(true)

# ==========================================
# DATOS
# ==========================================

func _heroes() -> Array:
	var out: Array = []
	if pm:
		for h in pm.get_children():
			if h.name != "DatosEquipo":
				out.append(h)
	return out

func _nodo_heroe() -> Node:
	return pm.get_node_or_null(NodePath(heroe)) if pm and heroe != "" else null

func _rellenar_desplegables() -> void:
	opt_objeto.clear()
	if cat:
		var ids: Array = cat.objetos.keys()
		ids.sort()
		for id in ids:
			var o = cat.objetos[id]
			opt_objeto.add_icon_item(o.icono, "%s  (%s, %.1f kg)" % [o.nombre_completo(), id, o.peso])
			opt_objeto.set_item_metadata(opt_objeto.item_count - 1, id)
		opt_tabla.clear()
		for id in cat.tablas:
			opt_tabla.add_item(id)

func _refrescar(forzar: bool) -> void:
	var h := _nodo_heroe()
	if not h:
		return
	_pintar_stats(h)
	var nueva := var_to_str([h.get_meta("inventario", []), h.get_meta("equipo", {}), _heroes().size()])
	if forzar or nueva != firma:
		firma = nueva
		_pintar_heroes()
		_pintar_equipo(h)
		_pintar_inventario(h)

func _pintar_heroes() -> void:
	for c in caja_heroes.get_children():
		c.queue_free()
	for h in _heroes():
		var nombre := str(h.name)
		var b := _boton(nombre, func():
			heroe = nombre
			_refrescar(true))
		b.toggle_mode = true
		b.button_pressed = nombre == heroe
		if h.get_meta("esta_muerto", false):
			b.modulate = Color(1, 0.5, 0.5)
		caja_heroes.add_child(b)

func _color_hex(c: Color) -> String:
	return c.to_html(false)

func _pintar_stats(h: Node) -> void:
	var base: Dictionary = h.get_meta("stats_base", {})
	var t := "[b]%s[/b]  ·  clase %s  ·  oro %d  ·  comida %d\n" % [h.get_meta("nombre_display", h.name), h.get_meta("clase", "?"), h.get_meta("oro", 0), h.get_meta("comida_bolsa", 0)]
	t += "Vida %d/%d · PA %d/%d · Cordura %d/%d · Hambre %d/%d\n" % [
		h.get_meta("hp_actual", 0), h.get_meta("hp_max", 0), h.get_meta("pa_actual", 0), h.get_meta("pa_max", 0),
		h.get_meta("sanidad_actual", 0), h.get_meta("sanidad_max", 0), h.get_meta("hambre_actual", 0), h.get_meta("hambre_max", 0)]
	t += "[color=#%s]Stat: base → final[/color]\n" % _color_hex(GRIS)
	var columnas: PackedStringArray = []
	for s in STATS:
		var b := int(base.get(s[0], 0))
		var f := int(h.get_meta(s[0], 0))
		var color := GRIS if f == b else (VERDE if f > b else ROJO)
		columnas.append("%s [color=#%s]%d → %d[/color]" % [s[1], _color_hex(color), b, f])
	for i in range(0, columnas.size(), 2):
		t += "   ".join(columnas.slice(i, i + 2)) + "\n"

	# Arma: lo que usará el combate
	var arma = cat.obtener(h.get_meta("equipo", {}).get("mano_principal", "")) if cat else null
	var suerte := int(h.get_meta("suerte", 0))
	if arma is ArmaData:
		var stat_v := int(h.get_meta(arma.stat_ataque, 0))
		t += "[b]Arma:[/b] %s · daño %d + %s %d = %d (−defensa) · %d PA · alcance %d-%d%s · crít %d%% x%.1f\n" % [
			arma.nombre_completo(), arma.dano, arma.stat_ataque, stat_v, arma.dano + stat_v, arma.coste_pa,
			arma.alcance_min, arma.alcance_max, " (visión)" if arma.requiere_vision else "", mini(100, suerte * 2 + arma.critico_extra), arma.mult_critico]
	else:
		t += "[b]Arma:[/b] puños · daño fuerza %d (−defensa) · 2 PA · alcance 1 · crít %d%% x2\n" % [h.get_meta("fuerza", 0), mini(100, suerte * 2)]

	var temporales: Array = h.get_meta("mods_temporales", [])
	if not temporales.is_empty():
		var partes: PackedStringArray = []
		for m in temporales:
			partes.append("%s %+d (%s)" % [m["stat"], m["valor"], "%d días" % m["turnos"] if int(m["turnos"]) >= 0 else "todo el piso"])
		t += "[b]Efectos temporales:[/b] " + ", ".join(partes) + "\n"
	lbl_stats.text = t

	var peso: float = pm.call("PesoInventario", heroe)
	var cap: float = pm.call("CapacidadInventario", heroe)
	lbl_peso.text = "Carga: %.1f / %.1f kg (lo equipado no pesa)" % [peso, cap]
	barra_peso.max_value = maxf(cap, 0.1)
	barra_peso.value = peso

func _pintar_equipo(h: Node) -> void:
	for c in caja_equipo.get_children():
		c.queue_free()
	var eq: Dictionary = h.get_meta("equipo", {})
	for r in RANURAS:
		var fila := HBoxContainer.new()
		var l := _etiqueta(r[1], 11, GRIS)
		l.custom_minimum_size = Vector2(82, 0)
		fila.add_child(l)
		var o = cat.obtener(eq[r[0]]) if cat and eq.has(r[0]) else null
		fila.add_child(_icono(o.icono if o else null))
		var nombre := _etiqueta(_describir(o) if o else "—", 11, Color.WHITE if o else Color(0.45, 0.45, 0.55))
		nombre.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		nombre.clip_text = true
		fila.add_child(nombre)
		if o:
			var ranura: String = r[0]
			fila.add_child(_boton("Quitar", func():
				var err: String = pm.call("Desequipar", heroe, ranura)
				_log("Quitar %s: %s" % [o.nombre, "OK" if err == "" else err])))
		caja_equipo.add_child(fila)

func _describir(o) -> String:
	return Textos.resumen(o)

func _pintar_inventario(h: Node) -> void:
	for c in caja_inv.get_children():
		c.queue_free()
	var inv: Array = pm.call("DescribirInventario", heroe)
	if inv.is_empty():
		caja_inv.add_child(_etiqueta("(vacío)", 11, Color(0.45, 0.45, 0.55)))
	var otros: Array = []
	for o in _heroes():
		if str(o.name) != heroe:
			otros.append(str(o.name))
	for e in inv:
		var indice := int(e["indice"])
		var fila := HBoxContainer.new()
		fila.add_child(_icono(e["icono"]))
		var l := _etiqueta("%s x%d (%.1f kg)" % [e["nombre"], e["cantidad"], float(e["peso"]) * int(e["cantidad"])], 11, Color.WHITE)
		l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		l.clip_text = true
		l.tooltip_text = _describir(cat.obtener(e["id"])) if cat and cat.obtener(e["id"]) else ""
		l.mouse_filter = Control.MOUSE_FILTER_PASS
		fila.add_child(l)
		if e["usable"]:
			fila.add_child(_boton("Usar", func():
				var err: String = pm.call("UsarObjeto", heroe, indice)
				_log("Usar %s: %s" % [e["nombre"], "OK" if err == "" else err])))
		if e["equipable"]:
			fila.add_child(_boton("Equipar", func():
				var err: String = pm.call("Equipar", heroe, indice)
				_log("Equipar %s: %s" % [e["nombre"], "OK" if err == "" else err])))
		if not otros.is_empty():
			var destino := OptionButton.new()
			destino.add_theme_font_size_override("font_size", 10)
			destino.focus_mode = Control.FOCUS_NONE
			for n in otros:
				destino.add_item(n)
			fila.add_child(destino)
			fila.add_child(_boton("Dar", func():
				var para := destino.get_item_text(destino.selected)
				var err: String = pm.call("DarObjeto", heroe, indice, para, 1)
				_log("Dar %s a %s: %s" % [e["nombre"], para, "OK" if err == "" else err])))
		fila.add_child(_boton("Tirar", func():
			pm.call("QuitarObjeto", heroe, indice, 1)
			_log("Tirado: %s" % e["nombre"])))
		caja_inv.add_child(fila)

# ==========================================
# ACCIONES
# ==========================================

func _on_anadir() -> void:
	if opt_objeto.selected < 0:
		return
	var id: String = opt_objeto.get_item_metadata(opt_objeto.selected)
	var pedidas := int(spin_cantidad.value)
	var entran: int = pm.call("AnadirObjeto", heroe, id, pedidas)
	_log("Añadir %d x %s: entran %d%s" % [pedidas, id, entran, "" if entran == pedidas else " (no cabe más en la mochila)"])

func _on_probar() -> void:
	if opt_objeto.selected < 0:
		return
	var id: String = opt_objeto.get_item_metadata(opt_objeto.selected)
	_log("Efectos de %s: %s" % [id, pm.call("ProbarEfectos", heroe, id)])

func _on_botin() -> void:
	if opt_tabla.selected < 0:
		return
	var tabla := opt_tabla.get_item_text(opt_tabla.selected)
	_log("Tabla '%s': %s" % [tabla, pm.call("DarBotinDebug", heroe, tabla)])

func _on_recargar() -> void:
	cat.recargar(true)
	pm.call("RecalcularTodo")
	_rellenar_desplegables()
	_log("Catálogo recargado del disco: %d objetos, %d tablas%s" % [cat.objetos.size(), cat.tablas.size(), "" if cat.avisos.is_empty() else " · avisos: " + ", ".join(cat.avisos)])

func _sumar_meta(meta: String, delta: int, minimo: int, meta_max: String) -> void:
	var h := _nodo_heroe()
	if not h:
		return
	var v := int(h.get_meta(meta, 0)) + delta
	if meta_max != "":
		v = mini(v, int(h.get_meta(meta_max, v)))
	h.set_meta(meta, maxi(minimo, v))
	var ui := get_node_or_null("/root/Main/CanvasLayer")
	if ui:
		ui.call("UpdateWarriorUI")
		ui.call("ActualizarStatsUI")
	_log("%s = %d" % [meta, h.get_meta(meta)])
