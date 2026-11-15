extends SceneTree
# Playable smoke test: starts main.tscn, simulates a game and saves screenshots.
#   godot --path . -s res://Tools/test_play.gd -- <screenshots_folder>
# (without --headless so there's rendering and the screenshots come out)

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
	Engine.set_meta("tirada_forzada", 15) # Fixed d20: the script's attacks always hit (no critical)
	Engine.set_meta("test_sin_espera_banner", true) # The script doesn't wait for "Turno de X" to be read (except where it tests it)
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = args[0]
	DirAccess.make_dir_recursive_absolute(out_dir)
	change_scene_to_file("res://Scenes/main.tscn")
	_definir_pasos()

# ---------- utilities ----------

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

# Is their portrait on the HUD top bar? ("Enemigo0" = there's some enemy on the bar)
func ficha_visible(nombre: String) -> bool:
	var hud = ui.get_node_or_null("HUD")
	if hud == null:
		return false
	if nombre.begins_with("Enemigo"):
		for s in hud.slots_enemigo:
			if s.visible:
				return true
		return false
	return hud.slots.has(nombre) and hud.slots[nombre].visible

# Teleport (debug) to a free tile next to 'objetivo'
func junto_a(objetivo: Vector2i) -> bool:
	for d in [Vector2i(0, 1), Vector2i(1, 0), Vector2i(0, -1), Vector2i(-1, 0)]:
		if objetivo + d == dm.get_meta("casilla_salida", Vector2i(-1, -1)):
			continue # Stepping on the stairs takes the hero off the floor
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

# The camera is anchored to the active hero, with no side offset
func _check_camara_centrada(cuando: String) -> void:
	var cam: Camera3D = dm.get_node("Camera3D")
	var token: Node3D = dm.get_node_or_null(str(activo().name)) if activo() else null
	if not token:
		return
	var piv: Vector3 = cam.get_meta("pivote")
	var d := Vector2(piv.x, piv.z).distance_to(Vector2(token.position.x, token.position.z))
	check(d < 0.25 and absf(cam.h_offset) < 0.3, "%s la cámara está centrada en el héroe (a %.2f, h_offset %.2f)" % [cuando, d, cam.h_offset])

# What a hero has (to see whether a chest gave them something)
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

# Simulated mouse in canvas coordinates (press/release if 'boton', or just move)
func _raton(pos: Vector2, pulsado: bool, boton: bool) -> void:
	var ventana := root.get_final_transform() * pos
	if boton:
		var ev := InputEventMouseButton.new()
		ev.button_index = MOUSE_BUTTON_LEFT
		ev.pressed = pulsado
		ev.position = ventana
		ev.global_position = ventana
		Input.parse_input_event(ev)
	else:
		var m := InputEventMouseMotion.new()
		m.position = ventana
		m.global_position = ventana
		m.button_mask = MOUSE_BUTTON_MASK_LEFT
		m.relative = Vector2(8, 8)
		Input.parse_input_event(m)

func _nodo_con_script(archivo: String) -> Node:
	for c in root.get_node("Main").get_children():
		if c.get_script() and str(c.get_script().resource_path).ends_with(archivo):
			return c
	return null

func _heroes() -> Array:
	var out: Array = []
	for h in pm.get_children():
		if h.name != "DatosEquipo":
			out.append(h)
	return out

# Inventory, equipment, weight, consumables and swapping (leaves the heroes as they were)
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

	# Stacking and weight limit
	var inv0: Array = pm.call("DescribirInventario", n)
	var entran: int = pm.call("AnadirObjeto", n, "pocion_vida", 2)
	var inv1: Array = pm.call("DescribirInventario", n)
	check(entran == 2 and inv1.size() == inv0.size(), "las pociones se apilan (%d entradas -> %d)" % [inv0.size(), inv1.size()])
	var cap: float = pm.call("CapacidadInventario", n)
	var caben: int = pm.call("AnadirObjeto", n, "cota_malla", 10)
	var peso: float = pm.call("PesoInventario", n)
	check(caben < 10 and peso <= cap + 0.01, "la mochila tiene límite: caben %d cotas (%.1f / %.1f kg)" % [caben, peso, cap])
	h.set_meta("inventario", guardado[h][0].duplicate(true))

	# Torch in the light slot: +1 radius and it can be removed
	var radio0 := int(h.get_meta("radio_luz"))
	pm.call("AnadirObjeto", n, "antorcha", 1)
	var err: String = pm.call("Equipar", n, _indice_de(n, "antorcha"))
	check(err == "" and int(h.get_meta("radio_luz")) == radio0 + 1, "equipar la antorcha da +1 de luz (%d -> %d) '%s'" % [radio0, h.get_meta("radio_luz"), err])
	err = pm.call("Desequipar", n, "luz")
	check(err == "" and int(h.get_meta("radio_luz")) == radio0, "al quitarla vuelve la luz de antes (%d)" % h.get_meta("radio_luz"))

	# Class restriction: a mage can't wear the chainmail
	for o in _heroes():
		if o.get_meta("clase") == "Mago":
			pm.call("AnadirObjeto", str(o.name), "cota_malla", 1)
			err = pm.call("Equipar", str(o.name), _indice_de(str(o.name), "cota_malla"))
			check(err.contains("clase"), "el mago no puede ponerse la cota de malla ('%s')" % err)

	# Potion: heals and gets spent
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

	# Giving items: only when adjacent (outside the campfire)
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

	# Everything as it was
	for o in guardado:
		o.set_meta("inventario", guardado[o][0])
		o.set_meta("equipo", guardado[o][1])
		o.set_meta("hp_actual", guardado[o][2])
	ui.call("UpdateWarriorUI")
	captura("01b_ficha_con_equipo")

# Puts the active hero 3-4 tiles from an enemy (with line of sight) and wakes it up
func _xp_total(h: Node) -> int:
	var prog = root.get_node("Progreso")
	var total := int(h.get_meta("xp", 0))
	for n in range(1, int(h.get_meta("nivel", 1))):
		total += prog.xp_para(n)
	return total

# Three free tiles in a line (A, B, destination): the active one on A and another hero on B
func _preparar_cruce() -> void:
	var a := activo()
	var otro: Node = null
	for h in _heroes():
		if h != a and not h.get_meta("ha_escapado", false) and not h.get_meta("esta_muerto", false):
			otro = h
			break
	if otro == null:
		print("[test] AVISO no hay otro héroe para probar el cruce")
		return
	var libres: PackedByteArray = dm.call("ObtenerTransitables")
	var lado := int(Engine.get_meta("alturas_lado", 40))
	var libre := func(c: Vector2i) -> bool:
		return c.x > 0 and c.y > 0 and c.x < lado - 1 and c.y < lado - 1 and libres[c.x * lado + c.y] == 1
	for x in range(1, lado - 1):
		for y in range(1, lado - 1):
			for d in [Vector2i(1, 0), Vector2i(0, 1)]:
				var c0 := Vector2i(x, y)
				if libre.call(c0) and libre.call(c0 + d) and libre.call(c0 + d * 2) and not bool(dm.call("EsEventoManual", (c0 + d * 2).x, (c0 + d * 2).y)):
					otro.set_meta("pa_actual", maxi(1, int(otro.get_meta("pa_actual", 0)))) # Without PA they can't be picked
					pm.call("CambiarPersonajeActivo", str(otro.name))
					dm.call("TeletransporteDebug", (c0 + d).x, (c0 + d).y)
					pm.call("CambiarPersonajeActivo", str(a.name))
					dm.call("TeletransporteDebug", c0.x, c0.y)
					a.set_meta("pa_actual", 6)
					dm.call("CalcularMapaDeLuces")
					dm.call("RefrescarLuces")
					if pos_activa() == c0 and otro.get_meta("grid_pos") == c0 + d:
						memo["cruce"] = [c0, c0 + d, c0 + d * 2]
						memo["cruce_b"] = otro
						return
	print("[test] AVISO no hay tres casillas libres en línea para probar el cruce")

