extends CanvasLayer

# Mochila: inventario y equipo (tecla I, botón MOCHILA del HUD o "Repartir objetos" en la fogata).
#   Izquierda: retratos de los héroes (clic = ver su mochila; soltar un objeto encima = dárselo).
#   Centro: el retrato grande con las 8 ranuras de equipo alrededor del cuerpo y sus stats debajo.
#   Derecha: la rejilla de la mochila y la carga dibujada dentro del icono de la mochila.
#   Al pasar el ratón por un objeto, una ficha flotante lo describe y lo compara con lo puesto.
# Arrastrar: mochila -> ranura = equipar; ranura -> mochila = quitar; mochila -> retrato = dar.
# Doble clic = equipar o usar. Clic derecho = menú (usar, equipar, dar, tirar, quitar).
# Nada de esto cuesta PA. Todo pasa por las funciones del PartyManager (Inventario.hpp).
# Estilo "piedra tallada": cajas y colores del autoload Estilo.

const Textos := preload("res://Scripts/GDD/textos_objetos.gd")

const TAM := 48           # casilla
const COLUMNAS := 6
const FILAS_MINIMAS := 4
const LADO_MUNECO := Vector2(312, 300)
# Ranura -> [nombre, posición en el muñeco]
const RANURAS := {
	"cabeza": ["Cabeza", Vector2(123, 4)],
	"luz": ["Luz", Vector2(18, 26)],
	"mochila": ["Mochila", Vector2(228, 26)],
	"mano_principal": ["Mano princ.", Vector2(18, 118)],
	"mano_secundaria": ["Mano secun.", Vector2(228, 118)],
	"cuerpo": ["Cuerpo", Vector2(123, 196)],
	"accesorio_1": ["Accesorio", Vector2(18, 210)],
	"accesorio_2": ["Accesorio", Vector2(228, 210)],
}

var pm: Node
var cat: Node
var heroe := ""
var seleccion := {} # {"origen": "inv", "indice": i} o {"origen": "eq", "ranura": r}
var firma := ""
var bajo_raton := {}  # casilla bajo el ratón (para la ficha flotante)

var fondo: ColorRect
var marco: PanelContainer
var retratos: VBoxContainer
var muneco: Control
var retrato_grande: TextureRect
var casillas_eq := {} # ranura -> casilla
var lbl_stats: RichTextLabel
var rejilla_inv: GridContainer
var carga_barra: ProgressBar
var carga_icono: TextureRect
var lbl_carga: Label
var det_icono: TextureRect
var det_texto: RichTextLabel
var lbl_msg: Label
var flotante: PanelContainer
var flotante_texto: RichTextLabel
var menu: PopupMenu
var menu_acciones := {} # id -> Callable
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
		boton_hud.visible = not get_tree().paused and puede_abrir() and not marco.visible
	if not marco.visible:
		return
	_refrescar(false)
	if flotante.visible:
		_colocar_flotante()

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
	flotante.visible = false

# ==========================================
# CONSTRUCCIÓN
# ==========================================

func _boton(texto: String, accion: Callable) -> Button:
	var b := Button.new()
	b.text = texto
	b.add_theme_font_size_override("font_size", 14)
	b.focus_mode = Control.FOCUS_NONE
	b.pressed.connect(accion)
	return b

func _texto_rico(tamano: int) -> RichTextLabel:
	var r := RichTextLabel.new()
	r.bbcode_enabled = true
	r.fit_content = true
	r.scroll_active = false
	r.mouse_filter = Control.MOUSE_FILTER_IGNORE
	r.add_theme_font_size_override("normal_font_size", tamano)
	r.add_theme_font_size_override("bold_font_size", tamano + 2)
	r.add_theme_font_size_override("italics_font_size", tamano)
	return r

