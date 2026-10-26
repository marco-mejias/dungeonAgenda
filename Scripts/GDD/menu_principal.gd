extends Control

# Referencias a los paneles de la UI
@onready var contenedor_principal: VBoxContainer = $VBoxContainer
@onready var panel_opciones: Panel = $PanelOpciones

# Referencias a los sliders dentro de opciones
@onready var slider_musica: HSlider = $PanelOpciones/VBoxContainer/SliderMusica
@onready var slider_sfx: HSlider = $PanelOpciones/VBoxContainer/SliderSFX

func _ready():
	# Asegurarnos de que el panel de opciones empieza oculto
	panel_opciones.visible = false
	contenedor_principal.visible = true
	
	# --- SINCRONIZACIÓN CON BUSES DE AUDIO ---
	# Buscamos la posición de nuestros canales en la mesa de mezclas
	var index_musica = AudioServer.get_bus_index("Musica")
	var index_sfx = AudioServer.get_bus_index("SFX")
	
	# Usamos db_to_linear para traducir los decibelios del motor (ej. 0 dB) al porcentaje del slider (1.0)
	slider_musica.value = db_to_linear(AudioServer.get_bus_volume_db(index_musica))
	slider_sfx.value = db_to_linear(AudioServer.get_bus_volume_db(index_sfx))
	
	# Arrancar la música de fondo
	AudioManager.play_music("dungeon")
	
	# Asegurarnos de que el juego no empiece pausado por error
	get_tree().paused = false
	
# --- BOTONES DEL MENÚ PRINCIPAL ---
	
func _on_btn_jugar_pressed():
	AudioManager.play_ui("clic")
	get_tree().change_scene_to_file("res://Scenes/main.tscn")
	
func _on_opciones_pressed() -> void:
	AudioManager.play_ui("clic")
	# Escondemos los botones principales y mostramos las opciones
	contenedor_principal.visible = false
	panel_opciones.visible = true
	
func _on_btn_salir_pressed():
	AudioManager.play_ui("no")
	await get_tree().create_timer(0.5).timeout # Espera a que termine el sonido
	get_tree().quit()

# --- BOTONES DE OPCIONES ---

func _on_volver_pressed() -> void:
	AudioManager.play_ui("clic")
	# Proceso inverso: escondemos opciones, mostramos menú principal
	panel_opciones.visible = false
	contenedor_principal.visible = true


# --- CONTROL DE VOLUMEN PRO (CON BUSES) ---

func _on_slider_musica_value_changed(value: float) -> void:
	var bus_index = AudioServer.get_bus_index("Musica")
	# linear_to_db convierte el 0.0-1.0 del slider a los decibelios correctos (-80dB a 0dB)
	AudioServer.set_bus_volume_db(bus_index, linear_to_db(value))

func _on_slider_sfx_value_changed(value: float) -> void:
	var bus_index = AudioServer.get_bus_index("SFX")
	AudioServer.set_bus_volume_db(bus_index, linear_to_db(value))
	
	if not AudioManager.sfx_player.playing:
		AudioManager.play_sfx("walk")

# --- SONIDOS HOVER (PASAR EL RATÓN) ---

func _on_btn_jugar_mouse_entered() -> void:
	AudioManager.play_ui("hover")

func _on_btn_salir_mouse_entered() -> void:
	AudioManager.play_ui("hover")

# (Opcional) Si añades sonido Hover al botón de opciones o volver, puedes conectar sus señales aquí:
func _on_opciones_mouse_entered() -> void:
	AudioManager.play_ui("hover")

func _on_volver_mouse_entered() -> void:
	AudioManager.play_ui("hover")


func _on_btn_opciones_pressed() -> void:
	pass # Replace with function body.