# An event investigated by clicking, with the hero 3 tiles away
func _preparar_evento_lejano() -> void:
	var libres: PackedByteArray = dm.call("ObtenerTransitables")
	var lado := int(Engine.get_meta("alturas_lado", 40))
	for ev in dm.get_meta("lista_eventos", []):
		if not bool(dm.call("EsEventoManual", ev.x, ev.y)):
			continue
		for d in [Vector2i(3, 0), Vector2i(-3, 0), Vector2i(0, 3), Vector2i(0, -3)]:
			var c: Vector2i = ev + d
			var paso: Vector2i = ev + d / 3
			if c.x <= 0 or c.y <= 0 or c.x >= lado - 1 or c.y >= lado - 1:
				continue
			if libres[c.x * lado + c.y] != 1 or libres[paso.x * lado + paso.y] != 1:
				continue
			dm.call("TeletransporteDebug", c.x, c.y)
			activo().set_meta("pa_actual", 6)
			dm.call("CalcularMapaDeLuces")
			dm.call("RefrescarLuces")
			if pos_activa() == c:
				memo["evento_lejano"] = ev
				return
	print("[test] AVISO no hay ningún evento manual con sitio para probar el acercamiento")

# If an event triggered while walking (proximity), close it so it doesn't block the rest of the
# script
func _cerrar_evento(donde: String) -> void:
	var pe = root.get_node_or_null("Main/CanvasLayer2/PantallaEvento")
	if pe == null:
		return
	if pe.visible or dm.get_meta("bloqueo_evento", false):
		print("[test] AVISO evento abierto %s (%s): se cierra" % [donde, pe.evento.resource_path if pe.evento else "?"])
		pe.cerrar_ya()
		dm.call("FinalizarEvento")

func _preparar_turno_enemigo() -> void:
	var h := activo()
	if h == null:
		return
	for pos in dm.get_meta("lista_enemigos", []):
		var e := dm.get_node_or_null("Enemy_%d_%d" % [pos.x, pos.y])
		if e == null:
			continue
		for d in [Vector2i(0, 1), Vector2i(1, 0), Vector2i(0, -1), Vector2i(-1, 0)]:
			for k in [3, 4]:
				var c: Vector2i = pos + d * k
				dm.call("TeletransporteDebug", c.x, c.y)
				if pos_activa() == c and bool(dm.call("HayLineaDeVision", c.x, c.y, pos.x, pos.y)):
					dm.call("CalcularMapaDeLuces")
					e.set_meta("in_combat", true)
					if e.has_meta("desobediente"):
						e.remove_meta("desobediente") # Set when rolling its attack (hit or miss)
					memo["ia_enemigo"] = e
					memo["ia_pos"] = pos
					memo["ia_heroe"] = h
					memo["ia_dist"] = k
					memo["ia_hp"] = int(h.get_meta("hp_actual"))
					memo["ia_nombre"] = str(e.get_meta("nombre_display"))
					return
	print("[test] AVISO no hay sitio para probar la IA")

# Equips a bow on the active hero and places them 2-3 tiles in a straight line from an enemy
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

# ---------- script ---------- Every step returns the seconds to wait before the next one.