func _construir() -> void:
	fondo = ColorRect.new()
	fondo.color = Estilo.color("velo")
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
	marco.offset_left = -480
	marco.offset_right = 480
	marco.offset_top = -300
	marco.offset_bottom = 300
	marco.add_theme_stylebox_override("panel", Estilo.caja("panel_oro"))
	add_child(marco)

	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 10)
	marco.add_child(v)

	# Cabecera: título, último mensaje y cerrar
	var cab := HBoxContainer.new()
	cab.add_child(Estilo.etiqueta("MOCHILA", "titulo", "oro"))
	lbl_msg = Estilo.etiqueta("", "pequeno", "oro")
	lbl_msg.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	lbl_msg.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	cab.add_child(lbl_msg)
	cab.add_child(_boton(" X ", cerrar))
	v.add_child(cab)

	var cuerpo := HBoxContainer.new()
	cuerpo.add_theme_constant_override("separation", 14)
	cuerpo.size_flags_vertical = Control.SIZE_EXPAND_FILL
	v.add_child(cuerpo)

	# 1. Retratos
	retratos = VBoxContainer.new()
	retratos.add_theme_constant_override("separation", 8)
	cuerpo.add_child(retratos)

	# 2. Muñeco con las ranuras alrededor + stats
	var col_eq := VBoxContainer.new()
	col_eq.add_theme_constant_override("separation", 8)
	muneco = PanelContainer.new()
	muneco.custom_minimum_size = LADO_MUNECO
	muneco.add_theme_stylebox_override("panel", Estilo.caja("hueco"))
	var lienzo := Control.new()
	lienzo.mouse_filter = Control.MOUSE_FILTER_PASS
	muneco.add_child(lienzo)
	retrato_grande = TextureRect.new()
	retrato_grande.position = Vector2(87, 66)
	retrato_grande.size = Vector2(120, 120)
	retrato_grande.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	retrato_grande.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	retrato_grande.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	retrato_grande.mouse_filter = Control.MOUSE_FILTER_IGNORE
	retrato_grande.modulate = Color(1, 1, 1, 0.9)
	lienzo.add_child(retrato_grande)
	for ranura in RANURAS:
		var pos: Vector2 = RANURAS[ranura][1]
		var casilla := _casilla()
		casilla.position = pos
		casilla.set_meta("clave", {"origen": "eq", "ranura": ranura})
		_conectar_casilla(casilla)
		lienzo.add_child(casilla)
		casillas_eq[ranura] = casilla
		var l := Estilo.etiqueta(RANURAS[ranura][0], "pequeno", "texto_suave")
		l.position = pos + Vector2(-18, TAM + 1)
		l.size = Vector2(TAM + 36, 14)
		l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		l.mouse_filter = Control.MOUSE_FILTER_IGNORE
		lienzo.add_child(l)
	col_eq.add_child(muneco)
	lbl_stats = _texto_rico(13)
	lbl_stats.custom_minimum_size = Vector2(LADO_MUNECO.x, 0)
	col_eq.add_child(lbl_stats)
	cuerpo.add_child(col_eq)

	# 3. Mochila
	var col_inv := VBoxContainer.new()
	col_inv.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	col_inv.add_theme_constant_override("separation", 8)
	var fila_carga := HBoxContainer.new()
	fila_carga.add_theme_constant_override("separation", 8)
	var hueco_carga := Control.new()
	hueco_carga.custom_minimum_size = Vector2(40, 40)
	carga_barra = ProgressBar.new()
	carga_barra.fill_mode = ProgressBar.FILL_BOTTOM_TO_TOP
	carga_barra.show_percentage = false
	carga_barra.set_anchors_preset(Control.PRESET_FULL_RECT)
	carga_barra.add_theme_stylebox_override("background", Estilo.caja("hueco"))
	hueco_carga.add_child(carga_barra)
	carga_icono = TextureRect.new()
	carga_icono.set_anchors_preset(Control.PRESET_FULL_RECT)
	carga_icono.offset_left = 6
	carga_icono.offset_top = 6
	carga_icono.offset_right = -6
	carga_icono.offset_bottom = -6
	carga_icono.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	carga_icono.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	carga_icono.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	hueco_carga.add_child(carga_icono)
	fila_carga.add_child(hueco_carga)
	var textos_carga := VBoxContainer.new()
	textos_carga.add_child(Estilo.etiqueta("INVENTARIO", "normal", "oro"))
	lbl_carga = Estilo.etiqueta("", "pequeno", "texto_suave")
	textos_carga.add_child(lbl_carga)
	fila_carga.add_child(textos_carga)
	col_inv.add_child(fila_carga)

	var zona := PanelContainer.new()
	zona.size_flags_vertical = Control.SIZE_EXPAND_FILL
	zona.add_theme_stylebox_override("panel", Estilo.caja("hueco"))
	zona.set_drag_forwarding(Callable(), _puede_soltar_en_mochila, _soltar_en_mochila)
	rejilla_inv = GridContainer.new()
	rejilla_inv.columns = COLUMNAS
	rejilla_inv.add_theme_constant_override("h_separation", 6)
	rejilla_inv.add_theme_constant_override("v_separation", 6)
	zona.add_child(rejilla_inv)
	col_inv.add_child(zona)
	cuerpo.add_child(col_inv)

	# Ficha del objeto elegido
	var det := PanelContainer.new()
	det.custom_minimum_size = Vector2(0, 92)
	det.add_theme_stylebox_override("panel", Estilo.caja("hueco"))
	var dh := HBoxContainer.new()
	dh.add_theme_constant_override("separation", 12)
	det_icono = TextureRect.new()
	det_icono.custom_minimum_size = Vector2(56, 56)
	det_icono.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	det_icono.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	det_icono.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	dh.add_child(det_icono)
	det_texto = _texto_rico(13)
	det_texto.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	dh.add_child(det_texto)
	det.add_child(dh)
	v.add_child(det)

	# Ficha flotante (al pasar el ratón) y menú de clic derecho
	flotante = PanelContainer.new()
	flotante.add_theme_stylebox_override("panel", Estilo.caja("tooltip"))
	flotante.mouse_filter = Control.MOUSE_FILTER_IGNORE
	flotante.custom_minimum_size = Vector2(280, 0)
	flotante_texto = _texto_rico(13)
	flotante_texto.custom_minimum_size = Vector2(268, 0)
	flotante.add_child(flotante_texto)
	flotante.visible = false
	add_child(flotante)

	menu = PopupMenu.new()
	menu.add_theme_font_size_override("font_size", 14)
	menu.id_pressed.connect(func(id):
		if menu_acciones.has(id):
			menu_acciones[id].call())
	add_child(menu)

