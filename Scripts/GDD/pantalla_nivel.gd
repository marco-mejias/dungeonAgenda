extends CanvasLayer

# Level up! What the hero gained and the draft: 1 of 3 abilities (Progreso.opciones_draft). If the
# slots are full you pick which one to forget (or learn nothing). A passive they already have
# stacks. It opens on its own when there are pending level ups (Progreso.pendientes) and the board
# is free. Keys 1-3 pick a card.

const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const NOMBRE_STAT := {"hp_max": "Vida", "fuerza": "Fuerza", "defensa": "Defensa", "magia": "Magia", "suerte": "Suerte", "velocidad": "Velocidad"}

var velo: ColorRect
var panel: PanelContainer
var caja: VBoxContainer
var titulo: Label
var subtitulo: Label
var cartas: HBoxContainer
var pie: Label
var heroe: Node = null
var opciones: Array = []
var eligiendo := "" # Chosen id waiting for the pick of what to forget

func _ready() -> void:
	name = "PantallaNivel"
	layer = 14
	process_mode = Node.PROCESS_MODE_ALWAYS
	velo = ColorRect.new()
	velo.color = Estilo.color("velo")
	velo.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(velo)
	panel = PanelContainer.new()
	panel.add_theme_stylebox_override("panel", Estilo.caja("panel_oro"))
	add_child(panel)
	caja = VBoxContainer.new()
	caja.add_theme_constant_override("separation", 10)
	panel.add_child(caja)
	titulo = Label.new()
	Estilo.como_titulo(titulo, 40)
	titulo.add_theme_color_override("font_color", Estilo.color("oro"))
	titulo.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	caja.add_child(titulo)
	subtitulo = Estilo.etiqueta("", "normal", "hueso")
	subtitulo.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	caja.add_child(subtitulo)
	cartas = HBoxContainer.new()
	cartas.add_theme_constant_override("separation", 12)
	cartas.alignment = BoxContainer.ALIGNMENT_CENTER
	caja.add_child(cartas)
	pie = Estilo.etiqueta("", "pequeno", "texto_suave")
	pie.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	caja.add_child(pie)
	visible = false

func esta_abierta() -> bool:
	return visible

func _dm() -> Node:
	return get_node_or_null(RUTA_DM)

func _process(_delta: float) -> void:
	if visible:
		_centrar()
		return
	if Progreso.pendientes.is_empty() or get_tree().paused:
		return
	var dm := _dm()
	if dm == null or dm.get_meta("is_moving", false) or dm.get_meta("turno_enemigo", false) or dm.get_meta("bloqueo_evento", false) or dm.get_meta("bloqueo_banner", false):
		return
	_abrir(Progreso.pendientes.pop_front())

# Keeps the panel centred on screen
func _centrar() -> void:
	panel.size = panel.get_combined_minimum_size()
	var pantalla := get_viewport().get_visible_rect().size
	panel.position = (pantalla - panel.size) * 0.5

# Opens the draft for a pending level up: what was gained and 3 ability cards
func _abrir(p: Dictionary) -> void:
	var pm := get_node_or_null("/root/Main/PartyManager")
	heroe = pm.get_node_or_null(NodePath(p["heroe"])) if pm else null
	if heroe == null:
		return
	opciones = Progreso.opciones_draft(heroe)
	titulo.text = "¡NIVEL %d!" % int(p["nivel"])
	var gan := []
	for s in p["crecimiento"]:
		gan.append("%s +%d" % [NOMBRE_STAT.get(s, s), p["crecimiento"][s]])
	subtitulo.text = "%s  ·  %s" % [str(heroe.get_meta("nombre_display", heroe.name)), "  ".join(gan)]
	eligiendo = ""
	_pintar_cartas()
	visible = true
	var dm := _dm()
	if dm:
		dm.set_meta("bloqueo_evento", true) # Nothing moves while choosing
	_centrar()

# One card per draft option
func _pintar_cartas() -> void:
	for c in cartas.get_children():
		c.queue_free()
	if opciones.is_empty():
		pie.text = "No queda nada nuevo que aprender. [Clic para seguir]"
		var b := _boton_texto("Seguir", func(): _cerrar())
		cartas.add_child(b)
		return
	pie.text = "Elige una (1-3). Ranuras: %d/%d" % [Progreso.lista(heroe).size(), Progreso.max_ranuras(heroe)]
	for i in opciones.size():
		cartas.add_child(_carta(opciones[i], i))

