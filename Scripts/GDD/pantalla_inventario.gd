extends CanvasLayer

# Pantalla de inventario y equipo (tecla I, botón MOCHILA del HUD o "Repartir objetos" en la fogata).
#   Izquierda: las 8 ranuras de equipo. Centro: stats finales. Derecha: la mochila (rejilla) y su carga.
#   Abajo: la ficha del objeto elegido, la comparación con lo que llevas puesto y las acciones.
# Arrastrar: de la mochila a una ranura = equipar; de una ranura a la mochila = quitar;
# de la mochila a la pestaña de otro héroe = dárselo (pegados en la mazmorra, o en la fogata).
# Nada de esto cuesta PA. Todo pasa por las funciones del PartyManager (Inventario.hpp).

const Textos := preload("res://Scripts/GDD/textos_objetos.gd")

const RANURAS := [
	["mano_principal", "Mano princ."], ["mano_secundaria", "Mano secun."], ["cabeza", "Cabeza"], ["cuerpo", "Cuerpo"],
	["accesorio_1", "Accesorio"], ["accesorio_2", "Accesorio"], ["luz", "Luz"], ["mochila", "Mochila"],
]
const TAM := 46
const COLUMNAS := 6
const ORO := Color(1.0, 0.8, 0.3)
const AZUL := Color(0.55, 0.6, 0.85)
const FONDO := Color(0.04, 0.05, 0.16, 0.96)

var pm: Node
var cat: Node
var heroe := ""
var seleccion := {} # {"origen": "inv", "indice": i} o {"origen": "eq", "ranura": r}
var firma := ""

var fondo: ColorRect
var marco: PanelContainer
var pestanas: HBoxContainer
var rejilla_eq: GridContainer
var rejilla_inv: GridContainer
var zona_inv: PanelContainer
var lbl_stats: RichTextLabel
var lbl_carga: Label
var barra_carga: ProgressBar
var det_icono: TextureRect
var det_texto: RichTextLabel
var caja_acciones: HBoxContainer
var lbl_msg: Label
var boton_hud: Button
var boton_fogata: Button

func _ready() -> void:
	layer = 12
	pm = get_node_or_null("/root/Main/PartyManager")
	cat = get_node_or_null("/root/Catalogo")
	_construir()
	_crear_botones_de_acceso()
	cerrar()

func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.physical_keycode == KEY_I:
		alternar()
		get_viewport().set_input_as_handled()

func _process(_delta: float) -> void:
	if boton_hud:
		boton_hud.visible = not get_tree().paused and puede_abrir()
	if marco.visible:
		_refrescar(false)

func esta_abierta() -> bool:
	return marco.visible

func puede_abrir() -> bool:
	var dm := get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	if dm and (dm.get_meta("turno_enemigo", false) or dm.get_meta("bloqueo_evento", false)):
		return false
	return pm != null

func alternar() -> void:
	if marco.visible:
		cerrar()
	else:
		abrir()

func abrir(nombre: String = "") -> void:
	if not puede_abrir():
		return
	if nombre != "":
		heroe = nombre
	if heroe == "" or not _nodo(heroe) or _nodo(heroe).get_meta("esta_muerto", false):
		var act = pm.call("GetActiveCharacter")
		heroe = str(act.name) if act else (str(_heroes()[0].name) if not _heroes().is_empty() else "")
	seleccion = {}
	lbl_msg.text = ""
	fondo.visible = true
	marco.visible = true
	_refrescar(true)
	_sonido("open")

func cerrar() -> void:
	fondo.visible = false
	marco.visible = false

# ==========================================
# CONSTRUCCIÓN
# ==========================================

func _estilo(bg: Color, borde: Color, ancho: int = 2) -> StyleBoxFlat:
	var s := StyleBoxFlat.new()
	s.bg_color = bg
	s.border_color = borde
	s.set_border_width_all(ancho)
	s.set_corner_radius_all(3)
	s.set_content_margin_all(6)
	return s

func _etiqueta(texto: String, tamano: int, color: Color) -> Label:
	var l := Label.new()
	l.text = texto
	l.add_theme_font_size_override("font_size", tamano)
	l.add_theme_color_override("font_color", color)
	return l