func _crear_botones_de_acceso() -> void:
	boton_hud = _boton("MOCHILA (I)", alternar)
	boton_hud.add_theme_font_size_override("font_size", 12)
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

# Una casilla tallada de 48x48 con icono y cantidad
func _casilla() -> PanelContainer:
	var c := PanelContainer.new()
	c.custom_minimum_size = Vector2(TAM, TAM)
	c.size = Vector2(TAM, TAM)
	c.mouse_filter = Control.MOUSE_FILTER_STOP
	var t := TextureRect.new()
	t.name = "Icono"
	t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	t.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	t.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(t)
	var l := Estilo.etiqueta("", "pequeno", "hueso")
	l.name = "Cantidad"
	l.size_flags_horizontal = Control.SIZE_SHRINK_END
	l.size_flags_vertical = Control.SIZE_SHRINK_END
	l.add_theme_constant_override("outline_size", 4)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(l)
	_marcar(c)
	return c

func _rellenar_casilla(c: PanelContainer, icono: Texture2D, cantidad: int) -> void:
	(c.get_node("Icono") as TextureRect).texture = icono
	(c.get_node("Cantidad") as Label).text = str(cantidad) if cantidad > 1 else ""

# Hueco normal, con el ratón encima (borde claro) o elegido (borde de oro)
func _marcar(c: Control) -> void:
	var clave: Dictionary = c.get_meta("clave", {})
	var nombre := "hueco"
	if not clave.is_empty() and clave == seleccion:
		nombre = "hueco_oro"
	elif not clave.is_empty() and clave == bajo_raton:
		nombre = "hueco_hover"
	c.add_theme_stylebox_override("panel", Estilo.caja(nombre))

