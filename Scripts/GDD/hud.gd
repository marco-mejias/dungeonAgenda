extends Control

# Tactical HUD with a dark GRIMOIRE look: dark leather pieces with a golden trim and parchment
# cards, nothing animated in the background (it has to read effortlessly). The bottom isn't a
# straight bar: loose pieces, each with its own meaning. Everything reacts to the mouse (grows,
# squashes, bounces) and changes are noticeable (health drops with "ghost damage", the portrait
# shakes, new things pop in with a bounce). Textures: Tools/generar_ui_grimorio.gd -> Assets/UI/Grimorio.
#   top left       "Piso 1 · Día 2" and a pill with the mode (Explorando / ¡Combate!). Under it,
#                  the PARTY COLUMN: portrait, health and PA of each hero (click = select).
#   top centre     TURN ORDER in combat or "A la vista" while exploring. Hidden if there's nothing.
#   top right      minimap (its own node)
#   bottom left    LOG of what's happening
#   bottom         hero plate with gauges · basic actions · the HAND of abilities (Z X C V) ·
#                  the BELT of items (5-9) · with turns, PA and the END OF TURN seal ·
#                  backpack, day, far ones
# Colours with a fixed meaning: gold = basic actions, violet = abilities, green = consumables,
# red = enemies. F9 shows the debug button.
#
# Performance: the expensive stuff (inventory, reach, groups) is only recomputed when something
# visible changes (FIRMA) or every REFRESCO seconds; every frame we only place things and read keys.

const Iconos := preload("res://Scripts/GDD/iconos_pixel.gd")
const Textos := preload("res://Scripts/GDD/textos_objetos.gd")
const Jugo := preload("res://Scripts/GDD/ui_jugo.gd")
# Font: Alegreya (old book but clean, reads well small) for everything; IM Fell (old printing press,
# "dirty" edges) only for the big titles
const FUENTE := preload("res://Assets/Fonts/alegreya/alegreya_texto.tres")
const FUENTE_FUERTE := preload("res://Assets/Fonts/alegreya/alegreya_fuerte.tres")
const FUENTE_CURSIVA := preload("res://Assets/Fonts/alegreya/alegreya_cursiva.tres")
const FUENTE_TITULO := preload("res://Assets/Fonts/imfell/grimorio_titulo.tres")
const GRIM_DIR := "res://Assets/UI/Grimorio/"
const HUD_DIR := "res://Assets/UI/HUD/"
const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const MARGEN := 12.0
const TAM_RETRATO := 58.0
const TAM_MEDIDOR := 20.0
const TAM_GRUPO := 30.0   # Portraits in the party column
const TAM_TURNO := 28.0   # Portraits in the turn order
const TAM_CARTA := Vector2(42, 54)  # Ability cards (the hand)
const TAM_ACCION := Vector2(34, 44) # Basic action cards
const TAM_OBJ := 34.0
const SOMBRA := 4         # px of shadow the loose textures carry (Tools/generar_ui_grimorio.gd)
const CON_SOMBRA := ["cuero", "cuero_tira", "carta", "carta_hover", "carta_off", "carta_roja", "sello", "sello_hover", "sello_off"]
const MAX_ENEMIGOS := 6
const MAX_USABLES := 5    # Keys 5-9
const MAX_HAB := 4        # The Mage has 4 slots; the rest, 3
const TECLAS_HAB := [KEY_Z, KEY_X, KEY_C, KEY_V]
const LETRAS_HAB := ["Z", "X", "C", "V"]
const LINEAS_REGISTRO := 5
const VIDA_REGISTRO := 7.0 # Seconds a line lasts
const REFRESCO := 0.15     # At most, how often the expensive stuff is recomputed when nothing changes
const RETRASO_FICHA := 0.12

# Font sizes (pixels of the 1152x648 base canvas; at 1080p they show x1.67)
const TXT_MINI := 9
const TXT_PEQ := 10
const TXT := 11
const TXT_GRANDE := 15
const GIROS := [-1.0, 0.7, -0.5, 0.9, -0.8, 0.4, 1.1, -0.9, 0.6, -0.3] # Barely tilted cards (degrees)

# Grimoire palette: LEATHER covers with a golden trim (bone text), parchment CARDS in a candle tone
# (ink text), sealing wax and violet magic ink
const C_TINTA := Color(0.17, 0.09, 0.05)         # Text and strokes on the cards
const C_TINTA_SUAVE := Color(0.36, 0.24, 0.14)   # Watered ink (secondary)
const C_PERGAMINO := Color(0.90, 0.83, 0.66)     # Bone text on the leather
const C_CUERO := Color(0.17, 0.11, 0.08)
const C_DORADO := Color(0.82, 0.64, 0.32)        # Trims, day, PA in the column
const C_DORADO_OSCURO := Color(0.48, 0.36, 0.18)
const C_HUECO := Color(0.08, 0.05, 0.035)        # Portrait background
const C_HIERRO := Color(0.55, 0.42, 0.22, 0.5)   # Lines and separators
const C_HIERRO_CLARO := Color(0.48, 0.36, 0.18)
const C_VERDE_CLARO := Color(0.58, 0.76, 0.42)   # "Explorando" on the leather
const C_ROJO_CLARO := Color(0.93, 0.42, 0.33)    # "¡Combate!" on the leather
const C_HUESO := C_PERGAMINO
const C_SUAVE := Color(0.68, 0.60, 0.46)
const C_ORO := Color(0.60, 0.38, 0.05)           # Ochre: basic actions
const C_VIOLETA := Color(0.42, 0.22, 0.72)       # Magic ink: abilities
const C_VERDE := Color(0.24, 0.42, 0.14)         # Consumables
const C_ROJO := Color(0.62, 0.10, 0.08)          # Red ink / sealing wax
const C_VIDA := Color(0.58, 0.07, 0.06)
const C_MAGIA := Color(0.62, 0.38, 1.0)

var pm: Node
var dm: Node
var ui: Node

# Top
var lbl_piso: Label
var lbl_dia: Label
var lbl_modo: Label
var pastilla_modo: PanelContainer
var columna: VBoxContainer
var slots := {}            # name -> Button (party column)
var rayas_grupo: Array = []
var barra_turno: PanelContainer
var lbl_turno: Label
var fila_turno_heroes: HBoxContainer
var separador: Control
var fila_enemigos: HBoxContainer
var slots_enemigo: Array = []
var minis_turno := {}      # name -> TextureRect (heroes with PA in the turn order)

# Bar (not a bar any more: loose pieces inside a full screen Control)
var barra: Control
var placa_heroe: PanelContainer
var acciones: Control
var mano: Control
var cinturon: PanelContainer
var grupo_sello: HBoxContainer
var retrato: TextureRect
var marco_retrato: PanelContainer
var medidores := {}        # meta -> Medidor
var caja_pa: VBoxContainer
var fila_pa: HBoxContainer
var lbl_pa: Label
var lbl_nivel: Label
var barra_xp: ColorRect
var fondo_xp: ColorRect
var huecos_acc := {}       # id -> Button (atacar, ayudar, retirada, ocultarse, antorcha)
var huecos_hab: Array = []
var huecos_usables: Array = []
var huecos_equipo := {}    # (no equipment on the bar any more: you check it in the backpack)
var botones := {}          # id -> Button: atacar, ayudar, retirada, pasar, dia, mochila, lejanos
var btn_lejanos: Button
var heroe_pintado := ""
var fotograma_pintado := -1

# Hover cards and log
var ficha_objeto: PanelContainer
var texto_objeto: RichTextLabel
var objeto_hover: Button = null
var t_hover := 0.0
var registro: VBoxContainer
var teclas_previas := {}

# Refresh
var firma_vista := ""
var t_refresco := 0.0
var modo_visto := ""
var pa_visto := -1
var giro_i := 0

const ACCIONES := [
	# id, icon, title, key, text
	["atacar", "espada", "Atacar", "1", "Al enemigo más cercano a tu alcance."],
	["ayudar", "mano", "Ayudar", "2", "A un aliado pegado: [color=#f0c860]+3[/color] a su próximo ataque."],
	["retirada", "res://Assets/Habilidades/retirada.png", "Retirada", "R", "Hasta mañana, alejarte no provoca [color=#f0c860]ataques de oportunidad[/color]."],
	["ocultarse", "oculto", "Ocultarse", "H", "Agachado: pegado a una pared no te ven de lado y, con la antorcha apagada, tampoco desde lejos. Andar oculto cuesta [color=#f0c860]2 PA[/color] por casilla."],
	["antorcha", "antorcha", "Antorcha", "T", "Gratis. Apagada te ven menos... pero tú ves mucho menos."],
]

# ==========================================
# STYLE (cached: creating new StyleBoxes every frame is very expensive)
# ==========================================
var _estilos := {}

# Flat box with border and shadow (cached by its parameters)
func _caja(fondo: Color, borde: Color, grosor: int = 2, radio: int = 5, relleno: int = 4, sombra: int = 0) -> StyleBoxFlat:
	var clave := "%s|%s|%d|%d|%d|%d" % [fondo.to_html(), borde.to_html(), grosor, radio, relleno, sombra]
	if _estilos.has(clave):
		return _estilos[clave]
	var s := StyleBoxFlat.new()
	s.bg_color = fondo
	s.border_color = borde
	s.set_border_width_all(grosor)
	s.set_corner_radius_all(radio)
	s.set_content_margin_all(relleno)
	s.anti_aliasing = true
	if sombra > 0:
		s.shadow_color = Color(0, 0, 0, 0.45)
		s.shadow_size = sombra
		s.shadow_offset = Vector2(0, 2)
	_estilos[clave] = s
	return s

var _texturas := {}

