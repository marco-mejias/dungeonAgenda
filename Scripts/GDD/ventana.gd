extends Node

# Ventana (autoload): pantalla completa a la resolución nativa del monitor.
#   - Al arrancar se aplica lo guardado en user://ajustes.cfg ([video] pantalla_completa, por defecto sí)
#   - F11 o Alt+Intro la alternan (y se guarda)
#   - El menú de pausa tiene la casilla PANTALLA COMPLETA (llama a fijar())
# La interfaz se escala sola (stretch canvas_items + aspect expand) y el mapa llena la ventana.

const RUTA_AJUSTES := "user://ajustes.cfg"

signal cambiada(pantalla_completa: bool)

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	# Los tests y las capturas van en ventana (no tapar el escritorio del que los lanza)
	var args := OS.get_cmdline_user_args()
	if DisplayServer.get_name() == "headless" or (args.size() > 0 and not "--pantalla_completa" in args):
		return
	var cfg := ConfigFile.new()
	var completa := true
	if cfg.load(RUTA_AJUSTES) == OK:
		completa = bool(cfg.get_value("video", "pantalla_completa", true))
	fijar(completa, false)

## Jugando dentro del editor (pestaña Game con "Embed Game") la ventana solo puede ir en modo ventana
func incrustada() -> bool:
	return Engine.has_method("is_embedded_in_editor") and Engine.is_embedded_in_editor()

func es_completa() -> bool:
	var modo := DisplayServer.window_get_mode()
	return modo == DisplayServer.WINDOW_MODE_FULLSCREEN or modo == DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN

func fijar(completa: bool, guardar: bool = true) -> void:
	if incrustada():
		# Godot no deja cambiar el modo de una ventana incrustada en el editor: se guarda la
		# preferencia (se aplicará al jugar en una ventana propia o en el juego exportado)
		if guardar:
			_guardar(completa)
		print("[Ventana] El juego está incrustado en el editor: la pantalla completa se aplicará al jugar en una ventana aparte.")
		return
	if completa:
		# A pantalla completa sin bordes: ocupa el monitor entero a su resolución nativa
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
	else:
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
		# En ventana: 1920x1080 si cabe (si no, lo que quepa), centrada en su monitor
		var pantalla := DisplayServer.window_get_current_screen()
		var util := DisplayServer.screen_get_usable_rect(pantalla)
		var tam := Vector2i(mini(1920, util.size.x - 40), mini(1080, util.size.y - 60))
		DisplayServer.window_set_size(tam)
		DisplayServer.window_set_position(util.position + (util.size - tam) / 2)
	if guardar:
		_guardar(completa)
	cambiada.emit(completa)

func _guardar(completa: bool) -> void:
	var cfg := ConfigFile.new()
	cfg.load(RUTA_AJUSTES)
	cfg.set_value("video", "pantalla_completa", completa)
	cfg.save(RUTA_AJUSTES)

func _input(event: InputEvent) -> void:
	if not (event is InputEventKey and event.pressed and not event.echo):
		return
	var k: InputEventKey = event
	if k.physical_keycode == KEY_F11 or (k.physical_keycode == KEY_ENTER and k.alt_pressed):
		fijar(not es_completa())
		get_viewport().set_input_as_handled()
