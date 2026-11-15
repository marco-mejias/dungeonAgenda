extends Node

# Especiales (autoload): los objetos de broma que no caben en los efectos normales y la gallina.
#   - Poción "¿Qué podría malir sal?": d6 -> pollo / vida al máximo y +3 fuerza / explosión.
#   - Botón de "¡PÁNICO!": teletransporte a una casilla lejana; los demás pierden cordura.
#   - Gallina lanza-huevos (habilidad, 3 PA): aliada que al empezar el turno enemigo lanza un huevo
#     (3 de daño y ciega: -3 a su próxima tirada). Cada 5 disparos quiere grano de oro; sin él, a
#     veces el huevo es para quien la invocó.
# El resto (chancleta, cartón, navaja, cubo, máscara) está en C++ (Reglas.hpp, EnemyManager...).

const RUTA_DM := "/root/Main/ContenedorMapa/SubViewport/DungeonManager"
const RUTA_PM := "/root/Main/PartyManager"
const LADO := 40 # mazmorra::GRID_SIZE
const HOJA_GALLINA := preload("res://Assets/Sprites/Heroes32/gallina.png")
const COSTE_GALLINA := 3
const ALCANCE_HUEVO := 5
const RADIO_EXPLOSION := 3
const DANO_EXPLOSION := 8

var gallina: Node3D = null
var piso_gallina := -1

func _dm() -> Node:
	return get_node_or_null(RUTA_DM)

func _pm() -> Node:
	return get_node_or_null(RUTA_PM)

func es_especial(id: String) -> bool:
	return id in ["pocion_malir_sal", "boton_panico", "elixir_ingenio"]

# ---------------- Utilidades ----------------

func _manhattan(a: Vector2i, b: Vector2i) -> int:
	return absi(a.x - b.x) + absi(a.y - b.y)

func _mundo(c: Vector2i) -> Vector3:
	# A la altura del suelo de la casilla (salas altas y escalones: Mazmorra/Alturas.hpp)
	var y := 0.0
	if Engine.has_meta("alturas"):
		var lado := int(Engine.get_meta("alturas_lado", 0))
		var a: PackedByteArray = Engine.get_meta("alturas")
		if c.x >= 0 and c.y >= 0 and c.x < lado and c.y < lado:
			y = a[c.x * lado + c.y] * 0.2
	return Vector3(c.x + 0.5, y, c.y + 0.5)

## Texto que sube y se desvanece sobre una casilla del mapa
func texto(pos: Vector3, t: String, color: Color, tam: int = 40) -> void:
	var dm := _dm()
	if not dm:
		return
	var l := Label3D.new()
	l.text = t
	l.modulate = color
	l.font_size = tam
	l.outline_size = 10
	l.outline_modulate = Color(0.05, 0.02, 0.05)
	l.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	l.no_depth_test = true
	l.pixel_size = 0.008
	l.position = pos + Vector3(0, 1.2, 0)
	dm.add_child(l)
	var tw := l.create_tween()
	tw.tween_property(l, "position:y", l.position.y + 0.6, 0.9)
	tw.parallel().tween_property(l, "modulate:a", 0.0, 0.9).set_delay(0.4)
	tw.tween_callback(l.queue_free)

func _heroes_en_mapa() -> Array:
	var out := []
	var pm := _pm()
	if not pm:
		return out
	for h in pm.get_children():
		if h.name == "DatosEquipo" or h.get_meta("esta_muerto", false) or h.get_meta("ha_escapado", false) or not h.has_meta("grid_pos"):
			continue
		out.append(h)
	return out

func _enemigos() -> Array:
	var out := []
	var dm := _dm()
	if not dm:
		return out
	for p in dm.get_meta("lista_enemigos", []):
		var e := dm.get_node_or_null("Enemy_%d_%d" % [p.x, p.y])
		if e and not str(e.name).ends_with("_muerto") and int(e.get_meta("hp_actual", 1)) > 0:
			out.append(e)
	return out

