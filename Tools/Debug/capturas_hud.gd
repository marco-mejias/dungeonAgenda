extends SceneTree
# Quick interface screenshots (without running the test): exploring, ability card, bubble and
# character card, backpack, combat, hit, event, level up and campfire.
#   godot --path . -s res://Tools/Debug/capturas_hud.gd -- <folder>
var carpeta := "user://"
var t := 0.0
var paso := 0
var dm: Node
var pm: Node
var ui: Node
var hud: Node

func _initialize():
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		carpeta = args[0]
	DirAccess.make_dir_recursive_absolute(carpeta)
	Engine.set_meta("test_sin_espera_banner", true)
	change_scene_to_file("res://Scenes/main.tscn")

func _foto(nombre: String) -> void:
	root.get_texture().get_image().save_png(carpeta.path_join(nombre + ".png"))
	print("[capturas] ", nombre)

func _activo() -> Node:
	return pm.call("GetActiveCharacter")

func _junto_a(obj: Vector2i) -> bool:
	for d in [Vector2i(0, 1), Vector2i(1, 0), Vector2i(0, -1), Vector2i(-1, 0)]:
		if obj + d == dm.get_meta("casilla_salida", Vector2i(-1, -1)):
			continue
		dm.call("TeletransporteDebug", obj.x + d.x, obj.y + d.y)
		if _activo().get_meta("grid_pos") == obj + d:
			return true
	return false

func _siguiente(espera: float) -> void:
	paso += 1
	t = -espera

func _process(d: float) -> bool:
	t += d
	if t < 0.0:
		return false
	match paso:
		0:
			if t < 4.0:
				return false
			dm = root.get_node("Main/ContenedorMapa/SubViewport/DungeonManager")
			pm = root.get_node("Main/PartyManager")
			ui = root.get_node("Main/CanvasLayer")
			hud = ui.get_node("HUD")
			_foto("1_explorando")
			hud.objeto_hover = hud.huecos_hab[0]
			hud.t_hover = 1.0
			_siguiente(0.5)
		1:
			_foto("2_ficha_habilidad")
			hud.objeto_hover = null
			# Next to a chest (bubble) and with the mouse over another hero (their card)
			for c in dm.get_meta("lista_cofres", []):
				if _junto_a(c):
					break
			dm.call("RefrescarLuces")
			for h in pm.get_children():
				if h.has_meta("grid_pos") and h != _activo():
					dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_DISABLED
					var g: Vector2i = h.get_meta("grid_pos")
					dm.call("ActualizarHover", g.x, g.y)
					break
			_siguiente(0.6)
		2:
			_foto("3_bocadillo_y_tarjeta")
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_INHERIT
			for inv in get_nodes_in_group("pantalla_inventario"):
				inv.abrir(str(_activo().name))
			_siguiente(0.8)
		3:
			_foto("4_mochila")
			for inv in get_nodes_in_group("pantalla_inventario"):
				inv.cerrar()
			for u in root.get_node("Tactica").unidades():
				if not root.get_node("Tactica").es_heroe(u):
					u.set_meta("in_combat", true)
					break
			_siguiente(2.5)
		4:
			_foto("5_combate")
			var act := _activo()
			act.set_meta("hp_actual", int(act.get_meta("hp_actual")) - 20)
			_siguiente(0.25)
		5:
			_foto("6_golpe")
			ui.set_meta("heroe_en_peligro", _activo())
			dm.set_meta("bloqueo_evento", true)
			ui.call("MostrarEventoVisual", load("res://Data/Events/FuenteMisteriosa.tres"))
			_siguiente(0.3)
		6:
			_foto("7a_evento_entrando")
			_siguiente(1.2)
		7:
			_foto("7_evento")
			ui.call("CerrarEventoPorDebug")
			var prog = root.get_node("Progreso")
			var h := _activo()
			prog.ganar_xp(h, prog.xp_para(int(h.get_meta("nivel"))) - int(h.get_meta("xp")))
			_siguiente(1.2)
		8:
			_foto("8_nivel")
			var pn = root.get_node_or_null("Main/PantallaNivel")
			if pn and pn.visible:
				pn.call("_cerrar")
			for h in pm.get_children():
				if h.has_meta("grid_pos") and not h.get_meta("ha_escapado", false):
					pm.call("PersonajeEscapa", h)
			_siguiente(3.0)
		9:
			_foto("9_fogata")
			return true
	return false
