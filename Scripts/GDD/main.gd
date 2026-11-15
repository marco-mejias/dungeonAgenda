extends Node

# 1. Native GDScript state machine
enum GameState {
	EXPLORING_MAP,
	CHARACTER_STATS_OPEN,
	EVENT_REIGNS_ACTIVE
}

var current_state: GameState = GameState.EXPLORING_MAP

@onready var contenedor_mapa: Control = $ContenedorMapa

func _ready():
	print("[Main GDScript] Iniciado. Jefe al mando.")
	# HUD and event screen titles in gothic type
	Estilo.como_titulo(get_node_or_null("CanvasLayer/MarcadorTurnos"), 24)
	Estilo.como_titulo(get_node_or_null("CanvasLayer2/PantallaEvento/Titulo"), 32)
	# Inventory screen (I) and inventory/equipment debug panel (F10)
	add_child(preload("res://Scripts/GDD/pantalla_inventario.gd").new())
	add_child(preload("res://Scripts/GDD/debug_inventario.gd").new())
	# Level up: what was gained and the ability draft (1 of 3)
	add_child(preload("res://Scripts/GDD/pantalla_nivel.gd").new())
	# The map takes the whole window (with aspect "expand" it can be wider or taller than 1152x648):
	# its parent is a Node2D, so there are no anchors to do it for us
	get_viewport().size_changed.connect(_ajustar_mapa)
	_ajustar_mapa()

# The map container always covers the whole window
func _ajustar_mapa() -> void:
	if contenedor_mapa:
		contenedor_mapa.position = Vector2.ZERO
		contenedor_mapa.size = get_viewport().get_visible_rect().size

func _process(_delta):
	if Input.is_action_just_pressed("clic_izq"):
		match current_state:
			GameState.EXPLORING_MAP:
				# Anti input bleeding shield: if the mouse is over any interface control (buttons,
				# cards...), the click does NOT reach the map.
				var bajo_raton := get_viewport().gui_get_hovered_control()
				if bajo_raton != null and bajo_raton != contenedor_mapa:
					return

				# Aiming an ability: the click picks its target
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
	pass