# Box with a grimoire texture (Assets/UI/Grimorio): 9-slice, the borders don't stretch
func _tex(nombre: String, margen: int, relleno: int = 4) -> StyleBoxTexture:
	var clave := "%s|%d|%d" % [nombre, margen, relleno]
	if _texturas.has(clave):
		return _texturas[clave]
	if nombre in CON_SOMBRA:
		margen += SOMBRA
		relleno += SOMBRA
	var s := StyleBoxTexture.new()
	s.texture = load(GRIM_DIR + nombre + ".png")
	s.texture_margin_left = margen
	s.texture_margin_top = margen
	s.texture_margin_right = margen
	s.texture_margin_bottom = margen
	s.set_content_margin_all(relleno)
	_texturas[clave] = s
	return s

# Tilted card (each one slightly different)
func _torcer(c: Control) -> void:
	Jugo.torcer(c, GIROS[giro_i % GIROS.size()])
	giro_i += 1

# Pixel-sharp icon of a fixed size
func _icono_rect(tex: Texture2D, tam: float) -> TextureRect:
	var r := TextureRect.new()
	r.texture = tex
	r.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	r.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	r.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	r.custom_minimum_size = Vector2(tam, tam)
	r.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return r

# Label with the HUD font, size and colour
func _label(tam: int, color: Color = C_TINTA, fuerte: bool = false) -> Label:
	var l := Label.new()
	l.add_theme_font_override("font", FUENTE_FUERTE if fuerte else FUENTE)
	l.add_theme_font_size_override("font_size", tam)
	l.add_theme_color_override("font_color", color)
	# Ink on parchment goes without an outline; light text (on dark), with an ink outline
	if color.get_luminance() > 0.45:
		l.add_theme_color_override("font_outline_color", C_TINTA)
		l.add_theme_constant_override("outline_size", 4)
	else:
		l.add_theme_constant_override("outline_size", 0)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return l

# Only touches the Label when the text really changes (avoids relayouts every frame)
func _set_texto(l: Label, t: String) -> void:
	if l.text != t:
		l.text = t

# Same idea for styleboxes: only override when it changes
func _set_estilo(c: Control, nombre: String, s: StyleBox) -> void:
	var clave := "_estilo_" + nombre
	if not c.has_meta(clave) or c.get_meta(clave) != s:
		c.set_meta(clave, s)
		c.add_theme_stylebox_override(nombre, s)

# Thin bar with background, "ghost" and fill (see Jugo.barra)
func _barra(ancho: float, alto: float, color: Color) -> ColorRect:
	var fondo := ColorRect.new()
	fondo.name = "VidaFondo"
	fondo.color = C_TINTA
	fondo.size = Vector2(ancho, alto)
	fondo.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var fantasma := ColorRect.new()
	fantasma.name = "Fantasma"
	fantasma.color = Color(0.98, 0.86, 0.55, 0.9)
	fantasma.mouse_filter = Control.MOUSE_FILTER_IGNORE
	fondo.add_child(fantasma)
	var vida := ColorRect.new()
	vida.name = "Vida"
	vida.color = color
	vida.mouse_filter = Control.MOUSE_FILTER_IGNORE
	fondo.add_child(vida)
	return fondo

# ==========================================
# LIQUID GAUGE
# ==========================================
class Medidor extends Control:
	var lleno: Texture2D
	var vacio: Texture2D
	var nivel := 1.0      # What gets drawn (moves towards the target little by little)
	var objetivo := 1.0
	var texto: Label
	var t := 0.0

	func _init(nombre_icono: String, tam: float, fuente: Font) -> void:
		lleno = load(HUD_DIR + nombre_icono + "_lleno.png")
		vacio = load(HUD_DIR + nombre_icono + "_vacio.png")
		custom_minimum_size = Vector2(tam, tam)
		mouse_filter = Control.MOUSE_FILTER_STOP
		texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		texto = Label.new()
		texto.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		texto.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
		texto.set_anchors_preset(Control.PRESET_FULL_RECT)
		texto.add_theme_font_override("font", fuente)
		texto.add_theme_font_size_override("font_size", 8)
		texto.add_theme_constant_override("outline_size", 4)
		texto.add_theme_color_override("font_outline_color", Color(0.02, 0.02, 0.03))
		texto.mouse_filter = Control.MOUSE_FILTER_IGNORE
		texto.visible = false
		add_child(texto)
		mouse_entered.connect(func(): texto.visible = true)
		mouse_exited.connect(func(): texto.visible = false)

	func fijar(valor: int, maximo: int) -> void:
		var nuevo := clampf(float(valor) / float(maxi(1, maximo)), 0.0, 1.0)
		if nuevo < objetivo - 0.001 and is_inside_tree():
			Jugo.sacudir(self, 0.08)
		objetivo = nuevo
		var t_nuevo := "%d/%d" % [valor, maximo]
		if texto.text != t_nuevo:
			texto.text = t_nuevo

	func _process(delta: float) -> void:
		# Only redrawn while the liquid moves (or if it's half full, because of the wave)
		var antes := nivel
		nivel = move_toward(nivel, objetivo, delta * 0.8)
		if nivel < 0.999:
			t += delta
			queue_redraw()
		elif antes != nivel:
			queue_redraw()

	func _draw() -> void:
		var r := Rect2(Vector2.ZERO, size)
		if vacio:
			draw_texture_rect(vacio, r, false)
		if not lleno or nivel <= 0.0:
			return
		var tam := lleno.get_size()
		var ola := sin(t * 3.0) * 0.012 if nivel < 0.999 else 0.0
		var f := clampf(nivel + ola, 0.0, 1.0)
		draw_texture_rect_region(lleno, Rect2(0, size.y * (1.0 - f), size.x, size.y * f), Rect2(0, tam.y * (1.0 - f), tam.x, tam.y * f))
		if nivel < 0.999:
			var y := size.y * (1.0 - f)
			draw_texture_rect_region(lleno, Rect2(0, y, size.x, 2), Rect2(0, tam.y * (1.0 - f), tam.x, 1), Color(1.6, 1.6, 1.6, 0.9))

# ==========================================
# ASSEMBLY
# ==========================================
func _ready() -> void:
	add_to_group("hud")
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	pm = get_node_or_null("/root/Main/PartyManager")
	dm = get_node_or_null(RUTA_DM)
	ui = get_parent()
	_ocultar_hud_viejo()
	_montar_cabecera()
	_montar_turno()
	_montar_barra()
	_montar_registro()
	_montar_ficha()

# The old cards, their button and the marker stay in the scene (C++ still fills them) but they're
# hidden. The debug button, hidden until F9 is pressed.
func _ocultar_hud_viejo() -> void:
	for n in ["PanelFichas", "BtnFichas", "MarcadorTurnos", "BtnSalidaDebug"]:
		var c := ui.get_node_or_null(n) as CanvasItem
		if c:
			c.visible = false
	var debug := ui.get_node_or_null("BtnSalidaDebug") as Control
	if debug:
		debug.set_anchors_preset(Control.PRESET_TOP_LEFT)
		debug.size = Vector2(180, 22)
		debug.add_theme_font_size_override("font_size", TXT)
	call_deferred("_quitar_boton_mochila")

# The backpack screen creates its own HUD button; ours replaces it, so we remove that one
func _quitar_boton_mochila() -> void:
	for inv in get_tree().get_nodes_in_group("pantalla_inventario"):
		if inv.boton_hud:
			inv.boton_hud.queue_free()
			inv.boton_hud = null

# --- top left: floor, day, mode and the party column ---
func _montar_cabecera() -> void:
	var tira := PanelContainer.new()
	tira.position = Vector2(MARGEN - 6, MARGEN - 8)
	tira.add_theme_stylebox_override("panel", _tex("cuero_tira", 10, 9))
	tira.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(tira)
	var h := HBoxContainer.new()
	h.add_theme_constant_override("separation", 5)
	h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	tira.add_child(h)
	lbl_piso = _label(TXT_GRANDE, C_HUESO, true)
	lbl_piso.add_theme_font_override("font", FUENTE_TITULO)
	lbl_piso.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	h.add_child(lbl_piso)
	var punto := _label(TXT_GRANDE, C_DORADO_OSCURO, true)
	punto.text = "·"
	punto.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	h.add_child(punto)
	lbl_dia = _label(TXT_GRANDE, C_DORADO, true)
	lbl_dia.add_theme_font_override("font", FUENTE_TITULO)
	lbl_dia.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	h.add_child(lbl_dia)
	pastilla_modo = PanelContainer.new()
	pastilla_modo.add_theme_stylebox_override("panel", StyleBoxEmpty.new())
	pastilla_modo.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	pastilla_modo.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_child(pastilla_modo)
	lbl_modo = _label(TXT, C_VERDE_CLARO)
	lbl_modo.add_theme_font_override("font", FUENTE_CURSIVA)
	pastilla_modo.add_child(lbl_modo)
	columna = VBoxContainer.new()
	columna.add_theme_constant_override("separation", -2)
	columna.position = Vector2(MARGEN - 8, 46)
	columna.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(columna)

