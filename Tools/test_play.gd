extends SceneTree
# Test de humo jugable: arranca main.tscn, simula una partida y guarda capturas.
#   godot --path . -s res://Tools/test_play.gd -- <carpeta_capturas>
# (sin --headless para que haya render y las capturas salgan)

var out_dir := "user://test_shots"
var dm: Node
var pm: Node
var ui: Node
var fallos: Array[String] = []
var pasos: Array[Callable] = []
var indice := -1
var espera_hasta := 0.0
var memo := {}

func _initialize():
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = args[0]
	DirAccess.make_dir_recursive_absolute(out_dir)
	change_scene_to_file("res://Scenes/main.tscn")
	_definir_pasos()

# ---------- utilidades ----------

func ahora() -> float:
	return Time.get_ticks_msec() / 1000.0

func captura(nombre: String) -> void:
	root.get_texture().get_image().save_png(out_dir.path_join(nombre + ".png"))
	print("[test] captura: ", nombre)

func check(cond: bool, msg: String) -> void:
	print("[test] ", "OK   " if cond else "FAIL ", msg)
	if not cond:
		fallos.append(msg)

func activo() -> Node:
	return pm.call("GetActiveCharacter")

func pos_activa() -> Vector2i:
	return pm.call("GetActiveGridPos")

func ocupado() -> bool:
	return dm.get_meta("is_moving", false) or dm.get_meta("bloqueo_evento", false)

func tecla(code: Key, pulsada: bool) -> void:
	var ev := InputEventKey.new()
	ev.physical_keycode = code
	ev.keycode = code
	ev.pressed = pulsada
	Input.parse_input_event(ev)

func rueda(abajo: bool) -> void:
	for pulsada in [true, false]:
		var ev := InputEventMouseButton.new()
		ev.button_index = MOUSE_BUTTON_WHEEL_DOWN if abajo else MOUSE_BUTTON_WHEEL_UP
		ev.pressed = pulsada
		Input.parse_input_event(ev)

func raiz2d() -> Node:
	return dm.get_node_or_null("DungeonRenderer/Capa2D/Raiz")

func bocadillos(canal: int) -> Array:
	var out := []
	var r := raiz2d()
	if r:
		for h in r.get_children():
			if str(h.name).begins_with("Bocadillo%d_" % canal):
				out.append(h)
	return out

func ficha_visible(nombre: String) -> bool:
	var f = ui.get_node_or_null("PanelFichas/" + nombre)
	return f != null and f.visible

# Teletransporte (debug) a una casilla libre junto a 'objetivo'
func junto_a(objetivo: Vector2i) -> bool:
	for d in [Vector2i(0, 1), Vector2i(1, 0), Vector2i(0, -1), Vector2i(-1, 0)]:
		dm.call("TeletransporteDebug", objetivo.x + d.x, objetivo.y + d.y)
		if pos_activa() == objetivo + d:
			return true
	return false

func raton_a_celda(c: Vector2i) -> void:
	var cam: Camera3D = dm.get_node("Camera3D")
	var cont: SubViewportContainer = root.get_node("Main/ContenedorMapa")
	var p_sub := cam.unproject_position(Vector3(c.x + 0.5, 0, c.y + 0.5))
	var p_canvas := cont.global_position + p_sub * cont.stretch_shrink
	Input.warp_mouse(root.get_final_transform() * p_canvas)

# La cámara está anclada al héroe activo, sin desplazamiento lateral
func _check_camara_centrada(cuando: String) -> void:
	var cam: Camera3D = dm.get_node("Camera3D")
	var token: Node3D = dm.get_node_or_null(str(activo().name)) if activo() else null
	if not token:
		return
	var piv: Vector3 = cam.get_meta("pivote")
	var d := Vector2(piv.x, piv.z).distance_to(Vector2(token.position.x, token.position.z))
	check(d < 0.25 and absf(cam.h_offset) < 0.3, "%s la cámara está centrada en el héroe (a %.2f, h_offset %.2f)" % [cuando, d, cam.h_offset])

# Lo que tiene un héroe (para ver si un cofre le ha dado algo)
func _riqueza(h: Node) -> String:
	var objetos := 0
	for e in h.get_meta("inventario", []):
		objetos += int(e["cantidad"])
	return "%d oro, %d comida, %d objetos" % [h.get_meta("oro", 0), h.get_meta("comida_bolsa", 0), objetos]