func _danar_enemigo(e: Node, dano: int) -> void:
	var hp := int(e.get_meta("hp_actual", 1)) - dano
	e.set_meta("hp_actual", hp)
	e.set_meta("in_combat", true)
	# El número lo enseñan los Indicadores (corazón sobre la cabeza)
	if hp <= 0:
		Indicadores.revisar(e) # antes de que desaparezca
		# XP para quien invocó a la gallina
		var inv: Node = _pm().get_node_or_null(str(gallina.get_meta("invocador", ""))) if is_instance_valid(gallina) else null
		Progreso.enemigo_muerto(e, inv)
		var p: Vector2i = e.get_meta("grid_pos")
		_dm().call("EliminarEnemigo", p.x, p.y)

func _dado(numero: int, bueno: bool, sello: String, detalle: String) -> void:
	var d := get_node_or_null("/root/DadoCombate")
	if d:
		d.tirar(numero, 1 if bueno else 0, sello, detalle)

# ---------------- Consumibles ----------------

## Devuelve "" si se ha usado (el PartyManager quita el objeto) o el motivo por el que no.
func usar(h: Node, id: String) -> String:
	match id:
		"pocion_malir_sal": return _pocion(h)
		"boton_panico": return _boton(h)
		"elixir_ingenio":
			Progreso.restaurar(h)
			return ""
	return "No se puede usar"

func _pocion(h: Node) -> String:
	var d6 := randi_range(1, 6)
	var pos: Vector3 = _mundo(h.get_meta("grid_pos")) if h.has_meta("grid_pos") else Vector3.ZERO
	var nombre := str(h.get_meta("nombre_display", h.name))
	if d6 <= 2:
		# Pollo: pierde lo que le queda del turno (vuelve a ser persona al empezar el día)
		h.set_meta("es_pollo", true)
		h.set_meta("pa_actual", 0)
		_dado(d6, false, "¡POLLO!", "%s: d6 %d -> te conviertes en pollo" % [nombre, d6])
		texto(pos, "¡Cloc!", Color(1, 0.85, 0.4))
		var dm := _dm()
		var pm := _pm()
		if dm and pm and pm.call("GetActiveCharacter") == h:
			dm.call("PedirFinDeTurno")
	elif d6 <= 4:
		h.set_meta("hp_actual", int(h.get_meta("hp_max", 1)))
		var temporales: Array = h.get_meta("mods_temporales", [])
		temporales.append({"stat": "fuerza", "valor": 3, "turnos": 3})
		h.set_meta("mods_temporales", temporales)
		_pm().call("RecalcularTodo")
		_dado(d6, true, "¡SUBIDÓN!", "%s: d6 %d -> vida al máximo y +3 fuerza" % [nombre, d6])
		texto(pos, "¡Subidón!", Color(0.5, 1, 0.5))
	else:
		_dado(d6, false, "¡BUM!", "%s: d6 %d -> explotas" % [nombre, d6])
		_explosion(h)
	return ""

func _explosion(h: Node) -> void:
	if not h.has_meta("grid_pos"):
		return
	var centro: Vector2i = h.get_meta("grid_pos")
	var feel := get_node_or_null("/root/Feel")
	if feel:
		feel.sacudir(0.7)
		feel.destello_pantalla(Color(1, 0.6, 0.2, 0.6), 0.4)
	texto(_mundo(centro), "¡BUM!", Color(1, 0.55, 0.2), 64)
	for e in _enemigos():
		if _manhattan(e.get_meta("grid_pos"), centro) <= RADIO_EXPLOSION:
			_danar_enemigo(e, DANO_EXPLOSION)
	var pm := _pm()
	for o in _heroes_en_mapa():
		if _manhattan(o.get_meta("grid_pos"), centro) <= RADIO_EXPLOSION:
			pm.call("RecibirDano", DANO_EXPLOSION, o)

