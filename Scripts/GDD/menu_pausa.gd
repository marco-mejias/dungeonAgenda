extends CanvasLayer

const RUTA_AJUSTES := "user://ajustes.cfg"
# Internal map resolution: how much the SubViewport shrinks (more = more pixelated)
const RESOLUCIONES := [["PS1 (baja)", 3], ["Media", 2], ["Alta", 1]]
const SACUDIDAS := [["Desactivada", 0], ["Sutil", 1], ["Fuerte", 2]]

@onready var panel_opciones: Panel = $ColorRect/PanelOpciones
@onready var caja: VBoxContainer = $ColorRect/PanelOpciones/VBoxContainer
@onready var slider_musica: HSlider = $ColorRect/PanelOpciones/VBoxContainer/SliderMusica
@onready var slider_sfx: HSlider = $ColorRect/PanelOpciones/VBoxContainer/SliderSFX
@onready var btn_volver: Button = $ColorRect/PanelOpciones/VBoxContainer/BtnVolver

var opt_resolucion: OptionButton
var opt_sacudida: OptionButton
var slider_jitter: HSlider
var chk_tramado: CheckBox
var chk_crt: CheckBox
var chk_pantalla: CheckBox

var ajustes := {"resolucion": 2, "jitter": 1.5, "tramado": true, "crt": false, "sacudida": 1}

func _ready() -> void:
	# The menu starts hidden while you're playing
	self.visible = false
	panel_opciones.visible = true
	$ColorRect.color = Estilo.color("velo")
	_traducir()
	_crear_opciones_visuales()
	_cargar_ajustes()
	_aplicar_ajustes()

# Texts and title style of the pause menu
func _traducir() -> void:
	var titulo: Label = caja.get_node("Label")
	titulo.text = "Opciones"
	Estilo.como_titulo(titulo, 32)
	_estilo_etiqueta(caja.get_node("Label2"), "VOLUMEN DE MÚSICA")
	_estilo_etiqueta(caja.get_node("Label3"), "VOLUMEN DE EFECTOS")
	btn_volver.text = "VOLVER"
	btn_volver.custom_minimum_size = Vector2(0, 40)
	# Make the button use the theme's PS1 style instead of the old greys
	for e in ["normal", "hover"]:
		btn_volver.remove_theme_stylebox_override(e)

# Small soft label for an option
func _estilo_etiqueta(l: Label, texto: String) -> Label:
	l.text = texto
	l.add_theme_font_size_override("font_size", 11)
	l.add_theme_color_override("font_color", Estilo.color("texto_suave"))
	return l

# This function reads the keys you press at all times
func _input(event: InputEvent) -> void:
	# "ui_cancel" is the ESCAPE key by default in Godot
	if event.is_action_pressed("ui_cancel"):
		alternar_pausa()

# Pauses or resumes the game (ESC) and shows/hides the menu
func alternar_pausa() -> void:
	var esta_pausado = get_tree().paused
	get_tree().paused = not esta_pausado

	# We show or hide the WHOLE layer
	self.visible = get_tree().paused

	if get_tree().paused:
		AudioManager.play_ui("open")
		var index_musica = AudioServer.get_bus_index("Musica")
		var index_sfx = AudioServer.get_bus_index("SFX")
		slider_musica.value = db_to_linear(AudioServer.get_bus_volume_db(index_musica))
		slider_sfx.value = db_to_linear(AudioServer.get_bus_volume_db(index_sfx))
	else:
		_guardar_ajustes()

# --- VISUAL OPTIONS ---

# Adds the visual options (resolution, dithering, CRT, fullscreen, screen shake) under the volume sliders
func _crear_opciones_visuales() -> void:
	# Room for the new controls: we widen the centred box
	caja.offset_left = -140
	caja.offset_right = 140
	caja.offset_top = -250
	caja.offset_bottom = 250
	caja.add_theme_constant_override("separation", 4)
	var pos := btn_volver.get_index()

	var titulo := Label.new()
	titulo.text = "Gráficos"
	Estilo.como_titulo(titulo, 24)
	titulo.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var elementos: Array[Control] = [titulo]

	elementos.append(_estilo_etiqueta(Label.new(), "RESOLUCIÓN INTERNA"))
	opt_resolucion = _opciones(RESOLUCIONES, "resolucion")
	elementos.append(opt_resolucion)

	elementos.append(_estilo_etiqueta(Label.new(), "TEMBLOR DE VÉRTICES"))
	slider_jitter = HSlider.new()
	slider_jitter.min_value = 0.0
	slider_jitter.max_value = 3.0
	slider_jitter.step = 0.25
	slider_jitter.value_changed.connect(_cambiar.bind("jitter"))
	elementos.append(slider_jitter)

	chk_tramado = _casilla("TRAMADO DE COLOR", "tramado")
	elementos.append(chk_tramado)
	chk_crt = _casilla("SCANLINES CRT", "crt")
	elementos.append(chk_crt)
	# Fullscreen at the monitor resolution (handled by the Ventana autoload; F11 too)
	chk_pantalla = CheckBox.new()
	chk_pantalla.text = "PANTALLA COMPLETA  (F11)"
	chk_pantalla.focus_mode = Control.FOCUS_NONE
	chk_pantalla.add_theme_font_size_override("font_size", 12)
	chk_pantalla.set_pressed_no_signal(Ventana.es_completa())
	if Ventana.incrustada():
		chk_pantalla.disabled = true
		chk_pantalla.tooltip_text = "Dentro del editor (Embed Game) el juego solo puede ir en ventana.