func _boton(texto: String, accion: Callable) -> Button:
	var b := Button.new()
	b.text = texto
	b.add_theme_font_size_override("font_size", 12)
	b.focus_mode = Control.FOCUS_NONE
	b.pressed.connect(accion)
	return b

func _construir() -> void:
	fondo = ColorRect.new()
	fondo.color = Color(0, 0, 0, 0.55)
	fondo.set_anchors_preset(Control.PRESET_FULL_RECT)
	fondo.gui_input.connect(func(ev):
		if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
			cerrar())
	add_child(fondo)

	marco = PanelContainer.new()
	marco.name = "PantallaInventario"
	marco.anchor_left = 0.5
	marco.anchor_right = 0.5
	marco.anchor_top = 0.5
	marco.anchor_bottom = 0.5
	marco.offset_left = -470
	marco.offset_right = 470
	marco.offset_top = -300
	marco.offset_bottom = 300
	marco.add_theme_stylebox_override("panel", _estilo(FONDO, ORO, 3))
	add_child(marco)

	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 8)
	marco.add_child(v)

	# Cabecera: título, pestañas de héroes y cerrar
	var cab := HBoxContainer.new()
	cab.add_child(_etiqueta("MOCHILA", 22, ORO))
	pestanas = HBoxContainer.new()
	pestanas.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	pestanas.alignment = BoxContainer.ALIGNMENT_CENTER
	cab.add_child(pestanas)
	cab.add_child(_boton("X", cerrar))
	v.add_child(cab)

	var cuerpo := HBoxContainer.new()
	cuerpo.add_theme_constant_override("separation", 16)
	cuerpo.size_flags_vertical = Control.SIZE_EXPAND_FILL
	v.add_child(cuerpo)

	# Equipo
	var col_eq := VBoxContainer.new()
	col_eq.add_child(_etiqueta("EQUIPO", 13, AZUL))
	rejilla_eq = GridContainer.new()
	rejilla_eq.columns = 2
	rejilla_eq.add_theme_constant_override("h_separation", 10)
	rejilla_eq.add_theme_constant_override("v_separation", 6)
	col_eq.add_child(rejilla_eq)
	cuerpo.add_child(col_eq)

	# Stats
	var col_st := VBoxContainer.new()
	col_st.custom_minimum_size = Vector2(250, 0)
	col_st.add_child(_etiqueta("ESTADÍSTICAS", 13, AZUL))
	lbl_stats = RichTextLabel.new()
	lbl_stats.bbcode_enabled = true
	lbl_stats.fit_content = true
	lbl_stats.scroll_active = false
	lbl_stats.add_theme_font_size_override("normal_font_size", 12)
	lbl_stats.add_theme_font_size_override("bold_font_size", 12)
	col_st.add_child(lbl_stats)
	cuerpo.add_child(col_st)

	# Mochila
	var col_inv := VBoxContainer.new()
	col_inv.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	col_inv.add_child(_etiqueta("INVENTARIO  (arrastra a una ranura para equipar)", 13, AZUL))
	zona_inv = PanelContainer.new()
	zona_inv.size_flags_vertical = Control.SIZE_EXPAND_FILL
	zona_inv.add_theme_stylebox_override("panel", _estilo(Color(0.02, 0.03, 0.1, 0.8), Color(0.25, 0.28, 0.5), 1))
	zona_inv.set_drag_forwarding(Callable(), _puede_soltar_en_mochila, _soltar_en_mochila)
	rejilla_inv = GridContainer.new()
	rejilla_inv.columns = COLUMNAS
	rejilla_inv.add_theme_constant_override("h_separation", 6)
	rejilla_inv.add_theme_constant_override("v_separation", 6)
	zona_inv.add_child(rejilla_inv)
	col_inv.add_child(zona_inv)
	lbl_carga = _etiqueta("", 12, AZUL)
	col_inv.add_child(lbl_carga)
	barra_carga = ProgressBar.new()
	barra_carga.custom_minimum_size = Vector2(0, 10)
	barra_carga.show_percentage = false
	col_inv.add_child(barra_carga)
	cuerpo.add_child(col_inv)

	# Ficha del objeto elegido
	var det := PanelContainer.new()
	det.custom_minimum_size = Vector2(0, 130)
	det.add_theme_stylebox_override("panel", _estilo(Color(0.06, 0.07, 0.2, 0.9), Color(0.3, 0.33, 0.6), 1))
	var dh := HBoxContainer.new()
	dh.add_theme_constant_override("separation", 12)
	det_icono = TextureRect.new()
	det_icono.custom_minimum_size = Vector2(64, 64)
	det_icono.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	det_icono.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	det_icono.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	dh.add_child(det_icono)
	var dv := VBoxContainer.new()
	dv.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	det_texto = RichTextLabel.new()
	det_texto.bbcode_enabled = true
	det_texto.fit_content = true
	det_texto.scroll_active = false
	det_texto.add_theme_font_size_override("normal_font_size", 12)
	det_texto.add_theme_font_size_override("bold_font_size", 14)
	dv.add_child(det_texto)
	caja_acciones = HBoxContainer.new()
	dv.add_child(caja_acciones)
	dh.add_child(dv)
	det.add_child(dh)
	v.add_child(det)

	lbl_msg = _etiqueta("", 12, ORO)
	v.add_child(lbl_msg)

