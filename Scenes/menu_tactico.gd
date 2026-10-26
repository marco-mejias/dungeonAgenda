extends Control

const Iconos := preload("res://Scripts/GDD/iconos_pixel.gd")

# Menú contextual anclado al héroe activo, con estética de ventana PS1.
# Vive en la capa de HUD (CanvasLayer) y se recoloca cada frame proyectando la ficha 3D.
# Atajos: 1 = Ataque, 2 = Habilidades, 3 = Pasar turno.

const ESCALA_BASE := 0.5
const OFFSET_PANTALLA := Vector2(26, -70) # a la derecha y por encima de la cabeza
const COSTE_ATAQUE := 2
const ORO := Color(1.0, 0.8, 0.3)

var combat_manager: Node = null
var dungeon: Node = null
var token_to_follow: Node3D = null
var cam: Camera3D = null
var contenedor: SubViewportContainer = null
var ventana: PanelContainer
var filas := {}
var teclas_previas := {}

func _ready() -> void:
	visible = false
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	dungeon = get_node_or_null("/root/Main/ContenedorMapa/SubViewport/DungeonManager")
	contenedor = get_node_or_null("/root/Main/ContenedorMapa")
	if dungeon:
		combat_manager = dungeon.get_node_or_null("CombatManager")
		cam = dungeon.get_node_or_null("Camera3D")
		var pm = get_node_or_null("/root/Main/PartyManager")
		if pm:
			var act_char = pm.call("GetActiveCharacter")
			if act_char:
				token_to_follow = dungeon.get_node_or_null(str(act_char.name))
	_construir()
	_actualizar_transformacion()
	visible = true
	# Despliegue con rebote
	ventana.pivot_offset = Vector2(0, ventana.size.y)
	ventana.scale = Vector2(0.6, 0.2)
	ventana.modulate.a = 0.0
	var tw := create_tween()
	tw.tween_property(ventana, "scale", Vector2.ONE, 0.16).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tw.parallel().tween_property(ventana, "modulate:a", 1.0, 0.1)

func _construir() -> void:
	ventana = PanelContainer.new()
	ventana.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var estilo := StyleBoxFlat.new()
	estilo.bg_color = Color(0.04, 0.05, 0.16, 0.95)
	estilo.set_border_width_all(2)
	estilo.border_color = Color(0.55, 0.6, 0.85)
	estilo.corner_detail = 1
	estilo.set_content_margin_all(4)
	estilo.shadow_color = Color(0, 0, 0, 0.55)
	estilo.shadow_size = 3
	ventana.add_theme_stylebox_override("panel", estilo)
	add_child(ventana)

	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 2)
	v.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ventana.add_child(v)

	var estado := _estado_ataque()
	_fila(v, "atacar", "espada", "ATAQUE", "1", estado[0], estado[1])
	_fila(v, "habilidad", "chispa", "HABILIDADES", "2", false, "Próximamente")
	_fila(v, "pasar", "reloj", "PASAR TURNO", "3", true, "Fin del turno")

# [disponible, texto de la derecha]
func _estado_ataque() -> Array:
	var pm = get_node_or_null("/root/Main/PartyManager")
	var act = pm.call("GetActiveCharacter") if pm else null
	var pa: int = act.get_meta("pa_actual", 0) if act else 0
	var debug: bool = dungeon.get_meta("debug_mode", false) if dungeon else false
	var objetivo: Vector2i = dungeon.call("BuscarEnemigoAdyacente") if dungeon else Vector2i(-1, -1)
	if objetivo.x < 0:
		return [false, "Nadie al lado"]
	if pa < COSTE_ATAQUE and not debug:
		return [false, "Faltan PA"]
	return [true, "%d PA" % (0 if debug else COSTE_ATAQUE)]

