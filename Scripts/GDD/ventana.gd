extends Node

# Ventana (autoload): fullscreen at the monitor's native resolution.
#   - At startup it applies what's saved in user://ajustes.cfg ([video] pantalla_completa, yes by default)
#   - F11 or Alt+Enter toggle it (and it's saved)
#   - The pause menu has the FULLSCREEN checkbox (calls fijar())
# The interface scales on its own (stretch canvas_items + aspect expand) and the map fills the window.

const RUTA_AJUSTES := "user://ajustes.cfg"

signal cambiada(pantalla_completa: bool)

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	# Tests and screenshots run windowed (so they don't cover the desktop of whoever launches them)
	var args := OS.get_cmdline_user_args()
	if DisplayServer.get_name() == "headless" or (args.size() > 0 and not "--pantalla_completa" in args):
		return
	var cfg := ConfigFile.new()
	var completa := true
	if cfg.load(RUTA_AJUSTES) == OK:
		completa = bool(cfg.get_value("video", "pantalla_completa", true))
	fijar(completa, false)

## Playing inside the editor (Game tab with "Embed Game") the window can only be windowed
func incrustada() -> bool:
	return Engine.has_method("is_embedded_in_editor") and Engine.is_embedded_in_editor()

# Is the window fullscreen right now?
func es_completa() -> bool:
	var modo := DisplayServer.window_get_mode()
	return modo == DisplayServer.WINDOW_MODE_FULLSCREEN or modo == DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN

# Fullscreen on or off (and saves the choice)
func fijar(completa: bool, guardar: bool = true) -> void:
	if incrustada():
		# Godot doesn't let you change the mode of a window embedded in the editor: we save the
		# preference (it'll apply when playing in its own window or in the exported game)
		if guardar:
			_guardar(completa)
		print("[Ventana] El juego está incrustado en el editor: la pantalla completa se aplicará al jugar en una ventana aparte.")
		return
	if completa:
		# Borderless fullscreen: takes the whole monitor at its native resolution
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
	else:
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
		# Windowed: 1920x1080 if it fits (otherwise whatever fits), centred on its monitor
		var pantalla := DisplayServer.window_get_current_screen()
		var util := DisplayServer.screen_get_usable_rect(pantalla)
		var tam := Vector2i(mini(1920, util.size.x - 40), mini(1080, util.size.y - 60))
		DisplayServer.window_set_size(tam)
		DisplayServer.window_set_position(util.position + (util.size - tam) / 2)
	if guardar:
		_guardar(completa)
	cambiada.emit(completa)

# Saves the choice in user://ajustes.cfg
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
