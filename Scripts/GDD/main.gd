extends Node

# 1. Máquina de estados nativa de GDScript
enum GameState {
	EXPLORING_MAP,
	CHARACTER_STATS_OPEN,
	EVENT_REIGNS_ACTIVE
}

var current_state: GameState = GameState.EXPLORING_MAP

func _ready():
	print("[Main GDScript] Iniciado. Jefe al mando.")

func _process(_delta):
	# Escuchamos el clic izquierdo
	if Input.is_action_just_pressed("clic_izq"):
		
		match current_state:
			
			GameState.EXPLORING_MAP:
				# En GDScript, buscar a un hijo directo es tan fácil como usar el símbolo $
				if has_node("DungeonManager"):
					var dungeon = $DungeonManager
					var mouse_pos = get_viewport().get_mouse_position()
					
					print("[Main GDScript] Clic detectado. Pasando coordenadas al DungeonManager...")
					# Llamamos a tu función de C++ desde GDScript
					dungeon.call("HandleMapClick", mouse_pos.x, mouse_pos.y)
				else:
					print("[ERROR] El Main no encuentra a su hijo 'DungeonManager'.")
					
			GameState.CHARACTER_STATS_OPEN:
				print("[Main GDScript] Clic ignorado por el mapa. Menú de Stats abierto.")
				
			GameState.EVENT_REIGNS_ACTIVE:
				print("[Main GDScript] Clic ignorado por el mapa. Evento activo.")