Juega en una ventana aparte o con el juego exportado."
	chk_pantalla.toggled.connect(func(v): Ventana.fijar(v))
	Ventana.cambiada.connect(func(v): chk_pantalla.set_pressed_no_signal(v))
	elementos.append(chk_pantalla)

	elementos.append(_estilo_etiqueta(Label.new(), "SACUDIDA DE PANTALLA"))
	opt_sacudida = _opciones(SACUDIDAS, "sacudida")
	elementos.append(opt_sacudida)

	for e in elementos:
		caja.add_child(e)
		caja.move_child(e, pos)
		pos += 1

# Dropdown bound to one setting
func _opciones(lista: Array, clave: String) -> OptionButton:
	var o := OptionButton.new()
	o.focus_mode = Control.FOCUS_NONE
	o.add_theme_font_size_override("font_size", 12)
	for r in lista:
		o.add_item(r[0], r[1])
	o.item_selected.connect(func(i): _cambiar(o.get_item_id(i), clave))
	return o

# Checkbox bound to one setting
func _casilla(texto: String, clave: String) -> CheckBox:
	var c := CheckBox.new()
	c.text = texto
	c.focus_mode = Control.FOCUS_NONE
	c.add_theme_font_size_override("font_size", 12)
	c.toggled.connect(_cambiar.bind(clave))
	return c

# A setting changed: apply it right away
func _cambiar(valor, clave: String) -> void:
	ajustes[clave] = valor
	_aplicar_ajustes()

# Selects the dropdown item with that id
func _seleccionar(o: OptionButton, id: int) -> void:
	for i in o.item_count:
		if o.get_item_id(i) == id:
			o.select(i)

# Reads the saved settings (user://ajustes.cfg)
func _cargar_ajustes() -> void:
	var cfg := ConfigFile.new()
	if cfg.load(RUTA_AJUSTES) == OK:
		for k in ajustes.keys():
			ajustes[k] = cfg.get_value("graficos", k, ajustes[k])
	_seleccionar(opt_resolucion, int(ajustes["resolucion"]))
	_seleccionar(opt_sacudida, int(ajustes["sacudida"]))
	slider_jitter.set_value_no_signal(float(ajustes["jitter"]))
	chk_tramado.set_pressed_no_signal(bool(ajustes["tramado"]))
	chk_crt.set_pressed_no_signal(bool(ajustes["crt"]))

# Saves the settings
func _guardar_ajustes() -> void:
	var cfg := ConfigFile.new()
	cfg.load(RUTA_AJUSTES) # Without losing other sections ([video] for the window)
	for k in ajustes.keys():
		cfg.set_value("graficos", k, ajustes[k])
	cfg.save(RUTA_AJUSTES)

# Applies every setting: map resolution, post-process shaders, vertex jitter and Feel level
func _aplicar_ajustes() -> void:
	var contenedor := get_node_or_null("/root/Main/ContenedorMapa") as SubViewportContainer
	if contenedor:
		contenedor.stretch_shrink = int(ajustes["resolucion"])
		var post := contenedor.material as ShaderMaterial
		if post:
			post.set_shader_parameter("dither_strength", 0.85 if ajustes["tramado"] else 0.0)
			post.set_shader_parameter("levels", 32.0 if ajustes["tramado"] else 256.0)
			post.set_shader_parameter("crt", bool(ajustes["crt"]))
	RenderingServer.global_shader_parameter_set("ps1_jitter", float(ajustes["jitter"]))
	var feel := get_node_or_null("/root/Feel")
	if feel:
		feel.nivel = int(ajustes["sacudida"])

# --- BACK BUTTON ---
func _on_return_pressed() -> void:
	AudioManager.play_ui("clic")
	# When the button is pressed we unpause (which hides the menu automatically)
	alternar_pausa()

# --- VOLUME CONTROL (same as in the Main Menu) ---
func _on_slider_musica_value_changed(value: float) -> void:
	var bus_index = AudioServer.get_bus_index("Musica")
	AudioServer.set_bus_volume_db(bus_index, linear_to_db(value))

func _on_slider_sfx_value_changed(value: float) -> void:
	var bus_index = AudioServer.get_bus_index("SFX")
	AudioServer.set_bus_volume_db(bus_index, linear_to_db(value))

	if not AudioManager.sfx_player.playing:
		AudioManager.play_sfx("walk")

func _on_return_mouse_entered() -> void:
	AudioManager.play_ui("hover")
