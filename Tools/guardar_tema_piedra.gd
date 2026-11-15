extends SceneTree
# Saves the "carved stone" theme (Scripts/GDD/estilo.gd) as the project theme.
# After Tools/generar_ui_piedra.gd (and importing the PNGs):
#   godot --headless --path . -s res://Tools/guardar_tema_piedra.gd

const RUTA := "res://Assets/UI/Piedra/tema_piedra.tres"

func _init() -> void:
	var estilo = load("res://Scripts/GDD/estilo.gd").new()
	var tema: Theme = estilo.crear_tema()
	var err := ResourceSaver.save(tema, RUTA)
	print("tema ", RUTA, " -> ", "OK" if err == OK else "ERROR %d" % err)
	estilo.free()
	quit()