func _crear_botones_de_acceso() -> void:
	boton_hud = _boton("MOCHILA (I)", alternar)
	boton_hud.position = Vector2(186, 8)
	boton_hud.size = Vector2(130, 30)
	add_child(boton_hud)
	move_child(boton_hud, 0)
	# En la fogata, para repartir el botín entre todos
	var fogata := get_node_or_null("/root/Main/CanvasLayer3/PantallaFogata")
	if fogata:
		boton_fogata = _boton("Repartir objetos (I)", func(): abrir())
		boton_fogata.name = "BtnRepartir"
		boton_fogata.anchor_left = 0.5
		boton_fogata.anchor_right = 0.5
		boton_fogata.anchor_top = 1.0
		boton_fogata.anchor_bottom = 1.0
		boton_fogata.offset_left = -154
		boton_fogata.offset_right = 154
		boton_fogata.offset_top = -148
		boton_fogata.offset_bottom = -108
		fogata.add_child(boton_fogata)

func _marcar(c: Control, resaltada: bool) -> void:
	c.add_theme_stylebox_override("panel", _estilo(Color(0.09, 0.1, 0.25), ORO if resaltada else Color(0.3, 0.33, 0.6), 2 if resaltada else 1))

# Elegir un objeto: marca su casilla y rellena la ficha (sin tocar el resto)
func _seleccionar(sel: Dictionary) -> void:
	seleccion = sel
	for rejilla in [rejilla_eq, rejilla_inv]:
		for c in rejilla.get_children():
			var casilla: Control = c.get_child(1) if rejilla == rejilla_eq and c.get_child_count() > 1 else c
			if casilla.has_meta("clave"):
				_marcar(casilla, casilla.get_meta("clave") == sel)
	_pintar_detalle()

# Una casilla de 46x46 con icono y cantidad
func _casilla(icono: Texture2D, cantidad: int, resaltada: bool) -> PanelContainer:
	var c := PanelContainer.new()
	c.custom_minimum_size = Vector2(TAM, TAM)
	_marcar(c, resaltada)
	c.mouse_filter = Control.MOUSE_FILTER_STOP
	var t := TextureRect.new()
	t.texture = icono
	t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	t.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	t.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(t)
	if cantidad > 1:
		var l := _etiqueta(str(cantidad), 11, Color.WHITE)
		l.size_flags_horizontal = Control.SIZE_SHRINK_END
		l.size_flags_vertical = Control.SIZE_SHRINK_END
		l.add_theme_color_override("font_outline_color", Color.BLACK)
		l.add_theme_constant_override("outline_size", 4)
		c.add_child(l)
	return c

func _vista_arrastre(icono: Texture2D) -> Control:
	var t := TextureRect.new()
	t.texture = icono
	t.size = Vector2(TAM, TAM)
	t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	t.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	t.modulate.a = 0.8
	return t