# A hero in the column: a leather strip with their portrait in a golden medallion, name, health and
# PA
func _ficha_grupo(nombre: String) -> Button:
	var s := float(SOMBRA)
	var b := Button.new()
	b.name = nombre
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = Vector2(TAM_GRUPO + 112, TAM_GRUPO + 18) + Vector2(s, s) * 2.0
	var tira := _tex("cuero_tira", 10, 0)
	for e in ["normal", "hover", "pressed"]:
		b.add_theme_stylebox_override(e, tira)
	var marco := Panel.new()
	marco.name = "Marco"
	marco.position = Vector2(9, 9) + Vector2(s, s)
	marco.size = Vector2(TAM_GRUPO, TAM_GRUPO)
	marco.mouse_filter = Control.MOUSE_FILTER_IGNORE
	marco.add_theme_stylebox_override("panel", _caja(C_HUECO, C_DORADO_OSCURO, 2, int(TAM_GRUPO / 2), 0))
	b.add_child(marco)
	var r := _icono_rect(null, TAM_GRUPO - 6)
	r.name = "Retrato"
	r.position = Vector2(3, 3)
	r.size = Vector2(TAM_GRUPO - 6, TAM_GRUPO - 6)
	marco.add_child(r) # Inside the frame: so it shakes and grows with it
	var x0 := TAM_GRUPO + 17 + s
	var nom := _label(TXT, C_HUESO, true)
	nom.name = "Nombre"
	nom.position = Vector2(x0, 7 + s)
	nom.size = Vector2(86, 13)
	nom.clip_text = true
	b.add_child(nom)
	var vida := _barra(76, 4, C_VIDA)
	vida.position = Vector2(x0, 23 + s)
	b.add_child(vida)
	var num := _label(TXT_MINI, C_SUAVE)
	num.name = "VidaNum"
	num.position = Vector2(x0, 28 + s)
	b.add_child(num)
	var pa := _label(TXT_PEQ, C_DORADO, true)
	pa.name = "PA"
	pa.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	pa.position = Vector2(x0 + 36, 28 + s)
	pa.size = Vector2(40, 10)
	b.add_child(pa)
	b.pressed.connect(_seleccionar.bind(nombre))
	b.mouse_entered.connect(func():
		set_meta("slot_hover", b)
		Jugo.escalar(marco, 1.06)
		Jugo.girar(b, 0.0))
	b.mouse_exited.connect(func():
		if has_meta("slot_hover") and get_meta("slot_hover") == b:
			remove_meta("slot_hover")
		Jugo.escalar(marco, 1.08 if b.get_meta("activo", false) else 1.0)
		if not b.get_meta("activo", false):
			Jugo.girar(b, float(b.get_meta("_jugo_rot", 0.0)), 0.2))
	Jugo.centrar_pivote(marco)
	Jugo.centrar_pivote(b)
	_torcer(b)
	return b

# Selects a hero through the uiManager (same path as clicking them on the map)
func _seleccionar(nombre: String) -> void:
	if ui:
		ui.call("SeleccionarPersonajeDesdeUI", nombre)

# --- top centre: turn order / in view ---
func _montar_turno() -> void:
	barra_turno = PanelContainer.new()
	barra_turno.add_theme_stylebox_override("panel", _tex("cuero_tira", 10, 8))
	barra_turno.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(barra_turno)
	var h := HBoxContainer.new()
	h.add_theme_constant_override("separation", 3)
	h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	barra_turno.add_child(h)
	lbl_turno = _label(TXT, C_HUESO, true)
	lbl_turno.add_theme_font_override("font", FUENTE_TITULO)
	lbl_turno.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	h.add_child(lbl_turno)
	fila_turno_heroes = HBoxContainer.new()
	fila_turno_heroes.add_theme_constant_override("separation", 3)
	fila_turno_heroes.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_child(fila_turno_heroes)
	separador = ColorRect.new()
	separador.color = C_HIERRO
	separador.custom_minimum_size = Vector2(1, TAM_TURNO - 8)
	separador.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	separador.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_child(separador)
	fila_enemigos = HBoxContainer.new()
	fila_enemigos.add_theme_constant_override("separation", -2)
	fila_enemigos.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_child(fila_enemigos)
	for i in MAX_ENEMIGOS:
		var s := _mini_enemigo()
		s.visible = false
		fila_enemigos.add_child(s)
		slots_enemigo.append(s)

# Small portrait for an enemy in the turn order
func _mini_enemigo() -> Button:
	var s := float(SOMBRA)
	var b := Button.new()
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = Vector2(TAM_TURNO, TAM_TURNO + 4) + Vector2(s, s) * 2.0
	var carta := _tex("carta_roja", 6, 0)
	for e in ["normal", "hover", "pressed"]:
		b.add_theme_stylebox_override(e, carta)
	var r := _icono_rect(null, TAM_TURNO - 8)
	r.name = "Retrato"
	r.position = Vector2(4, 3) + Vector2(s, s)
	r.size = Vector2(TAM_TURNO - 8, TAM_TURNO - 8)
	b.add_child(r)
	var vida := _barra(TAM_TURNO - 10, 3, C_VIDA)
	vida.position = Vector2(5, TAM_TURNO - 4) + Vector2(s, s)
	b.add_child(vida)
	b.mouse_entered.connect(func(): set_meta("slot_hover", b))
	b.mouse_exited.connect(func():
		if has_meta("slot_hover") and get_meta("slot_hover") == b:
			remove_meta("slot_hover"))
	Jugo.boton(b, 1.06)
	_torcer(b)
	return b