func _fila(padre: Control, id: String, icono: String, texto: String, tecla: String, disponible: bool, detalle: String) -> void:
	var b := Button.new()
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = Vector2(168, 26)
	b.disabled = not disponible
	for nombre_estilo in ["normal", "hover", "pressed", "disabled"]:
		var s := StyleBoxFlat.new()
		s.bg_color = Color(0.14, 0.13, 0.32) if nombre_estilo in ["hover", "pressed"] else Color(0, 0, 0, 0)
		s.set_border_width_all(1 if nombre_estilo == "hover" else 0)
		s.border_color = ORO
		s.corner_detail = 1
		b.add_theme_stylebox_override(nombre_estilo, s)
	padre.add_child(b)

	var h := HBoxContainer.new()
	h.set_anchors_preset(Control.PRESET_FULL_RECT)
	h.offset_left = 4
	h.offset_right = -4
	h.add_theme_constant_override("separation", 5)
	h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(h)

	var t := Label.new()
	t.text = tecla
	t.add_theme_font_size_override("font_size", 9)
	t.add_theme_color_override("font_color", Color(0.55, 0.6, 0.85))
	t.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	h.add_child(t)

	var ic := TextureRect.new()
	ic.texture = Iconos.textura(icono, 2)
	ic.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	ic.custom_minimum_size = Vector2(18, 18)
	ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	ic.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	ic.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	ic.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.add_child(ic)

	var l := Label.new()
	l.text = texto
	l.add_theme_font_size_override("font_size", 12)
	l.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	l.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	h.add_child(l)

	var d := Label.new()
	d.text = detalle
	d.add_theme_font_size_override("font_size", 9)
	d.add_theme_color_override("font_color", ORO if disponible else Color(0.6, 0.45, 0.45))
	d.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	h.add_child(d)

	if not disponible:
		b.modulate = Color(0.6, 0.6, 0.7)
	b.pressed.connect(_pulsar.bind(id))
	if id == "atacar":
		b.mouse_entered.connect(_previsualizar.bind(true))
		b.mouse_exited.connect(_previsualizar.bind(false))
	b.mouse_entered.connect(_sonido_hover.bind(b))
	filas[id] = b

func _sonido_hover(b: Button) -> void:
	if not b.disabled:
		AudioManager.play_ui("hover")

func _previsualizar(mostrar: bool) -> void:
	var hud := get_parent()
	if not hud or not dungeon:
		return
	if mostrar:
		hud.set_meta("prevision_objetivo", dungeon.call("BuscarEnemigoAdyacente"))
	elif hud.has_meta("prevision_objetivo"):
		hud.remove_meta("prevision_objetivo")

func _process(_delta):
	_actualizar_transformacion()
	# Atajos de teclado
	for tecla in [[KEY_1, "atacar"], [KEY_2, "habilidad"], [KEY_3, "pasar"]]:
		var pulsada := Input.is_physical_key_pressed(tecla[0])
		if pulsada and not teclas_previas.get(tecla[0], false) and filas.has(tecla[1]) and not filas[tecla[1]].disabled:
			_pulsar(tecla[1])
		teclas_previas[tecla[0]] = pulsada

func _actualizar_transformacion():
	if not token_to_follow or not cam or not contenedor:
		return
	var p_sub := cam.unproject_position(token_to_follow.global_position + Vector3(0, 0.9, 0))
	var zoom: float = cam.get_meta("zoom_factor", 2.0)
	scale = Vector2.ONE * ESCALA_BASE * zoom
	position = contenedor.global_position + p_sub * contenedor.stretch_shrink + OFFSET_PANTALLA * scale

# Primero nos quitamos de en medio (para que el DungeonManager no nos vea abiertos)
# y luego avisamos al CombatManager.
func _pulsar(id: String) -> void:
	if is_queued_for_deletion():
		return
	_previsualizar(false)
	name = "MenuTactico_Cerrando"
	queue_free()
	if AudioManager:
		AudioManager.play_ui("clic")
	if not combat_manager:
		return
	match id:
		"atacar": combat_manager.call("OnBotonAtacar")
		"habilidad": combat_manager.call("OnBotonHabilidad")
		"pasar": combat_manager.call("OnBotonPasar")

func _exit_tree() -> void:
	_previsualizar(false)

# Compatibilidad con las señales antiguas de la escena
func _on_btn_atacar_pressed() -> void:
	_pulsar("atacar")

func _on_btn_habilidad_pressed() -> void:
	_pulsar("habilidad")

func _on_btn_pasar_pressed() -> void:
	_pulsar("pasar")