# ==========================================
# DATOS
# ==========================================

func _heroes() -> Array:
	var out: Array = []
	if pm:
		for h in pm.get_children():
			if h.name != "DatosEquipo" and not h.get_meta("esta_muerto", false):
				out.append(h)
	return out

func _nodo(nombre: String) -> Node:
	return pm.get_node_or_null(NodePath(nombre)) if pm and nombre != "" else null

func _def(id: String) -> ObjetoData:
	return cat.obtener(id) if cat and id != "" else null

func _inventario() -> Array:
	return pm.call("DescribirInventario", heroe) if heroe != "" else []

func _equipado(ranura: String) -> ObjetoData:
	var h := _nodo(heroe)
	return _def(h.get_meta("equipo", {}).get(ranura, "")) if h else null

# Lo que lleva en la ranura donde iría este objeto (para comparar)
func _equipado_para(o: ObjetoData) -> ObjetoData:
	if not (o is EquipoData):
		return null
	return _equipado(Textos.RANURA_DE[o.ranura])

func _sonido(nombre: String) -> void:
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.call("play_ui", nombre)

func _resultado(accion: String, error: String) -> void:
	lbl_msg.text = accion + (": hecho" if error == "" else ": " + error)
	lbl_msg.add_theme_color_override("font_color", ORO if error == "" else Color(1.0, 0.5, 0.4))
	_sonido("clic" if error == "" else "no")
	_refrescar(true)

# ==========================================
# PINTAR
# ==========================================

func _refrescar(forzar: bool) -> void:
	var h := _nodo(heroe)
	if not h:
		return
	var nueva := var_to_str([heroe, h.get_meta("inventario", []), h.get_meta("equipo", {}), h.get_meta("hp_actual", 0), _heroes().size()])
	if not forzar and nueva == firma:
		return
	firma = nueva
	_pintar_pestanas()
	_pintar_equipo(h)
	_pintar_stats(h)
	_pintar_inventario(h)
	_pintar_detalle()

func _pintar_pestanas() -> void:
	for c in pestanas.get_children():
		c.queue_free()
	for h in _heroes():
		var nombre := str(h.name)
		var b := _boton(str(h.get_meta("nombre_display", nombre)), func():
			heroe = nombre
			seleccion = {}
			lbl_msg.text = ""
			_refrescar(true))
		b.toggle_mode = true
		b.button_pressed = nombre == heroe
		# Soltar un objeto en la pestaña de otro héroe = dárselo
		b.set_drag_forwarding(Callable(), func(_p, d): return _puede_dar(d, nombre), func(_p, d): _dar(int(d["indice"]), nombre))
		var junto: bool = nombre == heroe or bool(pm.call("PuedenIntercambiar", heroe, nombre))
		b.tooltip_text = "" if junto else "Está lejos: solo podéis intercambiar pegados o en la fogata"
		if not junto:
			b.modulate = Color(0.75, 0.75, 0.8)
		pestanas.add_child(b)

func _pintar_equipo(h: Node) -> void:
	for c in rejilla_eq.get_children():
		c.queue_free()
	var eq: Dictionary = h.get_meta("equipo", {})
	for r in RANURAS:
		var ranura: String = r[0]
		var o := _def(eq.get(ranura, ""))
		var caja := VBoxContainer.new()
		caja.add_theme_constant_override("separation", 1)
		caja.add_child(_etiqueta(r[1], 10, AZUL))
		var sel: bool = seleccion.get("origen", "") == "eq" and seleccion.get("ranura", "") == ranura
		var casilla := _casilla(o.icono if o else null, 1, sel)
		casilla.set_meta("clave", {"origen": "eq", "ranura": ranura})
		casilla.tooltip_text = Textos.resumen(o) if o else "Vacía"
		casilla.gui_input.connect(func(ev):
			if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT and o:
				_seleccionar({"origen": "eq", "ranura": ranura}))
		casilla.set_drag_forwarding(
			func(_p):
				if not o:
					return null
				casilla.set_drag_preview(_vista_arrastre(o.icono))
				return {"origen": "eq", "ranura": ranura},
			func(_p, d): return _puede_equipar(d),
			func(_p, d): _equipar(int(d["indice"])))
		caja.add_child(casilla)
		rejilla_eq.add_child(caja)