# An ability card: icon, name, class tag, numbers and description
func _carta(d: HabilidadData, i: int) -> Button:
	var b := Button.new()
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = Vector2(250, 300)
	b.add_theme_stylebox_override("normal", Estilo.caja("panel"))
	b.add_theme_stylebox_override("hover", Estilo.caja("panel_oro"))
	b.add_theme_stylebox_override("pressed", Estilo.caja("panel_oro"))
	b.pressed.connect(elegir.bind(i))
	var v := VBoxContainer.new()
	v.set_anchors_preset(Control.PRESET_FULL_RECT)
	v.offset_left = 14
	v.offset_right = -14
	v.offset_top = 12
	v.offset_bottom = -12
	v.add_theme_constant_override("separation", 6)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(v)
	var cab := HBoxContainer.new()
	cab.add_theme_constant_override("separation", 10)
	cab.mouse_filter = Control.MOUSE_FILTER_IGNORE
	v.add_child(cab)
	var ic := TextureRect.new()
	ic.texture = d.icono
	ic.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	ic.custom_minimum_size = Vector2(48, 48)
	ic.mouse_filter = Control.MOUSE_FILTER_IGNORE
	cab.add_child(ic)
	var nv := VBoxContainer.new()
	nv.mouse_filter = Control.MOUSE_FILTER_IGNORE
	cab.add_child(nv)
	var n := Estilo.etiqueta(d.nombre, "normal", "oro")
	n.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	n.custom_minimum_size = Vector2(160, 0)
	nv.add_child(n)
	nv.add_child(Estilo.etiqueta("%s · %s" % [d.etiqueta_clase(), "ACTIVA" if d.es_activa() else "PASIVA"], "pequeno", "texto_suave"))
	var res := Estilo.etiqueta(d.resumen(), "pequeno", "oro")
	res.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	v.add_child(res)
	var desc := Estilo.etiqueta(d.descripcion, "pequeno", "hueso")
	desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	desc.size_flags_vertical = Control.SIZE_EXPAND_FILL
	v.add_child(desc)
	var ya: Dictionary = Progreso._buscar(heroe, d.obtener_id())
	if ya.size() > 0:
		v.add_child(Estilo.etiqueta("Ya la tienes: se apila (x%d → x%d)" % [ya["pilas"], int(ya["pilas"]) + 1], "pequeno", "bien"))
	var tecla := Estilo.etiqueta("[%d]" % (i + 1), "pequeno", "texto_off")
	tecla.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	v.add_child(tecla)
	for c in v.get_children():
		if c is Control:
			c.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return b

# Plain text button (skip, forget...)
func _boton_texto(t: String, f: Callable) -> Button:
	var b := Button.new()
	b.text = t
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = Vector2(200, 40)
	b.pressed.connect(f)
	return b

## Picks card i (for the tests too)
func elegir(i: int) -> void:
	if not visible or eligiendo != "" or i < 0 or i >= opciones.size():
		return
	var id: String = opciones[i].obtener_id()
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.play_ui("clic")
	if Progreso.necesita_ranura(heroe, id) and Progreso.ranuras_llenas(heroe):
		eligiendo = id
		_pintar_olvidar()
		return
	Progreso.aprender(heroe, id)
	_cerrar()

# Slots full: which one do they forget?
func _pintar_olvidar() -> void:
	for c in cartas.get_children():
		c.queue_free()
	var nueva: HabilidadData = Progreso.datos(eligiendo)
	pie.text = "Ranuras llenas. Para aprender %s, olvida una:" % nueva.nombre
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 6)
	cartas.add_child(v)
	for e in Progreso.lista(heroe):
		var d: HabilidadData = Progreso.datos(e["id"])
		if d:
			v.add_child(_boton_texto("Olvidar %s" % d.nombre, olvidar.bind(str(e["id"]))))
	v.add_child(_boton_texto("No aprender nada", func(): _cerrar()))

# Slots were full: learn the chosen ability forgetting 'id_viejo'
func olvidar(id_viejo: String) -> void:
	if eligiendo == "":
		return
	Progreso.aprender(heroe, eligiendo, id_viejo)
	_cerrar()

# Closes and frees the board again
func _cerrar() -> void:
	visible = false
	eligiendo = ""
	heroe = null
	var dm := _dm()
	if dm:
		dm.set_meta("bloqueo_evento", false)
		dm.call("RefrescarLuces")
	var ui := get_node_or_null("/root/Main/CanvasLayer")
	if ui:
		ui.call("UpdateWarriorUI")

func _unhandled_input(event: InputEvent) -> void:
	if not visible or not (event is InputEventKey and event.pressed and not event.echo):
		return
	var k: int = event.physical_keycode
	if k >= KEY_1 and k <= KEY_3:
		elegir(k - KEY_1)
		get_viewport().set_input_as_handled()