func _remarcar_todo() -> void:
	for c in casillas_eq.values():
		_marcar(c)
	for c in rejilla_inv.get_children():
		_marcar(c)

func _vista_arrastre(icono: Texture2D) -> Control:
	var t := TextureRect.new()
	t.texture = icono
	t.size = Vector2(TAM, TAM)
	t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	t.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	t.modulate.a = 0.85
	return t

# Ratón, arrastre y menú de una casilla (de equipo o de mochila); lo que contiene se lee de su 'clave'
func _conectar_casilla(c: PanelContainer) -> void:
	c.mouse_entered.connect(func():
		bajo_raton = c.get_meta("clave", {})
		_marcar(c)
		_mostrar_flotante(_objeto_de(bajo_raton)))
	c.mouse_exited.connect(func():
		if bajo_raton == c.get_meta("clave", {}):
			bajo_raton = {}
		_marcar(c)
		flotante.visible = false)
	c.gui_input.connect(func(ev): _al_pulsar(c, ev))
	c.set_drag_forwarding(
		func(_p):
			var clave: Dictionary = c.get_meta("clave", {})
			var o := _objeto_de(clave)
			if o == null:
				return null
			flotante.visible = false
			c.set_drag_preview(_vista_arrastre(o.icono))
			var datos := clave.duplicate()
			datos["id"] = o.obtener_id()
			return datos,
		# Soltar en una ranura de equipo = equipar; en una casilla de la mochila = quitar lo equipado
		func(_p, d): return _puede_equipar(d) if _es_ranura(c) else _puede_soltar_en_mochila(_p, d),
		func(_p, d):
			if _es_ranura(c):
				_equipar(int(d["indice"]))
			else:
				_soltar_en_mochila(_p, d))

func _es_ranura(c: Control) -> bool:
	return c in casillas_eq.values()

func _al_pulsar(c: Control, ev: InputEvent) -> void:
	if not (ev is InputEventMouseButton and ev.pressed):
		return
	var clave: Dictionary = c.get_meta("clave", {})
	var o := _objeto_de(clave)
	if o == null:
		return
	if ev.button_index == MOUSE_BUTTON_LEFT:
		if ev.double_click:
			if clave["origen"] == "eq":
				_quitar(clave["ranura"])
			elif o is EquipoData:
				_equipar(int(clave["indice"]))
			elif not o.efectos_al_usar.is_empty():
				_usar(int(clave["indice"]))
		else:
			_seleccionar(clave)
	elif ev.button_index == MOUSE_BUTTON_RIGHT:
		_seleccionar(clave)
		_abrir_menu(clave, o)

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

func _equipado_para(o: ObjetoData) -> ObjetoData:
	if not (o is EquipoData):
		return null
	return _equipado(Textos.RANURA_DE[o.ranura])

func _objeto_de(clave: Dictionary) -> ObjetoData:
	match clave.get("origen", ""):
		"eq":
			return _equipado(clave["ranura"])
		"inv":
			var inv := _inventario()
			var i := int(clave["indice"])
			return _def(inv[i]["id"]) if i < inv.size() else null
	return null

func _retrato(h: Node) -> Texture2D:
	var ruta := str(h.get_meta("retrato_path", ""))
	return load(ruta) if ruta != "" and ResourceLoader.exists(ruta) else null

func _sonido(nombre: String) -> void:
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.call("play_ui", nombre)

func _resultado(accion: String, error: String) -> void:
	lbl_msg.text = accion + (": hecho" if error == "" else ": " + error)
	lbl_msg.add_theme_color_override("font_color", Estilo.color("oro") if error == "" else Estilo.color("mal"))
	_sonido("clic" if error == "" else "no")
	_refrescar(true)

# ==========================================
# PINTAR
# ==========================================

