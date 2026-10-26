extends Node

# 1. Máquina de estados nativa de GDScript
enum GameState {
	EXPLORING_MAP,
	CHARACTER_STATS_OPEN,
	EVENT_REIGNS_ACTIVE
}

var current_state: GameState = GameState.EXPLORING_MAP

@onready var contenedor_mapa: Control = $ContenedorMapa

func _ready():
	print("[Main GDScript] Iniciado. Jefe al mando.")

func _process(_delta):
	if Input.is_action_just_pressed("clic_izq"):
		match current_state:
			GameState.EXPLORING_MAP:
				# Escudo anti-input bleeding: si el ratón está sobre cualquier control
				# de la interfaz (botones, fichas, menú táctico...), el clic NO llega al mapa.
				var bajo_raton := get_viewport().gui_get_hovered_control()
				if bajo_raton != null and bajo_raton != contenedor_mapa:
					return

				var input_manager = get_node_or_null("ContenedorMapa/SubViewport/DungeonManager/InputManager")
				if input_manager:
					var mouse_pos = get_viewport().get_mouse_position()
					input_manager.call("HandleMapClick", mouse_pos.x, mouse_pos.y)
				else:
					print("[ERROR] No encuentro el InputManager. ¿Lo has colgado como hijo de DungeonManager?")


func _on_btn_volver_pressed() -> void:
	pass # Replace with function body.