# --- bottom: loose pieces, each one with its meaning ---
#   HERO plate (who) · basic ACTIONS (small cards) · the ability HAND (fanned, they rise on
#   hover) · the BELT with the items (pockets) · the end of turn SEAL with the PA on top, and
#   next to it the backpack, the day and "lejanos"
func _montar_barra() -> void:
	barra = Control.new()
	barra.set_anchors_preset(Control.PRESET_FULL_RECT)
	barra.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(barra)

	# 1) The hero: leather plate with portrait, gauges and level
	placa_heroe = PanelContainer.new()
	placa_heroe.add_theme_stylebox_override("panel", _tex("cuero", 12, 8))
	placa_heroe.mouse_filter = Control.MOUSE_FILTER_IGNORE
	barra.add_child(placa_heroe)
	var izq := VBoxContainer.new()
	izq.add_theme_constant_override("separation", 3)
	izq.mouse_filter = Control.MOUSE_FILTER_IGNORE
	placa_heroe.add_child(izq)
	var caja_r := Control.new()
	caja_r.custom_minimum_size = Vector2(TAM_RETRATO + TAM_MEDIDOR - 2, TAM_RETRATO)
	caja_r.mouse_filter = Control.MOUSE_FILTER_IGNORE
	izq.add_child(caja_r)
	marco_retrato = PanelContainer.new()
	marco_retrato.add_theme_stylebox_override("panel", _caja(C_HUECO, C_DORADO, 2, 8, 2))
	marco_retrato.size = Vector2(TAM_RETRATO, TAM_RETRATO)
	marco_retrato.mouse_filter = Control.MOUSE_FILTER_IGNORE
	caja_r.add_child(marco_retrato)
	retrato = _icono_rect(null, TAM_RETRATO - 8)
	marco_retrato.add_child(retrato)
	var col := VBoxContainer.new()
	col.add_theme_constant_override("separation", -1)
	col.position = Vector2(TAM_RETRATO - 2, -1)
	col.mouse_filter = Control.MOUSE_FILTER_IGNORE
	caja_r.add_child(col)
	for par in [["hp_actual", "corazon"], ["hambre_actual", "carne"], ["sanidad_actual", "ojo"]]:
		var m := Medidor.new(par[1], TAM_MEDIDOR, FUENTE_FUERTE)
		m.name = par[0]
		col.add_child(m)
		medidores[par[0]] = m
	var xp := HBoxContainer.new()
	xp.add_theme_constant_override("separation", 4)
	xp.mouse_filter = Control.MOUSE_FILTER_IGNORE
	izq.add_child(xp)
	lbl_nivel = _label(TXT_PEQ, C_HUESO, true)
	xp.add_child(lbl_nivel)
	fondo_xp = ColorRect.new()
	fondo_xp.color = Color(0.04, 0.025, 0.015)
	fondo_xp.custom_minimum_size = Vector2(TAM_RETRATO - 18, 3)
	fondo_xp.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	fondo_xp.mouse_filter = Control.MOUSE_FILTER_PASS
	xp.add_child(fondo_xp)
	barra_xp = ColorRect.new()
	barra_xp.color = C_DORADO
	barra_xp.mouse_filter = Control.MOUSE_FILTER_IGNORE
	fondo_xp.add_child(barra_xp)
	Jugo.centrar_pivote(placa_heroe)
	Jugo.torcer(placa_heroe, -1.2)

	# 2) Basic actions: small cards
	acciones = Control.new()
	acciones.mouse_filter = Control.MOUSE_FILTER_IGNORE
	barra.add_child(acciones)
	for a in ACCIONES:
		var tex: Texture2D = load(a[1]) if a[1].begins_with("res://") else Iconos.textura(a[1], 1)
		var b := _hueco(C_ORO, a[3], TAM_ACCION)
		b.name = "Acc_" + a[0]
		b.get_node("Icono").texture = tex
		b.set_meta("texto", _ficha_texto(a[2], a[3], a[4], C_ORO))
		var id: String = a[0]
		b.pressed.connect(_accion.bind(id))
		if id == "atacar":
			b.mouse_entered.connect(func():
				if dm and not _bloqueado():
					dm.call("MostrarRangoAccion", 0))
			b.mouse_exited.connect(func():
				if dm and not _bloqueado():
					dm.call("OcultarRangoAccion"))
		elif Progreso.acciones.has(id):
			b.mouse_entered.connect(func(): Progreso.mostrar_rango_de(pm.call("GetActiveCharacter") if pm else null, Progreso.acciones.get(id)))
			b.mouse_exited.connect(func(): Progreso.ocultar_rango())
		acciones.add_child(b)
		huecos_acc[id] = b
		botones[id] = b

	# 3) The hand: the abilities, fanned
	mano = Control.new()
	mano.mouse_filter = Control.MOUSE_FILTER_IGNORE
	barra.add_child(mano)
	for i in MAX_HAB:
		var b := _hueco(C_VIOLETA, LETRAS_HAB[i], TAM_CARTA)
		b.name = "Hab_%d" % i
		b.set_meta("habilidad_i", i)
		var velo := ColorRect.new()
		velo.name = "Enfriamiento"
		velo.color = Color(0.08, 0.05, 0.03, 0.72)
		velo.set_anchors_preset(Control.PRESET_FULL_RECT)
		velo.offset_left = SOMBRA + 3
		velo.offset_top = SOMBRA + 3
		velo.offset_right = -SOMBRA - 3
		velo.offset_bottom = -SOMBRA - 3
		velo.mouse_filter = Control.MOUSE_FILTER_IGNORE
		velo.visible = false
		b.add_child(velo)
		var dias := _label(TXT_GRANDE, C_HUESO, true)
		dias.name = "Dias"
		dias.set_anchors_preset(Control.PRESET_FULL_RECT)
		dias.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		dias.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
		velo.add_child(dias)
		b.pressed.connect(func(): Progreso.pulsar(i))
		b.mouse_entered.connect(func(): Progreso.mostrar_rango(i))
		b.mouse_exited.connect(func(): Progreso.ocultar_rango())
		mano.add_child(b)
		huecos_hab.append(b)

	# 4) The belt: single-use items, in pockets
	cinturon = PanelContainer.new()
	cinturon.add_theme_stylebox_override("panel", _tex("cuero", 12, 6))
	cinturon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	barra.add_child(cinturon)
	var fila_o := HBoxContainer.new()
	fila_o.add_theme_constant_override("separation", 4)
	fila_o.mouse_filter = Control.MOUSE_FILTER_IGNORE
	cinturon.add_child(fila_o)
	for i in MAX_USABLES:
		var b := _bolsillo(str(5 + i))
		b.name = "Uso_%d" % i
		b.pressed.connect(_usar_hueco.bind(i))
		fila_o.add_child(b)
		huecos_usables.append(b)
	Jugo.centrar_pivote(cinturon)
	Jugo.torcer(cinturon, 1.0)

	# 5) The seal: PA as diamonds on top and END OF TURN (only with turns); next to it backpack, day
	# and far ones
	grupo_sello = HBoxContainer.new()
	grupo_sello.add_theme_constant_override("separation", 6)
	grupo_sello.alignment = BoxContainer.ALIGNMENT_END
	grupo_sello.mouse_filter = Control.MOUSE_FILTER_IGNORE
	barra.add_child(grupo_sello)
	caja_pa = VBoxContainer.new()
	caja_pa.add_theme_constant_override("separation", 0)
	caja_pa.alignment = BoxContainer.ALIGNMENT_END
	caja_pa.mouse_filter = Control.MOUSE_FILTER_IGNORE
	grupo_sello.add_child(caja_pa)
	lbl_pa = _label(TXT, C_HUESO, true)
	lbl_pa.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	caja_pa.add_child(lbl_pa)
	fila_pa = HBoxContainer.new()
	fila_pa.add_theme_constant_override("separation", 2)
	fila_pa.alignment = BoxContainer.ALIGNMENT_CENTER
	fila_pa.mouse_filter = Control.MOUSE_FILTER_IGNORE
	caja_pa.add_child(fila_pa)
	var fin := Button.new()
	fin.name = "Btn_pasar"
	fin.focus_mode = Control.FOCUS_NONE
	fin.custom_minimum_size = Vector2(60, 60) + Vector2(SOMBRA, SOMBRA) * 2.0
	fin.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	fin.add_theme_stylebox_override("normal", _tex("sello", 0, 0))
	fin.add_theme_stylebox_override("hover", _tex("sello_hover", 0, 0))
	fin.add_theme_stylebox_override("pressed", _tex("sello", 0, 0))
	fin.add_theme_stylebox_override("disabled", _tex("sello_off", 0, 0))
	var txt_fin := _label(TXT_PEQ, C_HUESO, true)
	txt_fin.add_theme_font_override("font", FUENTE_TITULO)
	txt_fin.text = "Fin del\nturno"
	txt_fin.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	txt_fin.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	txt_fin.set_anchors_preset(Control.PRESET_FULL_RECT)
	fin.add_child(txt_fin)
	var chispas := CPUParticles2D.new()
	chispas.name = "Chispas"
	chispas.position = Vector2(30, 30) + Vector2(SOMBRA, SOMBRA)
	chispas.amount = 4
	chispas.lifetime = 2.2
	chispas.emission_shape = CPUParticles2D.EMISSION_SHAPE_SPHERE
	chispas.emission_sphere_radius = 22.0
	chispas.direction = Vector2(0, -1)
	chispas.spread = 25.0
	chispas.gravity = Vector2(0, -12)
	chispas.initial_velocity_min = 6.0
	chispas.initial_velocity_max = 14.0
	chispas.scale_amount_min = 1.0
	chispas.scale_amount_max = 1.5
	var rampa := Gradient.new()
	rampa.set_color(0, Color(C_MAGIA, 0.0))
	rampa.add_point(0.3, Color(C_MAGIA.lightened(0.3), 0.45))
	rampa.set_color(rampa.get_point_count() - 1, Color(C_MAGIA, 0.0))
	chispas.color_ramp = rampa
	chispas.show_behind_parent = true
	fin.add_child(chispas)
	fin.pressed.connect(_accion.bind("pasar"))
	_hover_ficha(fin)
	Jugo.boton(fin)
	Jugo.torcer(fin, 3.0)
	caja_pa.add_child(fin)
	botones["pasar"] = fin
	var peq := VBoxContainer.new()
	peq.add_theme_constant_override("separation", 4)
	peq.alignment = BoxContainer.ALIGNMENT_END
	peq.mouse_filter = Control.MOUSE_FILTER_IGNORE
	grupo_sello.add_child(peq)
	btn_lejanos = Button.new()
	btn_lejanos.focus_mode = Control.FOCUS_NONE
	btn_lejanos.add_theme_font_override("font", FUENTE_FUERTE)
	btn_lejanos.add_theme_font_size_override("font_size", TXT_PEQ)
	btn_lejanos.add_theme_color_override("font_color", C_HUESO)
	btn_lejanos.add_theme_color_override("font_hover_color", C_DORADO)
	btn_lejanos.add_theme_color_override("font_pressed_color", C_DORADO)
	btn_lejanos.add_theme_color_override("font_disabled_color", C_SUAVE)
	btn_lejanos.add_theme_stylebox_override("normal", _caja(C_CUERO, C_DORADO_OSCURO, 2, 6, 5, 3))
	btn_lejanos.add_theme_stylebox_override("hover", _caja(C_CUERO, C_DORADO, 2, 6, 5, 3))
	btn_lejanos.add_theme_stylebox_override("pressed", _caja(C_CUERO, C_DORADO, 2, 6, 5, 3))
	btn_lejanos.add_theme_stylebox_override("disabled", _caja(C_CUERO, C_HIERRO, 2, 6, 5, 3))
	btn_lejanos.set_meta("texto", _ficha_texto("Pasar lejanos", "L", "Acaba el turno de los héroes que están lejos de los enemigos despiertos.", C_HUESO))
	btn_lejanos.pressed.connect(func(): Modo.pasar_lejanos())
	btn_lejanos.visible = false
	_hover_ficha(btn_lejanos)
	Jugo.boton(btn_lejanos)
	peq.add_child(btn_lejanos)
	botones["lejanos"] = btn_lejanos
	var moch := _boton_peq("mochila", load(HUD_DIR + "mochila.png"), _ficha_texto("Mochila", "I", "Equipo, objetos y oro del héroe.", C_HUESO))
	moch.pressed.connect(_abrir_mochila)
	peq.add_child(moch)
	var dia := _boton_peq("dia", load(HUD_DIR + "sol.png"), "")
	dia.pressed.connect(_accion.bind("dia"))
	peq.add_child(dia)
	_abanico(acciones, huecos_acc.values(), 34.0, 4.0, 1.5)

# Fans some cards out (like a hand): each one a bit more rotated and lower towards the sides. Only
# redoes the layout if the number of visible cards changes
func _abanico(cont: Control, cartas: Array, paso: float, giro: float, curva: float) -> void:
	var vis := cartas.filter(func(c): return c.visible)
	var n := vis.size()
	if int(cont.get_meta("_n", -1)) == n:
		return
	cont.set_meta("_n", n)
	var alto := 0.0
	var ancho := 0.0
	for i in n:
		var c: Control = vis[i]
		var k := float(i) - (n - 1) * 0.5
		var y := k * k * curva
		c.position = Vector2(i * paso, y)
		c.set_meta("_y_base", y)
		Jugo.torcer(c, k * giro)
		alto = maxf(alto, y + c.custom_minimum_size.y)
		ancho = maxf(ancho, i * paso + c.custom_minimum_size.x)
	cont.custom_minimum_size = Vector2(ancho, alto)
	cont.size = cont.custom_minimum_size

# Card tooltip text (on dark parchment): title in the colour of its type, brightened so it reads;
# the key in watered ink and the explanation below
func _ficha_texto(titulo: String, tecla: String, texto: String, color: Color) -> String:
	var c := color.lightened(0.5) if color.get_luminance() < 0.5 else color
	var t := "[font_size=13][b][color=#%s]%s[/color][/b][/font_size]" % [c.to_html(false), titulo]
	if tecla != "":
		t += "  [color=#a8916c][%s][/color]" % tecla
	if texto != "":
		t += "\n" + texto
	return t

