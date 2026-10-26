extends CanvasLayer

@onready var panel_opciones: Panel = $ColorRect/PanelOpciones
@onready var slider_musica: HSlider = $ColorRect/PanelOpciones/VBoxContainer/SliderMusica
@onready var slider_sfx: HSlider = $ColorRect/PanelOpciones/VBoxContainer/SliderSFX

func _ready() -> void:
	# El menú empieza oculto cuando estás jugando
	self.visible = false
	panel_opciones.visible = true

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