func _refrescar(forzar: bool) -> void:
	var h := _nodo(heroe)
	if not h:
		return
	# La selección no entra en la firma: elegir un objeto no reconstruye nada (si no, se perdería el arrastre)
	var nueva := var_to_str([heroe, h.get_meta("inventario", []), h.get_meta("equipo", {}), h.get_meta("hp_actual", 0), h.get_meta("pa_actual", 0), _heroes().size()])
	if not forzar and nueva == firma:
		return
	firma = nueva
	_pintar_retratos()
	_pintar_equipo(h)
	_pintar_stats(h)
	_pintar_inventario()
	_pintar_detalle()

func _pintar_retratos() -> void:
	for c in retratos.get_children():
		c.queue_free()
	for h in _heroes():
		var nombre := str(h.name)
		var junto: bool = nombre == heroe or bool(pm.call("PuedenIntercambiar", heroe, nombre))
		var r := PanelContainer.new()
		r.custom_minimum_size = Vector2(64, 64)
		r.add_theme_stylebox_override("panel", Estilo.caja("hueco_oro" if nombre == heroe else "hueco"))
		r.tooltip_text = str(h.get_meta("nombre_display", nombre)) + ("" if junto else "\nEstá lejos: solo podéis intercambiar pegados o en la fogata")
		r.mouse_filter = Control.MOUSE_FILTER_STOP
		var t := TextureRect.new()
		t.texture = _retrato(h)
		t.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		t.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		t.mouse_filter = Control.MOUSE_FILTER_IGNORE
		r.add_child(t)
		if not junto:
			r.modulate = Color(0.55, 0.55, 0.6)
		r.gui_input.connect(func(ev):
			if ev is InputEventMouseButton and ev.pressed and ev.button_index == MOUSE_BUTTON_LEFT:
				heroe = nombre
				seleccion = {}
				lbl_msg.text = ""
				_refrescar(true))
		# Soltar un objeto en el retrato de otro héroe = dárselo
		r.set_drag_forwarding(Callable(), func(_p, d): return _puede_dar(d, nombre), func(_p, d): _dar(int(d["indice"]), nombre))
		retratos.add_child(r)

func _pintar_equipo(h: Node) -> void:
	retrato_grande.texture = _retrato(h)
	for ranura in casillas_eq:
		var o := _equipado(ranura)
		_rellenar_casilla(casillas_eq[ranura], o.icono if o else null, 1)
		_marcar(casillas_eq[ranura])

func _pintar_stats(h: Node) -> void:
	var base: Dictionary = h.get_meta("stats_base", {})
	var hex := func(nombre: String) -> String: return Estilo.color(nombre).to_html(false)
	var t := "[b]%s[/b]  [color=#%s]%s[/color]\n" % [h.get_meta("nombre_display", h.name), hex.call("texto_suave"), h.get_meta("clase", "")]
	t += "[color=#%s]Vida %d/%d[/color]   [color=#%s]PA %d/%d[/color]\n[color=#%s]Cordura %d[/color]   [color=#%s]Hambre %d[/color]\n" % [
		hex.call("vida"), h.get_meta("hp_actual", 0), h.get_meta("hp_max", 0), hex.call("pa"), h.get_meta("pa_actual", 0), h.get_meta("pa_max", 0),
		hex.call("cordura"), h.get_meta("sanidad_actual", 0), hex.call("hambre"), h.get_meta("hambre_actual", 0)]
	var partes: PackedStringArray = []
	for s in ["fuerza", "defensa", "magia", "suerte", "radio_luz"]:
		var b := int(base.get(s, 0))
		var f := int(h.get_meta(s, 0))
		var bono := "" if f == b else "[color=#%s](%+d)[/color]" % [hex.call("bien") if f > b else hex.call("mal"), f - b]
		partes.append("%s %d%s" % [Textos.nombre_stat(s).capitalize(), f, bono])
	t += "   ".join(partes)
	lbl_stats.text = t

