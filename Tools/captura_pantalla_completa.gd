extends SceneTree

# Arranca la partida a pantalla completa, espera y guarda una captura (comprobar que llena el monitor).
# Uso: Godot --path . -s res://Tools/captura_pantalla_completa.gd -- <carpeta> --pantalla_completa

var t := 0.0
var carpeta := "user://"

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0 and not args[0].begins_with("--"):
		carpeta = args[0]
	change_scene_to_file("res://Scenes/main.tscn")

func _process(delta: float) -> bool:
	t += delta
	if t > 12.0:
		var img := root.get_texture().get_image()
		img.save_png(carpeta.path_join("pantalla_completa.png"))
		var c := root.get_node_or_null("Main/ContenedorMapa")
		print("[captura] ventana ", DisplayServer.window_get_size(), " modo ", DisplayServer.window_get_mode(), " imagen ", img.get_size(), " mapa ", c.size if c else "?", " subviewport ", c.get_child(0).size if c else "?")
		return true
	return false
