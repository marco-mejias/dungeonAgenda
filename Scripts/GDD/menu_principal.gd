extends Control

# References to the UI panels
@onready var contenedor_principal: VBoxContainer = $VBoxContainer
@onready var panel_opciones: Panel = $PanelOpciones

# References to the sliders inside the options
@onready var slider_musica: HSlider = $PanelOpciones/VBoxContainer/SliderMusica
@onready var slider_sfx: HSlider = $PanelOpciones/VBoxContainer/SliderSFX

func _ready():
	# Make sure the options panel starts hidden
	panel_opciones.visible = false
	contenedor_principal.visible = true
	# Game title in gothic type
	var titulo: Label = contenedor_principal.get_node_or_null("Label")
	if titulo:
		titulo.text = "Dungeon Agenda"
		Estilo.como_titulo(titulo, 48)
	
	# --- AUDIO BUS SYNC --- We look up the position of our channels in the mixer
	var index_musica = AudioServer.get_bus_index("Musica")
	var index_sfx = AudioServer.get_bus_index("SFX")
	
	# We use db_to_linear to translate the engine decibels (e.g. 0 dB) into the slider percentage
	# (1.0)
	slider_musica.value = db_to_linear(AudioServer.get_bus_volume_db(index_musica))
	slider_sfx.value = db_to_linear(AudioServer.get_bus_volume_db(index_sfx))
	
	# Start the background music
	AudioManager.play_music("dungeon")
	
	# Make sure the game doesn't start paused by mistake
	get_tree().paused = false
	
# --- MAIN MENU BUTTONS ---
	
# Start a new game
func _on_btn_jugar_pressed():
	AudioManager.play_ui("clic")
	get_tree().change_scene_to_file("res://Scenes/main.tscn")
	
func _on_opciones_pressed() -> void:
	AudioManager.play_ui("clic")
	# We hide the main buttons and show the options
	contenedor_principal.visible = false
	panel_opciones.visible = true
	
# Quit (after the "no" sound plays)
func _on_btn_salir_pressed():
	AudioManager.play_ui("no")
	await get_tree().create_timer(0.5).timeout # Wait for the sound to finish
	get_tree().quit()

# --- OPTIONS BUTTONS ---

func _on_volver_pressed() -> void:
	AudioManager.play_ui("clic")
	# The other way round: we hide the options and show the main menu
	panel_opciones.visible = false
	contenedor_principal.visible = true


# --- PRO VOLUME CONTROL (WITH BUSES) ---

func _on_slider_musica_value_changed(value: float) -> void:
	var bus_index = AudioServer.get_bus_index("Musica")
	# linear_to_db turns the slider's 0.0-1.0 into the right decibels (-80dB to 0dB)
	AudioServer.set_bus_volume_db(bus_index, linear_to_db(value))

func _on_slider_sfx_value_changed(value: float) -> void:
	var bus_index = AudioServer.get_bus_index("SFX")
	AudioServer.set_bus_volume_db(bus_index, linear_to_db(value))
	
	if not AudioManager.sfx_player.playing:
		AudioManager.play_sfx("walk")

# --- HOVER SOUNDS ---

func _on_btn_jugar_mouse_entered() -> void:
	AudioManager.play_ui("hover")

func _on_btn_salir_mouse_entered() -> void:
	AudioManager.play_ui("hover")

# (Optional) If you add a hover sound to the options or back button, you can connect its signals
# here:
func _on_opciones_mouse_entered() -> void:
	AudioManager.play_ui("hover")

func _on_volver_mouse_entered() -> void:
	AudioManager.play_ui("hover")


func _on_btn_opciones_pressed() -> void:
	pass
