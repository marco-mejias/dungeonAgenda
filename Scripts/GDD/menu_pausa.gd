extends CanvasLayer

const RUTA_AJUSTES := "user://ajustes.cfg"
# Resolución interna del mapa: cuánto se reduce el SubViewport (más = más pixelado)
const RESOLUCIONES := [["PS1 (baja)", 3], ["Media", 2], ["Alta", 1]]

@onready var panel_opciones: Panel = $ColorRect/PanelOpciones
@onready var caja: VBoxContainer = $ColorRect/PanelOpciones/VBoxContainer
@onready var slider_musica: HSlider = $ColorRect/PanelOpciones/VBoxContainer/SliderMusica
@onready var slider_sfx: HSlider = $ColorRect/PanelOpciones/VBoxContainer/SliderSFX
@onready var btn_volver: Button = $ColorRect/PanelOpciones/VBoxContainer/BtnVolver

var opt_resolucion: OptionButton
var slider_jitter: HSlider
var chk_tramado: CheckBox
var chk_crt: CheckBox

var ajustes := {"resolucion": 2, "jitter": 1.5, "tramado": true, "crt": false}

func _ready() -> void:
	# El menú empieza oculto cuando estás jugando
	self.visible = false
	panel_opciones.visible = true
	$ColorRect.color = Color(0, 0, 0, 0.55)
	_crear_opciones_visuales()
	_cargar_ajustes()
	_aplicar_ajustes()

# Esta función lee las teclas que pulsas en todo momento
func _input(event: InputEvent) -> void:
	# "ui_cancel" es la tecla ESCAPE por defecto en Godot
	if event.is_action_pressed("ui_cancel"):
		alternar_pausa()

func alternar_pausa() -> void:
	var esta_pausado = get_tree().paused
	get_tree().paused = not esta_pausado

	# Mostramos u ocultamos la capa ENTERA
	self.visible = get_tree().paused

	if get_tree().paused:
		AudioManager.play_ui("open")
		var index_musica = AudioServer.get_bus_index("Musica")
		var index_sfx = AudioServer.get_bus_index("SFX")
		slider_musica.value = db_to_linear(AudioServer.get_bus_volume_db(index_musica))
		slider_sfx.value = db_to_linear(AudioServer.get_bus_volume_db(index_sfx))
	else:
		_guardar_ajustes()

# --- OPCIONES VISUALES ---

func _etiqueta(texto: String) -> Label:
	var l := Label.new()
	l.text = texto
	return l

func _crear_opciones_visuales() -> void:
	# Hueco para los controles nuevos: ensanchamos la caja centrada
	caja.offset_left = -130
	caja.offset_right = 130
	caja.offset_top = -230
	caja.offset_bottom = 230
	var pos := btn_volver.get_index()

	var titulo := _etiqueta("GRAPHICS")
	titulo.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var elementos: Array[Control] = [titulo]

	elementos.append(_etiqueta("INTERNAL RESOLUTION"))
	opt_resolucion = OptionButton.new()
	opt_resolucion.focus_mode = Control.FOCUS_NONE
	for r in RESOLUCIONES:
		opt_resolucion.add_item(r[0], r[1])
	opt_resolucion.item_selected.connect(func(i): ajustes["resolucion"] = opt_resolucion.get_item_id(i); _aplicar_ajustes())
	elementos.append(opt_resolucion)

	elementos.append(_etiqueta("VERTEX JITTER"))
	slider_jitter = HSlider.new()
	slider_jitter.min_value = 0.0
	slider_jitter.max_value = 3.0
	slider_jitter.step = 0.25
	slider_jitter.value_changed.connect(func(v): ajustes["jitter"] = v; _aplicar_ajustes())
	elementos.append(slider_jitter)

	chk_tramado = CheckBox.new()
	chk_tramado.text = "DITHERING"
	chk_tramado.focus_mode = Control.FOCUS_NONE
	chk_tramado.toggled.connect(func(on): ajustes["tramado"] = on; _aplicar_ajustes())
	elementos.append(chk_tramado)

	chk_crt = CheckBox.new()
	chk_crt.text = "CRT SCANLINES"
	chk_crt.focus_mode = Control.FOCUS_NONE
	chk_crt.toggled.connect(func(on): ajustes["crt"] = on; _aplicar_ajustes())
	elementos.append(chk_crt)

	for e in elementos:
		caja.add_child(e)
		caja.move_child(e, pos)
		pos += 1

func _cargar_ajustes() -> void:
	var cfg := ConfigFile.new()
	if cfg.load(RUTA_AJUSTES) == OK:
		for k in ajustes.keys():
			ajustes[k] = cfg.get_value("graficos", k, ajustes[k])
	for i in opt_resolucion.item_count:
		if opt_resolucion.get_item_id(i) == int(ajustes["resolucion"]):
			opt_resolucion.select(i)
	slider_jitter.set_value_no_signal(float(ajustes["jitter"]))
	chk_tramado.set_pressed_no_signal(bool(ajustes["tramado"]))
	chk_crt.set_pressed_no_signal(bool(ajustes["crt"]))

func _guardar_ajustes() -> void:
	var cfg := ConfigFile.new()
	for k in ajustes.keys():
		cfg.set_value("graficos", k, ajustes[k])
	cfg.save(RUTA_AJUSTES)

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

# --- BOTÓN RETURN / VOLVER ---
func _on_return_pressed() -> void:
	AudioManager.play_ui("clic")
	# Al darle al botón, quitamos la pausa (que ocultará el menú automáticamente)
	alternar_pausa()

# --- CONTROL DE VOLUMEN (Igual que en el Menú Principal) ---
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