func _pintar_inventario() -> void:
	var inv := _inventario()
	var total := maxi(COLUMNAS * FILAS_MINIMAS, int(ceil(inv.size() / float(COLUMNAS))) * COLUMNAS)
	while rejilla_inv.get_child_count() < total:
		var c := _casilla()
		_conectar_casilla(c)
		rejilla_inv.add_child(c)
	for i in rejilla_inv.get_child_count():
		var c: PanelContainer = rejilla_inv.get_child(i)
		if i < inv.size():
			c.set_meta("clave", {"origen": "inv", "indice": i})
			_rellenar_casilla(c, inv[i]["icono"], int(inv[i]["cantidad"]))
		else:
			c.set_meta("clave", {})
			_rellenar_casilla(c, null, 0)
		c.visible = i < total
		_marcar(c)
	# La carga se ve llenando el icono de la mochila
	var peso: float = pm.call("PesoInventario", heroe)
	var cap: float = pm.call("CapacidadInventario", heroe)
	var mochila := _equipado("mochila")
	carga_icono.texture = mochila.icono if mochila else null
	carga_barra.max_value = maxf(cap, 0.1)
	carga_barra.value = peso
	carga_barra.add_theme_stylebox_override("fill", Estilo.caja("barra_vida" if peso >= cap * 0.9 else "barra_pa"))
	lbl_carga.text = "Carga %.1f / %.1f kg  ·  lo equipado no pesa" % [peso, cap]

func _pintar_detalle() -> void:
	var o := _objeto_de(seleccion)
	if o == null:
		det_icono.texture = null
		det_texto.text = "[color=#%s]Pasa el ratón por un objeto para verlo. Doble clic: equipar o usar. Clic derecho: más acciones. Arrastra a una ranura para equiparlo, o al retrato de otro héroe para dárselo.[/color]" % Estilo.color("texto_suave").to_html(false)
		return
	det_icono.texture = o.icono
	det_texto.text = _texto_objeto(o, seleccion)

func _texto_objeto(o: ObjetoData, clave: Dictionary) -> String:
	var t := Textos.detalle(o)
	if clave.get("origen", "") == "inv":
		var comp := Textos.comparar(o, _equipado_para(o))
		if comp != "":
			t += "\n" + comp
	return t

func _mostrar_flotante(o: ObjetoData) -> void:
	if o == null:
		flotante.visible = false
		return
	flotante_texto.text = _texto_objeto(o, bajo_raton)
	flotante.reset_size()
	flotante.visible = true
	_colocar_flotante()

func _colocar_flotante() -> void:
	var raton := flotante.get_viewport().get_mouse_position()
	var pantalla := flotante.get_viewport().get_visible_rect().size
	var pos := raton + Vector2(18, 18)
	if pos.x + flotante.size.x > pantalla.x - 8:
		pos.x = raton.x - flotante.size.x - 12
	if pos.y + flotante.size.y > pantalla.y - 8:
		pos.y = raton.y - flotante.size.y - 12
	flotante.position = pos

func _seleccionar(clave: Dictionary) -> void:
	seleccion = clave
	_remarcar_todo()
	_pintar_detalle()

func _abrir_menu(clave: Dictionary, o: ObjetoData) -> void:
	menu.clear()
	menu_acciones.clear()
	var add := func(texto: String, accion: Callable):
		var id := menu.item_count
		menu.add_item(texto, id)
		menu_acciones[id] = accion
	if clave["origen"] == "eq":
		add.call("Quitar", func(): _quitar(clave["ranura"]))
	else:
		var indice := int(clave["indice"])
		if not o.efectos_al_usar.is_empty():
			add.call("Usar", func(): _usar(indice))
		if o is EquipoData:
			add.call("Equipar", func(): _equipar(indice))
		for h in _heroes():
			var n := str(h.name)
			if n != heroe and bool(pm.call("PuedenIntercambiar", heroe, n)):
				add.call("Dar a %s" % h.get_meta("nombre_display", n), func(): _dar(indice, n))
		menu.add_separator()
		add.call("Tirar", func():
			pm.call("QuitarObjeto", heroe, indice, 1)
			seleccion = {}
			_resultado("Tirar " + o.nombre, ""))
	menu.position = Vector2i(menu.get_viewport().get_mouse_position())
	menu.reset_size()
	menu.popup()

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