func _indice_de(heroe: String, id: String) -> int:
	for e in pm.call("DescribirInventario", heroe):
		if e["id"] == id:
			return int(e["indice"])
	return -1

func _heroes() -> Array:
	var out: Array = []
	for h in pm.get_children():
		if h.name != "DatosEquipo":
			out.append(h)
	return out

# Inventario, equipo, peso, consumibles e intercambio (deja a los héroes como estaban)
func _check_inventario() -> void:
	var cat := root.get_node("Catalogo")
	check(cat.objetos.size() >= 15, "el catálogo carga los objetos de Data/Objetos (%d)" % cat.objetos.size())
	check(cat.avisos.is_empty(), "el catálogo no da avisos %s" % [cat.avisos])
	var guardado := {}
	for h in _heroes():
		guardado[h] = [h.get_meta("inventario").duplicate(true), h.get_meta("equipo").duplicate(true), int(h.get_meta("hp_actual"))]

	var h := activo()
	var n := str(h.name)
	var eq: Dictionary = h.get_meta("equipo", {})
	check(eq.has("mano_principal") and eq.has("mochila"), "%s empieza con arma y mochila (%s, %s)" % [n, eq.get("mano_principal", "-"), eq.get("mochila", "-")])
	var base: Dictionary = h.get_meta("stats_base")
	var bono := 0
	for r in eq:
		var d = cat.obtener(eq[r])
		if d is EquipoData:
			for m in d.modificadores:
				if m.stat == "defensa":
					bono += m.plano
	check(int(h.get_meta("defensa")) == int(base["defensa"]) + bono, "la defensa final suma el equipo (%d + %d = %d)" % [base["defensa"], bono, h.get_meta("defensa")])

	# Apilado y límite de peso
	var inv0: Array = pm.call("DescribirInventario", n)
	var entran: int = pm.call("AnadirObjeto", n, "pocion_vida", 2)
	var inv1: Array = pm.call("DescribirInventario", n)
	check(entran == 2 and inv1.size() == inv0.size(), "las pociones se apilan (%d entradas -> %d)" % [inv0.size(), inv1.size()])
	var cap: float = pm.call("CapacidadInventario", n)
	var caben: int = pm.call("AnadirObjeto", n, "cota_malla", 10)
	var peso: float = pm.call("PesoInventario", n)
	check(caben < 10 and peso <= cap + 0.01, "la mochila tiene límite: caben %d cotas (%.1f / %.1f kg)" % [caben, peso, cap])
	h.set_meta("inventario", guardado[h][0].duplicate(true))

	# Antorcha en la ranura de luz: +1 de radio y se puede quitar
	var radio0 := int(h.get_meta("radio_luz"))
	pm.call("AnadirObjeto", n, "antorcha", 1)
	var err: String = pm.call("Equipar", n, _indice_de(n, "antorcha"))
	check(err == "" and int(h.get_meta("radio_luz")) == radio0 + 1, "equipar la antorcha da +1 de luz (%d -> %d) '%s'" % [radio0, h.get_meta("radio_luz"), err])
	err = pm.call("Desequipar", n, "luz")
	check(err == "" and int(h.get_meta("radio_luz")) == radio0, "al quitarla vuelve la luz de antes (%d)" % h.get_meta("radio_luz"))

	# Restricción de clase: un mago no se pone la cota de malla
	for o in _heroes():
		if o.get_meta("clase") == "Mago":
			pm.call("AnadirObjeto", str(o.name), "cota_malla", 1)
			err = pm.call("Equipar", str(o.name), _indice_de(str(o.name), "cota_malla"))
			check(err.contains("clase"), "el mago no puede ponerse la cota de malla ('%s')" % err)

	# Poción: cura y se gasta
	h.set_meta("hp_actual", int(h.get_meta("hp_max")) - 30)
	var pociones := 0
	for e in pm.call("DescribirInventario", n):
		if e["id"] == "pocion_vida":
			pociones += int(e["cantidad"])
	err = pm.call("UsarObjeto", n, _indice_de(n, "pocion_vida"))
	var quedan := 0
	for e in pm.call("DescribirInventario", n):
		if e["id"] == "pocion_vida":
			quedan += int(e["cantidad"])
	check(err == "" and int(h.get_meta("hp_actual")) == int(h.get_meta("hp_max")) - 5 and quedan == pociones - 1, "la poción cura 25 PV y se gasta (%d -> %d pociones)" % [pociones, quedan])

	# Dar objetos: solo pegados (fuera de la fogata)
	var pegado: Node = null
	var lejos: Node = null
	var pos: Vector2i = h.get_meta("grid_pos")
	for o in _heroes():
		if o == h:
			continue
		var d := absi(Vector2i(o.get_meta("grid_pos")).x - pos.x) + absi(Vector2i(o.get_meta("grid_pos")).y - pos.y)
		if d == 1 and pegado == null:
			pegado = o
		elif d > 1 and lejos == null:
			lejos = o
	if lejos:
		err = pm.call("DarObjeto", n, 0, str(lejos.name), 1)
		check(err != "", "no se puede dar nada a %s si está lejos ('%s')" % [lejos.name, err])
	if pegado:
		var antes := int(pm.call("PesoInventario", str(pegado.name)))
		err = pm.call("DarObjeto", n, 0, str(pegado.name), 1)
		check(err == "" and float(pm.call("PesoInventario", str(pegado.name))) > antes - 0.01, "%s le da un objeto a %s, que está pegado ('%s')" % [n, pegado.name, err])

	# Todo como estaba
	for o in guardado:
		o.set_meta("inventario", guardado[o][0])
		o.set_meta("equipo", guardado[o][1])
		o.set_meta("hp_actual", guardado[o][2])
	ui.call("UpdateWarriorUI")
	captura("01b_ficha_con_equipo")