func _boton(h: Node) -> String:
	var dm := _dm()
	var pm := _pm()
	if not dm or not pm or not h.has_meta("grid_pos"):
		return "Aquí no hay a dónde huir"
	if pm.call("GetActiveCharacter") != h:
		return "Solo puede pulsarlo quien tiene el turno"
	var desde: Vector2i = h.get_meta("grid_pos")
	var libres: PackedByteArray = dm.call("ObtenerTransitables")
	var lejos := []
	for i in libres.size():
		if libres[i] == 1:
			var c := Vector2i(i / LADO, i % LADO)
			if _manhattan(c, desde) >= 8:
				lejos.append(c)
	if lejos.is_empty():
		return "No hay ningún sitio libre lejos"
	var destino: Vector2i = lejos[randi() % lejos.size()]
	dm.call("TeletransporteDebug", destino.x, destino.y)
	if Vector2i(h.get_meta("grid_pos")) != destino:
		return "El botón no responde"
	dm.call("CalcularMapaDeLuces")
	texto(_mundo(destino), "¡PÁNICO!", Color(1, 0.35, 0.3), 52)
	# El odio eterno del grupo
	for o in _heroes_en_mapa():
		if o != h:
			o.set_meta("sanidad_actual", maxi(0, int(o.get_meta("sanidad_actual", 0)) - 5))
			texto(_mundo(o.get_meta("grid_pos")), "-5 cordura", Color(0.7, 0.55, 1))
	var feel := get_node_or_null("/root/Feel")
	if feel:
		feel.destello_pantalla(Color(1, 0.2, 0.2, 0.5), 0.3)
	return ""

# ---------------- La gallina ----------------

## Habilidad: invoca a la gallina junto al héroe (3 PA; solo una a la vez)
func invocar_gallina(h: Node) -> String:
	var dm := _dm()
	var pm := _pm()
	if not dm or not pm or not h.has_meta("grid_pos"):
		return "No hay sitio para la gallina"
	if is_instance_valid(gallina):
		return "La gallina ya está en el campo"
	var pa := int(h.get_meta("pa_actual", 0))
	if pa < COSTE_GALLINA and not dm.get_meta("debug_mode", false):
		return "Necesitas %d PA para invocar a la gallina" % COSTE_GALLINA
	var libres: PackedByteArray = dm.call("ObtenerTransitables")
	var p: Vector2i = h.get_meta("grid_pos")
	var sitio := Vector2i(-1, -1)
	for d in [Vector2i(1, 0), Vector2i(0, 1), Vector2i(-1, 0), Vector2i(0, -1), Vector2i(1, 1), Vector2i(-1, 1), Vector2i(1, -1), Vector2i(-1, -1)]:
		var c: Vector2i = p + d
		if c.x >= 0 and c.y >= 0 and c.x < LADO and c.y < LADO and libres[c.x * LADO + c.y] == 1:
			sitio = c
			break
	if sitio.x < 0:
		return "No hay sitio libre al lado para la gallina"
	if not dm.get_meta("debug_mode", false):
		h.set_meta("pa_actual", pa - COSTE_GALLINA)
	gallina = Node3D.new()
	gallina.name = "Gallina"
	gallina.position = _mundo(sitio)
	gallina.set_meta("grid_pos", sitio)
	gallina.set_meta("invocador", str(h.name))
	gallina.set_meta("disparos", 0)
	var cuerpo := Sprite3D.new()
	cuerpo.name = "CuerpoGallina" # no "Cuerpo": así el renderer no la toma por una ficha
	cuerpo.texture = HOJA_GALLINA
	cuerpo.hframes = 4
	cuerpo.pixel_size = 0.055
	cuerpo.billboard = BaseMaterial3D.BILLBOARD_FIXED_Y
	cuerpo.alpha_cut = SpriteBase3D.ALPHA_CUT_DISCARD
	cuerpo.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	cuerpo.position = Vector3(0, 0.44, 0)
	gallina.add_child(cuerpo)
	dm.add_child(gallina)
	piso_gallina = int(pm.get_meta("piso_actual", 1))
	texto(gallina.position, "¡Cloc cloc!", Color(1, 0.85, 0.4))
	return ""