# The floor structure (Zonas.gd + Mazmorra.hpp: AsignarZonas)
func _check_zonas() -> void:
	var zonas = root.get_node("Zonas")
	var lista: Array = dm.get_meta("zonas", [])
	var tipos := {}
	for z in lista:
		tipos[z["tipo"]] = int(tipos.get(z["tipo"], 0)) + 1
	check(tipos.get("entrada", 0) == 1 and tipos.get("jefe", 0) == 1 and tipos.get("campamento", 0) >= 2, "el piso tiene entrada, sala del jefe y campamentos %s" % tipos)
	var salida: Vector2i = dm.get_meta("casilla_salida")
	var sala_jefe: Dictionary = zonas.zonas("jefe")[0]
	check((sala_jefe["caja"] as Rect2i).has_point(salida), "la escalera está en la sala del jefe")
	# Campfire at the entrance
	var hoguera: Vector2i = dm.get_meta("casilla_hoguera", Vector2i(-1, -1))
	var entrada: Dictionary = zonas.zonas("entrada")[0]
	check(hoguera.x >= 0 and (entrada["caja"] as Rect2i).has_point(hoguera) and hoguera in dm.get_meta("lista_braseros", []), "hay una hoguera en la entrada %s" % hoguera)
	# Camps: their squad (3+, same id) around their chest
	var bien := true
	for z in zonas.zonas("campamento"):
		var suyos: Array = zonas.miembros(int(z["indice"]))
		var ids := {}
		for e in suyos:
			ids[e.get_meta("peloton")] = true
		if suyos.size() < 3 or ids.size() != 1 or not z["cofre"] in dm.get_meta("lista_cofres", []):
			bien = false
			print("[test] campamento ", z, " con ", suyos.size(), " enemigos")
	check(bien, "cada campamento tiene su pelotón (3+) y su cofre")
	# The boss: buffed, with the door locked until you have the key
	var jefe: Node = zonas.jefe()
	check(jefe != null and jefe.get_meta("jefe", false) and int(jefe.get_meta("hp_max", 0)) >= 20, "hay jefe reforzado (%s, %s PV)" % [jefe.get_meta("nombre_display", "") if jefe else "-", jefe.get_meta("hp_max", 0) if jefe else 0])
	var puertas: Array = dm.get_meta("puertas_jefe", [])
	var llave: Vector2i = dm.get_meta("cofre_llave", Vector2i(-1, -1))
	check(puertas.size() > 0 and llave in dm.get_meta("lista_cofres", []), "la sala del jefe está cerrada (%d puertas) y la llave está en un cofre %s" % [puertas.size(), llave])
	if puertas.size() > 0:
		check(not zonas.puede_abrir(puertas[0].x, puertas[0].y, activo()), "sin la llave la puerta del jefe no se abre")
		zonas.al_abrir_cofre(llave.x, llave.y, activo())
		check(zonas.tiene_llave() and zonas.puede_abrir(puertas[0].x, puertas[0].y, activo()), "con la llave del cofre del campamento, sí")
		dm.set_meta("llave_jefe", false)
	# Campfire: heals everyone and the day goes by
	var h := activo()
	h.set_meta("hp_actual", 1)
	var dia := int(pm.get_meta("turno_actual", 1))
	check(zonas.descansar(h), "se puede descansar en la hoguera explorando")
	check(int(h.get_meta("hp_actual")) == int(h.get_meta("hp_max")) and int(pm.get_meta("turno_actual", 1)) == dia + 1, "descansar cura y pasa el día")
	# The dungeon stirs: reinforcements to the camps still standing
	var antes := (dm.get_meta("lista_enemigos", []) as Array).size()
	var llegados: int = zonas.agitar()
	check(llegados > 0 and (dm.get_meta("lista_enemigos", []) as Array).size() == antes + llegados, "al agitarse la mazmorra llegan refuerzos (%d)" % llegados)

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
			# At the start no enemies are awake: we explore (no PA and no walk area)
			var modo = root.get_node("Modo")
			check(not modo.en_combate and dm.get_meta("modo_exploracion", false), "se empieza explorando (sin turnos)")
			check(ui.get_node("HUD").lbl_modo.text == "Explorando", "y el HUD lo dice ('%s')" % ui.get_node("HUD").lbl_modo.text)
			return 0.2,
		func():
			_check_zonas()
			return 0.3,
		func():
			_check_inventario()
			# Debug panel (F10)
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
			# Inventory screen (I)
			var inv = _nodo_con_script("pantalla_inventario.gd")
			memo["inventario"] = inv
			check(inv != null, "existe la pantalla de inventario")
			if inv:
				var n := str(activo().name)
				pm.call("AnadirObjeto", n, "antorcha", 1)
				pm.call("AnadirObjeto", n, "cota_malla", 1)
				inv.abrir(n)
				check(inv.esta_abierta() and inv.heroe == n, "la tecla I abre la mochila de %s" % n)
			return 0.3,
		func():
			var inv = memo.get("inventario")
			if inv:
				var n: String = inv.heroe
				var inv_n: Array = pm.call("DescribirInventario", n)
				check(inv.objetos.size() == inv_n.size(), "el saco tiene un objeto por cada entrada del inventario (%d)" % inv.objetos.size())
				check(inv.capas["heroe"].texture != null and inv.capas["mano_principal"].texture != null, "el héroe se ve con su arma puesta")
				# The chainmail might not fit if the random loot already filled the backpack: then,
				# the torch
				var i_cota := _indice_de(n, "cota_malla")
				var nombre_obj := "Cota de malla"
				if i_cota < 0:
					i_cota = _indice_de(n, "antorcha")
					nombre_obj = "Antorcha"
				inv._mostrar_bocadillo({"origen": "inv", "indice": i_cota}, Vector2(700, 300))
				check(inv.bocadillo.visible and inv.det_texto.text.contains(nombre_obj) and (inv.det_texto.text.contains("Frente a") or inv.det_texto.text.contains("vacía") or inv.det_texto.text.contains("Igual que")), "el bocadillo del objeto lo compara con lo puesto")

			return 0.3,
		func():
			var inv = memo.get("inventario")
			if inv:
				var n: String = inv.heroe
				captura("01c2_bocadillo")
				inv._cerrar_bocadillo()
				var radio0 := int(activo().get_meta("radio_luz"))
				inv._equipar(_indice_de(n, "antorcha"))
				check(int(activo().get_meta("radio_luz")) == radio0 + 1 and inv.lbl_msg.text.ends_with("hecho"), "equipar desde la mochila: %s" % inv.lbl_msg.text)
				captura("01d_inventario")
				var capa_h: Control = inv.capas["heroe"]
				check(capa_h.size == Vector2(16, 16) * inv.ESCALA_HEROE, "el héroe de la mochila se dibuja a %d px por píxel (%s)" % [inv.ESCALA_HEROE, capa_h.size])
				inv._quitar("luz")
				check(int(activo().get_meta("radio_luz")) == radio0, "y quitarla: %s" % inv.lbl_msg.text)
				memo["radio_arrastre"] = radio0
				inv._refrescar(true)
			return 1.5, # Let the items fall to the bottom of the sack
		func():
			# Drag the torch with the mouse from the backpack to the light slot
			var inv = memo.get("inventario")
			if inv:
				var n: String = inv.heroe
				var origen: Control = inv.nodo_de_objeto(_indice_de(n, "antorcha"))
				var destino: Control = inv.muneco
				# Make sure no other item in the sack covers it and that it stops bouncing
				origen.move_to_front()
				var ent: int = inv._entrada_de(origen)
				if ent >= 0:
					inv.objetos[ent]["vel"] = Vector2.ZERO
				memo["arrastre"] = [origen.get_global_rect().get_center(), destino.get_global_rect().get_center()]
				_raton(memo["arrastre"][0], true, true)
			return 0.1,
		func():
			if memo.has("arrastre"):
				var a: Vector2 = memo["arrastre"][0]
				var b: Vector2 = memo["arrastre"][1]
				for k in range(1, 6):
					_raton(a.lerp(b, k / 5.0), true, false)
			return 0.2,
		func():
			if memo.has("arrastre"):
				_raton(memo["arrastre"][1], false, true)
			return 0.3,
		func():
			var inv = memo.get("inventario")
			if inv:
				var n: String = inv.heroe
				check(int(activo().get_meta("radio_luz")) == int(memo["radio_arrastre"]) + 1, "arrastrar la antorcha del saco al héroe la equipa (%s)" % inv.lbl_msg.text)
				captura("01e_arrastrar")
				inv._quitar("luz")
				pm.call("QuitarObjeto", n, _indice_de(n, "antorcha"), 1)
				pm.call("QuitarObjeto", n, _indice_de(n, "cota_malla"), 1)
				inv.cerrar()
				check(not inv.esta_abierta(), "la mochila se cierra")
			return 0.2,
		func():
			# Hover the furthest reachable tile: the path and the cost must show
			var alc: PackedInt32Array = dm.call("DebugAlcance")
			var p := pos_activa()
			var mejor := p
			for i in range(0, alc.size(), 2):
				var c := Vector2i(alc[i], alc[i + 1])
				# Close by (exploring you can go anywhere explored: far away would wake someone up)
				var d := absi(c.x - p.x) + absi(c.y - p.y)
				if d <= 4 and d > absi(mejor.x - p.x) + absi(mejor.y - p.y):
					mejor = c
			memo["destino"] = mejor
			# The real mouse (if someone moves it) mustn't overwrite the simulated hover
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_DISABLED
			dm.call("ActualizarHover", mejor.x, mejor.y)
			return 0.3,
		func():
			var coste: Label = dm.get_node("DungeonRenderer/Capa2D/Raiz/Coste")
			var ruta: MultiMesh = dm.get_node("DungeonRenderer/Ruta").multimesh
			check(ruta.instance_count > 0, "al pasar el ratón se dibuja la ruta (%d pasos)" % ruta.instance_count)
			check(not coste.visible or coste.text == "", "explorando no sale coste en PA junto al cursor ('%s')" % coste.text)
			captura("02_hover_ruta")
			var mejor: Vector2i = memo["destino"]
			memo["pa_antes"] = int(activo().get_meta("pa_actual"))
			memo["movido"] = str(activo().name)
			memo["pos_antes"] = pos_activa()
			memo["grupo_antes"] = {}
			for h in _heroes():
				if h != activo():
					memo["grupo_antes"][str(h.name)] = h.get_meta("grid_pos")
			dm.call("ProcesarClicEnCasilla", mejor.x, mejor.y)
			check(dm.get_meta("is_moving", false), "empieza a moverse")
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_INHERIT
			return 2.0,
		func():
			captura("03_tras_mover")
			_cerrar_evento("tras caminar")
			var gastados: int = memo["pa_antes"] - int(pm.get_node(NodePath(memo["movido"])).get_meta("pa_actual"))
			if root.get_node("Modo").en_combate:
				print("[test] AVISO un enemigo despertó por el camino: no se puede probar la exploración aquí")
			else:
				check(gastados == 0, "explorando, andar no gasta PA (%d)" % gastados)
				var siguieron := 0
				for n in memo["grupo_antes"]:
					var h = pm.get_node_or_null(n)
					if h and h.get_meta("grid_pos") != memo["grupo_antes"][n]:
						siguieron += 1
				check(siguieron > 0, "el resto del grupo sigue al que anda (%d se han movido)" % siguieron)
				# Single file: each one right behind the one ahead
				var modo = root.get_node("Modo")
				var delante: Vector2i = pos_activa()
				var en_fila := true
				for n in modo.fila:
					var f = pm.get_node_or_null(n)
					if f == null:
						continue
					if absi(f.get_meta("grid_pos").x - delante.x) + absi(f.get_meta("grid_pos").y - delante.y) > 1:
						en_fila = false
					delante = f.get_meta("grid_pos")
				var posiciones := [pos_activa()]
				for n in modo.fila:
					posiciones.append(pm.get_node(n).get_meta("grid_pos"))
				check(en_fila and modo.fila.size() > 0, "van en fila india, cada uno pegado al de delante (%s: %s, salió de %s)" % [modo.fila, posiciones, memo["pos_antes"]])
			if not root.get_node("Modo").en_combate:
				_check_camara_centrada("tras caminar") # When a fight starts the camera focuses on the enemy for a moment
			# Peek (what the minimap or the drag does): the camera goes to another point...
			var cam: Camera3D = dm.get_node("Camera3D")
			var p := pos_activa()
			var lado := int(Engine.get_meta("alturas_lado", 40))
			memo["vistazo"] = Vector3(clampi(p.x + 8, 1, lado - 2) + 0.5, 0, clampi(p.y + 8, 1, lado - 2) + 0.5)
			cam.set_meta("vistazo", memo["vistazo"])
			return 1.2,
		func():
			var cam: Camera3D = dm.get_node("Camera3D")
			var piv: Vector3 = cam.get_meta("pivote")
			var dxz := Vector2(piv.x, piv.z).distance_to(Vector2(memo["vistazo"].x, memo["vistazo"].z))
			check(dxz < 1.0, "durante el vistazo la cámara va al punto (a %.2f)" % dxz)
			captura("03b_vistazo")
			# ...and on release it comes back to the hero on its own
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
			captura("05_hud")
			# New HUD: clicking your hero doesn't open a floating menu any more; the actions are on
			# the bar
			check(not dm.get_meta("is_moving", false), "clicar en tu héroe no hace nada (las acciones están en la barra)")
			var hud = ui.get_node("HUD")
			for id in ["mochila", "atacar", "dia"]:
				check(hud.botones.has(id) and hud.botones[id].is_visible_in_tree(), "la barra tiene el botón '%s'" % id)
			check(hud.botones["pasar"].is_visible_in_tree() == root.get_node("Modo").en_combate, "FIN DEL TURNO solo se ve en combate")
			# Abilities: every hero starts with their class one, on the bottom bar (Z X C V)
			var prog = root.get_node("Progreso")
			var lista_h: Array = prog.lista(activo())
			check(lista_h.size() == 1 and prog.INICIALES.get(str(activo().get_meta("clase")), "") == lista_h[0]["id"], "el héroe empieza con la habilidad de su clase (%s)" % [lista_h])
			var visibles_hab := 0
			for b in hud.huecos_hab:
				if b.visible:
					visibles_hab += 1
			check(visibles_hab == prog.max_ranuras(activo()), "la barra tiene %d ranuras de habilidad (%s: %d)" % [visibles_hab, activo().get_meta("clase"), prog.max_ranuras(activo())])
			check(int(activo().get_meta("nivel", 0)) == 1 and hud.lbl_nivel.text == "NV 1", "empieza en nivel 1 (%s)" % hud.lbl_nivel.text)
			# Hovering ATTACK / ABILITY shows its reach (red / blue)
			var mm_alc: MultiMesh = dm.get_node("DungeonRenderer/Alcance").multimesh
			hud.botones["atacar"].mouse_entered.emit()
			var rojos := 0
			for i in mm_alc.instance_count:
				var c: Color = mm_alc.get_instance_custom_data(i)
				if c.r > 0.9 and c.g < 0.3:
					rojos += 1
			check(rojos > 0, "al pasar por ATACAR se ve el alcance del arma en rojo (%d casillas)" % rojos)
			hud.botones["atacar"].mouse_exited.emit()
			hud.huecos_hab[0].mouse_entered.emit()
			var azules := 0
			for i in mm_alc.instance_count:
				var c: Color = mm_alc.get_instance_custom_data(i)
				if c.b > 0.9 and c.r < 0.5:
					azules += 1
			check(azules > 0, "al pasar por una habilidad se ve su alcance en azul (%d casillas)" % azules)
			captura("05a_rango_habilidad")
			hud.huecos_hab[0].mouse_exited.emit()
			# The active hero block: gauges with their health and PA
			var hp := int(activo().get_meta("hp_actual"))
			var hp_max := int(activo().get_meta("hp_max"))
			check(hud.medidores["hp_actual"].texto.text == "%d/%d" % [hp, hp_max], "el corazón del retrato lleva la vida (%s)" % hud.medidores["hp_actual"].texto.text)
			if root.get_node("Modo").en_combate:
				check(hud.lbl_pa.text.begins_with("PA %d/" % int(activo().get_meta("pa_actual"))), "debajo del retrato salen los PA (%s)" % hud.lbl_pa.text)
			else:
				check(hud.lbl_pa.text == "Explorando", "explorando, debajo del retrato no hay PA sino 'Explorando' (%s)" % hud.lbl_pa.text)
			check(hud.huecos_usables.size() == 5 and hud.huecos_acc.size() == 5, "la barra tiene las 5 acciones básicas (con ocultarse y antorcha) y 5 huecos de un uso")
			check(hud.barra_turno.visible == (root.get_node("Modo").en_combate or hud.slots_enemigo.any(func(s): return s.visible)), "el orden de turno solo sale en combate o con enemigos a la vista")
			var slot_act = hud.slots[str(activo().name)]
			var con_turnos: bool = root.get_node("Modo").en_combate
			check(slot_act.get_node("PA").visible == (con_turnos and int(activo().get_meta("pa_actual")) > 0) and slot_act.get_node("PA").text == str(int(activo().get_meta("pa_actual"))), "el retrato de arriba lleva sus PA en amarillo solo en combate (%s, combate %s)" % [slot_act.get_node("PA").text, con_turnos])
			check(not ui.get_node("PanelFichas").visible, "las fichas viejas de la izquierda no se ven")
			# Bubble when hovering over a hero
			var p: Vector2i = memo["pos_antes"]
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_DISABLED
			dm.call("ActualizarHover", p.x, p.y)
			return 0.2,
		func():
			var prev = ui.get_node("Prevision")
			check(prev.visible and prev.titulo.text == str(activo().get_meta("nombre_display", activo().name)).to_upper(), "al pasar el ratón por un héroe sale su bocadillo ('%s')" % prev.titulo.text)
			captura("05b_bocadillo_heroe")
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_INHERIT
			check(not dm.get_meta("is_moving", false) and pos_activa() == memo["pos_antes"], "y no provoca movimiento")
			# Click on another hero: they become the active one and their card stays pinned
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
			# The top bar: all the heroes; clicking a portrait selects that hero
			var hud = ui.get_node("HUD")
			var n_heroes := 0
			for h in pm.get_children():
				if h.has_meta("grid_pos") and h.has_meta("hp_actual"):
					n_heroes += 1
					check(ficha_visible(str(h.name)), "%s está en la barra de arriba" % h.name)
			var otro: Node = null
			for h in pm.get_children():
				if h.has_meta("grid_pos") and h != activo() and not h.get_meta("ha_escapado", false) and int(h.get_meta("pa_actual", 0)) > 0:
					otro = h
					break
			if otro:
				hud.slots[str(otro.name)].pressed.emit()
				check(activo() == otro, "clicar el retrato de %s en la barra lo selecciona" % otro.name)
			return 0.3,
		func():
			captura("06_barra_heroes")
			# While the turn change is being read you can't act
			Engine.remove_meta("test_sin_espera_banner")
			ui.get_node("Banner").mostrar("Turno de prueba")
			memo["pos_banner"] = pos_activa()
			return 0.1,
		func():
			check(dm.get_meta("bloqueo_banner", false), "mientras se lee el cambio de turno las acciones están bloqueadas")
			var p: Vector2i = memo["pos_banner"]
			dm.call("ProcesarClicEnCasilla", p.x + 1, p.y)
			check(not dm.get_meta("is_moving", false), "y un clic en el mapa no hace nada")
			check(ui.get_node("HUD").botones["pasar"].disabled, "y la rueda está desactivada")
			return 1.6,
		func():
			check(not dm.get_meta("bloqueo_banner", false), "al desaparecer el mensaje se puede volver a actuar")
			Engine.set_meta("test_sin_espera_banner", true)
			_preparar_cruce()
			return 0.2,
		func():
			# Walk through a teammate: A --> (B) --> destination
			if memo.has("cruce"):
				var c: Array = memo["cruce"]
				dm.call("ProcesarClicEnCasilla", c[2].x, c[2].y)
			return 1.4,
		func():
			if memo.has("cruce") and (dm.get_meta("bloqueo_evento", false) or root.get_node("Modo").con_turnos() and pos_activa() != memo["cruce"][2]):
				# On the way a trap triggered or an enemy saw them: it can't be tested here
				print("[test] AVISO algo frenó la prueba de cruzar (trampa o enemigo)")
				_cerrar_evento("al cruzar")
				memo.erase("cruce")
			if memo.has("cruce"):
				var c: Array = memo["cruce"]
				var b: Node = memo["cruce_b"]
				check(pos_activa() == c[2], "un héroe puede cruzar por encima de un compañero (%s -> %s, quiere %s)" % [c[0], pos_activa(), c[2]])
				check(b.get_meta("grid_pos") == c[1], "y el compañero sigue en su sitio")
				# With 1 PA they'd only reach the teammate's tile: they can't end there, so they
				# don't move
				dm.call("TeletransporteDebug", c[0].x, c[0].y)
				activo().set_meta("pa_actual", 1)
				memo["cruce_pa1"] = true
				dm.call("ProcesarClicEnCasilla", c[2].x, c[2].y)
			return 0.8,
		func():
			_cerrar_evento("tras cruzar")
			if memo.has("cruce_pa1") and memo.has("cruce"):
				var c: Array = memo["cruce"]
				if root.get_node("Modo").en_combate:
					check(pos_activa() == c[0], "pero nunca acaba en la misma casilla que otro (se queda en %s)" % pos_activa())
				else:
					# Exploring there's no PA cutting the path: they arrive, and the teammate (from
					# their group) steps aside
					var b_cruce: Node = memo["cruce_b"]
					check(pos_activa() != b_cruce.get_meta("grid_pos"), "nunca acaban dos en la misma casilla (%s / %s)" % [pos_activa(), b_cruce.get_meta("grid_pos")])
				activo().set_meta("pa_actual", 6)
			_preparar_evento_lejano()
			return 0.3,
		func():
			if memo.has("evento_lejano"):
				var ev: Vector2i = memo["evento_lejano"]
				_cerrar_evento("antes de ir al evento")
				dm.call("ProcesarClicEnCasilla", ev.x, ev.y)
			return 2.6,
		func():
			if memo.has("evento_lejano"):
				var ev: Vector2i = memo["evento_lejano"]
				var p := pos_activa()
				if absi(p.x - ev.x) + absi(p.y - ev.y) > 1 and (activo().get_meta("in_combat", false) or root.get_node("Modo").en_combate):
					# An enemy saw them on the way and stopped them (which is correct): it can't be
					# tested here
					print("[test] AVISO un enemigo frenó al héroe antes de llegar al evento")
				else:
					check(p != ev and absi(p.x - ev.x) + absi(p.y - ev.y) == 1, "al clicar un evento lejano el héroe se pone a su lado, no encima (%s, evento %s)" % [p, ev])
					check(dm.get_meta("bloqueo_evento", false), "y al llegar el evento se abre solo")
				_cerrar_evento("tras ir al evento")
			dm.call("ToggleModoDebug")
			return 0.3,
		func():
			captura("07_debug_mapa_completo")
			var cofres: Array = dm.get_meta("lista_cofres", [])
			check(cofres.size() > 0, "hay cofres (%d)" % cofres.size())
			if cofres.size() > 0:
				# The first one with a free tile next to it (some may be boxed in next to the
				# stairs)
				var elegido: Vector2i = cofres[0]
				var llega := false
				for c in cofres:
					if junto_a(c):
						elegido = c
						llega = true
						break
				memo["cofre"] = elegido
				check(llega, "teletransporte junto al cofre %s" % elegido)
			return 0.6,
		func():
			var b := bocadillos(0)
			check(b.size() > 0, "junto al cofre aparece un bocadillo")
			if b.size() > 0:
				var con_pa: bool = str(b[0].get_meta("texto")).contains("PA")
				check(con_pa == root.get_node("Modo").con_turnos(), "el bocadillo indica el coste solo con turnos (combate o sigilo): '%s'" % str(b[0].get_meta("texto")).replace("\n", " | "))
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
			# Door: we stand next to it and open it
			var puertas: Array = dm.get_meta("lista_puertas", []).filter(func(q): return not q in dm.get_meta("puertas_jefe", []))
			memo["puerta"] = null
			if puertas.size() > 0 and junto_a(puertas[0]):
				memo["puerta"] = puertas[0]
				# No debug mode and with just enough PA: the hero switch has to wait for the door
				memo["debug_puerta"] = dm.get_meta("debug_mode", false)
				if memo["debug_puerta"]:
					dm.call("ToggleModoDebug")
				activo().set_meta("pa_actual", 2)
				memo["heroe_puerta"] = activo()
				memo["puerta_en_combate"] = root.get_node("Modo").con_turnos()
				dm.call("ProcesarClicEnCasilla", puertas[0].x, puertas[0].y)
				check(activo() == memo["heroe_puerta"], "al abrir la puerta con los últimos PA no se cambia de héroe en el acto")
			else:
				print("[test] AVISO este piso no tiene puertas accesibles")
			# Braziers: they take up their tile
			var braseros: Array = dm.get_meta("lista_braseros", [])
			var alc: PackedInt32Array = dm.call("DebugAlcance")
			var pisable := false
			for i in range(0, alc.size(), 2):
				if Vector2i(alc[i], alc[i + 1]) in braseros:
					pisable = true
			check(not pisable, "los braseros (%d) no se pueden pisar" % braseros.size())
			return 1.0,
		func():
			if memo.has("heroe_puerta"):
				# Exploring there are no turns or PA to spend: only with turns (combat or stealth)
				# does it move on to the next one
				if memo.get("puerta_en_combate", false):
					check(activo() != memo["heroe_puerta"], "cuando acaba la puerta pasa al siguiente héroe (%s)" % (activo().name if activo() else "nadie"))
				else:
					check(activo() == memo["heroe_puerta"], "explorando, abrir una puerta no pasa el turno")
				if memo["debug_puerta"]:
					dm.call("ToggleModoDebug")
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
			# Preview card when hovering the enemy
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_DISABLED
			dm.call("ActualizarHover", e.x, e.y)
			return 0.2,
		func():
			var e: Vector2i = memo.get("enemigo", Vector2i(-1, -1))
			var prev: Control = ui.get_node("Prevision")
			check(prev.visible, "al pasar sobre el enemigo sale la tarjeta de previsión")
			check(prev.acierto.text.ends_with("de acertar") and prev.filas.get_child_count() >= 1, "la tarjeta enseña solo las stats tocadas y el %% de acierto ('%s', %d filas)" % [prev.acierto.text, prev.filas.get_child_count()])
			check(int(dm.call("TipoCursor", e.x, e.y)) == 2, "el cursor sobre un enemigo es la espada")
			var enemigo_n := dm.get_node_or_null("Enemy_%d_%d" % [e.x, e.y])
			var prevision: Dictionary = dm.get_node("CombatManager").call("PrevisionAtaque", e.x, e.y)
			memo["contra_previsto"] = int(prevision.get("contra", 0))
			memo["enemigo_nombre"] = str(prevision.get("defensor", "?"))
			var arma = root.get_node("Catalogo").obtener(activo().get_meta("equipo").get("mano_principal", ""))
			if arma and enemigo_n:
				var esperado := maxi(1, arma.dano + int(activo().get_meta(arma.stat_ataque)) - int(enemigo_n.get_meta("defensa")))
				var coste_esperado: int = 0 if dm.get_meta("debug_mode", false) else arma.coste_pa
				check(int(prevision["dano"]) == esperado and int(prevision["coste"]) == coste_esperado, "la previsión usa el arma (%s: %d de daño, %d PA)" % [arma.nombre, prevision["dano"], prevision["coste"]])
			captura("10_bocadillo_enemigo")
			var nodo := dm.get_node_or_null("Enemy_%d_%d" % [e.x, e.y])
			if nodo:
				# Let it survive the hit: that way the counterattack shows
				nodo.set_meta("hp_max", 999)
				nodo.set_meta("hp_actual", 999)
			memo["hp0"] = int(nodo.get_meta("hp_actual")) if nodo else -1
			memo["nodo"] = nodo
			memo["atacante"] = activo()
			memo["hp_atacante"] = int(activo().get_meta("hp_actual"))
			memo["indicadores0"] = int(root.get_node("Indicadores").mostrados)
			dm.call("ProcesarClicEnCasilla", e.x, e.y)
			check(dm.get_meta("is_moving", false), "el ataque es una secuencia animada (tablero bloqueado)")
			check(root.get_node("DadoCombate").esta_visible(), "al atacar se tira el d20 en pantalla")
			check(dm.get_node("Camera3D").has_meta("encuadre"), "al atacar la cámara encuadra al héroe y al enemigo")
			dm.get_node("InputManager").process_mode = Node.PROCESS_MODE_INHERIT
			return 0.12,
		func():
			captura("10a_dado_ataque")
			check(ui.get_node("HUD").modulate.a < 0.9, "durante el ataque el HUD se aparta (alfa %.2f)" % ui.get_node("HUD").modulate.a)
			return 0.9,
		func():
			captura("10b_impacto")
			return 1.6,
		func():
			var nodo = memo.get("nodo")
			var hp1: int = nodo.get_meta("hp_actual") if is_instance_valid(nodo) and not str(nodo.name).ends_with("_muerto") else -999
			check(hp1 < int(memo["hp0"]), "el ataque quita vida (%d -> %d)" % [memo["hp0"], hp1])
			var nuevos: int = int(root.get_node("Indicadores").mostrados) - int(memo["indicadores0"])
			check(nuevos >= 1, "sobre el enemigo sale el corazón con el daño (%d indicadores)" % nuevos)
			check(not dm.get_node("Camera3D").has_meta("encuadre"), "al acabar el ataque la cámara vuelve al héroe")
			var atacante: Node = memo["atacante"]
			# Counterattacks if its weapon reaches 1 tile (a bow doesn't): same as the preview
			# announces
			var herido: bool = int(atacante.get_meta("hp_actual")) < int(memo["hp_atacante"])
			check(herido == (int(memo["contra_previsto"]) > 0), "el %s %s, como anunciaba la previsión (%s: %d -> %d PV)" % [memo["enemigo_nombre"], "contraataca" if herido else "no contraataca", atacante.name, memo["hp_atacante"], atacante.get_meta("hp_actual")])
			if is_instance_valid(nodo):
				nodo.set_meta("hp_actual", 5) # So the rest of the script doesn't have to fight it
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
			return 2.8, # d20 + projectile
		func():
			if memo.has("nodo_arco"):
				var nodo = memo["nodo_arco"]
				var hp1: int = nodo.get_meta("hp_actual") if is_instance_valid(nodo) and not str(nodo.name).ends_with("_muerto") else -999
				check(hp1 < int(memo["hp_e_arco"]), "el disparo quita vida (%d -> %d)" % [memo["hp_e_arco"], hp1])
				var h: Node = memo["arquero"]
				check(int(h.get_meta("hp_actual")) >= int(memo["hp_h_arco"]), "a distancia no hay contraataque (%s sigue con %d PV)" % [h.name, h.get_meta("hp_actual")])
				captura("11b_disparo")
			_preparar_turno_enemigo()
			return 0.5,
		func():
			# Joke items: they're in the catalogue and their equipment effects work
			var cat = root.get_node("Catalogo")
			for id in ["chancleta_inquisicion", "escudo_carton", "pocion_malir_sal", "espada_multiusos", "casco_cubo_basura", "boton_panico", "mascara_no_estoy", "grano_oro"]:
				check(cat.existe(id), "el objeto %s está en el catálogo" % id)
			var h := activo()
			var n := str(h.name)
			# Trash helmet: +4 defense (over what you get with nothing on your head)
			var cabeza_antes := str(h.get_meta("equipo", {}).get("cabeza", ""))
			if cabeza_antes != "":
				pm.call("Desequipar", n, "cabeza")
			var def0 := int(h.get_meta("defensa"))
			if int(pm.call("AnadirObjeto", n, "casco_cubo_basura", 1)) > 0:
				var err = pm.call("Equipar", n, _indice_de(n, "casco_cubo_basura"))
				check(err == "" and int(h.get_meta("defensa")) == def0 + 4, "el casco de basura da +4 de defensa (%d -> %d) %s" % [def0, h.get_meta("defensa"), err])
				pm.call("Desequipar", n, "cabeza")
				pm.call("QuitarObjeto", n, _indice_de(n, "casco_cubo_basura"), 1) # So it doesn't fill the backpack
			if cabeza_antes != "":
				pm.call("Equipar", n, _indice_de(n, cabeza_antes))
			# Cardboard shield: +5 on the first combat turn
			var mano_antes := str(h.get_meta("equipo", {}).get("mano_secundaria", ""))
			if mano_antes != "":
				pm.call("Desequipar", n, "mano_secundaria")
			var def1 := int(h.get_meta("defensa"))
			if int(pm.call("AnadirObjeto", n, "escudo_carton", 1)) > 0:
				h.set_meta("carton_turnos", 0)
				var err2 = pm.call("Equipar", n, _indice_de(n, "escudo_carton"))
				check(err2 == "" and int(h.get_meta("defensa")) == def1 + 5, "el escudo de cartón da +5 el primer turno (%d -> %d) %s" % [def1, h.get_meta("defensa"), err2])
				pm.call("Desequipar", n, "mano_secundaria")
				pm.call("QuitarObjeto", n, _indice_de(n, "escudo_carton"), 1)
			if mano_antes != "":
				pm.call("Equipar", n, _indice_de(n, mano_antes))
			# The chicken is now a global ability: learnt and cast from the bar
			var prog = root.get_node("Progreso")
			h.set_meta("pa_actual", maxi(int(h.get_meta("pa_actual")), 6))
			h.set_meta("habilidades", []) # Free room (the script's automatic drafts may have filled them)
			prog.aprender(h, "gallina")
			var i_gal := -1
			for k in prog.lista(h).size():
				if prog.lista(h)[k]["id"] == "gallina":
					i_gal = k
			memo["i_gal"] = i_gal
			memo["heroe_gal"] = h
			check(i_gal >= 0, "se aprende la habilidad de la gallina")
			prog.pulsar(i_gal)
			var banner = ui.get_node("Banner")
			check(banner.texto.text == "Gallina lanza-huevos", "se anuncia la habilidad ('%s')" % banner.texto.text)
			check(dm.get_node("Camera3D").has_meta("encuadre"), "la cámara se acerca a quien lanza la habilidad")
			return 1.4,
		func():
			var esp = root.get_node("Especiales")
			var prog = root.get_node("Progreso")
			check(is_instance_valid(esp.gallina), "la habilidad invoca a la gallina lanza-huevos")
			var e_gal: Dictionary = prog.lista(memo["heroe_gal"])[memo["i_gal"]]
			check(int(e_gal["usos"]) == 0 and int(e_gal["enfriamiento"]) == 3, "gasta su uso y se pone a recargar (%s)" % [e_gal])
			check(prog.motivo_no_usar(memo["heroe_gal"], memo["i_gal"]) != "", "y no se puede volver a lanzar ('%s')" % prog.motivo_no_usar(memo["heroe_gal"], memo["i_gal"]))
			# Killing gives XP; with Bloodthirst, killing heals (a passive)
			var h := activo()
			prog.aprender(h, "sed_sangre")
			h.set_meta("hp_actual", maxi(1, int(h.get_meta("hp_max")) - 30))
			memo["hp_sed"] = int(h.get_meta("hp_actual"))
			memo["xp_total"] = _xp_total(h)
			var victima: Node = null
			for pos in dm.get_meta("lista_enemigos", []):
				var e = dm.get_node_or_null("Enemy_%d_%d" % [pos.x, pos.y])
				if e and e != memo.get("ia_enemigo"):
					victima = e
					break
			memo["victima"] = victima
			if victima:
				prog.golpear_enemigo(h, victima, 9999)
			return 0.5,
		func():
			var prog = root.get_node("Progreso")
			var h := activo()
			if memo["victima"]:
				check(not is_instance_valid(memo["victima"]) or str(memo["victima"].name).ends_with("_muerto"), "una habilidad puede matar a un enemigo")
				check(_xp_total(h) > int(memo["xp_total"]), "matar da XP (%d -> %d)" % [memo["xp_total"], _xp_total(h)])
				check(int(h.get_meta("hp_actual")) > int(memo["hp_sed"]), "Sed de sangre cura al matar (%d -> %d)" % [memo["hp_sed"], h.get_meta("hp_actual")])
			# Level up: stats by class and the draft
			memo["nivel_antes"] = int(h.get_meta("nivel"))
			memo["hp_max_antes"] = int(h.get_meta("hp_max"))
			prog.pendientes.clear()
			var pn = root.get_node("Main/PantallaNivel")
			if pn.visible:
				pn.call("_cerrar")
			memo["probando_draft"] = true
			prog.ganar_xp(h, prog.xp_para(int(h.get_meta("nivel"))) - int(h.get_meta("xp")))
			check(int(h.get_meta("nivel")) == int(memo["nivel_antes"]) + 1, "con la XP justa sube de nivel (%d)" % h.get_meta("nivel"))
			check(int(h.get_meta("hp_max")) > int(memo["hp_max_antes"]), "y le sube la vida máxima (%d -> %d)" % [memo["hp_max_antes"], h.get_meta("hp_max")])
			return 1.2,
		func():
			var pn = root.get_node("Main/PantallaNivel")
			var prog = root.get_node("Progreso")
			check(pn.visible, "al subir de nivel sale la pantalla del draft")
			check(pn.opciones.size() >= 1 and pn.opciones.size() <= 3, "con hasta 3 habilidades a elegir (%d)" % pn.opciones.size())
			check(dm.get_meta("bloqueo_evento", false), "y mientras se elige el tablero está bloqueado")
			captura("11e_draft")
			if pn.visible and pn.opciones.size() > 0:
				var h: Node = pn.heroe
				var id: String = pn.opciones[0].obtener_id()
				var antes: Dictionary = prog._buscar(h, id).duplicate()
				pn.elegir(0)
				if pn.visible:
					# Slots full: forgets the first one
					pn.olvidar(str(prog.lista(h)[0]["id"]))
				var ahora: Dictionary = prog._buscar(h, id)
				check(ahora.size() > 0 and (antes.is_empty() or int(ahora["pilas"]) > int(antes["pilas"])), "elegir una carta la aprende (o la apila): %s" % id)
			check(not pn.visible and not dm.get_meta("bloqueo_evento", false), "y la pantalla se cierra")
			memo.erase("probando_draft")
			return 0.3,
		func():
			# POSITIONING: flanking, attack of opportunity, states, push and Retreat
			var tac = root.get_node("Tactica")
			var est = root.get_node("Estados")
			var h := activo()
			var e: Node = null
			for pos in dm.get_meta("lista_enemigos", []):
				var cand = dm.get_node_or_null("Enemy_%d_%d" % [pos.x, pos.y])
				if cand and cand != memo.get("ia_enemigo") and junto_a(pos):
					e = cand
					break
			if e == null:
				print("[test] AVISO no hay enemigo con sitio al lado para probar el posicionamiento")
				return 0.1
			e.set_meta("hp_max", 200)
			e.set_meta("hp_actual", 200)
			memo["e_tac"] = e
			var pe: Vector2i = e.get_meta("grid_pos")
			var ph := pos_activa()
			# Flanking: an ally on the other side of the enemy
			var opuesto: Vector2i = pe + (pe - ph)
			var otro: Node = null
			for o in _heroes():
				if o != h and not o.get_meta("ha_escapado", false) and not o.get_meta("esta_muerto", false):
					otro = o
					break
			var libres: PackedByteArray = dm.call("ObtenerTransitables")
			var lado := int(Engine.get_meta("alturas_lado", 40))
			if otro and opuesto.x > 0 and opuesto.y > 0 and opuesto.x < lado and opuesto.y < lado and libres[opuesto.x * lado + opuesto.y] == 1:
				var sitio_otro: Vector2i = otro.get_meta("grid_pos")
				dm.call("ColocarHeroe", str(otro.name), opuesto.x, opuesto.y, 1)
				var m: Dictionary = tac.modificadores(h, e)
				check(int(m["ventaja"]) == 1 and "Flanqueo" in m["razones"], "con un aliado al otro lado hay flanqueo: ventaja (%s)" % [m])
				var prev: Dictionary = dm.get_node("CombatManager").call("PrevisionAtaque", pe.x, pe.y)
				check("Flanqueo" in prev.get("razones", PackedStringArray()), "la previsión del ataque lo explica (%s)" % [prev.get("razones")])
				dm.call("ColocarHeroe", str(otro.name), sitio_otro.x, sitio_otro.y, 1)
			else:
				print("[test] AVISO no hay sitio al otro lado del enemigo para probar el flanqueo")
			# Attack of opportunity: moving away from an awake enemy gives it a free hit
			e.set_meta("in_combat", true)
			if e.has_meta("reaccion_dia"):
				e.remove_meta("reaccion_dia")
			est.quitar(h, "desenganchado")
			tac.al_mover(h, ph, ph + (ph - pe))
			check(int(e.get_meta("reaccion_dia", -1)) == int(pm.get_meta("turno_actual")), "alejarse de un enemigo provoca su ataque de oportunidad")
			# States: ardiendo burns at the start of the day
			est.aplicar(e, "ardiendo", 2, 1, h)
			check(est.tiene(e, "ardiendo"), "se puede prender fuego a un enemigo")
			var hp0 := int(e.get_meta("hp_actual"))
			est.nuevo_dia()
			check(int(e.get_meta("hp_actual")) < hp0 and est.de(e)["ardiendo"]["dias"] == 1, "al empezar el día arder quita vida (%d -> %d) y se gasta un día" % [hp0, e.get_meta("hp_actual")])
			# Push: away from the hero up to 5 tiles (or it hits something and gets stunned)
			var antes: Vector2i = e.get_meta("grid_pos")
			var hp_antes := int(e.get_meta("hp_actual"))
			var movidas: int = tac.empujar(e, tac.dir_hacia(ph, antes), 5, h)
			check(movidas > 0 or est.tiene(e, "aturdido") or int(e.get_meta("hp_actual")) < hp_antes, "un empujón mueve al enemigo o lo estampa (%d casillas, aturdido %s, vida %d -> %d)" % [movidas, est.tiene(e, "aturdido"), hp_antes, e.get_meta("hp_actual")])
			check(e.get_meta("grid_pos") == antes + tac.dir_hacia(ph, antes) * movidas, "y el mapa lo sabe (%s)" % e.get_meta("grid_pos"))
			# Retreat: basic action
			h.set_meta("pa_actual", maxi(int(h.get_meta("pa_actual")), 4))
			root.get_node("Progreso").lanzar_accion("retirada")
			captura("11f_posicionamiento")
			return 1.4,
		func():
			var est = root.get_node("Estados")
			check(est.tiene(activo(), "desenganchado"), "la Retirada quita los ataques de oportunidad hasta mañana")
			# Squads: the awake enemy wakes up its whole squad, and there's combat while any of them
			# is left
			var modo = root.get_node("Modo")
			if memo.has("e_tac") and is_instance_valid(memo["e_tac"]) and memo["e_tac"].has_meta("peloton"):
				var id: int = memo["e_tac"].get_meta("peloton")
				var dormidos := 0
				var total := 0
				for e in root.get_node("Tactica").unidades():
					if e.get_meta("peloton", -2) == id:
						total += 1
						if not e.get_meta("in_combat", false):
							dormidos += 1
				check(dormidos == 0 and modo.en_combate, "si despierta uno, despierta todo su pelotón (%d de %d) y hay combate" % [total - dormidos, total])
			# Hero groups: the active one's first
			var grupos: Array = modo.grupos_heroes()
			check(not grupos.is_empty() and activo() in grupos[0], "los héroes cercanos forman grupo; el del activo va primero (%d grupos)" % grupos.size())
			return 0.2,
		func():
			var gal = root.get_node("Especiales").gallina
			if is_instance_valid(gal):
				check(gal.is_visible_in_tree(), "la gallina se ve en el mapa")
			captura("11d_gallina")
			if is_instance_valid(gal):
				# Out: so its egg doesn't get into the enemy turn script
				gal.queue_free()
				root.get_node("Especiales").gallina = null
			# The day ends: everyone out of PA -> the awake enemies play
			memo["turno_antes"] = int(pm.get_meta("turno_actual"))
			for h in _heroes():
				h.set_meta("pa_actual", 0)
			pm.call("TerminarTurnoPersonaje")
			check(dm.get_meta("turno_enemigo", false), "al acabar el día empieza el turno enemigo")
			return 0.3, # The script just waits for it to end (the board is locked meanwhile)
		func():
			check(not dm.get_meta("turno_enemigo", false) and int(pm.get_meta("turno_actual")) == int(memo["turno_antes"]) + 1, "tras el turno enemigo empieza el día %d" % pm.get_meta("turno_actual"))
			if memo.has("ia_enemigo"):
				var e = memo["ia_enemigo"]
				var h: Node = memo["ia_heroe"]
				var antes: int = memo["ia_dist"]
				var ahora_d := -1
				var se_movio := false
				var tiro := false
				if is_instance_valid(e):
					se_movio = e.get_meta("grid_pos") != memo["ia_pos"]
					tiro = bool(e.get_meta("desobediente", false))
					var ep: Vector2i = e.get_meta("grid_pos")
					var hp: Vector2i = h.get_meta("grid_pos")
					ahora_d = absi(ep.x - hp.x) + absi(ep.y - hp.y)
				var herido := int(h.get_meta("hp_actual")) < int(memo["ia_hp"])
				# Acting = moving or rolling its attack (it can go for another hero or miss the
				# roll)
				check(herido or tiro or se_movio, "el %s despierto actúa: distancia %d -> %d, %s" % [memo["ia_nombre"], antes, ahora_d, "ataca a %s" % h.name if herido else ("tira su ataque" if tiro else "sin atacar")])
			captura("11c_turno_enemigo")
			return 0.1,
		func():
			# One at a time: switching to the next hero waits for the previous thing to end
			var salida: Vector2i = dm.get_meta("casilla_salida")
			if activo() != null and not root.get_node("Main/CanvasLayer3").visible:
				dm.call("TeletransporteDebug", salida.x, salida.y)
			return 1.1,
		func():
			# One at a time: switching to the next hero waits for the previous thing to end
			var salida: Vector2i = dm.get_meta("casilla_salida")
			if activo() != null and not root.get_node("Main/CanvasLayer3").visible:
				dm.call("TeletransporteDebug", salida.x, salida.y)
			return 1.1,
		func():
			# One at a time: switching to the next hero waits for the previous thing to end
			var salida: Vector2i = dm.get_meta("casilla_salida")
			if activo() != null and not root.get_node("Main/CanvasLayer3").visible:
				dm.call("TeletransporteDebug", salida.x, salida.y)
			return 1.1,
		func():
			# One at a time: switching to the next hero waits for the previous thing to end
			var salida: Vector2i = dm.get_meta("casilla_salida")
			if activo() != null and not root.get_node("Main/CanvasLayer3").visible:
				dm.call("TeletransporteDebug", salida.x, salida.y)
			return 1.1,
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
			var repartir: Button = root.get_node_or_null("Main/CanvasLayer3/PantallaFogata/BtnRepartir")
			check(repartir != null and repartir.is_visible_in_tree(), "en la fogata está el botón Repartir objetos")
			if repartir:
				repartir.pressed.emit()
				var inv = memo.get("inventario")
				check(inv != null and inv.esta_abierta(), "y abre la mochila")
			return 0.3,
		func():
			var inv = memo.get("inventario")
			if inv and inv.esta_abierta():
				var lejos := 0
				for b in inv.retratos.get_children():
					if b.modulate != Color.WHITE:
						lejos += 1
				check(lejos == 0, "en la fogata todos pueden intercambiar")
				captura("12c_fogata_mochila")
				inv.cerrar()
			captura("12_fogata")
			ui.call("DescansarYContinuar")
			return 1.0,
		func():
			var negro: ColorRect = root.get_node("Main/CapaTransicion/Negro")
			var rotulo: Label = root.get_node("Main/CapaTransicion/Rotulo")
			check(negro.color.a > 0.9 and rotulo.text == "Piso 2", "la transición funde a negro con el rótulo '%s'" % rotulo.text)
			captura("12b_transicion")
			return 2.4,
		func():
			captura("13_piso_2")
			check(root.get_node("Main/CapaTransicion/Negro").color.a < 0.05, "la transición termina")
			check(int(pm.get_meta("piso_actual")) == 2, "estamos en el piso 2")
			check(activo() != null, "hay personaje activo en el piso 2")
			# On arrival you only know what's seen from the entrance
			var exploradas: PackedInt32Array = dm.call("DebugExploradas")
			var inicio_p: Vector2i = dm.get_meta("casilla_inicio")
			var lejos := 0
			for i in range(0, exploradas.size(), 2):
				if absi(exploradas[i] - inicio_p.x) + absi(exploradas[i + 1] - inicio_p.y) > 9 + 3:  # Light + stealth VISION_EXTRA
					lejos += 1
			check(exploradas.size() > 0 and lejos == 0, "en el piso nuevo no hay salas reveladas lejos de la entrada (%d exploradas, %d lejos)" % [exploradas.size() / 2, lejos])
			var inicio: Vector2i = dm.get_meta("casilla_inicio")
			for h in pm.get_children():
				if h.name == "DatosEquipo":
					continue
				var gp: Vector2i = h.get_meta("grid_pos")
				check(abs(gp.x - inicio.x) + abs(gp.y - inicio.y) <= 3, "%s aparece junto al inicio %s (está en %s)" % [h.name, inicio, gp])
			dm.call("ToggleModoDebug")
			return 3.0, # The camera slides to the new active hero
		func():
			# Picking: we project a tile to SubViewport pixels and cast the same ray against the
			# floor the InputManager uses (h_offset included); it must come back to the same tile
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
			# Two heights: we move the active hero up to a high room
			var lado := int(Engine.get_meta("alturas_lado", 0))
			var niveles: PackedByteArray = Engine.get_meta("alturas", PackedByteArray())
			var libres: PackedByteArray = dm.call("ObtenerTransitables")
			var alta := Vector2i(-1, -1)
			for i in niveles.size():
				if niveles[i] == 2 and i < libres.size() and libres[i] == 1:
					alta = Vector2i(i / lado, i % lado)
					break
			check(alta.x >= 0, "el piso tiene alguna sala alta")
			if alta.x >= 0:
				memo["pos_baja"] = pos_activa()
				memo["pos_alta"] = alta
				dm.call("TeletransporteDebug", alta.x, alta.y)
				dm.call("CalcularMapaDeLuces")
			return 0.8,
		func():
			if memo.has("pos_alta"):
				var ficha: Node3D = dm.get_node(str(activo().name))
				check(absf(ficha.position.y - 0.4) < 0.01, "en la sala alta la ficha está a su altura (y = %.2f)" % ficha.position.y)
				captura("15b_sala_alta")
				# If a proximity event triggered when going up, close it so it doesn't mess up the
				# rest of the script
				_cerrar_evento("en la sala alta")
				var p0: Vector2i = memo["pos_baja"]
				dm.call("TeletransporteDebug", p0.x, p0.y)
				dm.call("CalcularMapaDeLuces")
			return 0.5,
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
			var pe = root.get_node("Main/CanvasLayer2/PantallaEvento")
			check(pe.visible, "la pantalla de evento aparece")
			var con_prob := 0
			for b in pe.get_node("CajaBotones").get_children():
				if b.visible and b.text.contains("%"):
					con_prob += 1
			check(con_prob == 3, "cada opción del evento enseña su probabilidad (%d de 3)" % con_prob)
			check(pe.get_node("Foto").texture != null, "el evento enseña su ilustración")
			# Every event has its art (even if it's a placeholder)
			var sin_arte: Array = []
			for f in DirAccess.get_files_at("res://Data/Events"):
				if not f.ends_with(".tres"):
					continue
				var ev = load("res://Data/Events/" + f)
				var ok: bool = ev.ilustracion != null and ev.sprite_mapa != null
				for o in ev.opciones:
					ok = ok and o.imagen_exito != null and o.imagen_fracaso != null
				if not ok:
					sin_arte.append(f)
			check(sin_arte.is_empty(), "todos los eventos tienen sprite, ilustración e imágenes de ganar y perder %s" % [sin_arte])
			captura("16_evento")
			ui.call("ResolverOpcion", 0)
			memo["dado_t"] = ahora()
			return 0.6,
		func():
			check(root.get_node("Main/CanvasLayer2/PantallaEvento/Dado").visible, "durante la tirada se ve el dado 3D")
			captura("16b_dado_rodando")
			return 4.0,
		func():
			var pe = root.get_node("Main/CanvasLayer2/PantallaEvento")
			var tirada: Dictionary = pe.ultima_tirada
			check(int(tirada.get("d20", 0)) == 15 and bool(tirada.get("exito", false)), "el evento se resuelve con un d20 (%s)" % [tirada])
			var foto: Texture2D = pe.get_node("Foto").texture
			check(foto != null and foto.resource_path.ends_with("opcion1_exito.png"), "al ganar se ve la imagen de victoria de esa opción (%s)" % (foto.resource_path if foto else "nada"))
			check(pe.get_node("CajaBotones/BtnContinuar").visible, "tras el resultado sale Continuar")
			captura("17_evento_resuelto")
			ui.call("CerrarEvento")
			return 0.8,
		func():
			check(not dm.get_meta("bloqueo_evento", false), "al cerrar el evento el tablero se desbloquea")
			# Minimap: something explored has to be painted
			var mapa: TextureRect = ui.get_node("Minimapa/Mascara/Mapa")
			var img := mapa.texture.get_image()
			var pintados := 0
			for x in img.get_width():
				for y in img.get_height():
					if img.get_pixel(x, y).a > 0.5:
						pintados += 1
			check(pintados > 10, "el minimapa muestra lo explorado (%d casillas)" % pintados)
			# Pause menu visual options: low PS1 + CRT
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
		espera_hasta = ahora() + 2.2 # "PISO 1" fade in
		return false
	if ahora() < espera_hasta:
		return false
	# Movement steps wait for the tween to end (events are handled separately)
	if dm.get_meta("is_moving", false):
		return false
	# Level ups open the draft when their time comes: outside the step that tests it, the first card
	# is picked (or the first ability forgotten) so the script goes on
	var pn = root.get_node_or_null("Main/PantallaNivel")
	if pn and pn.visible and not memo.get("probando_draft", false):
		pn.elegir(0)
		if pn.visible and pn.heroe:
			pn.olvidar(str(root.get_node("Progreso").lista(pn.heroe)[0]["id"]))
		if pn.visible:
			pn.call("_cerrar")
		return false

	indice += 1
	if indice >= pasos.size():
		print("[test] RESULTADO: ", "TODO OK" if fallos.is_empty() else "%d FALLOS: %s" % [fallos.size(), fallos])
		quit(0 if fallos.is_empty() else 1)
		return false
	var segundos: float = pasos[indice].call()
	espera_hasta = ahora() + segundos
	return false