func _pintar_stats(h: Node) -> void:
	var base: Dictionary = h.get_meta("stats_base", {})
	var t := "[b]%s[/b]  [color=#%s](%s)[/color]\n" % [h.get_meta("nombre_display", h.name), Textos.GRIS, h.get_meta("clase", "")]
	t += "Vida %d/%d · PA %d/%d\nCordura %d/%d · Hambre %d/%d\n" % [h.get_meta("hp_actual", 0), h.get_meta("hp_max", 0),
		h.get_meta("pa_actual", 0), h.get_meta("pa_max", 0), h.get_meta("sanidad_actual", 0), h.get_meta("sanidad_max", 0),
		h.get_meta("hambre_actual", 0), h.get_meta("hambre_max", 0)]
	for s in ["fuerza", "defensa", "magia", "suerte", "radio_luz"]:
		var b := int(base.get(s, 0))
		var f := int(h.get_meta(s, 0))
		var bono := "" if f == b else " [color=#%s](%+d)[/color]" % [Textos.VERDE if f > b else Textos.ROJO, f - b]
		t += "%s: %d%s\n" % [Textos.nombre_stat(s).capitalize(), f, bono]
	var arma := _equipado("mano_principal")
	if arma is ArmaData:
		t += "[color=#%s]Ataque:[/color] %d de base (%s) · %d PA · alcance %s\n" % [Textos.GRIS, arma.dano + int(h.get_meta(arma.stat_ataque, 0)), arma.stat_ataque, arma.coste_pa, Textos._alcance(arma)]
	else:
		t += "[color=#%s]Ataque:[/color] puños, %d de base · 2 PA\n" % [Textos.GRIS, h.get_meta("fuerza", 0)]
	t += "Oro %d · Comida %d" % [h.get_meta("oro", 0), h.get_meta("comida_bolsa", 0)]
	lbl_stats.text = t

func _pintar_inventario(_h: Node) -> void:
	for c in rejilla_inv.get_children():
		c.queue_free()
	var inv := _inventario()
	for e in inv:
		var indice := int(e["indice"])
		var o := _def(e["id"])
		var sel: bool = seleccion.get("origen", "") == "inv" and int(seleccion.get("indice", -1)) == indice
		var casilla := _casilla(e["icono"], int(e["cantidad"]), sel)
		casilla.set_meta("clave", {"origen": "inv", "indice": indice})
		var tip := Textos.resumen(o)
		var comp := Textos.comparar_plano(o, _equipado_para(o))
		casilla.tooltip_text = tip + ("\n" + comp if comp != "" else "")
		casilla.gui_input.connect(func(ev):
			if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
				if ev.double_click and e["equipable"]:
					_equipar(indice)
				elif ev.double_click and e["usable"]:
					_usar(indice)
				else:
					_seleccionar({"origen": "inv", "indice": indice}))
		casilla.set_drag_forwarding(
			func(_p):
				casilla.set_drag_preview(_vista_arrastre(e["icono"]))
				return {"origen": "inv", "indice": indice, "id": e["id"]},
			func(_p, _d): return false,
			func(_p, _d): pass)
		rejilla_inv.add_child(casilla)
	# Casillas vacías para que se vea el hueco
	for i in maxi(0, COLUMNAS * 3 - inv.size()):
		var vacia := _casilla(null, 0, false)
		vacia.mouse_filter = Control.MOUSE_FILTER_PASS
		rejilla_inv.add_child(vacia)
	var peso: float = pm.call("PesoInventario", heroe)
	var cap: float = pm.call("CapacidadInventario", heroe)
	lbl_carga.text = "Carga: %.1f / %.1f kg   (lo equipado no pesa)" % [peso, cap]
	barra_carga.max_value = maxf(cap, 0.1)
	barra_carga.value = peso

