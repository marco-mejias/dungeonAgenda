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
	# Títulos del HUD y de la pantalla de evento en gótica
	Estilo.como_titulo(get_node_or_null("CanvasLayer/MarcadorTurnos"), 24)
	Estilo.como_titulo(get_node_or_null("CanvasLayer2/PantallaEvento/Titulo"), 32)
	# Pantalla de inventario (I) y panel de depuración de inventario y equipo (F10)
	add_child(preload("res://Scripts/GDD/pantalla_inventario.gd").new())
	add_child(preload("res://Scripts/GDD/debug_inventario.gd").new())
	# Subida de nivel: lo ganado y el draft de habilidades (1 de 3)
	add_child(preload("res://Scripts/GDD/pantalla_nivel.gd").new())
	# El mapa ocupa toda la ventana (con aspect "expand" puede ser más ancha o alta que 1152x648):
	# su padre es un Node2D, así que no hay anclas que lo hagan solo
	get_viewport().size_changed.connect(_ajustar_mapa)
	_ajustar_mapa()

func _ajustar_mapa() -> void:
	if contenedor_mapa:
		contenedor_mapa.position = Vector2.ZERO
		contenedor_mapa.size = get_viewport().get_visible_rect().size

func _process(_delta):
	if Input.is_action_just_pressed("clic_izq"):
		match current_state:
			GameState.EXPLORING_MAP:
				# Escudo anti-input bleeding: si el ratón está sobre cualquier control
				# de la interfaz (botones, fichas, menú táctico...), el clic NO llega al mapa.
				var bajo_raton := get_viewport().gui_get_hovered_control()
				if bajo_raton != null and bajo_raton != contenedor_mapa:
					return

				# Apuntando una habilidad: el clic elige su objetivo
				if not Progreso.apuntando.is_empty():
					var dm = get_node_or_null("ContenedorMapa/SubViewport/DungeonManager")
					if dm:
						Progreso.confirmar(dm.get_meta("hover_celda", Vector2i(-1, -1)))
					return
				var input_manager = get_node_or_null("ContenedorMapa/SubViewport/DungeonManager/InputManager")
				if input_manager:
					var mouse_pos = get_viewport().get_mouse_position()
					input_manager.call("HandleMapClick", mouse_pos.x, mouse_pos.y)
				else:
					print("[ERROR] No encuentro el InputManager. ¿Lo has colgado como hijo de DungeonManager?")


func _on_btn_volver_pressed() -> void:
	pass # Replace with function body.