# A card (action or ability) of size 'tam' (plus the shadow around it): icon in the centre, the key
# bottom left, the PA cost (red ink) top right and uses/amount bottom right. On hover it
# straightens, grows a bit, rises and its ink turns violet.
func _hueco(color: Color, tecla: String, tam: Vector2) -> Button:
	var s := float(SOMBRA)
	var b := Button.new()
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = tam + Vector2(s, s) * 2.0
	b.size = b.custom_minimum_size
	b.add_theme_stylebox_override("normal", _tex("carta", 6, 0))
	b.add_theme_stylebox_override("hover", _tex("carta_hover", 6, 0))
	b.add_theme_stylebox_override("pressed", _tex("carta_hover", 6, 0))
	b.add_theme_stylebox_override("disabled", _tex("carta_off", 6, 0))
	var ic := tam.x - 14.0
	var r := _icono_rect(null, ic)
	r.name = "Icono"
	r.position = Vector2(s + (tam.x - ic) * 0.5, s + (tam.y - ic) * 0.5 - 2.0)
	b.add_child(r)
	var raya := ColorRect.new()
	raya.color = Color(color, 0.85)
	raya.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	raya.offset_left = s + 9
	raya.offset_right = -s - 9
	raya.offset_top = -s - 7
	raya.offset_bottom = -s - 6
	raya.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(raya)
	var t := _label(TXT_MINI, C_TINTA_SUAVE, true)
	t.text = tecla
	t.position = Vector2(s + 5, s + tam.y - 15)
	b.add_child(t)
	var coste := _label(TXT_PEQ, C_ROJO, true)
	coste.name = "Coste"
	coste.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	coste.offset_left = -s - 16
	coste.offset_right = -s - 5
	coste.offset_top = s + 1
	coste.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	b.add_child(coste)
	var n := _label(TXT_PEQ, C_TINTA, true)
	n.name = "Cantidad"
	n.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
	n.offset_left = -s - 20
	n.offset_top = -s - 17
	n.offset_right = -s - 5
	n.offset_bottom = -s - 3
	n.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	b.add_child(n)
	_hover_ficha(b)
	Jugo.boton(b)
	Jugo.elevar(b, 8.0)
	return b

# A belt pocket (single-use items): round, leather, with a golden trim
func _bolsillo(tecla: String) -> Button:
	var b := Button.new()
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = Vector2(30, 30)
	b.add_theme_stylebox_override("normal", _caja(Color(0.07, 0.045, 0.03), C_DORADO_OSCURO, 2, 15, 0))
	b.add_theme_stylebox_override("hover", _caja(Color(0.11, 0.07, 0.05), C_DORADO, 2, 15, 0))
	b.add_theme_stylebox_override("pressed", _caja(Color(0.11, 0.07, 0.05), C_DORADO.lightened(0.3), 2, 15, 0))
	b.add_theme_stylebox_override("disabled", _caja(Color(0.07, 0.045, 0.03), C_HIERRO, 2, 15, 0))
	var r := _icono_rect(null, 20)
	r.name = "Icono"
	r.position = Vector2(5, 5)
	b.add_child(r)
	var t := _label(TXT_MINI, C_SUAVE, true)
	t.text = tecla
	t.position = Vector2(-2, -5)
	b.add_child(t)
	var n := _label(TXT_PEQ, C_HUESO, true)
	n.name = "Cantidad"
	n.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
	n.offset_left = -18
	n.offset_top = -12
	n.offset_right = 2
	n.offset_bottom = 2
	n.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	b.add_child(n)
	_hover_ficha(b)
	Jugo.boton(b)
	return b

# Round leather button (backpack, day)
func _boton_peq(id: String, tex: Texture2D, texto: String) -> Button:
	var b := Button.new()
	b.name = "Btn_" + id
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = Vector2(28, 28)
	b.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	b.add_theme_stylebox_override("normal", _caja(C_CUERO, C_DORADO_OSCURO, 2, 14, 0, 3))
	b.add_theme_stylebox_override("hover", _caja(C_CUERO, C_DORADO, 2, 14, 0, 3))
	b.add_theme_stylebox_override("pressed", _caja(C_CUERO, C_DORADO.lightened(0.3), 2, 14, 0, 3))
	b.add_theme_stylebox_override("disabled", _caja(C_CUERO, C_HIERRO, 2, 14, 0, 3))
	var r := _icono_rect(tex, 18)
	r.name = "Icono"
	r.position = Vector2(5, 5)
	b.add_child(r)
	b.set_meta("texto", texto)
	_hover_ficha(b)
	Jugo.boton(b)
	botones[id] = b
	return b

# Remembers which slot is under the mouse, for the delayed tooltip card
func _hover_ficha(b: Control) -> void:
	b.mouse_entered.connect(func():
		objeto_hover = b
		t_hover = 0.0)
	b.mouse_exited.connect(func():
		if objeto_hover == b:
			objeto_hover = null)

# --- bottom left: log ---
func _montar_registro() -> void:
	registro = VBoxContainer.new()
	registro.add_theme_constant_override("separation", 0)
	registro.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(registro)

## Writes a line in the log (called by Indicadores, DadoCombate, Estados...)
func registrar(texto: String, color: Color = C_PERGAMINO) -> void:
	if registro == null:
		return
	var l := _label(TXT, color)
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size = Vector2(200, 0)
	l.add_theme_font_override("font", FUENTE_CURSIVA)
	l.add_theme_color_override("font_outline_color", C_TINTA)
	l.add_theme_constant_override("outline_size", 5)
	l.text = texto
	l.set_meta("t", 0.0)
	registro.add_child(l)
	Jugo.aparecer(l)
	while registro.get_child_count() > LINEAS_REGISTRO:
		var vieja := registro.get_child(0)
		registro.remove_child(vieja)
		vieja.queue_free()

# The tooltip card (dark parchment) shared by every slot
func _montar_ficha() -> void:
	ficha_objeto = PanelContainer.new()
	ficha_objeto.add_theme_stylebox_override("panel", _tex("pergamino_oscuro", 16, 16))
	ficha_objeto.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ficha_objeto.visible = false
	add_child(ficha_objeto)
	texto_objeto = RichTextLabel.new()
	texto_objeto.bbcode_enabled = true
	texto_objeto.fit_content = true
	texto_objeto.scroll_active = false
	texto_objeto.custom_minimum_size = Vector2(200, 0)
	texto_objeto.add_theme_font_override("normal_font", FUENTE)
	texto_objeto.add_theme_font_override("bold_font", FUENTE_FUERTE)
	texto_objeto.add_theme_font_size_override("normal_font_size", TXT)
	texto_objeto.add_theme_font_size_override("bold_font_size", TXT)
	texto_objeto.add_theme_color_override("default_color", C_PERGAMINO)
	texto_objeto.add_theme_font_override("italics_font", FUENTE_CURSIVA)
	texto_objeto.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ficha_objeto.add_child(texto_objeto)

# ==========================================
# ACTIONS
# ==========================================
func _usar_hueco(i: int) -> void:
	var b: Button = huecos_usables[i]
	var act = pm.call("GetActiveCharacter") if pm else null
	if act == null or b.disabled or not b.has_meta("indice"):
		Jugo.sacudir(b)
		return
	var error: String = pm.call("UsarObjeto", str(act.name), int(b.get_meta("indice")))
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.play_ui("no" if error != "" else "clic")
	if error != "":
		Jugo.sacudir(b)
	elif ui:
		Jugo.latido(b)
		ui.call("UpdateWarriorUI")
	_pedir_refresco()

# Opens the backpack if nothing is blocking it
func _abrir_mochila() -> void:
	for inv in get_tree().get_nodes_in_group("pantalla_inventario"):
		if inv.puede_abrir():
			inv.alternar()
		return

# A basic action from the bar (attack, help, retreat, hide, torch, end turn...)
func _accion(id: String) -> void:
	var combate := dm.get_node_or_null("CombatManager") if dm else null
	if combate == null or not botones.has(id):
		return
	var b: Button = botones[id]
	if not b.visible or b.disabled:
		if b.visible:
			Jugo.sacudir(b)
		return
	var audio := get_node_or_null("/root/AudioManager")
	if audio:
		audio.play_ui("clic")
	Jugo.latido(b, 1.06)
	match id:
		"atacar":
			combate.call("OnBotonAtacar")
		"ayudar", "retirada":
			Progreso.lanzar_accion(id)
		"pasar":
			combate.call("OnBotonPasar")
		"ocultarse":
			Sigilo.ocultarse(pm.call("GetActiveCharacter"))
		"antorcha":
			Sigilo.antorcha(pm.call("GetActiveCharacter"))
		"dia":
			if not Modo.con_turnos():
				Modo.descansar() # Exploring: rest, the day goes by without walking
			else:
				# Everyone runs out of PA: the day ends (the enemies play)
				for h in _heroes():
					h.set_meta("pa_actual", 0)
				pm.call("TerminarTurnoPersonaje")
	_pedir_refresco()

# ==========================================
# EVERY FRAME
# ==========================================
func _heroes() -> Array:
	var out := []
	if pm:
		for h in pm.get_children():
			if h.has_meta("grid_pos") and h.has_meta("hp_actual"):
				out.append(h)
	return out

const ALFA_EN_COMBATE := 0.15 # What is visible of the HUD while a hit or an ability lasts

# Forces the expensive refresh on the next frame
func _pedir_refresco() -> void:
	firma_vista = ""

# The cheap stuff that changes what's visible. If it doesn't change, the expensive stuff is only
# recomputed every REFRESCO s
func _firma(act) -> String:
	if act == null:
		return "-"
	var partes := [str(act.name), act.get_meta("pa_actual", 0), act.get_meta("hp_actual", 0), act.get_meta("xp", 0),
		Modo.en_combate, Sigilo.en_sigilo, _bloqueado(), dm.get_meta("debug_mode", false), pm.get_meta("turno_actual", 1), pm.get_meta("piso_actual", 1)]
	for h in _heroes():
		partes.append(h.get_meta("hp_actual", 0))
		partes.append(h.get_meta("pa_actual", 0))
	return str(partes)

func _process(delta: float) -> void:
	if pm == null or dm == null:
		pm = get_node_or_null("/root/Main/PartyManager")
		dm = get_node_or_null(RUTA_DM)
		return
	_atenuar(delta)
	var act = pm.call("GetActiveCharacter")
	t_refresco -= delta
	var firma := _firma(act)
	if firma != firma_vista or t_refresco <= 0.0:
		firma_vista = firma
		t_refresco = REFRESCO
		_refrescar(act)
	_colocar()
	_envejecer_registro(delta)
	_colocar_ficha_objeto(act, delta)
	_teclas()