func _pintar_detalle() -> void:
	for c in caja_acciones.get_children():
		c.queue_free()
	var o: ObjetoData = null
	if seleccion.get("origen", "") == "eq":
		o = _equipado(seleccion["ranura"])
	elif seleccion.get("origen", "") == "inv":
		var inv := _inventario()
		var i := int(seleccion["indice"])
		if i < inv.size():
			o = _def(inv[i]["id"])
	if o == null:
		det_icono.texture = null
		det_texto.text = "[color=#%s]Elige un objeto. Doble clic: equipar o usar. Arrastra a la pestaña de otro héroe para dárselo.[/color]" % Textos.GRIS
		return
	det_icono.texture = o.icono
	var t := Textos.detalle(o)
	if seleccion["origen"] == "inv":
		var comp := Textos.comparar(o, _equipado_para(o))
		if comp != "":
			t += "\n" + comp
	det_texto.text = t

	if seleccion["origen"] == "eq":
		var ranura: String = seleccion["ranura"]
		caja_acciones.add_child(_boton("Quitar", func(): _quitar(ranura)))
		return
	var indice := int(seleccion["indice"])
	if not o.efectos_al_usar.is_empty():
		caja_acciones.add_child(_boton("Usar", func(): _usar(indice)))
	if o is EquipoData:
		caja_acciones.add_child(_boton("Equipar", func(): _equipar(indice)))
	var destinos := OptionButton.new()
	destinos.focus_mode = Control.FOCUS_NONE
	destinos.add_theme_font_size_override("font_size", 12)
	for h in _heroes():
		var n := str(h.name)
		if n != heroe and bool(pm.call("PuedenIntercambiar", heroe, n)):
			destinos.add_item(str(h.get_meta("nombre_display", n)))
			destinos.set_item_metadata(destinos.item_count - 1, n)
	if destinos.item_count > 0:
		caja_acciones.add_child(destinos)
		caja_acciones.add_child(_boton("Dar", func(): _dar(indice, destinos.get_item_metadata(destinos.selected))))
	else:
		caja_acciones.add_child(_etiqueta("  (nadie a tu lado para darle cosas)", 11, AZUL))
	caja_acciones.add_child(_boton("Tirar", func():
		pm.call("QuitarObjeto", heroe, indice, 1)
		seleccion = {}
		_resultado("Tirar " + o.nombre, "")))

# ==========================================
# ACCIONES
# ==========================================

func _equipar(indice: int) -> void:
	var inv := _inventario()
	var nombre: String = inv[indice]["nombre"] if indice < inv.size() else "?"
	var error: String = pm.call("Equipar", heroe, indice)
	if error == "":
		seleccion = {}
	_resultado("Equipar " + nombre, error)

func _quitar(ranura: String) -> void:
	var o := _equipado(ranura)
	var error: String = pm.call("Desequipar", heroe, ranura)
	if error == "":
		seleccion = {}
	_resultado("Quitar " + (o.nombre if o else ""), error)

func _usar(indice: int) -> void:
	var inv := _inventario()
	var nombre: String = inv[indice]["nombre"] if indice < inv.size() else "?"
	var error: String = pm.call("UsarObjeto", heroe, indice)
	seleccion = {}
	_resultado("Usar " + nombre, error)

func _dar(indice: int, destino: String) -> void:
	var inv := _inventario()
	var nombre: String = inv[indice]["nombre"] if indice < inv.size() else "?"
	var error: String = pm.call("DarObjeto", heroe, indice, destino, 1)
	if error == "":
		seleccion = {}
	_resultado("Dar %s a %s" % [nombre, destino], error)

func _puede_equipar(d) -> bool:
	if not (d is Dictionary) or d.get("origen", "") != "inv":
		return false
	return _def(d.get("id", "")) is EquipoData

func _puede_dar(d, destino: String) -> bool:
	return d is Dictionary and d.get("origen", "") == "inv" and destino != heroe and bool(pm.call("PuedenIntercambiar", heroe, destino))

func _puede_soltar_en_mochila(_p, d) -> bool:
	return d is Dictionary and d.get("origen", "") == "eq"

func _soltar_en_mochila(_p, d) -> void:
	_quitar(d["ranura"])
