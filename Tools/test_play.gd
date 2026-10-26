extends SceneTree
# Test de humo jugable: arranca main.tscn, simula una partida y guarda capturas.
#   godot --path . -s res://Tools/test_play.gd -- <carpeta_capturas>
# (sin --headless para que haya render y las capturas salgan)

var out_dir := "user://test_shots"
var paso := 0
var espera := 0
var dm: Node
var pm: Node
var ui: Node
var fallos: Array[String] = []
var pa_antes := 0
var pos_antes := Vector2i.ZERO
var objetivo_pick := Vector2i(-1, -1)
var paso_zoom := 0
var movido := ""

func _initialize():
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = args[0]
	DirAccess.make_dir_recursive_absolute(out_dir)
	change_scene_to_file("res://Scenes/main.tscn")

func captura(nombre: String) -> void:
	var img := root.get_texture().get_image()
	img.save_png(out_dir.path_join(nombre + ".png"))
	print("[test] captura: ", nombre)

func check(cond: bool, msg: String) -> void:
	if cond:
		print("[test] OK   ", msg)
	else:
		print("[test] FAIL ", msg)
		fallos.append(msg)

func activo() -> Node:
	return pm.call("GetActiveCharacter")

func pos_activa() -> Vector2i:
	return pm.call("GetActiveGridPos")

func moviendo() -> bool:
	return dm.get_meta("is_moving", false) or dm.get_meta("bloqueo_evento", false)

func tecla(code: Key, pulsada: bool) -> void:
	var ev := InputEventKey.new()
	ev.physical_keycode = code
	ev.keycode = code
	ev.pressed = pulsada
	Input.parse_input_event(ev)

# Teletransporte (debug) a una casilla libre junto a 'objetivo'
func junto_a(objetivo: Vector2i) -> bool:
	for d in [Vector2i(0, 1), Vector2i(1, 0), Vector2i(0, -1), Vector2i(-1, 0)]:
		dm.call("TeletransporteDebug", objetivo.x + d.x, objetivo.y + d.y)
		if pos_activa() == objetivo + d:
			return true
	return false