# Expensive refresh: floor, day, mode, party column, turn order, abilities and items
func _refrescar(act) -> void:
	_set_texto(lbl_piso, "Piso %d" % int(pm.get_meta("piso_actual", 1)))
	_set_texto(lbl_dia, "Día %d" % int(pm.get_meta("turno_actual", 1)))
	var combate: bool = Modo.en_combate
	var turnos: bool = Modo.con_turnos()
	var modo := "¡Combate!" if combate else ("Sigilo" if turnos else "Explorando")
	_set_texto(lbl_modo, modo)
	if modo != modo_visto:
		modo_visto = modo
		lbl_modo.add_theme_color_override("font_color", C_ROJO_CLARO if combate else (Sigilo.COLOR_SIGILO if turnos else C_VERDE_CLARO))
		Jugo.latido(pastilla_modo, 1.12)
		if turnos:
			Jugo.aparecer(caja_pa)
	_actualizar_columna(act)
	_actualizar_turno(act)
	_actualizar_activo(act)
	_actualizar_habilidades(act)
	_abanico(mano, huecos_hab, 38.0, 5.0, 2.5)
	_actualizar_objetos(act)
	_actualizar_botones(act)

# Places the bottom pieces around the hand (which goes in the centre), each one at its height. With
# an event open (its panel takes the right side) everything at the bottom and the turn order slide
# left, into the free space, and the minimap goes off the top. When it closes, they come back.
var _dx := 0.0           # Current (smoothed) offset of the bottom pieces
var _mini_fuera := 0.0   # 0 = minimap in place, 1 = off the top

func _colocar() -> void:
	var pantalla := get_viewport_rect().size
	var dt := get_process_delta_time()
	var suave := 1.0 - exp(-dt * 9.0)
	var cx := pantalla.x * 0.5
	var suelo := pantalla.y - 8.0
	mano.position = Vector2(roundf(cx - mano.size.x * 0.5), suelo - mano.size.y)
	acciones.position = Vector2(mano.position.x - 16.0 - acciones.size.x, suelo - acciones.size.y - 4.0)
	placa_heroe.size = placa_heroe.get_combined_minimum_size()
	placa_heroe.position = Vector2(acciones.position.x - 18.0 - placa_heroe.size.x, suelo - placa_heroe.size.y + 4.0)
	cinturon.size = cinturon.get_combined_minimum_size()
	cinturon.position = Vector2(mano.position.x + mano.size.x + 16.0, suelo - cinturon.size.y + 2.0)
	grupo_sello.size = grupo_sello.get_combined_minimum_size()
	grupo_sello.position = Vector2(cinturon.position.x + cinturon.size.x + 14.0, suelo - grupo_sello.size.y + 2.0)
	# Is there an event open? The free space is whatever is left of its panel
	var pe := get_node_or_null("/root/Main/CanvasLayer2/PantallaEvento") as Control
	var evento := pe != null and pe.visible and pe.position.y < pantalla.y * 0.5
	var objetivo := 0.0
	if evento:
		var libre := pe.global_position.x
		objetivo = libre * 0.5 - cx                                                   # Centred in the free space...
		objetivo = minf(objetivo, libre - 10.0 - (cinturon.position.x + cinturon.size.x))   # ...without going under it
		objetivo = maxf(objetivo, MARGEN - placa_heroe.position.x)                       # ...nor off the left edge
	_dx = lerpf(_dx, objetivo, suave) if absf(_dx - objetivo) > 0.5 else objetivo
	for pieza in [mano, acciones, placa_heroe, cinturon, grupo_sello]:
		pieza.position.x += roundf(_dx)
	# During an event you can't end the turn: the seal (and backpack/day) fade out
	grupo_sello.modulate.a = 1.0 - _mini_fuera
	grupo_sello.visible = grupo_sello.modulate.a > 0.02
	barra_turno.size = barra_turno.get_combined_minimum_size()
	var centro_turno: float = (pe.global_position.x * 0.5) if evento else cx
	var x_turno := roundf(centro_turno - barra_turno.size.x * 0.5)
	barra_turno.position = Vector2(x_turno if absf(barra_turno.position.x - x_turno) < 1.0 else lerpf(barra_turno.position.x, x_turno, suave), 6)
	# Minimap: goes up and off the screen
	var mini: Control = (ui.get_node_or_null("Minimapa") as Control) if ui else null
	if mini:
		if not mini.has_meta("y_base"):
			mini.set_meta("y_base", mini.position.y)
		_mini_fuera = lerpf(_mini_fuera, 1.0 if evento else 0.0, suave)
		mini.position.y = float(mini.get_meta("y_base")) - _mini_fuera * (mini.size.y + float(mini.get_meta("y_base")) + 20.0)
	# The log goes bottom left; if the hero plate reaches there, it moves up above it
	var alto_reg := registro.get_combined_minimum_size().y
	var y_reg := pantalla.y - 14.0 - alto_reg - 8
	if placa_heroe.position.x < MARGEN + 215.0:
		y_reg = minf(y_reg, placa_heroe.position.y - 10.0 - alto_reg)
	registro.position = Vector2(MARGEN, y_reg)

# While the camera frames an attack or an ability, the HUD (and the minimap) almost disappear
func _atenuar(delta: float) -> void:
	var cam := dm.get_node_or_null("Camera3D")
	var en_golpe: bool = cam != null and cam.has_meta("encuadre")
	var objetivo := ALFA_EN_COMBATE if en_golpe else 1.0
	if is_equal_approx(modulate.a, objetivo):
		return
	modulate.a = move_toward(modulate.a, objetivo, delta * (6.0 if en_golpe else 3.0))
	for n in [ui.get_node_or_null("Minimapa"), ui.get_node_or_null("BtnSalidaDebug"), dm.get_node_or_null("DungeonRenderer/Capa2D/Raiz")]:
		if n is CanvasItem:
			n.modulate.a = modulate.a

# Reading metas from the DungeonManager (C++ script) costs ~0.13 ms each: once per frame
var _bloq_frame := -1
var _bloq := false

# Can the player act right now? Cached once per frame (it reads C++ metas)
func _bloqueado() -> bool:
	var frame := Engine.get_process_frames()
	if frame != _bloq_frame:
		_bloq_frame = frame
		_bloq = dm.get_meta("is_moving", false) or dm.get_meta("turno_enemigo", false) or dm.get_meta("bloqueo_evento", false) or dm.get_meta("bloqueo_banner", false)
	return _bloq

# Health bar of a portrait, with ghost damage
func _fijar_vida(s: Control, hp: int, hp_max: int) -> void:
	var fondo: ColorRect = s.get_node("VidaFondo")
	Jugo.barra(fondo, fondo.get_node("Vida"), float(hp) / float(maxi(1, hp_max)))

# The party column: the leader and their line first; then the ones apart, separated by a line
func _actualizar_columna(act) -> void:
	var nombres := {}
	var turnos: bool = Modo.con_turnos()
	for h in _heroes():
		var n := str(h.name)
		nombres[n] = true
		var s: Button = slots.get(n)
		if s == null:
			s = _ficha_grupo(n)
			columna.add_child(s)
			slots[n] = s
			Jugo.aparecer(s)
		var muerto: bool = h.get_meta("esta_muerto", false) or h.get_meta("ha_escapado", false)
		var pa := int(h.get_meta("pa_actual", 0))
		var marco: Panel = s.get_node("Marco")
		var r: TextureRect = marco.get_node("Retrato")
		var tex: Texture2D = Apariencia.textura(h)
		if r.texture != tex:
			r.texture = tex
		_set_texto(s.get_node("Nombre"), str(h.get_meta("nombre_display", n)))
		var hp := int(h.get_meta("hp_actual", 0))
		var hp_max := int(h.get_meta("hp_max", 1))
		_set_texto(s.get_node("VidaNum"), "%d/%d" % [hp, hp_max])
		s.modulate = Color(0.35, 0.35, 0.4) if muerto else (Color(0.6, 0.6, 0.66) if pa <= 0 and turnos else Color.WHITE)
		var lbl_pa_slot: Label = s.get_node("PA")
		lbl_pa_slot.visible = not muerto and turnos
		_set_texto(lbl_pa_slot, str(pa) if pa > 0 else "hecho")
		var es_act: bool = h == act
		if bool(s.get_meta("activo", false)) != es_act:
			s.set_meta("activo", es_act)
			# The active one: violet medallion, the right strip and its runes lit
			_set_estilo(marco, "panel", _caja(C_HUECO, C_MAGIA if es_act else C_DORADO_OSCURO, 2, int(TAM_GRUPO / 2), 0))
			Jugo.escalar(marco, 1.08 if es_act else 1.0, 0.2)
			Jugo.girar(s, 0.0 if es_act else float(s.get_meta("_jugo_rot", 0.0)), 0.25)
		# They got hit: shake
		var hp_prev := int(s.get_meta("hp_prev", hp))
		if hp < hp_prev:
			Jugo.sacudir(marco, 0.08)
			Jugo.destello(marco, Color(1.5, 0.8, 0.8))
		elif hp > hp_prev:
			Jugo.destello(marco, Color(0.85, 1.35, 0.9))
		s.set_meta("hp_prev", hp)
		_fijar_vida(s, hp, hp_max)
		s.set_meta("personaje", h)
	for n in slots.keys():
		if not nombres.has(n):
			slots[n].queue_free()
			slots.erase(n)
	# Order by groups (the active one's first) and lines between groups
	var grupos: Array = Modo.grupos_heroes()
	var idx := 0
	var raya_i := 0
	for gi in grupos.size():
		if gi > 0:
			if raya_i >= rayas_grupo.size():
				var r := ColorRect.new()
				r.color = C_HIERRO_CLARO
				r.custom_minimum_size = Vector2(TAM_GRUPO + 90, 2)
				r.mouse_filter = Control.MOUSE_FILTER_IGNORE
				columna.add_child(r)
				rayas_grupo.append(r)
			var raya: ColorRect = rayas_grupo[raya_i]
			raya.visible = true
			if raya.get_index() != idx:
				columna.move_child(raya, idx)
			idx += 1
			raya_i += 1
		for h in grupos[gi]:
			var s: Button = slots.get(str(h.name))
			if s == null:
				continue
			if s.get_index() != idx:
				columna.move_child(s, idx)
			idx += 1
			var nombre := str(h.get_meta("nombre_display", h.name))
			var tip := nombre if gi == 0 else "%s · apartado del grupo" % nombre
			if s.tooltip_text != tip:
				s.tooltip_text = tip
			if gi > 0 and s.modulate == Color.WHITE:
				s.modulate = Color(0.78, 0.78, 0.82)
	for j in range(raya_i, rayas_grupo.size()):
		rayas_grupo[j].visible = false