# Equipa un arco al héroe activo y lo coloca a 2-3 casillas en línea recta de un enemigo
func _preparar_arco() -> void:
	var h := activo()
	if h == null:
		return
	var n := str(h.name)
	pm.call("AnadirObjeto", n, "arco_corto", 1)
	var err: String = pm.call("Equipar", n, _indice_de(n, "arco_corto"))
	check(err == "", "%s se equipa el arco ('%s')" % [n, err])
	h.set_meta("pa_actual", maxi(int(h.get_meta("pa_actual")), 4))
	for e in dm.get_meta("lista_enemigos", []):
		for d in [Vector2i(0, 1), Vector2i(1, 0), Vector2i(0, -1), Vector2i(-1, 0)]:
			for k in [2, 3]:
				var c: Vector2i = e + d * k
				dm.call("TeletransporteDebug", c.x, c.y)
				dm.call("CalcularMapaDeLuces")
				if pos_activa() == c and bool(dm.call("PuedeAtacarA", e.x, e.y)):
					memo["objetivo_arco"] = e
					return
	print("[test] AVISO no hay sitio para probar el arco")

# ---------- guion ----------
# Cada paso devuelve los segundos a esperar antes del siguiente.

func _definir_pasos():
	pasos = [
		func():
			captura("01_inicio")
			check(activo() != null, "hay personaje activo al empezar")
			var p := pos_activa()
			check(p.x > 0 and p.y > 0, "el héroe activo está en una casilla válida %s" % p)
			var alc: PackedInt32Array = dm.call("DebugAlcance")
			check(alc.size() > 0, "hay casillas alcanzables (%d)" % (alc.size() / 2))
			check(ficha_visible(str(activo().name)), "la ficha del héroe activo se ve")
			var mm: MultiMesh = dm.get_node("DungeonRenderer/Alcance").multimesh
			check(mm.instance_count > 0, "el overlay de alcance tiene casillas (%d)" % mm.instance_count)
			return 0.2,
		func():
			_check_inventario()
			# Panel de depuración (F10)
			var dbg: CanvasLayer = null
			for c in root.get_node("Main").get_children():
				if c.get_script() and str(c.get_script().resource_path).ends_with("debug_inventario.gd"):
					dbg = c
			memo["debug"] = dbg
			check(dbg != null, "existe el panel de depuración de inventario")
			if dbg:
				dbg.alternar()
				dbg.heroe = str(activo().name)
				for i in dbg.opt_tabla.item_count:
					if dbg.opt_tabla.get_item_text(i) == "enemigo":
						dbg.opt_tabla.select(i)
				dbg._on_botin()
				check(dbg.lbl_log.text.begins_with("Tabla 'enemigo'"), "el panel tira una tabla de botín: %s" % dbg.lbl_log.text)
			return 0.4,
		func():
			var dbg = memo.get("debug")
			if dbg:
				check(dbg.visible and dbg.caja_equipo.get_child_count() == 8, "F10 abre el panel con las 8 ranuras de equipo")
				check(dbg.caja_inv.get_child_count() > 0, "el panel lista el inventario (%d filas)" % dbg.caja_inv.get_child_count())
				captura("01c_panel_debug")
				dbg.alternar()
			return 0.2,
		func():
			# Hover sobre la casilla alcanzable más lejana: debe salir la ruta y el coste
			var alc: PackedInt32Array = dm.call("DebugAlcance")
			var p := pos_activa()
			var mejor := p
			for i in range(0, alc.size(), 2):
				var c := Vector2i(alc[i], alc[i + 1])
				if abs(c.x - p.x) + abs(c.y - p.y) > abs(mejor.x - p.x) + abs(mejor.y - p.y):
					mejor = c
			memo["destino"] = mejor
			# El ratón real (si alguien lo mueve) no debe pisar el hover simulado
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_DISABLED
			dm.call("ActualizarHover", mejor.x, mejor.y)
			return 0.3,
		func():
			var coste: Label = dm.get_node("DungeonRenderer/Capa2D/Raiz/Coste")
			var ruta: MultiMesh = dm.get_node("DungeonRenderer/Ruta").multimesh
			check(ruta.instance_count > 0, "al pasar el ratón se dibuja la ruta (%d pasos)" % ruta.instance_count)
			check(coste.visible and coste.text.ends_with("PA"), "y aparece el coste junto al cursor ('%s')" % coste.text)
			captura("02_hover_ruta")
			var mejor: Vector2i = memo["destino"]
			memo["pa_antes"] = int(activo().get_meta("pa_actual"))
			memo["movido"] = str(activo().name)
			memo["pos_antes"] = pos_activa()
			dm.call("ProcesarClicEnCasilla", mejor.x, mejor.y)
			check(dm.get_meta("is_moving", false), "empieza a moverse")
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_INHERIT
			return 2.0,
		func():
			captura("03_tras_mover")
			var gastados: int = memo["pa_antes"] - int(pm.get_node(NodePath(memo["movido"])).get_meta("pa_actual"))
			check(gastados > 0, "moverse gasta PA (%d)" % gastados)
			_check_camara_centrada("tras caminar")
			# Vistazo (lo que hace el minimapa o el arrastre): la cámara se va a otro punto...
			var cam: Camera3D = dm.get_node("Camera3D")
			var p := pos_activa()
			memo["vistazo"] = Vector3(clampi(p.x + 8, 1, 33) + 0.5, 0, clampi(p.y + 8, 1, 33) + 0.5)
			cam.set_meta("vistazo", memo["vistazo"])
			return 1.2,
		func():
			var cam: Camera3D = dm.get_node("Camera3D")
			var piv: Vector3 = cam.get_meta("pivote")
			check(piv.distance_to(memo["vistazo"]) < 1.0, "durante el vistazo la cámara va al punto (a %.2f)" % piv.distance_to(memo["vistazo"]))
			captura("03b_vistazo")
			# ...y al soltar vuelve sola al héroe
			cam.remove_meta("vistazo")
			return 2.0,
		func():
			_check_camara_centrada("al soltar el vistazo")
			tecla(KEY_E, true)
			return 0.05,
		func():
			tecla(KEY_E, false)
			return 0.8,
		func():
			_check_camara_centrada("tras rotar con E")
			captura("04_camara_rotada")
			var p := pos_activa()
			memo["pos_antes"] = p
			dm.call("ProcesarClicEnCasilla", p.x, p.y)
			return 0.2,
		func():
			captura("05_menu_tactico")
			var menu = ui.get_node_or_null("MenuTactico_Instancia")
			check(menu != null, "el menú táctico se abre")
			if menu:
				check(menu.filas.size() == 3, "el menú tiene sus tres acciones")
				check(menu.filas["habilidad"].disabled, "habilidades aparece deshabilitado")
			var p: Vector2i = memo["pos_antes"]
			dm.call("ProcesarClicEnCasilla", p.x + 1, p.y)
			return 0.2,
		func():
			check(ui.get_node_or_null("MenuTactico_Instancia") == null, "el clic fuera cierra el menú")
			check(not dm.get_meta("is_moving", false) and pos_activa() == memo["pos_antes"], "y no provoca movimiento")
			# Clic en otro héroe: pasa a ser el activo y su ficha queda fijada
			var otro: Node = null
			for h in pm.get_children():
				if h.name != "DatosEquipo" and h != activo() and not h.get_meta("ha_escapado") and int(h.get_meta("pa_actual")) > 0:
					otro = h
					break
			if otro:
				var gp: Vector2i = otro.get_meta("grid_pos")
				dm.call("ProcesarClicEnCasilla", gp.x, gp.y)
				check(activo() == otro, "clicar a %s en el mapa lo selecciona" % otro.name)
			return 0.3,
		func():
			# Modo OCULTAS: solo queda la ficha fijada
			ui.call("AlternarModoFichas")
			var visibles := 0
			for f in ui.get_node("PanelFichas").get_children():
				if f.visible:
					visibles += 1
			check(visibles == 0, "en modo OCULTAS no se ve ninguna ficha (%d)" % visibles)
			ui.call("FijarFicha", str(activo().name))
			return 0.3,
		func():
			check(ficha_visible(str(activo().name)), "una ficha fijada se ve en modo OCULTAS")
			captura("06_fichas_ocultas")
			ui.call("AlternarModoFichas")
			dm.call("ToggleModoDebug")
			return 0.3,
		func():
			captura("07_debug_mapa_completo")
			var cofres: Array = dm.get_meta("lista_cofres", [])
			check(cofres.size() > 0, "hay cofres (%d)" % cofres.size())
			if cofres.size() > 0:
				memo["cofre"] = cofres[0]
				check(junto_a(cofres[0]), "teletransporte junto al cofre %s" % cofres[0])
			return 0.6,
		func():
			var b := bocadillos(0)
			check(b.size() > 0, "junto al cofre aparece un bocadillo")
			if b.size() > 0:
				check(str(b[0].get_meta("texto")).contains("PA"), "el bocadillo indica el coste: '%s'" % str(b[0].get_meta("texto")).replace("\n", " | "))
			captura("08_bocadillo_cofre")
			var c: Vector2i = memo.get("cofre", Vector2i(-1, -1))
			memo["riqueza_cofre"] = _riqueza(activo())
			memo["heroe_cofre"] = activo()
			dm.call("ProcesarClicEnCasilla", c.x, c.y)
			return 0.6,
		func():
			var h: Node = memo["heroe_cofre"]
			check(_riqueza(h) != memo["riqueza_cofre"], "el botín del cofre llega a %s (%s -> %s)" % [h.name, memo["riqueza_cofre"], _riqueza(h)])
			captura("09_cofre_abierto")
			# Puerta: nos ponemos al lado y la abrimos
			var puertas: Array = dm.get_meta("lista_puertas", [])
			memo["puerta"] = null
			if puertas.size() > 0 and junto_a(puertas[0]):
				memo["puerta"] = puertas[0]
				dm.call("ProcesarClicEnCasilla", puertas[0].x, puertas[0].y)
			else:
				print("[test] AVISO este piso no tiene puertas accesibles")
			# Braseros: ocupan su casilla
			var braseros: Array = dm.get_meta("lista_braseros", [])
			var alc: PackedInt32Array = dm.call("DebugAlcance")
			var pisable := false
			for i in range(0, alc.size(), 2):
				if Vector2i(alc[i], alc[i + 1]) in braseros:
					pisable = true
			check(not pisable, "los braseros (%d) no se pueden pisar" % braseros.size())
			return 1.0,
		func():
			if memo["puerta"] != null:
				var p: Vector2i = memo["puerta"]
				var bisagra: Node3D = dm.get_node_or_null("DungeonRenderer/Props/Puerta_%d_%d/Bisagra" % [p.x, p.y])
				check(bisagra != null and abs(bisagra.rotation.y) > 1.0, "la puerta %s gira al abrirse" % p)
				captura("09b_puerta_abierta")
			var braseros: Array = dm.get_meta("lista_braseros", [])
			if braseros.size() > 0:
				junto_a(braseros[0])
			return 0.8,
		func():
			if dm.get_meta("lista_braseros", []).size() > 0:
				captura("09c_brasero")
			var enemigos: Array = dm.get_meta("lista_enemigos", [])
			check(enemigos.size() > 0, "hay enemigos (%d)" % enemigos.size())
			if enemigos.size() > 0:
				var e: Vector2i = enemigos[0]
				memo["enemigo"] = e
				check(junto_a(e), "teletransporte junto al enemigo %s" % e)
			return 0.6,
		func():
			var e: Vector2i = memo.get("enemigo", Vector2i(-1, -1))
			var b := bocadillos(0)
			var hay_ataque := false
			for x in b:
				if str(x.get_meta("texto")).begins_with("Atacar"):
					hay_ataque = true
			check(hay_ataque, "junto al enemigo el bocadillo ofrece atacar")
			check(ficha_visible("Enemigo0"), "la ficha del enemigo en pantalla se ve")
			# Tarjeta de previsión al pasar sobre el enemigo
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_DISABLED
			dm.call("ActualizarHover", e.x, e.y)
			return 0.2,
		func():
			var e: Vector2i = memo.get("enemigo", Vector2i(-1, -1))
			var prev: Control = ui.get_node("Prevision")
			check(prev.visible, "al pasar sobre el enemigo sale la tarjeta de previsión")
			check(int(dm.call("TipoCursor", e.x, e.y)) == 2, "el cursor sobre un enemigo es la espada")
			var enemigo_n := dm.get_node_or_null("Enemy_%d_%d" % [e.x, e.y])
			var prevision: Dictionary = dm.get_node("CombatManager").call("PrevisionAtaque", e.x, e.y)
			var arma = root.get_node("Catalogo").obtener(activo().get_meta("equipo").get("mano_principal", ""))
			if arma and enemigo_n:
				var esperado := maxi(1, arma.dano + int(activo().get_meta(arma.stat_ataque)) - int(enemigo_n.get_meta("defensa")))
				var coste_esperado: int = 0 if dm.get_meta("debug_mode", false) else arma.coste_pa
				check(int(prevision["dano"]) == esperado and int(prevision["coste"]) == coste_esperado, "la previsión usa el arma (%s: %d de daño, %d PA)" % [arma.nombre, prevision["dano"], prevision["coste"]])
			captura("10_bocadillo_enemigo")
			var nodo := dm.get_node_or_null("Enemy_%d_%d" % [e.x, e.y])
			memo["hp0"] = int(nodo.get_meta("hp_actual")) if nodo else -1
			memo["nodo"] = nodo
			dm.call("ProcesarClicEnCasilla", e.x, e.y)
			check(dm.get_meta("is_moving", false), "el ataque es una secuencia animada (tablero bloqueado)")
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_INHERIT
			return 0.12,
		func():
			captura("10b_impacto")
			return 1.4,
		func():
			var nodo = memo.get("nodo")
			var hp1: int = nodo.get_meta("hp_actual") if is_instance_valid(nodo) and not str(nodo.name).ends_with("_muerto") else -999
			check(hp1 < int(memo["hp0"]), "el ataque quita vida (%d -> %d)" % [memo["hp0"], hp1])
			check(not dm.get_meta("is_moving", false), "al acabar la secuencia el tablero se desbloquea")
			captura("11_combate")
			_preparar_arco()
			return 0.8,
		func():
			var e: Vector2i = memo.get("objetivo_arco", Vector2i(-1, -1))
			if e.x >= 0:
				check(bool(dm.call("PuedeAtacarA", e.x, e.y)), "con el arco se puede disparar a %d casillas" % (absi(pos_activa().x - e.x) + absi(pos_activa().y - e.y)))
				var nodo := dm.get_node_or_null("Enemy_%d_%d" % [e.x, e.y])
				memo["hp_e_arco"] = int(nodo.get_meta("hp_actual")) if nodo else -1
				memo["nodo_arco"] = nodo
				memo["arquero"] = activo()
				memo["hp_h_arco"] = int(activo().get_meta("hp_actual"))
				dm.call("ProcesarClicEnCasilla", e.x, e.y)
			return 1.6,
		func():
			if memo.has("nodo_arco"):
				var nodo = memo["nodo_arco"]
				var hp1: int = nodo.get_meta("hp_actual") if is_instance_valid(nodo) and not str(nodo.name).ends_with("_muerto") else -999
				check(hp1 < int(memo["hp_e_arco"]), "el disparo quita vida (%d -> %d)" % [memo["hp_e_arco"], hp1])
				var h: Node = memo["arquero"]
				check(int(h.get_meta("hp_actual")) == int(memo["hp_h_arco"]), "a distancia no hay contraataque (%s sigue con %d PV)" % [h.name, h.get_meta("hp_actual")])
				captura("11b_disparo")
			var salida: Vector2i = dm.get_meta("casilla_salida")
			for i in 6:
				if activo() == null:
					break
				dm.call("TeletransporteDebug", salida.x, salida.y)
			return 1.2,
		func():
			check(root.get_node("Main/CanvasLayer3").visible, "al escapar todos aparece la fogata")
			var sentados := 0
			for c in root.get_node("Main/CanvasLayer3/PantallaFogata").get_children():
				if c is TextureRect and c.has_meta("frame"):
					sentados += 1
			check(sentados > 0, "en la fogata se sienta el grupo (%d)" % sentados)
			var vivos: Array = []
			for h in pm.get_children():
				if h.name != "DatosEquipo" and not h.get_meta("esta_muerto", false):
					vivos.append(h)
			if vivos.size() >= 2:
				var a: String = str(vivos[0].name)
				var b: String = str(vivos[1].name)
				var inv_a: Array = pm.call("DescribirInventario", a)
				if inv_a.size() > 0:
					var err: String = pm.call("DarObjeto", a, 0, b, 1)
					check(err == "", "en la fogata %s le da %s a %s ('%s')" % [a, inv_a[0]["nombre"], b, err])
			captura("12_fogata")
			ui.call("DescansarYContinuar")
			return 1.0,
		func():
			var negro: ColorRect = root.get_node("Main/CapaTransicion/Negro")
			var rotulo: Label = root.get_node("Main/CapaTransicion/Rotulo")
			check(negro.color.a > 0.9 and rotulo.text == "PISO 2", "la transición funde a negro con el rótulo '%s'" % rotulo.text)
			captura("12b_transicion")
			return 2.4,
		func():
			captura("13_piso_2")
			check(root.get_node("Main/CapaTransicion/Negro").color.a < 0.05, "la transición termina")
			check(int(pm.get_meta("piso_actual")) == 2, "estamos en el piso 2")
			check(activo() != null, "hay personaje activo en el piso 2")
			var inicio: Vector2i = dm.get_meta("casilla_inicio")
			for h in pm.get_children():
				if h.name == "DatosEquipo":
					continue
				var gp: Vector2i = h.get_meta("grid_pos")
				check(abs(gp.x - inicio.x) + abs(gp.y - inicio.y) <= 3, "%s aparece junto al inicio %s (está en %s)" % [h.name, inicio, gp])
			dm.call("ToggleModoDebug")
			return 3.0, # la cámara se desliza hasta el nuevo héroe activo
		func():
			# Picking: proyectamos una casilla a píxeles del SubViewport y lanzamos el mismo rayo
			# contra el suelo que usa el InputManager (con h_offset incluido); debe volver a la misma casilla
			var cam: Camera3D = dm.get_node("Camera3D")
			var alc: PackedInt32Array = dm.call("DebugAlcance")
			var ok := alc.size() >= 2
			for i in range(0, alc.size(), 2):
				var c := Vector2i(alc[i], alc[i + 1])
				var p_sub := cam.unproject_position(Vector3(c.x + 0.5, 0, c.y + 0.5))
				var o := cam.project_ray_origin(p_sub)
				var d := cam.project_ray_normal(p_sub)
				var hit := o + d * (-o.y / d.y)
				if Vector2i(floori(hit.x), floori(hit.z)) != c:
					ok = false
			check(ok, "el rayo de picking devuelve la casilla correcta en todo el alcance (%d casillas)" % (alc.size() / 2))
			if alc.size() >= 2:
				dm.call("ActualizarHover", alc[0], alc[1])
			return 0.3,
		func():
			captura("14_hover_piso2")
			memo["minimapa_antes"] = (ui.get_node("Minimapa/Mascara/Mapa") as Control).size.x
			for i in 8:
				rueda(true)
			return 0.1,
		func():
			for i in 8:
				rueda(true)
			return 1.0,
		func():
			var ahora_mm: float = (ui.get_node("Minimapa/Mascara/Mapa") as Control).size.x
			check(ahora_mm < float(memo["minimapa_antes"]) - 1.0, "al alejar la cámara el minimapa también se aleja (%d -> %d px)" % [memo["minimapa_antes"], ahora_mm])
			captura("15_niebla_lejos")
			var p := pos_activa()
			dm.get_node("EventManager").call("Interactuar", p.x, p.y, "TRAMPA", "res://Data/Events/Roca.tres")
			check(dm.get_meta("bloqueo_evento", false), "el evento bloquea el tablero")
			check(bocadillos(0).is_empty(), "durante el evento no hay bocadillos")
			return 2.0,
		func():
			check(root.get_node("Main/CanvasLayer2/PantallaEvento").visible, "la pantalla de evento aparece")
			captura("16_evento")
			ui.call("ResolverOpcion", 0)
			memo["dado_t"] = ahora()
			return 0.6,
		func():
			check(root.get_node("Main/CanvasLayer2/PantallaEvento/Dado").visible, "durante la tirada se ve el dado 3D")
			captura("16b_dado_rodando")
			return 4.0,
		func():
			captura("17_evento_resuelto")
			ui.call("CerrarEvento")
			return 0.8,
		func():
			check(not dm.get_meta("bloqueo_evento", false), "al cerrar el evento el tablero se desbloquea")
			# Minimapa: algo explorado tiene que estar pintado
			var mapa: TextureRect = ui.get_node("Minimapa/Mascara/Mapa")
			var img := mapa.texture.get_image()
			var pintados := 0
			for x in img.get_width():
				for y in img.get_height():
					if img.get_pixel(x, y).a > 0.5:
						pintados += 1
			check(pintados > 10, "el minimapa muestra lo explorado (%d casillas)" % pintados)
			# Opciones visuales del menú de pausa: PS1 baja + CRT
			var menu := root.get_node("Main/CanvasMenuPausa")
			memo["ajustes"] = menu.ajustes.duplicate()
			menu.ajustes["resolucion"] = 3
			menu.ajustes["crt"] = true
			menu.ajustes["jitter"] = 3.0
			menu.call("_aplicar_ajustes")
			var cont: SubViewportContainer = root.get_node("Main/ContenedorMapa")
			check(cont.stretch_shrink == 3, "la opción de resolución cambia el viewport")
			check(cont.material.get_shader_parameter("crt") == true, "la opción CRT llega al shader")
			return 0.6,
		func():
			captura("18_opciones_ps1_crt")
			var menu := root.get_node("Main/CanvasMenuPausa")
			menu.ajustes = memo["ajustes"]
			menu.call("_aplicar_ajustes")
			return 0.3,
	]

func _process(_delta):
	if dm == null:
		dm = root.get_node_or_null("Main/ContenedorMapa/SubViewport/DungeonManager")
		pm = root.get_node_or_null("Main/PartyManager")
		ui = root.get_node_or_null("Main/CanvasLayer")
		espera_hasta = ahora() + 2.2 # fundido de entrada "PISO 1"
		return false
	if ahora() < espera_hasta:
		return false
	# Los pasos de movimiento esperan a que termine el tween (los eventos se gestionan aparte)
	if dm.get_meta("is_moving", false):
		return false

	indice += 1
	if indice >= pasos.size():
		print("[test] RESULTADO: ", "TODO OK" if fallos.is_empty() else "%d FALLOS: %s" % [fallos.size(), fallos])
		quit(0 if fallos.is_empty() else 1)
		return false
	var segundos: float = pasos[indice].call()
	espera_hasta = ahora() + segundos
	return false