func _process(_delta):
	if espera > 0:
		espera -= 1
		return false
	if dm == null:
		dm = root.get_node_or_null("Main/ContenedorMapa/SubViewport/DungeonManager")
		pm = root.get_node_or_null("Main/PartyManager")
		ui = root.get_node_or_null("Main/CanvasLayer")
		espera = 60
		return false
	if moviendo() and paso not in [5, 13, 17, 18, 19]:
		return false

	paso += 1
	match paso:
		1:
			captura("01_inicio")
			check(activo() != null, "hay personaje activo al empezar")
			var p := pos_activa()
			check(p.x > 0 and p.y > 0, "el héroe activo está en una casilla válida %s" % p)
			var alc: PackedInt32Array = dm.call("DebugAlcance")
			check(alc.size() > 0, "hay casillas alcanzables (%d)" % (alc.size() / 2))
		2:
			# Movernos a la casilla alcanzable más lejana
			var alc: PackedInt32Array = dm.call("DebugAlcance")
			var p := pos_activa()
			var mejor := p
			for i in range(0, alc.size(), 2):
				var c := Vector2i(alc[i], alc[i + 1])
				if abs(c.x - p.x) + abs(c.y - p.y) > abs(mejor.x - p.x) + abs(mejor.y - p.y):
					mejor = c
			pa_antes = activo().get_meta("pa_actual")
			movido = str(activo().name)
			pos_antes = p
			print("[test] moviendo de ", p, " a ", mejor, " con PA ", pa_antes)
			dm.call("ProcesarClicEnCasilla", mejor.x, mejor.y)
			check(dm.get_meta("is_moving", false), "empieza a moverse")
		3:
			captura("02_tras_mover")
			var gastados: int = pa_antes - int(pm.get_node(NodePath(movido)).get_meta("pa_actual"))
			check(gastados > 0, "moverse gasta PA (%d)" % gastados)
			check(pos_activa() != pos_antes or activo() == null, "la posición ha cambiado")
		4:
			tecla(KEY_E, true)
			espera = 2
		5:
			tecla(KEY_E, false)
			espera = 40
		6:
			captura("03_camara_rotada")
			var p := pos_activa()
			pos_antes = p
			dm.call("ProcesarClicEnCasilla", p.x, p.y)
			espera = 10
		7:
			captura("04_menu_tactico")
			check(ui.get_node_or_null("MenuTactico_Instancia") != null, "el menú táctico se abre")
			dm.call("ProcesarClicEnCasilla", pos_antes.x + 1, pos_antes.y)
			espera = 5
		8:
			check(ui.get_node_or_null("MenuTactico_Instancia") == null, "el clic fuera cierra el menú")
			check(not dm.get_meta("is_moving", false) and pos_activa() == pos_antes, "y no provoca movimiento")
			dm.call("ToggleModoDebug")
			espera = 10
		9:
			captura("05_debug_mapa_completo")
			var cofres: Array = dm.get_meta("lista_cofres", [])
			check(cofres.size() > 0, "hay cofres (%d)" % cofres.size())
			if cofres.size() > 0:
				var c: Vector2i = cofres[0]
				check(junto_a(c), "teletransporte junto al cofre %s" % c)
				dm.call("ProcesarClicEnCasilla", c.x, c.y)
			espera = 30
		10:
			captura("06_cofre_abierto")
			var enemigos: Array = dm.get_meta("lista_enemigos", [])
			check(enemigos.size() > 0, "hay enemigos (%d)" % enemigos.size())
			if enemigos.size() > 0:
				var e: Vector2i = enemigos[0]
				check(junto_a(e), "teletransporte junto al enemigo %s" % e)
				var nodo := dm.get_node_or_null("Enemy_%d_%d" % [e.x, e.y])
				var hp0: int = nodo.get_meta("hp_actual") if nodo else -1
				dm.call("ProcesarClicEnCasilla", e.x, e.y)
				var hp1: int = nodo.get_meta("hp_actual") if is_instance_valid(nodo) else -999
				check(hp1 < hp0, "el ataque quita vida (%d -> %d)" % [hp0, hp1])
			espera = 20
		11:
			captura("07_combate")
			# Todos a la escalera
			var salida: Vector2i = dm.get_meta("casilla_salida")
			for i in 6:
				var a := activo()
				if a == null:
					break
				dm.call("TeletransporteDebug", salida.x, salida.y)
			espera = 30
		12:
			var fogata := root.get_node("Main/CanvasLayer3")
			check(fogata.visible, "al escapar todos aparece la fogata")
			captura("08_fogata")
			ui.call("DescansarYContinuar")
			espera = 60
		13:
			captura("09_piso_2")
			check(int(pm.get_meta("piso_actual")) == 2, "estamos en el piso 2")
			check(activo() != null, "hay personaje activo en el piso 2")
			var inicio: Vector2i = dm.get_meta("casilla_inicio")
			for h in pm.get_children():
				if h.name == "DatosEquipo":
					continue
				var gp: Vector2i = h.get_meta("grid_pos")
				check(abs(gp.x - inicio.x) + abs(gp.y - inicio.y) <= 3, "%s aparece junto al inicio %s (está en %s)" % [h.name, inicio, gp])
			dm.call("ToggleModoDebug")
			espera = 20
		14:
			captura("10_piso_2_niebla")
			# Picking: llevamos el ratón real a la casilla vecina del héroe y miramos dónde cae el cursor 3D
			var cam: Camera3D = dm.get_node("Camera3D")
			var cont: SubViewportContainer = root.get_node("Main/ContenedorMapa")
			var alc: PackedInt32Array = dm.call("DebugAlcance")
			if alc.size() >= 2:
				objetivo_pick = Vector2i(alc[0], alc[1])
				var p_sub := cam.unproject_position(Vector3(objetivo_pick.x + 0.5, 0, objetivo_pick.y + 0.5))
				var p_canvas := cont.global_position + p_sub * cont.stretch_shrink
				var p_ventana: Vector2 = root.get_final_transform() * p_canvas
				Input.warp_mouse(p_ventana)
			espera = 10
		15:
			var cursor: Node3D = dm.get_node("DungeonRenderer/Cursor")
			var celda := Vector2i(floori(cursor.position.x), floori(cursor.position.z))
			check(cursor.visible and celda == objetivo_pick, "el ratón sobre %s marca la casilla %s" % [objetivo_pick, celda])
			captura("11_hover")
			for i in 8:
				var ev := InputEventMouseButton.new()
				ev.button_index = MOUSE_BUTTON_WHEEL_DOWN
				ev.pressed = true
				Input.parse_input_event(ev)
			paso_zoom = 0
		16:
			# Alejamos la cámara (una rueda por frame) para ver la niebla de guerra entera
			paso_zoom += 1
			var ev := InputEventMouseButton.new()
			ev.button_index = MOUSE_BUTTON_WHEEL_DOWN
			ev.pressed = paso_zoom % 2 == 1
			Input.parse_input_event(ev)
			if paso_zoom < 24:
				paso -= 1
			else:
				espera = 40
		17:
			captura("12_niebla_lejos")
			# Evento: lanzamos una trampa directamente sobre el héroe activo
			var p := pos_activa()
			dm.get_node("EventManager").call("Interactuar", p.x, p.y, "TRAMPA", "res://Data/Events/Roca.tres")
			check(dm.get_meta("bloqueo_evento", false), "el evento bloquea el tablero")
			espera = 400
		18:
			var pantalla: Control = root.get_node("Main/CanvasLayer2/PantallaEvento")
			check(pantalla.visible, "la pantalla de evento aparece")
			captura("13_evento")
			ui.call("ResolverOpcion", 0)
			espera = 900
		19:
			captura("14_evento_resuelto")
			ui.call("CerrarEvento")
			espera = 40
		20:
			check(not dm.get_meta("bloqueo_evento", false), "al cerrar el evento el tablero se desbloquea")
			print("[test] RESULTADO: ", "TODO OK" if fallos.is_empty() else "%d FALLOS: %s" % [fallos.size(), fallos])
			quit(0 if fallos.is_empty() else 1)
	return false