# Top: in combat, who has a turn (heroes with PA) and against whom; exploring, what's in view
func _actualizar_turno(act) -> void:
	var combate: bool = Modo.con_turnos()
	var cam: Camera3D = dm.get_node_or_null("Camera3D")
	var lista := []
	for e in Tactica.unidades():
		if Tactica.es_heroe(e) or not (e is Node3D) or not e.visible:
			continue
		var en_pantalla: bool = cam != null and cam.is_position_in_frustum(e.global_position + Vector3(0, 0.5, 0))
		if (combate and e.get_meta("in_combat", false)) or en_pantalla:
			lista.append(e)
	lista.sort_custom(func(a, b): return int(a.get_meta("in_combat", false)) > int(b.get_meta("in_combat", false)))
	for i in MAX_ENEMIGOS:
		var s: Button = slots_enemigo[i]
		var ver := i < lista.size()
		if ver and not s.visible:
			s.visible = true
			Jugo.aparecer(s, i * 0.04)
		elif not ver:
			s.visible = false
			continue
		var e: Node3D = lista[i]
		s.set_meta("personaje", e)
		var tip := str(e.get_meta("nombre_display", "Enemigo"))
		if s.tooltip_text != tip:
			s.tooltip_text = tip
		var r: TextureRect = s.get_node("Retrato")
		var frame := int(e.get_meta("frame", 11))
		if int(s.get_meta("frame", -1)) != frame:
			s.set_meta("frame", frame)
			var at := AtlasTexture.new()
			at.atlas = load("res://Assets/2D Pixel Dungeon Asset Pack/character and tileset/Dungeon_Character_2.png")
			at.region = Rect2((frame % 7) * 16, (frame / 7) * 16, 16, 16)
			r.texture = at
		_fijar_vida(s, int(e.get_meta("hp_actual", 0)), int(e.get_meta("hp_max", 1)))
	# Heroes who still have a turn (the active one first and bigger)
	var con_turno := []
	if combate:
		if act and int(act.get_meta("pa_actual", 0)) > 0:
			con_turno.append(act)
		for h in _heroes():
			if h != act and int(h.get_meta("pa_actual", 0)) > 0 and not h.get_meta("esta_muerto", false) and not h.get_meta("ha_escapado", false):
				con_turno.append(h)
	var vistos := {}
	for h in con_turno:
		var n := str(h.name)
		vistos[n] = true
		var m: TextureRect = minis_turno.get(n)
		if m == null:
			m = _icono_rect(null, TAM_TURNO - 6)
			minis_turno[n] = m
			fila_turno_heroes.add_child(m)
			Jugo.aparecer(m)
		var tex: Texture2D = Apariencia.textura(h)
		if m.texture != tex:
			m.texture = tex
		m.modulate = Color.WHITE if h == act else Color(0.7, 0.62, 0.55)
		var escala := 1.15 if h == act else 0.9
		if not is_equal_approx(float(m.get_meta("escala", 0.0)), escala):
			m.set_meta("escala", escala)
			Jugo.escalar(m, escala, 0.2)
	for n in minis_turno.keys():
		if not vistos.has(n):
			minis_turno[n].queue_free()
			minis_turno.erase(n)
	var i := 0
	for h in con_turno:
		var m: Control = minis_turno[str(h.name)]
		if m.get_index() != i:
			fila_turno_heroes.move_child(m, i)
		i += 1
	fila_turno_heroes.visible = not con_turno.is_empty()
	separador.visible = not con_turno.is_empty() and not lista.is_empty()
	_set_texto(lbl_turno, ("Sigilo" if Sigilo.en_sigilo else "Turno") if combate else "A la vista")
	var ver_barra: bool = combate or not lista.is_empty()
	if ver_barra and not barra_turno.visible:
		Jugo.aparecer(barra_turno)
	barra_turno.visible = ver_barra

# Hero plate: portrait, gauges, level and XP of the active hero
func _actualizar_activo(act) -> void:
	barra.visible = act != null
	if act == null:
		return
	var n := str(act.name)
	var f: int = Apariencia.fotograma(act)
	if n != heroe_pintado or f != fotograma_pintado:
		if n != heroe_pintado:
			Jugo.latido(marco_retrato, 1.1)
		heroe_pintado = n
		fotograma_pintado = f
		retrato.texture = Apariencia.textura(act, f)
	medidores["hp_actual"].fijar(int(act.get_meta("hp_actual", 0)), int(act.get_meta("hp_max", 1)))
	medidores["hambre_actual"].fijar(int(act.get_meta("hambre_actual", 0)), int(act.get_meta("hambre_max", 10)))
	medidores["sanidad_actual"].fijar(int(act.get_meta("sanidad_actual", 0)), int(act.get_meta("sanidad_max", 100)))
	var pa := int(act.get_meta("pa_actual", 0))
	var pa_max := maxi(pa, int(act.get_meta("pa_max", act.get_meta("velocidad", pa))))
	var combate: bool = Modo.con_turnos()
	_set_texto(lbl_pa, "PA %d/%d" % [pa, pa_max] if combate else "Explorando")
	caja_pa.visible = combate
	while fila_pa.get_child_count() < pa_max:
		var p := ColorRect.new()
		p.custom_minimum_size = Vector2(6, 6)
		p.pivot_offset = Vector2(3, 3)
		p.rotation = PI * 0.25 # Sealing wax diamonds
		p.mouse_filter = Control.MOUSE_FILTER_IGNORE
		fila_pa.add_child(p)
	for i in fila_pa.get_child_count():
		var p: ColorRect = fila_pa.get_child(i)
		p.visible = i < pa_max
		var color := C_ROJO if i < pa else C_HIERRO
		if p.color != color:
			# A PA being spent (or coming back) does a little hop
			if p.is_visible_in_tree() and pa_visto >= 0:
				Jugo.latido(p, 1.3)
			p.color = color
	pa_visto = pa

# Their abilities: icon, cost, uses (actives) or stacks (passives), and the cooldown veil
func _actualizar_habilidades(act) -> void:
	if act == null:
		return
	var l: Array = Progreso.lista(act)
	var ranuras: int = Progreso.max_ranuras(act)
	var nivel := int(act.get_meta("nivel", 1))
	_set_texto(lbl_nivel, "NV %d" % nivel)
	var xp_max: int = Progreso.xp_para(nivel)
	barra_xp.size = Vector2(fondo_xp.size.x * clampf(float(act.get_meta("xp", 0)) / float(maxi(1, xp_max)), 0.0, 1.0), fondo_xp.size.y)
	var tip := "XP %d/%d" % [int(act.get_meta("xp", 0)), xp_max]
	if fondo_xp.tooltip_text != tip:
		fondo_xp.tooltip_text = tip
	for i in MAX_HAB:
		var b: Button = huecos_hab[i]
		b.visible = i < ranuras
		var ic: TextureRect = b.get_node("Icono")
		var velo: ColorRect = b.get_node("Enfriamiento")
		var n: Label = b.get_node("Cantidad")
		var coste: Label = b.get_node("Coste")
		if i >= l.size():
			ic.texture = null
			velo.visible = false
			_set_texto(n, "")
			_set_texto(coste, "")
			b.modulate = Color(1, 1, 1, 0.35)
			continue
		b.modulate = Color.WHITE
		var e: Dictionary = l[i]
		var d: HabilidadData = Progreso.datos(e["id"])
		var tex: Texture2D = d.icono if d else null
		if ic.texture != tex:
			ic.texture = tex
			Jugo.aparecer(b) # New ability on the bar
		if d and d.es_activa():
			_set_texto(n, str(e["usos"]))
			_set_texto(coste, str(d.coste_pa) if d.coste_pa > 0 else "")
			var cd := int(e["enfriamiento"])
			velo.visible = cd > 0
			_set_texto(velo.get_node("Dias"), str(cd))
			ic.modulate = Color.WHITE if Progreso.motivo_no_usar(act, i) == "" else Color(1, 1, 1, 0.35)
		else:
			_set_texto(n, ("x%d" % int(e["pilas"])) if int(e["pilas"]) > 1 else "")
			_set_texto(coste, "")
			velo.visible = false
			ic.modulate = Color(0.85, 0.85, 1.0)