## Al empezar el turno enemigo: un huevo al enemigo más cercano a la vista. Devuelve cuánto dura.
func turno_gallina() -> float:
	if not is_instance_valid(gallina):
		return 0.0
	var dm := _dm()
	var pm := _pm()
	var pos: Vector2i = gallina.get_meta("grid_pos")
	var objetivo: Node = null
	var mejor := 999
	for e in _enemigos():
		var c: Vector2i = e.get_meta("grid_pos")
		var d := _manhattan(c, pos)
		if d <= ALCANCE_HUEVO and d < mejor and Tactica.linea_de_vision(pos, c):
			mejor = d
			objetivo = e
	if objetivo == null:
		return 0.0
	var disparos := int(gallina.get_meta("disparos", 0)) + 1
	gallina.set_meta("disparos", disparos)
	if disparos % 5 == 0:
		# Quiere su grano de oro
		var invocador: Node = pm.get_node_or_null(str(gallina.get_meta("invocador")))
		if invocador:
			var inv: Array = pm.call("DescribirInventario", str(invocador.name))
			for i in inv.size():
				if str(inv[i]["id"]) == "grano_oro":
					pm.call("QuitarObjeto", str(invocador.name), i, 1)
					texto(gallina.position, "Ñam (grano de oro)", Color(1, 0.85, 0.4), 32)
					return _lanzar_huevo(objetivo)
			if randf() < 0.5 and invocador.has_meta("grid_pos"):
				texto(gallina.position, "¡Traición!", Color(1, 0.3, 0.3), 40)
				return _lanzar_huevo(invocador, true)
	return _lanzar_huevo(objetivo)

func _lanzar_huevo(objetivo: Node, a_un_heroe: bool = false) -> float:
	var dm := _dm()
	var huevo := MeshInstance3D.new()
	var esfera := SphereMesh.new()
	esfera.radius = 0.09
	esfera.height = 0.2
	huevo.mesh = esfera
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.96, 0.93, 0.85)
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	huevo.material_override = mat
	huevo.position = gallina.position + Vector3(0, 0.6, 0)
	dm.add_child(huevo)
	# La cámara encuadra a la gallina y a su víctima (el EnemyManager lo suelta al seguir)
	var cam := dm.get_node_or_null("Camera3D")
	if cam and objetivo is Node:
		var ficha_obj: Node = objetivo if objetivo is Node3D else dm.get_node_or_null(NodePath(str(objetivo.name)))
		if ficha_obj is Node3D:
			cam.set_meta("encuadre", PackedVector3Array([gallina.position, ficha_obj.position]))
	var destino: Vector3 = _mundo(objetivo.get_meta("grid_pos")) + Vector3(0, 0.55, 0)
	var tw := huevo.create_tween()
	tw.tween_property(huevo, "position", destino, 0.35).set_trans(Tween.TRANS_QUAD)
	tw.tween_callback(func():
		huevo.queue_free()
		if a_un_heroe:
			_pm().call("RecibirDano", 3, objetivo)
		elif is_instance_valid(objetivo):
			objetivo.set_meta("cegado", 2)
			texto(destino, "¡Cegado!", Color(1, 0.95, 0.7), 30)
			_danar_enemigo(objetivo, 3))
	return 0.9

func _process(_dt: float) -> void:
	if not is_instance_valid(gallina):
		gallina = null
		return
	var pm := _pm()
	# Al bajar de piso la gallina se queda atrás
	if pm and int(pm.get_meta("piso_actual", 1)) != piso_gallina:
		gallina.queue_free()
		gallina = null
		return
	var cuerpo := gallina.get_node_or_null("CuerpoGallina") as Sprite3D
	if cuerpo:
		cuerpo.frame = int(Time.get_ticks_msec() / 220.0) % 4
