extends SceneTree
# Guarda el tema "piedra tallada" (Scripts/GDD/estilo.gd) como tema del proyecto.
# Después de Tools/generar_ui_piedra.gd (y de importar los PNG):
#   godot --headless --path . -s res://Tools/guardar_tema_piedra.gd

const RUTA := "res://Assets/UI/Piedra/tema_piedra.tres"

func _init() -> void:
	var estilo = load("res://Scripts/GDD/estilo.gd").new()
	var tema: Theme = estilo.crear_tema()
	var err := ResourceSaver.save(tema, RUTA)
	print("tema ", RUTA, " -> ", "OK" if err == OK else "ERROR %d" % err)
	estilo.free()
	quit()
