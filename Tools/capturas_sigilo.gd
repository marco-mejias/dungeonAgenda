extends SceneTree
# Capturas del sigilo (sin pasar el test): llegar a la vista de un campamento sin que os vean,
# la tarjeta de un guardia, ocultarse con la antorcha apagada, una ronda (los vigías giran) y una
# puñalada por la espalda.   godot --path . -s res://Tools/capturas_sigilo.gd -- <carpeta>
var carpeta := "user://"
var t := 0.0
var paso := 0
var dm: Node
var pm: Node
var hud: Node
var sig: Node
var objetivo: Node = null
var peor := 0.0   # el frame más largo de este paso (ms)
var ultimo_us := 0

func _initialize():
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		carpeta = args[0]
	DirAccess.make_dir_recursive_absolute(carpeta)
	Engine.set_meta("test_sin_espera_banner", true)
	change_scene_to_file("res://Scenes/main.tscn")

func _foto(nombre: String) -> void:
	print("[capturas] (antes de la foto, frame más largo %.1f ms)" % peor)
	root.get_texture().get_image().save_png(carpeta.path_join(nombre + ".png"))
	print("[capturas] ", nombre)

func _act() -> Node:
	return pm.call("GetActiveCharacter")

func _siguiente(espera: float) -> void:
	print("[capturas] paso %d: frame más largo %.1f ms" % [paso, peor])
	peor = 0.0
	paso += 1
	t = -espera
	ultimo_us = 0 # el frame de la foto (guardar el PNG) no cuenta

func _no_me_ve(c: Vector2i, h: Node) -> bool:
	for e in root.get_node("Tactica").unidades():
		if not root.get_node("Tactica").es_heroe(e) and sig.ve(e, c, h):
			return false
	return true

# Un sitio a 4-7 casillas del cofre de un campamento donde nadie le vea
func _acercarse() -> bool:
	var h := _act()
	var tac = root.get_node("Tactica")
	for z in dm.get_meta("zonas", []):
		if z["tipo"] != "campamento":
			continue
		var centro: Vector2i = z["cofre"]
		for r in [5, 4, 6, 7]:
			for dx in range(-r, r + 1):
				for dy in [-r, r]:
					for c in [centro + Vector2i(dx, dy), centro + Vector2i(dy, dx)]:
						if tac.solido(c) or tac.unidad_en(c) != null or not _no_me_ve(c, h):
							continue
						dm.call("TeletransporteDebug", c.x, c.y)
						if h.get_meta("grid_pos") != c:
							continue
						dm.call("RefrescarLuces")
						if not sig.vigilantes().is_empty():
							print("[capturas] en ", c, " junto al campamento ", z["indice"])
							return int(sig.comprobar(h, c.x, c.y)) == 1
	return false

func _process(d: float) -> bool:
	var ahora := Time.get_ticks_usec()
	if ultimo_us > 0:
		peor = maxf(peor, (ahora - ultimo_us) / 1000.0)
	ultimo_us = ahora
	t += d
	if t < 0.0:
		return false
	match paso:
		0:
			if t < 4.0:
				return false
			dm = root.get_node("Main/ContenedorMapa/SubViewport/DungeonManager")
			pm = root.get_node("Main/PartyManager")
			hud = root.get_node("Main/CanvasLayer/HUD")
			sig = root.get_node("Sigilo")
			if dm.get_meta("debug_mode", false):
				dm.call("ToggleModoDebug")
			print("[capturas] sigilo empieza: ", _acercarse())
			_siguiente(1.5)
		1:
			_foto("s1_sigilo")
			# La cámara mira al primer vigilante (para ver su cono) y se sacan tres fotos del pulso
			for e in sig.vigilantes():
				dm.get_node("Camera3D").set_meta("vistazo", e.position)
				break
			_siguiente(1.0)
		2:
			_foto("s1b_cono_a")
			_siguiente(1.3)
		3:
			_foto("s1c_cono_b")
			_siguiente(1.3)
		4:
			_foto("s1d_cono_c")
			var cam = dm.get_node("Camera3D")
			if cam.has_meta("vistazo"):
				cam.remove_meta("vistazo")
			for e in sig.vigilantes():
				objetivo = e
				dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_DISABLED
				var g: Vector2i = e.get_meta("grid_pos")
				dm.call("ActualizarHover", g.x, g.y)
				break
			_siguiente(0.5)
		5:
			_foto("s2_tarjeta_guardia")
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_INHERIT
			dm.set_meta("hover_celda", Vector2i(-1, -1))
			dm.call("RefrescarLuces")
			var antes: int = (dm.call("DebugAlcance") as PackedInt32Array).size() / 2
			var pa_antes := int(_act().get_meta("pa_actual"))
			hud._accion("ocultarse")
			dm.call("RefrescarLuces")
			print("[capturas] alcance antes de ocultarse: %d casillas con %d PA | oculto: %d casillas con %d PA" % [antes, pa_antes, (dm.call("DebugAlcance") as PackedInt32Array).size() / 2, int(_act().get_meta("pa_actual"))])
			hud._accion("antorcha")
			_siguiente(0.6)
		6:
			_foto("s3_oculto_sin_antorcha")
			# Se acaba la ronda: todos sin PA -> los vigías giran
			for h in pm.get_children():
				if h.has_meta("grid_pos"):
					h.set_meta("pa_actual", 0)
			pm.call("TerminarTurnoPersonaje")
			_siguiente(1.5)
		7:
			_foto("s4_nueva_ronda")
			print("[capturas] sigue en sigilo: ", sig.en_sigilo, " | combate: ", root.get_node("Modo").en_combate)
			# Puñalada: detrás de un desprevenido que no duerma
			var h := _act()
			var hecho := false
			for e in root.get_node("Tactica").unidades():
				if root.get_node("Tactica").es_heroe(e) or e.get_meta("in_combat", false) or sig.tipo(e) == "dormido":
					continue
				var c: Vector2i = e.get_meta("grid_pos") - sig.mirada(e)
				dm.call("TeletransporteDebug", c.x, c.y)
				if h and h.get_meta("grid_pos") == c:
					print("[capturas] puñalada a ", e.name, " desde ", c, ": ", root.get_node("Tactica").modificadores(h, e))
					var g: Vector2i = e.get_meta("grid_pos")
					h.set_meta("pa_actual", 6)
					dm.call("ProcesarClicEnCasilla", g.x, g.y)
					hecho = true
					break
			print("[capturas] intento de puñalada: ", hecho)
			_siguiente(1.0)
		8:
			_foto("s5_punalada")
			_siguiente(2.0)
		9:
			_foto("s6_combate_sorpresa")
			return true
	return false