# The consumables (single-use, stacked)
func _actualizar_objetos(act) -> void:
	if act == null:
		return
	var usables := []
	for e in pm.call("DescribirInventario", str(act.name)):
		if e["usable"] and not e["equipable"]:
			usables.append(e)
	var bloqueado: bool = _bloqueado()
	for i in MAX_USABLES:
		var b: Button = huecos_usables[i]
		var ic: TextureRect = b.get_node("Icono")
		if i < usables.size():
			var e: Dictionary = usables[i]
			if ic.texture != e["icono"]:
				ic.texture = e["icono"]
			ic.modulate = Color.WHITE
			b.modulate = Color.WHITE
			b.set_meta("indice", int(e["indice"]))
			b.set_meta("objeto", Catalogo.obtener(str(e["id"])))
			_set_texto(b.get_node("Cantidad"), str(e["cantidad"]) if int(e["cantidad"]) > 1 else "")
			b.disabled = bloqueado
		else:
			ic.texture = null
			b.modulate = Color(1, 1, 1, 0.45) # Empty slot: barely visible
			_set_texto(b.get_node("Cantidad"), "")
			for k in ["indice", "objeto"]:
				if b.has_meta(k):
					b.remove_meta(k)
			b.disabled = true

# Enables/disables every action card depending on PA, reach and mode
func _actualizar_botones(act) -> void:
	var bloqueado: bool = _bloqueado() or act == null
	var debug: bool = dm.get_meta("debug_mode", false)
	var pa: int = int(act.get_meta("pa_actual", 0)) if act else 0
	var combate: bool = Modo.con_turnos()
	# Attack: someone in reach and PA for the weapon
	var puede_atacar := false
	var coste_ataque := 2
	var motivo_ataque := "Nadie al alcance"
	if not bloqueado:
		var objetivo: Vector2i = dm.call("BuscarEnemigoAdyacente")
		if objetivo.x >= 0:
			var cm := dm.get_node_or_null("CombatManager")
			var d: Dictionary = cm.call("PrevisionAtaque", objetivo.x, objetivo.y) if cm else {}
			coste_ataque = int(d.get("coste", 2))
			puede_atacar = debug or pa >= coste_ataque or not combate
			motivo_ataque = "" if puede_atacar else "Faltan PA"
	var b_at: Button = botones["atacar"]
	b_at.disabled = not puede_atacar
	_set_texto(b_at.get_node("Coste"), str(coste_ataque) if combate else "")
	b_at.get_node("Icono").modulate.a = 1.0 if puede_atacar else 0.35
	b_at.set_meta("texto", _ficha_texto("Atacar", "1", ACCIONES[0][4] + _motivo_txt(motivo_ataque), C_ORO))
	for k in [1, 2]:
		var id: String = ACCIONES[k][0]
		var motivo: String = Progreso.motivo_accion(id)
		var b: Button = botones[id]
		b.disabled = bloqueado or motivo != ""
		_set_texto(b.get_node("Coste"), "1" if combate else "")
		b.get_node("Icono").modulate.a = 1.0 if not b.disabled else 0.35
		b.set_meta("texto", _ficha_texto(ACCIONES[k][2], ACCIONES[k][3], ACCIONES[k][4] + _motivo_txt(motivo), C_ORO))
	# Stealth: hide (1 PA; if already hidden, stand up) and the torch (free)
	var oculto: bool = act != null and Estados.tiene(act, "oculto")
	var b_oc: Button = botones["ocultarse"]
	var motivo_oc := "" if oculto else ("Solo con turnos (sigilo o combate)" if not combate else ("Faltan PA" if pa < Sigilo.COSTE_OCULTARSE and not debug else ""))
	b_oc.disabled = bloqueado or motivo_oc != ""
	_set_texto(b_oc.get_node("Coste"), "" if oculto or not combate else str(Sigilo.COSTE_OCULTARSE))
	b_oc.get_node("Icono").modulate = Color(0.75, 0.6, 1.0) if oculto else Color(1, 1, 1, 1.0 if not b_oc.disabled else 0.35)
	b_oc.set_meta("texto", _ficha_texto("Levantarse" if oculto else ACCIONES[3][2], ACCIONES[3][3], ("Estás oculto. Pulsa para levantarte (gratis)." if oculto else ACCIONES[3][4]) + _motivo_txt(motivo_oc), C_ORO))
	var apagada: bool = act != null and act.get_meta("antorcha_apagada", false)
	var b_an: Button = botones["antorcha"]
	b_an.disabled = bloqueado
	b_an.get_node("Icono").modulate = Color(0.45, 0.42, 0.4) if apagada else Color.WHITE
	b_an.set_meta("texto", _ficha_texto("Encender la antorcha" if apagada else "Apagar la antorcha", ACCIONES[4][3], ACCIONES[4][4], C_ORO))
	var fin: Button = botones["pasar"]
	fin.disabled = bloqueado or not combate
	(fin.get_node("Chispas") as CPUParticles2D).emitting = not fin.disabled
	fin.set_meta("texto", _ficha_texto("Fin del turno", "3", "El héroe activo acaba su turno.", C_ORO))
	var dia: Button = botones["dia"]
	dia.disabled = bloqueado
	dia.set_meta("texto", _ficha_texto("Fin del día", "4", "Todos acaban su turno: juegan los enemigos.", C_HUESO) if combate else _ficha_texto("Descansar", "4", "Pasa el día sin moverse.", C_HUESO))
	botones["mochila"].disabled = dm.get_meta("turno_enemigo", false) or dm.get_meta("bloqueo_evento", false)
	var n_lejos: int = Modo.lejanos().size()
	var ver_lejos: bool = combate and n_lejos > 0
	if ver_lejos and not btn_lejanos.visible:
		Jugo.aparecer(btn_lejanos)
	btn_lejanos.visible = ver_lejos
	btn_lejanos.disabled = bloqueado
	btn_lejanos.text = "Lejanos (%d)" % n_lejos

# Red line explaining why an action can't be used ("" if it can)
func _motivo_txt(motivo: String) -> String:
	return "\n[color=#e0675a]%s[/color]" % motivo if motivo != "" else ""

# The card of the hovered slot: shows up after a moment, rising and fading in
func _colocar_ficha_objeto(act, delta: float) -> void:
	var b := objeto_hover
	if b == null or not is_instance_valid(b) or not b.is_visible_in_tree():
		ficha_objeto.visible = false
		return
	t_hover += delta
	if t_hover < RETRASO_FICHA:
		ficha_objeto.visible = false
		return
	var t := ""
	if b.has_meta("habilidad_i"):
		var i: int = b.get_meta("habilidad_i")
		t = Progreso.describir(act, i) if act else ""
		if act and i < Progreso.lista(act).size() and Progreso.datos(Progreso.lista(act)[i]["id"]).es_activa():
			t += "\n[color=#7d7d8c]Clic o tecla %s[/color]" % LETRAS_HAB[i]
	elif b.has_meta("objeto"):
		var o: ObjetoData = b.get_meta("objeto")
		t = Textos.detalle(o)
		if b.has_meta("indice"):
			t += "\n[color=#7d7d8c]Clic o tecla %s[/color]" % str(5 + huecos_usables.find(b))
	elif b.has_meta("texto"):
		t = str(b.get_meta("texto"))
	if t == "":
		ficha_objeto.visible = false
		return
	if texto_objeto.text != t:
		texto_objeto.text = t
	ficha_objeto.size = ficha_objeto.get_combined_minimum_size()
	var r := b.get_global_rect()
	var pantalla := get_viewport_rect().size
	var subida := clampf((t_hover - RETRASO_FICHA) / 0.12, 0.0, 1.0)
	var y := r.position.y - ficha_objeto.size.y - 8 + (1.0 - subida) * 6.0
	ficha_objeto.position = Vector2(r.get_center().x - ficha_objeto.size.x * 0.5, y).clamp(Vector2(8, 8), pantalla - ficha_objeto.size - Vector2(8, 8))
	ficha_objeto.modulate.a = subida
	ficha_objeto.visible = true

# Log lines fade out after a while and then go away
func _envejecer_registro(delta: float) -> void:
	for l in registro.get_children():
		var t: float = float(l.get_meta("t", 0.0)) + delta
		l.set_meta("t", t)
		if t > 0.3:
			l.modulate.a = clampf((VIDA_REGISTRO - t) / 1.5, 0.0, 1.0)
		if t > VIDA_REGISTRO:
			l.queue_free()

# Shortcuts: 1 attack · 2 help · R retreat · H hide · T torch · 3 end of turn · 4 end of day/rest ·
# L far ones · Z X C V abilities · 5-9 items · F9 debug button
func _teclas() -> void:
	var atajos := {KEY_1: "atacar", KEY_2: "ayudar", KEY_R: "retirada", KEY_H: "ocultarse", KEY_T: "antorcha", KEY_3: "pasar", KEY_4: "dia", KEY_L: "lejanos"}
	for tecla in atajos:
		var ahora := Input.is_physical_key_pressed(tecla)
		if ahora and not teclas_previas.get(tecla, false) and not get_tree().paused:
			if atajos[tecla] == "lejanos":
				if btn_lejanos.visible and not btn_lejanos.disabled:
					Modo.pasar_lejanos()
			else:
				_accion(atajos[tecla])
		teclas_previas[tecla] = ahora
	for i in MAX_HAB:
		var tecla: int = TECLAS_HAB[i]
		var ahora := Input.is_physical_key_pressed(tecla)
		if ahora and not teclas_previas.get(tecla, false) and not get_tree().paused:
			Jugo.latido(huecos_hab[i], 1.06)
			Progreso.pulsar(i)
			_pedir_refresco()
		teclas_previas[tecla] = ahora
	for i in MAX_USABLES:
		var tecla: int = KEY_5 + i
		var ahora := Input.is_physical_key_pressed(tecla)
		if ahora and not teclas_previas.get(tecla, false) and not get_tree().paused:
			_usar_hueco(i)
		teclas_previas[tecla] = ahora
	var f9 := Input.is_physical_key_pressed(KEY_F9)
	if f9 and not teclas_previas.get(KEY_F9, false):
		var debug := ui.get_node_or_null("BtnSalidaDebug") as Control
		if debug:
			debug.visible = not debug.visible
			debug.position = Vector2(get_viewport_rect().size.x - debug.size.x - MARGEN, 180)
	teclas_previas[KEY_F9] = f9
